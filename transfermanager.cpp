#include "transfermanager.h"

TransferManager::TransferManager(QObject *parent) :
    QObject(parent)
{
    currentDownloadCount = 0;
    currentUploadCount = 0;
    zeroHostAddress = QHostAddress("0.0.0.0");
    protocolOrderPreference = QByteArray();
    qsrand(QDateTime::currentMSecsSinceEpoch());
    nextSegmentId = qrand();
    if (nextSegmentId == 0)
        nextSegmentId++;
}

TransferManager::~TransferManager()
{
    QHashIterator<QByteArray, Transfer*> it(transferObjectTable);
    while (it.hasNext())
    {
        Transfer *p = it.next().value();
        destroyTransferObject(p);
    }

    QMapIterator<int, QList<DownloadTransferQueueItem>*> i(downloadTransferQueue);
    while (i.hasNext())
        delete i.next().value();
}

// remove the pointer to the transfer object from the transfer object table before deleting the object.
void TransferManager::destroyTransferObject(Transfer* transferObject)
{
    int type = transferObject->getTransferType();

    if (transferObjectTable.remove(*transferObject->getTTH(), transferObject) != 0)
    {
        if (type == TRANSFER_TYPE_DOWNLOAD)
            currentDownloadCount--;
        else if (type == TRANSFER_TYPE_UPLOAD)
        {
            currentUploadCount--;
            currentUploadingHosts.remove(*transferObject->getRemoteHost());
        }

        QMutableHashIterator<QHostAddress, Transfer *> i(peerProtocolDiscoveryWaitingPool);
        while (i.hasNext())
        {
            i.next();
            if (i.value() == transferObject)
                i.remove();
        }
    }
    transferObject->deleteLater();

    if (currentDownloadCount < maximumSimultaneousDownloads)
        startNextDownload();
}

// incoming data packets
void TransferManager::incomingDataPacket(quint8 transferPacket, QHostAddress fromHost, QByteArray datagram)
{
    QByteArray tmp = datagram.mid(2, 8);
    quint64 offset = getQuint64FromByteArray(&tmp);
    if (offset > LLONG_MAX)
        return;

    QByteArray tth = datagram.mid(10, 24);
    QByteArray data = datagram.mid(34);
    if ((data.length() == datagram.length() - 34) && (transferObjectTable.contains(tth)))
    {
        Transfer *t = getTransferObjectPointer(tth, TRANSFER_TYPE_DOWNLOAD);
        if (t)
            t->incomingDataPacket(transferPacket, offset, data);
        else
        {
            t = getTransferObjectPointer(tth, TRANSFER_TYPE_UPLOAD, &fromHost);
            if (t)
            {
                qDebug() << "TransferManager::incomingDataPacket() for upload" << t->getFileName();
                t->incomingDataPacket(transferPacket, (qint64)offset, data);
            }
        }
    }
}

// incoming direct dispatched data packets
void TransferManager::incomingDirectDataPacket(quint32 segmentId, qint64 offset, QByteArray data)
{
    TransferSegment *t = getTransferSegmentPointer(segmentId);
    qDebug() << "TransferManager::incomingDirectDataPacket()" << segmentId << t << data.length();
    if (t)
        t->incomingDataPacket(offset, data);
}

void TransferManager::incomingTransferError(QHostAddress fromHost, QByteArray tth, qint64 offset, quint8 error)
{
    Transfer *t = getTransferObjectPointer(tth, TRANSFER_TYPE_DOWNLOAD);
    if (t)
        t->incomingTransferError(offset, error);
}

// incoming requests for files we share
void TransferManager::incomingUploadRequest(quint8 protocol, QHostAddress fromHost, QByteArray tth, qint64 offset, qint64 length, quint32 segmentId)
{
    //qDebug() << "TransferManager::incomingUploadRequest(): Data request offset " << offset << " length " << length;
    Transfer *t = getTransferObjectPointer(tth, TRANSFER_TYPE_UPLOAD, &fromHost);
    if (t)
    {
        t->setFileOffset(offset);
        t->setSegmentLength(length);
        t->startTransfer();
    }
    else
    {
        // Only allow one file transfer per host
        if (currentUploadingHosts.contains(fromHost))
        {
            emit sendTransferError(fromHost, PeerAlreadyTransferring, tth, offset);
            return;
        }

        //Only queue the item if there are "slots" open
        if (currentUploadCount < maximumSimultaneousUploads)
        {
            currentUploadCount++;
            UploadTransferQueueItem *i = new UploadTransferQueueItem;
            i->protocol = protocol;
            i->requestingHost = fromHost;
            i->fileOffset = offset;
            i->requestLength = length;
            i->segmentId = segmentId;
            uploadTransferQueue.insertMulti(tth, i);
            currentUploadingHosts.insert(fromHost);
            emit filePathNameRequest(tth);
        }
        else
        {
            emit sendTransferError(fromHost, NoSlotsAvailable, tth, offset);
        }
    }
}

// sharing engine replies with file name for tth being requested for download
// this structure assumes only one user requests a specific file during the time it takes to dispatch the request to a transfer object.
// should more than one user try simultaneously, their requests will be deleted off the queue in .remove(tth) and they should try again. oops.
void TransferManager::filePathNameReply(QByteArray tth, QString filename, quint64 fileSize)
{
    qDebug() << "TransferManager::filePathNameReply()" << filename << fileSize;
    if (filename == "" || !uploadTransferQueue.contains(tth))
    {
        uploadTransferQueue.remove(tth);
        currentUploadCount--;
        if (uploadTransferQueue.contains(tth))
        {
            currentUploadingHosts.remove(uploadTransferQueue.value(tth)->requestingHost);
            emit sendTransferError(uploadTransferQueue.value(tth)->requestingHost, FileNotSharedError, tth, uploadTransferQueue.value(tth)->fileOffset);
        }
        qDebug() << "TransferManager::filePathNameReply() abort";
        return;
    }
    Transfer *t = new UploadTransfer(this);
    connect(t, SIGNAL(abort(Transfer*)), this, SLOT(destroyTransferObject(Transfer*)));
    connect(t, SIGNAL(transmitDatagram(QHostAddress,QByteArray*)), this, SIGNAL(transmitDatagram(QHostAddress,QByteArray*)));
    connect(t, SIGNAL(sendTransferError(QHostAddress,quint8,QByteArray,qint64)), this, SIGNAL(sendTransferError(QHostAddress,quint8,QByteArray,qint64)));
    connect(t, SIGNAL(setTransferSegmentPointer(quint32,TransferSegment*)), this, SLOT(setTransferSegmentPointer(quint32,TransferSegment*)));
    connect(t, SIGNAL(removeTransferSegmentPointer(quint32)), this, SLOT(removeTransferSegmentPointer(quint32)));
    TransferSegment *s = t->createUploadObject(uploadTransferQueue.value(tth)->protocol, uploadTransferQueue.value(tth)->segmentId);
    //via signal
    //setTransferSegmentPointer(uploadTransferQueue.value(tth)->segmentId, s);
    t->setFileName(filename);
    t->setFileSize(fileSize);
    t->setTTH(tth);
    t->setFileOffset(uploadTransferQueue.value(tth)->fileOffset);
    t->setSegmentLength(uploadTransferQueue.value(tth)->requestLength);
    t->setRemoteHost(uploadTransferQueue.value(tth)->requestingHost);
    uploadTransferQueue.remove(tth);
    transferObjectTable.insertMulti(tth, t);
    t->startTransfer();
}

// incoming requests from user interface for files we want to download
// the higher the priority, the lower the number.
void TransferManager::queueDownload(int priority, QByteArray tth, QString filePathName, quint64 fileSize, QHostAddress fileHost)
{
    if (!downloadTransferQueue.contains(priority))
    {
        QList<DownloadTransferQueueItem> *list = new QList<DownloadTransferQueueItem>();
        downloadTransferQueue.insert(priority, list);
    }

    //Test if item isn't already in the queue
    bool found = false;
    int size = downloadTransferQueue.value(priority)->size();
    foreach (DownloadTransferQueueItem item, *downloadTransferQueue.value(priority))
    {
        if (item.tth == tth)
        {
            found = true;
            break;
        }
    }

    //Only queue the item if it's not already in the queue and not busy downloading
    if (!found && !getTransferObjectPointer(tth, TRANSFER_TYPE_DOWNLOAD))
    {
        DownloadTransferQueueItem i;
        i.filePathName = filePathName;
        i.tth = tth;
        i.fileSize = fileSize;
        i.fileHost = fileHost;
        downloadTransferQueue.value(priority)->append(i);
        // start the next highest priority queued download, unless max downloads active.
        if (currentDownloadCount < maximumSimultaneousDownloads)
            startNextDownload();
    }
}

// requeue a download that "failed"
void TransferManager::requeueDownload(Transfer *transfer, bool failed)
{
    DownloadTransferQueueItem i;
    i.filePathName = *transfer->getFileName();
    i.tth = *transfer->getTTH();
    i.fileSize = transfer->getFileSize();
    i.fileHost = *transfer->getRemoteHost();
    
    //Tell the GUI that you want to requeue the download
    emit requeueDownload(i.tth);

    //Destroy the object
    destroyTransferObject(transfer);

    // TODO: if failed, set priority to PAUSED or something alike.
    //Requeue as low priority download
    queueDownload(3, i.tth, i.filePathName, i.fileSize, i.fileHost);
}

// get next item to be downloaded off the queue
DownloadTransferQueueItem TransferManager::getNextQueuedDownload()
{
    QMapIterator<int, QList<DownloadTransferQueueItem>* > i(downloadTransferQueue);
    while (i.hasNext())
    {
        QList<DownloadTransferQueueItem>* l = i.next().value();
        if (!l->isEmpty())
        {
            DownloadTransferQueueItem nextItem;
            nextItem = l->first();
            l->removeFirst();
            return nextItem;
        }
    }
    DownloadTransferQueueItem noItem;
    noItem.filePathName = "";
    return noItem;
}

void TransferManager::startNextDownload()
{
    DownloadTransferQueueItem i = getNextQueuedDownload();
    if (i.filePathName == "")
        return;

    currentDownloadCount++;
    Transfer *t = new DownloadTransfer(this);
    t->setCurrentlyDownloadingPeers(&currentDownloadingHosts);
    connect(t, SIGNAL(abort(Transfer*)), this, SLOT(destroyTransferObject(Transfer*)));
    connect(t, SIGNAL(requeue(Transfer *,bool)), this, SLOT(requeueDownload(Transfer *,bool)));
    connect(t, SIGNAL(hashBucketRequest(QByteArray,int,QByteArray,QHostAddress)), this, SIGNAL(hashBucketRequest(QByteArray,int,QByteArray,QHostAddress)));
    connect(t, SIGNAL(TTHTreeRequest(QHostAddress,QByteArray,quint32,quint32)),
            this, SIGNAL(TTHTreeRequest(QHostAddress,QByteArray,quint32,quint32)));
    connect(t, SIGNAL(searchTTHAlternateSources(QByteArray)), this, SIGNAL(searchTTHAlternateSources(QByteArray)));
    //connect(t, SIGNAL(loadTTHSourcesFromDatabase(QByteArray)), this, SIGNAL(loadTTHSourcesFromDatabase(QByteArray)));
    connect(t, SIGNAL(requestProtocolCapability(QHostAddress,Transfer*)), this, SLOT(requestPeerProtocolCapability(QHostAddress,Transfer*)));
    connect(t, SIGNAL(sendDownloadRequest(quint8,QHostAddress,QByteArray,qint64,qint64,quint32,QByteArray)),
            this, SIGNAL(sendDownloadRequest(quint8,QHostAddress,QByteArray,qint64,qint64,quint32,QByteArray)));
    connect(t, SIGNAL(flushBucket(QString,QByteArray*)), this, SIGNAL(flushBucket(QString,QByteArray*)));
    connect(t, SIGNAL(assembleOutputFile(QString,QString,int,int)), this, SIGNAL(assembleOutputFile(QString,QString,int,int)));
    connect(t, SIGNAL(flushBucketDirect(QString,int,QByteArray*,QByteArray)), this, SIGNAL(flushBucketDirect(QString,int,QByteArray*,QByteArray)));
    connect(t, SIGNAL(renameIncompleteFile(QString)), this, SIGNAL(renameIncompleteFile(QString)));
    connect(t, SIGNAL(transferFinished(QByteArray)), this, SLOT(transferDownloadCompleted(QByteArray)));
    connect(t, SIGNAL(transmitDatagram(QHostAddress,QByteArray*)), this, SIGNAL(transmitDatagram(QHostAddress,QByteArray*)));
    connect(t, SIGNAL(requestNextSegmentId(TransferSegment*)), this, SLOT(requestNextSegmentId(TransferSegment*)));
    connect(t, SIGNAL(saveBucketFlushStateBitmap(QByteArray,QByteArray)), this, SIGNAL(saveBucketFlushStateBitmap(QByteArray,QByteArray)));
    connect(t, SIGNAL(setTransferSegmentPointer(quint32,TransferSegment*)), this, SLOT(setTransferSegmentPointer(quint32,TransferSegment*)));
    connect(t, SIGNAL(removeTransferSegmentPointer(quint32)), this, SLOT(removeTransferSegmentPointer(quint32)));
    connect(t, SIGNAL(flagDownloadPeer(QHostAddress)), this, SLOT(addDownloadPeer(QHostAddress)));
    connect(t, SIGNAL(unflagDownloadPeer(QHostAddress)), this, SLOT(removeDownloadPeer(QHostAddress)));

    t->setFileName(i.filePathName);
    t->setTTH(i.tth);
    t->setFileSize(i.fileSize);
    t->setProtocolOrderPreference(protocolOrderPreference);
    // we can lose the ip address from the queue, alternate search tells us everything we need.
    t->addPeer(i.fileHost, i.tth);
    transferObjectTable.insertMulti(i.tth, t);
    emit loadBucketFlushStateBitmap(i.tth);
    //emit loadTTHSourcesFromDatabase(i.tth);
    emit searchTTHAlternateSources(i.tth);
    t->startTransfer();

    emit downloadStarted(i.tth);
}

void TransferManager::changeQueuedDownloadPriority(int oldPriority, int newPriority, QByteArray tth)
{
    DownloadTransferQueueItem item;
    int index = 0;
    if (downloadTransferQueue.contains(oldPriority))
    {
        QListIterator<DownloadTransferQueueItem> i(*downloadTransferQueue.value(oldPriority));
        while (i.hasNext())
        {
            if (i.peekNext().tth == tth)
            {
                item = i.next();
                break;
            }
            i.next();
            index++;
        }
    }
    if (item.filePathName != "")
    {
        //Remove from old priority queue
        downloadTransferQueue.value(oldPriority)->removeAt(index);

        //Add to new priority queue
        if (!downloadTransferQueue.contains(newPriority))
        {
            //Add priority type if it doesn't exist
            QList<DownloadTransferQueueItem> *list = new QList<DownloadTransferQueueItem>();
            downloadTransferQueue.insert(newPriority, list);
        }
        if (oldPriority > newPriority)
            downloadTransferQueue.value(newPriority)->append(item);
        else
            downloadTransferQueue.value(newPriority)->prepend(item);
    }
}

// downloads that are still queued can just be lifted off the queue, their transfer objects do not exist yet.
void TransferManager::removeQueuedDownload(int priority, QByteArray tth)
{
    if (downloadTransferQueue.contains(priority))
    {
        int pos = -1;
        int foundPos = -1;
        QListIterator<DownloadTransferQueueItem> i(*downloadTransferQueue.value(priority));
        while (i.hasNext())
        {
            pos++;
            if (i.next().tth == tth)
            {
                foundPos = pos;
                break;
            }
        }
        if (foundPos > -1)
        {   // As far as I understand, a list's index will always start at 0 and follow sequentially. Touch wood.
            downloadTransferQueue.value(priority)->removeAt(foundPos);
        }
    }

    //Stop the transfer if it's not in the queue (it might've already started)
    stopTransfer(tth, TRANSFER_TYPE_DOWNLOAD, QHostAddress());
}

//Stop a transfer already running
void TransferManager::stopTransfer(QByteArray tth, int transferType, QHostAddress hostAddr)
{
    Transfer *t;

    if (transferType == TRANSFER_TYPE_DOWNLOAD)
        t = getTransferObjectPointer(tth, transferType);
    else
        t = getTransferObjectPointer(tth, transferType, &hostAddr);
    if (t)
    {
        //Abort transfer before deletion
        t->abortTransfer();
        //t->deleteLater();
        //transferObjectTable.remove(tth, t);

        //Start next transfer
        if (currentDownloadCount < maximumSimultaneousDownloads)
            startNextDownload();
    }
}

//Download completed
void TransferManager::transferDownloadCompleted(QByteArray tth)
{
    //Update counts and start next download in queue
    if (currentDownloadCount-1 < maximumSimultaneousDownloads)
        startNextDownload();

    //Emit completed to GUI
    emit downloadCompleted(tth);
}

//Get transfer status
void TransferManager::requestGlobalTransferStatus()
{
    emit returnGlobalTransferStatus(getGlobalTransferStatus());
}

// look for pointer to Transfer object matching tth, transfer type and host address
Transfer* TransferManager::getTransferObjectPointer(QByteArray &tth, int transferType, QHostAddress *hostAddr)
{
    if (hostAddr == 0)
        hostAddr = &zeroHostAddress;

    if (transferObjectTable.contains(tth))
    {
        QListIterator<Transfer*> it(transferObjectTable.values(tth));
        while (it.hasNext())
        {
            Transfer* p = it.next();
            //if ((p->getTransferType() == transferType) && (p->getRemoteHost()->toString() == hostAddr.toString()))
            if ((p->getTransferType() == transferType) && (*p->getRemoteHost() == *hostAddr))
                return p;
        }
    }
    return 0;
}

// return a list of structs that contain the status of all the current transfers
QList<TransferItemStatus> TransferManager::getGlobalTransferStatus()
{
    QList<TransferItemStatus> status;
    QHashIterator<QByteArray, Transfer*> it(transferObjectTable);
    while (it.hasNext())
    {
        TransferItemStatus tis;
        tis.TTH = *it.peekNext().value()->getTTH();
        tis.filePathName = *it.peekNext().value()->getFileName();
        tis.transferType = it.peekNext().value()->getTransferType();
        tis.transferStatus = it.peekNext().value()->getTransferStatus();
        tis.transferProgress = it.peekNext().value()->getTransferProgress();
        tis.transferRate = it.peekNext().value()->getTransferRate();
        tis.uptime = it.peekNext().value()->getUptime();
        tis.host = *it.peekNext().value()->getRemoteHost();
        tis.onlineSegments = it.peekNext().value()->getSegmentCount();
        tis.segmentStatuses = it.peekNext().value()->getSegmentStatuses();
        tis.segmentBitmap = it.peekNext().value()->getTransferStateBitmap();
        status.append(tis);
        
        it.next();
    }
    return status;
}

// signals from db restore and incoming tth search both route here
void TransferManager::incomingTTHSource(QByteArray tth, QHostAddress sourcePeer, QByteArray sourceCID)
{
    //qDebug() << "TransferManager::incomingTTHSource() tth sourcePeer" << tth.toBase64() << sourcePeer;
    //if (transferObjectTable.contains(tth))
    //    transferObjectTable.value(tth)->addPeer(sourcePeer);
    Transfer *t = getTransferObjectPointer(tth, TRANSFER_TYPE_DOWNLOAD);
    if (t)
        t->addPeer(sourcePeer, sourceCID);
}

// packet containing part of a tree
// qint32 bucket number followed by 24-byte QByteArray of bucket 1MBTTH
// therefore, 2.25 petabyte max file size, should suffice.
void TransferManager::incomingTTHTree(QByteArray tth, QByteArray tree)
{
    Transfer *t = getTransferObjectPointer(tth, TRANSFER_TYPE_DOWNLOAD);
    if (t)
        t->TTHTreeReply(tree);
}

void TransferManager::hashBucketReply(QByteArray rootTTH, int bucketNumber, QByteArray bucketTTH, QHostAddress peer)
{
    Transfer *t = getTransferObjectPointer(rootTTH, TRANSFER_TYPE_DOWNLOAD);
    if (t)
        t->hashBucketReply(bucketNumber, bucketTTH, peer);
    // should be no else, if the download object mysteriously disappeared somewhere, we can just silently drop the message here.
}

void TransferManager::incomingProtocolCapabilityResponse(QHostAddress peer, char protocols)
{
    peerProtocolCapabilities.insert(peer, protocols);
    if (peerProtocolDiscoveryWaitingPool.contains(peer) && peerProtocolDiscoveryWaitingPool.value(peer))
    {
        peerProtocolDiscoveryWaitingPool.value(peer)->receivedPeerProtocolCapability(peer, protocols);
        peerProtocolDiscoveryWaitingPool.take(peer);
    }
}

void TransferManager::requestPeerProtocolCapability(QHostAddress peer, Transfer *transferObject)
{
    //================================================================================================================
    //After a few hours of debugging = This shouldn't be a direct call - it messes up the calling order!
    //It will call getSegmentForDownloading BEFORE startTransfer which will ensure the download is never started since:
    
    //transferSegmentStateBitmap has length 0 before startTransfer
    //which ensures a segment is returned with segmentEnd = 0
    //which makes downloadNextAvailableChunk think the segment is done

    //The first download will work since the call to getSegmentForDownloading is queued. The second, third etc from the same source will fail.
    //Either this function should always use the deferred protocol capability request (through the emit below) or the whole calling order should be reexamined... 
    //(Which by the way is pretty obtuse - probably since this protocol capability functionality was added later. A comment or two on the logical process would've been nice ;) )
    //================================================================================================================

    //if (peerProtocolCapabilities.contains(peer))
    //    transferObject->receivedPeerProtocolCapability(peer, peerProtocolCapabilities.value(peer));
    //else
    //{
        peerProtocolDiscoveryWaitingPool.insert(peer, transferObject);
        emit requestProtocolCapability(peer);
    //}
}

void TransferManager::bucketFlushed(QByteArray tth, int bucketNo)
{
    Transfer* t = getTransferObjectPointer(tth, TRANSFER_TYPE_DOWNLOAD);
    if (t)
        t->bucketFlushed(bucketNo);
    //qDebug() << "TM bucket flush"; << t;
}

void TransferManager::bucketFlushFailed(QByteArray tth, int bucketNo)
{
    Transfer* t = getTransferObjectPointer(tth, TRANSFER_TYPE_DOWNLOAD);
    if (t)
        t->bucketFlushFailed(bucketNo);
    //qDebug() << "TM bucket flush fail" << t;
}

void TransferManager::setMaximumSimultaneousDownloads(int n)
{
    maximumSimultaneousDownloads = n;
}

void TransferManager::setMaximumSimultaneousUploads(int n)
{
    maximumSimultaneousUploads = n;
}

void TransferManager::setProtocolOrderPreference(QByteArray p)
{
    protocolOrderPreference = p;
}

void TransferManager::requestNextSegmentId(TransferSegment *segment)
{
    nextSegmentId++;
    if (nextSegmentId == 0)
        nextSegmentId++;

    segment->setSegmentId(nextSegmentId);
}

void TransferManager::restoreBucketFlushStateBitmap(QByteArray tth, QByteArray bitmap)
{
    Transfer *t = getTransferObjectPointer(tth, TRANSFER_TYPE_DOWNLOAD);
    if (t)
        t->setBucketFlushStateBitmap(bitmap);
}

TransferSegment* TransferManager::getTransferSegmentPointer(quint32 segmentId)
{
    qDebug() << "TransferManager::getTransferSegmentPointer()" << segmentId << transferSegmentPointers;
    // QHash automatically returns 0 for TransferSegment* when key not found
    return transferSegmentPointers.value(segmentId);
}

void TransferManager::setTransferSegmentPointer(quint32 segmentId, TransferSegment *segment)
{
    qDebug() << "TransferManager::setTransferSegmentPointer()" << segmentId << segment;
    transferSegmentPointers.insert(segmentId, segment);
}

void TransferManager::removeTransferSegmentPointer(quint32 segmentId)
{
    qDebug() << "TransferManager::removeTransferSegmentPointer()" << segmentId;
    transferSegmentPointers.remove(segmentId);
}

void TransferManager::addDownloadPeer(QHostAddress peer)
{
    currentDownloadingHosts.insert(peer);
}

void TransferManager::removeDownloadPeer(QHostAddress peer)
{
    currentDownloadingHosts.remove(peer);
}

bool TransferManager::checkDownloadPeer(QHostAddress peer)
{
    if (currentDownloadingHosts.contains(peer))
        return false;
    else
        return true;
}

void TransferManager::closeClientEvent()
{
    QHashIterator<QByteArray, Transfer*> i(transferObjectTable);
    while (i.hasNext())
        i.next().value()->abortTransfer();

    //QTimer::singleShot(100, this, SIGNAL(closeClientEventReturn()));
    emit closeClientEventReturn();
}

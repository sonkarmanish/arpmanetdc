#include "networkbootstrap.h"

NetworkBootstrap::NetworkBootstrap(QObject *parent) :
    QObject(parent)
{
    // Init scanlist
    // Load hardcoded hints
    QFile file("nodes.dat");
    if (file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QString line;
        line = file.readLine(64);
        while (line.length() >= 8)
        {
            QHostAddress ip;
            if (!ip.setAddress(line.trimmed()))
            {
                line = file.readLine(64);
                continue;
            }
            lastGoodNodes.append(ip);
            line = file.readLine(64);
        }
    }
    // Read dynamically saved hosts from datastore
    // Note: Must reply with empty list if no nodes are stored, otherwise the entries in nodes.dat won't be queried.
    QTimer::singleShot(100, this, SIGNAL(requestLastKnownPeers())); //This signal isn't connected yet when this line is reached - wait 100msecs

    // Init bootstrap
    bootstrapStatus = -3;
    setBootstrapStatus(-2);
    bootstrapTimer = new QTimer(this);
    bootstrapTimer->setSingleShot(true);
    connect(bootstrapTimer, SIGNAL(timeout()), this, SLOT(performBootstrap()));

    // Init scanning timer
    networkScanTimer = new QTimer(this);
    networkScanTimer->setSingleShot(true);
    connect(networkScanTimer, SIGNAL(timeout()), this, SLOT(networkScanTimerEvent()));

    // Keepalive timer
    keepaliveTimer = new QTimer(this);
    keepaliveTimer->setSingleShot(false);
    connect(keepaliveTimer, SIGNAL(timeout()), this, SLOT(keepaliveTimerEvent()));
    keepaliveTimer->start(300000);

    // Begin bootstrap proses
    bootstrapTimer->start(1000);
    networkScanTimer->start(1500);

    totalScanHosts = 0;
    networkScanTimeouts = 0;
}

NetworkBootstrap::~NetworkBootstrap()
{
    bootstrapTimer->deleteLater();
    networkScanTimer->deleteLater();
    keepaliveTimer->deleteLater();
}

void NetworkBootstrap::receiveLastKnownPeers(QList<QHostAddress> peers)
{
   lastGoodNodes.append(peers);
   QListIterator<QHostAddress> i(lastGoodNodes);
   while (i.hasNext())
   {
       emit sendRequestAllBuckets(i.next());
   }
}

void NetworkBootstrap::performBootstrap()
{
    // bootstrapStatus
    // -2: Absoluut clueless en nowhere
    // -1: Besig om multicast bootstrap te probeer
    //  0: Besig om broadcast bootstrap te probeer
    //  1: Aanvaar broadcast, geen ander nodes naby, het bucket ID gegenereer.
    //  2: Suksesvol gebootstrap op broadcast
    //  3: Suksesvol gebootstrap op multicast
    // receive functions kan value na 1 of 2 set indien suksesvol.
    // gebruik setBootstrapStatus, hy emit sommer die change signal ook.

    switch(bootstrapStatus)
    {
    case NETWORK_BOOTATTEMPT_NONE:
        emit sendMulticastAnnounce();
        setBootstrapStatus(NETWORK_BOOTATTEMPT_MCAST);
        bootstrapTimer->start(7000);
        break;
    case NETWORK_BOOTATTEMPT_MCAST:
        emit sendBroadcastAnnounce();
        setBootstrapStatus(NETWORK_BOOTATTEMPT_BCAST);
        bootstrapTimer->start(8000);
        break;
    case NETWORK_BOOTATTEMPT_BCAST:
        setBootstrapStatus(NETWORK_BOOTATTEMPT_NONE);
        bootstrapTimer->start(1000);
        break;
    case NETWORK_BCAST_ALONE:
        emit sendMulticastAnnounce();
        emit sendBroadcastAnnounce();
        bootstrapTimer->start(15000);
        break;
    case NETWORK_BCAST:
        emit sendBroadcastAnnounce();
        bootstrapTimer->start(120000);
        break;
    case NETWORK_MCAST:
        emit sendMulticastAnnounce();
        bootstrapTimer->start(120000);
        break;
    }
}

void NetworkBootstrap::setBootstrapStatus(int status)
{
    if (bootstrapStatus != status)
    {
        bootstrapStatus = status;
        emit bootstrapStatusChanged(bootstrapStatus);
    }
}

void NetworkBootstrap::networkScanTimerEvent()
{
    networkScanTimeouts++;
    if (networkScanTimeouts == 3)
        networkScanTimeouts = 0;

    if (bootstrapStatus <= 0)
        networkScanTimer->start(1500);
    else if (bootstrapStatus == 1)
        networkScanTimer->start(10000);
    else
    {
        networkScanTimer->start(30000);
        if (networkScanTimeouts == 0)
            emit initiateBucketExchanges();
    }

    if (totalScanHosts == 0)
        return;

    qsrand((quint32)(QDateTime::currentMSecsSinceEpoch()&0xFFFFFFFF));
    for (int i = 0; i < 3; i++)
    {
        quint32 scanHostOffset = qrand() % totalScanHosts;
        QMapIterator<quint32, quint32> it(networkScanRanges);
        quint32 currentOffset = 0;
        QHostAddress scanHost;
        while (it.hasNext())
        {
            currentOffset += it.peekNext().value();
            if (scanHostOffset <= currentOffset)
            {
                scanHost = QHostAddress(it.peekNext().key() + scanHostOffset % it.peekNext().value());
                break;
            }
            else
                it.next();
        }
        //qDebug() << "NetworkBootstrap::networkScanTimerEvent(): Scanning host:" << scanHost.toString();
        emit sendRequestAllBuckets(scanHost);
    }
}

void NetworkBootstrap::keepaliveTimerEvent()
{
    if (bootstrapStatus == NETWORK_MCAST)
        emit sendMulticastAnnounceNoReply();
    else if (bootstrapStatus == NETWORK_BCAST)
        emit sendBroadcastAnnounceNoReply();
    // else wait patiently
}

int NetworkBootstrap::getBootstrapStatus()
{
    return bootstrapStatus;
}

void NetworkBootstrap::addNetworkScanRange(quint32 rangeBase, quint32 rangeLength)
{
    if (!networkScanRanges.contains(rangeBase))
    {
        networkScanRanges.insert(rangeBase, rangeLength);
        totalScanHosts += rangeLength;
    }
}

void NetworkBootstrap::removeNetworkScanRange(quint32 rangeBase)
{
    if (networkScanRanges.contains(rangeBase))
    {
        totalScanHosts -= networkScanRanges.value(rangeBase);
        networkScanRanges.remove(rangeBase);
    }
}

void NetworkBootstrap::initiateLinscan()
{
    // TODO: Check whether Timer is already running
    // Stop random network scanner
    networkScanTimer->stop();

    // Init linear scanning timer
    linscanTimer = new QTimer(this);
    connect(linscanTimer, SIGNAL(timeout()), this, SLOT(linscanTimerEvent()));

    // hackish PoC/PoS (depends on how you look at it), TODO: use only one timer
    linscanOutputTimer = new QTimer(this);
    connect(linscanOutputTimer, SIGNAL(timeout()), this, SLOT(linscanOutputTimerEvent()));

    // Initialize iterator to networkScanRanges
    linscanIterator = networkScanRanges.begin();

    // Start timer!
    linscanTimer->start(1500);

    qDebug() << "NetworkBootstrap::initiateLinscan(): Starting linear scan";
    emit appendChatLine("<font color=\"grey\">[LINSCAN] Linear scan started</font>");
}


void NetworkBootstrap::killLinscan()
{
    // TODO: Show amount of hosts added during linear scan
    linscanTimer->stop();
    delete linscanTimer;
    linscanOutputTimer->stop();
    delete linscanOutputTimer;
    emit appendChatLine("<font color=\"grey\">[LINSCAN] Linear scan stopped</font>");

    // Start bootstrap timer again
    networkScanTimer->start(2000);
}

void NetworkBootstrap::linscanTimerEvent()
{
    // TODO: Replace unicast with multicast
    quint32 rangeBase = linscanIterator.key();
    quint32 rangeLength = linscanIterator.value();
    qDebug() << "NetworkBootstrap::linscanTimerEvent(): Scanning range " << QHostAddress(rangeBase).toString() << " - " << QHostAddress(rangeBase + rangeLength).toString();
    QString msg = "<font color=\"grey\">[LINSCAN] Scanning range " + QHostAddress(rangeBase).toString() + " - " + QHostAddress(rangeBase + rangeLength).toString() + "</font>";
    emit appendChatLine(msg);

    for (quint32 host = rangeBase; host <= rangeBase + rangeLength; ++host)
    {
        QHostAddress scanHost = QHostAddress(host);
        if (scanHost.isNull() || scanHost == QHostAddress(QHostAddress::LocalHost) || scanHost == QHostAddress(QHostAddress::Null) || scanHost == QHostAddress(QHostAddress::Any))
            continue;
        //emit sendRequestAllBuckets(scanHost);
        linscanOutputQueue.push_back(scanHost);
    }
    if (!linscanOutputTimer->isActive())
        linscanOutputTimer->start(50);

    ++linscanIterator;
    if (linscanIterator==networkScanRanges.end())
        killLinscan();

}

// I remember the days when some of the routers crashed every 2 hours when all those Windows virii were scanning for friends like there were no tomorrow.
// These outages were not caused by the sheer volume of traffic (the bit throughput rate was not at all that high), but rather because it tried to send data
// to many hosts at once. Ethernet ARPs every IP before the first packet to a destination is sent, to look up the ethernet address associated with that particular
// IP address, so that the Ethernet layer can actually deliver the frame to the relevant NIC. Should we drop a bomb like this at an inopportune time, we might as
// well repeat what we have learned 6 years ago.
// I suggest moderating the output rate of the scanner. We really do not need 2 timers for this, but this was the quickest/easiest way to draft the concept.
void NetworkBootstrap::linscanOutputTimerEvent()
{
    if (!linscanOutputQueue.isEmpty())
        emit sendRequestAllBuckets(linscanOutputQueue.takeFirst());
    else
        linscanOutputTimer->stop();
}

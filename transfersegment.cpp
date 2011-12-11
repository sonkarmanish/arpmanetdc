#include "transfersegment.h"

TransferSegment::TransferSegment(QObject *parent) :
    QObject(parent)
{
    pDownloadBucketTable = 0;
}

TransferSegment::~TransferSegment(){}
void TransferSegment::transferTimerEvent(){}
void TransferSegment::setFileSize(quint64){}
//void TransferSegment::receivedPeerProtocolCapability(char){}
qint64 TransferSegment::getBytesReceivedNotFlushed(){return 0;}

void TransferSegment::setSegmentStart(qint64 start)
{
    segmentStart = start;
    segmentLength = segmentEnd - segmentStart > 0 ? segmentEnd - segmentStart : 0;
    calculateLastBucketParams();
}

void TransferSegment::setSegmentEnd(qint64 end)
{
    segmentEnd = end;
    if (segmentEnd - segmentStart > 0)
    {
        segmentLength = segmentEnd - segmentStart;
    }
    else
    {
        segmentLength = 0;
        segmentEnd = segmentStart;
    }
    calculateLastBucketParams();
}

void TransferSegment::calculateLastBucketParams()
{
    lastBucketNumber = segmentEnd % HASH_BUCKET_SIZE == 0 ? (segmentEnd >> 20) - 1: segmentEnd >> 20;
    lastBucketNumber = lastBucketNumber < 0 ? 0 : lastBucketNumber;
    lastBucketSize = segmentLength % HASH_BUCKET_SIZE == 0 ? HASH_BUCKET_SIZE : segmentLength % HASH_BUCKET_SIZE;
}

qint64 TransferSegment::getSegmentStart()
{
    return segmentStart;
}

qint64 TransferSegment::getSegmentEnd()
{
    return segmentEnd;
}

qint64 TransferSegment::getSegmentStartTime()
{
    return segmentStartTime;
}

QHostAddress TransferSegment::getSegmentRemotePeer()
{
    return remoteHost;
}

void TransferSegment::setTTH(QByteArray tth)
{
    TTH = tth;
}

void TransferSegment::setRemoteHost(QHostAddress host)
{
    remoteHost = host;
}

void TransferSegment::setDownloadBucketTablePointer(QHash<int, QByteArray *> *dbt)
{
    pDownloadBucketTable = dbt;
}

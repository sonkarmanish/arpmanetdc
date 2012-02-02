#include "containerthread.h"
#include <QDir>
#include "util.h"
#include <QDirIterator>

ContainerThread::ContainerThread(QObject *parent) : QObject(parent)
{

}

ContainerThread::~ContainerThread()
{

}

//Request all containers in a directory
void ContainerThread::requestContainers(QString containerDirectory)
{
    QDir cDir(containerDirectory);
    cDir.setFilter(QDir::Files);
    cDir.setNameFilters(QStringList("*." + QString(CONTAINER_EXTENSION)));

    //Get all container files in this directory
    QFileInfoList fileList = cDir.entryInfoList();

    QHash<QString, ContainerContentsType> containerHash;
    foreach (QFileInfo file, fileList)
    {
        //Process file
        ContainerContentsType c = processContainerFileIndex(file);
        //Add to hash
        containerHash.insert(file.fileName().left(file.fileName().indexOf("." + QString(CONTAINER_EXTENSION))), c);
    }

    emit returnContainers(containerHash);
}

//Save the containers to files in the directory specified
void ContainerThread::saveContainers(QHash<QString, ContainerContentsType> containerHash, QString containerDirectory)
{
    QHashIterator<QString, ContainerContentsType> i(containerHash);
    while (i.hasNext())
    {
        QString containerPath = containerDirectory + i.next().key();
        ContainerContentsType contents = i.value();

        //Parse all paths in the container to write index
        QMutableHashIterator<QString, quint64> k(contents.second);

        QHash<QString, QStringList> filePathHash;
        while (k.hasNext())
        {
            QString path = k.next().key();
            
            quint64 pathSize = 0;
            QStringList allFilePaths;            

            QDirIterator diri(path, QDir::Files | QDir::NoSymLinks | QDir::Readable, QDirIterator::Subdirectories);
            while (diri.hasNext())
            {
                diri.next();
            
                //Read file information
                allFilePaths.append(diri.filePath());

                //Update path size
                pathSize += diri.fileInfo().size();
            }

            //Set path size in containerHash
            k.setValue(pathSize);

            //Add entry to hash
            filePathHash.insert(path, allFilePaths);
        }

        //Write index to file
        writeContainerFileIndex(containerPath, contents);

        //Request hashes from DB
        emit requestTTHsFromPaths(filePathHash, containerPath); //FileList paths, parent path, container path
    }
}

//Return hashes from DB
void ContainerThread::returnTTHsFromPaths(QHash<QString, QList<ContainerLookupReturnStruct> > results, QString containerPath)
{
    QFile file(containerPath);
    if (!file.open(QIODevice::Append))
        return;

    //Iterate through all data in the container
    QHashIterator<QString, QList<ContainerLookupReturnStruct> > i(results);
    while (i.hasNext())
    {
        i.next();

        QString path = i.key();

        //Iterate through all files inside shared path
        QListIterator<ContainerLookupReturnStruct> k(i.value());
        while (k.hasNext())
        {
            QString filePath = k.peekNext().filePath;
            quint64 fileSize = k.peekNext().fileSize;
            QByteArray fileTTH = k.next().rootTTH;

            //===================================
            //        DATA ENTRY STRUCTURE
            //===================================
            // filePath     - String (variable)
            // sizeOfTTH    - quint16
            // fileTTH      - ByteArray
            // fileSize     - quint64
            
            QByteArray entry;
            entry.append(stringToByteArray(filePath));
            entry.append(sizeOfByteArray(&fileTTH));
            entry.append(fileTTH);
            entry.append(quint64ToByteArray(fileSize));

            //Write entry to file
            qint64 written = file.write(entry);
            if (written != entry.size())
                return;
        }
    }
}

//Process a file index (for local display)
ContainerContentsType ContainerThread::processContainerFileIndex(QFileInfo fileInfo)
{
    ContainerContentsType c;
    c.first = 0;

    QFile file(fileInfo.filePath());
    if (!file.open(QIODevice::ReadOnly))
        return c;

    //Read header from file
    QByteArray header = file.read(HEADER_LENGTH);
    if (header.size() < HEADER_LENGTH)
        return c;

    //===================================
    //            HEADER LAYOUT
    //===================================
    // totalBytes  - quint64
    // indexSize   - quint64

    quint64 totalBytes = getQuint64FromByteArray(&header);
    quint64 indexSize = getQuint64FromByteArray(&header);

    //Read index from file
    QByteArray index = file.read(indexSize);
    if (index.size() < indexSize)
        return c;

    //===================================
    //            INDEX LAYOUT
    //===================================
    // filePath  - String (variable size)
    // pathSize  - quint64

    while (!index.isEmpty())
    {
        QString filePath = getStringFromByteArray(&index);
        quint64 pathSize = getQuint64FromByteArray(&index);

        //Insert entry into hash
        c.second.insert(filePath, pathSize);
        c.first += pathSize;
    }

    return c;
}

//Start the writing process
bool ContainerThread::writeContainerFileIndex(QString filePath, ContainerContentsType contents)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly))
        return false;

    //Build index
    QByteArray index;
    QHashIterator<QString, quint64> i(contents.second);
    while (i.hasNext())
    {
        index.append(stringToByteArray(i.peekNext().key())); //filePath
        index.append(quint64ToByteArray(i.peekNext().value())); //pathSize
        i.next();
    }

    //Build header
    QByteArray header;
    header.append(quint64ToByteArray(contents.first)); //Container size
    header.append(quint64ToByteArray(index.size())); //Index size

    //Write header and index to file
    qint64 written;
    written = file.write(header);
    if (written != header.size())
        return false;

    written = file.write(index);
    if (written != index.size())
        return false;

    file.close();
    return true;
}
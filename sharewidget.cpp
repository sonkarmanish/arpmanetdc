#include "sharewidget.h"
#include "customtableitems.h"
#include "arpmanetdc.h"

ShareWidget::ShareWidget(ShareSearch *share, ArpmanetDC *parent)
{
    //Constructor
    pParent = parent;
    pShare = share;

    //Shares signals/slots
    connect(this, SIGNAL(updateShares(QList<QDir> *)), pShare, SLOT(updateShares(QList<QDir> *)), Qt::QueuedConnection);
    connect(this, SIGNAL(updateShares()), pShare, SLOT(updateShares()), Qt::QueuedConnection);
    connect(this, SIGNAL(requestShares()), pShare, SLOT(requestShares()), Qt::QueuedConnection);
    connect(pShare, SIGNAL(returnShares(QList<QDir>)), this, SLOT(returnShares(QList<QDir>)), Qt::QueuedConnection);

    //Magnet links signals/slots
    connect(this, SIGNAL(requestTTHFromPath(quint8, QString)), pShare, SLOT(requestTTHFromPath(quint8, QString)), Qt::QueuedConnection);
    connect(pShare, SIGNAL(returnTTHFromPath(quint8, QString, QByteArray, quint64)), this, SLOT(returnTTHFromPath(quint8, QString, QByteArray, quint64)), Qt::QueuedConnection);

    //Container signals/slots
    connect(this, SIGNAL(requestContainers(QString)), 
        pShare, SLOT(requestContainers(QString)), Qt::QueuedConnection);
    //connect(this, SIGNAL(processContainer(QString)), 
    //    pShare, SLOT(processContainer(QString)), Qt::QueuedConnection);
    connect(this, SIGNAL(saveContainers(QHash<QString, ContainerContentsType>, QString)),
        pParent, SLOT(queueSaveContainers(QHash<QString, ContainerContentsType>, QString)), Qt::QueuedConnection);
    connect(pShare, SIGNAL(returnContainers(QHash<QString, ContainerContentsType>)),
        this, SLOT(returnContainers(QHash<QString, ContainerContentsType>)), Qt::QueuedConnection);
    
    createWidgets();
    placeWidgets();
    connectWidgets();

    pContainerDirectory = QDir::currentPath();
    if (pContainerDirectory.endsWith("/"))
        pContainerDirectory.chop(1);
    pContainerDirectory.append(ArpmanetDC::settingsManager()->getSetting(SettingsManager::CONTAINER_DIRECTORY));

    //Populate container list
    emit requestContainers(pContainerDirectory);

    //Populate share list
    emit requestShares();
}

ShareWidget::~ShareWidget()
{
    //Destructor
    fileTree->deleteLater();
    fileModel->deleteLater();
    checkProxyModel->deleteLater();
}

void ShareWidget::createWidgets()
{
    pWidget = new QWidget((QWidget *)pParent);

    saveButton = new QPushButton(QIcon(":/ArpmanetDC/Resources/CheckIcon.png"), tr("Save shares"), pWidget);
    refreshButton = new QPushButton(QIcon(":/ArpmanetDC/Resources/RefreshIcon.png"), tr("Refresh shares"), pWidget);
    containerButton = new QPushButton(QIcon(":/ArpmanetDC/Resources/ContainerIcon.png"), tr("Show Containers"), pWidget);

    //========== DEBUG ==========
    //containerButton->setVisible(false);
    //========== END DEBUG ==========
    
    addContainerButton = new QPushButton(QIcon(":/ArpmanetDC/Resources/AddIcon.png"), tr("Add"), pWidget);
    addContainerButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    removeContainerButton = new QPushButton(QIcon(":/ArpmanetDC/Resources/DeleteIcon.png"), tr("Delete"), pWidget);
    removeContainerButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    renameContainerButton = new QPushButton(QIcon(""), tr("Rename"), pWidget);
    renameContainerButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    
    containerCombo = new QComboBox(pWidget);
    containerCombo->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    QSortFilterProxyModel *proxyModel = new QSortFilterProxyModel(this);
    proxyModel->setSortCaseSensitivity(Qt::CaseInsensitive);
    QAbstractItemModel *m = containerCombo->model();
    QAbstractItemView *v = containerCombo->view();
    proxyModel->setSourceModel(new QStandardItemModel(this));
    containerCombo->setModel(proxyModel);
    //containerCombo->view()->setModel(proxyModel);

    ((QStandardItemModel *)((QSortFilterProxyModel *)containerCombo->model())->sourceModel())->appendRow(new QStandardItem(QIcon(":/ArpmanetDC/Resources/ContainerIcon.png"), "Sample container"));
        
    ContainerContentsType contents;
    contents.first = 0;
    pContainerHash.insert(tr("Sample container"), contents);

    containerModel = new QStandardItemModel(0, 4, this);
    containerModel->setHeaderData(0, Qt::Horizontal, tr("Filename"));
    containerModel->setHeaderData(1, Qt::Horizontal, tr("Filepath"));
    containerModel->setHeaderData(2, Qt::Horizontal, tr("Filesize"));
    containerModel->setHeaderData(3, Qt::Horizontal, tr("Bytes"));

    pParentItem = containerModel->invisibleRootItem();

    containerTreeView = new CDropTreeView(tr("Drag files/folders here to add..."), pWidget);
    containerTreeView->setModel(containerModel);
    containerTreeView->setAcceptDrops(true);
    containerTreeView->setContextMenuPolicy(Qt::CustomContextMenu);
    containerTreeView->setSelectionBehavior(QAbstractItemView::SelectRows);
    containerTreeView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    containerTreeView->setColumnWidth(0, 200);
    containerTreeView->setColumnWidth(1, 100);
    containerTreeView->hideColumn(3);
            
    fileModel = new QFileSystemModel(this);
    //fileModel->setFilter(QDir::Dirs | QDir::Drives | QDir::NoDotAndDotDot);
    fileModel->setRootPath("c:/");
    fileModel->setResolveSymlinks(false);

    checkProxyModel = new CheckableProxyModel(this);
    checkProxyModel->setSourceModel(fileModel);
    checkProxyModel->setSortCaseSensitivity(Qt::CaseInsensitive);
        
    fileTree = new CDragTreeView(tr("Drag files/folders here to add"), pWidget);
    fileTree->setModel(checkProxyModel);
    //fileTree->sortByColumn(0, Qt::AscendingOrder);
    fileTree->setColumnWidth(0, 500);
    fileTree->setUniformRowHeights(true);
    fileTree->setSortingEnabled(false);
    fileTree->setSelectionMode(QAbstractItemView::ExtendedSelection);
    fileTree->header()->setHighlightSections(false);
    fileTree->setContextMenuPolicy(Qt::CustomContextMenu);
    //fileTree->setDragDropMode(QAbstractItemView::DragOnly);
    fileTree->setDragEnabled(true);

    checkProxyModel->setDefaultCheckState(Qt::Unchecked);    
    //checkProxyModel->sort(0, Qt::AscendingOrder);

    busyLabel = new QLabel(tr("<font color=\"red\">Busy loading directory structure. Please wait...</font>"), pWidget);

    //Context menu
    contextMenu = new QMenu(pWidget);
    calculateMagnetAction = new QAction(QIcon(":/ArpmanetDC/Resources/MagnetIcon.png"), tr("Copy magnet link"), pWidget);
    contextMenu->addAction(calculateMagnetAction);
    contextMenu->addSeparator();

    addToMagnetListAction = new QAction(QIcon(":/ArpmanetDC/Resources/AddIcon.png"), tr("Add to magnet list"), pWidget);
    copyListToClipboardAction = new QAction(QIcon(":/ArpmanetDC/Resources/CopyIcon.png"), tr("Copy list to clipboard"), pWidget);
    clearMagnetListAction = new QAction(QIcon(":/ArpmanetDC/Resources/RemoveIcon.png"), tr("Clear magnet list"), pWidget);
    contextMenu->addAction(addToMagnetListAction);
    contextMenu->addAction(copyListToClipboardAction);
    contextMenu->addSeparator();
    contextMenu->addAction(clearMagnetListAction);

    magnetRemoveListContextMenu = new QMenu(tr("Remove from magnet list"), pWidget);
    magnetRemoveListContextMenu->setIcon(QIcon(":/ArpmanetDC/Resources/DeleteIcon.png"));
    contextMenu->addMenu(magnetRemoveListContextMenu);

    containerContextMenu = new QMenu((QWidget *)pParent);
    removeContainerEntryAction = new QAction(QIcon(":/ArpmanetDC/Resources/RemoveIcon.png"), tr("Remove entry"), pWidget);
    containerContextMenu->addAction(removeContainerEntryAction);
}

void ShareWidget::placeWidgets()
{
    QHBoxLayout *hlayout = new QHBoxLayout;
    hlayout->addWidget(refreshButton);
    hlayout->addWidget(containerButton);
    hlayout->addSpacing(10);
    hlayout->addWidget(busyLabel);
    hlayout->addStretch(1);
    hlayout->addWidget(saveButton);

    QGridLayout *topContainerLayout = new QGridLayout;
    topContainerLayout->addWidget(containerCombo, 0, 0);
    topContainerLayout->addWidget(renameContainerButton, 0, 1);
    topContainerLayout->addWidget(addContainerButton, 0, 2);
    topContainerLayout->addWidget(removeContainerButton, 0, 3);

    QVBoxLayout *containerLayout = new QVBoxLayout;
    containerLayout->addLayout(topContainerLayout);
    containerLayout->addWidget(containerTreeView);
    containerLayout->setContentsMargins(0,0,0,0);

    QWidget *containerWidget = new QWidget();
    containerWidget->setLayout(containerLayout);
    containerWidget->setContentsMargins(0,0,0,0);

    splitter = new QSplitter(Qt::Horizontal, pWidget);
    splitter->addWidget(fileTree);
    splitter->addWidget(containerWidget);

    QVBoxLayout *vlayout = new QVBoxLayout;
    //vlayout->addLayout(hTreeLayout);
    vlayout->addWidget(splitter);
    vlayout->addLayout(hlayout);
    vlayout->setContentsMargins(0,0,0,0);

    splitter->widget(1)->hide();
    splitter->setCollapsible(0,false);
    splitter->setCollapsible(1, false);
       
    pWidget->setLayout(vlayout);

    QList<int> sizeList;
    int halfWidth = ((QWidget *)pParent)->size().width()/2;
    sizeList << halfWidth << halfWidth;
    splitter->setSizes(sizeList);
}

void ShareWidget::connectWidgets()
{
    //Context menu
    connect(fileTree, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(contextMenuRequested(const QPoint &)));
    connect(containerTreeView, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(containerContextMenuRequested(const QPoint &)));
    
    //View
    connect(containerTreeView, SIGNAL(droppedURLList(QList<QUrl>)), this, SLOT(droppedURLList(QList<QUrl>)));
    connect(containerTreeView, SIGNAL(keyPressed(Qt::Key)), this, SLOT(containerTreeViewKeyPressed(Qt::Key)));

    //Combo box
    connect(containerCombo, SIGNAL(currentIndexChanged(const QString &)), this, SLOT(switchedContainer(const QString &)));

    //Models
    connect(checkProxyModel, SIGNAL(checkedNodesChanged()), this, SLOT(selectedItemsChanged()));
    connect(fileModel, SIGNAL(directoryLoaded(QString)), this, SLOT(pathLoaded(QString)));

    //Buttons
    connect(saveButton, SIGNAL(clicked()), this, SLOT(saveSharePressed()));
    connect(refreshButton, SIGNAL(clicked()), this, SLOT(refreshButtonPressed()));
    connect(containerButton, SIGNAL(clicked()), this, SLOT(containerButtonPressed()));
    connect(addContainerButton, SIGNAL(clicked()), this, SLOT(addContainerButtonPressed()));
    connect(removeContainerButton, SIGNAL(clicked()), this, SLOT(removeContainerButtonPressed()));
    connect(renameContainerButton, SIGNAL(clicked()), this, SLOT(renameContainerButtonPressed()));

    //Actions
    connect(calculateMagnetAction, SIGNAL(triggered()), this, SLOT(calculateMagnetActionPressed()));
    connect(removeContainerEntryAction, SIGNAL(triggered()), this, SLOT(removeContainerEntryActionPressed()));
    connect(addToMagnetListAction, SIGNAL(triggered()), this, SLOT(addToMagnetListActionPressed()));
    connect(copyListToClipboardAction, SIGNAL(triggered()), this, SLOT(copyListToClipboardActionPressed()));
    connect(clearMagnetListAction, SIGNAL(triggered()), this, SLOT(clearMagnetListActionPressed()));
    connect(magnetRemoveListContextMenu, SIGNAL(triggered(QAction *)), this, SLOT(magnetRemoveListContextMenuPressed(QAction *)));
}

void ShareWidget::changeRoot(QString path)
{
    QFileInfo fi(path);
    if (fi.isDir())
        pLoadingPaths.append(path);

    fileModel->setRootPath(path);
    //QModelIndex index = checkProxyModel->index(0,0, fileModel->index(path,0));
    //QModelIndex index2 = checkProxyModel->mapFromSource(fileModel->index(path));
    
    if (pSharesList.contains(path))
        checkProxyModel->setSourceIndexCheckedState(fileModel->index(path,0), true);
    //fileTree->setRootIndex(index);
}

void ShareWidget::pathLoaded(QString path)
{
    pLoadingPaths.removeAll(path);
    pLoadingPaths.removeAll(path + "/");

    //Check if loading is complete
    if (pLoadingPaths.isEmpty() && finishedLoading)
    {
        busyLabel->hide();   
    }
}

void ShareWidget::selectedItemsChanged()
{
    //Selection changed in the checked proxy model
}

void ShareWidget::saveSharePressed()
{
    QModelIndexList selectedFiles;
    QModelIndexList selectedDirectories;

    //Extract selected files and directories
    CheckableProxyModelState *state = checkProxyModel->checkedState();
    state->checkedLeafSourceModelIndexes(selectedFiles);
    state->checkedBranchSourceModelIndexes(selectedDirectories);
    delete state;

    //Construct list of selected directory paths
    QList<QDir> *dirList = new QList<QDir>();
    QList<QString> strList;
    foreach (const QModelIndex index, selectedDirectories)
    {
        strList.append(index.data(QFileSystemModel::FilePathRole).toString());
        if (!strList.last().endsWith("/"))
            strList.last().append("/");
    }
    foreach (const QModelIndex index, selectedFiles)
    {
        strList.append(index.data(QFileSystemModel::FilePathRole).toString());
    }

    QList<QString> finalList;
    QList<QString> containerList;

    //Add container directory to share automatically
    containerList.append(pContainerDirectory);

    //Massively complex system to ensure that only parent directories are shared across sharing selections and ALL containers

    //Pass 1 : Remove all subdirectories between containers
    foreach (ContainerContentsType c, pContainerHash)
    {
        QHashIterator<QString, quint64> i(c.second);
        while (i.hasNext())
        {
            i.next();
            QMutableListIterator<QString> k(containerList);
            while (k.hasNext())
            {
                k.next();
                if (k.value().contains(i.key())) //If existing directory is a subdirectory of new directory - remove old one
                {
                    k.remove();
                }
            }
            
            //Add new directory if it doesn't exist yet
            if (!containerList.contains(i.key()))
                containerList.append(i.key());
        }
    }

    //Pass 2 : Remove all subdirectories from strList where parent directories are contained in containerList
    QMutableListIterator<QString> i(strList);
    while (i.hasNext())
    {
        i.next();
        QListIterator<QString> k(containerList);
        while (k.hasNext())
        {
            if (i.value().contains(k.next())) //A path in strList is a subdirectory of a path in containerList
            {
                i.remove();
                break;
            }
        }
    }

    //Pass 3 : Remove all subdirectories from containerList where parent directories are contained in strList
    QMutableListIterator<QString> j(containerList);
    while (j.hasNext())
    {
        j.next();
        QListIterator<QString> m(strList);
        while (m.hasNext())
        {
            if (j.value().contains(m.next())) //A path in containerList is a subdirectory of a path in strList
            {
                j.remove();
                break;
            }
        }
    }

    //Combine the two
    finalList.append(strList);
    finalList.append(containerList);

    //Convert to QDirs
    foreach (QString str, finalList)
        dirList->append(QDir(str));

    pShare->stopParsing();
    pShare->stopHashing();

    //Update shares
    emit updateShares(dirList);
    //Save containers
    emit saveContainers(pContainerHash, pContainerDirectory);
    //Let GUI know
    emit saveButtonPressed();
}

void ShareWidget::refreshButtonPressed()
{
    emit updateShares();
    emit saveButtonPressed();
}

void ShareWidget::containerButtonPressed()
{
    if (splitter->widget(1)->isHidden())
    {
        splitter->widget(1)->setVisible(true);
        containerButton->setText(tr("Hide Containers"));
    }
    else
    {
        splitter->widget(1)->hide();
        containerButton->setText(tr("Show Containers"));
    }
}

void ShareWidget::addContainerButtonPressed()
{
    QString name = QInputDialog::getText((QWidget *)pParent, tr("ArpmanetDC"), tr("Please enter a name for the container"));
    if (!name.isEmpty() && !pContainerHash.contains(name))
    {
        //Add container to hash list
        ContainerContentsType c;
        pContainerHash.insert(name, c);

        //Add container name to combo box
        containerCombo->addItem(QIcon(":/ArpmanetDC/Resources/ContainerIcon.png"), name);
        containerCombo->setCurrentIndex(containerCombo->count()-1);

        //Sort list of containers
        containerCombo->view()->model()->sort(0);
        
        //Clear model
        containerModel->removeRows(0, containerModel->rowCount());

        //Enable tree view if it was disabled
        removeContainerButton->setEnabled(true);
        renameContainerButton->setEnabled(true);
        containerTreeView->setEnabled(true);
        containerTreeView->setPlaceholderText(tr("Drag files/folders here to add..."));
    }
    else if (!name.isEmpty())
        QMessageBox::information(pParent, tr("ArpmanetDC"), tr("A container with that name already exists. Please try another name."));
}

void ShareWidget::removeContainerButtonPressed()
{
    if (containerCombo->count() == 0)
        return;

    QString containerName = containerCombo->currentText();
    if (QMessageBox::information((QWidget *)pParent, tr("ArpmanetDC"), tr("Are you sure you want to remove the container <b>%1</b>?")
        .arg(containerName), QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes)
    {
        //Remove from combobox
        containerCombo->removeItem(containerCombo->currentIndex());

        //Remove from hash
        pContainerHash.remove(containerName);

        //Clear model
        containerModel->removeRows(0, containerModel->rowCount());

        //Disable tree view if no containers exist
        if (containerCombo->count() == 0)
        {
            removeContainerButton->setEnabled(false);
            renameContainerButton->setEnabled(false);
            containerTreeView->setEnabled(false);
            containerTreeView->setPlaceholderText(tr("To start using containers, click Add..."));
        }
        else
        {
            switchedContainer(containerCombo->currentText());            
        }
    }
}

void ShareWidget::renameContainerButtonPressed()
{
    QString oldName = containerCombo->currentText();
    QString newName = QInputDialog::getText((QWidget *)pParent, tr("ArpmanetDC"), tr("Please enter a new name for container <b>%1</b>").arg(oldName));
    if (!newName.isEmpty() && !pContainerHash.contains(newName))
    {
        //Replace container
        ContainerContentsType c = pContainerHash.value(oldName);
        pContainerHash.remove(oldName);
        pContainerHash.insert(newName, c);

        //Add container name to combo box
        containerCombo->removeItem(containerCombo->currentIndex());
        containerCombo->addItem(QIcon(":/ArpmanetDC/Resources/ContainerIcon.png"), newName);
        containerCombo->setCurrentIndex(containerCombo->count()-1);        

        //Sort list of containers
        containerCombo->view()->model()->sort(0);
    }
    else if (!newName.isEmpty())
        QMessageBox::information(pParent, tr("ArpmanetDC"), tr("A container with that name already exists. Please try another name."));
}

//Calculate a magnet link from a file
void ShareWidget::calculateMagnetActionPressed()
{
    QModelIndex index = fileTree->selectionModel()->selectedRows().first(); 
    QString filePath = fileModel->filePath(checkProxyModel->mapToSource(index));
    
    emit requestTTHFromPath((int)SingleMagnetType, filePath);

    QWhatsThis::showText(contextMenu->pos(), tr("Calculating hash. Please wait..."));
}

void ShareWidget::removeContainerEntryActionPressed()
{
    QList<QModelIndex> rows = containerTreeView->selectionModel()->selectedRows();
    QList<QString> deletePaths;
    for (int i = 0; i < rows.size(); i++)
    {
        //Get selected entry values
        QModelIndex index = containerTreeView->selectionModel()->selectedRows().at(i);
        QString filePath = containerModel->itemFromIndex(containerModel->index(index.row(), 1))->text();
        deletePaths.append(filePath);
        quint64 fileSize = containerModel->itemFromIndex(containerModel->index(index.row(), 3))->text().toLongLong();
        
        QString containerName = containerCombo->currentText();

        //Remove from hash
        pContainerHash[containerName].first -= fileSize;
        pContainerHash[containerName].second.remove(filePath);
    }

    //Remove from list
    foreach (QString path, deletePaths)
    {
        QList<QStandardItem *> foundItems = containerModel->findItems(path, Qt::MatchExactly, 1);
        if (!foundItems.isEmpty())
            containerModel->removeRow(foundItems.first()->row());
    }
}

void ShareWidget::switchedContainer(const QString &name)
{
    //Remove model contents
    containerModel->removeRows(0, containerModel->rowCount());

    //Get previous data from hash
    QHashIterator<QString, quint64> i(pContainerHash.value(name).second);
    while (i.hasNext())
    {
        QFileInfo fi(i.peekNext().key());
        QString fileName = fi.fileName();
        QString filePath = fi.filePath();
        quint64 fileSize = i.peekNext().value();
        if (fileName.isEmpty())
            fileName = QDir(filePath).dirName();

        //Add contents to model
        QList<QStandardItem *> row;
        QIcon icon;
        
        if (fi.isFile())
        {
            QString suffix = fi.suffix();
            icon = pParent->resourceExtractorObject()->getIconFromName(suffix);
        }
        else if (fi.isDir())
        {
            QString f = tr("folder");
            icon = pParent->resourceExtractorObject()->getIconFromName(f);
        }

        row.append(new CStandardItem(CStandardItem::CaseInsensitiveTextType, fileName, icon));
        row.append(new CStandardItem(CStandardItem::CaseInsensitiveTextType, filePath));
        row.append(new CStandardItem(CStandardItem::SizeType, bytesToSize(fileSize)));
        row.append(new CStandardItem(CStandardItem::IntegerType, tr("%1").arg(fileSize)));

        pParentItem->appendRow(row);  
        
        i.next();
    }

    containerModel->sort(1);
}

//Add a magnet link to the list
void ShareWidget::addToMagnetListActionPressed()
{
    QModelIndex index = fileTree->selectionModel()->selectedRows().first(); 
    QString filePath = fileModel->filePath(checkProxyModel->mapToSource(index));
    
    emit requestTTHFromPath((int)MultipleListMagnetType, filePath);

    QWhatsThis::showText(contextMenu->pos(), tr("Calculating hash. Please wait..."));
}

//Copy list to clipboard
void ShareWidget::copyListToClipboardActionPressed()
{
    //Generate a clipboard entry
    QClipboard *clipboard = QApplication::clipboard();
    QString clipboardEntry;
    foreach (QString magnet, pMagnetList)
    {
        if (!clipboardEntry.isEmpty())
            clipboardEntry.append("\n");
        clipboardEntry.append(magnet);
    }
    clipboard->setText(clipboardEntry);
}

//Clear the magnet list
void ShareWidget::clearMagnetListActionPressed()
{
    pMagnetList.clear();
}

//Remove a magnet link from the list
void ShareWidget::magnetRemoveListContextMenuPressed(QAction *action)
{
    QString filePath = action->data().toString();

    pMagnetList.remove(filePath);

    //Generate a clipboard entry
    QClipboard *clipboard = QApplication::clipboard();
    QString clipboardEntry;
    foreach (QString magnet, pMagnetList)
    {
        if (!clipboardEntry.isEmpty())
            clipboardEntry.append("\n");
        clipboardEntry.append(magnet);
    }
    clipboard->setText(clipboardEntry);
}

void ShareWidget::returnTTHFromPath(quint8 type, QString filePath, QByteArray tthRoot, quint64 fileSize)
{
    if (!tthRoot.isEmpty())
    {
        //TTH was found in database
        QByteArray base32TTH(tthRoot);
        base32Encode(base32TTH);
        
        QString tthStr(base32TTH);

        QFileInfo fi(filePath);
        
        //Generate magnet link
        QString fileName = fi.fileName();
        QString magnetLink = tr("magnet:?xt=urn:tree:tiger:%1&xl=%2&dn=%3").arg(tthStr).arg(fileSize).arg(fileName.replace(" ", "+"));

        QClipboard *clipboard = QApplication::clipboard();

        //Copy to clipboard
        if (type == SingleMagnetType)
        {
            //Just copy the single magnet link to the clipboard
            clipboard->setText(magnetLink);

            QWhatsThis::showText(contextMenu->pos(), tr("Magnet link copied to clipboard"));
        }
        else if (type == MultipleListMagnetType)
        {
            //Save the entry to the list and copy list to clipboard
            pMagnetList.insert(filePath, magnetLink);

            //Generate a clipboard entry
            QString clipboardEntry;
            foreach (QString magnet, pMagnetList)
            {
                if (!clipboardEntry.isEmpty())
                    clipboardEntry.append("\n");
                clipboardEntry.append(magnet);
            }
            clipboard->setText(clipboardEntry);
            QWhatsThis::showText(contextMenu->pos(), tr("Magnet link added to list"));
        }
    }
}

//Return slot for current shares in the DB
void ShareWidget::returnShares(QList<QDir> shares)
{
    while (!shares.isEmpty())
    {
        QDir currentPath = shares.takeFirst();
        pSharesList.append(currentPath.absolutePath());
        changeRoot(currentPath.absolutePath());
        while (currentPath.cdUp())
            changeRoot(currentPath.absolutePath());

        //QModelIndex index = fileModel->index(currentPath.absolutePath(), 0);
        //bool res = checkProxyModel->setSourceIndexCheckedState(index, true);
    }
    finishedLoading = true;
}

//Return the containers requested
void ShareWidget::returnContainers(QHash<QString, ContainerContentsType> containerHash)
{
    //Clear model
    containerModel->removeRows(0, containerModel->rowCount());

    //Clear container list
    containerCombo->clear();

    //Replace the current container hash
    pContainerHash.clear();
    pContainerHash = containerHash;

    QHashIterator<QString, ContainerContentsType> i(pContainerHash);
    while (i.hasNext())
    {
        QString name = i.next().key();
        containerCombo->addItem(QIcon(":/ArpmanetDC/Resources/ContainerIcon.png"), name);
    }

    //Sort list of containers
    containerCombo->view()->model()->sort(0);

    //Disable tree view if no containers exist
    if (containerCombo->count() == 0)
    {
        renameContainerButton->setEnabled(false);
        removeContainerButton->setEnabled(false);
        containerTreeView->setEnabled(false);
        containerTreeView->setPlaceholderText(tr("To start using containers, click Add..."));
    }
    else
    {
        renameContainerButton->setEnabled(true);
        removeContainerButton->setEnabled(true);
        //Enable tree view if it was disabled
        containerTreeView->setEnabled(true);
        containerTreeView->setPlaceholderText(tr("Drag files/folders here to add..."));            
    }

    
}

//Context menu for magnets
void ShareWidget::contextMenuRequested(const QPoint &pos)
{
    if (fileTree->selectionModel()->selectedRows().size() == 0)
        return;

    QModelIndex index = fileTree->selectionModel()->selectedRows().first();
    QString filePath = fileModel->filePath(checkProxyModel->mapToSource(index));
    QFileInfo fi(filePath);
    if (fi.isFile())
    {
        calculateMagnetAction->setVisible(true);
        addToMagnetListAction->setVisible(true);
    }
    else
    {
        calculateMagnetAction->setVisible(false);
        addToMagnetListAction->setVisible(false);
    }

    //Generate submenu
    magnetRemoveListContextMenu->clear();
        
    QHashIterator<QString, QString> i(pMagnetList);
    while (i.hasNext())
    {
        i.next();
        QAction *action = new QAction(QIcon(":/ArpmanetDC/Resources/MagnetIcon.png"), i.key(), this);
        action->setData(i.key());
        magnetRemoveListContextMenu->addAction(action);
    }

    if (pMagnetList.isEmpty())
    {
        magnetRemoveListContextMenu->setTitle(tr("No entries in list"));
        magnetRemoveListContextMenu->setEnabled(false);
        clearMagnetListAction->setEnabled(false);
        copyListToClipboardAction->setEnabled(false);
    }
    else
    {
        magnetRemoveListContextMenu->setTitle(tr("Remove magnet from list"));
        magnetRemoveListContextMenu->setEnabled(true);
        clearMagnetListAction->setEnabled(true);
        copyListToClipboardAction->setEnabled(true);
    }
                
    QPoint globalPos = fileTree->viewport()->mapToGlobal(pos);
    contextMenu->popup(globalPos);
}

//Context menu for containers
void ShareWidget::containerContextMenuRequested(const QPoint &pos)
{
    if (containerTreeView->selectionModel()->selectedRows().size() == 0)
        return;

    QPoint globalPos = containerTreeView->viewport()->mapToGlobal(pos);
    containerContextMenu->popup(globalPos);
}

void ShareWidget::containerTreeViewKeyPressed(Qt::Key key)
{
    if (containerTreeView->selectionModel()->selectedRows().size() == 0)
        return;

    if (key == Qt::Key_Delete)
        removeContainerEntryAction->trigger();
}

//Item dropped in container
void ShareWidget::droppedURLList(QList<QUrl> list)
{
    foreach(QUrl url, list)
    {
        QString path = url.toString();
        if (path.startsWith("file:///"))
            path.remove("file:///");
        QFileInfo fi(path);

        QString fileName = fi.fileName();
        QString filePath = fi.filePath();
        if (fileName.isEmpty())
            fileName = filePath;
        quint64 fileSize = 0;
        if (fi.isFile())
            fileSize = fi.size();
        else if (!filePath.endsWith("/"))
            filePath += "/";

        //Get current contents of container
        ContainerContentsType contents = pContainerHash.value(containerCombo->currentText());
        //Add fileSize to total fileSize
        contents.first += fileSize;

        bool add = true;
        QHashIterator<QString, quint64> i(contents.second);
        while (i.hasNext())
        {
            QString p = i.next().key();
            if (p == filePath)
            {
                //Don't add duplicates
                add = false;
                break;
            }

            if (filePath.contains(p))
            {
                //Trying to add a lower level folder
                contents.second.remove(p);
                containerModel->removeRow(containerModel->findItems(p, Qt::MatchExactly, 1).first()->row());
            }

            if (p.contains(filePath))
            {
                //Trying to add a higher level folder
                contents.second.remove(p);
                containerModel->removeRow(containerModel->findItems(p, Qt::MatchExactly, 1).first()->row());
            }
        }
        
        if (!add)
            continue;

        //Add the filePath and fileSize to contents
        contents.second.insert(filePath, fileSize);
                
        //Add contents to list
        pContainerHash[containerCombo->currentText()] = contents;
        
        //Add contents to model
        QList<QStandardItem *> row;
        QIcon icon;
        
        if (fi.isFile())
        {
            QString suffix = fi.suffix();
            icon = pParent->resourceExtractorObject()->getIconFromName(suffix);
        }
        else
        {
            QString f = tr("folder");
            icon = pParent->resourceExtractorObject()->getIconFromName(f);
        }
            
        row.append(new CStandardItem(CStandardItem::CaseInsensitiveTextType, fileName, icon));
        row.append(new CStandardItem(CStandardItem::CaseInsensitiveTextType, filePath));
        row.append(new CStandardItem(CStandardItem::SizeType, bytesToSize(fileSize)));
        row.append(new CStandardItem(CStandardItem::IntegerType, tr("%1").arg(fileSize)));

        pParentItem->appendRow(row);      
    }

    containerModel->sort(1);
}

QWidget *ShareWidget::widget()
{
    //TODO: Return widget containing all search widgets
    return pWidget;
}

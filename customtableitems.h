/* This file is part of ArpmanetDC. Copyright (C) 2012
 * Source code can be found at http://code.google.com/p/arpmanetdc/
 * 
 * ArpmanetDC is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * ArpmanetDC is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with ArpmanetDC.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef CUSTOMTABLEITEMS_H
#define CUSTOMTABLEITEMS_H

#include <QStyledItemDelegate>
#include <QStandardItem>
#include <QTableView>
#include <QVariant>
#include <QPen>
#include <QString>
#include <QSet>
#include <QProgressBar>
#include <QDateTime>
#include <QTreeView>
#include <QListView>
#include <QLineEdit>
#include <QUrl>
#include <QRgb>
#include "transfer.h"

//Bitmap colours
const static QColor downloadedColor(79, 189, 54); //Green
const static QColor notDownloadedColor(255,255,255); //White

const static QColor uploadedColor(37, 149, 214); //Blue
const static QColor notUploadedColor(255,255,255); //While

const static QColor downloadingColor(79, 189, 54, 192); //Light green
const static QColor uploadingColor(37, 149, 214, 192); //Light blue

const static QColor hashingColor(228, 66, 243); //Magenta
const static QColor flushingColor(255, 240, 0); //Yellow

//Initialize the color map
static QMap<char, QColor> initColourMapValues() {
    QMap<char, QColor> map;
    map.insert(SegmentNotDownloaded, notDownloadedColor);
    map.insert(SegmentDownloaded, downloadedColor);
    map.insert(SegmentCurrentlyDownloading, downloadingColor);
    map.insert(SegmentCurrentlyHashing, hashingColor);
    map.insert(SegmentCurrentlyFlushing, flushingColor);
    map.insert(SegmentCurrentlyUploading, uploadingColor);
    map.insert(SegmentNotUploaded, notUploadedColor);
    map.insert(SegmentUploaded, uploadedColor);
    return map;
}

//Initialize the color map
static QMap<char, QColor> initColourMapValuesDarker() {
    QMap<char, QColor> map;
    map.insert(SegmentNotDownloaded, notDownloadedColor.darker(150));
    map.insert(SegmentDownloaded, downloadedColor.darker(150));
    map.insert(SegmentCurrentlyDownloading, downloadingColor.darker(150));
    map.insert(SegmentCurrentlyHashing, hashingColor.darker(150));
    map.insert(SegmentCurrentlyFlushing, flushingColor.darker(150));
    map.insert(SegmentCurrentlyUploading, uploadingColor.darker(150));
    map.insert(SegmentNotUploaded, notUploadedColor.darker(150));
    map.insert(SegmentUploaded, uploadedColor.darker(150));
    return map;
}

static const QMap<char, QColor> BITMAP_COLOUR_MAP = initColourMapValues();
static const QMap<char, QColor> BITMAP_COLOUR_MAP_DARKER = initColourMapValuesDarker();

//Custom delegate to display HTML code in QTableView
class HTMLDelegate : public QStyledItemDelegate
{
public:
    HTMLDelegate(QTableView *tableView);
protected:
    void paint ( QPainter * painter, const QStyleOptionViewItem & option, const QModelIndex & index ) const;
    QSize sizeHint ( const QStyleOptionViewItem & option, const QModelIndex & index ) const;
private:
    QPen pGridPen;
};

//Custom delegate to display progress bars in QTableView
class ProgressDelegate : public QStyledItemDelegate
{
public:
    ProgressDelegate();
protected:
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const;
};

//Custom delegate to display bitmap in QTableView
class BitmapDelegate : public QStyledItemDelegate
{
public:
    BitmapDelegate();
    static void removePixmap(quint8 id) {renderedPixmaps.remove(id);}
    static void clearRenderedPixmaps() {renderedPixmaps.clear();}
protected:
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const;
private:
    static QHash<quint8, QPair<quint8, QPixmap> > renderedPixmaps;
};


//Custom QStandardItem to allow for integer/size sorting
class CStandardItem : public QStandardItem
{
public:
    enum CStandardItemType {IntegerType, DoubleType, SizeType, RateType, CaseInsensitiveTextType, PriorityType, ProgressType, BitmapType, DateType, TimeDurationType, PathType};
    CStandardItem(CStandardItemType type, const QString &value) : QStandardItem(value) {pType = type; setEditable(false);}
    CStandardItem(CStandardItemType type, const QString &value, const QIcon &icon) : QStandardItem(icon, value) {pType = type; setEditable(false);}

    void setFormat(const QString &format);
    QString format();

    bool operator<(const QStandardItem &other) const;
private:
    CStandardItemType pType;
    QString pFormat;
};

//Custom tab widget with public access to the tabBar
class CTabWidget : public QTabWidget
{
public:
    CTabWidget(QWidget *parent = 0);
    QTabBar *tabBar() const { return (QTabWidget::tabBar()); }
};

//Custom progressbar that shows text on top
class TextProgressBar : public QProgressBar
{
public:
    //No implementation
    TextProgressBar(QString text, QWidget *parent = 0) : QProgressBar(parent) {pText = text;}

    QString topText() const;
    void setTopText(const QString &text);
protected:
    void paintEvent(QPaintEvent *);

private:
    QString pText;
};

//TableView that emits keypresses
class CKeyTableView : public QTableView
{
    Q_OBJECT
public:
    CKeyTableView(QWidget *parent = 0) : QTableView(parent) {}

    void keyPressEvent(QKeyEvent *event);
signals:
    void keyPressed(Qt::Key key, QString keyStr);
};

//TreeView that can display placeholder text
class CTextTreeView : public QTreeView
{
    Q_OBJECT
public:
    CTextTreeView(const QString &text = "", QWidget *parent = 0) : QTreeView(parent) {pText = text;}

    void setPlaceholderText(const QString &text) {pText = text;}
    QString placeholderText() {return pText;}

    void keyPressEvent(QKeyEvent *event);
signals:
    void keyPressed(Qt::Key key);
protected:
    void paintEvent(QPaintEvent *);
private:
    QString pText;

};

//Draggable QTreeView
class CDragTreeView : public CTextTreeView
{
public:
    CDragTreeView(QString text = "", QWidget *parent = 0) : CTextTreeView(text, parent) {} //Empty constructor

    void mousePressEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);

    void dragMoveEvent(QDragMoveEvent *event);
    //void dragEnterEvent(QDragEnterEvent *event) {}
    //void dropEvent(QDropEvent *event) {}

private:
    QPoint dragStartPosition;
};

//Droppble QTreeView
class CDropTreeView : public CTextTreeView
{
    Q_OBJECT
public:
    CDropTreeView(QString text = "", QWidget *parent = 0) : CTextTreeView(text, parent) {} //Empty constructor
    
    void dragMoveEvent(QDragMoveEvent *event);
    void dragEnterEvent(QDragEnterEvent *event);
    void dropEvent(QDropEvent *event);

signals:
    void droppedURLList(QList<QUrl> list);
};

//QLineEdit that emits keypresses
class KeyLineEdit : public QLineEdit
{
    Q_OBJECT
public:
    KeyLineEdit(QWidget *parent = 0) : QLineEdit(parent) {}
    KeyLineEdit(const QString &text, QWidget *parent = 0) : QLineEdit(text, parent) {}

    bool event(QEvent *event);
    void keyPressEvent(QKeyEvent *event);
signals:
    void keyPressed(Qt::Key key, QString keyStr);
};


#endif

#include "blockarea.h"

#include <QDebug>
#include <QGridLayout>
#include <QLabel>
#include <QPainter>
#include <QPixmap>
#include <QRandomGenerator>
#include <QScrollBar>
#include <QtMath>
#include <iostream>

BlockArea::BlockArea(QWidget *parent)
    : QAbstractScrollArea(parent), blockCount(0), scale(1), selectedFile(nullptr), zoomEnabled(false) {
    setAttribute(Qt::WA_StyledBackground);
    setVerticalScrollBarPolicy(Qt::ScrollBarPolicy::ScrollBarAlwaysOn);
    connect(verticalScrollBar(), &QScrollBar::valueChanged, this, [this](int value) { onStatusBarChanged(value); });
}

void BlockArea::setPhysicalBlockCount(int count) {
    physicalBlockCount = count;
    blockCount = physicalBlockCount / scale;

    updateParams(viewport()->size());
}

void BlockArea::setScale(int s) {
    scale = s;
    blockCount = physicalBlockCount / scale;

    updateParams(viewport()->size());
}

void BlockArea::scan(QString path) { ExtentInfo::scan(path.toStdString()); }

void BlockArea::setSelectedFile(QString path) {
    if (selectedFile) {
        delete selectedFile;
    }
    selectedFile = new FileNode(getFileInfoByPath(path.toStdString()));

    viewport()->update();
}

void BlockArea::unsetSelectedFile() {
    if (selectedFile) {
        delete selectedFile;
        selectedFile = nullptr;
    }

    viewport()->update();
}

void BlockArea::paintEvent(QPaintEvent *) {
    if (blockCount == 0) {
        return;
    }
    QPainter p(viewport());
    p.setPen(Qt::GlobalColor::gray);
    p.setBrush(Qt::BrushStyle::NoBrush);
    QSize size(blockSize, blockSize);
    int index = yOffset * xNum;
    for (int y = 0; y < yNum; y++) {
        for (int x = 0; x < xNum; x++) {
            QPoint pos(blockSpace + x * (blockSize + blockSpace), blockSpace + y * (blockSize + blockSpace));

            std::size_t count = getExtCountFromBlockRange(index * scale, (index + 1) * scale);
            if (count == 0) {
                p.setBrush(Qt::GlobalColor::white);
            } else {
                p.setBrush(Qt::GlobalColor::green);
            }
            if (selectedFile) {
                std::size_t start = index * scale;
                std::size_t end = (index + 1) * scale;
                for (const auto &ext : selectedFile->exts) {
                    if ((start >= ext.first && start < ext.second) || (end > ext.first && end <= ext.second) ||
                        (start < ext.first && end > ext.second)) {
                        p.setBrush(Qt::GlobalColor::red);
                        break;
                    }
                }
            }

            p.drawRect(QRect(pos, size));

            if (index++ >= blockCount) {
                return;
            }
        }
    }
}

void BlockArea::resizeEvent(QResizeEvent *event)
{
    updateParams(event->size());

    verticalScrollBar()->setRange(0, yMax - yNum + 1);
}

void BlockArea::mouseReleaseEvent(QMouseEvent *event)
{
    auto pos = event->pos();
    int mx = (pos.x() - blockSpace) % (blockSize + blockSpace);
    int my = (pos.y() - blockSpace) % (blockSize + blockSpace);
    if (mx > blockSize || my > blockSize) {
        return;
    }
    int ix = (pos.x() - blockSpace) / (blockSize + blockSpace);
    int iy = (pos.y() - blockSpace) / (blockSize + blockSpace);

    int index = xNum * (iy + yOffset) + ix;
    if (index < blockCount) {
        unsetSelectedFile();
        emit mouseClicked(index);
    }
}

void BlockArea::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key::Key_Control) {
        zoomEnabled = true;
    }
}

void BlockArea::keyReleaseEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key::Key_Control) {
        zoomEnabled = false;
    }
}

void BlockArea::wheelEvent(QWheelEvent *event)
{
    if (zoomEnabled) {
        //        emit zoomed(event->angleDelta().x(), event->angleDelta().y());
        if (event->angleDelta().y() < 0) {
            setScale(scale * 2);
        } else {
            setScale(scale / 2 ? scale / 2 : 1);
        }
    } else {
        QAbstractScrollArea::wheelEvent(event);
    }
}

void BlockArea::onStatusBarChanged(int value)
{
    yOffset = value;
}

void BlockArea::updateParams(QSize size) {
    xNum = (size.width() - blockSpace) / (blockSize + blockSpace);
    yNum = (size.height() - blockSpace) / (blockSize + blockSpace);
    yMax = blockCount / xNum;

    verticalScrollBar()->setRange(0, yMax - yNum + 1);

    viewport()->update();
}

#ifndef BLOCKAREA_H
#define BLOCKAREA_H

#include "extentinfo.h"

#include <QAbstractScrollArea>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QWheelEvent>
#include <vector>

class BlockArea : public QAbstractScrollArea, public ExtentInfo {
    Q_OBJECT
public:
    explicit BlockArea(QWidget *parent);

    void setPhysicalBlockCount(int count);
    void setScale(int scale);
    int getScale() { return scale; }
    void scan(QString path);
    void setSelectedFile(QString path);
    void unsetSelectedFile();

signals:
    void mouseClicked(int index);
    void zoomed(int x, int y);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    void onStatusBarChanged(int value);
    void updateParams(QSize size);

private:
    int xNum;
    int yNum;
    int yMax;
    int yOffset;
    int blockCount;

    int scale;
    int physicalBlockCount;

    FileNode *selectedFile;

    bool zoomEnabled;

    static constexpr int blockSize = 8;
    static constexpr int blockSpace = 0;
};

#endif // BLOCKAREA_H

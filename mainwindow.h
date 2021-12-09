#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <extentinfo.h>

#include <QMainWindow>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    void setupUi();
    void setupSignal();

    std::size_t getFilesystemBlockCount(const char *path);
    void fillTable(std::vector<ExtentInfo::FileNode> &fileList);

private:
    Ui::MainWindow *ui;
};
#endif // MAINWINDOW_H

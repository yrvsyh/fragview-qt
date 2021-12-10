#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "extentinfo.h"

#include <QDebug>
#include <QFileDialog>
#include <QProcess>
#include <QRandomGenerator>
#include <QTableWidgetItem>
#include <QtMath>
#include <iostream>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow) {
    ui->setupUi(this);

    setupUi();
    setupSignal();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setupUi() {
    ui->tableWidget->verticalHeader()->hide();
    //    ui->tableWidget->setSortingEnabled(true);
    ui->tableWidget->setColumnCount(3);
    ui->tableWidget->setHorizontalHeaderLabels(QStringList() << tr("碎片数") << tr("文件大小") << tr("文件路径"));
    ui->tableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    ui->tableWidget->horizontalHeader()->setStretchLastSection(true);
    ui->tableWidget->setColumnWidth(0, 80);
    ui->tableWidget->setColumnWidth(1, 100);
    ui->tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->tableWidget->verticalHeader()->setDefaultSectionSize(6);
    QFont font("Source Code Pro", 8, -1);
    ui->tableWidget->setFont(font);
}

void MainWindow::setupSignal() {
    connect(ui->openDir, &QAction::triggered, this, [this] {
        auto dialog = new QFileDialog(this);
        dialog->setWindowTitle("选择文件夹");
        dialog->setFileMode(QFileDialog::FileMode::DirectoryOnly);
        if (dialog->exec()) {
            auto path = dialog->selectedFiles()[0];

            std::size_t count = getFilesystemBlockCount(path.toStdString().c_str());
            ui->blockArea->scan(path);
            ui->blockArea->setPhysicalBlockCount(count);
            ui->blockArea->setScale(1024);
        }
    });

    connect(ui->sortByExtCount, &QAction::triggered, this, [this] {
        auto fileList = ui->blockArea->getFileListSortByExtCount(20);
        fillTable(fileList);
    });

    connect(ui->sortByFileSize, &QAction::triggered, this, [this] {
        auto fileList = ui->blockArea->getFileListSortByFileSize(20);
        fillTable(fileList);
    });

    connect(ui->blockArea, &BlockArea::mouseClicked, this, [this](int index) {
        int scale = ui->blockArea->getScale();

        auto start = index * scale;
        auto end = (index + 1) * scale;
        auto fileList = ui->blockArea->getFileListFromBlockRange(start, end);

        if (!fileList.empty()) {
            fillTable(fileList);
        }

        ui->statusbar->showMessage(QString("index[%1] scale[%4] block[%2-->%3] files[%5]")
                                       .arg(index)
                                       .arg(index * scale)
                                       .arg((index + 1) * scale)
                                       .arg(scale)
                                       .arg(fileList.size()));
    });

    connect(ui->tableWidget, &QTableWidget::cellClicked, this, [this](int row, int) {
        auto path = ui->tableWidget->item(row, 2)->text();
        ui->blockArea->setSelectedFile(path);
    });
}

std::size_t MainWindow::getFilesystemBlockCount(const char *path) {
    QProcess process;
    QString cmd = QString("/bin/bash -c \"df -B4k %1 | sed -n 2p | awk \'{print $2}\'\"").arg(path);
    process.start(cmd);
    process.waitForFinished(-1);
    QString stdout = process.readAllStandardOutput();
    ui->statusbar->showMessage(QString("Filesystem 4K-Block Count: %1").arg(stdout));
    return stdout.toULongLong();
}

void MainWindow::fillTable(std::vector<ExtentInfo::FileNode> &fileList) {
    ui->tableWidget->clear();
    ui->tableWidget->setRowCount(fileList.size());
    ui->tableWidget->setHorizontalHeaderLabels(QStringList() << tr("碎片数") << tr("文件大小") << tr("文件路径"));
    for (std::size_t i = 0; i < fileList.size(); i++) {
        auto &file = fileList[i];
        ui->tableWidget->setItem(i, 0, new QTableWidgetItem(QString::number(file.extCnt)));
        QString formattedSize = this->locale().formattedDataSize(file.size);
        ui->tableWidget->setItem(i, 1, new QTableWidgetItem(formattedSize));
        ui->tableWidget->setItem(i, 2, new QTableWidgetItem(QString(file.path.c_str())));
    }
}

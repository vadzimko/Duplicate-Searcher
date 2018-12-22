#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    ui->tree->setUniformRowHeights(true);
    setWindowTitle(QString("Duplicate Searcher"));

    qRegisterMetaType<QVector<int> >("QVector<int>");
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_open_clicked()
{
    if (!finished) {
        aborted = true;
    }

    directory = QFileDialog::getExistingDirectory(this, "Select directory", QString(), QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    dirView = "\'..." + directory.mid(qMax(0, directory.length() - 50), 50) + "\'";
    ui->directoryName->setText("Selected " + dirView);
}

void MainWindow::start()
{
    timer.start();
    aborted = false;
    finished = false;

    sizeSum = 0;
    sizeProcessed = 0;
    filesNumber = 0;
    filesNumberProcessed = 0;

    ui->tree->clear();
    ui->directoryName->setText("Scanning " + dirView + " ...");
}

void MainWindow::finish()
{
    finished = true;
    status = Status::CHILLING;
    if (!aborted) {
        ui->directoryName->setText("Scanned " + dirView + " in " + QString::number(timer.elapsed() / 1000.0) + " sec");
    } else {
        ui->directoryName->setText("Scanning " + dirView + " aborted, took " + QString::number(timer.elapsed() / 1000.0) + " sec");
    }    
    ui->status->setText("Finished");
}

void MainWindow::on_scan_clicked()
{
    if (directory.length() == 0) {
        ui->directoryName->setText("Choose directory to scan!");
        return;
    }
    if (!finished) {
        finish();
    }
    start();

    QFuture<void> searcher = QtConcurrent::run(this, &MainWindow::search);
    QFuture<void> refresher = QtConcurrent::run(this, &MainWindow::refreshStatus);
}

bool MainWindow::cancelRequested() {
    if (aborted) {
        if (!finished) {
            finish();
        }
        return true;
    }
    return false;
}

void MainWindow::search() {
    status = Status::SCANNING_FILES;

    QDirIterator it(directory, QDir::Files | QDir::NoDotAndDotDot | QDir::NoSymLinks, QDirIterator::Subdirectories);
    QVector<QString> files;
    while (it.hasNext()) {
        auto file = it.next();
        if (it.fileInfo().isSymLink()) {
            continue;
        }

        files.push_back(file);
        filesNumber++;
        if (cancelRequested()) {
            return;
        }
    }

    QMap<qint64, QVector<QString>> groups;
    QVector<qint64> order;
    for (auto file : files) {
        QFileInfo fileinfo(file);
        qint64 size = fileinfo.size();
        sizeSum += size;
        if (groups.count(size)) {
            groups.find(size)->push_back(file);
        } else {
            groups.insert(size, QVector<QString>(1, file));
            order.push_back(size);
        }
        if (cancelRequested()) {
            return;
        }
    }
    std::sort(order.begin(), order.end());
    std::reverse(order.begin(), order.end());

    status = Status::SEARCHING_DUPLICATES;
    QFuture<void> inserter;
    QCryptographicHash hashSHA256(QCryptographicHash::Sha256);
    for (auto size : order) {
        auto group = groups[size];
        if (group.empty()) {
            continue;
        }
        if (group.size() == 1) {
            sizeProcessed += QFileInfo(group[0]).size();
            filesNumberProcessed++;
            continue;
        }

        QMap<QByteArray, QVector<QString>> hashes;
        for (QString name : group) {
            hashSHA256.reset();
            QFile file(name);
            if (file.open(QIODevice::ReadOnly)) {
                hashSHA256.addData(&file);
            }
            QByteArray hash = hashSHA256.result();
            if (hashes.count(hash)) {
                hashes.find(hash)->push_back(name);
            } else {
                hashes.insert(hash, QVector<QString>(1, name));
            }
            if (cancelRequested()) {
                return;
            }
            sizeProcessed += QFileInfo(group[0]).size();
            filesNumberProcessed++;
        }

        for (auto dups : hashes) {
            if (dups.size() > 1) {
                inserter.waitForFinished();
                inserter = QtConcurrent::run(this, &MainWindow::addDuplicates, dups);
            }
            if (cancelRequested()) {
                return;
            }
        }
        if (cancelRequested()) {
            return;
        }
    }

    finish();
}

void MainWindow::on_cancel_clicked()
{
    aborted = true;
}

void MainWindow::on_delete_button_clicked()
{
    auto selected = ui->tree->selectedItems();
    if (selected.empty()) {
        return;
    }

    QMessageBox messageBox;
    messageBox.setText("Delete selected files?");
    messageBox.setStandardButtons(QMessageBox::No | QMessageBox::Yes);

    if (messageBox.exec() == QMessageBox::Yes) {
        for (auto file : selected) {
            if (QFile::remove(file->text(0))) {
                file->setSelected(false);
                file->setDisabled(true);
            }
        }
    }
}

void MainWindow::refreshStatus()
{
    qint16 dots = 1;
    while (!finished) {
        QString dot = "";
        for (int i = 0; i < dots / 5; i++) {
            dot += '.';
        }
        switch (status) {
            case Status::CHILLING : {
                ui->status->setText("Finished");
                break;
            }
            case Status::SCANNING_FILES : {
                qint32 time = timer.elapsed() / 1000;
                ui->status->setText("Indexing files " + QString::number(time) + " sec" + dot);
                break;
            }
            case Status::SEARCHING_DUPLICATES : {
                double progress = qMin(100.0, sizeProcessed * 33.0 / sizeSum + filesNumberProcessed * 67.0 / filesNumber);
                ui->status->setText(QString().sprintf("%.1f", progress) + "%" + dot);
                break;
            }
        }
        dots = (dots % 15) + 1;
        QThread::msleep(100);
    }
}

void MainWindow::addDuplicates(QVector<QString> dups) {
    QVector<QFileInfo> fileInfo(dups.size());
    for (int i = 0; i < dups.size(); i++) {
        fileInfo[i] = QFileInfo(dups[i]);
    }

    auto group = new QTreeWidgetItem(ui->tree);
    group->setText(0, QString::number(dups.size()) + QString(" duplicates ") + sizeFormat(fileInfo[0].size()) + " each");

    for (auto info : fileInfo) {
       auto child = new QTreeWidgetItem();
       child->setText(0, info.filePath());
       group->addChild(child);

       if (cancelRequested()) {
           break;
       }
    }
    ui->tree->addTopLevelItem(group);
}

QString MainWindow::sizeFormat(double size)
{
    QStringList list;
    list << "KB" << "MB" << "GB" << "TB";

    QStringListIterator it(list);
    QString unit("bytes");

    while (size >= 1024.0 && it.hasNext())
    {
        unit = it.next();
        size /= 1024.0;
    }
    return QString::number(size, 'f', 2) + " " + unit;
}


void MainWindow::on_tree_itemDoubleClicked(QTreeWidgetItem *item, int column)
{
    if (item->parent() != nullptr) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(item->text(column)));
    }
}

void MainWindow::on_tree_itemClicked(QTreeWidgetItem *item)
{
    if (item->parent() == nullptr) {
        item->setSelected(false);
    }
}

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTime>
#include <QCommonStyle>
#include <QDesktopWidget>
#include <QDir>
#include <QFileDialog>
#include <QMessageBox>
#include <QDirIterator>
#include <QDesktopServices>
#include <QtGlobal>
#include <QDebug>
#include <QThread>
#include <QFuture>
#include <QtConcurrent/QtConcurrent>
#include <QTreeWidgetItem>

namespace Ui {
class MainWindow;
}

enum class Status
{
    CHILLING,
    SCANNING_FILES,
    SEARCHING_DUPLICATES
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_open_clicked();
    void on_cancel_clicked();
    void on_delete_button_clicked();
    void on_scan_clicked();

    void search();

    void on_tree_itemDoubleClicked(QTreeWidgetItem *item, int column);

    void on_tree_itemClicked(QTreeWidgetItem *item);

private:
    Ui::MainWindow *ui;
    QTime timer;
    QString directory;
    QString dirView;


    bool cancelRequested();
    void start();
    void finish();
    void refreshStatus();
    void addDuplicates(QVector<QString>);
    QString sizeFormat(double);

    bool aborted;
    bool finished = true;
    Status status;

    qint64 sizeSum;
    qint64 sizeProcessed;

    qint32 filesNumber;
    qint32 filesNumberProcessed;
};

#endif // MAINWINDOW_H

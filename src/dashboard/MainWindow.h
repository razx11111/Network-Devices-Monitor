#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTcpSocket>
#include <QTableWidget>
#include <QLabel>
#include <QVBoxLayout>

// Include your shared protocol definition
#include "protocol.h"

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void connectToServer();
    void onConnected();
    void onReadyRead();
    void onSocketError(QAbstractSocket::SocketError socketError);

private:
    void setupUI();
    void processJson(const QByteArray &data);
    void addLogEntry(const QString &ts, const QString &src, const QString &sev, const QString &app, const QString &msg);

    QTcpSocket *socket;
    QTableWidget *logTable;
    QLabel *statusLabel;
    QByteArray buffer; // Handle TCP fragmentation
};

#endif // MAINWINDOW_H
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTcpSocket>
#include <QTableWidget>
#include <QLabel>
#include <QTimer> // <-- Add this
#include "protocol.h"

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void attemptConnection(); // Renamed from connectToServer
    void onConnected();
    void onDisconnected();    // <-- Add this
    void onReadyRead();
    void onSocketError(QAbstractSocket::SocketError socketError);

private:
    void setupUI();
    void processJson(const QByteArray &data);
    void addLogEntry(const QString &ts, const QString &src, const QString &sev, const QString &app, const QString &msg);

    QTcpSocket *socket;
    QTimer *reconnectTimer;   // <-- Add this
    QTableWidget *logTable;
    QLabel *statusLabel;
    QByteArray buffer;
};

#endif // MAINWINDOW_H
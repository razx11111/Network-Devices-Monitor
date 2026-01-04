#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTcpSocket>
#include <QTableWidget>
#include <QLabel>
#include <QTimer> 
#include <QLineEdit>      
#include <QPushButton>    
#include <QComboBox>

#include "protocol.h"

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void attemptConnection(); // Renamed from connectToServer
    void onConnected();
    void onDisconnected();    
    void onReadyRead();
    void onSocketError(QAbstractSocket::SocketError socketError);
    void sendSearchRequest();

private:
    void setupUI();
    void processJson(const QByteArray &data);
    void addLogEntry(const QString &ts, const QString &src, const QString &pid, const QString &sev, const QString &app, const QString &msg);

    QTcpSocket *socket;
    QTimer *reconnectTimer;   
    QTableWidget *logTable;
    QLabel *statusLabel;
    QByteArray buffer;
    QLineEdit *searchBar;
    QComboBox *severityFilter;
    QPushButton *searchButton;
};

#endif 
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
#include <QGroupBox>      
#include <QVBoxLayout>

#include "protocol.h"

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QString user, QString pass, QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void attemptConnection();
    void onConnected();
    void onDisconnected();    
    void onReadyRead();
    void onSocketError(QAbstractSocket::SocketError socketError);
    
    void sendSearchRequest();
    void requestStats();
    void requestAgents(); 
    
    void onActivateAgent();
    void onBlockAgent();
    void onAddAgent();

private:
    void setupUI();
    void applyModernStyle(); // <--- Adauga asta
    void processJson(const QByteArray &data);
    void addLogEntry(const QString &ts, const QString &src, const QString &pid, 
                     const QString &fac, const QString &sev, const QString &app, const QString &msg);

    QTcpSocket *socket;
    QTimer *reconnectTimer;   
    QTimer *statsTimer;
    QTimer *agentsTimer; 

    QString m_username;
    QString m_password;
    QByteArray buffer;

    QLabel *statusLabel;
    QTableWidget *logTable;
    QLineEdit *searchBar;
    QComboBox *severityFilter;
    QPushButton *searchButton;
    QGroupBox *statsBox;
    QLabel *lblInfoCount;
    QLabel *lblWarnCount;
    QLabel *lblErrCount;
    QTableWidget *topSourcesTable; 
    
    QTableWidget *agentTable;
    QPushButton *btnActivate;
    QPushButton *btnBlock;
    QPushButton *btnAddAgent;
};

#endif 

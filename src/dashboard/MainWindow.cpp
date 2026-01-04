#include "MainWindow.h"
#include <QCoreApplication>
#include <QHeaderView>
#include <QJsonDocument>
#include <QJsonObject>
#include <QHBoxLayout>
#include <QtEndian> 
#include <QJsonArray>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setupUI();

    socket = new QTcpSocket(this);
    reconnectTimer = new QTimer(this);

    // Conectare semnale socket
    connect(socket, &QTcpSocket::connected, this, &MainWindow::onConnected);
    connect(socket, &QTcpSocket::disconnected, this, &MainWindow::onDisconnected);
    connect(socket, &QTcpSocket::readyRead, this, &MainWindow::onReadyRead);
    
    // FIX 2: Înlocuit 'error' cu 'errorOccurred' (Qt 5.15+)
    connect(socket, &QTcpSocket::errorOccurred, this, &MainWindow::onSocketError);

    // Timer pentru reconectare
    connect(reconnectTimer, &QTimer::timeout, this, &MainWindow::attemptConnection);

    // Încearcă conectarea la pornire
    attemptConnection();
}

MainWindow::~MainWindow() {}

void MainWindow::attemptConnection() {
    // Only try if we are not already connected or trying
    if (socket->state() == QAbstractSocket::UnconnectedState) {
        statusLabel->setText("Status: Searching for Server...");
        statusLabel->setStyleSheet("font-weight: bold; color: orange; padding: 5px;");
        
        // Connect to localhost (Docker forwarded port)
        socket->connectToHost("127.0.0.1", 9999);
    }
}

void MainWindow::onConnected() {
    reconnectTimer->stop();
    statusLabel->setText("Status: Connected (Live Stream)");
    statusLabel->setStyleSheet("font-weight: bold; color: #00ff00; font-size: 14px; padding: 5px; background-color: #222;");

    // --- SEND HANDSHAKE (ADMIN ROLE) ---
    QJsonObject authObj;
    authObj["role"] = "ADMIN";
    QJsonDocument doc(authObj);
    QByteArray payload = doc.toJson(QJsonDocument::Compact);

    AMPHeader header;
    header.version = 1;
    header.message_type = CMD_AUTH;
    header.reserved = 0;
    header.payload_length = qToBigEndian((uint32_t)payload.size());

    socket->write((char*)&header, sizeof(header));
    socket->write(payload);
}

void MainWindow::onDisconnected() {
    statusLabel->setText("Status: Disconnected. Retrying...");
    statusLabel->setStyleSheet("font-weight: bold; color: red; padding: 5px;");
    
    // Încearcă reconectarea la fiecare 2 secunde
    reconnectTimer->start(2000);
}

void MainWindow::onSocketError(QAbstractSocket::SocketError socketError) {
    // Dacă eroarea este "Connection Refused" (Docker oprit), doar reîncercăm
    if (socket->state() == QAbstractSocket::UnconnectedState) {
        if (!reconnectTimer->isActive()) {
            reconnectTimer->start(2000); 
        }
    } else {
        statusLabel->setText("Status: Error - " + socket->errorString());
        statusLabel->setStyleSheet("font-weight: bold; color: red; padding: 5px;");
    }
}

void MainWindow::setupUI() {
    setWindowTitle("Network Devices Monitor - Admin Dashboard");
    resize(1000, 600);

    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    QVBoxLayout *layout = new QVBoxLayout(centralWidget);

    // --- SEARCH BAR (Phase 2) ---
    QHBoxLayout *searchLayout = new QHBoxLayout();

    searchBar = new QLineEdit(this);
    searchBar->setPlaceholderText("Search messages (e.g., 'failed', 'error')...");
    searchBar->setStyleSheet("padding: 5px; color: white; background-color: #444; border: 1px solid #666;");

    severityFilter = new QComboBox(this);
    severityFilter->addItems({"ALL", "EMERGENCY", "ALERT", "CRITICAL", "ERROR", "WARNING", "NOTICE", "INFO", "DEBUG"});
    severityFilter->setStyleSheet("padding: 5px; color: white; background-color: #444;");

    searchButton = new QPushButton("Search Logs", this);
    searchButton->setStyleSheet("background-color: #007acc; color: white; padding: 5px; font-weight: bold;");
    connect(searchButton, &QPushButton::clicked, this, &MainWindow::sendSearchRequest);

    searchLayout->addWidget(searchBar);
    searchLayout->addWidget(severityFilter);
    searchLayout->addWidget(searchButton);
    
    // Header Status & PID
    QHBoxLayout *statusLayout = new QHBoxLayout();
    statusLabel = new QLabel("Status: Connecting...", this);
    statusLabel->setStyleSheet("font-weight: bold; color: orange; font-size: 14px; padding: 5px;");
    

    statusLayout->addWidget(statusLabel);
    
    // Table
    logTable = new QTableWidget(this);
    logTable->setColumnCount(6);
    logTable->setHorizontalHeaderLabels({"Timestamp", "Source", "PID", "Severity", "App", "Message"});
    
    // Styling
    logTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    logTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents); // Timestamp
    logTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents); // PID
    logTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents); // Severity
    
    // Dark Mode Style
    logTable->setStyleSheet("QTableWidget { background-color: #2d2d2d; color: white; gridline-color: #db1134; }"
                            "QHeaderView::section { background-color:rgb(0, 0, 128); color: white; padding: 4px; border: 1px solid #db1134; }");

    layout->addLayout(statusLayout);
    layout->addLayout(searchLayout); // Adăugare Search Bar
    layout->addWidget(logTable);
}

void MainWindow::onReadyRead() {
    buffer.append(socket->readAll());

    while (true) {
        if (static_cast<size_t>(buffer.size()) < sizeof(AMPHeader)) {
            return; // Wait for full header
        }

        AMPHeader *header = reinterpret_cast<AMPHeader*>(buffer.data());
        uint32_t payloadLen = qFromBigEndian(header->payload_length);

        if (static_cast<size_t>(buffer.size()) < sizeof(AMPHeader) + payloadLen) {
            return; // Wait for full payload
        }

        // Remove header
        buffer.remove(0, sizeof(AMPHeader));
        
        // Extract payload
        QByteArray payload = buffer.left(payloadLen);
        buffer.remove(0, payloadLen);

        processJson(payload);
    }
}

void MainWindow::processJson(const QByteArray &data) {
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull() || !doc.isObject()) return;
    QJsonObject obj = doc.object();

    // CASE 1: Search Results (Array)
    if (obj.contains("results")) {
        QJsonArray results = obj["results"].toArray(); // Acum funcționează datorită #include <QJsonArray>
        logTable->setRowCount(0); // Clear table
        
        for (const auto &val : results) {
            QJsonObject log = val.toObject();
            addLogEntry(
                log["timestamp"].toString(),
                log["hostname"].toString(),
                log["pid"].toString(),
                log["severity"].toString(),
                log["application"].toString(),
                log["message"].toString()
            );
        }
        return;
    }

    // CASE 2: Live Log (Single Object)
    if (obj.contains("status") && obj["status"].toString() == "ok") return;

    QString ts = obj.value("timestamp").toString();
    QString src = obj.value("hostname").toString();
    if (src.isEmpty()) src = obj.value("source").toString();
    
    QString pid = obj.value("pid").toString();
    QString sev = obj.value("severity").toString();
    QString app = obj.value("application").toString();
    QString msg = obj.value("message").toString();

    addLogEntry(ts, src, pid, sev, app, msg);
}

void MainWindow::addLogEntry(const QString &ts, const QString &src, const QString &pid, const QString &sev, const QString &app, const QString &msg) {
    int row = 0; // Insert la început
    logTable->insertRow(row);

    logTable->setItem(row, 0, new QTableWidgetItem(ts));
    logTable->setItem(row, 1, new QTableWidgetItem(src));
    logTable->setItem(row, 2, new QTableWidgetItem(pid));
    logTable->setItem(row, 3, new QTableWidgetItem(sev));
    logTable->setItem(row, 4, new QTableWidgetItem(app));
    logTable->setItem(row, 5, new QTableWidgetItem(msg));

    // Color Codingd
    QColor color = Qt::white;
    if (sev.contains("ERR") || sev.contains("CRIT") || sev.contains("FATAL") || sev.contains("EMERG") || sev.contains("ALERT")) color = QColor("#ff4d4d"); 
    else if (sev.contains("WARNING")) color = QColor("orange");

    for (int i=0; i<6; i++) {
        logTable->item(row, i)->setForeground(color);
    }
    
    // Limitare rânduri pentru performanță
    if (logTable->rowCount() > 200) logTable->removeRow(200);
}


void MainWindow::sendSearchRequest() {
    if (socket->state() != QAbstractSocket::ConnectedState) return;

    // 1. Build JSON Request
    QJsonObject searchObj;
    searchObj["keyword"] = searchBar->text();
    searchObj["severity"] = severityFilter->currentText();
    searchObj["limit"] = "50";

    QJsonDocument doc(searchObj);
    QByteArray payload = doc.toJson(QJsonDocument::Compact);

    // 2. Send Header + Payload
    AMPHeader header;
    header.version = 1;
    header.message_type = CMD_SEARCH; // Ensure CMD_SEARCH is defined in protocol.h (value 4)
    header.reserved = 0;
    header.payload_length = qToBigEndian((uint32_t)payload.size());

    socket->write((char*)&header, sizeof(header));
    socket->write(payload);
    
    // Curăță tabelul pentru a afișa rezultatele
    logTable->setRowCount(0);
}
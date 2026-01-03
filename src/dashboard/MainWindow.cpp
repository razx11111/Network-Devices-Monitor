#include "MainWindow.h"
#include <QHeaderView>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtEndian> // For qToBigEndian / qFromBigEndian

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setupUI();

    socket = new QTcpSocket(this);
    connect(socket, &QTcpSocket::connected, this, &MainWindow::onConnected);
    connect(socket, &QTcpSocket::readyRead, this, &MainWindow::onReadyRead);
    connect(socket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred),
            this, &MainWindow::onSocketError);

    // Auto-connect on startup
    connectToServer();
}

MainWindow::~MainWindow() {}

void MainWindow::setupUI() {
    setWindowTitle("Network Devices Monitor - Admin Dashboard");
    resize(1000, 600);

    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    QVBoxLayout *layout = new QVBoxLayout(centralWidget);

    // Header
    statusLabel = new QLabel("Status: Connecting...", this);
    statusLabel->setStyleSheet("font-weight: bold; color: orange; font-size: 14px; padding: 5px;");
    layout->addWidget(statusLabel);

    // Table
    logTable = new QTableWidget(this);
    logTable->setColumnCount(5);
    logTable->setHorizontalHeaderLabels({"Timestamp", "Source", "Severity", "App", "Message"});
    
    // Styling
    logTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    logTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    logTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    
    // Dark Mode Style
    logTable->setStyleSheet("QTableWidget { background-color: #2d2d2d; color: white; gridline-color: #555; }"
                            "QHeaderView::section { background-color: #333; color: white; padding: 4px; border: 1px solid #555; }");
    
    layout->addWidget(logTable);
}

void MainWindow::connectToServer() {
    // Docker maps port 9999 to localhost
    socket->connectToHost("127.0.0.1", 9999);
}

void MainWindow::onConnected() {
    statusLabel->setText("Status: Connected (Live Stream)");
    statusLabel->setStyleSheet("font-weight: bold; color: #00ff00; font-size: 14px; padding: 5px; background-color: #222;");

    // --- 1. SEND HANDSHAKE (ADMIN ROLE) ---
    QJsonObject authObj;
    authObj["role"] = "ADMIN";
    QJsonDocument doc(authObj);
    QByteArray payload = doc.toJson(QJsonDocument::Compact);

    AMPHeader header;
    header.version = 1;
    header.message_type = 1; // CMD_AUTH (Ensure this matches protocol.h)
    header.reserved = 0;
    header.payload_length = qToBigEndian((uint32_t)payload.size());

    socket->write((char*)&header, sizeof(header));
    socket->write(payload);
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

    // Ignore handshakes/heartbeats
    if (obj.contains("status") && obj["status"].toString() == "ok") return;

    QString ts = obj.value("timestamp").toString();
    QString src = obj.value("hostname").toString();
    if (src.isEmpty()) src = obj.value("source").toString();
    
    QString sev = obj.value("severity").toString();
    QString app = obj.value("application").toString();
    QString msg = obj.value("message").toString();

    addLogEntry(ts, src, sev, app, msg);
}

void MainWindow::addLogEntry(const QString &ts, const QString &src, const QString &sev, const QString &app, const QString &msg) {
    int row = 0; // Insert at top
    logTable->insertRow(row);

    logTable->setItem(row, 0, new QTableWidgetItem(ts));
    logTable->setItem(row, 1, new QTableWidgetItem(src));
    logTable->setItem(row, 2, new QTableWidgetItem(sev));
    logTable->setItem(row, 3, new QTableWidgetItem(app));
    logTable->setItem(row, 4, new QTableWidgetItem(msg));

    // Color Coding
    QColor color = Qt::white;
    if (sev.contains("ERR") || sev.contains("CRIT")) color = QColor("#ff4d4d"); // Red
    else if (sev.contains("WARN")) color = QColor("orange");

    for (int i=0; i<5; i++) {
        logTable->item(row, i)->setForeground(color);
    }
    
    // Limit rows to avoid memory issues
    if (logTable->rowCount() > 200) logTable->removeRow(200);
}

void MainWindow::onSocketError(QAbstractSocket::SocketError socketError) {
    statusLabel->setText("Status: Error - " + socket->errorString());
    statusLabel->setStyleSheet("font-weight: bold; color: red; padding: 5px;");
}
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

    connect(socket, &QTcpSocket::connected, this, &MainWindow::onConnected);
    connect(socket, &QTcpSocket::disconnected, this, &MainWindow::onDisconnected);
    connect(socket, &QTcpSocket::readyRead, this, &MainWindow::onReadyRead);
    connect(socket, &QTcpSocket::errorOccurred, this, &MainWindow::onSocketError);

    connect(reconnectTimer, &QTimer::timeout, this, &MainWindow::attemptConnection);

    attemptConnection();
}

MainWindow::~MainWindow() {}

void MainWindow::attemptConnection() {
    if (socket->state() == QAbstractSocket::UnconnectedState) {
        statusLabel->setText("Status: Searching for Server...");
        statusLabel->setStyleSheet("font-weight: bold; color: orange; padding: 5px;");
        socket->connectToHost("127.0.0.1", 9999);
    }
}

void MainWindow::onConnected() {
    reconnectTimer->stop();
    statusLabel->setText("Status: Connected");
    statusLabel->setStyleSheet("font-weight: bold; color: #00ff00; font-size: 14px; padding: 5px; background-color: #222;");

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

    QTimer *statsTimer = new QTimer(this);
    connect(statsTimer, &QTimer::timeout, this, &MainWindow::requestStats);
    statsTimer->start(5000);
}

void MainWindow::onDisconnected() {
    statusLabel->setText("Status: Disconnected. Retrying...");
    statusLabel->setStyleSheet("font-weight: bold; color: red; padding: 5px;");
    reconnectTimer->start(2000);
}

void MainWindow::onSocketError(QAbstractSocket::SocketError socketError) {
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
    resize(1100, 600); // Slightly wider for extra column

    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    QVBoxLayout *layout = new QVBoxLayout(centralWidget);

    // --- SEARCH BAR ---
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
    
    // Header Status
    QHBoxLayout *statusLayout = new QHBoxLayout();
    statusLabel = new QLabel("Status: Connecting...", this);
    statusLabel->setStyleSheet("font-weight: bold; color: orange; font-size: 14px; padding: 5px;");
    statusLayout->addWidget(statusLabel);

    // --- STATS PANEL ---
    statsBox = new QGroupBox("Live Statistics", this);
    statsBox->setStyleSheet("QGroupBox { border: 1px solid gray; border-radius: 5px; margin-top: 10px; color: white; font-weight: bold; } QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top center; padding: 0 3px; }");
    statsBox->setFixedHeight(80);

    QHBoxLayout *statsLayout = new QHBoxLayout();

    // Helper to create styled labels
    auto createStat = [](QString title, QString color) {
        QLabel *lbl = new QLabel("0", nullptr);
        lbl->setStyleSheet("font-size: 18px; font-weight: bold; color: " + color + "; border: 1px solid " + color + "; border-radius: 4px; padding: 5px;");
        lbl->setAlignment(Qt::AlignCenter);
        
        QVBoxLayout *vbox = new QVBoxLayout();
        QLabel *titleLbl = new QLabel(title);
        titleLbl->setStyleSheet("color: #aaa; font-size: 10px;");
        titleLbl->setAlignment(Qt::AlignCenter);
        
        vbox->addWidget(lbl);
        vbox->addWidget(titleLbl);
        
        QWidget *container = new QWidget();
        container->setLayout(vbox);
        return qMakePair(lbl, container);
    };

    auto infoPair = createStat("INFO", "#00ff00");
    lblInfoCount = infoPair.first;

    auto warnPair = createStat("WARNING", "orange");
    lblWarnCount = warnPair.first;

    auto errPair = createStat("CRITICAL/ERR", "#ff4d4d");
    lblErrCount = errPair.first;

    statsLayout->addWidget(infoPair.second);
    statsLayout->addWidget(warnPair.second);
    statsLayout->addWidget(errPair.second);

    statsBox->setLayout(statsLayout);
    
    // --- TABLE CONFIGURATION ---
    logTable = new QTableWidget(this);
    logTable->setColumnCount(7); 
    logTable->setHorizontalHeaderLabels({"Timestamp", "Source", "PID", "Facility", "Severity", "App", "Message"});
    
    // Styling
    logTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    logTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents); // Timestamp
    logTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents); // PID
    logTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents); // Facility (NEW)
    logTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents); // Severity
    
    // Dark Mode Style
    logTable->setStyleSheet("QTableWidget { background-color: #2d2d2d; color: white; gridline-color: #db1134; }"
                            "QHeaderView::section { background-color:rgb(0, 0, 128); color: white; padding: 4px; border: 1px solid #db1134; }");

    layout->addLayout(statusLayout);
    layout->addWidget(statsBox);
    layout->addLayout(searchLayout);
    layout->addWidget(logTable);
}

void MainWindow::onReadyRead() {
    buffer.append(socket->readAll());

    while (true) {
        if (static_cast<size_t>(buffer.size()) < sizeof(AMPHeader)) {
            return;
        }

        AMPHeader *header = reinterpret_cast<AMPHeader*>(buffer.data());
        uint32_t payloadLen = qFromBigEndian(header->payload_length);

        if (static_cast<size_t>(buffer.size()) < sizeof(AMPHeader) + payloadLen) {
            return;
        }

        buffer.remove(0, sizeof(AMPHeader));
        QByteArray payload = buffer.left(payloadLen);
        buffer.remove(0, payloadLen);

        processJson(payload);
    }
}

void MainWindow::processJson(const QByteArray &data) {
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull() || !doc.isObject()) return;
    QJsonObject obj = doc.object();

    if (obj.contains("stats")) {
        QJsonObject stats = obj["stats"].toObject();
        
        int info = stats["INFO"].toInt() + stats["NOTICE"].toInt() + stats["DEBUG"].toInt();
        int warn = stats["WARNING"].toInt();
        int err  = stats["ERROR"].toInt() + stats["CRITICAL"].toInt() + stats["ALERT"].toInt() + stats["EMERGENCY"].toInt();

        lblInfoCount->setText(QString::number(info));
        lblWarnCount->setText(QString::number(warn));
        lblErrCount->setText(QString::number(err));
        return;
    }

    // CASE 1: Search Results
    if (obj.contains("results")) {
        QJsonArray results = obj["results"].toArray();
        logTable->setRowCount(0);
        
        for (const auto &val : results) {
            QJsonObject log = val.toObject();
            addLogEntry(
                log["timestamp"].toString(),
                log["hostname"].toString(),
                log["pid"].toString(),
                log["facility"].toString(), // NEW
                log["severity"].toString(),
                log["application"].toString(),
                log["message"].toString()
            );
        }
        return;
    }

    // CASE 2: Live Log
    if (obj.contains("status") && obj["status"].toString() == "ok") return;

    QString ts = obj.value("timestamp").toString();
    QString src = obj.value("hostname").toString();
    if (src.isEmpty()) src = obj.value("source").toString();
    
    QString pid = obj.value("pid").toString();
    QString fac = obj.value("facility").toString(); // NEW: Extract Facility
    QString sev = obj.value("severity").toString();
    QString app = obj.value("application").toString();
    QString msg = obj.value("message").toString();

    // If facility is empty (legacy logs), default to "-"
    if (fac.isEmpty()) fac = "-";

    addLogEntry(ts, src, pid, fac, sev, app, msg);
}

void MainWindow::addLogEntry(const QString &ts, const QString &src, const QString &pid, 
                             const QString &fac, const QString &sev, const QString &app, const QString &msg) {
    int row = 0; 
    logTable->insertRow(row);

    logTable->setItem(row, 0, new QTableWidgetItem(ts));
    logTable->setItem(row, 1, new QTableWidgetItem(src));
    logTable->setItem(row, 2, new QTableWidgetItem(pid));
    logTable->setItem(row, 3, new QTableWidgetItem(fac)); 
    logTable->setItem(row, 4, new QTableWidgetItem(sev));
    logTable->setItem(row, 5, new QTableWidgetItem(app));
    logTable->setItem(row, 6, new QTableWidgetItem(msg));

    // Color Coding
    QColor color = Qt::white;
    if (sev.contains("ERR") || sev.contains("CRIT") || sev.contains("FATAL") || sev.contains("EMERG") || sev.contains("ALERT")) 
        color = QColor("#ff4d4d"); 
    else if (sev.contains("WARNING")) 
        color = QColor("orange");

    // Apply color to all 7 columns
    for (int i=0; i<7; i++) {
        logTable->item(row, i)->setForeground(color);
    }
    
    if (logTable->rowCount() > 200) logTable->removeRow(200);
}

void MainWindow::sendSearchRequest() {
    if (socket->state() != QAbstractSocket::ConnectedState) return;

    QJsonObject searchObj;
    searchObj["keyword"] = searchBar->text();
    searchObj["severity"] = severityFilter->currentText();
    searchObj["limit"] = "50";

    QJsonDocument doc(searchObj);
    QByteArray payload = doc.toJson(QJsonDocument::Compact);

    AMPHeader header;
    header.version = 1;
    header.message_type = CMD_SEARCH; 
    header.reserved = 0;
    header.payload_length = qToBigEndian((uint32_t)payload.size());

    socket->write((char*)&header, sizeof(header));
    socket->write(payload);
    
    logTable->setRowCount(0);
}

void MainWindow::requestStats() {
    if (socket->state() != QAbstractSocket::ConnectedState) return;

    AMPHeader header;
    header.version = 1;
    header.message_type = CMD_STATS; // 5
    header.reserved = 0;
    header.payload_length = 0; // No payload needed for request

    socket->write((char*)&header, sizeof(header));
}

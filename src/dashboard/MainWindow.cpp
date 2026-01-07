#include "MainWindow.h"
#include <QCoreApplication>
#include <QHeaderView>
#include <QJsonDocument>
#include <QJsonObject>
#include <QHBoxLayout>
#include <QtEndian> 
#include <QJsonArray>
#include <QDebug>
#include <QMetaObject>
#include <QTabWidget>
#include <QMessageBox>
#include <QInputDialog>
#include <QCheckBox> 

using namespace std;

MainWindow::MainWindow(QString user, QString pass, QWidget *parent) 
    : QMainWindow(parent), m_username(user), m_password(pass), socket(nullptr),          // <--- INIT NULL
      reconnectTimer(nullptr),  
      statsTimer(nullptr),      
      agentsTimer(nullptr),   
      btnAddAgent(nullptr),
      btnActivate(nullptr),
      btnBlock(nullptr)
{
    setupUI();

    socket = new QTcpSocket(this);
    
    reconnectTimer = new QTimer(this);
    connect(reconnectTimer, &QTimer::timeout, this, &MainWindow::attemptConnection);
    reconnectTimer->start(2000); // Verifică la fiecare 2 secunde
    // --------------------------------------

    connect(socket, &QTcpSocket::connected, this, &MainWindow::onConnected);
    connect(socket, &QTcpSocket::disconnected, this, &MainWindow::onDisconnected);
    connect(socket, &QTcpSocket::readyRead, this, &MainWindow::onReadyRead);
    // Nu mai avem nevoie de onSocketError pentru a reporni timerul, el rulează mereu

    // Încercare imediată
    attemptConnection();
}

MainWindow::~MainWindow() {}

void MainWindow::setupUI() {
    // 1. Aplica Tema Moderna
    applyModernStyle();

    setWindowTitle("Network Monitor v3.0 - Sentinel");
    resize(1280, 850);

    QTabWidget *tabWidget = new QTabWidget(this);
    setCentralWidget(tabWidget);

    // ============================================
    // TAB 1: DASHBOARD
    // ============================================
    QWidget *dashTab = new QWidget();
    QVBoxLayout *dashLayout = new QVBoxLayout(dashTab);
    dashLayout->setContentsMargins(20, 20, 20, 20); // Spatiere mai aerisita
    dashLayout->setSpacing(15);

    // --- Stats Box ---
    statsBox = new QGroupBox("LIVE TELEMETRY", dashTab);
    statsBox->setFixedHeight(140);
    
    QHBoxLayout *statsLayout = new QHBoxLayout();
    
    // Helper pentru carduri de statistici
    auto createStat = [](QString title, QString colorCode) {
        QLabel *lbl = new QLabel("0", nullptr);
        lbl->setAlignment(Qt::AlignCenter);
        // Stil specific pentru numere mari
        lbl->setStyleSheet("font-size: 28px; font-weight: bold; color: " + colorCode + ";");
        
        QLabel *titleLbl = new QLabel(title);
        titleLbl->setAlignment(Qt::AlignCenter);
        titleLbl->setStyleSheet("color: #888; font-size: 12px; text-transform: uppercase; letter-spacing: 1px;");
        
        QVBoxLayout *vbox = new QVBoxLayout();
        vbox->addStretch();
        vbox->addWidget(lbl);
        vbox->addWidget(titleLbl);
        vbox->addStretch();
        
        QFrame *card = new QFrame();
        card->setLayout(vbox);
        card->setStyleSheet("background-color: #252526; border-radius: 8px; border: 1px solid #333;");
        return qMakePair(lbl, card);
    };

    auto infoPair = createStat("NORMAL ACTIVITY", "#4CAF50"); // Green
    lblInfoCount = infoPair.first;
    
    auto warnPair = createStat("WARNINGS", "#FFC107"); // Amber
    lblWarnCount = warnPair.first;
    
    auto errPair = createStat("CRITICAL ERRORS", "#FF5252"); // Red
    lblErrCount = errPair.first;

    statsLayout->addWidget(infoPair.second);
    statsLayout->addWidget(warnPair.second);
    statsLayout->addWidget(errPair.second);

    // Tabelul mic din dreapta (Top Sources)
    topSourcesTable = new QTableWidget(dashTab);
    topSourcesTable->setColumnCount(2);
    topSourcesTable->setHorizontalHeaderLabels({"TOP SOURCE", "HITS"});
    topSourcesTable->verticalHeader()->setVisible(false);
    topSourcesTable->setFixedWidth(350);
    topSourcesTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    topSourcesTable->setStyleSheet("background-color: #252526; border-radius: 8px; border: 1px solid #333;"); // Card style
    
    statsLayout->addWidget(topSourcesTable);
    statsBox->setLayout(statsLayout);
    dashLayout->addWidget(statsBox);

    // --- Search Bar Area ---
    QHBoxLayout *searchLayout = new QHBoxLayout();
    
    searchBar = new QLineEdit(dashTab);
    searchBar->setPlaceholderText("🔍 Filter logs by keyword...");
    searchBar->setFixedHeight(35);
    
    severityFilter = new QComboBox(dashTab);
    severityFilter->addItems({"ALL SEVERITIES", "EMERGENCY", "ALERT", "CRITICAL", "ERROR", "WARNING", "NOTICE", "INFO", "DEBUG"});
    severityFilter->setFixedHeight(35);
    severityFilter->setFixedWidth(150);
    
    searchButton = new QPushButton("SEARCH LOGS", dashTab);
    searchButton->setFixedHeight(35);
    searchButton->setCursor(Qt::PointingHandCursor);
    connect(searchButton, &QPushButton::clicked, this, &MainWindow::sendSearchRequest);

    statusLabel = new QLabel("⚫ Connecting...", dashTab);
    statusLabel->setStyleSheet("font-weight: bold; color: #888;");

    searchLayout->addWidget(statusLabel);
    searchLayout->addSpacing(20);
    searchLayout->addWidget(searchBar);
    searchLayout->addWidget(severityFilter);
    searchLayout->addWidget(searchButton);
    dashLayout->addLayout(searchLayout);

    // --- Log Table ---
    logTable = new QTableWidget(dashTab);
    logTable->setColumnCount(7);
    logTable->setHorizontalHeaderLabels({"TIME", "SOURCE", "PID", "FACILITY", "SEVERITY", "APP", "MESSAGE"});
    logTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive); // User can resize
    logTable->horizontalHeader()->setSectionResizeMode(6, QHeaderView::Stretch); // Message stretches
    logTable->verticalHeader()->setVisible(false);
    logTable->setShowGrid(false); // Mai curat fara gridlines
    logTable->setAlternatingRowColors(true); // Randuri alternative
    
    // Style specific pentru row colors
    logTable->setStyleSheet(logTable->styleSheet() + "QTableWidget { alternate-background-color: #262626; }");

    dashLayout->addWidget(logTable);

    tabWidget->addTab(dashTab, QIcon(), "MONITOR DASHBOARD");

    // ============================================
    // TAB 2: AGENT MANAGEMENT
    // ============================================
    QWidget *agentTab = new QWidget();
    QVBoxLayout *agentLayout = new QVBoxLayout(agentTab);
    agentLayout->setContentsMargins(40, 40, 40, 40);
    agentLayout->setSpacing(20);

    QLabel *agentTitle = new QLabel("ENDPOINT SECURITY MANAGEMENT", agentTab);
    agentTitle->setStyleSheet("font-size: 22px; font-weight: bold; color: white; margin-bottom: 10px; border-bottom: 2px solid #007acc; padding-bottom: 10px;");
    agentLayout->addWidget(agentTitle);

    agentTable = new QTableWidget(agentTab);
    agentTable->setColumnCount(3);
    agentTable->setHorizontalHeaderLabels({"ENDPOINT IP / ID", "SECURITY STATUS", "LAST HEARTBEAT"});
    agentTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    agentTable->verticalHeader()->setVisible(false);
    agentTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    agentTable->setSelectionMode(QAbstractItemView::SingleSelection);
    agentTable->setShowGrid(false);
    agentTable->setAlternatingRowColors(true);
    agentTable->setStyleSheet("alternate-background-color: #262626; font-size: 15px;");
    
    agentLayout->addWidget(agentTable);
    
    // Buttons Area
    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(15);
    
    btnAddAgent = new QPushButton("✚ DEPLOY SIMULATOR", agentTab);
    btnAddAgent->setCursor(Qt::PointingHandCursor);
    btnAddAgent->setStyleSheet("background-color: #007acc; font-size: 14px; padding: 12px;"); // Blue
    connect(btnAddAgent, &QPushButton::clicked, this, &MainWindow::onAddAgent);

    btnActivate = new QPushButton("✔ AUTHORIZE AGENT", agentTab);
    btnActivate->setCursor(Qt::PointingHandCursor);
    btnActivate->setStyleSheet("background-color: #2ea043; font-size: 14px; padding: 12px;"); // GitHub Green
    connect(btnActivate, &QPushButton::clicked, this, &MainWindow::onActivateAgent);

    btnBlock = new QPushButton("✖ REVOKE ACCESS", agentTab);
    btnBlock->setCursor(Qt::PointingHandCursor);
    btnBlock->setStyleSheet("background-color: #da3633; font-size: 14px; padding: 12px;"); // Red
    connect(btnBlock, &QPushButton::clicked, this, &MainWindow::onBlockAgent);

    btnLayout->addWidget(btnAddAgent);
    btnLayout->addWidget(btnActivate);
    btnLayout->addWidget(btnBlock);
    agentLayout->addLayout(btnLayout);

    tabWidget->addTab(agentTab, "CONFIGURATION");
}

void MainWindow::attemptConnection() {
    // Dacă suntem deja conectați, nu facem nimic
    if (socket->state() == QAbstractSocket::ConnectedState) {
        return;
    }
    
    // Dacă suntem în curs de conectare, așteptăm
    if (socket->state() == QAbstractSocket::HostLookupState || 
        socket->state() == QAbstractSocket::ConnectingState) {
        statusLabel->setText("Status: Negotiating...");
        return;
    }

    // Altfel, încercăm conectarea
    statusLabel->setText("Status: Connecting to Server...");
    socket->connectToHost("127.0.0.1", 9999);
}

void MainWindow::onConnected() {
    reconnectTimer->stop();
    statusLabel->setText("Status: Authenticating...");

    QJsonObject authObj;
    authObj["username"] = m_username;
    authObj["password"] = m_password;
    QJsonDocument doc(authObj);
    QByteArray payload = doc.toJson(QJsonDocument::Compact);
    AMPHeader header = {1, CMD_AUTH, 0, qToBigEndian((uint32_t)payload.size()), 0};
    
    socket->write((char*)&header, sizeof(header));
    socket->write(payload);
    
    // NU mai apelam requestAgents() aici!
}

void MainWindow::onDisconnected() {
    statusLabel->setText("Status: Disconnected. Retrying...");
    statusLabel->setStyleSheet("color: red; font-weight: bold;");
    // Timerul reconnectTimer va prinde faptul ca socket-ul e Unconnected si va incerca din nou
}

void MainWindow::onSocketError(QAbstractSocket::SocketError) {
    // Doar afisam eroarea, nu oprim logica
    qDebug() << "Socket Error:" << socket->errorString();
}

void MainWindow::onReadyRead() {
    buffer.append(socket->readAll());
    
    // Buclă de procesare pachete
    while (buffer.size() >= (int)sizeof(AMPHeader)) {
        AMPHeader *header = reinterpret_cast<AMPHeader*>(buffer.data());
        uint32_t payloadLen = qFromBigEndian(header->payload_length);
        
        if (buffer.size() < (int)(sizeof(AMPHeader) + payloadLen)) return; // Așteptăm restul datelor

        buffer.remove(0, sizeof(AMPHeader)); // Scoatem header
        QByteArray payload = buffer.left(payloadLen); // Luăm datele
        buffer.remove(0, payloadLen); // Scoatem datele din buffer
        
        qDebug() << "[CLIENT] RX Payload:" << payload; // <--- ADAUGA
        processJson(payload);
    }
}

void MainWindow::requestAgents() {
    if (socket->state() == QAbstractSocket::ConnectedState) {
        AMPHeader h = {1, CMD_GET_AGENTS, 0, 0, 0};
        socket->write((char*)&h, sizeof(h));
    }
}

void MainWindow::onActivateAgent() {
    int row = agentTable->currentRow();
    if (row < 0) return;
    QString ip = agentTable->item(row, 0)->text(); // IP din Col 0

    QJsonObject obj;
    obj["ip"] = ip;
    obj["status"] = "ACTIVE"; 
    
    QJsonDocument doc(obj);
    QByteArray p = doc.toJson(QJsonDocument::Compact);
    AMPHeader h = {1, CMD_UPDATE_AGENT, 0, qToBigEndian((uint32_t)p.size()), 0};
    socket->write((char*)&h, sizeof(h));
    socket->write(p);
    
    // Fortam refresh rapid
    QTimer::singleShot(200, this, &MainWindow::requestAgents);
}

void MainWindow::onBlockAgent() {
    int row = agentTable->currentRow();
    if (row < 0) return;
    QString ip = agentTable->item(row, 0)->text();

    QJsonObject obj;
    obj["ip"] = ip;
    obj["status"] = "BLOCKED";
    
    QJsonDocument doc(obj);
    QByteArray p = doc.toJson(QJsonDocument::Compact);
    AMPHeader h = {1, CMD_UPDATE_AGENT, 0, qToBigEndian((uint32_t)p.size()), 0};
    socket->write((char*)&h, sizeof(h));
    socket->write(p);
    
    QTimer::singleShot(200, this, &MainWindow::requestAgents);
}

void MainWindow::onAddAgent() {
    QDialog dlg(this);
    dlg.setWindowTitle("Deploy New Agent");
    dlg.setFixedSize(300, 150);
    dlg.setStyleSheet("background: #333; color: white;");
    
    QVBoxLayout *lay = new QVBoxLayout(&dlg);
    QLineEdit *ipEdit = new QLineEdit(&dlg);
    ipEdit->setPlaceholderText("Agent Identifier (IP)");
    ipEdit->setStyleSheet("padding: 5px; color: black; background: white;");
    
    QCheckBox *chkVirtual = new QCheckBox("Virtual Simulator", &dlg);
    chkVirtual->setStyleSheet("color: white;");
    
    QPushButton *btnOk = new QPushButton("Deploy", &dlg);
    btnOk->setStyleSheet("background: #007acc; padding: 5px; font-weight: bold;");
    connect(btnOk, &QPushButton::clicked, &dlg, &QDialog::accept);
    
    lay->addWidget(new QLabel("Agent IP / Name:"));
    lay->addWidget(ipEdit);
    lay->addWidget(chkVirtual);
    lay->addWidget(btnOk);
    
    if (dlg.exec() == QDialog::Accepted) {
        QString ip = ipEdit->text();
        if (ip.isEmpty()) return;
        
        QJsonObject obj;
        obj["ip"] = ip;
        obj["name"] = ip; 
        obj["type"] = chkVirtual->isChecked() ? "VIRTUAL" : "REAL";
        
        QJsonDocument doc(obj);
        QByteArray p = doc.toJson(QJsonDocument::Compact);
        AMPHeader h = {1, CMD_ADD_AGENT, 0, qToBigEndian((uint32_t)p.size()), 0};
        socket->write((char*)&h, sizeof(h));
        socket->write(p);
        
        QTimer::singleShot(500, this, &MainWindow::requestAgents);
    }
}

void MainWindow::processJson(const QByteArray &data) {
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull() || !doc.isObject()) return;
    QJsonObject obj = doc.object();

    if (obj.contains("role")) {
        QString role = obj["role"].toString();
        QMetaObject::invokeMethod(this, [=]() {
            setWindowTitle("Network Monitor - " + role + " Mode");
            statusLabel->setText("Status: Connected (" + role + ")");
            
            if (role == "ADMIN") {
                statusLabel->setText("🟢 ONLINE (ADMIN ACCESS)");
                statusLabel->setStyleSheet("color: #4CAF50; font-weight: bold;");

                btnAddAgent->setEnabled(true);
                btnActivate->setEnabled(true);
                btnBlock->setEnabled(true);
                
                // AICI cerem datele
                requestAgents();
                
                if (statsTimer) { 
                    statsTimer->stop(); 
                    delete statsTimer; 
                    statsTimer = nullptr; 
                }
                statsTimer = new QTimer(this);
                connect(statsTimer, &QTimer::timeout, this, &MainWindow::requestStats);
                statsTimer->start(3000);

                if (agentsTimer) { 
                    agentsTimer->stop(); 
                    delete agentsTimer; 
                    agentsTimer = nullptr; 
                }
                agentsTimer = new QTimer(this);
                connect(agentsTimer, &QTimer::timeout, this, &MainWindow::requestAgents);
                agentsTimer->start(2000);
            } else {
                statusLabel->setText("🟡 READ-ONLY MODE");
                // Viewer mode
                btnAddAgent->setEnabled(false);
                btnActivate->setEnabled(false);
                btnBlock->setEnabled(false);
                requestStats(); // Viewer vede doar stats si logs
            }
        });
        return;
    }

    // --- AGENTS LIST ---
    if (obj.contains("agents")) {
        QJsonArray agents = obj["agents"].toArray();
        
        // DEBUG: Afiseaza un popup daca lista nu e goala (sa vedem daca ajunge ceva)
        if (agents.size() > 0) {
            // Poti decomenta linia de mai jos daca vrei confirmare vizuala ca datele au ajuns
            // QMessageBox::information(this, "Debug Data", "Received " + QString::number(agents.size()) + " agents from server.");
        }

        QMetaObject::invokeMethod(this, [=]() {
            // 1. Asigură-te că avem 3 coloane
            if (agentTable->columnCount() != 3) {
                agentTable->setColumnCount(3);
                agentTable->setHorizontalHeaderLabels({"Source IP", "Status", "Last Seen"});
            }

            int currentRow = agentTable->currentRow();
            agentTable->setRowCount(0);
            
            for(const auto &val : agents) {
                QJsonObject a = val.toObject();
                int r = agentTable->rowCount();
                agentTable->insertRow(r);
                
                // Col 0: IP (Fortam text ALB)
                QTableWidgetItem *ipItem = new QTableWidgetItem(a["ip"].toString());
                ipItem->setForeground(Qt::white); // <--- FIX VIZUAL
                agentTable->setItem(r, 0, ipItem);
                
                // Col 1: Status
                QTableWidgetItem *statusItem = new QTableWidgetItem(a["status"].toString());
                statusItem->setForeground(Qt::white); // <--- FIX VIZUAL
                if (a["status"].toString() == "ACTIVE") statusItem->setBackground(QColor("#28a745"));
                else if (a["status"].toString() == "PENDING") statusItem->setBackground(QColor("orange"));
                else statusItem->setBackground(QColor("#dc3545"));
                agentTable->setItem(r, 1, statusItem);

                // Col 2: Last Seen (Fortam text ALB)
                QTableWidgetItem *seenItem = new QTableWidgetItem(a["last_seen"].toString());
                seenItem->setForeground(Qt::white); // <--- FIX VIZUAL
                agentTable->setItem(r, 2, seenItem);
            }
            if(currentRow >= 0 && currentRow < agentTable->rowCount()) agentTable->selectRow(currentRow);
        });
        return;
    }

    if (obj.contains("results")) {
        QJsonArray results = obj["results"].toArray();
        QMetaObject::invokeMethod(this, [=]() {
            logTable->setRowCount(0);
            for (const auto &val : results) {
                QJsonObject log = val.toObject();
                addLogEntry(log["timestamp"].toString(), log["hostname"].toString(), log["pid"].toString(), 
                            log["facility"].toString(), log["severity"].toString(), log["application"].toString(), log["message"].toString());
            }
        });
        return;
    }

    if (obj.contains("stats")) {
        QJsonObject stats = obj["stats"].toObject();
        int info = 0, warn = 0, err = 0;
        for(const QString &key : stats.keys()) {
            if (key == "top_sources") continue;
            int val = stats[key].toInt();
            if (key.contains("ERR") || key.contains("CRIT") || key.contains("ALE")) err += val;
            else if (key.contains("WARN")) warn += val;
            else info += val;
        }
        int total = info + warn + err;
        auto fmt = [&](int v) { double p = total ? (v * 100.0 / total) : 0; return QString("%1 - %2%").arg(v).arg(QString::number(p, 'f', 1)); };
        
        QMetaObject::invokeMethod(this, [=]() {
            lblInfoCount->setText(fmt(info));
            lblWarnCount->setText(fmt(warn));
            lblErrCount->setText(fmt(err));
            QJsonArray tops = stats["top_sources"].toArray();
            topSourcesTable->setRowCount(0);
            for(const auto &t : tops) {
                QJsonObject o = t.toObject();
                int r = topSourcesTable->rowCount();
                topSourcesTable->insertRow(r);
                topSourcesTable->setItem(r, 0, new QTableWidgetItem(o["name"].toString()));
                topSourcesTable->setItem(r, 1, new QTableWidgetItem(QString::number(o["count"].toInt())));
            }
        });
        return;
    }

    // Live Logs
    if (obj.contains("message") && obj.contains("hostname")) {
        QMetaObject::invokeMethod(this, [=]() {
             addLogEntry(obj["timestamp"].toString(), obj["hostname"].toString(), obj["pid"].toString(), 
                         obj["facility"].toString(), obj["severity"].toString(), obj["application"].toString(), obj["message"].toString());
        });
    }
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

    QColor color = Qt::white;
    if (sev.contains("ERR") || sev.contains("CRIT")) color = QColor("#ff4d4d");
    else if (sev.contains("WARN")) color = QColor("orange");

    for (int i=0; i<7; i++) logTable->item(row, i)->setForeground(color);
    if (logTable->rowCount() > 100) logTable->removeRow(100);
}

void MainWindow::requestStats() {
    if (socket->state() == QAbstractSocket::ConnectedState) {
        AMPHeader h = {1, CMD_STATS, 0, 0, 0};
        socket->write((char*)&h, sizeof(h));
    }
}

void MainWindow::sendSearchRequest() {
    if (socket->state() == QAbstractSocket::ConnectedState) {
        QJsonObject s;
        s["keyword"] = searchBar->text();
        s["severity"] = severityFilter->currentText();
        s["limit"] = "50";
        QJsonDocument d(s);
        QByteArray p = d.toJson(QJsonDocument::Compact);
        AMPHeader h = {1, CMD_SEARCH, 0, qToBigEndian((uint32_t)p.size()), 0};
        socket->write((char*)&h, sizeof(h));
        socket->write(p);
    }
}

void MainWindow::applyModernStyle() {
    QString style = R"(
        /* --- GENERAL --- */
        QMainWindow {
            background-color: #1e1e1e; /* Dark Grey (VS Code style) */
        }
        QWidget {
            font-family: 'Segoe UI', 'Roboto', sans-serif;
            font-size: 14px;
            color: #d4d4d4; /* Off-white text */
        }
        
        /* --- TABS --- */
        QTabWidget::pane {
            border: 1px solid #333333;
            background: #252526;
            top: -1px; 
        }
        QTabBar::tab {
            background: #2d2d2d;
            border: 1px solid #333;
            padding: 8px 20px;
            margin-right: 2px;
            color: #888;
            border-top-left-radius: 4px;
            border-top-right-radius: 4px;
        }
        QTabBar::tab:selected {
            background: #1e1e1e;
            color: #ffffff;
            border-bottom: 2px solid #007acc; /* Blue accent */
        }
        QTabBar::tab:hover {
            background: #3e3e42;
            color: white;
        }

        /* --- BUTTONS --- */
        QPushButton {
            background-color: #0e639c;
            color: white;
            border: none;
            padding: 8px 16px;
            border-radius: 4px;
            font-weight: bold;
        }
        QPushButton:hover {
            background-color: #1177bb;
        }
        QPushButton:pressed {
            background-color: #094771;
        }
        QPushButton:disabled {
            background-color: #333;
            color: #555;
        }

        /* --- INPUTS & COMBOBOX --- */
        QLineEdit, QComboBox {
            background-color: #3c3c3c;
            border: 1px solid #555;
            border-radius: 3px;
            padding: 5px;
            color: white;
            selection-background-color: #007acc;
        }
        QLineEdit:focus, QComboBox:focus {
            border: 1px solid #007acc;
        }
        
        /* --- TABLES --- */
        QTableWidget {
            background-color: #1e1e1e;
            gridline-color: #333;
            border: none;
            selection-background-color: #264f78;
            selection-color: white;
        }
        QTableWidget::item {
            padding: 5px;
            border-bottom: 1px solid #2d2d2d;
        }
        QHeaderView::section {
            background-color: #252526;
            color: #cccccc;
            padding: 6px;
            border: none;
            border-bottom: 2px solid #333;
            font-weight: bold;
        }
        QTableCornerButton::section {
            background-color: #252526;
            border: none;
        }

        /* --- SCROLLBARS (Modern Slim) --- */
        QScrollBar:vertical {
            border: none;
            background: #1e1e1e;
            width: 10px;
            margin: 0px 0px 0px 0px;
        }
        QScrollBar::handle:vertical {
            background: #424242;
            min-height: 20px;
            border-radius: 5px;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0px;
        }

        /* --- GROUP BOX --- */
        QGroupBox {
            border: 1px solid #444;
            border-radius: 6px;
            margin-top: 20px;
            font-weight: bold;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 10px;
            padding: 0 5px;
            color: #007acc; 
        }
    )";
    this->setStyleSheet(style);
}
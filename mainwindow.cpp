#include "mainwindow.h"
#include <QApplication>
#include <QNetworkInterface>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QMessageBox>
#include <QFont>
#include <QUrl>
#include <QNetworkRequest>
#include <QClipboard>
#include <QCoreApplication>
#include <QFileDialog>
#include <QDateTime>
#include <QDesktopServices>
#include <QSettings>
#include <QPainter>
#include <QLinearGradient>
#include <QElapsedTimer>
#include <QInputDialog>
#include <QHeaderView>
#include <QRegularExpression>
#include <functional>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , networkManager(new QNetworkAccessManager(this))
    , trayIcon(nullptr)
    , darkTheme(false)
    , autoRefreshEnabled(false)
{
    loadSettings();
    setupUi();
    if (darkTheme) {
        applyDarkTheme();
    } else {
        applyLightTheme();
    }

    refreshIps();
    updateSystemTray();

    if (!trayIcon->isVisible()) {
        trayIcon->show();
    }
}

MainWindow::~MainWindow()
{
    saveSettings();
}

void MainWindow::loadSettings()
{
    QSettings settings("IPInspector", "Main");
    darkTheme = settings.value("darkTheme", false).toBool();
    autoRefreshEnabled = settings.value("autoRefresh", false).toBool();
}

void MainWindow::saveSettings()
{
    QSettings settings("IPInspector", "Main");
    settings.setValue("darkTheme", darkTheme);
    settings.setValue("autoRefresh", autoRefreshEnabled);
}

void MainWindow::setupUi()
{
    QWidget *centralWidget = new QWidget(this);
    centralWidget->setObjectName("centralWidget");
    setCentralWidget(centralWidget);

    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(15, 15, 15, 15);
    mainLayout->setSpacing(10);

    // Title bar
    QHBoxLayout *titleBar = new QHBoxLayout();
    QLabel *titleLabel = new QLabel("IP Inspector Pro", this);
    titleLabel->setObjectName("titleLabel");
    titleLabel->setFont(QFont("Segoe UI", 18, QFont::Bold));
    titleBar->addWidget(titleLabel);
    titleBar->addStretch();

    themeButton = new QPushButton(darkTheme ? "\u263d" : "\u2600", this);
    themeButton->setObjectName("toolButton");
    themeButton->setFixedSize(35, 35);
    themeButton->setToolTip("Toggle Theme");
    themeButton->setFont(QFont("Arial", 14));
    titleBar->addWidget(themeButton);
    connect(themeButton, &QPushButton::clicked, this, &MainWindow::onThemeToggle);

    autoRefreshButton = new QPushButton("Auto: OFF", this);
    autoRefreshButton->setObjectName("toolButton");
    autoRefreshButton->setFixedSize(90, 35);
    autoRefreshButton->setFont(QFont("Arial", 10));
    titleBar->addWidget(autoRefreshButton);
    connect(autoRefreshButton, &QPushButton::clicked, this, &MainWindow::toggleAutoRefresh);

    mainLayout->addLayout(titleBar);

    // Status indicator
    QHBoxLayout *statusLayout = new QHBoxLayout();
    statusIndicator = new QLabel("\u25cf Disconnected", this);
    statusIndicator->setObjectName("statusOffline");
    statusIndicator->setFont(QFont("Arial", 10, QFont::Bold));
    statusLayout->addWidget(statusIndicator);
    statusLayout->addStretch();
    mainLayout->addLayout(statusLayout);

    // Main info cards
    QGridLayout *infoGrid = new QGridLayout();
    infoGrid->setSpacing(10);

    // Local IP card
    QWidget *localCard = new QWidget(this);
    localCard->setObjectName("card");
    QHBoxLayout *localLayout = new QHBoxLayout(localCard);
    localLayout->setContentsMargins(12, 12, 12, 12);

    QLabel *localIcon = new QLabel("\u25cf", this);
    localIcon->setObjectName("iconLocal");
    localIcon->setFont(QFont("Arial", 20));
    localLayout->addWidget(localIcon);

    QVBoxLayout *localInfo = new QVBoxLayout();
    QLabel *localLabel = new QLabel("Local IP", this);
    localLabel->setObjectName("subtitle");
    localLabel->setFont(QFont("Arial", 10));
    localInfo->addWidget(localLabel);

    localValueLabel = new QLabel("Loading...", this);
    localValueLabel->setObjectName("valueText");
    localValueLabel->setFont(QFont("Courier New", 14, QFont::Bold));
    localInfo->addWidget(localValueLabel);

    localLayout->addLayout(localInfo);
    localLayout->addStretch();

    QPushButton *copyLocalBtn = new QPushButton("Copy", this);
    copyLocalBtn->setObjectName("copyBtn");
    copyLocalBtn->setFixedSize(55, 28);
    copyLocalBtn->setFont(QFont("Arial", 9));
    copyLocalBtn->setToolTip("Copy to clipboard");
    localLayout->addWidget(copyLocalBtn);
    connect(copyLocalBtn, &QPushButton::clicked, this, &MainWindow::copyToLocalClipboard);

    infoGrid->addWidget(localCard, 0, 0);

    // Public IP card
    QWidget *publicCard = new QWidget(this);
    publicCard->setObjectName("card");
    QHBoxLayout *publicLayout = new QHBoxLayout(publicCard);
    publicLayout->setContentsMargins(12, 12, 12, 12);

    QLabel *publicIcon = new QLabel("\u25cf", this);
    publicIcon->setObjectName("iconPublic");
    publicIcon->setFont(QFont("Arial", 20));
    publicLayout->addWidget(publicIcon);

    QVBoxLayout *publicInfo = new QVBoxLayout();
    QLabel *publicLabel = new QLabel("Public IP", this);
    publicLabel->setObjectName("subtitle");
    publicLabel->setFont(QFont("Arial", 10));
    publicInfo->addWidget(publicLabel);

    publicValueLabel = new QLabel("Loading...", this);
    publicValueLabel->setObjectName("valueText");
    publicValueLabel->setFont(QFont("Courier New", 14, QFont::Bold));
    publicInfo->addWidget(publicValueLabel);

    publicLayout->addLayout(publicInfo);
    publicLayout->addStretch();

    QPushButton *copyPublicBtn = new QPushButton("Copy", this);
    copyPublicBtn->setObjectName("copyBtn");
    copyPublicBtn->setFixedSize(55, 28);
    copyPublicBtn->setFont(QFont("Arial", 9));
    copyPublicBtn->setToolTip("Copy to clipboard");
    publicLayout->addWidget(copyPublicBtn);
    connect(copyPublicBtn, &QPushButton::clicked, this, &MainWindow::copyPublicToClipboard);

    infoGrid->addWidget(publicCard, 0, 1);

    // Location card
    QWidget *geoCard = new QWidget(this);
    geoCard->setObjectName("card");
    QHBoxLayout *geoLayout = new QHBoxLayout(geoCard);
    geoLayout->setContentsMargins(12, 12, 12, 12);

    QLabel *geoIcon = new QLabel("\u25cf", this);
    geoIcon->setObjectName("iconGeo");
    geoIcon->setFont(QFont("Arial", 20));
    geoLayout->addWidget(geoIcon);

    QVBoxLayout *geoInfo = new QVBoxLayout();
    QLabel *geoLabel = new QLabel("Location", this);
    geoLabel->setObjectName("subtitle");
    geoLabel->setFont(QFont("Arial", 10));
    geoInfo->addWidget(geoLabel);

    geoLocationLabel = new QLabel("Loading...", this);
    geoLocationLabel->setObjectName("valueText");
    geoLocationLabel->setFont(QFont("Arial", 11));
    geoInfo->addWidget(geoLocationLabel);

    geoLayout->addLayout(geoInfo);
    geoLayout->addStretch();

    infoGrid->addWidget(geoCard, 1, 0);

    // System info card
    QWidget *systemCard = new QWidget(this);
    systemCard->setObjectName("card");
    QHBoxLayout *systemLayout = new QHBoxLayout(systemCard);
    systemLayout->setContentsMargins(12, 12, 12, 12);

    QLabel *sysIcon = new QLabel("\u25cf", this);
    sysIcon->setObjectName("iconSystem");
    sysIcon->setFont(QFont("Arial", 20));
    systemLayout->addWidget(sysIcon);

    QVBoxLayout *systemInfo = new QVBoxLayout();
    QLabel *sysLabel = new QLabel("System Info", this);
    sysLabel->setObjectName("subtitle");
    sysLabel->setFont(QFont("Arial", 10));
    systemInfo->addWidget(sysLabel);

    hostnameLabel = new QLabel("Hostname: Loading...", this);
    hostnameLabel->setObjectName("valueText");
    hostnameLabel->setFont(QFont("Arial", 10));
    systemInfo->addWidget(hostnameLabel);

    macLabel = new QLabel("MAC: Loading...", this);
    macLabel->setObjectName("valueText");
    macLabel->setFont(QFont("Arial", 10));
    systemInfo->addWidget(macLabel);

    systemLayout->addLayout(systemInfo);
    systemLayout->addStretch();

    infoGrid->addWidget(systemCard, 1, 1);

    mainLayout->addLayout(infoGrid);

    // Buttons row
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();

    QPushButton *refreshBtn = new QPushButton("\u21bb Refresh All", this);
    refreshBtn->setObjectName("btnPrimary");
    refreshBtn->setFixedSize(130, 35);
    refreshBtn->setFont(QFont("Arial", 11, QFont::Bold));
    buttonLayout->addWidget(refreshBtn);
    connect(refreshBtn, &QPushButton::clicked, this, &MainWindow::refreshIps);

    QPushButton *exportBtn = new QPushButton("Export", this);
    exportBtn->setObjectName("btnSuccess");
    exportBtn->setFixedSize(100, 35);
    exportBtn->setFont(QFont("Arial", 11, QFont::Bold));
    buttonLayout->addWidget(exportBtn);
    connect(exportBtn, &QPushButton::clicked, this, &MainWindow::exportData);

    QPushButton *clearHistoryBtn = new QPushButton("Clear History", this);
    clearHistoryBtn->setObjectName("btnDanger");
    clearHistoryBtn->setFixedSize(130, 35);
    clearHistoryBtn->setFont(QFont("Arial", 11, QFont::Bold));
    buttonLayout->addWidget(clearHistoryBtn);
    connect(clearHistoryBtn, &QPushButton::clicked, this, &MainWindow::clearHistory);

    mainLayout->addLayout(buttonLayout);

    // Separator
    QWidget *separator = new QWidget(this);
    separator->setObjectName("separator");
    separator->setFixedHeight(1);
    mainLayout->addWidget(separator);

    // Network interfaces section
    QLabel *ifaceTitle = new QLabel("Network Interfaces", this);
    ifaceTitle->setObjectName("sectionTitle");
    ifaceTitle->setFont(QFont("Segoe UI", 13, QFont::Bold));
    mainLayout->addWidget(ifaceTitle);

    // Search box
    searchBox = new QLineEdit(this);
    searchBox->setObjectName("searchBox");
    searchBox->setPlaceholderText("Filter interfaces...");
    searchBox->setFixedHeight(30);
    searchBox->setFont(QFont("Arial", 10));
    mainLayout->addWidget(searchBox);
    connect(searchBox, &QLineEdit::textChanged, this, &MainWindow::filterInterfaces);

    // Interfaces table
    interfacesTable = new QTableWidget(this);
    interfacesTable->setObjectName("interfacesTable");
    interfacesTable->setColumnCount(5);
    interfacesTable->setHorizontalHeaderLabels({"Interface", "IP Address", "MAC Address", "Status", "Type"});
    interfacesTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    interfacesTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    interfacesTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    interfacesTable->verticalHeader()->setVisible(false);
    interfacesTable->setAlternatingRowColors(true);
    interfacesTable->setFont(QFont("Courier New", 10));
    mainLayout->addWidget(interfacesTable);
    connect(interfacesTable, static_cast<void(QTableWidget::*)(int, int)>(&QTableWidget::cellDoubleClicked), this, [this](int row, int) {
        QTableWidgetItem *item = interfacesTable->item(row, 1);
        if (item) {
            QApplication::clipboard()->setText(item->text());
            QMessageBox::information(this, "Copied", "IP copied: " + item->text());
        }
    });

    populateInterfaces();

    // Auto refresh timer
    autoRefreshTimer = new QTimer(this);
    autoRefreshTimer->setInterval(60000);
    connect(autoRefreshTimer, &QTimer::timeout, this, &MainWindow::refreshIps);
    if (autoRefreshEnabled) {
        autoRefreshTimer->start();
        autoRefreshButton->setText("Auto: ON");
    }

    // Context menu for tray
    QMenu *trayMenu = new QMenu();
    QAction *showAction = new QAction("Show Window", trayMenu);
    connect(showAction, &QAction::triggered, this, &MainWindow::showWindow);
    trayMenu->addAction(showAction);
    trayMenu->addSeparator();
    QAction *quitAction = new QAction("Quit", trayMenu);
    connect(quitAction, &QAction::triggered, this, []() { QCoreApplication::quit(); });
    trayMenu->addAction(quitAction);
    trayIcon = new QSystemTrayIcon(this);
    trayIcon->setContextMenu(trayMenu);
    trayIcon->setIcon(QIcon::fromTheme("application-internet"));
    trayIcon->setToolTip("IP Inspector Pro");
    connect(trayIcon, &QSystemTrayIcon::activated, this, &MainWindow::onTrayActivated);
}

void MainWindow::populateInterfaces()
{
    QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
    interfacesTable->setRowCount(interfaces.count());

    int row = 0;
    for (const QNetworkInterface &iface : interfaces) {
        if (!iface.isValid() || (iface.flags() & QNetworkInterface::IsLoopBack)) {
            continue;
        }

        interfacesTable->setItem(row, 0, new QTableWidgetItem(iface.humanReadableName()));

        QString mac = iface.hardwareAddress();
        interfacesTable->setItem(row, 2, new QTableWidgetItem(mac.isEmpty() ? "N/A" : mac));

        bool up = iface.flags() & QNetworkInterface::IsUp;
        bool running = iface.flags() & QNetworkInterface::IsRunning;
        QString status = (up && running) ? "Active" : "Inactive";
        QTableWidgetItem *statusItem = new QTableWidgetItem(status);
        if (up && running) {
            statusItem->setForeground(QColor("#27ae60"));
        } else {
            statusItem->setForeground(QColor("#e74c3c"));
        }
        interfacesTable->setItem(row, 3, statusItem);

        QStringList addresses;
        QList<QNetworkAddressEntry> entries = iface.addressEntries();
        for (const QNetworkAddressEntry &entry : entries) {
            if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol || entry.ip().protocol() == QAbstractSocket::IPv6Protocol) {
                QString addr = entry.ip().toString();
                if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol) {
                    QString netmask = entry.netmask().toString();
                    if (!netmask.isNull() && netmask != "255.255.255.255") {
                        addr += " /" + netmask;
                    }
                }
                addresses << addr;
            }
        }
        interfacesTable->setItem(row, 1, new QTableWidgetItem(addresses.join(", ")));

        QString type;
        if (iface.type() == QNetworkInterface::Wifi) type = "WiFi";
        else if (iface.type() == QNetworkInterface::Ethernet) type = "Ethernet";
        else type = "Other";
        interfacesTable->setItem(row, 4, new QTableWidgetItem(type));

        row++;
    }

    interfacesTable->setRowCount(row);
}

void MainWindow::filterInterfaces()
{
    QString filter = searchBox->text().toLower();
    for (int row = 0; row < interfacesTable->rowCount(); row++) {
        QString name = interfacesTable->item(row, 0)->text().toLower();
        QString ip = interfacesTable->item(row, 1)->text().toLower();
        bool match = filter.isEmpty() || name.contains(filter) || ip.contains(filter);
        interfacesTable->setRowHidden(row, !match);
    }
}

void MainWindow::setTimeout(std::function<void()> func, int ms)
{
    QTimer *timer = new QTimer(this);
    timer->setSingleShot(true);
    connect(timer, &QTimer::timeout, this, [func, timer]() {
        func();
        timer->deleteLater();
    });
    timer->start(ms);
}

QString MainWindow::getLocalIp() const
{
    QString ipv4 = "Not found";
    QString ipv6 = "Not found";
    QList<QHostAddress> localAddresses = QNetworkInterface::allAddresses();
    for (const QHostAddress &address : localAddresses) {
        if (address != QHostAddress(QHostAddress::LocalHost) && address != QHostAddress(QHostAddress::LocalHostIPv6)) {
            if (address.protocol() == QAbstractSocket::IPv4Protocol && ipv4 == "Not found") {
                ipv4 = address.toString();
            } else if (address.protocol() == QAbstractSocket::IPv6Protocol && ipv6 == "Not found" && !address.isLoopback()) {
                ipv6 = address.toString();
            }
        }
    }
    if (ipv4 != "Not found" && ipv6 != "Not found") {
        return ipv4 + " | " + ipv6;
    } else if (ipv4 != "Not found") {
        return ipv4;
    } else if (ipv6 != "Not found") {
        return ipv6;
    }
    return "Not found";
}

QList<QPair<QString, QString>> MainWindow::getAllLocalIps() const
{
    QList<QPair<QString, QString>> result;
    QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface &iface : interfaces) {
        if (!iface.isValid()) continue;
        QList<QNetworkAddressEntry> entries = iface.addressEntries();
        for (const QNetworkAddressEntry &entry : entries) {
            if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol &&
                entry.ip() != QHostAddress(QHostAddress::LocalHost)) {
                result.append(qMakePair(entry.ip().toString(), iface.name()));
            }
        }
    }
    return result;
}

QString MainWindow::getMacAddress() const
{
    QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
    QStringList macs;
    for (const QNetworkInterface &iface : interfaces) {
        if (iface.isValid() && !(iface.flags() & QNetworkInterface::IsLoopBack)) {
            QString mac = iface.hardwareAddress();
            if (!mac.isEmpty()) macs << mac;
        }
    }
    return macs.isEmpty() ? "N/A" : macs.join(", ");
}

QString MainWindow::getHostname() const
{
    return QHostInfo::localHostName();
}

bool MainWindow::isOnline() const
{
    QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface &iface : interfaces) {
        if (iface.isValid() && (iface.flags() & QNetworkInterface::IsUp) &&
            !(iface.flags() & QNetworkInterface::IsLoopBack)) {
            QList<QNetworkAddressEntry> entries = iface.addressEntries();
            for (const QNetworkAddressEntry &entry : entries) {
                if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol) {
                    return true;
                }
            }
        }
    }
    return false;
}

void MainWindow::fetchPublicIp()
{
    QUrl url("https://api.ipify.org?format=json");
    QNetworkRequest request(url);
    QNetworkReply *reply = networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        onFetchPublicIpFinished(reply);
    });

    // Also fetch public IPv6
    QUrl url6("https://api6.ipify.org?format=json");
    QNetworkRequest request6(url6);
    QNetworkReply *reply6 = networkManager->get(request6);
    connect(reply6, &QNetworkReply::finished, this, [this, reply6]() {
        if (reply6->error() == QNetworkReply::NoError) {
            QByteArray data = reply6->readAll();
            QString jsonStr(data);
            int start = jsonStr.indexOf("\"ip\":\"") + 6;
            int end = jsonStr.indexOf("\"", start);
            if (start > 6 && end > start) {
                QString ip6 = jsonStr.mid(start, end - start);
                QString currentPub = publicValueLabel->text();
                if (!currentPub.contains("Loading") && !currentPub.contains("Error") && !currentPub.contains(ip6)) {
                    publicValueLabel->setText(currentPub + "\n(v6: " + ip6 + ")");
                } else if (currentPub.contains("Loading") || currentPub.contains("Error")) {
                    publicValueLabel->setText(ip6 + " (v6)");
                }
            }
        }
        reply6->deleteLater();
    });
}

void MainWindow::onFetchPublicIpFinished(QNetworkReply *reply)
{
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray data = reply->readAll();
        QString jsonStr(data);
        int start = jsonStr.indexOf("\"ip\":\"") + 6;
        int end = jsonStr.indexOf("\"", start);
        if (start > 6 && end > start) {
            QString ip = jsonStr.mid(start, end - start);
            publicValueLabel->setText(ip);

            historyList.prepend(ip);
            if (historyList.size() > 10) historyList.removeLast();

            fetchGeoInfo(ip);
        } else {
            publicValueLabel->setText("Invalid format");
        }
    } else {
        publicValueLabel->setText("Error fetching");
    }
    reply->deleteLater();
}

void MainWindow::fetchGeoInfo(const QString &ip)
{
    QUrl url("https://ipapi.co/" + ip + "/json/");
    QNetworkRequest request(url);
    QNetworkReply *reply = networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        onFetchGeoInfoFinished(reply);
    });
}

void MainWindow::onFetchGeoInfoFinished(QNetworkReply *reply)
{
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray data = reply->readAll();
        QString jsonStr(data);

        QString city = extractJsonString(jsonStr, "city");
        QString region = extractJsonString(jsonStr, "region");
        QString country = extractJsonString(jsonStr, "country_name");
        QString org = extractJsonString(jsonStr, "org");

        QString location;
        if (!city.isNull()) location += city;
        if (!region.isNull() && region != city) location += ", " + region;
        if (!country.isNull()) location += ", " + country;
        if (!org.isNull()) location += " (" + org + ")";

        geoLocationLabel->setText(location.isEmpty() ? "Unknown" : location);
    } else {
        geoLocationLabel->setText("Location unavailable");
    }
    reply->deleteLater();
}

QString MainWindow::extractJsonString(const QString &json, const QString &key)
{
    QString pattern = "\"" + key + "\":\"";
    int start = json.indexOf(pattern);
    if (start == -1) return QString();
    start += pattern.length();
    int end = json.indexOf("\"", start);
    if (end == -1) return QString();
    return json.mid(start, end - start);
}

void MainWindow::refreshIps()
{
    localValueLabel->setText(getLocalIp());
    hostnameLabel->setText("Hostname: " + getHostname());
    macLabel->setText("MAC: " + getMacAddress());

    bool online = isOnline();
    if (online) {
        statusIndicator->setText("\u25cf Online");
        statusIndicator->setObjectName("statusOnline");
    } else {
        statusIndicator->setText("\u25cf Offline");
        statusIndicator->setObjectName("statusOffline");
    }
    statusIndicator->style()->unpolish(statusIndicator);
    statusIndicator->style()->polish(statusIndicator);

    fetchPublicIp();
    populateInterfaces();
    updateSystemTray();
}

void MainWindow::copyToLocalClipboard()
{
    QString ip = localValueLabel->text();
    if (ip != "Loading..." && ip != "Not found") {
        QApplication::clipboard()->setText(ip);
        QMessageBox::information(this, "Copied", "Local IP copied to clipboard!");
    }
}

void MainWindow::copyPublicToClipboard()
{
    QString ip = publicValueLabel->text();
    if (ip != "Loading..." && ip != "Error fetching" && ip != "Invalid format") {
        QApplication::clipboard()->setText(ip);
        QMessageBox::information(this, "Copied", "Public IP copied to clipboard!");
    }
}

void MainWindow::exportData()
{
    QString localIp = localValueLabel->text();
    QString publicIp = publicValueLabel->text();
    QString location = geoLocationLabel->text();
    QString hostname = hostnameLabel->text();
    QString mac = macLabel->text();

    QString content = "=== IP Inspector Pro - Export ===\n\n";
    content += "Export Time: " + QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss") + "\n\n";
    content += "--- Network Info ---\n";
    content += hostname + "\n";
    content += mac + "\n";
    content += "Local IP: " + localIp + "\n";
    content += "Public IP: " + publicIp + "\n";
    content += "Location: " + location + "\n\n";

    content += "--- Network Interfaces ---\n";
    for (int row = 0; row < interfacesTable->rowCount(); row++) {
        content += interfacesTable->item(row, 0)->text() + ": ";
        content += interfacesTable->item(row, 1)->text() + " | ";
        content += interfacesTable->item(row, 2)->text() + " | ";
        content += interfacesTable->item(row, 3)->text() + " | ";
        content += interfacesTable->item(row, 4)->text() + "\n";
    }

    QString fileName = QFileDialog::getSaveFileName(this, "Export IP Data", "", "Text Files (*.txt);;JSON Files (*.json)");
    if (!fileName.isEmpty()) {
        QFile file(fileName);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            file.write(content.toUtf8());
            QMessageBox::information(this, "Success", "Data exported successfully!\n" + fileName);
        } else {
            QMessageBox::critical(this, "Error", "Could not save file.");
        }
    }
}

void MainWindow::toggleAutoRefresh()
{
    autoRefreshEnabled = !autoRefreshEnabled;
    if (autoRefreshEnabled) {
        autoRefreshTimer->start();
        autoRefreshButton->setText("Auto: ON");
    } else {
        autoRefreshTimer->stop();
        autoRefreshButton->setText("Auto: OFF");
    }
    saveSettings();
}

void MainWindow::onThemeToggle()
{
    darkTheme = !darkTheme;
    if (darkTheme) {
        applyDarkTheme();
        themeButton->setText("\u263d");
    } else {
        applyLightTheme();
        themeButton->setText("\u2600");
    }

    bool online = isOnline();
    if (online) {
        statusIndicator->setObjectName("statusOnline");
    } else {
        statusIndicator->setObjectName("statusOffline");
    }
    statusIndicator->style()->unpolish(statusIndicator);
    statusIndicator->style()->polish(statusIndicator);

    saveSettings();
}

void MainWindow::applyLightTheme()
{
    setStyleSheet(
        "QMainWindow, #centralWidget { background-color: #ecf0f1; }"
        "QWidget { color: #2c3e50; }"
        "#titleLabel { color: #2c3e50; }"
        "#sectionTitle { color: #2c3e50; padding: 5px; }"
        "#subtitle { color: #6c757d; }"
        "#valueText { color: #2c3e50; }"
        "#card { background-color: #ffffff; border-radius: 10px; }"
        "#iconLocal { color: #3498db; }"
        "#iconPublic { color: #e74c3c; }"
        "#iconGeo { color: #2ecc71; }"
        "#iconSystem { color: #9b59b6; }"
        "#statusOnline { color: #27ae60; }"
        "#statusOffline { color: #e74c3c; }"
        "#separator { background-color: #dee2e6; }"
        "QPushButton { background-color: #ffffff; color: #2c3e50; border: 1px solid #dee2e6; border-radius: 5px; padding: 5px 10px; }"
        "QPushButton:hover { background-color: #f8f9fa; border-color: #3498db; }"
        "QPushButton:pressed { background-color: #e9ecef; }"
        "#toolButton { background-color: #ffffff; border: 1px solid #dee2e6; border-radius: 5px; }"
        "#copyBtn { background-color: #ffffff; color: #2c3e50; border: 1px solid #dee2e6; border-radius: 4px; }"
        "#copyBtn:hover { background-color: #3498db; color: white; border-color: #3498db; }"
        "#btnPrimary { background-color: #3498db; color: white; border: none; border-radius: 5px; }"
        "#btnPrimary:hover { background-color: #2980b9; }"
        "#btnPrimary:pressed { background-color: #21618c; }"
        "#btnSuccess { background-color: #2ecc71; color: white; border: none; border-radius: 5px; }"
        "#btnSuccess:hover { background-color: #27ae60; }"
        "#btnSuccess:pressed { background-color: #229954; }"
        "#btnDanger { background-color: #e74c3c; color: white; border: none; border-radius: 5px; }"
        "#btnDanger:hover { background-color: #c0392b; }"
        "#btnDanger:pressed { background-color: #a93226; }"
        "#searchBox { background-color: #ffffff; color: #2c3e50; border: 1px solid #dee2e6; border-radius: 5px; padding: 5px; }"
        "#searchBox:focus { border: 2px solid #3498db; }"
        "#interfacesTable { background-color: #ffffff; alternate-background-color: #f8f9fa; border: 1px solid #dee2e6; border-radius: 5px; gridline-color: #dee2e6; }"
        "#interfacesTable::item { padding: 5px; color: #2c3e50; }"
        "#interfacesTable::item:selected { background-color: #3498db; color: white; }"
        "QHeaderView::section { background-color: #f8f9fa; color: #2c3e50; padding: 6px; border: none; border-bottom: 1px solid #dee2e6; font-weight: bold; }"
        "QScrollBar:vertical { background: #f8f9fa; width: 10px; border: none; }"
        "QScrollBar::handle:vertical { background: #dee2e6; border-radius: 5px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
    );
}

void MainWindow::applyDarkTheme()
{
    setStyleSheet(
        "QMainWindow, #centralWidget { background-color: #1e1e2e; }"
        "QWidget { color: #cdd6f4; }"
        "#titleLabel { color: #89b4fa; }"
        "#sectionTitle { color: #cdd6f4; padding: 5px; }"
        "#subtitle { color: #7f849c; }"
        "#valueText { color: #cdd6f4; }"
        "#card { background-color: #313244; border-radius: 10px; }"
        "#iconLocal { color: #89b4fa; }"
        "#iconPublic { color: #f38ba8; }"
        "#iconGeo { color: #a6e3a1; }"
        "#iconSystem { color: #cba6f7; }"
        "#statusOnline { color: #a6e3a1; }"
        "#statusOffline { color: #f38ba8; }"
        "#separator { background-color: #45475a; }"
        "QPushButton { background-color: #313244; color: #cdd6f4; border: 1px solid #45475a; border-radius: 5px; padding: 5px 10px; }"
        "QPushButton:hover { background-color: #45475a; border-color: #89b4fa; }"
        "QPushButton:pressed { background-color: #585b70; }"
        "#toolButton { background-color: #313244; color: #cdd6f4; border: 1px solid #45475a; border-radius: 5px; }"
        "#copyBtn { background-color: #45475a; color: #cdd6f4; border: 1px solid #585b70; border-radius: 4px; }"
        "#copyBtn:hover { background-color: #89b4fa; color: #1e1e2e; border-color: #89b4fa; }"
        "#btnPrimary { background-color: #89b4fa; color: #1e1e2e; border: none; border-radius: 5px; }"
        "#btnPrimary:hover { background-color: #b4befe; }"
        "#btnPrimary:pressed { background-color: #74c7ec; }"
        "#btnSuccess { background-color: #a6e3a1; color: #1e1e2e; border: none; border-radius: 5px; }"
        "#btnSuccess:hover { background-color: #b5e8b0; }"
        "#btnSuccess:pressed { background-color: #94d68f; }"
        "#btnDanger { background-color: #f38ba8; color: #1e1e2e; border: none; border-radius: 5px; }"
        "#btnDanger:hover { background-color: #eba0bc; }"
        "#btnDanger:pressed { background-color: #eb779e; }"
        "#searchBox { background-color: #313244; color: #cdd6f4; border: 1px solid #45475a; border-radius: 5px; padding: 5px; }"
        "#searchBox:focus { border: 2px solid #89b4fa; }"
        "#interfacesTable { background-color: #1e1e2e; alternate-background-color: #313244; border: 1px solid #45475a; border-radius: 5px; gridline-color: #45475a; }"
        "#interfacesTable::item { padding: 5px; color: #cdd6f4; }"
        "#interfacesTable::item:selected { background-color: #89b4fa; color: #1e1e2e; }"
        "QHeaderView::section { background-color: #313244; color: #cdd6f4; padding: 6px; border: none; border-bottom: 1px solid #45475a; font-weight: bold; }"
        "QScrollBar:vertical { background: #1e1e2e; width: 10px; border: none; }"
        "QScrollBar::handle:vertical { background: #45475a; border-radius: 5px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
        "QMenu { background-color: #313244; color: #cdd6f4; border: 1px solid #45475a; }"
        "QMenu::item:selected { background-color: #45475a; }"
        "QMessageBox { background-color: #1e1e2e; }"
        "QMessageBox QLabel { color: #cdd6f4; }"
        "QFileDialog { background-color: #1e1e2e; color: #cdd6f4; }"
    );
}

void MainWindow::updateSystemTray()
{
    QString tip = "IP Inspector Pro\nLocal: " + localValueLabel->text();
    if (publicValueLabel->text() != "Loading..." && publicValueLabel->text() != "Error fetching") {
        tip += "\nPublic: " + publicValueLabel->text();
    }
    trayIcon->setToolTip(tip);
}

void MainWindow::showContextMenu([[maybe_unused]] const QPoint &pos)
{
}

void MainWindow::onTrayActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) {
        showWindow();
    }
}

void MainWindow::showWindow()
{
    showNormal();
    raise();
    activateWindow();
}

void MainWindow::clearHistory()
{
    historyList.clear();
    QMessageBox::information(this, "Cleared", "IP history cleared!");
}

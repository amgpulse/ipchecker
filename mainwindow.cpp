#include "mainwindow.h"
#include "netutils.h"

#include <QAbstractItemView>
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QColor>
#include <QComboBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFont>
#include <QFontDatabase>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QHostAddress>
#include <QIcon>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLabel>
#include <QMenu>
#include <QNetworkInterface>
#include <QNetworkRequest>
#include <QPushButton>
#include <QSettings>
#include <QStatusBar>
#include <QTableWidget>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

namespace {

QString loadTextResource(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    return QString::fromUtf8(file.readAll());
}

QString displayHostname()
{
    const QString value = NetUtils::hostname();
    return value.isEmpty() ? QObject::tr("Unknown") : value;
}

QString displayMacAddress()
{
    const QString value = NetUtils::macAddress();
    return value.isEmpty() ? QStringLiteral("N/A") : value;
}

QString replyErrorText(QNetworkReply *reply)
{
    switch (reply->error()) {
    case QNetworkReply::TimeoutError:
    case QNetworkReply::OperationCanceledError:
        return QObject::tr("Timed out");
    case QNetworkReply::HostNotFoundError:
        return QObject::tr("Host not found");
    default:
        return QObject::tr("Error fetching");
    }
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , networkManager(new QNetworkAccessManager(this))
    , trayIcon(nullptr)
    , darkTheme(false)
    , autoRefreshEnabled(false)
{
    networkManager->setTransferTimeout(kNetworkTimeoutMs);
    networkManager->setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);

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
    QSettings settings(QStringLiteral("IPInspector"), QStringLiteral("Main"));
    darkTheme = settings.value(QStringLiteral("darkTheme"), false).toBool();
    autoRefreshEnabled = settings.value(QStringLiteral("autoRefresh"), false).toBool();

    const int defaultIndex = NetUtils::intervalIndexForMs(refreshIntervalMs);
    const int intervalIndex = settings.value(QStringLiteral("refreshIntervalIndex"), defaultIndex).toInt();
    refreshIntervalMs = NetUtils::refreshIntervalMsAt(intervalIndex);

    historyList.clear();
    const int historyCount = settings.beginReadArray(QStringLiteral("history"));
    for (int i = 0; i < historyCount; ++i) {
        settings.setArrayIndex(i);
        IpHistoryEntry entry;
        entry.timestamp = settings.value(QStringLiteral("timestamp")).toDateTime();
        entry.ipv4 = settings.value(QStringLiteral("ipv4")).toString();
        entry.ipv6 = settings.value(QStringLiteral("ipv6")).toString();
        historyList.append(entry);
    }
    settings.endArray();
    while (historyList.size() > kMaxHistoryEntries) {
        historyList.removeLast();
    }

    geoCache.clear();
    const int geoCount = settings.beginReadArray(QStringLiteral("geoCache"));
    for (int i = 0; i < geoCount && geoCache.size() < kMaxGeoCacheEntries; ++i) {
        settings.setArrayIndex(i);
        const QString ip = settings.value(QStringLiteral("ip")).toString();
        const QString location = settings.value(QStringLiteral("location")).toString();
        if (!ip.isEmpty() && !location.isEmpty()) {
            geoCache.insert(ip, location);
        }
    }
    settings.endArray();

    const QByteArray geometry = settings.value(QStringLiteral("geometry")).toByteArray();
    if (!geometry.isEmpty()) {
        restoreGeometry(geometry);
    }
}

void MainWindow::saveHistory()
{
    QSettings settings(QStringLiteral("IPInspector"), QStringLiteral("Main"));
    settings.beginWriteArray(QStringLiteral("history"));
    for (int i = 0; i < historyList.size(); ++i) {
        const IpHistoryEntry &entry = historyList.at(i);
        settings.setArrayIndex(i);
        settings.setValue(QStringLiteral("timestamp"), entry.timestamp);
        settings.setValue(QStringLiteral("ipv4"), entry.ipv4);
        settings.setValue(QStringLiteral("ipv6"), entry.ipv6);
    }
    settings.endArray();
}

void MainWindow::saveGeoCache()
{
    QSettings settings(QStringLiteral("IPInspector"), QStringLiteral("Main"));
    settings.beginWriteArray(QStringLiteral("geoCache"));
    int index = 0;
    for (auto it = geoCache.cbegin(); it != geoCache.cend() && index < kMaxGeoCacheEntries; ++it) {
        settings.setArrayIndex(index++);
        settings.setValue(QStringLiteral("ip"), it.key());
        settings.setValue(QStringLiteral("location"), it.value());
    }
    settings.endArray();
}

void MainWindow::saveSettings()
{
    QSettings settings(QStringLiteral("IPInspector"), QStringLiteral("Main"));
    settings.setValue(QStringLiteral("darkTheme"), darkTheme);
    settings.setValue(QStringLiteral("autoRefresh"), autoRefreshEnabled);
    settings.setValue(QStringLiteral("refreshIntervalIndex"),
                      NetUtils::intervalIndexForMs(refreshIntervalMs));
    settings.setValue(QStringLiteral("geometry"), saveGeometry());
    saveHistory();
    saveGeoCache();
}

void MainWindow::setupUi()
{
    QWidget *centralWidget = new QWidget(this);
    centralWidget->setObjectName(QStringLiteral("centralWidget"));
    setCentralWidget(centralWidget);

    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(15, 15, 15, 15);
    mainLayout->setSpacing(10);

    auto sizedFont = [this](int pointSize, bool bold = false) {
        QFont f = font();
        f.setPointSize(pointSize);
        f.setBold(bold);
        return f;
    };

    // Title bar
    QHBoxLayout *titleBar = new QHBoxLayout();
    QLabel *titleLabel = new QLabel(QStringLiteral("IP Inspector Pro"), this);
    titleLabel->setObjectName(QStringLiteral("titleLabel"));
    titleLabel->setFont(sizedFont(18, true));
    titleBar->addWidget(titleLabel);
    titleBar->addStretch();

    themeButton = new QPushButton(darkTheme ? QStringLiteral("\u263d") : QStringLiteral("\u2600"), this);
    themeButton->setObjectName(QStringLiteral("toolButton"));
    themeButton->setFixedSize(35, 35);
    themeButton->setToolTip(tr("Toggle theme"));
    themeButton->setFont(sizedFont(14));
    titleBar->addWidget(themeButton);
    connect(themeButton, &QPushButton::clicked, this, &MainWindow::onThemeToggle);

    autoRefreshButton = new QPushButton(QStringLiteral("Auto: OFF"), this);
    autoRefreshButton->setObjectName(QStringLiteral("toolButton"));
    autoRefreshButton->setFixedSize(90, 35);
    autoRefreshButton->setFont(sizedFont(10));
    autoRefreshButton->setToolTip(tr("Toggle automatic refresh"));
    titleBar->addWidget(autoRefreshButton);
    connect(autoRefreshButton, &QPushButton::clicked, this, &MainWindow::toggleAutoRefresh);

    intervalCombo = new QComboBox(this);
    intervalCombo->setObjectName(QStringLiteral("intervalCombo"));
    intervalCombo->setFixedHeight(35);
    intervalCombo->setFixedWidth(75);
    intervalCombo->setFont(sizedFont(10));
    intervalCombo->setToolTip(tr("Auto-refresh interval"));
    for (int i = 0; i < NetUtils::refreshIntervalCount(); ++i) {
        intervalCombo->addItem(NetUtils::refreshIntervalLabelAt(i));
    }
    intervalCombo->setCurrentIndex(NetUtils::intervalIndexForMs(refreshIntervalMs));
    titleBar->addWidget(intervalCombo);
    connect(intervalCombo, &QComboBox::currentIndexChanged, this, &MainWindow::onRefreshIntervalChanged);

    mainLayout->addLayout(titleBar);

    // Status indicator
    QHBoxLayout *statusLayout = new QHBoxLayout();
    statusIndicator = new QLabel(QStringLiteral("\u25cf Disconnected"), this);
    statusIndicator->setObjectName(QStringLiteral("statusOffline"));
    statusIndicator->setFont(sizedFont(10, true));
    statusIndicator->setTextFormat(Qt::PlainText);
    statusLayout->addWidget(statusIndicator);
    statusLayout->addStretch();
    mainLayout->addLayout(statusLayout);

    // Main info cards
    QGridLayout *infoGrid = new QGridLayout();
    infoGrid->setSpacing(10);

    // Local IP card
    QWidget *localCard = new QWidget(this);
    localCard->setObjectName(QStringLiteral("card"));
    QHBoxLayout *localLayout = new QHBoxLayout(localCard);
    localLayout->setContentsMargins(12, 12, 12, 12);

    QLabel *localIcon = new QLabel(QStringLiteral("\u25cf"), this);
    localIcon->setObjectName(QStringLiteral("iconLocal"));
    localIcon->setFont(sizedFont(20));
    localLayout->addWidget(localIcon);

    QVBoxLayout *localInfo = new QVBoxLayout();
    QLabel *localLabel = new QLabel(tr("Local IP"), this);
    localLabel->setObjectName(QStringLiteral("subtitle"));
    localLabel->setFont(sizedFont(10));
    localInfo->addWidget(localLabel);

    QFont ipFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    ipFont.setPointSize(13);
    ipFont.setBold(true);

    localValueLabel = new QLabel(QStringLiteral("Loading..."), this);
    localValueLabel->setObjectName(QStringLiteral("valueText"));
    localValueLabel->setTextFormat(Qt::PlainText);
    localValueLabel->setFont(ipFont);
    localInfo->addWidget(localValueLabel);

    localLayout->addLayout(localInfo);
    localLayout->addStretch();

    QPushButton *copyLocalBtn = new QPushButton(tr("Copy"), this);
    copyLocalBtn->setObjectName(QStringLiteral("copyBtn"));
    copyLocalBtn->setFixedSize(55, 28);
    copyLocalBtn->setFont(sizedFont(9));
    copyLocalBtn->setToolTip(tr("Copy to clipboard"));
    localLayout->addWidget(copyLocalBtn);
    connect(copyLocalBtn, &QPushButton::clicked, this, &MainWindow::copyToLocalClipboard);

    infoGrid->addWidget(localCard, 0, 0);

    // Public IP card
    QWidget *publicCard = new QWidget(this);
    publicCard->setObjectName(QStringLiteral("card"));
    QHBoxLayout *publicLayout = new QHBoxLayout(publicCard);
    publicLayout->setContentsMargins(12, 12, 12, 12);

    QLabel *publicIcon = new QLabel(QStringLiteral("\u25cf"), this);
    publicIcon->setObjectName(QStringLiteral("iconPublic"));
    publicIcon->setFont(sizedFont(20));
    publicLayout->addWidget(publicIcon);

    QVBoxLayout *publicInfo = new QVBoxLayout();
    QLabel *publicLabel = new QLabel(tr("Public IP"), this);
    publicLabel->setObjectName(QStringLiteral("subtitle"));
    publicLabel->setFont(sizedFont(10));
    publicInfo->addWidget(publicLabel);

    publicValueLabel = new QLabel(QStringLiteral("Loading..."), this);
    publicValueLabel->setObjectName(QStringLiteral("valueText"));
    publicValueLabel->setTextFormat(Qt::PlainText);
    publicValueLabel->setFont(ipFont);
    publicInfo->addWidget(publicValueLabel);

    publicLayout->addLayout(publicInfo);
    publicLayout->addStretch();

    QPushButton *copyPublicBtn = new QPushButton(tr("Copy"), this);
    copyPublicBtn->setObjectName(QStringLiteral("copyBtn"));
    copyPublicBtn->setFixedSize(55, 28);
    copyPublicBtn->setFont(sizedFont(9));
    copyPublicBtn->setToolTip(tr("Copy to clipboard"));
    publicLayout->addWidget(copyPublicBtn);
    connect(copyPublicBtn, &QPushButton::clicked, this, &MainWindow::copyPublicToClipboard);

    infoGrid->addWidget(publicCard, 0, 1);

    // Location card
    QWidget *geoCard = new QWidget(this);
    geoCard->setObjectName(QStringLiteral("card"));
    QHBoxLayout *geoLayout = new QHBoxLayout(geoCard);
    geoLayout->setContentsMargins(12, 12, 12, 12);

    QLabel *geoIcon = new QLabel(QStringLiteral("\u25cf"), this);
    geoIcon->setObjectName(QStringLiteral("iconGeo"));
    geoIcon->setFont(sizedFont(20));
    geoLayout->addWidget(geoIcon);

    QVBoxLayout *geoInfo = new QVBoxLayout();
    QLabel *geoLabel = new QLabel(tr("Location"), this);
    geoLabel->setObjectName(QStringLiteral("subtitle"));
    geoLabel->setFont(sizedFont(10));
    geoInfo->addWidget(geoLabel);

    geoLocationLabel = new QLabel(QStringLiteral("Loading..."), this);
    geoLocationLabel->setObjectName(QStringLiteral("valueText"));
    geoLocationLabel->setTextFormat(Qt::PlainText);
    geoLocationLabel->setFont(sizedFont(11));
    geoInfo->addWidget(geoLocationLabel);

    geoLayout->addLayout(geoInfo);
    geoLayout->addStretch();

    infoGrid->addWidget(geoCard, 1, 0);

    // System info card
    QWidget *systemCard = new QWidget(this);
    systemCard->setObjectName(QStringLiteral("card"));
    QHBoxLayout *systemLayout = new QHBoxLayout(systemCard);
    systemLayout->setContentsMargins(12, 12, 12, 12);

    QLabel *sysIcon = new QLabel(QStringLiteral("\u25cf"), this);
    sysIcon->setObjectName(QStringLiteral("iconSystem"));
    sysIcon->setFont(sizedFont(20));
    systemLayout->addWidget(sysIcon);

    QVBoxLayout *systemInfo = new QVBoxLayout();
    QLabel *sysLabel = new QLabel(tr("System Info"), this);
    sysLabel->setObjectName(QStringLiteral("subtitle"));
    sysLabel->setFont(sizedFont(10));
    systemInfo->addWidget(sysLabel);

    hostnameLabel = new QLabel(QStringLiteral("Hostname: Loading..."), this);
    hostnameLabel->setObjectName(QStringLiteral("valueText"));
    hostnameLabel->setTextFormat(Qt::PlainText);
    hostnameLabel->setFont(sizedFont(10));
    systemInfo->addWidget(hostnameLabel);

    macLabel = new QLabel(QStringLiteral("MAC: Loading..."), this);
    macLabel->setObjectName(QStringLiteral("valueText"));
    macLabel->setTextFormat(Qt::PlainText);
    macLabel->setFont(sizedFont(10));
    systemInfo->addWidget(macLabel);

    systemLayout->addLayout(systemInfo);
    systemLayout->addStretch();

    infoGrid->addWidget(systemCard, 1, 1);

    mainLayout->addLayout(infoGrid);

    // Buttons row
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();

    QPushButton *refreshBtn = new QPushButton(QStringLiteral("\u21bb ") + tr("Refresh All"), this);
    refreshBtn->setObjectName(QStringLiteral("btnPrimary"));
    refreshBtn->setFixedSize(130, 35);
    refreshBtn->setFont(sizedFont(11, true));
    buttonLayout->addWidget(refreshBtn);
    connect(refreshBtn, &QPushButton::clicked, this, &MainWindow::refreshIps);

    QPushButton *historyBtn = new QPushButton(tr("History"), this);
    historyBtn->setFixedSize(100, 35);
    historyBtn->setFont(sizedFont(11, true));
    historyBtn->setToolTip(tr("Show recorded public IP history"));
    buttonLayout->addWidget(historyBtn);
    connect(historyBtn, &QPushButton::clicked, this, &MainWindow::showHistory);

    QPushButton *exportBtn = new QPushButton(tr("Export"), this);
    exportBtn->setObjectName(QStringLiteral("btnSuccess"));
    exportBtn->setFixedSize(100, 35);
    exportBtn->setFont(sizedFont(11, true));
    buttonLayout->addWidget(exportBtn);
    connect(exportBtn, &QPushButton::clicked, this, &MainWindow::exportData);

    QPushButton *clearHistoryBtn = new QPushButton(tr("Clear History"), this);
    clearHistoryBtn->setObjectName(QStringLiteral("btnDanger"));
    clearHistoryBtn->setFixedSize(130, 35);
    clearHistoryBtn->setFont(sizedFont(11, true));
    buttonLayout->addWidget(clearHistoryBtn);
    connect(clearHistoryBtn, &QPushButton::clicked, this, &MainWindow::clearHistory);

    mainLayout->addLayout(buttonLayout);

    // Separator
    QWidget *separator = new QWidget(this);
    separator->setObjectName(QStringLiteral("separator"));
    separator->setFixedHeight(1);
    mainLayout->addWidget(separator);

    // Network interfaces section
    QLabel *ifaceTitle = new QLabel(tr("Network Interfaces"), this);
    ifaceTitle->setObjectName(QStringLiteral("sectionTitle"));
    ifaceTitle->setFont(sizedFont(13, true));
    mainLayout->addWidget(ifaceTitle);

    // Search box
    searchBox = new QLineEdit(this);
    searchBox->setObjectName(QStringLiteral("searchBox"));
    searchBox->setPlaceholderText(tr("Filter interfaces..."));
    searchBox->setFixedHeight(30);
    searchBox->setFont(sizedFont(10));
    mainLayout->addWidget(searchBox);
    connect(searchBox, &QLineEdit::textChanged, this, &MainWindow::filterInterfaces);

    // Interfaces table
    interfacesTable = new QTableWidget(this);
    interfacesTable->setObjectName(QStringLiteral("interfacesTable"));
    interfacesTable->setColumnCount(5);
    interfacesTable->setHorizontalHeaderLabels({tr("Interface"), tr("IP Address"), tr("MAC Address"),
                                                tr("Status"), tr("Type")});
    interfacesTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    interfacesTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    interfacesTable->verticalHeader()->setVisible(false);
    interfacesTable->setAlternatingRowColors(true);
    interfacesTable->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));

    QHeaderView *header = interfacesTable->horizontalHeader();
    header->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(1, QHeaderView::Stretch);
    header->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    header->setSortIndicatorShown(true);

    mainLayout->addWidget(interfacesTable);
    connect(interfacesTable, qOverload<int, int>(&QTableWidget::cellDoubleClicked), this,
            [this](int row, int) {
                const QTableWidgetItem *item = interfacesTable->item(row, 1);
                if (!item || item->text().isEmpty()) {
                    return;
                }
                QApplication::clipboard()->setText(item->text());
                toast(tr("Copied: %1").arg(item->text()));
            });

    interfacesTable->setSortingEnabled(true);

    // Privacy footnote
    QLabel *footnote = new QLabel(tr("Public IP: api.ipify.org  •  Location: ipapi.co"), this);
    footnote->setObjectName(QStringLiteral("footnote"));
    footnote->setAlignment(Qt::AlignRight);
    footnote->setTextFormat(Qt::PlainText);
    mainLayout->addWidget(footnote);

    // Auto refresh timer
    autoRefreshTimer = new QTimer(this);
    autoRefreshTimer->setInterval(refreshIntervalMs);
    connect(autoRefreshTimer, &QTimer::timeout, this, &MainWindow::refreshIps);
    if (autoRefreshEnabled) {
        autoRefreshTimer->start();
        autoRefreshButton->setText(QStringLiteral("Auto: ON"));
    }

    // Context menu for tray
    QMenu *trayMenu = new QMenu(this);
    QAction *showAction = trayMenu->addAction(tr("Show Window"));
    connect(showAction, &QAction::triggered, this, &MainWindow::showWindow);
    QAction *trayRefreshAction = trayMenu->addAction(tr("Refresh Now"));
    connect(trayRefreshAction, &QAction::triggered, this, &MainWindow::refreshIps);
    trayMenu->addSeparator();
    QAction *quitAction = trayMenu->addAction(tr("Quit"));
    connect(quitAction, &QAction::triggered, QCoreApplication::instance(), &QCoreApplication::quit);

    trayIcon = new QSystemTrayIcon(this);
    trayIcon->setContextMenu(trayMenu);
    const QIcon appIcon(QStringLiteral(":/icons/ipchecker.png"));
    trayIcon->setIcon(appIcon);
    setWindowIcon(appIcon);
    trayIcon->setToolTip(QStringLiteral("IP Inspector Pro"));
    connect(trayIcon, &QSystemTrayIcon::activated, this, &MainWindow::onTrayActivated);
}

void MainWindow::populateInterfaces()
{
    const bool sortingWasEnabled = interfacesTable->isSortingEnabled();
    interfacesTable->setSortingEnabled(false);
    interfacesTable->setRowCount(0);

    const QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface &iface : interfaces) {
        if (!iface.isValid() || (iface.flags() & QNetworkInterface::IsLoopBack)) {
            continue;
        }

        const int row = interfacesTable->rowCount();
        interfacesTable->insertRow(row);
        interfacesTable->setItem(row, 0, new QTableWidgetItem(iface.humanReadableName()));

        const QString mac = iface.hardwareAddress();
        interfacesTable->setItem(row, 2, new QTableWidgetItem(mac.isEmpty() ? QStringLiteral("N/A") : mac));

        const bool active = (iface.flags() & QNetworkInterface::IsUp) && (iface.flags() & QNetworkInterface::IsRunning);
        QTableWidgetItem *statusItem = new QTableWidgetItem(active ? QStringLiteral("Active")
                                                                   : QStringLiteral("Inactive"));
        statusItem->setForeground(QColor(active ? QStringLiteral("#27ae60") : QStringLiteral("#e74c3c")));
        interfacesTable->setItem(row, 3, statusItem);

        QStringList ipv4Addresses;
        QStringList ipv6Addresses;
        const QList<QNetworkAddressEntry> entries = iface.addressEntries();
        for (const QNetworkAddressEntry &entry : entries) {
            const QHostAddress ip = entry.ip();
            if (ip.isLoopback()) {
                continue;
            }
            if (ip.protocol() == QAbstractSocket::IPv4Protocol) {
                QString addr = ip.toString();
                const QString netmask = entry.netmask().toString();
                if (!netmask.isEmpty() && netmask != QStringLiteral("255.255.255.255")) {
                    addr += QStringLiteral(" /") + netmask;
                }
                ipv4Addresses << addr;
            } else if (ip.protocol() == QAbstractSocket::IPv6Protocol) {
                ipv6Addresses << ip.toString() + QStringLiteral("/") + QString::number(entry.prefixLength());
            }
        }
        ipv4Addresses.sort(Qt::CaseInsensitive);
        ipv6Addresses.sort(Qt::CaseInsensitive);
        const QStringList addresses = ipv4Addresses + ipv6Addresses;
        interfacesTable->setItem(row, 1, new QTableWidgetItem(addresses.join(QStringLiteral(", "))));

        QString type = QStringLiteral("Other");
        if (iface.type() == QNetworkInterface::Wifi) {
            type = QStringLiteral("WiFi");
        } else if (iface.type() == QNetworkInterface::Ethernet) {
            type = QStringLiteral("Ethernet");
        }
        interfacesTable->setItem(row, 4, new QTableWidgetItem(type));
    }

    interfacesTable->setSortingEnabled(sortingWasEnabled);
    filterInterfaces();
}

void MainWindow::filterInterfaces()
{
    const QString filter = searchBox->text().trimmed().toLower();
    for (int row = 0; row < interfacesTable->rowCount(); ++row) {
        bool match = filter.isEmpty();
        if (!match) {
            for (int col = 0; col < interfacesTable->columnCount(); ++col) {
                const QTableWidgetItem *item = interfacesTable->item(row, col);
                if (item && item->text().toLower().contains(filter)) {
                    match = true;
                    break;
                }
            }
        }
        interfacesTable->setRowHidden(row, !match);
    }
}

void MainWindow::fetchPublicIp()
{
    const int requestId = ++publicIpRequestId;
    publicIpPendingResponses = 2;
    publicIpCycleHadSuccess = false;
    publicIpLastError.clear();
    if (publicIpv4.isEmpty() && publicIpv6.isEmpty()) {
        publicIpErrorText = QStringLiteral("Loading...");
        updatePublicIpDisplay();
    }

    QNetworkRequest request4(QUrl(QStringLiteral("https://api.ipify.org?format=json")));
    QNetworkReply *reply4 = networkManager->get(request4);
    connect(reply4, &QNetworkReply::finished, this, [this, reply4, requestId]() {
        reply4->deleteLater();
        if (requestId != publicIpRequestId) {
            return;
        }
        onFetchPublicIpFinished(reply4);
    });

    QNetworkRequest request6(QUrl(QStringLiteral("https://api6.ipify.org?format=json")));
    QNetworkReply *reply6 = networkManager->get(request6);
    connect(reply6, &QNetworkReply::finished, this, [this, reply6, requestId]() {
        reply6->deleteLater();
        if (requestId != publicIpRequestId) {
            return;
        }
        onFetchIpv6Finished(reply6);
    });
}

void MainWindow::onFetchPublicIpFinished(QNetworkReply *reply)
{
    const QString ip = reply->error() == QNetworkReply::NoError
                           ? NetUtils::parsePublicIpJson(reply->readAll())
                           : QString();
    if (!ip.isEmpty()) {
        publicIpv4 = ip;
        publicIpErrorText.clear();
        publicIpCycleHadSuccess = true;
        publicIpStaleText.clear();
        updatePublicIpDisplay();
        recordHistory();
        fetchGeoInfo(ip);
    } else {
        publicIpLastError = replyErrorText(reply);
    }
    finishPublicIpCycle();
}

void MainWindow::onFetchIpv6Finished(QNetworkReply *reply)
{
    const QString ip = reply->error() == QNetworkReply::NoError
                           ? NetUtils::parsePublicIpJson(reply->readAll())
                           : QString();
    if (!ip.isEmpty()) {
        publicIpv6 = ip;
        publicIpErrorText.clear();
        publicIpCycleHadSuccess = true;
        publicIpStaleText.clear();
        updatePublicIpDisplay();
        recordHistory();
        if (publicIpv4.isEmpty()) {
            fetchGeoInfo(ip);
        }
    } else {
        publicIpLastError = replyErrorText(reply);
    }
    finishPublicIpCycle();
}

void MainWindow::finishPublicIpCycle()
{
    if (publicIpPendingResponses > 0) {
        --publicIpPendingResponses;
    }
    if (publicIpPendingResponses > 0) {
        return;
    }

    if (!publicIpCycleHadSuccess) {
        publicIpStaleText = publicIpLastError.isEmpty() ? tr("Error fetching") : publicIpLastError;
        if (publicIpv4.isEmpty() && publicIpv6.isEmpty()) {
            publicIpErrorText = publicIpStaleText;
        }
        updatePublicIpDisplay();
    }
}

void MainWindow::fetchGeoInfo(const QString &ip)
{
    QHostAddress address;
    if (!address.setAddress(ip)) {
        geoLocationLabel->setText(tr("Unknown"));
        return;
    }

    const QString canonical = address.toString();
    if (geoCache.contains(canonical)) {
        geoLocationLabel->setText(geoCache.value(canonical));
        return;
    }

    const int requestId = ++geoRequestId;
    geoLocationLabel->setText(tr("Loading location..."));

    QNetworkRequest request(QUrl(QStringLiteral("https://ipapi.co/%1/json/").arg(canonical)));
    QNetworkReply *reply = networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, requestId, canonical]() {
        reply->deleteLater();
        if (requestId != geoRequestId) {
            return;
        }
        onFetchGeoInfoFinished(reply, canonical);
    });
}

void MainWindow::onFetchGeoInfoFinished(QNetworkReply *reply, const QString &ip)
{
    if (reply->error() != QNetworkReply::NoError) {
        const bool timedOut = reply->error() == QNetworkReply::TimeoutError
                              || reply->error() == QNetworkReply::OperationCanceledError;
        geoLocationLabel->setText(timedOut ? tr("Timed out") : tr("Location unavailable"));
        return;
    }

    const NetUtils::GeoInfo info = NetUtils::parseGeoInfo(reply->readAll());
    if (!info.parsed) {
        geoLocationLabel->setText(tr("Location unavailable"));
        return;
    }
    if (!info.errorReason.isEmpty()) {
        geoLocationLabel->setText(tr("Lookup failed: %1").arg(info.errorReason));
        return;
    }
    if (info.location.isEmpty()) {
        geoLocationLabel->setText(tr("Unknown"));
        return;
    }

    if (geoCache.size() >= kMaxGeoCacheEntries && !geoCache.contains(ip)) {
        geoCache.erase(geoCache.begin());
    }
    geoCache.insert(ip, info.location);
    saveGeoCache();
    geoLocationLabel->setText(info.location);
}

void MainWindow::updatePublicIpDisplay()
{
    if (publicIpv4.isEmpty() && publicIpv6.isEmpty()) {
        publicValueLabel->setText(publicIpErrorText);
        updateSystemTray();
        return;
    }

    QString text = !publicIpv4.isEmpty() ? publicIpv4 : publicIpv6;
    if (!publicIpv4.isEmpty() && !publicIpv6.isEmpty() && publicIpv4 != publicIpv6) {
        text += QLatin1Char('\n') + QStringLiteral("(v6: ") + publicIpv6 + QLatin1Char(')');
    }
    if (!publicIpStaleText.isEmpty()) {
        text += QLatin1Char('\n') + tr("(last known - %1)").arg(publicIpStaleText);
    }
    publicValueLabel->setText(text);
    updateSystemTray();
}

void MainWindow::updateLocalIpDisplay()
{
    if (localIpv4.isEmpty() && localIpv6.isEmpty()) {
        localValueLabel->setText(tr("Not found"));
        return;
    }

    QString text = !localIpv4.isEmpty() ? localIpv4 : localIpv6;
    if (!localIpv4.isEmpty() && !localIpv6.isEmpty()) {
        text += QStringLiteral(" | ") + localIpv6;
    }
    localValueLabel->setText(text);
}

void MainWindow::refreshIps()
{
    NetUtils::detectLocalAddresses(&localIpv4, &localIpv6);
    updateLocalIpDisplay();
    hostnameLabel->setText(tr("Hostname: %1").arg(displayHostname()));
    macLabel->setText(tr("MAC: %1").arg(displayMacAddress()));
    updateStatusIndicator();
    fetchPublicIp();
    populateInterfaces();
    updateSystemTray();
}

void MainWindow::updateStatusIndicator()
{
    const bool online = NetUtils::isOnline();
    statusIndicator->setText(online ? QStringLiteral("\u25cf Online") : QStringLiteral("\u25cf Offline"));
    statusIndicator->setObjectName(online ? QStringLiteral("statusOnline") : QStringLiteral("statusOffline"));
    statusIndicator->style()->unpolish(statusIndicator);
    statusIndicator->style()->polish(statusIndicator);
}

void MainWindow::recordHistory()
{
    if (publicIpv4.isEmpty() && publicIpv6.isEmpty()) {
        return;
    }

    if (!historyList.isEmpty()) {
        IpHistoryEntry &front = historyList.first();
        if (front.ipv4 == publicIpv4 && front.ipv6 == publicIpv6) {
            return;
        }
        // The two replies for one refresh may arrive separately; merge them
        // into a single history entry instead of recording two half entries.
        if (front.ipv4 == publicIpv4 && front.ipv6.isEmpty() && !publicIpv6.isEmpty()) {
            front.ipv6 = publicIpv6;
            front.timestamp = QDateTime::currentDateTime();
            saveHistory();
            return;
        }
        if (front.ipv6 == publicIpv6 && front.ipv4.isEmpty() && !publicIpv4.isEmpty()) {
            front.ipv4 = publicIpv4;
            front.timestamp = QDateTime::currentDateTime();
            saveHistory();
            return;
        }
    }

    IpHistoryEntry entry;
    entry.timestamp = QDateTime::currentDateTime();
    entry.ipv4 = publicIpv4;
    entry.ipv6 = publicIpv6;
    historyList.prepend(entry);
    while (historyList.size() > kMaxHistoryEntries) {
        historyList.removeLast();
    }
    saveHistory();
}

void MainWindow::showHistory()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("IP History"));
    dialog.setMinimumSize(620, 360);

    QVBoxLayout *layout = new QVBoxLayout(&dialog);

    QLabel *hintLabel = new QLabel(tr("Public IP addresses recorded on this machine."), &dialog);
    hintLabel->setObjectName(QStringLiteral("subtitle"));
    hintLabel->setTextFormat(Qt::PlainText);
    layout->addWidget(hintLabel);

    QTableWidget *table = new QTableWidget(&dialog);
    table->setObjectName(QStringLiteral("interfacesTable"));
    table->setColumnCount(3);
    table->setHorizontalHeaderLabels({tr("Time"), tr("IPv4"), tr("IPv6")});
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->verticalHeader()->setVisible(false);
    table->setRowCount(historyList.size());
    for (int i = 0; i < historyList.size(); ++i) {
        const IpHistoryEntry &entry = historyList.at(i);
        table->setItem(i, 0, new QTableWidgetItem(entry.timestamp.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))));
        table->setItem(i, 1, new QTableWidgetItem(entry.ipv4.isEmpty() ? QStringLiteral("-") : entry.ipv4));
        table->setItem(i, 2, new QTableWidgetItem(entry.ipv6.isEmpty() ? QStringLiteral("-") : entry.ipv6));
    }
    table->setColumnWidth(0, 180);
    table->setColumnWidth(1, 160);
    table->horizontalHeader()->setStretchLastSection(true);
    table->setSortingEnabled(true);
    layout->addWidget(table);

    if (historyList.isEmpty()) {
        QLabel *emptyLabel = new QLabel(tr("No history recorded yet."), &dialog);
        emptyLabel->setObjectName(QStringLiteral("footnote"));
        emptyLabel->setAlignment(Qt::AlignCenter);
        layout->addWidget(emptyLabel);
    }

    QHBoxLayout *buttonRow = new QHBoxLayout();
    buttonRow->addStretch();
    QPushButton *closeBtn = new QPushButton(tr("Close"), &dialog);
    closeBtn->setFixedSize(90, 32);
    connect(closeBtn, &QPushButton::clicked, &dialog, &QDialog::accept);
    buttonRow->addWidget(closeBtn);
    layout->addLayout(buttonRow);

    dialog.exec();
}

void MainWindow::clearHistory()
{
    historyList.clear();
    saveHistory();
    toast(tr("IP history cleared"));
}

void MainWindow::copyToLocalClipboard()
{
    if (localIpv4.isEmpty() && localIpv6.isEmpty()) {
        toast(tr("No local IP to copy yet"));
        return;
    }
    const QString text = localValueLabel->text();
    QApplication::clipboard()->setText(text);
    toast(tr("Copied: %1").arg(text));
}

void MainWindow::copyPublicToClipboard()
{
    const QString text = !publicIpv4.isEmpty() ? publicIpv4 : publicIpv6;
    if (text.isEmpty()) {
        toast(tr("No public IP to copy yet"));
        return;
    }
    QApplication::clipboard()->setText(text);
    toast(tr("Copied: %1").arg(text));
}

void MainWindow::exportData()
{
    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"));

    QFileDialog dialog(this, tr("Export Network Report"));
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setNameFilters({tr("Text Files (*.txt)"), tr("JSON Files (*.json)")});
    dialog.setDefaultSuffix(QStringLiteral("txt"));
    dialog.selectFile(QDir::home().filePath(QStringLiteral("ipchecker-report-") + timestamp));
    connect(&dialog, &QFileDialog::filterSelected, &dialog, [&dialog](const QString &filter) {
        dialog.setDefaultSuffix(filter.contains(QStringLiteral("*.json")) ? QStringLiteral("json")
                                                                          : QStringLiteral("txt"));
    });
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    const QString fileName = dialog.selectedFiles().value(0);
    if (fileName.isEmpty()) {
        return;
    }
    const bool asJson = dialog.selectedNameFilter().contains(QStringLiteral("*.json"));

    QJsonArray interfaceArray;
    QString textInterfaces;
    for (int row = 0; row < interfacesTable->rowCount(); ++row) {
        auto cell = [this, row](int column) -> QString {
            const QTableWidgetItem *item = interfacesTable->item(row, column);
            return item ? item->text() : QString();
        };

        QJsonObject iface;
        iface.insert(QStringLiteral("name"), cell(0));
        iface.insert(QStringLiteral("addresses"), cell(1));
        iface.insert(QStringLiteral("mac"), cell(2));
        iface.insert(QStringLiteral("status"), cell(3));
        iface.insert(QStringLiteral("type"), cell(4));
        interfaceArray.append(iface);

        textInterfaces += cell(0) + QStringLiteral(": ") + cell(1) + QStringLiteral(" | ") + cell(2)
                        + QStringLiteral(" | ") + cell(3) + QStringLiteral(" | ") + cell(4)
                        + QLatin1Char('\n');
    }

    QJsonObject root;
    root.insert(QStringLiteral("exportedAt"), QDateTime::currentDateTime().toString(Qt::ISODate));
    root.insert(QStringLiteral("hostname"), displayHostname());
    root.insert(QStringLiteral("macAddresses"), displayMacAddress());
    root.insert(QStringLiteral("localIpv4"), localIpv4);
    root.insert(QStringLiteral("localIpv6"), localIpv6);
    root.insert(QStringLiteral("publicIpv4"), publicIpv4);
    root.insert(QStringLiteral("publicIpv6"), publicIpv6);
    root.insert(QStringLiteral("location"), geoLocationLabel->text());
    root.insert(QStringLiteral("interfaces"), interfaceArray);

    QString textContent = QStringLiteral("=== IP Inspector Pro - Export ===\n\n");
    textContent += QStringLiteral("Export Time: ")
                 + QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))
                 + QStringLiteral("\n\n");
    textContent += QStringLiteral("--- Network Info ---\n");
    textContent += QStringLiteral("Hostname: ") + displayHostname() + QLatin1Char('\n');
    textContent += QStringLiteral("MAC: ") + displayMacAddress() + QLatin1Char('\n');
    textContent += QStringLiteral("Local IPv4: ")
                 + (localIpv4.isEmpty() ? QStringLiteral("Not found") : localIpv4) + QLatin1Char('\n');
    textContent += QStringLiteral("Local IPv6: ")
                 + (localIpv6.isEmpty() ? QStringLiteral("Not found") : localIpv6) + QLatin1Char('\n');
    textContent += QStringLiteral("Public IPv4: ")
                 + (publicIpv4.isEmpty() ? QStringLiteral("Not found") : publicIpv4) + QLatin1Char('\n');
    textContent += QStringLiteral("Public IPv6: ")
                 + (publicIpv6.isEmpty() ? QStringLiteral("Not found") : publicIpv6) + QLatin1Char('\n');
    textContent += QStringLiteral("Location: ") + geoLocationLabel->text() + QStringLiteral("\n\n");
    textContent += QStringLiteral("--- Network Interfaces ---\n");
    textContent += textInterfaces;

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        toast(tr("Could not save file: %1").arg(fileName), 5000);
        return;
    }
    const QByteArray payload = asJson ? QJsonDocument(root).toJson(QJsonDocument::Indented)
                                      : textContent.toUtf8();
    file.write(payload);
    file.close();
    toast(tr("Exported to %1").arg(fileName), 5000);
}

void MainWindow::toggleAutoRefresh()
{
    autoRefreshEnabled = !autoRefreshEnabled;
    if (autoRefreshEnabled) {
        autoRefreshTimer->start();
        autoRefreshButton->setText(QStringLiteral("Auto: ON"));
        toast(tr("Auto-refresh enabled every %1").arg(intervalCombo->currentText()));
    } else {
        autoRefreshTimer->stop();
        autoRefreshButton->setText(QStringLiteral("Auto: OFF"));
        toast(tr("Auto-refresh disabled"));
    }
    saveSettings();
}

void MainWindow::onRefreshIntervalChanged(int index)
{
    if (index < 0 || index >= NetUtils::refreshIntervalCount()) {
        return;
    }
    refreshIntervalMs = NetUtils::refreshIntervalMsAt(index);
    if (autoRefreshTimer) {
        autoRefreshTimer->setInterval(refreshIntervalMs);
        if (autoRefreshTimer->isActive()) {
            autoRefreshTimer->start();
        }
    }
    saveSettings();
}

void MainWindow::onThemeToggle()
{
    darkTheme = !darkTheme;
    if (darkTheme) {
        applyDarkTheme();
        themeButton->setText(QStringLiteral("\u263d"));
    } else {
        applyLightTheme();
        themeButton->setText(QStringLiteral("\u2600"));
    }

    updateStatusIndicator();
    saveSettings();
}

void MainWindow::applyLightTheme()
{
    setStyleSheet(loadTextResource(QStringLiteral(":/themes/light.qss")));
}

void MainWindow::applyDarkTheme()
{
    setStyleSheet(loadTextResource(QStringLiteral(":/themes/dark.qss")));
}

void MainWindow::updateSystemTray()
{
    if (!trayIcon) {
        return;
    }
    QString tip = QStringLiteral("IP Inspector Pro\nLocal: ") + localValueLabel->text();
    const QString publicIp = !publicIpv4.isEmpty() ? publicIpv4 : publicIpv6;
    if (!publicIp.isEmpty()) {
        tip += QStringLiteral("\nPublic: ") + publicIp;
    }
    trayIcon->setToolTip(tip);
}

void MainWindow::toast(const QString &message, int timeoutMs)
{
    statusBar()->showMessage(message, timeoutMs);
}

void MainWindow::onTrayActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason != QSystemTrayIcon::Trigger && reason != QSystemTrayIcon::DoubleClick) {
        return;
    }

    // Some platforms emit Trigger and then DoubleClick for one double click;
    // ignore the second event so a single gesture produces exactly one toggle.
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (lastTrayToggleTimestamp != 0 && now - lastTrayToggleTimestamp < 400) {
        return;
    }
    lastTrayToggleTimestamp = now;

    // Toggle: click hides a visible window to the tray, click shows it again.
    if (isVisible() && !isMinimized()) {
        hide();
    } else {
        showWindow();
    }
}

void MainWindow::showWindow()
{
    showNormal();
    raise();
    activateWindow();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    const bool trayUsable = QSystemTrayIcon::isSystemTrayAvailable() && trayIcon && trayIcon->isVisible();
    if (!trayUsable) {
        event->accept();
        return;
    }

    event->ignore();
    hide();
    if (!trayCloseNotified && trayIcon->supportsMessages()) {
        trayCloseNotified = true;
        trayIcon->showMessage(QStringLiteral("IP Inspector Pro"),
                              tr("Still running in the system tray. Right-click the tray icon to quit."),
                              QSystemTrayIcon::Information, 3000);
    }
}

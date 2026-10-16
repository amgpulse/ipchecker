#include "mainwindow.h"

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
#include <QUdpSocket>
#include <QUrl>
#include <QVBoxLayout>

namespace {

const int kIntervalMs[] = {10000, 30000, 60000, 300000, 900000};
const char *const kIntervalLabels[] = {"10s", "30s", "1m", "5m", "15m"};

int intervalIndexForMs(int ms)
{
    const int count = int(sizeof(kIntervalMs) / sizeof(kIntervalMs[0]));
    for (int i = 0; i < count; ++i) {
        if (kIntervalMs[i] == ms) {
            return i;
        }
    }
    return 2;
}

bool isVirtualInterface(const QString &name)
{
    static const char *const prefixes[] = {
        "docker", "br-", "veth", "virbr", "vnet", "tun", "tap", "wg", "zt", "tailscale", "vmnet", "vboxnet",
    };
    for (const char *prefix : prefixes) {
        if (name.startsWith(QString::fromLatin1(prefix), Qt::CaseInsensitive)) {
            return true;
        }
    }
    return false;
}

// Returns a canonical IP address parsed from an ipify-style {"ip": "..."} payload,
// or an empty string when the payload is not valid JSON / not a valid IP address.
QString parsePublicIpJson(const QByteArray &payload)
{
    QJsonParseError parseError{};
    const QJsonDocument doc = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        return {};
    }
    const QString ip = doc.object().value(QStringLiteral("ip")).toString().trimmed();
    QHostAddress address;
    if (!address.setAddress(ip)) {
        return {};
    }
    return address.toString();
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

    const int defaultIndex = intervalIndexForMs(refreshIntervalMs);
    const int intervalIndex = settings.value(QStringLiteral("refreshIntervalIndex"), defaultIndex).toInt();
    const int count = int(sizeof(kIntervalMs) / sizeof(kIntervalMs[0]));
    refreshIntervalMs = kIntervalMs[qBound(0, intervalIndex, count - 1)];

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

void MainWindow::saveSettings()
{
    QSettings settings(QStringLiteral("IPInspector"), QStringLiteral("Main"));
    settings.setValue(QStringLiteral("darkTheme"), darkTheme);
    settings.setValue(QStringLiteral("autoRefresh"), autoRefreshEnabled);
    settings.setValue(QStringLiteral("refreshIntervalIndex"), intervalIndexForMs(refreshIntervalMs));
    settings.setValue(QStringLiteral("geometry"), saveGeometry());
    saveHistory();
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
    for (const char *label : kIntervalLabels) {
        intervalCombo->addItem(QString::fromLatin1(label));
    }
    intervalCombo->setCurrentIndex(intervalIndexForMs(refreshIntervalMs));
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

    populateInterfaces();
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

void MainWindow::detectLocalIps()
{
    localIpv4.clear();
    localIpv6.clear();

    // Ask the OS which source address it would use to reach the internet.
    // A UDP "connect" never sends a packet; it only consults the routing table.
    QUdpSocket probe;
    probe.connectToHost(QHostAddress(QStringLiteral("1.1.1.1")), 53);
    if (probe.state() == QAbstractSocket::ConnectedState) {
        const QHostAddress routed = probe.localAddress();
        if (routed.protocol() == QAbstractSocket::IPv4Protocol && !routed.isLoopback()) {
            localIpv4 = routed.toString();
        } else if (routed.protocol() == QAbstractSocket::IPv6Protocol && !routed.isLoopback()
                   && !routed.isLinkLocal()) {
            localIpv6 = routed.toString();
        }
    }

    const QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface &iface : interfaces) {
        if (!iface.isValid() || (iface.flags() & QNetworkInterface::IsLoopBack)) {
            continue;
        }
        if (isVirtualInterface(iface.name())) {
            continue;
        }
        for (const QNetworkAddressEntry &entry : iface.addressEntries()) {
            const QHostAddress ip = entry.ip();
            if (ip.isLoopback() || ip.isLinkLocal() || ip.isMulticast()) {
                continue;
            }
            if (ip.protocol() == QAbstractSocket::IPv4Protocol && localIpv4.isEmpty()) {
                localIpv4 = ip.toString();
            } else if (ip.protocol() == QAbstractSocket::IPv6Protocol && localIpv6.isEmpty()) {
                localIpv6 = ip.toString();
            }
        }
    }
}

QString MainWindow::getMacAddress() const
{
    QStringList macs;
    const QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface &iface : interfaces) {
        if (!iface.isValid() || (iface.flags() & QNetworkInterface::IsLoopBack)) {
            continue;
        }
        if (isVirtualInterface(iface.name())) {
            continue;
        }
        const QString mac = iface.hardwareAddress();
        if (!mac.isEmpty() && mac != QStringLiteral("00:00:00:00:00:00")) {
            macs << mac;
        }
    }
    return macs.isEmpty() ? QStringLiteral("N/A") : macs.join(QStringLiteral(", "));
}

QString MainWindow::getHostname() const
{
    const QString hostname = QHostInfo::localHostName();
    return hostname.isEmpty() ? QStringLiteral("Unknown") : hostname;
}

bool MainWindow::isOnline() const
{
    // A successful UDP "connect" means the OS found a route (no packet is sent).
    QUdpSocket probe;
    probe.connectToHost(QHostAddress(QStringLiteral("1.1.1.1")), 53);
    if (probe.state() == QAbstractSocket::ConnectedState) {
        return true;
    }

    // Fallback for unusual setups: any non-virtual interface with a global address.
    const QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface &iface : interfaces) {
        if (!iface.isValid() || (iface.flags() & QNetworkInterface::IsLoopBack)) {
            continue;
        }
        if (!(iface.flags() & QNetworkInterface::IsUp) || !(iface.flags() & QNetworkInterface::IsRunning)) {
            continue;
        }
        if (isVirtualInterface(iface.name())) {
            continue;
        }
        for (const QNetworkAddressEntry &entry : iface.addressEntries()) {
            const QHostAddress ip = entry.ip();
            if (!ip.isLoopback() && !ip.isLinkLocal() && !ip.isMulticast()) {
                return true;
            }
        }
    }
    return false;
}

void MainWindow::fetchPublicIp()
{
    const int requestId = ++publicIpRequestId;
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
    const QString ip = reply->error() == QNetworkReply::NoError ? parsePublicIpJson(reply->readAll())
                                                                : QString();
    if (!ip.isEmpty()) {
        publicIpv4 = ip;
        publicIpErrorText.clear();
        updatePublicIpDisplay();
        recordHistory();
        fetchGeoInfo(ip);
    } else {
        publicIpErrorText = replyErrorText(reply);
        updatePublicIpDisplay();
    }
}

void MainWindow::onFetchIpv6Finished(QNetworkReply *reply)
{
    const QString ip = reply->error() == QNetworkReply::NoError ? parsePublicIpJson(reply->readAll())
                                                                : QString();
    if (!ip.isEmpty()) {
        publicIpv6 = ip;
        publicIpErrorText.clear();
        updatePublicIpDisplay();
        recordHistory();
        if (publicIpv4.isEmpty()) {
            fetchGeoInfo(ip);
        }
    } else {
        publicIpErrorText = replyErrorText(reply);
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

    QJsonParseError parseError{};
    const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        geoLocationLabel->setText(tr("Location unavailable"));
        return;
    }

    const QJsonObject obj = doc.object();
    if (obj.value(QStringLiteral("error")).toBool()) {
        const QString reason = obj.value(QStringLiteral("reason")).toString();
        geoLocationLabel->setText(reason.isEmpty() ? tr("Location unavailable")
                                                   : tr("Lookup failed: %1").arg(reason));
        return;
    }

    const QString city = obj.value(QStringLiteral("city")).toString();
    const QString region = obj.value(QStringLiteral("region")).toString();
    const QString country = obj.value(QStringLiteral("country_name")).toString();
    const QString org = obj.value(QStringLiteral("org")).toString();

    QStringList parts;
    if (!city.isEmpty()) {
        parts << city;
    }
    if (!region.isEmpty() && region != city) {
        parts << region;
    }
    if (!country.isEmpty()) {
        parts << country;
    }

    QString location = parts.join(QStringLiteral(", "));
    if (!org.isEmpty()) {
        location += QStringLiteral(" (") + org + QStringLiteral(")");
    }

    if (location.isEmpty()) {
        geoLocationLabel->setText(tr("Unknown"));
        return;
    }

    geoCache.insert(ip, location);
    geoLocationLabel->setText(location);
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
    detectLocalIps();
    updateLocalIpDisplay();
    hostnameLabel->setText(tr("Hostname: %1").arg(getHostname()));
    macLabel->setText(tr("MAC: %1").arg(getMacAddress()));
    updateStatusIndicator();
    fetchPublicIp();
    populateInterfaces();
    updateSystemTray();
}

void MainWindow::updateStatusIndicator()
{
    const bool online = isOnline();
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
    root.insert(QStringLiteral("hostname"), getHostname());
    root.insert(QStringLiteral("macAddresses"), getMacAddress());
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
    textContent += QStringLiteral("Hostname: ") + getHostname() + QLatin1Char('\n');
    textContent += QStringLiteral("MAC: ") + getMacAddress() + QLatin1Char('\n');
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
    const int count = int(sizeof(kIntervalMs) / sizeof(kIntervalMs[0]));
    if (index < 0 || index >= count) {
        return;
    }
    refreshIntervalMs = kIntervalMs[index];
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
    setStyleSheet(
        "QMainWindow, #centralWidget { background-color: #ecf0f1; }"
        "QWidget { color: #2c3e50; }"
        "#titleLabel { color: #2c3e50; }"
        "#sectionTitle { color: #2c3e50; padding: 5px; }"
        "#subtitle { color: #6c757d; }"
        "#valueText { color: #2c3e50; }"
        "#footnote { color: #95a5a6; }"
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
        "#intervalCombo { background-color: #ffffff; color: #2c3e50; border: 1px solid #dee2e6; border-radius: 5px; padding: 4px 8px; }"
        "#intervalCombo:hover { border-color: #3498db; }"
        "#intervalCombo::drop-down { border: none; width: 18px; }"
        "#intervalCombo QAbstractItemView { background-color: #ffffff; color: #2c3e50; border: 1px solid #dee2e6; selection-background-color: #3498db; selection-color: #ffffff; }"
        "#interfacesTable { background-color: #ffffff; alternate-background-color: #f8f9fa; border: 1px solid #dee2e6; border-radius: 5px; gridline-color: #dee2e6; }"
        "#interfacesTable::item { padding: 5px; color: #2c3e50; }"
        "#interfacesTable::item:selected { background-color: #3498db; color: white; }"
        "QHeaderView::section { background-color: #f8f9fa; color: #2c3e50; padding: 6px; border: none; border-bottom: 1px solid #dee2e6; font-weight: bold; }"
        "QScrollBar:vertical { background: #f8f9fa; width: 10px; border: none; }"
        "QScrollBar::handle:vertical { background: #dee2e6; border-radius: 5px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
        "QDialog { background-color: #ecf0f1; }"
        "QMenu { background-color: #ffffff; color: #2c3e50; border: 1px solid #dee2e6; }"
        "QMenu::item:selected { background-color: #3498db; color: #ffffff; }"
        "QStatusBar { background: transparent; color: #6c757d; }"
        "QStatusBar::item { border: none; }"
        "QFileDialog { background-color: #ecf0f1; color: #2c3e50; }"
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
        "#footnote { color: #585b70; }"
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
        "#intervalCombo { background-color: #313244; color: #cdd6f4; border: 1px solid #45475a; border-radius: 5px; padding: 4px 8px; }"
        "#intervalCombo:hover { border-color: #89b4fa; }"
        "#intervalCombo::drop-down { border: none; width: 18px; }"
        "#intervalCombo QAbstractItemView { background-color: #313244; color: #cdd6f4; border: 1px solid #45475a; selection-background-color: #89b4fa; selection-color: #1e1e2e; }"
        "#interfacesTable { background-color: #1e1e2e; alternate-background-color: #313244; border: 1px solid #45475a; border-radius: 5px; gridline-color: #45475a; }"
        "#interfacesTable::item { padding: 5px; color: #cdd6f4; }"
        "#interfacesTable::item:selected { background-color: #89b4fa; color: #1e1e2e; }"
        "QHeaderView::section { background-color: #313244; color: #cdd6f4; padding: 6px; border: none; border-bottom: 1px solid #45475a; font-weight: bold; }"
        "QScrollBar:vertical { background: #1e1e2e; width: 10px; border: none; }"
        "QScrollBar::handle:vertical { background: #45475a; border-radius: 5px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
        "QDialog { background-color: #1e1e2e; }"
        "QMenu { background-color: #313244; color: #cdd6f4; border: 1px solid #45475a; }"
        "QMenu::item:selected { background-color: #45475a; }"
        "QMessageBox { background-color: #1e1e2e; }"
        "QMessageBox QLabel { color: #cdd6f4; }"
        "QStatusBar { background: transparent; color: #7f849c; }"
        "QStatusBar::item { border: none; }"
        "QFileDialog { background-color: #1e1e2e; color: #cdd6f4; }"
    );
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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QCloseEvent>
#include <QComboBox>
#include <QDateTime>
#include <QHash>
#include <QHostInfo>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QMainWindow>
#include <QMenu>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPushButton>
#include <QString>
#include <QStringList>
#include <QStyle>
#include <QSystemTrayIcon>
#include <QTableWidget>
#include <QTimer>

struct IpHistoryEntry {
    QDateTime timestamp;
    QString ipv4;
    QString ipv6;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

public slots:
    void showWindow();

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onFetchPublicIpFinished(QNetworkReply *reply);
    void onFetchIpv6Finished(QNetworkReply *reply);
    void onFetchGeoInfoFinished(QNetworkReply *reply, const QString &ip);
    void refreshIps();
    void copyToLocalClipboard();
    void copyPublicToClipboard();
    void exportData();
    void toggleAutoRefresh();
    void onThemeToggle();
    void onRefreshIntervalChanged(int index);
    void onTrayActivated(QSystemTrayIcon::ActivationReason reason);
    void showHistory();
    void clearHistory();
    void filterInterfaces();

private:
    void setupUi();
    void applyLightTheme();
    void applyDarkTheme();
    void loadSettings();
    void saveSettings();
    void populateInterfaces();
    void detectLocalIps();
    QString getMacAddress() const;
    QString getHostname() const;
    bool isOnline() const;
    void fetchPublicIp();
    void fetchGeoInfo(const QString &ip);
    void updatePublicIpDisplay();
    void updateLocalIpDisplay();
    void updateStatusIndicator();
    void updateSystemTray();
    void recordHistory();
    void saveHistory();
    void toast(const QString &message, int timeoutMs = 3000);

    QNetworkAccessManager *networkManager = nullptr;
    QLabel *localValueLabel = nullptr;
    QLabel *publicValueLabel = nullptr;
    QLabel *geoLocationLabel = nullptr;
    QLabel *statusIndicator = nullptr;
    QLabel *macLabel = nullptr;
    QLabel *hostnameLabel = nullptr;
    QTableWidget *interfacesTable = nullptr;
    QLineEdit *searchBox = nullptr;
    QSystemTrayIcon *trayIcon = nullptr;
    QTimer *autoRefreshTimer = nullptr;
    QPushButton *themeButton = nullptr;
    QPushButton *autoRefreshButton = nullptr;
    QComboBox *intervalCombo = nullptr;

    bool darkTheme;
    bool autoRefreshEnabled;
    bool trayCloseNotified = false;

    QString localIpv4;
    QString localIpv6;
    QString publicIpv4;
    QString publicIpv6;
    QString publicIpErrorText = QStringLiteral("Loading...");

    QList<IpHistoryEntry> historyList;
    QHash<QString, QString> geoCache;

    int publicIpRequestId = 0;
    int geoRequestId = 0;
    int refreshIntervalMs = 60000;

    static constexpr int kNetworkTimeoutMs = 8000;
    static constexpr int kMaxHistoryEntries = 50;
};

#endif // MAINWINDOW_H

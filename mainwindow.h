#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QLabel>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QTimer>
#include <QTableWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QHostInfo>
#include <QStyle>
#include <functional>

struct IpHistoryEntry {
    QString ip;
    QString location;
    QDateTime timestamp;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onFetchPublicIpFinished(QNetworkReply* reply);
    void onFetchGeoInfoFinished(QNetworkReply* reply);
    void refreshIps();
    void copyToLocalClipboard();
    void copyPublicToClipboard();
    void exportData();
    void toggleAutoRefresh();
    void onThemeToggle();
    void showContextMenu(const QPoint& pos);
    void onTrayActivated(QSystemTrayIcon::ActivationReason reason);
    void showWindow();
    void clearHistory();
    void filterInterfaces();

private:
    QString getLocalIp() const;
    QList<QPair<QString, QString>> getAllLocalIps() const;
    QString getMacAddress() const;
    QString getHostname() const;
    bool isOnline() const;
    void fetchPublicIp();
    void fetchGeoInfo(const QString& ip);
    void setupUi();
    void applyLightTheme();
    void applyDarkTheme();
    void updateSystemTray();
    void populateHistory();
    void loadSettings();
    void saveSettings();
    void populateInterfaces();
    void setTimeout(std::function<void()> func, int ms);
    QString extractJsonString(const QString &json, const QString &key);

    QNetworkAccessManager *networkManager;
    QLabel *localValueLabel;
    QLabel *publicValueLabel;
    QLabel *geoLocationLabel;
    QLabel *statusIndicator;
    QLabel *macLabel;
    QLabel *hostnameLabel;
    QTableWidget *interfacesTable;
    QLineEdit *searchBox;
    QSystemTrayIcon *trayIcon;
    QTimer *autoRefreshTimer;
    QPushButton *themeButton;
    QPushButton *autoRefreshButton;
    bool darkTheme;
    bool autoRefreshEnabled;
    QStringList historyList;
};

#endif // MAINWINDOW_H

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
    void onTrayActivated(QSystemTrayIcon::ActivationReason reason);
    void showWindow();
    void filterInterfaces();

private:
    QString getLocalIp() const;
    QString getMacAddress() const;
    QString getHostname() const;
    bool isOnline() const;
    void fetchPublicIp();
    void fetchGeoInfo(const QString& ip);
    void setupUi();
    void applyLightTheme();
    void applyDarkTheme();
    void updateSystemTray();
    void loadSettings();
    void saveSettings();
    void populateInterfaces();

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

    static constexpr int kNetworkTimeoutMs = 8000;
};

#endif // MAINWINDOW_H

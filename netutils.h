#ifndef NETUTILS_H
#define NETUTILS_H

#include <QByteArray>
#include <QString>

class QNetworkInterface;

namespace NetUtils {

int refreshIntervalCount();
int refreshIntervalMsAt(int index);
QString refreshIntervalLabelAt(int index);
int intervalIndexForMs(int ms);

QString parsePublicIpJson(const QByteArray &payload);

bool isVirtualInterfaceName(const QString &name);
bool isVirtualInterface(const QNetworkInterface &iface);

struct GeoInfo {
    bool parsed = false;
    QString location;
    QString errorReason;
};

GeoInfo parseGeoInfo(const QByteArray &payload);

bool detectLocalAddresses(QString *ipv4, QString *ipv6);
bool isOnline();
QString macAddress();
QString hostname();

} // namespace NetUtils

#endif // NETUTILS_H

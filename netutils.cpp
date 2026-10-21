#include "netutils.h"

#include <QHostAddress>
#include <QHostInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QNetworkInterface>
#include <QStringList>
#include <QUdpSocket>

namespace {

constexpr int kIntervalMs[] = {10000, 30000, 60000, 300000, 900000};
constexpr const char *kIntervalLabels[] = {"10s", "30s", "1m", "5m", "15m"};
constexpr int kIntervalCount = int(sizeof(kIntervalMs) / sizeof(kIntervalMs[0]));

bool isUsableAddress(const QHostAddress &ip)
{
    return !ip.isNull() && !ip.isLoopback() && !ip.isLinkLocal() && !ip.isMulticast();
}

bool hasDefaultRoute()
{
    // A UDP "connect" never sends a packet; it only consults the routing table.
    QUdpSocket probe;
    probe.connectToHost(QHostAddress(QStringLiteral("1.1.1.1")), 53);
    return probe.state() == QAbstractSocket::ConnectedState;
}

} // namespace

namespace NetUtils {

int refreshIntervalCount()
{
    return kIntervalCount;
}

int refreshIntervalMsAt(int index)
{
    return (index >= 0 && index < kIntervalCount) ? kIntervalMs[index] : kIntervalMs[2];
}

QString refreshIntervalLabelAt(int index)
{
    return (index >= 0 && index < kIntervalCount) ? QString::fromLatin1(kIntervalLabels[index])
                                                  : QString::fromLatin1(kIntervalLabels[2]);
}

int intervalIndexForMs(int ms)
{
    for (int i = 0; i < kIntervalCount; ++i) {
        if (kIntervalMs[i] == ms) {
            return i;
        }
    }
    return 2;
}

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

bool isVirtualInterfaceName(const QString &name)
{
    static const char *const prefixes[] = {
        "docker", "br-", "veth", "virbr", "vnet", "tun", "tap", "wg", "zt",
        "tailscale", "vmnet", "vboxnet",
    };
    for (const char *prefix : prefixes) {
        if (name.startsWith(QString::fromLatin1(prefix), Qt::CaseInsensitive)) {
            return true;
        }
    }
    return false;
}

bool isVirtualInterface(const QNetworkInterface &iface)
{
    return isVirtualInterfaceName(iface.name()) || isVirtualInterfaceName(iface.humanReadableName());
}

GeoInfo parseGeoInfo(const QByteArray &payload)
{
    GeoInfo info;

    QJsonParseError parseError{};
    const QJsonDocument doc = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        return info;
    }
    info.parsed = true;

    const QJsonObject obj = doc.object();
    if (obj.value(QStringLiteral("error")).toBool()) {
        info.errorReason = obj.value(QStringLiteral("reason")).toString();
        return info;
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

    info.location = parts.join(QStringLiteral(", "));
    if (!org.isEmpty()) {
        if (!info.location.isEmpty()) {
            info.location += QLatin1Char(' ');
        }
        info.location += QLatin1Char('(') + org + QLatin1Char(')');
    }
    return info;
}

bool detectLocalAddresses(QString *ipv4, QString *ipv6)
{
    *ipv4 = QString();
    *ipv6 = QString();

    // Ask the OS which source address it would use to reach the internet.
    QUdpSocket probe;
    probe.connectToHost(QHostAddress(QStringLiteral("1.1.1.1")), 53);
    if (probe.state() == QAbstractSocket::ConnectedState) {
        const QHostAddress routed = probe.localAddress();
        if (routed.protocol() == QAbstractSocket::IPv4Protocol && isUsableAddress(routed)) {
            *ipv4 = routed.toString();
        } else if (routed.protocol() == QAbstractSocket::IPv6Protocol && isUsableAddress(routed)) {
            *ipv6 = routed.toString();
        }
    }

    const QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface &iface : interfaces) {
        if (!iface.isValid() || (iface.flags() & QNetworkInterface::IsLoopBack)) {
            continue;
        }
        if (isVirtualInterface(iface)) {
            continue;
        }
        for (const QNetworkAddressEntry &entry : iface.addressEntries()) {
            const QHostAddress ip = entry.ip();
            if (!isUsableAddress(ip)) {
                continue;
            }
            if (ip.protocol() == QAbstractSocket::IPv4Protocol && ipv4->isEmpty()) {
                *ipv4 = ip.toString();
            } else if (ip.protocol() == QAbstractSocket::IPv6Protocol && ipv6->isEmpty()) {
                *ipv6 = ip.toString();
            }
        }
    }

    return !ipv4->isEmpty() || !ipv6->isEmpty();
}

bool isOnline()
{
    if (hasDefaultRoute()) {
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
        if (isVirtualInterface(iface)) {
            continue;
        }
        for (const QNetworkAddressEntry &entry : iface.addressEntries()) {
            if (isUsableAddress(entry.ip())) {
                return true;
            }
        }
    }
    return false;
}

QString macAddress()
{
    QStringList macs;
    const QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface &iface : interfaces) {
        if (!iface.isValid() || (iface.flags() & QNetworkInterface::IsLoopBack)) {
            continue;
        }
        if (isVirtualInterface(iface)) {
            continue;
        }
        const QString mac = iface.hardwareAddress();
        if (!mac.isEmpty() && mac != QStringLiteral("00:00:00:00:00:00")) {
            macs << mac;
        }
    }
    return macs.join(QStringLiteral(", "));
}

QString hostname()
{
    return QHostInfo::localHostName();
}

} // namespace NetUtils

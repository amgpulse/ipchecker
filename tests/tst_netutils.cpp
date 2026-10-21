#include "netutils.h"

#include <QTest>

class TestNetUtils : public QObject
{
    Q_OBJECT

private slots:
    void parsePublicIp_data();
    void parsePublicIp();

    void virtualInterfaceName_data();
    void virtualInterfaceName();

    void refreshIntervals();
    void intervalIndexForMs();

    void parseGeoInfo_data();
    void parseGeoInfo();
};

void TestNetUtils::parsePublicIp_data()
{
    QTest::addColumn<QByteArray>("payload");
    QTest::addColumn<QString>("expected");

    QTest::newRow("ipv4") << QByteArray(R"({"ip":"203.0.113.7"})") << QStringLiteral("203.0.113.7");
    QTest::newRow("trimmed") << QByteArray(R"({"ip":" 198.51.100.9 "})")
                             << QStringLiteral("198.51.100.9");
    QTest::newRow("ipv6-canonical") << QByteArray(R"({"ip":"2001:0db8:0000:0000:0000:0000:0000:0001"})")
                                    << QStringLiteral("2001:db8::1");
    QTest::newRow("invalid-json") << QByteArray("not json") << QString();
    QTest::newRow("json-array") << QByteArray(R"(["203.0.113.7"])") << QString();
    QTest::newRow("missing-ip") << QByteArray(R"({"address":"203.0.113.7"})") << QString();
    QTest::newRow("invalid-ip") << QByteArray(R"({"ip":"999.1.1.1"})") << QString();
    QTest::newRow("empty") << QByteArray() << QString();
}

void TestNetUtils::parsePublicIp()
{
    QFETCH(QByteArray, payload);
    QFETCH(QString, expected);

    QCOMPARE(NetUtils::parsePublicIpJson(payload), expected);
}

void TestNetUtils::virtualInterfaceName_data()
{
    QTest::addColumn<QString>("name");
    QTest::addColumn<bool>("virtualIface");

    QTest::newRow("docker") << QStringLiteral("docker0") << true;
    QTest::newRow("bridge") << QStringLiteral("br-1a2b3c") << true;
    QTest::newRow("veth") << QStringLiteral("veth12ab") << true;
    QTest::newRow("virbr") << QStringLiteral("virbr0") << true;
    QTest::newRow("tun") << QStringLiteral("tun0") << true;
    QTest::newRow("tap") << QStringLiteral("tap0") << true;
    QTest::newRow("wireguard") << QStringLiteral("wg0") << true;
    QTest::newRow("zerotier") << QStringLiteral("zt123456") << true;
    QTest::newRow("tailscale") << QStringLiteral("tailscale0") << true;
    QTest::newRow("vmware") << QStringLiteral("vmnet8") << true;
    QTest::newRow("virtualbox") << QStringLiteral("vboxnet0") << true;
    QTest::newRow("case-insensitive") << QStringLiteral("Docker0") << true;
    QTest::newRow("ethernet") << QStringLiteral("eth0") << false;
    QTest::newRow("wifi") << QStringLiteral("wlan0") << false;
    QTest::newRow("predictable") << QStringLiteral("enp3s0") << false;
    QTest::newRow("empty") << QString() << false;
}

void TestNetUtils::virtualInterfaceName()
{
    QFETCH(QString, name);
    QFETCH(bool, virtualIface);

    QCOMPARE(NetUtils::isVirtualInterfaceName(name), virtualIface);
}

void TestNetUtils::refreshIntervals()
{
    QCOMPARE(NetUtils::refreshIntervalCount(), 5);
    QCOMPARE(NetUtils::refreshIntervalMsAt(0), 10000);
    QCOMPARE(NetUtils::refreshIntervalMsAt(2), 60000);
    QCOMPARE(NetUtils::refreshIntervalMsAt(4), 900000);
    QCOMPARE(NetUtils::refreshIntervalLabelAt(2), QStringLiteral("1m"));
    QCOMPARE(NetUtils::refreshIntervalLabelAt(-1), QStringLiteral("1m"));
    QCOMPARE(NetUtils::refreshIntervalMsAt(99), 60000);
}

void TestNetUtils::intervalIndexForMs()
{
    QCOMPARE(NetUtils::intervalIndexForMs(10000), 0);
    QCOMPARE(NetUtils::intervalIndexForMs(30000), 1);
    QCOMPARE(NetUtils::intervalIndexForMs(60000), 2);
    QCOMPARE(NetUtils::intervalIndexForMs(300000), 3);
    QCOMPARE(NetUtils::intervalIndexForMs(900000), 4);
    QCOMPARE(NetUtils::intervalIndexForMs(1234), 2);
}

void TestNetUtils::parseGeoInfo_data()
{
    QTest::addColumn<QByteArray>("payload");
    QTest::addColumn<bool>("parsed");
    QTest::addColumn<QString>("location");
    QTest::addColumn<QString>("errorReason");

    QTest::newRow("city-region-country")
        << QByteArray(R"({"city":"Munich","region":"Bavaria","country_name":"Germany"})")
        << true << QStringLiteral("Munich, Bavaria, Germany") << QString();
    QTest::newRow("region-equals-city")
        << QByteArray(R"({"city":"Tehran","region":"Tehran","country_name":"Iran"})")
        << true << QStringLiteral("Tehran, Iran") << QString();
    QTest::newRow("with-org")
        << QByteArray(R"({"city":"Paris","region":"Ile-de-France","country_name":"France","org":"AS1234 Example"})")
        << true << QStringLiteral("Paris, Ile-de-France, France (AS1234 Example)") << QString();
    QTest::newRow("org-only")
        << QByteArray(R"({"org":"AS1234 Example"})")
        << true << QStringLiteral("(AS1234 Example)") << QString();
    QTest::newRow("api-error")
        << QByteArray(R"({"error":true,"reason":"RateLimited"})")
        << true << QString() << QStringLiteral("RateLimited");
    QTest::newRow("invalid-json") << QByteArray("{oops") << false << QString() << QString();
    QTest::newRow("json-array") << QByteArray("[]") << false << QString() << QString();
    QTest::newRow("empty-object") << QByteArray("{}") << true << QString() << QString();
}

void TestNetUtils::parseGeoInfo()
{
    QFETCH(QByteArray, payload);
    QFETCH(bool, parsed);
    QFETCH(QString, location);
    QFETCH(QString, errorReason);

    const NetUtils::GeoInfo info = NetUtils::parseGeoInfo(payload);
    QCOMPARE(info.parsed, parsed);
    QCOMPARE(info.location, location);
    QCOMPARE(info.errorReason, errorReason);
}

QTEST_GUILESS_MAIN(TestNetUtils)

#include "tst_netutils.moc"

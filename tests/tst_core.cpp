#include "configgen.h"
#include "geoip.h"
#include "i18n.h"
#include "linkparser.h"
#include "masking.h"
#include "subscription.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>
#include <QTest>
#include <QtEndian>

using namespace Qt::StringLiterals;
using namespace rl;

namespace {
const QString kUuid = u"11111111-2222-4333-8444-555555555555"_s;
const QString kPbk = u"SFZ1bXpsVzBjOFJ4X2VYeTVnR0hNc2I3dGlLY2pUMms"_s;
const QString kSid = u"0123abcd"_s;

QString reality(const QString &extra = QString(), const QString &id = kUuid, const QString &hostport = u"example.com:443"_s)
{
    return u"vless://"_s + id + u'@' + hostport + u"?type=tcp&encryption=none&security=reality&pbk="_s + kPbk
         + u"&fp=chrome&sni=www.example.org&sid="_s + kSid + u"&spx=%2F&flow=xtls-rprx-vision"_s + extra
         + u"#%D0%A1%D0%B5%D1%80%D0%B2%D0%B5%D1%80%201"_s;
}

QString base(const QString &query)
{
    return u"vless://"_s + kUuid + u"@example.com:443?"_s + query + u"#n"_s;
}

}

class TestCore : public QObject {
    Q_OBJECT

private slots:
    void sampleLink()
    {
        const ParseResult r = parseLink(reality());
        QVERIFY2(!r.error, qPrintable(r.error ? r.error->text() : QString()));
        QCOMPARE(r.profile.name, u"Сервер 1"_s);
        QCOMPARE(r.profile.spx, u"/"_s);
        QCOMPARE(r.profile.uuid, kUuid);
        QCOMPARE(r.profile.port, 443);
        QCOMPARE(r.profile.sid, kSid);
        QCOMPARE(r.profile.badge(), u"VLESS · REALITY"_s);
        QVERIFY(proxyEntry(r.profile, u"p"_s)[u"reality-opts"_s][u"support-x25519mlkem768"_s].toBool());
        QVERIFY(!r.profile.nonStandardId());
    }

    // 3x-ui puts a random spx into nearly every link; it must not block the import.
    void spxPathAccepted()
    {
        const ParseResult r = parseLink(
            u"vless://"_s + kUuid + u"@203.0.113.7:443?encryption=none&flow=xtls-rprx-vision&fp=chrome&pbk="_s + kPbk
            + u"&security=reality&sid=41&sni=www.example.org&spx=%2Fa946e9d04d0603b&support-x25519mlkem768=true"
              "&type=tcp#UAE1-Office5"_s);
        QVERIFY2(!r.error, r.error ? qPrintable(r.error->text()) : "");
        QCOMPARE(r.profile.spx, u"/a946e9d04d0603b"_s);
        QVERIFY(r.profile.unknownParams.isEmpty());
        QVERIFY(!QJsonDocument(proxyEntry(r.profile, u"p"_s)).toJson().contains("a946e9d04d0603b"));
    }

    void nonStandardId()
    {
        const ParseResult r = parseLink(reality(QString(), u"not-a-uuid"_s));
        QVERIFY(!r.error);
        QVERIFY(r.profile.nonStandardId());
        const QJsonObject proxy = proxyEntry(r.profile, u"p"_s);
        QCOMPARE(proxy.value(u"uuid"_s).toString(), u"not-a-uuid"_s);
    }

    void visionUdp443PassesThrough()
    {
        QString link = reality();
        link.replace(u"flow=xtls-rprx-vision"_s, u"flow=xtls-rprx-vision-udp443"_s);
        const ParseResult r = parseLink(link);
        QVERIFY(!r.error);
        QCOMPARE(proxyEntry(r.profile, u"p"_s).value(u"flow"_s).toString(), u"xtls-rprx-vision-udp443"_s);
    }

    void rejected_data()
    {
        QTest::addColumn<QString>("link");
        QTest::addColumn<QString>("field");
        QTest::addColumn<QString>("message");

        QTest::newRow("port 0") << reality({}, kUuid, u"example.com:0"_s) << "port" << "";
        QTest::newRow("port 65536") << reality({}, kUuid, u"example.com:65536"_s) << "port" << "";
        QTest::newRow("port abc") << reality({}, kUuid, u"example.com:abc"_s) << "port" << "";
        QTest::newRow("empty id") << reality({}, QString()) << "id" << "";
        QTest::newRow("long non-uuid") << reality({}, QString(31, u'a')) << "id" << "";
        QTest::newRow("id control char") << reality({}, u"abc%01def"_s) << "id" << "";
        QTest::newRow("ipv6 without []") << reality({}, kUuid, u"2001:db8::1:443"_s) << "host" << "";
        QTest::newRow("reality no pbk") << base(u"security=reality&sni=a.com"_s) << "pbk" << "";
        QTest::newRow("reality empty pbk") << base(u"security=reality&sni=a.com&pbk="_s) << "pbk" << "";
        QTest::newRow("reality fp unsafe") << base(u"security=reality&pbk="_s + kPbk + u"&fp=unsafe"_s) << "fp" << "";
        QTest::newRow("sid abc") << reality().replace(kSid, u"abc"_s) << "sid" << "";
        QTest::newRow("sid 18") << reality().replace(kSid, QString(18, u'a')) << "sid" << "";
        QTest::newRow("sid zz") << reality().replace(kSid, u"zz"_s) << "sid" << "";
        QTest::newRow("encryption empty") << reality().replace(u"encryption=none"_s, u"encryption="_s) << "encryption" << "";
        QTest::newRow("flow direct") << reality().replace(u"flow=xtls-rprx-vision"_s, u"flow=xtls-rprx-direct"_s)
                                     << "flow" << "not supported by the mihomo core";
        QTest::newRow("repeated sni") << reality(u"&sni=b.example.org"_s) << "sni" << "repeated";
        QTest::newRow("grpc no serviceName") << base(u"type=grpc&security=tls"_s) << "serviceName" << "";
        QTest::newRow("ssr") << u"ssr://abc"_s << "scheme" << "Protocol is not supported";
        QTest::newRow("javascript") << u"javascript:alert(1)"_s << "scheme" << "";
        QTest::newRow("file") << u"file:///etc/passwd"_s << "scheme" << "";
        QTest::newRow("vmess") << u"vmess://eyJ2IjoyfQ=="_s << "scheme" << "Protocol is not supported";
        QTest::newRow("trojan") << u"trojan://pw@example.com:443"_s << "scheme" << "Protocol is not supported";
        QTest::newRow("pqv") << reality(u"&pqv=abc"_s) << "pqv" << "The pqv parameter is not supported by the core";
        QTest::newRow("spx control") << reality().replace(u"spx=%2F"_s, u"spx=%2F%07"_s) << "spx" << "control characters";
        QTest::newRow("fm") << reality(u"&fm=%7B%7D"_s) << "fm" << "not supported by the core";
        QTest::newRow("too long") << reality(u"&x="_s + QString(9000, u'a')) << "" << "8 KB";
        QTest::newRow("fragment control") << reality().replace(u"%201"_s, u"%07"_s) << "name" << "";
        QTest::newRow("kcp") << base(u"type=kcp"_s) << "type" << "not supported by the mihomo core";
        QTest::newRow("xhttp extra key") << base(u"type=xhttp&extra=%7B%22xmux%22%3A1%7D"_s) << "extra" << "";
    }

    void rejected()
    {
        QFETCH(QString, link);
        QFETCH(QString, field);
        QFETCH(QString, message);
        const ParseResult r = parseLink(link);
        QVERIFY2(r.error, "link must be rejected");
        QCOMPARE(r.error->field, field);
        QVERIFY2(r.error->text().contains(message), qPrintable(r.error->text()));
        QVERIFY(!r.error->text().contains(kUuid));
        QVERIFY(!r.error->text().contains(kPbk));
    }

    void russianTranslation()
    {
        QVERIFY(setLanguage(u"ru"_s));
        QVERIFY(parseLink(u"ssr://abc"_s).error->text().contains(u"Протокол не поддерживается"_s));
        QVERIFY(setLanguage(u"en"_s));
        QVERIFY(parseLink(u"ssr://abc"_s).error->text().contains(u"Protocol is not supported"_s));
        QVERIFY(!setLanguage(u"de"_s));
    }

    void defaults()
    {
        ParseResult r = parseLink(u"vless://"_s + kUuid + u"@example.com:443"_s);
        QVERIFY(!r.error);
        QCOMPARE(r.profile.network, u"tcp"_s);
        QCOMPARE(r.profile.encryption, u"none"_s);

        r = parseLink(base(u"type=ws"_s));
        QVERIFY(!r.error);
        QCOMPARE(r.profile.path, u"/"_s);

        r = parseLink(base(u"type=grpc&serviceName=svc"_s));
        QVERIFY(!r.error);
        QCOMPARE(r.profile.mode, u"gun"_s);

        r = parseLink(base(u"security=tls"_s));
        QVERIFY(!r.error);
        QCOMPARE(r.profile.sni, u"example.com"_s);
        QCOMPARE(r.profile.fp, u"chrome"_s);

        r = parseLink(base(u"security=reality&pbk="_s + kPbk));
        QVERIFY(!r.error);
        QCOMPARE(r.profile.fp, u"chrome"_s);
    }

    void unknownParamKeptByKeyOnly()
    {
        const ParseResult r = parseLink(reality(u"&foo=secretvalue"_s));
        QVERIFY(!r.error);
        QCOMPARE(r.unknownKeys, QStringList{u"foo"_s});
        QCOMPARE(r.profile.unknownParams.value(u"foo"_s).toString(), u"secretvalue"_s);
    }

    void ipv6AndIdn()
    {
        ParseResult r = parseLink(reality({}, kUuid, u"[2001:db8::1]:443"_s));
        QVERIFY(!r.error);
        QCOMPARE(r.profile.host, u"2001:db8::1"_s);
        r = parseLink(reality({}, kUuid, u"%D0%BF%D1%80%D0%B8%D0%BC%D0%B5%D1%80.%D1%80%D1%84:443"_s));
        QVERIFY(!r.error);
        QCOMPARE(r.profile.host, u"xn--e1afmkfd.xn--p1ai"_s);
    }

    void subscriptionBody()
    {
        QStringList lines;
        for (int i = 0; i < 8; ++i)
            lines << reality().replace(u"%201"_s, u"%20"_s + QString::number(i));
        lines.insert(3, u"vless://broken"_s);
        lines.insert(6, u"vmess://xyz"_s);
        const QByteArray plain = lines.join(u'\n').toUtf8();

        const ImportResult p = parseImport(plain);
        QCOMPARE(p.profiles.size(), 8);
        QCOMPARE(p.skipped.size(), 2);

        const ImportResult std64 = parseImport(plain.toBase64());
        const ImportResult url64 = parseImport(plain.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
        const ImportResult bom = parseImport("\xEF\xBB\xBF  " + plain.toBase64() + "\n");
        for (const ImportResult *r : {&std64, &url64, &bom}) {
            QCOMPARE(r->profiles.size(), 8);
            QCOMPARE(r->skipped.size(), 2);
            QCOMPARE(r->profiles.last().name, p.profiles.last().name);
        }
    }

    void importLimits()
    {
        QVERIFY(parseImport(QByteArray(kMaxBodyBytes + 1, 'a')).profiles.isEmpty());
    }

    void subscriptionHeaders()
    {
        SubscriptionInfo i = parseSubscriptionHeaders("upload=1; download=2; total=10; expire=1893456000000", "500",
                                                      "base64:" + QByteArray("<b>Мой VPN</b>").toBase64(),
                                                      "https://t.me/x", "");
        QCOMPARE(i.upload, 1);
        QCOMPARE(i.download, 2);
        QCOMPARE(i.total, 10);
        QCOMPARE(i.expire, 1893456000);
        QCOMPARE(i.updateIntervalHours, 168);
        QCOMPARE(i.title, u"<b>Мой VPN</b>"_s);
        QCOMPARE(parseSubscriptionHeaders("", "0", "", "", "").updateIntervalHours, 1);
        QCOMPARE(parseSubscriptionHeaders("", "", "", "", "").updateIntervalHours, 12);
    }

    void configProxyMode()
    {
        const Profile p = parseLink(reality()).profile;
        CoreSettings s;
        s.mode = u"proxy"_s;
        CoreRuntime rt{u"/run/superpupervpn/core/core.sock"_s, QString(), u"secret"_s, 34567, false, u"wlp4s0"_s};
        const QJsonObject c = buildCoreConfig(p, s, rt);
        checkCommonSecurity(c);
        QCOMPARE(c.value(u"interface-name"_s).toString(), u"wlp4s0"_s);
        const QJsonArray l = c.value(u"listeners"_s).toArray();
        QCOMPARE(l.size(), 1);
        QCOMPARE(l[0][u"listen"_s].toString(), u"127.0.0.1"_s);
        QCOMPARE(l[0][u"udp"_s].toBool(true), false);
        QCOMPARE(l[0][u"port"_s].toInt(), 34567);
        QVERIFY(!c.contains(u"tun"_s));
        maybeRunMihomo(c);
    }

    void configTunMode()
    {
        const Profile p = parseLink(reality()).profile;
        CoreSettings s;
        CoreRuntime rt{u"/run/superpupervpn/core/core.sock"_s, QString(), u"secret"_s, 0, true, u"wlp4s0"_s};
        const QJsonObject c = buildCoreConfig(p, s, rt);
        checkCommonSecurity(c);
        QVERIFY(!c.contains(u"listeners"_s));
        QVERIFY(!c.contains(u"interface-name"_s));
        const QJsonObject tun = c.value(u"tun"_s).toObject();
        QCOMPARE(tun.value(u"strict-route"_s).toBool(), true);
        QCOMPARE(tun.value(u"device"_s).toString(), u"superpupervpn"_s);
        QCOMPARE(c.value(u"dns"_s)[u"enhanced-mode"_s].toString(), u"fake-ip"_s);
        QVERIFY(!QJsonDocument(c).toJson().contains("spx"));
        QCOMPARE(c.value(u"rules"_s).toArray().last().toString(), u"MATCH,proxy"_s);
        maybeRunMihomo(c);
        maybeRunMihomo(buildTestConfig({p}, rt, QString()));
    }

    void settingsValidation()
    {
        CoreSettings s;
        QVERIFY(!validate(s));
        s.dohUrl = u"http://1.1.1.1/dns-query"_s;
        QVERIFY(validate(s));
        s = {};
        s.mode = u"socks"_s;
        QVERIFY(validate(s));
        s = {};
        s.tunMtu = 100;
        QVERIFY(validate(s));
    }

    void helperRejectsForeignKeys()
    {
        QJsonObject o = parseLink(reality()).profile.toJson();
        QVERIFY(!rejectUnknownKeys(o));
        QVERIFY(!validate(Profile::fromJson(o)));
        o.insert(u"rule-providers"_s, QJsonObject{{u"x"_s, QJsonObject{{u"path"_s, u"/etc/shadow"_s}}}});
        QVERIFY(rejectUnknownKeys(o));
        o.remove(u"rule-providers"_s);
        o[u"host"_s] = u"a.com\nrules: []"_s;
        QVERIFY(validate(Profile::fromJson(o)));
    }

    void masking()
    {
        const Profile p = parseLink(reality()).profile;
        Masker m;
        for (const QString &s : p.secrets())
            m.addSecret(s);
        m.addHost(p.host);
        const QString line = u"dial %1 uuid=%2 pbk=%3 sid=%4 sub=https://sub.example.net/api/sub/TOKEN123?x=1 other=%5"_s
                                 .arg(p.host, kUuid, kPbk, kSid, u"aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeee1a2b"_s);
        const QString out = m.mask(line);
        for (const QString &secret : {kUuid, kPbk, kSid, u"TOKEN123"_s, p.host, u"aaaaaaaa-bbbb"_s})
            QVERIFY2(!out.contains(secret), qPrintable(out));
        QVERIFY(out.contains(u"xxxxxxxx-…-5555"_s));
        QVERIFY(out.contains(u"xxxxxxxx-…-1a2b"_s));
        QVERIFY(out.contains(u"https://sub.example.net/***"_s));
        m.setMaskHosts(false);
        QVERIFY(m.mask(p.host).contains(p.host));
    }

    void subscriptionMergeKeepsIds()
    {
        QList<Profile> old;
        for (int i = 0; i < 3; ++i) {
            Profile p = parseLink(reality({}, kUuid, u"s%1.example.com:443"_s.arg(i))).profile;
            p.id = u"id%1"_s.arg(i);
            p.subscriptionId = u"sub"_s;
            p.pinned = i == 1;
            old << p;
        }
        QList<Profile> incoming = {old[2], old[1]};
        incoming[1].name = u"renamed"_s;
        for (Profile &p : incoming) {
            p.id.clear();
            p.pinned = false;
        }
        incoming << parseLink(reality({}, kUuid, u"new.example.com:443"_s)).profile;
        int n = 0;
        const QList<Profile> merged = mergeSubscription(old, incoming, u"sub"_s, [&] { return u"new%1"_s.arg(n++); });
        QCOMPARE(merged.size(), 3);
        QCOMPARE(merged[0].id, u"id2"_s);
        QCOMPARE(merged[1].id, u"id1"_s);
        QCOMPARE(merged[1].name, u"renamed"_s);
        QVERIFY(merged[1].pinned);
        QCOMPARE(merged[2].id, u"new0"_s);
        QCOMPARE(merged[2].subscriptionId, u"sub"_s);
    }

    void geoip()
    {
        QByteArray t = "GEO1";
        auto u32 = [&](quint32 v) { char b[4]; qToBigEndian(v, b); t.append(b, 4); };
        auto u64 = [&](quint64 v) { char b[8]; qToBigEndian(v, b); t.append(b, 8); };
        u32(3);
        u32(0x00000000); t += "ZZ";
        u32(0x01000000); t += "AU";
        u32(0x01000100); t += "CN";
        u32(2);
        u64(0); t += "ZZ";
        u64(0x20010db800000000ULL); t += "DE";
        const GeoIp geo(t);
        QCOMPARE(geo.country(QHostAddress(u"1.0.0.5"_s)), u"AU"_s);
        QCOMPARE(geo.country(QHostAddress(u"1.0.2.1"_s)), u"CN"_s);
        QCOMPARE(geo.country(QHostAddress(u"0.1.2.3"_s)), QString());
        QCOMPARE(geo.country(QHostAddress(u"::ffff:1.0.0.9"_s)), u"AU"_s);
        QCOMPARE(geo.country(QHostAddress(u"2001:db8::1"_s)), u"DE"_s);
        QCOMPARE(GeoIp(t.left(20)).country(QHostAddress(u"1.0.0.5"_s)), QString());
        QCOMPARE(flagEmoji(u"hk"_s), u"🇭🇰"_s);
        QCOMPARE(flagEmoji(u"1A"_s), QString());

        QFile f(QStringLiteral(SUPERPUPERVPN_GEO_TABLE));
        QVERIFY(f.open(QIODevice::ReadOnly));
        const GeoIp real(f.readAll());
        QCOMPARE(real.country(QHostAddress(u"77.88.8.8"_s)), u"RU"_s);
        QCOMPARE(real.country(QHostAddress(u"8.8.8.8"_s)), u"US"_s);
        QCOMPARE(real.country(QHostAddress(u"192.168.1.1"_s)), QString());
    }

    void zip()
    {
        const QByteArray z = makeZip({{u"a.txt"_s, "hello"}});
        QVERIFY(z.startsWith("PK\x03\x04"));
        QCOMPARE(qFromLittleEndian<quint32>(z.constData() + 14), 0x3610a686u);
        QVERIFY(z.contains("PK\x05\x06"));
    }

private:
    static void checkCommonSecurity(const QJsonObject &c)
    {
        QVERIFY(!c.contains(u"external-controller"_s));
        QVERIFY(!c.contains(u"external-controller-tls"_s));
        QVERIFY(!c.contains(u"mixed-port"_s) && !c.contains(u"socks-port"_s) && !c.contains(u"port"_s));
        QCOMPARE(c.value(u"allow-lan"_s).toBool(true), false);
        QCOMPARE(c.value(u"skip-auth-prefixes"_s).toArray().size(), 0);
        QVERIFY(c.value(u"skip-auth-prefixes"_s).isArray());
        const QByteArray json = QJsonDocument(c).toJson();
        QVERIFY(!json.contains("GEOSITE") && !json.contains("GEOIP") && !json.contains("geo") && !json.contains("nameserver-policy"));
        QVERIFY(!c.contains(u"rule-providers"_s) && !c.contains(u"proxy-providers"_s));
    }

    static void maybeRunMihomo(const QJsonObject &c)
    {
        const QString bin = qEnvironmentVariable("SUPERPUPERVPN_TEST_MIHOMO");
        if (bin.isEmpty())
            return;
        QTemporaryDir dir;
        QProcess p;
        p.start(bin, {u"-t"_s, u"-d"_s, dir.path(), u"-f"_s, u"-"_s});
        QVERIFY(p.waitForStarted());
        p.write(QJsonDocument(c).toJson(QJsonDocument::Compact));
        p.closeWriteChannel();
        QVERIFY(p.waitForFinished(60000));
        const QByteArray out = p.readAll();
        QVERIFY2(p.exitCode() == 0 && out.contains("test is successful"), out.right(400).constData());
        QCOMPARE(QDir(dir.path()).entryList(QDir::Files), QStringList());
    }
};

QTEST_GUILESS_MAIN(TestCore)
#include "tst_core.moc"

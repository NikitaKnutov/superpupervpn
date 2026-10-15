#include "theme_tokens.h"

#include <QColor>
#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QTest>

using namespace Qt::StringLiterals;

namespace {
double luminance(const QColor &c)
{
    auto lin = [](double v) { return v <= 0.03928 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4); };
    return 0.2126 * lin(c.redF()) + 0.7152 * lin(c.greenF()) + 0.0722 * lin(c.blueF());
}

double contrast(const QColor &a, const QColor &b)
{
    const double la = luminance(a), lb = luminance(b);
    return (std::max(la, lb) + 0.05) / (std::min(la, lb) + 0.05);
}

QColor token(const char *name, bool dark)
{
    for (const auto &t : rl::theme::kTokens)
        if (qstrcmp(t.name, name) == 0)
            return QColor(QString::fromLatin1(dark ? t.dark : t.light));
    return {};
}

}

class TestGui : public QObject {
    Q_OBJECT

private slots:
    void contrast_data()
    {
        QTest::addColumn<QString>("fg");
        QTest::addColumn<QString>("bg");
        QTest::addColumn<bool>("dark");
        for (bool dark : {false, true})
            for (const char *fg : rl::theme::kTextTokens)
                for (const char *bg : rl::theme::kBackgroundTokens)
                    QTest::addRow("%s/%s/%s", fg, bg, dark ? "dark" : "light") << QString::fromLatin1(fg) << QString::fromLatin1(bg) << dark;
    }

    void contrast()
    {
        QFETCH(QString, fg);
        QFETCH(QString, bg);
        QFETCH(bool, dark);
        const double ratio = ::contrast(token(qPrintable(fg), dark), token(qPrintable(bg), dark));
        QVERIFY2(ratio >= 4.5, qPrintable(u"%1 on %2: %3:1"_s.arg(fg, bg).arg(ratio, 0, 'f', 2)));
    }

    void buttonTextOnAccent()
    {
        for (bool dark : {false, true})
            QVERIFY(::contrast(token("accentText", dark), token("accent", dark)) >= 4.5);
    }

    void qmlStringsGoThroughQsTr()
    {
        static const QRegularExpression literal(u"\"([^\"\\\\]|\\\\.)*\""_s);
        QStringList offenders;
        const QDir dir(QStringLiteral(SUPERPUPERVPN_QML_DIR));
        const QStringList files = dir.entryList({u"*.qml"_s});
        QVERIFY(!files.isEmpty());
        for (const QString &name : files) {
            QFile f(dir.filePath(name));
            QVERIFY(f.open(QIODevice::ReadOnly));
            int n = 0;
            while (!f.atEnd()) {
                const QString line = QString::fromUtf8(f.readLine());
                ++n;
                if (line.trimmed().startsWith(u"//"))
                    continue;
                for (auto it = literal.globalMatch(line); it.hasNext();) {
                    const auto m = it.next();
                    const QString lit = m.captured();
                    const bool cyrillic = std::any_of(lit.cbegin(), lit.cend(),
                                                      [](QChar c) { return c.script() == QChar::Script_Cyrillic; });
                    const QString before = line.left(m.capturedStart());
                    if (cyrillic && !before.endsWith(u"qsTr(") && !before.endsWith(u"QT_TR_NOOP("))
                        offenders << u"%1:%2"_s.arg(name).arg(n);
                }
            }
        }
        QVERIFY2(offenders.isEmpty(), qPrintable(offenders.join(u", "_s)));
    }
};

QTEST_GUILESS_MAIN(TestGui)
#include "tst_gui.moc"

#include "theme_tokens.h"

#include <QColor>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QRegularExpression>
#include <QSet>
#include <QTest>
#include <QXmlStreamReader>

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

    // English is the source language: Russian lives only in translations/superpupervpn_ru.ts.
    void sourcesAreEnglish()
    {
        QStringList offenders;
        QDirIterator it(QStringLiteral(SUPERPUPERVPN_SOURCE_DIR "/src"), {u"*.cpp"_s, u"*.h"_s, u"*.qml"_s},
                        QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            QFile f(it.next());
            QVERIFY(f.open(QIODevice::ReadOnly));
            int n = 0;
            while (!f.atEnd()) {
                QString line = QString::fromUtf8(f.readLine());
                ++n;
                line.remove(u"\"Русский\""_s);  // the language picker names each language in itself
                if (std::any_of(line.cbegin(), line.cend(), [](QChar c) { return c.script() == QChar::Script_Cyrillic; }))
                    offenders << u"%1:%2"_s.arg(it.fileName()).arg(n);
            }
        }
        QVERIFY2(offenders.isEmpty(), qPrintable(offenders.join(u", "_s)));
    }

    // Every string marked for translation has a finished Russian translation, so the .ts is not stale.
    void russianTranslationComplete()
    {
        QFile ts(QStringLiteral(SUPERPUPERVPN_SOURCE_DIR "/translations/superpupervpn_ru.ts"));
        QVERIFY(ts.open(QIODevice::ReadOnly));
        QSet<QString> translated;
        QStringList unfinished;
        QXmlStreamReader xml(&ts);
        QString source;
        while (xml.readNextStartElement() || !xml.atEnd()) {
            if (!xml.isStartElement())
                continue;
            if (xml.name() == u"source") {
                source = xml.readElementText();
            } else if (xml.name() == u"translation") {
                const bool done = xml.attributes().value(u"type"_s).isEmpty();
                if (done && !xml.readElementText().isEmpty())
                    translated << source;
                else
                    unfinished << source;
            }
        }
        QVERIFY(!xml.hasError());
        QVERIFY2(unfinished.isEmpty(), qPrintable(unfinished.join(u" | "_s)));

        static const QRegularExpression marked(
            u"(?:\\bqsTr|\\btr|QT_TR_NOOP|translate\\(\"\\w+\",)\\s*\\(?\\s*\"((?:[^\"\\\\]|\\\\.)*)\""_s);
        QStringList missing;
        QDirIterator it(QStringLiteral(SUPERPUPERVPN_SOURCE_DIR "/src"), {u"*.cpp"_s, u"*.qml"_s}, QDir::Files,
                        QDirIterator::Subdirectories);
        while (it.hasNext()) {
            QFile f(it.next());
            QVERIFY(f.open(QIODevice::ReadOnly));
            const QString text = QString::fromUtf8(f.readAll());
            for (auto m = marked.globalMatch(text); m.hasNext();) {
                QString s = m.next().captured(1);
                s.replace(u"\\n"_s, u"\n"_s).replace(u"\\\""_s, u"\""_s).replace(u"\\\\"_s, u"\\"_s);
                if (!translated.contains(s))
                    missing << u"%1: %2"_s.arg(it.fileName(), s);
            }
        }
        QVERIFY2(missing.isEmpty(), qPrintable(u"run the update_translations target: "_s + missing.join(u" | "_s)));
    }
};

QTEST_GUILESS_MAIN(TestGui)
#include "tst_gui.moc"

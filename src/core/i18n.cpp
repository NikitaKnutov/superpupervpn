#include "i18n.h"

#include <QCoreApplication>
#include <QTranslator>

using namespace Qt::StringLiterals;

namespace rl {

bool setLanguage(const QString &language)
{
    static QTranslator ru;
    if (language == u"ru") {
        if (ru.isEmpty() && !ru.load(u":/i18n/superpupervpn_ru.qm"_s))
            return false;
        return QCoreApplication::installTranslator(&ru);
    }
    QCoreApplication::removeTranslator(&ru);
    return language == u"en";
}

}

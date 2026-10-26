#include "models.h"

#include "app.h"

#include <QDateTime>
#include <QJsonArray>

#include <algorithm>
#include <climits>

using namespace Qt::StringLiterals;

namespace rl {
ProfileModel::ProfileModel(App *app) : QAbstractListModel(app), m_app(app) {}

std::pair<QString, QString> ProfileModel::splitFlag(const QString &name)
{
    const QList<uint> cps = name.toUcs4();
    auto ri = [](uint c) { return c >= 0x1F1E6 && c <= 0x1F1FF; };
    if (cps.size() >= 2 && ri(cps[0]) && ri(cps[1]))
        return {name.left(4), name.mid(4).trimmed()};
    return {QString(), name};
}

void ProfileModel::rebuild()
{
    const Storage &st = m_app->storage();
    const QString search = m_app->search().trimmed();
    const bool byDelay = m_app->sortMode() == u"delay";
    QStringList collapsed;
    for (const QJsonValue v : st.settings.value(u"collapsedGroups"_s).toArray())
        collapsed << v.toString();

    QStringList groups;
    for (const QJsonValue s : st.subscriptions)
        groups << s.toObject().value(u"id"_s).toString();
    groups << QString();

    beginResetModel();
    m_rows.clear();
    for (const QString &g : groups) {
        QList<int> members;
        for (int i = 0; i < st.profiles.size(); ++i) {
            const Profile &p = st.profiles[i];
            if (p.subscriptionId != g)
                continue;
            if (!search.isEmpty() && !p.name.contains(search, Qt::CaseInsensitive) && !p.host.contains(search, Qt::CaseInsensitive))
                continue;
            members << i;
        }
        if (g.isEmpty() && members.isEmpty() && st.profiles.isEmpty())
            continue;
        std::stable_sort(members.begin(), members.end(), [&](int a, int b) {
            const Profile &pa = st.profiles[a], &pb = st.profiles[b];
            if (pa.pinned != pb.pinned)
                return pa.pinned;
            if (byDelay) {
                auto key = [&](const Profile &p) { const int d = m_app->delayOf(p.id).toInt(); return d < 0 ? INT_MAX : d; };
                if (key(pa) != key(pb))
                    return key(pa) < key(pb);
            }
            return pa.name.localeAwareCompare(pb.name) < 0;
        });
        m_rows << Row{true, g, -1, int(members.size())};
        if (!collapsed.contains(g.isEmpty() ? u"mine"_s : g))
            for (int i : members)
                m_rows << Row{false, g, i};
    }
    endResetModel();
}

void ProfileModel::refresh(const QList<int> &roles)
{
    if (!m_rows.isEmpty())
        emit dataChanged(index(0), index(int(m_rows.size()) - 1), roles);
}

int ProfileModel::rowCount(const QModelIndex &) const
{
    return int(m_rows.size());
}

QVariant ProfileModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_rows.size())
        return {};
    const Row &r = m_rows[index.row()];
    const Storage &st = m_app->storage();
    if (role == IsHeaderRole)
        return r.header;
    if (role == GroupIdRole)
        return r.groupId.isEmpty() ? u"mine"_s : r.groupId;
    if (r.header) {
        const QJsonObject sub = m_app->subscription(r.groupId);
        const QJsonObject info = sub.value(u"info"_s).toObject();
        switch (role) {
        case GroupNameRole: return r.groupId.isEmpty() ? tr("My servers") : sub.value(u"name"_s).toString();
        case CollapsedRole: {
            const QJsonArray c = st.settings.value(u"collapsedGroups"_s).toArray();
            return c.contains(r.groupId.isEmpty() ? u"mine"_s : r.groupId);
        }
        case CountRole: return r.count;
        case IsSubscriptionRole: return !r.groupId.isEmpty();
        case UsedRole: return double(info.value(u"upload"_s).toInteger(0) + info.value(u"download"_s).toInteger(0));
        case TotalRole: return double(info.value(u"total"_s).toInteger(-1));
        case ExpireDaysRole: {
            const qint64 e = info.value(u"expire"_s).toInteger(-1);
            return e <= 0 ? -1 : int((e - QDateTime::currentSecsSinceEpoch()) / 86400);
        }
        case SubErrorRole: return sub.value(u"error"_s).toString();
        case SubUpdatingRole: return m_app->subscriptionUpdating(r.groupId);
        default: return {};
        }
    }
    const Profile &p = st.profiles[r.profile];
    switch (role) {
    case ProfileIdRole: return p.id;
    case NameRole: return splitFlag(p.name).second;
    case FlagRole: return m_app->flagOf(p);
    case BadgeRole: return p.badge();
    case DelayRole: return m_app->delayOf(p.id);
    case PinnedRole: return p.pinned;
    case SelectedRole: return st.settings.value(u"selectedProfileId"_s).toString() == p.id;
    case InsecureRole: return p.insecure;
    case NonStandardRole: return p.nonStandardId();
    default: return {};
    }
}

QHash<int, QByteArray> ProfileModel::roleNames() const
{
    return {{IsHeaderRole, "isHeader"}, {GroupIdRole, "groupId"}, {GroupNameRole, "groupName"},
            {CollapsedRole, "collapsed"}, {CountRole, "count"}, {IsSubscriptionRole, "isSubscription"},
            {UsedRole, "used"}, {TotalRole, "total"}, {ExpireDaysRole, "expireDays"}, {SubErrorRole, "subError"},
            {SubUpdatingRole, "subUpdating"}, {ProfileIdRole, "profileId"}, {NameRole, "name"}, {FlagRole, "flag"},
            {BadgeRole, "badge"}, {DelayRole, "delay"}, {PinnedRole, "pinned"}, {SelectedRole, "selected"},
            {InsecureRole, "insecure"}, {NonStandardRole, "nonStandard"}};
}

namespace {
int rank(const QString &level)
{
    static const QStringList order = {u"debug"_s, u"info"_s, u"warning"_s, u"error"_s, u"fatal"_s};
    const int i = int(order.indexOf(level));
    return i < 0 ? 1 : i;
}

constexpr int kMaxLines = 2000;

}

bool LogModel::matches(const Line &l) const
{
    if (m_level != u"all" && rank(l.level) < rank(m_level))
        return false;
    return m_search.isEmpty() || l.msg.contains(m_search, Qt::CaseInsensitive);
}

void LogModel::refilter()
{
    beginResetModel();
    m_visible.clear();
    for (int i = 0; i < m_lines.size(); ++i)
        if (matches(m_lines[i]))
            m_visible << i;
    endResetModel();
}

void LogModel::append(qint64 timeMs, const QString &level, const QString &message)
{
    m_lines << Line{timeMs, level, message};
    if (m_lines.size() > kMaxLines + 200) {
        m_lines.remove(0, 200);
        if (!m_paused)
            refilter();
        return;
    }
    if (m_paused || !matches(m_lines.last()))
        return;
    beginInsertRows({}, int(m_visible.size()), int(m_visible.size()));
    m_visible << int(m_lines.size() - 1);
    endInsertRows();
}

QVariant LogModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_visible.size())
        return {};
    const Line &l = m_lines[m_visible[index.row()]];
    switch (role) {
    case Qt::UserRole + 1: return QDateTime::fromMSecsSinceEpoch(l.time).toString(u"HH:mm:ss"_s);
    case Qt::UserRole + 2: return l.level;
    case Qt::UserRole + 3: return l.msg;
    default: return {};
    }
}

QHash<int, QByteArray> LogModel::roleNames() const
{
    return {{Qt::UserRole + 1, "time"}, {Qt::UserRole + 2, "level"}, {Qt::UserRole + 3, "msg"}};
}

QString LogModel::text() const
{
    QString out;
    for (const Line &l : m_lines)
        out += QDateTime::fromMSecsSinceEpoch(l.time).toString(Qt::ISODateWithMs) + u' ' + l.level + u' ' + l.msg + u'\n';
    return out;
}

void LogModel::setLevel(const QString &l)
{
    if (l == m_level)
        return;
    m_level = l;
    refilter();
    emit filterChanged();
}

void LogModel::setSearch(const QString &s)
{
    if (s == m_search)
        return;
    m_search = s;
    refilter();
    emit filterChanged();
}

void LogModel::setPaused(bool p)
{
    if (p == m_paused)
        return;
    m_paused = p;
    if (!p)
        refilter();
    emit pausedChanged();
}

}

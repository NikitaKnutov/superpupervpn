#pragma once

#include <QAbstractListModel>
#include <QQmlEngine>
#include <QDateTime>
#include <QStringList>

namespace rl {
class App;

class ProfileModel : public QAbstractListModel {
    Q_OBJECT
    QML_ANONYMOUS
public:
    enum Role {
        IsHeaderRole = Qt::UserRole + 1, GroupIdRole, GroupNameRole, CollapsedRole, CountRole, IsSubscriptionRole,
        UsedRole, TotalRole, ExpireDaysRole, SubErrorRole, SubUpdatingRole,
        ProfileIdRole, NameRole, FlagRole, BadgeRole, DelayRole, PinnedRole, SelectedRole, InsecureRole, NonStandardRole,
    };
    explicit ProfileModel(App *app);
    int rowCount(const QModelIndex & = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    void rebuild();
    void refresh(const QList<int> &roles);

    static std::pair<QString, QString> splitFlag(const QString &name);

private:
    struct Row {
        bool header;
        QString groupId;
        int profile = -1;
        int count = 0;
    };
    App *m_app;
    QList<Row> m_rows;
};

class LogModel : public QAbstractListModel {
    Q_OBJECT
    QML_ANONYMOUS
    Q_PROPERTY(QString level READ level WRITE setLevel NOTIFY filterChanged)
    Q_PROPERTY(QString search READ search WRITE setSearch NOTIFY filterChanged)
    Q_PROPERTY(bool paused READ paused WRITE setPaused NOTIFY pausedChanged)
public:
    using QAbstractListModel::QAbstractListModel;
    int rowCount(const QModelIndex & = {}) const override { return int(m_visible.size()); }
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void append(qint64 timeMs, const QString &level, const QString &message);
    QString text() const;
    QString level() const { return m_level; }
    QString search() const { return m_search; }
    bool paused() const { return m_paused; }
    void setLevel(const QString &l);
    void setSearch(const QString &s);
    void setPaused(bool p);

signals:
    void filterChanged();
    void pausedChanged();

private:
    struct Line {
        qint64 time;
        QString level, msg;
    };
    bool matches(const Line &l) const;
    void refilter();
    QList<Line> m_lines;
    QList<int> m_visible;
    QString m_level = QStringLiteral("all"), m_search;
    bool m_paused = false;
};

}

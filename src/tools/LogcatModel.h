#pragma once

#include <QAbstractListModel>
#include <QByteArray>
#include <QProcess>
#include <QRegularExpression>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

namespace polyscope {

// Streams `adb logcat` for one device with text / level filtering and pause.
class LogcatModel : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString serial READ serial WRITE setSerial NOTIFY serialChanged)
    Q_PROPERTY(bool running READ running NOTIFY runningChanged)
    Q_PROPERTY(bool paused READ paused WRITE setPaused NOTIFY pausedChanged)
    Q_PROPERTY(QString filterText READ filterText WRITE setFilterText NOTIFY filterChanged)
    Q_PROPERTY(bool useRegex READ useRegex WRITE setUseRegex NOTIFY filterChanged)
    Q_PROPERTY(QString minLevel READ minLevel WRITE setMinLevel NOTIFY filterChanged)
    Q_PROPERTY(QString buffers READ buffers WRITE setBuffers NOTIFY buffersChanged)
    Q_PROPERTY(int totalCount READ totalCount NOTIFY countsChanged)

public:
    enum Role { TextRole = Qt::UserRole + 1, LevelRole };

    explicit LogcatModel(QObject* parent = nullptr);
    ~LogcatModel() override;

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString serial() const { return m_serial; }
    void setSerial(const QString& serial);
    bool running() const;
    bool paused() const { return m_paused; }
    void setPaused(bool paused);
    QString filterText() const { return m_filterText; }
    void setFilterText(const QString& text);
    bool useRegex() const { return m_useRegex; }
    void setUseRegex(bool on);
    QString minLevel() const { return m_minLevel; }
    void setMinLevel(const QString& level);
    QString buffers() const { return m_buffers; }
    void setBuffers(const QString& buffers); // e.g. "main,system,crash" or "all"
    int totalCount() const { return static_cast<int>(m_all.size()); }

    Q_INVOKABLE void start();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void clear();          // clears the view
    Q_INVOKABLE void clearDevice();    // adb logcat -c
    Q_INVOKABLE bool save(const QString& path) const; // visible lines
    Q_INVOKABLE QString allVisibleText() const;

    struct Line {
        QString text;
        int level = 2; // 0 V, 1 D, 2 I, 3 W, 4 E, 5 F
    };

    static int levelRank(QChar c);

signals:
    void serialChanged();
    void runningChanged();
    void pausedChanged();
    void filterChanged();
    void buffersChanged();
    void countsChanged();

private:
    void onOutput();
    void flush();
    bool matches(const Line& line) const;
    void rebuildView();

    QString m_serial;
    QProcess* m_process = nullptr;
    QByteArray m_partial;
    QList<Line> m_pending;
    QList<Line> m_all;
    QList<Line> m_view;
    QTimer m_flushTimer;
    QString m_filterText;
    QString m_minLevel = QStringLiteral("V");
    QString m_buffers;
    QRegularExpression m_regex;
    bool m_useRegex = false;
    bool m_paused = false;
};

} // namespace polyscope

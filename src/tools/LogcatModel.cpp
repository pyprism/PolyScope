#include "tools/LogcatModel.h"

#include "adb/AdbClient.h"
#include "core/AppContext.h"
#include "core/Logging.h"

#include <QFile>
#include <QUrl>

namespace polyscope {

namespace {
constexpr int kMaxLines = 20000;
constexpr int kTrimLines = 2000;
} // namespace

LogcatModel::LogcatModel(QObject* parent) : QAbstractListModel(parent) {
    m_flushTimer.setInterval(100);
    connect(&m_flushTimer, &QTimer::timeout, this, &LogcatModel::flush);
}

LogcatModel::~LogcatModel() {
    if (m_process) {
        m_process->disconnect(this);
        m_process->kill();
        m_process->waitForFinished(500); // object is going away; keep Qt from warning about a live QProcess
    }
}

int LogcatModel::levelRank(QChar c) {
    switch (c.toLatin1()) {
    case 'V':
        return 0;
    case 'D':
        return 1;
    case 'I':
        return 2;
    case 'W':
        return 3;
    case 'E':
        return 4;
    case 'F':
    case 'A':
        return 5;
    default:
        return -1;
    }
}

int LogcatModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(m_view.size());
}

QHash<int, QByteArray> LogcatModel::roleNames() const {
    return {{TextRole, "line"}, {LevelRole, "level"}};
}

QVariant LogcatModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= m_view.size()) {
        return {};
    }
    const Line& line = m_view.at(index.row());
    return role == TextRole ? QVariant(line.text) : role == LevelRole ? QVariant(line.level) : QVariant();
}

bool LogcatModel::running() const {
    return m_process && m_process->state() != QProcess::NotRunning;
}

void LogcatModel::setSerial(const QString& serial) {
    if (serial == m_serial) {
        return;
    }
    stop();
    m_serial = serial;
    clear();
    emit serialChanged();
}

void LogcatModel::setPaused(bool paused) {
    if (paused == m_paused) {
        return;
    }
    m_paused = paused;
    if (!paused) {
        rebuildView();
    }
    emit pausedChanged();
}

void LogcatModel::setFilterText(const QString& text) {
    if (text == m_filterText) {
        return;
    }
    m_filterText = text;
    m_regex = m_useRegex ? QRegularExpression(text, QRegularExpression::CaseInsensitiveOption) : QRegularExpression();
    rebuildView();
    emit filterChanged();
}

void LogcatModel::setUseRegex(bool on) {
    if (on == m_useRegex) {
        return;
    }
    m_useRegex = on;
    m_regex = on ? QRegularExpression(m_filterText, QRegularExpression::CaseInsensitiveOption) : QRegularExpression();
    rebuildView();
    emit filterChanged();
}

void LogcatModel::setMinLevel(const QString& level) {
    const QString normalized = level.left(1).toUpper();
    if (normalized == m_minLevel || levelRank(normalized.isEmpty() ? QChar() : normalized.at(0)) < 0) {
        return;
    }
    m_minLevel = normalized;
    rebuildView();
    emit filterChanged();
}

void LogcatModel::setBuffers(const QString& buffers) {
    if (buffers == m_buffers) {
        return;
    }
    m_buffers = buffers;
    emit buffersChanged();
    if (running()) { // restart with the new buffer selection
        stop();
        start();
    }
}

bool LogcatModel::matches(const Line& line) const {
    if (line.level < levelRank(m_minLevel.at(0))) {
        return false;
    }
    if (m_filterText.isEmpty()) {
        return true;
    }
    if (m_useRegex) {
        return !m_regex.isValid() || m_regex.match(line.text).hasMatch();
    }
    return line.text.contains(m_filterText, Qt::CaseInsensitive);
}

void LogcatModel::rebuildView() {
    beginResetModel();
    m_view.clear();
    for (const Line& line : std::as_const(m_all)) {
        if (matches(line)) {
            m_view.append(line);
        }
    }
    endResetModel();
}

void LogcatModel::start() {
    if (running() || m_serial.isEmpty()) {
        return;
    }
    auto* client = AppContext::instance()->adb();
    if (!client->isAvailable()) {
        return;
    }
    QStringList args{QStringLiteral("logcat"), QStringLiteral("-v"), QStringLiteral("threadtime")};
    static const QRegularExpression bufferName(QStringLiteral("^[a-z]+$"));
    for (const QString& buffer : m_buffers.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
        if (bufferName.match(buffer.trimmed()).hasMatch()) {
            args << QStringLiteral("-b") << buffer.trimmed();
        }
    }
    m_partial.clear();
    m_process = client->createProcess(this, m_serial, args);
    m_process->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_process, &QProcess::readyReadStandardOutput, this, &LogcatModel::onOutput);
    connect(m_process, &QProcess::finished, this, [this]() {
        onOutput(); // the last lines can still be buffered when the process exits
        flush();
        m_process->deleteLater();
        m_process = nullptr;
        m_flushTimer.stop();
        emit runningChanged();
    });
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart && m_process) {
            m_process->deleteLater();
            m_process = nullptr;
            m_flushTimer.stop();
            emit runningChanged();
        }
    });
    m_process->start();
    m_flushTimer.start();
    emit runningChanged();
}

void LogcatModel::stop() {
    if (!m_process) {
        return;
    }
    // Detach immediately so start() can run again right away; the old process is
    // reaped on its own once it has actually exited.
    QProcess* old = std::exchange(m_process, nullptr);
    flush();
    old->disconnect(this);
    connect(old, &QProcess::finished, old, &QObject::deleteLater);
    old->kill();
    m_flushTimer.stop();
    emit runningChanged();
}

void LogcatModel::onOutput() {
    m_partial += m_process->readAllStandardOutput();
    qsizetype end;
    while ((end = m_partial.indexOf('\n')) >= 0) {
        QString text = QString::fromUtf8(m_partial.left(end));
        m_partial.remove(0, end + 1);
        if (text.endsWith(QLatin1Char('\r'))) {
            text.chop(1);
        }
        if (text.isEmpty() || text.startsWith(QStringLiteral("--------- beginning"))) {
            continue;
        }
        Line line;
        line.text = text;
        // threadtime: "MM-DD HH:MM:SS.mmm  PID  TID L Tag: msg" – level is the 5th token.
        static const QRegularExpression re(QStringLiteral("^\\d\\d-\\d\\d \\d\\d:\\d\\d:\\d\\d\\.\\d+\\s+\\d+\\s+\\d+\\s+([VDIWEFA])\\s"));
        const auto m = re.match(text);
        line.level = m.hasMatch() ? levelRank(m.captured(1).at(0)) : 2;
        m_pending.append(line);
    }
}

void LogcatModel::flush() {
    if (m_pending.isEmpty()) {
        return;
    }
    const QList<Line> batch = std::exchange(m_pending, {});
    m_all.append(batch);
    if (m_all.size() > kMaxLines) {
        m_all.remove(0, kTrimLines);
        if (!m_paused) {
            rebuildView(); // a paused view stays frozen; unpausing rebuilds it
        }
        emit countsChanged();
        return;
    }
    if (!m_paused) {
        QList<Line> visible;
        for (const Line& line : batch) {
            if (matches(line)) {
                visible.append(line);
            }
        }
        if (!visible.isEmpty()) {
            beginInsertRows({}, static_cast<int>(m_view.size()), static_cast<int>(m_view.size() + visible.size() - 1));
            m_view.append(visible);
            endInsertRows();
        }
    }
    emit countsChanged();
}

void LogcatModel::clear() {
    beginResetModel();
    m_all.clear();
    m_view.clear();
    m_pending.clear();
    m_partial.clear();
    endResetModel();
    emit countsChanged();
}

void LogcatModel::clearDevice() {
    if (m_serial.isEmpty()) {
        return;
    }
    const QString serial = m_serial;
    AppContext::instance()->adb()->run(this, serial, {QStringLiteral("logcat"), QStringLiteral("-c")}, 10000,
                                       [this, serial](const adb::ProcessResult&) {
                                           if (serial == m_serial) {
                                               clear(); // not when the user already switched device
                                           }
                                       });
}

QString LogcatModel::allVisibleText() const {
    QString out;
    for (const Line& line : m_view) {
        out += line.text + QLatin1Char('\n');
    }
    return out;
}

bool LogcatModel::save(const QString& path) const {
    QString local = path;
    if (local.startsWith(QStringLiteral("file:"))) {
        local = QUrl(local).toLocalFile();
    }
    QFile file(local);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    return file.write(allVisibleText().toUtf8()) >= 0;
}

} // namespace polyscope

#pragma once

#include <QObject>
#include <QProcess>
#include <QStringDecoder>
#include <QtQml/qqmlregistration.h>

namespace polyscope {

// Persistent `adb shell` session (no pty): write lines, receive output.
class ShellConsole : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString serial READ serial WRITE setSerial NOTIFY serialChanged)
    Q_PROPERTY(bool running READ running NOTIFY runningChanged)

public:
    explicit ShellConsole(QObject* parent = nullptr);
    ~ShellConsole() override;

    QString serial() const { return m_serial; }
    void setSerial(const QString& serial);
    bool running() const;

    Q_INVOKABLE void start();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void send(const QString& line);

signals:
    void serialChanged();
    void runningChanged();
    void output(const QString& text);

private:
    QString m_serial;
    QProcess* m_process = nullptr;
    QStringDecoder m_decoder{QStringDecoder::Utf8}; // incremental: reads can split a multibyte character
};

} // namespace polyscope

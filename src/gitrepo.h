#pragma once

#include <QObject>
#include <QStringList>
#include <QTimer>

#include <functional>

class QProcess;

// The opened folder's git repository, if it's in one: how many commits the
// upstream branch has that we don't, and a fast-forward-only pull. Every git
// call runs in the background without prompts (a missing SSH key or password
// is an error, never a hang), and results for a folder that's no longer open
// are dropped.
class GitRepo : public QObject {
    Q_OBJECT
    // A repository with an upstream branch to pull from.
    Q_PROPERTY(bool available READ available NOTIFY stateChanged)
    // Commits on the upstream branch that aren't here yet, as of the last fetch.
    Q_PROPERTY(int behind READ behind NOTIFY stateChanged)
    Q_PROPERTY(bool pulling READ pulling NOTIFY stateChanged)
    Q_PROPERTY(QString upstream READ upstream NOTIFY stateChanged)

public:
    // git empty: found on PATH.
    explicit GitRepo(QObject *parent = nullptr, const QString &git = {});
    ~GitRepo() override;

    bool available() const { return !m_top.isEmpty() && !m_upstream.isEmpty(); }
    int behind() const { return m_behind; }
    bool pulling() const { return m_pulling; }
    QString upstream() const { return m_upstream; }
    QString topLevel() const { return m_top; }
    bool busy() const { return m_running > 0; }

    void setFolder(const QString &folder);
    void setFetchInterval(int msec) { m_fetchTimer.setInterval(msec); }

    // Asks the remote for new commits.
    Q_INVOKABLE void fetch();
    // Recounts against what was last fetched; no network.
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void pull();

signals:
    void stateChanged();
    void message(const QString &text);

private:
    using Done = std::function<void(int exitCode, const QString &out, const QString &err)>;
    void run(const QStringList &arguments, int timeoutMs, Done done);
    void count(std::function<void()> then = {});
    void startPull();
    QString explain(const QString &err) const;

    QString m_git;
    QString m_folder;
    QString m_top;
    QString m_upstream;
    int m_behind = 0;
    bool m_pulling = false;
    bool m_fetching = false;
    bool m_pullQueued = false;
    bool m_fetchOnDetect = false;
    int m_generation = 0;   // bumped per folder; stale results are dropped
    int m_running = 0;
    QTimer m_fetchTimer;
};

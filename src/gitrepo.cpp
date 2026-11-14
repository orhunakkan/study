#include "gitrepo.h"

#include <QProcess>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QVariant>

namespace {
constexpr int kLocalTimeout = 10000;
constexpr int kNetworkTimeout = 90000;
constexpr int kFetchEvery = 10 * 60 * 1000;
}

GitRepo::GitRepo(QObject *parent, const QString &git)
    : QObject(parent),
      m_git(git.isEmpty() ? QStandardPaths::findExecutable("git") : git) {
    m_fetchTimer.setInterval(kFetchEvery);
    connect(&m_fetchTimer, &QTimer::timeout, this, &GitRepo::fetch);
}

// Quitting stops git the way a timeout does (SIGTERM, so it removes its lock
// files) and waits a moment for it.
GitRepo::~GitRepo() {
    for (QProcess *process : findChildren<QProcess *>(Qt::FindDirectChildrenOnly)) {
        process->disconnect(this);
        if (process->state() != QProcess::NotRunning) {
            process->terminate();
            process->waitForFinished(3000);
        }
    }
}

void GitRepo::setFolder(const QString &folder) {
    ++m_generation;
    m_folder = folder;
    m_top.clear();
    m_upstream.clear();
    m_behind = 0;
    m_pulling = m_fetching = m_pullQueued = m_fetchOnDetect = false;
    m_fetchTimer.stop();
    emit stateChanged();
    if (m_git.isEmpty() || folder.isEmpty())
        return;
    // Once the repository is known, see what the remote has.
    m_fetchOnDetect = true;
    m_fetchTimer.start();
    refresh();
}

// Runs git in the folder with no way to prompt: no terminal, no askpass
// window. Credential helpers and a loaded SSH agent still work.
void GitRepo::run(const QStringList &arguments, int timeoutMs, Done done) {
    auto *process = new QProcess(this);
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("GIT_TERMINAL_PROMPT", "0");
    env.insert("GIT_ASKPASS", "");
    env.insert("SSH_ASKPASS_REQUIRE", "never");
    env.insert("GIT_OPTIONAL_LOCKS", "0");
    env.insert("LC_ALL", "C");
    process->setProcessEnvironment(env);
    process->setStandardInputFile(QProcess::nullDevice());
    process->setProgram(m_git);
    process->setArguments(QStringList{"-C", m_top.isEmpty() ? m_folder : m_top} + arguments);

    const int generation = m_generation;
    ++m_running;
    auto finish = [this, process, generation, done](int code, const QString &err) {
        if (process->property("finished").toBool())
            return;
        process->setProperty("finished", true);
        process->deleteLater();
        --m_running;
        if (generation == m_generation)
            done(code, QString::fromUtf8(process->readAllStandardOutput()).trimmed(), err);
    };
    connect(process, &QProcess::finished, this, [process, finish](int code, QProcess::ExitStatus status) {
        const QString err = process->property("timedOut").toBool()
                                ? QStringLiteral("timed out")
                                : QString::fromUtf8(process->readAllStandardError());
        finish(status == QProcess::NormalExit ? code : -1, err);
    });
    connect(process, &QProcess::errorOccurred, this, [finish](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart)
            finish(-1, QStringLiteral("git couldn't start"));
    });
    // Terminate rather than kill, so git removes its lock files.
    QTimer::singleShot(timeoutMs, process, [process] {
        process->setProperty("timedOut", true);
        process->terminate();
    });
    process->start();
}

// Is the folder in a repository, which branch does it pull from, and how far
// behind is it?
void GitRepo::refresh() {
    if (m_git.isEmpty() || m_folder.isEmpty() || m_pulling)
        return;
    run({"rev-parse", "--show-toplevel"}, kLocalTimeout, [this](int code, const QString &out, const QString &) {
        const QString top = code == 0 ? out : QString();
        if (top != m_top) {
            m_top = top;
            emit stateChanged();
        }
        if (m_top.isEmpty())
            return;
        run({"rev-parse", "--abbrev-ref", "--symbolic-full-name", "@{upstream}"}, kLocalTimeout,
            [this](int code, const QString &out, const QString &) {
                const QString upstream = code == 0 ? out : QString();
                if (upstream != m_upstream) {
                    m_upstream = upstream;
                    emit stateChanged();
                }
                if (!available()) {
                    if (m_behind != 0) {
                        m_behind = 0;
                        emit stateChanged();
                    }
                    return;
                }
                count([this] {
                    if (std::exchange(m_fetchOnDetect, false))
                        fetch();
                });
            });
    });
}

void GitRepo::count(std::function<void()> then) {
    run({"rev-list", "--count", "HEAD..@{upstream}"}, kLocalTimeout,
        [this, then](int code, const QString &out, const QString &) {
            const int behind = code == 0 ? out.toInt() : 0;
            if (behind != m_behind) {
                m_behind = behind;
                emit stateChanged();
            }
            if (then)
                then();
        });
}

// In the background, and quiet when it fails: an offline laptop shouldn't
// nag every ten minutes. A pull reports what went wrong.
void GitRepo::fetch() {
    if (!available() || m_fetching || m_pulling)
        return;
    m_fetching = true;
    run({"fetch", "--quiet"}, kNetworkTimeout, [this](int, const QString &, const QString &) {
        m_fetching = false;
        count([this] {
            if (m_pullQueued)
                startPull();
        });
    });
}

void GitRepo::pull() {
    if (!available() || m_pulling)
        return;
    m_pulling = true;
    emit stateChanged();
    if (m_fetching)
        m_pullQueued = true;   // pulls once the fetch is done
    else
        startPull();
}

void GitRepo::startPull() {
    m_pullQueued = false;
    run({"rev-parse", "HEAD"}, kLocalTimeout, [this](int, const QString &before, const QString &) {
        run({"pull", "--ff-only", "--no-rebase", "--quiet"}, kNetworkTimeout,
            [this, before](int code, const QString &, const QString &err) {
                if (code != 0) {
                    m_pulling = false;
                    emit stateChanged();
                    emit message(explain(err));
                    count();
                    return;
                }
                run({"rev-list", "--count", before + "..HEAD"}, kLocalTimeout,
                    [this, before](int code, const QString &out, const QString &) {
                        const int pulled = code == 0 && !before.isEmpty() ? out.toInt() : 0;
                        m_pulling = false;
                        emit stateChanged();
                        emit message(pulled > 0 ? tr("Pulled %n commit(s) from %1.", "", pulled).arg(m_upstream)
                                                : tr("Already up to date with %1.").arg(m_upstream));
                        count();
                    });
            });
    });
}

// git's error, said plainly, with what to do about it.
QString GitRepo::explain(const QString &err) const {
    const QString e = err.toLower();
    if (e == "timed out")
        return tr("Couldn't pull: git took too long and was stopped.");
    if (e.contains("not possible to fast-forward") || e.contains("diverg"))
        return tr("Couldn't pull: this branch and %1 have both changed. Merge or rebase in a terminal.").arg(m_upstream);
    if (e.contains("would be overwritten"))
        return tr("Couldn't pull: it would overwrite changes in this folder. Commit or stash them first.");
    if (e.contains("terminal prompts disabled") || e.contains("could not read username")
        || e.contains("could not read password") || e.contains("permission denied")
        || e.contains("authentication failed") || e.contains("host key verification failed"))
        return tr("Couldn't pull: git needs a password or key it can't ask for here. Pull once in a terminal.");
    if (e.contains("could not resolve host") || e.contains("unable to access")
        || e.contains("could not read from remote") || e.contains("connection"))
        return tr("Couldn't pull: the remote can't be reached right now.");

    // Otherwise git's own last word.
    QString line;
    for (const QString &l : err.split('\n', Qt::SkipEmptyParts))
        if (!l.trimmed().startsWith("hint:"))
            line = l.trimmed();
    for (const char *prefix : {"fatal: ", "error: "})
        if (line.startsWith(QLatin1String(prefix)))
            line = line.mid(qstrlen(prefix));
    return line.isEmpty() ? tr("Couldn't pull.") : tr("Couldn't pull: %1").arg(line);
}

#include <QDir>
#include <QFile>
#include <QProcess>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include "backend.h"
#include "bridge.h"
#include "filetree.h"
#include "gitrepo.h"
#include "theme.h"
#include "translator.h"

class StudyTests : public QObject {
    Q_OBJECT

private slots:
    void recognisesMarkdown()
    {
        QVERIFY(FileTree::isMarkdown("/a/README.md"));
        QVERIFY(FileTree::isMarkdown("notes.MARKDOWN"));
        QVERIFY(!FileTree::isMarkdown("/a/script.js"));
        QVERIFY(!FileTree::isMarkdown("/a.md/file"));
        QVERIFY(FileTree::isExcluded(".git", true));
        QVERIFY(FileTree::isExcluded("x.pyc", false));
        QVERIFY(!FileTree::isExcluded(".github", true));
    }

    void opensFoldersAndFiles()
    {
        QTemporaryDir dir;
        write(dir.filePath("a.md"), "# A\n");
        write(dir.filePath("b.txt"), "b");
        Backend backend(dir.filePath("session.ini"));
        backend.openArguments({dir.filePath("a.md")});
        QCOMPARE(backend.rootFolder(), dir.path());
        QCOMPARE(backend.tabs()->count(), 1);
        QCOMPARE(backend.currentPath(), dir.filePath("a.md"));
        QVERIFY(!backend.editing());

        QVERIFY(!backend.openFile(dir.filePath("b.txt")));
        QCOMPARE(backend.tabs()->count(), 1);
        QVERIFY(!backend.message().isEmpty());
    }

    void peekTabsReplaceEachOther()
    {
        QTemporaryDir dir;
        for (const char *name : {"a.md", "b.md", "c.md"})
            write(dir.filePath(name), "x");
        Backend backend(dir.filePath("session.ini"));
        backend.openFolder(dir.path());
        backend.openFile(dir.filePath("a.md"), false);
        backend.openFile(dir.filePath("b.md"), true);
        backend.openFile(dir.filePath("c.md"), true);
        QCOMPARE(backend.tabs()->count(), 2);
        QCOMPARE(backend.currentPath(), dir.filePath("c.md"));
        QVERIFY(backend.tabs()->at(1).transient);

        // Opening it for real keeps it.
        backend.openFile(dir.filePath("c.md"), false);
        QVERIFY(!backend.tabs()->at(1).transient);
        backend.openFile(dir.filePath("b.md"), true);
        QCOMPARE(backend.tabs()->count(), 3);
    }

    void editingCommitsAPeek()
    {
        QTemporaryDir dir;
        write(dir.filePath("a.md"), "x");
        Backend backend(dir.filePath("session.ini"));
        backend.openFile(dir.filePath("a.md"), true);
        backend.toggleMode();
        QVERIFY(backend.editing());
        QVERIFY(!backend.tabs()->at(0).transient);
    }

    void togglesModeOncePerKeypress()
    {
        QTemporaryDir dir;
        write(dir.filePath("a.md"), "x");
        Backend backend(dir.filePath("session.ini"));
        backend.openFile(dir.filePath("a.md"));
        const int id = backend.tabs()->at(0).id;
        backend.toggleMode();
        backend.switchMode(id, false, -1);   // the editor's own binding, same keypress
        QVERIFY(backend.editing());
    }

    void savesAtomicallyAndCleansTheTab()
    {
        QTemporaryDir dir;
        write(dir.filePath("a.md"), "old");
        Backend backend(dir.filePath("session.ini"));
        backend.openFile(dir.filePath("a.md"));
        const int id = backend.tabs()->at(0).id;
        backend.setDirty(id, true);
        QVERIFY(backend.hasDirty());
        QSignalSpy saved(backend.bridge(), &Bridge::saved);
        backend.writeText(id, "new text");
        QCOMPARE(read(dir.filePath("a.md")), QByteArray("new text"));
        QVERIFY(!backend.hasDirty());
        QCOMPARE(saved.count(), 1);
    }

    void closingAnEditedTabAsks()
    {
        QTemporaryDir dir;
        write(dir.filePath("a.md"), "x");
        Backend backend(dir.filePath("session.ini"));
        backend.openFile(dir.filePath("a.md"));
        backend.setDirty(backend.tabs()->at(0).id, true);
        QSignalSpy ask(&backend, &Backend::confirmClose);
        backend.requestClose(0);
        QCOMPARE(ask.count(), 1);
        QCOMPARE(backend.tabs()->count(), 1);
        backend.closeTab(0);
        QCOMPARE(backend.tabs()->count(), 0);
        QVERIFY(backend.canReopen());
        backend.reopenClosed();
        QCOMPARE(backend.tabs()->count(), 1);
    }

    void reloadsCleanTabsAndFlagsEditedOnes()
    {
        QTemporaryDir dir;
        write(dir.filePath("a.md"), "one");
        write(dir.filePath("b.md"), "one");
        Backend backend(dir.filePath("session.ini"));
        backend.openFile(dir.filePath("a.md"));
        backend.openFile(dir.filePath("b.md"));
        backend.pageReady();
        backend.setDirty(backend.tabs()->at(1).id, true);
        QSignalSpy reloaded(backend.bridge(), &Bridge::documentReloaded);

        write(dir.filePath("a.md"), "two");
        write(dir.filePath("b.md"), "two");
        QTRY_COMPARE(reloaded.count(), 1);
        QCOMPARE(reloaded.first().at(1).toString(), QString("two"));
        QTRY_VERIFY(backend.tabs()->at(1).changedOnDisk);
    }

    void restoresTheSession()
    {
        QTemporaryDir dir;
        write(dir.filePath("a.md"), "x");
        write(dir.filePath("b.md"), "y");
        const QString session = dir.filePath("session.ini");
        {
            Backend backend(session);
            backend.openFolder(dir.path());
            backend.openFile(dir.filePath("a.md"));
            backend.openFile(dir.filePath("b.md"));
            backend.toggleMode();
            backend.activate(0);
            backend.saveSession();
        }
        Backend restored(session);
        restored.restoreSession();
        QCOMPARE(restored.rootFolder(), dir.path());
        QCOMPARE(restored.tabs()->count(), 2);
        QCOMPARE(restored.currentPath(), dir.filePath("a.md"));
        QVERIFY(restored.tabs()->at(1).editing);
    }

    void findsMarkdownForGoToFile()
    {
        QTemporaryDir dir;
        QDir(dir.path()).mkpath("docs/deep");
        QDir(dir.path()).mkpath("node_modules/x");
        QDir(dir.path()).mkpath(".git");
        write(dir.filePath("README.md"), "x");
        write(dir.filePath("docs/deep/guide.md"), "x");
        write(dir.filePath("docs/notes.txt"), "x");
        write(dir.filePath("node_modules/x/README.md"), "x");
        write(dir.filePath(".git/HEAD.md"), "x");
        QCOMPARE(Backend::scanMarkdown(dir.path()), QStringList({"docs/deep/guide.md", "README.md"}));
    }

    void tabTitlesDisambiguate()
    {
        QTemporaryDir dir;
        QDir(dir.path()).mkpath("one");
        QDir(dir.path()).mkpath("two");
        write(dir.filePath("one/README.md"), "x");
        write(dir.filePath("two/README.md"), "x");
        Backend backend(dir.filePath("session.ini"));
        backend.openFile(dir.filePath("one/README.md"));
        QCOMPARE(backend.tabs()->title(0), QString("README.md"));
        backend.openFile(dir.filePath("two/README.md"));
        QCOMPARE(backend.tabs()->title(0), QString("README.md — one"));
    }

    void themeReadsThePalette()
    {
        QTemporaryDir dir;
        QDir(dir.path()).mkpath("theme");
        write(dir.filePath("theme/colors.toml"), "mode = \"light\"\naccent = \"#112233\"\nbackground = \"#fafafa\"\n");
        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#112233"));
        QCOMPARE(theme.accentForeground(), QString("white"));
        QVERIFY(!theme.dark());
        QCOMPARE(theme.colors().value("background").toString(), QString("#fafafa"));
        QVERIFY(theme.colors().contains("magenta"));   // defaults fill the gaps
    }

    void themeFallsBack()
    {
        QTemporaryDir dir;
        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#FFD60A"));
        QVERIFY(theme.dark());
    }

    void translatorFollowsTheLanguageFile()
    {
        QTemporaryDir dir;
        write(dir.filePath("language"), "tr\n");
        write(dir.filePath("tr.json"), R"({"Save": "Kaydet"})");
        Translator translator(dir.filePath("language"), dir.path());
        QCOMPARE(translator.language(), QString("tr"));
        QCOMPARE(translator.translate("Main", "Save"), QString("Kaydet"));
        QVERIFY(translator.translate("Main", "Quit").isNull());
    }

    void gitIgnoresFoldersOutsideARepository()
    {
        QTemporaryDir dir;
        GitRepo repo;
        repo.setFolder(dir.path());
        QTRY_VERIFY(!repo.busy());
        QVERIFY(!repo.available());

        // A repository with nothing to pull from has no button either.
        QVERIFY(git(dir.path(), {"init", "-q"}));
        repo.setFolder(dir.path());
        QTRY_VERIFY(!repo.busy());
        QVERIFY(!repo.topLevel().isEmpty());
        QVERIFY(!repo.available());
    }

    void gitCountsAndPullsNewCommits()
    {
        QTemporaryDir dir;
        const QString mine = dir.filePath("mine"), theirs = dir.filePath("theirs");
        QVERIFY(makeRemote(dir.path()));
        write(theirs + "/notes.md", "two\n");
        QVERIFY(git(theirs, {"commit", "-qam", "two"}));
        write(theirs + "/more.md", "three\n");
        QVERIFY(git(theirs, {"add", "."}));
        QVERIFY(git(theirs, {"commit", "-qm", "three"}));

        // Opened before the remote moved on: the first fetch finds both.
        GitRepo repo;
        QSignalSpy messages(&repo, &GitRepo::message);
        repo.setFolder(mine + "/sub");
        QTRY_VERIFY(repo.available());
        QCOMPARE(repo.upstream(), QString("origin/main"));
        QCOMPARE(repo.behind(), 0);
        QVERIFY(git(theirs, {"push", "-q"}));
        repo.fetch();
        QTRY_COMPARE(repo.behind(), 2);

        repo.pull();
        QVERIFY(repo.pulling());
        QTRY_VERIFY(!repo.pulling());
        QTRY_COMPARE(messages.count(), 1);
        QVERIFY(messages.at(0).at(0).toString().startsWith("Pulled 2 commit"));
        QCOMPARE(read(mine + "/notes.md"), QByteArray("two\n"));
        QVERIFY(QFile::exists(mine + "/more.md"));
        QTRY_COMPARE(repo.behind(), 0);

        repo.pull();
        QTRY_COMPARE(messages.count(), 2);
        QVERIFY(messages.at(1).at(0).toString().startsWith("Already up to date"));
    }

    void gitRefusesToPullADivergedBranch()
    {
        QTemporaryDir dir;
        const QString mine = dir.filePath("mine"), theirs = dir.filePath("theirs");
        QVERIFY(makeRemote(dir.path()));
        write(theirs + "/notes.md", "theirs\n");
        QVERIFY(git(theirs, {"commit", "-qam", "theirs"}));
        QVERIFY(git(theirs, {"push", "-q"}));
        write(mine + "/notes.md", "mine\n");
        QVERIFY(git(mine, {"commit", "-qam", "mine"}));

        GitRepo repo;
        QSignalSpy messages(&repo, &GitRepo::message);
        repo.setFolder(mine);
        QTRY_VERIFY(repo.available());
        repo.pull();
        QTRY_COMPARE(messages.count(), 1);
        QVERIFY2(messages.at(0).at(0).toString().contains("both changed"), qPrintable(messages.at(0).at(0).toString()));
        QCOMPARE(read(mine + "/notes.md"), QByteArray("mine\n"));
        QVERIFY(!repo.pulling());
    }

    void backendFollowsTheFoldersRepository()
    {
        QTemporaryDir dir;
        QVERIFY(makeRemote(dir.path()));
        Backend backend(dir.filePath("session.ini"));
        backend.openFolder(dir.filePath("mine"));
        QTRY_VERIFY(backend.git()->available());
        backend.openFolder(dir.path());
        QVERIFY(!backend.git()->available());
    }

private:
    // dir/remote.git with one commit, cloned to dir/mine and dir/theirs.
    static bool makeRemote(const QString &dir)
    {
        if (!git(dir, {"init", "-q", "--bare", "-b", "main", "remote.git"}) || !git(dir, {"clone", "-q", "remote.git", "theirs"}))
            return false;
        QDir(dir).mkpath("theirs/sub");
        write(dir + "/theirs/notes.md", "one\n");
        write(dir + "/theirs/sub/a.md", "a\n");
        return git(dir + "/theirs", {"add", "."}) && git(dir + "/theirs", {"commit", "-qm", "one"})
            && git(dir + "/theirs", {"push", "-q", "-u", "origin", "main"})
            && git(dir, {"clone", "-q", "remote.git", "mine"});
    }
    static bool git(const QString &dir, const QStringList &arguments)
    {
        QProcess process;
        process.setWorkingDirectory(dir);
        process.start("git", QStringList{"-c", "user.name=Test", "-c", "user.email=test@example.com",
                                         "-c", "commit.gpgsign=false", "-c", "init.defaultBranch=main"} + arguments);
        if (!process.waitForFinished(10000) || process.exitCode() != 0) {
            qWarning() << "git" << arguments << process.readAllStandardError();
            return false;
        }
        return true;
    }
    static void write(const QString &path, const QByteArray &contents)
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(contents);
    }
    static QByteArray read(const QString &path)
    {
        QFile file(path);
        return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
    }
};

QTEST_MAIN(StudyTests)
#include "study_tests.moc"

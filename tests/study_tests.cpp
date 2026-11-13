#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include "backend.h"
#include "bridge.h"
#include "filetree.h"
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

private:
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

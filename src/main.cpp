// Study — a reader for folders of Markdown: a file tree, tabs, and documents
// rendered by default, with the source a toggle away.

#include <QDir>
#include <QGuiApplication>
#include <QIcon>
#include <QProcess>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWebEngineProfile>
#include <QStandardPaths>
#include <QUrl>
#include <QtWebEngineQuick/qtwebenginequickglobal.h>

#include "backend.h"
#include "bridge.h"
#include "portalfilepicker.h"
#include "schemehandler.h"
#include "singleinstance.h"
#include "theme.h"
#include "translator.h"

namespace {
// Wayland doesn't let a window raise itself; on Hyprland ask the compositor.
void raiseOnHyprland() {
    if (qEnvironmentVariableIsEmpty("HYPRLAND_INSTANCE_SIGNATURE"))
        return;
    const QString hyprctl = QStandardPaths::findExecutable("hyprctl");
    if (hyprctl.isEmpty())
        return;
    QProcess lua;
    lua.start(hyprctl, {"dispatch", R"(hl.dsp.focus({ window = "class:^(study)$" }))"});
    if (lua.waitForFinished(500) && lua.readAllStandardOutput().trimmed() == "ok")
        return;
    QProcess::startDetached(hyprctl, {"dispatch", "focuswindow", "class:^(study)$"});
}
}

int main(int argc, char *argv[]) {
    SchemeHandler::registerScheme();
    QtWebEngineQuick::initialize();

    QGuiApplication app(argc, argv);
    app.setApplicationName("study");
    app.setOrganizationName("study");
    app.setApplicationVersion("0.1.0");

    // Matches the window to study.desktop, so the compositor (Wayland app_id)
    // and the launcher pick up the installed icon.
    app.setDesktopFileName("study");
    app.setWindowIcon(QIcon::fromTheme("study"));

    const QStringList arguments = app.arguments().mid(1);
    SingleInstance instance;
    if (instance.forward(arguments))
        return 0;
    instance.listen();

    QQuickStyle::setStyle("Material");

    // Installed before anything is translated, and every qsTr() binding is
    // re-evaluated when the Omarchy language changes.
    Translator translator;
    app.installTranslator(&translator);

    Theme theme;
    PortalFilePicker picker;
    Backend backend({}, &picker);

    SchemeHandler scheme;
    QQuickWebEngineProfile::defaultProfile()->installUrlSchemeHandler("study", &scheme);

    backend.restoreSession();
    backend.openArguments(arguments);
    QObject::connect(&instance, &SingleInstance::received, &backend, [&backend](const QStringList &paths) {
        backend.openArguments(paths);
        raiseOnHyprland();
    });
    QObject::connect(&backend, &Backend::quitReady, &app, &QCoreApplication::quit);
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &backend, &Backend::saveSession);

    QQmlApplicationEngine engine;
    QObject::connect(&translator, &Translator::changed, &engine, &QQmlEngine::retranslate);
    engine.rootContext()->setContextProperty("translator", &translator);
    engine.rootContext()->setContextProperty("theme", &theme);
    engine.rootContext()->setContextProperty("backend", &backend);
    engine.load(QUrl("qrc:/Main.qml"));
    if (engine.rootObjects().isEmpty())
        return 1;

    return app.exec();
}

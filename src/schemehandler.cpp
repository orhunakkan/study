#include "schemehandler.h"

#include <QFile>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QUrl>
#include <QWebEngineUrlRequestJob>
#include <QWebEngineUrlScheme>

void SchemeHandler::registerScheme() {
    QWebEngineUrlScheme scheme("study");
    scheme.setSyntax(QWebEngineUrlScheme::Syntax::Host);
    scheme.setFlags(QWebEngineUrlScheme::SecureScheme | QWebEngineUrlScheme::LocalScheme |
                    QWebEngineUrlScheme::LocalAccessAllowed | QWebEngineUrlScheme::CorsEnabled |
                    QWebEngineUrlScheme::FetchApiAllowed);
    QWebEngineUrlScheme::registerScheme(scheme);
}

namespace {
QByteArray mimeFor(const QString &path) {
    const QString suffix = QFileInfo(path).suffix().toLower();
    if (suffix == "js") return "text/javascript";
    if (suffix == "css") return "text/css";
    if (suffix == "html") return "text/html";
    if (suffix == "svg") return "image/svg+xml";
    if (suffix == "woff2") return "font/woff2";
    static const QMimeDatabase db;
    return db.mimeTypeForFile(path, QMimeDatabase::MatchExtension).name().toUtf8();
}
}

void SchemeHandler::requestStarted(QWebEngineUrlRequestJob *job) {
    const QUrl url = job->requestUrl();
    QString path;
    if (url.host() == "app") {
        const QString rel = url.path() == "/" ? "/index.html" : url.path();
        path = rel == "/qwebchannel.js" ? ":/qtwebchannel/qwebchannel.js" : ":/web" + rel;
    } else if (url.host() == "file") {
        path = url.path(QUrl::FullyDecoded);
        if (!QFileInfo(path).isFile()) {
            job->fail(QWebEngineUrlRequestJob::UrlNotFound);
            return;
        }
    } else {
        job->fail(QWebEngineUrlRequestJob::UrlInvalid);
        return;
    }

    auto *file = new QFile(path, job);
    if (!file->open(QIODevice::ReadOnly)) {
        job->fail(QWebEngineUrlRequestJob::UrlNotFound);
        return;
    }
    job->reply(mimeFor(path), file);
}

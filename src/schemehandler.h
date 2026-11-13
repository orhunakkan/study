#pragma once

#include <QWebEngineUrlSchemeHandler>

// Serves study://app/… (the document page, compiled into the binary) and
// study://file/<absolute path> (a document's local images and media), so the
// page never needs file:// access.
class SchemeHandler : public QWebEngineUrlSchemeHandler {
    Q_OBJECT

public:
    using QWebEngineUrlSchemeHandler::QWebEngineUrlSchemeHandler;

    // Must run before the application object exists.
    static void registerScheme();

    void requestStarted(QWebEngineUrlRequestJob *job) override;
};

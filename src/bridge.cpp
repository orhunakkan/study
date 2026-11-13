#include "bridge.h"

#include "backend.h"

Bridge::Bridge(Backend *backend) : QObject(backend), m_backend(backend) {}

void Bridge::ready() { m_backend->pageReady(); }
void Bridge::setDirty(int id, bool dirty) { m_backend->setDirty(id, dirty); }
void Bridge::saveText(int id, const QString &text) { m_backend->writeText(id, text); }
void Bridge::reportTopLine(int id, double line) { m_backend->setTopLine(id, line); }
void Bridge::switchMode(int id, const QString &mode, int line) { m_backend->switchMode(id, mode == "edit", line); }
void Bridge::openPath(const QString &path, const QString &fragment) { m_backend->openLink(path, fragment); }
void Bridge::openExternal(const QString &url) { m_backend->openExternal(url); }

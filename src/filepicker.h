#pragma once

#include <QObject>
#include <QUrl>

// Asks the user for a folder. Abstract so tests can inject a fake picker.
class FilePicker : public QObject {
    Q_OBJECT

public:
    explicit FilePicker(QObject *parent = nullptr) : QObject(parent) {}
    virtual ~FilePicker() = default;

    virtual void chooseFolder(const QString &startIn) = 0;

signals:
    void folderSelected(const QUrl &url);
    void failed(const QString &message);
};

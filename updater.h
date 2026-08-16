// Uygulamanın kendini otomatik güncelleyebilmesi için gereken Updater sınıfının tanımlamaları.

// updater.h
// Bu dosya, uygulamanÄ±n kendi kendini gÃ¼ncellemesini (OTA - Over The Air Update) 
// saÄŸlayan Updater sÄ±nÄ±fÄ±nÄ±n tanÄ±mlarÄ±nÄ± iÃ§erir.
#ifndef UPDATER_H
#define UPDATER_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QString>

class Updater : public QObject {
    Q_OBJECT
public:
    explicit Updater(QObject *parent = nullptr);
    void checkForUpdates();

private:
    QNetworkAccessManager *networkManager;
    QString currentVersion = "3.4"; 
    void downloadAndApplyUpdate(const QString &fileUrl);
};

#endif 

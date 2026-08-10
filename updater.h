// updater.h
// Bu dosya, uygulamanın kendi kendini güncellemesini (OTA - Over The Air Update) 
// sağlayan Updater sınıfının tanımlarını içerir.
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
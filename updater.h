// Uygulamanin kendini otomatik guncelleyebilmesi icin gereken Updater sinifinin tanimlamalari.

// updater.h
// Bu dosya, uygulamanin kendi kendini guncellemesini (OTA - Over The Air Update) 
// saglayan Updater sinifinin tanimlarini icerir.
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

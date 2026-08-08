// updater.cpp
// İnternet üzerinden (veya yerel sunucudan) güncel sürümü kontrol etme,
// indirme ve eski .exe dosyasını silip yenisini başlatma işlemlerini yapar.
#include "updater.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QDir>
#include <QProcess>
#include <QDebug>
#include <QCoreApplication>
#include <QUrl>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QMessageBox> // MESAJ KUTUSU İÇİN EKLENEN KÜTÜPHANE

Updater::Updater(QObject *parent) : QObject(parent) {
    networkManager = new QNetworkAccessManager(this);
}

void Updater::checkForUpdates() {
    // 1. Adım: Sunucudaki version.json dosyasına istek atılır
    QNetworkRequest request(QUrl("http://localhost:8080/version.json"));
    QNetworkReply *reply = networkManager->get(request);

    connect(reply, &QNetworkReply::finished, this, [=]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            qDebug() << "Update check failed:" << reply->errorString();
            // KULLANICIYA GÖSTERİLEN HATA MESAJI
            QMessageBox::critical(nullptr, "Bağlanti Hatasi", "Güncelleme sunucusuna ulaşilamadi:\n" + reply->errorString());
            return;
        }

        QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
        if (obj["latest_version"].toString().toFloat() > currentVersion.toFloat()) {
            qDebug() << "New version found! Downloading from:" << obj["download_url"].toString();
            downloadAndApplyUpdate(obj["download_url"].toString());
        } else {
            qDebug() << "Application is up to date.";
            QMessageBox::information(nullptr, "Zaten Güncel", "Uygulamanız zaten güncel sürümde (v" + currentVersion + "). Yeni bir güncelleme bulunmuyor.");
        }
    });
}

void Updater::downloadAndApplyUpdate(const QString &fileUrl) {
    // 2. Adım: Yeni sürüm varsa .exe dosyası indirilmeye başlanır
    QUrl url(fileUrl);
    QNetworkRequest request(url);
    QNetworkReply *reply = networkManager->get(request);

    connect(reply, &QNetworkReply::finished, this, [=]() {
        reply->deleteLater();
        
        // 1. Ağ hatasi kontrolü
        if (reply->error() != QNetworkReply::NoError) {
            qDebug() << "File download network error:" << reply->errorString();
            QMessageBox::critical(nullptr, "İndirme Hatasi", "Dosya indirilirken ağ bağlantisi koptu:\n" + reply->errorString());
            return;
        }

        // 2. Gerçek HTTP Başari (200) kontrolü
        int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        qDebug() << "HTTP Status Code:" << statusCode;
        
        if (statusCode != 200) {
            qDebug() << "Server returned an error instead of the file! Status:" << statusCode;
            QMessageBox::warning(nullptr, "Sunucu Hatasi", "Sunucu dosyayi vermeyi reddetti. Hata Kodu: " + QString::number(statusCode));
            return;
        }

        // 3. Dosya boyutu ve veri kontrolü
        QByteArray fileData = reply->readAll();
        qDebug() << "Downloaded file size:" << fileData.size() << "bytes";

        if (fileData.size() < 10000) { 
            qDebug() << "CRITICAL ERROR: The downloaded file is too small! It might be corrupted or a text file.";
            QMessageBox::critical(nullptr, "Dosya Hatasi", "İndirilen dosya çok küçük veya bozuk. Lütfen daha sonra tekrar deneyin.");
            return;
        }

        // Uygulamanin kesin ve tam klasör yolunu (Absolute Path) aliyoruz
        QString appPath = QCoreApplication::applicationFilePath(); 
        QString appDir = QCoreApplication::applicationDirPath();   
        
        // update.exe'yi uygulamanin tam yanina kaydet
        QString updateExePath = QDir(appDir).filePath("update.exe");
        QFile file(updateExePath);
        
        if (!file.open(QIODevice::WriteOnly)) {
            qDebug() << "Failed to create update.exe! Check folder permissions.";
            QMessageBox::critical(nullptr, "Erişim Hatasi", "Güncelleme dosyasi yazilamadi. Klasör yetkilerini kontrol edin.");
            return;
        }
        file.write(fileData);
        file.close(); 

        // BAT dosyasini oluştur
        // Uygulamanın kendi kendini silebilmesi için geçici bir .bat dosyası yazılır.
        QString batPath = QDir(appDir).filePath("update.bat");
        QFile bat(batPath);
        
        if (bat.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream stream(&bat);
            
            QString winAppPath = appPath; winAppPath.replace("/", "\\");
            QString winAppDir = appDir; winAppDir.replace("/", "\\");
            
            stream << "@echo off\r\n"
                   << "cd /d \"" << winAppDir << "\"\r\n"
                   << "timeout /t 2 /nobreak > nul\r\n"
                   << "del /f /q \"" << winAppPath << "\"\r\n"
                   << "ren \"update.exe\" \"CanBusSimulator.exe\"\r\n"
                   << "start \"\" \"" << winAppPath << "\"\r\n"
                   << "del \"%~f0\"\r\n"
                   << "exit\r\n";
            bat.close();
        }

        qDebug() << "Update downloaded successfully. Restarting application...";
        
        QString winBatPath = batPath; winBatPath.replace("/", "\\");
        QProcess::startDetached("cmd.exe", {"/c", winBatPath});
        QCoreApplication::quit(); 
    });
}
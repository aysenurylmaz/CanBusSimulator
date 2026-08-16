// PostgreSQL veritabanina CAN sinyallerini yazan ve disaridan gelen (Web U) komutlari asenkron olarak dinleyen veritabani yonetim dosyasidir.

#include "dbmanager.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QDebug>
#include <QSqlDriver>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QSettings>
#include <QTimer>
#include <QCoreApplication>

DbManager::DbManager() {
    QSettings settings("config.ini", QSettings::niFormat);
    db = QSqlDatabase::addDatabase("QPSQL");
    db.setHostName(settings.value("DatabaseHost", "127.0.0.1").toString());
    db.setDatabaseName(settings.value("DatabaseName", "canbus_telemetry").toString());
    db.setUserName(settings.value("DatabaseUser", "postgres").toString());
    db.setPassword(settings.value("DatabasePass", "canbusadmin").toString());
    db.setPort(settings.value("DatabasePort", 5433).tont());
    
    // Veritaban kapalyken uygulamann donmasn (hang) engellemek iin balant zaman am ekliyoruz
    db.setConnectOptions("connect_timeout=2");
}
// Nesne yok edilirken (uygulama kapanrken) veritaban balantsn gvenli bir ekilde kapatyoruz.
DbManager::~DbManager() {
    if (db.isOpen()) {
        db.close();
    }
}
// Singleton (Tekil Nesne) Tasarm Deseni
// Uygulama boyunca DbManager snfnn sadece bir kez oluturulmasn ve her yerden ayn rnee (instance) eriilmesini salar.
DbManager& DbManager::instance() {
    static DbManager _instance;
    return _instance;
}

bool DbManager::connectToDatabase() {
    if (db.isOpen()) return true;

    if (!db.open()) {
        qDebug() << "Veritabani balanti hatasi:" << db.lastError().text();
        return false;
    }
    
    qDebug() << "PostgreSQL veritabanina baariyla balanildi.";

    // Postgres Trigger dinlemesi iin ayar yapyoruz
    if (db.driver()->hasFeature(QSqlDriver::EventNotifications)) {
        // Veritabanndan gelen bildirimleri onNotification fonksiyonuna balyoruz.
        connect(db.driver(), &QSqlDriver::notification, this, &DbManager::onNotification);
        db.driver()->subscribeToNotification("device_commands_channel");
        qDebug() << "Subscribed to device_commands_channel";
    } else {
        qDebug() << "Bu veritaban srcs Event Notifications (LSTEN/NOTFY) desteklemiyor!";
    }

    return true;
}
// Veritabanndan (PostgreSQL NOTFY) bir bildirim geldiinde tetiklenen fonksiyon.
void DbManager::onNotification(const QString& name, QSqlDriver::NotificationSource source, const QVariant& payload) {
    if (name == "device_commands_channel") {
        qDebug() << "Notification received from DB:" << payload.toString();
        
        QJsonDocument jsonDoc = QJsonDocument::fromJson(payload.toString().toUtf8());
        if (!jsonDoc.isNull() && jsonDoc.isObject()) {
            QJsonObject obj = jsonDoc.object();
            QString commandName = obj["command_name"].toString();
            QString commandValue = obj["command_value"].toString();
            
            if (commandValue.isEmpty() && obj.contains("id")) {
                int id = obj["id"].tont();
                QTimer::singleShot(0, this, [=]() {
                    QSqlQuery query(db);
                    query.prepare("SELECT command_value FROM device_commands WHERE id = :id");
                    query.bindValue(":id", id);
                    if (query.exec() && query.next()) {
                        QString val = query.value(0).toString();
                        qDebug() << "Fetched command_value for id" << id << ":" << val.left(100);
                        emit commandReceived(commandName, val);
                    } else {
                        qDebug() << "Failed to fetch command_value for id" << id << "Error:" << query.lastError().text();
                    }
                });
                return;
            }
            
            emit commandReceived(commandName, commandValue);
        }
    }
}
// Toplu veri eklemeleri (rn: saniyede yzlerce CAN Bus sinyali) iin performans artrmak adna Transaction balatr.
bool DbManager::transaction() {
    if (!db.isOpen() && !connectToDatabase()) return false;
    return db.transaction();
}
// Transaction iindeki tm ilemleri hata yoksa tek seferde veritabanna kalc olarak yazar (kaydeder).
bool DbManager::commit() {
    if (!db.isOpen()) return false;
    return db.commit();
}
// CAN Bus zerinden zmlenmi (parse edilmi) telemetri verilerini veritabanna kaydeder.
void DbManager::logSignal(const QString& messaged, const QString& signalName, double physicalValue, const QString& rawHex) {
    if (!db.isOpen() && !connectToDatabase()) {
        return;
    }

    QSqlQuery query(db);
    query.prepare("NSERT NTO can_signals (message_id, signal_name, physical_value, raw_hex) "
                  "VALUES (:message_id, :signal_name, :physical_value, :raw_hex) "
                  "ON CONFLCT (signal_name) DO UPDATE SET "
                  "message_id = EXCLUDED.message_id, "
                  "physical_value = EXCLUDED.physical_value, "
                  "raw_hex = EXCLUDED.raw_hex, "
                  "timestamp = CURRENT_TMESTAMP");
    query.bindValue(":message_id", messaged);
    query.bindValue(":signal_name", signalName);
    query.bindValue(":physical_value", physicalValue);
    query.bindValue(":raw_hex", rawHex);

    if (!query.exec()) {
        qDebug() << "Veritabanina yazma hatasi:" << query.lastError().text();
    }
}


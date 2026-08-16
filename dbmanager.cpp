// PostgreSQL veritabanina CAN sinyallerini yazan ve disaridan gelen (Web UI) komutlari asenkron olarak dinleyen veritabani yonetim dosyasidir.

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
    QSettings settings("config.ini", QSettings::IniFormat);
    db = QSqlDatabase::addDatabase("QPSQL");
    db.setHostName(settings.value("DatabaseHost", "127.0.0.1").toString());
    db.setDatabaseName(settings.value("DatabaseName", "canbus_telemetry").toString());
    db.setUserName(settings.value("DatabaseUser", "postgres").toString());
    db.setPassword(settings.value("DatabasePass", "canbusadmin").toString());
    db.setPort(settings.value("DatabasePort", 5433).toInt());
    
    // VeritabanÃƒâ€Ã‚Â± kapalÃƒâ€Ã‚Â±yken uygulamanÃƒâ€Ã‚Â±n donmasÃƒâ€Ã‚Â±nÃƒâ€Ã‚Â± (hang) engellemek iÃƒÆ’Ã‚Â§in baÃƒâ€Ã…Â¸lantÃƒâ€Ã‚Â± zaman aÃƒâ€¦Ã…Â¸Ãƒâ€Ã‚Â±mÃƒâ€Ã‚Â± ekliyoruz
    db.setConnectOptions("connect_timeout=2");
}
// Nesne yok edilirken (uygulama kapanÃƒâ€Ã‚Â±rken) veritabanÃƒâ€Ã‚Â± baÃƒâ€Ã…Â¸lantÃƒâ€Ã‚Â±sÃƒâ€Ã‚Â±nÃƒâ€Ã‚Â± gÃƒÆ’Ã‚Â¼venli bir Ãƒâ€¦Ã…Â¸ekilde kapatÃƒâ€Ã‚Â±yoruz.
DbManager::~DbManager() {
    if (db.isOpen()) {
        db.close();
    }
}
// Singleton (Tekil Nesne) TasarÃƒâ€Ã‚Â±m Deseni
// Uygulama boyunca DbManager sÃƒâ€Ã‚Â±nÃƒâ€Ã‚Â±fÃƒâ€Ã‚Â±nÃƒâ€Ã‚Â±n sadece bir kez oluÃƒâ€¦Ã…Â¸turulmasÃƒâ€Ã‚Â±nÃƒâ€Ã‚Â± ve her yerden aynÃƒâ€Ã‚Â± ÃƒÆ’Ã‚Â¶rneÃƒâ€Ã…Â¸e (instance) eriÃƒâ€¦Ã…Â¸ilmesini saÃƒâ€Ã…Â¸lar.
DbManager& DbManager::instance() {
    static DbManager _instance;
    return _instance;
}

bool DbManager::connectToDatabase() {
    if (db.isOpen()) return true;

    if (!db.open()) {
        qDebug() << "Veritabani baÃƒâ€Ã…Â¸lanti hatasi:" << db.lastError().text();
        return false;
    }
    
    qDebug() << "PostgreSQL veritabanina baÃƒâ€¦Ã…Â¸ariyla baÃƒâ€Ã…Â¸lanildi.";

    // Postgres Trigger dinlemesi iÃƒÆ’Ã‚Â§in ayar yapÃƒâ€Ã‚Â±yoruz
    if (db.driver()->hasFeature(QSqlDriver::EventNotifications)) {
        // VeritabanÃƒâ€Ã‚Â±ndan gelen bildirimleri onNotification fonksiyonuna baÃƒâ€Ã…Â¸lÃƒâ€Ã‚Â±yoruz.
        connect(db.driver(), &QSqlDriver::notification, this, &DbManager::onNotification);
        db.driver()->subscribeToNotification("device_commands_channel");
        qDebug() << "Subscribed to device_commands_channel";
    } else {
        qDebug() << "Bu veritabanÃƒâ€Ã‚Â± sÃƒÆ’Ã‚Â¼rÃƒÆ’Ã‚Â¼cÃƒÆ’Ã‚Â¼sÃƒÆ’Ã‚Â¼ Event Notifications (LISTEN/NOTIFY) desteklemiyor!";
    }

    return true;
}
// VeritabanÃƒâ€Ã‚Â±ndan (PostgreSQL NOTIFY) bir bildirim geldiÃƒâ€Ã…Â¸inde tetiklenen fonksiyon.
void DbManager::onNotification(const QString& name, QSqlDriver::NotificationSource source, const QVariant& payload) {
    if (name == "device_commands_channel") {
        qDebug() << "Notification received from DB:" << payload.toString();
        
        QJsonDocument jsonDoc = QJsonDocument::fromJson(payload.toString().toUtf8());
        if (!jsonDoc.isNull() && jsonDoc.isObject()) {
            QJsonObject obj = jsonDoc.object();
            QString commandName = obj["command_name"].toString();
            QString commandValue = obj["command_value"].toString();
            
            if (commandValue.isEmpty() && obj.contains("id")) {
                int id = obj["id"].toInt();
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
// Toplu veri eklemeleri (ÃƒÆ’Ã‚Â¶rn: saniyede yÃƒÆ’Ã‚Â¼zlerce CAN Bus sinyali) iÃƒÆ’Ã‚Â§in performansÃƒâ€Ã‚Â± artÃƒâ€Ã‚Â±rmak adÃƒâ€Ã‚Â±na Transaction baÃƒâ€¦Ã…Â¸latÃƒâ€Ã‚Â±r.
bool DbManager::transaction() {
    if (!db.isOpen() && !connectToDatabase()) return false;
    return db.transaction();
}
// Transaction iÃƒÆ’Ã‚Â§indeki tÃƒÆ’Ã‚Â¼m iÃƒâ€¦Ã…Â¸lemleri hata yoksa tek seferde veritabanÃƒâ€Ã‚Â±na kalÃƒâ€Ã‚Â±cÃƒâ€Ã‚Â± olarak yazar (kaydeder).
bool DbManager::commit() {
    if (!db.isOpen()) return false;
    return db.commit();
}
// CAN Bus ÃƒÆ’Ã‚Â¼zerinden ÃƒÆ’Ã‚Â§ÃƒÆ’Ã‚Â¶zÃƒÆ’Ã‚Â¼mlenmiÃƒâ€¦Ã…Â¸ (parse edilmiÃƒâ€¦Ã…Â¸) telemetri verilerini veritabanÃƒâ€Ã‚Â±na kaydeder.
void DbManager::logSignal(const QString& messageId, const QString& signalName, double physicalValue, const QString& rawHex) {
    if (!db.isOpen() && !connectToDatabase()) {
        return;
    }

    QSqlQuery query(db);
    query.prepare("INSERT INTO can_signals (message_id, signal_name, physical_value, raw_hex) "
                  "VALUES (:message_id, :signal_name, :physical_value, :raw_hex) "
                  "ON CONFLICT (signal_name) DO UPDATE SET "
                  "message_id = EXCLUDED.message_id, "
                  "physical_value = EXCLUDED.physical_value, "
                  "raw_hex = EXCLUDED.raw_hex, "
                  "timestamp = CURRENT_TIMESTAMP");
    query.bindValue(":message_id", messageId);
    query.bindValue(":signal_name", signalName);
    query.bindValue(":physical_value", physicalValue);
    query.bindValue(":raw_hex", rawHex);

    if (!query.exec()) {
        qDebug() << "Veritabanina yazma hatasi:" << query.lastError().text();
    }
}


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
    
    // VeritabanÃ„Â± kapalÃ„Â±yken uygulamanÃ„Â±n donmasÃ„Â±nÃ„Â± (hang) engellemek iÃƒÂ§in baÃ„Å¸lantÃ„Â± zaman aÃ…Å¸Ã„Â±mÃ„Â± ekliyoruz
    db.setConnectOptions("connect_timeout=2");
}
// Nesne yok edilirken (uygulama kapanÃ„Â±rken) veritabanÃ„Â± baÃ„Å¸lantÃ„Â±sÃ„Â±nÃ„Â± gÃƒÂ¼venli bir Ã…Å¸ekilde kapatÃ„Â±yoruz.
DbManager::~DbManager() {
    if (db.isOpen()) {
        db.close();
    }
}
// Singleton (Tekil Nesne) TasarÃ„Â±m Deseni
// Uygulama boyunca DbManager sÃ„Â±nÃ„Â±fÃ„Â±nÃ„Â±n sadece bir kez oluÃ…Å¸turulmasÃ„Â±nÃ„Â± ve her yerden aynÃ„Â± ÃƒÂ¶rneÃ„Å¸e (instance) eriÃ…Å¸ilmesini saÃ„Å¸lar.
DbManager& DbManager::instance() {
    static DbManager _instance;
    return _instance;
}

bool DbManager::connectToDatabase() {
    if (db.isOpen()) return true;

    if (!db.open()) {
        qDebug() << "Veritabani baÃ„Å¸lanti hatasi:" << db.lastError().text();
        return false;
    }
    
    qDebug() << "PostgreSQL veritabanina baÃ…Å¸ariyla baÃ„Å¸lanildi.";

    // Postgres Trigger dinlemesi iÃƒÂ§in ayar yapÃ„Â±yoruz
    if (db.driver()->hasFeature(QSqlDriver::EventNotifications)) {
        // VeritabanÃ„Â±ndan gelen bildirimleri onNotification fonksiyonuna baÃ„Å¸lÃ„Â±yoruz.
        connect(db.driver(), &QSqlDriver::notification, this, &DbManager::onNotification);
        db.driver()->subscribeToNotification("device_commands_channel");
        qDebug() << "Subscribed to device_commands_channel";
    } else {
        qDebug() << "Bu veritabanÃ„Â± sÃƒÂ¼rÃƒÂ¼cÃƒÂ¼sÃƒÂ¼ Event Notifications (LISTEN/NOTIFY) desteklemiyor!";
    }

    return true;
}
// VeritabanÃ„Â±ndan (PostgreSQL NOTIFY) bir bildirim geldiÃ„Å¸inde tetiklenen fonksiyon.
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
                        emit commandReceived(commandName, val);
                    }
                });
                return;
            }
            
            emit commandReceived(commandName, commandValue);
        }
    }
}
// Toplu veri eklemeleri (ÃƒÂ¶rn: saniyede yÃƒÂ¼zlerce CAN Bus sinyali) iÃƒÂ§in performansÃ„Â± artÃ„Â±rmak adÃ„Â±na Transaction baÃ…Å¸latÃ„Â±r.
bool DbManager::transaction() {
    if (!db.isOpen() && !connectToDatabase()) return false;
    return db.transaction();
}
// Transaction iÃƒÂ§indeki tÃƒÂ¼m iÃ…Å¸lemleri hata yoksa tek seferde veritabanÃ„Â±na kalÃ„Â±cÃ„Â± olarak yazar (kaydeder).
bool DbManager::commit() {
    if (!db.isOpen()) return false;
    return db.commit();
}
// CAN Bus ÃƒÂ¼zerinden ÃƒÂ§ÃƒÂ¶zÃƒÂ¼mlenmiÃ…Å¸ (parse edilmiÃ…Å¸) telemetri verilerini veritabanÃ„Â±na kaydeder.
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

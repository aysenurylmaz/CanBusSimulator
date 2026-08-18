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
    // Veritabanı yapılandırma ayarlarını 'config.ini' dosyasından okuruz.
    QSettings settings("config.ini", QSettings::IniFormat);
    db = QSqlDatabase::addDatabase("QPSQL");
    // Bağlantı parametrelerini ayarlar, yapılandırma dosyasında yoksa varsayılan değerleri kullanır.
    db.setHostName(settings.value("DatabaseHost", "127.0.0.1").toString());
    db.setDatabaseName(settings.value("DatabaseName", "canbus_telemetry").toString());
    db.setUserName(settings.value("DatabaseUser", "postgres").toString());
    db.setPassword(settings.value("DatabasePass", "canbusadmin").toString());
    db.setPort(settings.value("DatabasePort", 5433).toInt());
    
    // Veritabanı bağlantısı kurulurken beklenecek maksimum zaman aşımı (timeout) süresini 2 saniye olarak ayarlar.
    db.setConnectOptions("connect_timeout=2");
}
// Nesne yok edildiğinde (uygulama kapandığında vs.) açık olan veritabanı bağlantısını güvenlice kapatır.
DbManager::~DbManager() {
    if (db.isOpen()) {
        db.close();
    }
}
//Singleton (Tekil Nesne) Tasarım Deseni
// Uygulama boyunca DbManager sınıfının sadece bir kez oluşturulmasını ve her yerden aynı örneğe (instance) erişilmesini sağlar.
DbManager& DbManager::instance() {
    static DbManager _instance;
    return _instance;
}

bool DbManager::connectToDatabase() {
    // Bağlantı zaten açıksa tekrar açmaya çalışmaya gerek yok.
    if (db.isOpen()) return true;
// Veritabanı bağlantısını başlatmayı dener, başarısız olursa hatayı konsola yazar.
    if (!db.open()) {
        qDebug() << "Veritabanı bağlantı hatası:" << db.lastError().text();
        return false;
    }
    
    qDebug() << " Veritabanına başarıyla bağlanıldı..";

    // Postgres Trigger (Tetikleyici) Dinlemesi
    // Veritabanı sürücüsünün asenkron bildirimleri destekleyip desteklemediğini kontrol eder.
    if (db.driver()->hasFeature(QSqlDriver::EventNotifications)) {
       // Bildirim geldiğinde çalışacak fonksiyonu (slot) bağlar.
        connect(db.driver(), &QSqlDriver::notification, this, &DbManager::onNotification);
        db.driver()->subscribeToNotification("device_commands_channel");
        qDebug() << "Subscribed to device_commands_channel";
    } else {
        qDebug() << "Uyarı: Veritabanı sürücüsü asenkron bildirim (Event Notifications) özelliğini desteklemiyor!";
    }

    return true;
}
// Dinlenilen kanaldan bir bildirim (NOTIFY) geldiğinde tetiklenen fonksiyondur.
void DbManager::onNotification(const QString& name, QSqlDriver::NotificationSource source, const QVariant& payload) {
    if (name == "device_commands_channel") {
        qDebug() << "Notification received from DB:" << payload.toString();
        // Gelen JSON formatındaki payload verisini parse eder (ayrıştırır).
        QJsonDocument jsonDoc = QJsonDocument::fromJson(payload.toString().toUtf8());
        if (!jsonDoc.isNull() && jsonDoc.isObject()) {
            QJsonObject obj = jsonDoc.object();
            QString commandName = obj["command_name"].toString();
            QString commandValue = obj["command_value"].toString();
            
            // Eğer command_value boş gelmişse ancak 'id' verisi varsa,
            // değeri asenkron bir şekilde (ana thread'i kilitlemeden) veritabanından çeker.
            if (commandValue.isEmpty() && obj.contains("id")) {
                int id = obj["id"].toInt();
                QTimer::singleShot(0, this, [=]() {
                    QSqlQuery query(db);
                    query.prepare("SELECT command_value FROM device_commands WHERE id = :id");
                    query.bindValue(":id", id);
                    if (query.exec() && query.next()) {
                        QString val = query.value(0).toString();
                        qDebug() << "Fetched command_value for id" << id << ":" << val.left(100);
                        
                        // İşlem tamamlandığında alınan komutu sisteme sinyal olarak fırlatır.
                        emit commandReceived(commandName, val);
                    } else {
                        qDebug() << "Failed to fetch command_value for id" << id << "Error:" << query.lastError().text();
                    }
                });
                return;
            }
            // Eğer değerler tamamsa doğrudan komut alındı sinyalini fırlatır.
            emit commandReceived(commandName, commandValue);
        }
    }
}
// Veritabanı işlem (Transaction) bloğunu başlatır. Toplu insert/update işlemlerinde performansı artırır.
bool DbManager::transaction() {
    if (!db.isOpen() && !connectToDatabase()) return false;
    return db.transaction();
}
// Başlatılan veritabanı işlemini onaylar (Commit) ve değişiklikleri kalıcı hale getirir.
bool DbManager::commit() {
    if (!db.isOpen()) return false;
    return db.commit();
}
// CAN Bus üzerinden okunan sinyalleri veritabanındaki 'can_signals' tablosuna kaydeder.
// UPSERT mantığı ile çalışır: Sinyal veritabanında yoksa INSERT yapar (ekler), 
// varsa ON CONFLICT kullanılarak mevcut kaydı günceller (UPDATE).
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


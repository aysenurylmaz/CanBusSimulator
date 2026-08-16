// Veritabani baglantilarini ve sorgularini yoneten DbManager sinifinin tanimlamalarini icerir.

#ifndef DBMANAGER_H
#define DBMANAGER_H

#include <QSqlDatabase>
#include <QString>
#include <QObject>
#include <QSqlDriver>
#include <QVariant>

class DbManager : public QObject {
    Q_OBJECT
public:
    static DbManager& instance();
    bool connectToDatabase();
    bool transaction();
    bool commit();
    void logSignal(const QString& messageId, const QString& signalName, double physicalValue, const QString& rawHex);

signals:
    void commandReceived(const QString& commandName, const QString& commandValue);

private:
    DbManager();
    ~DbManager();
    QSqlDatabase db;

private slots:
    void onNotification(const QString& name, QSqlDriver::NotificationSource source, const QVariant& payload);
};

#endif // DBMANAGER_H


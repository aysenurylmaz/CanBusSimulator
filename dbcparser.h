// DBC dosyalarini okuyup anlamlandirmak icin kullanilan DbcParser sinifinin tanimlamalarini icerir.

// dbcparser.h
// Bu dosya, DBC Parser sinifinin (class) tanimini (basliklarini) icerir.
// DBC dosyasindaki verileri tutmak icin gerekli yapilari (struct) tanimlar.
#ifndef DBCPARSER_H
#define DBCPARSER_H

#include <QString>
#include <QMap>
#include <QList>

struct DbcSignal {
    QString name;
    int startBit;
    int length;
    bool isLittleEndian; // 1 = Little Endian (ntel), 0 = Big Endian (Motorola)
    bool isUnsigned;
    double factor;
    double offset;
    double min;
    double max;
    uint32_t messaged; // Which message this signal belongs to
};

struct DbcMessage {
    uint32_t id;
    QString name;
    int dlc; // Data length code in bytes
    QMap<QString, DbcSignal> msgSignals;
};

class DbcParser {
public:
    DbcParser();
    bool parseFile(const QString &filePath);
    
    // Auto-discover a signal by keywords
    DbcSignal findSignalByKeywords(const QStringList &keywords, bool &found) const;
    
    QMap<uint32_t, DbcMessage> getMessages() const;
    bool isEmpty() const;
    void exportToJson(const QString &jsonPath) const;

private:
    QMap<uint32_t, DbcMessage> m_messages;
};

#endif // DBCPARSER_H


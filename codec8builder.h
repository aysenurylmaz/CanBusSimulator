#ifndef CODEC8BUILDER_H
#define CODEC8BUILDER_H

#include <QByteArray>
#include <QDateTime>

// Teltonika cihazlari gibi davranarak CAN verilerini Codec 8 protokolune uygun paketleyen siniftir.
class Codec8Builder
{
public:
    Codec8Builder();

    // Verilen 8-byte payload'i ID 145 (veya istenen ID) ile Codec 8 formatina sokar.
    QByteArray buildPacket(const QByteArray& payload, quint16 propertyId = 145);

private:
    // Teltonika CRC-16 (IBM/ARC) algoritmasini hesaplar (Polynomial: 0xA001)
    quint16 calculateCrc16(const QByteArray& data);
    
    // Yardimci veri yazma fonksiyonlari (Big-Endian uyumlu)
    void appendUInt8(QByteArray& buffer, quint8 value);
    void appendUInt16(QByteArray& buffer, quint16 value);
    void appendUInt32(QByteArray& buffer, quint32 value);
    void appendUInt64(QByteArray& buffer, quint64 value);
};

#endif // CODEC8BUILDER_H

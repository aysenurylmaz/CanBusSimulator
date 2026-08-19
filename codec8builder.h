#ifndef CODEC8BUILDER_H
#define CODEC8BUILDER_H

#include <QByteArray>
#include <QDateTime>
#include <QMap>

// Teltonika FMC650 (ve benzeri) donanimlarin kullandigi "Codec 8" protokolunu
// C++ tarafinda simule etmemizi saglayan ana siniftir (Class).
// Amaci: Elimizdeki siradan 8-bytelik CAN (Controller Area Network) verisini,
// Teltonika sunucusunun anlayabilecegi resmi paket formatina (Preamble, Length, CRC) donusturmek.
class Codec8Builder
{
public:
    Codec8Builder();

    // Yeni nesil Coklu CAN destekleyen fonksiyon (Birden fazla 8-byte payload gonderebilir).
    QByteArray buildPacket(const QMap<quint8, QByteArray>& ioElements, double lat = 0.0, double lng = 0.0, double speed = 0.0);

    // Codec 8 Extended formatini destekleyen yeni fonksiyon (quint16 Property ID'leri kullanir)
    QByteArray buildExtendedPacket(const QMap<quint16, QByteArray>& ioElements, double lat = 0.0, double lng = 0.0, double speed = 0.0);

    // Geriye donuk uyumluluk (Kodu bozmamak icin tekil payload alan eski fonksiyon)
    QByteArray buildPacket(const QByteArray& payload, quint16 propertyId = 145, double lat = 0.0, double lng = 0.0, double speed = 0.0);

private:
    // CRC-16 (Cyclic Redundancy Check)
    // Paketin internet uzerinden giderken bozulup bozulmadigini anlamak icin kullanilan,
    // hata tespiti yapan matematiksel bir algoritmadir. Teltonika cihazlari "CRC-16/IBM"
    // veya "CRC-16/ARC" denilen, Polinomu 0xA001 olan ozel bir varyasyon kullanir.
    quint16 calculateCrc16(const QByteArray& data);
    
    // Asagidaki 4 fonksiyon, paket icerisine verileri Big-Endian (En anlamli byte'in en basta oldugu)
    // formatinda ardarda eklemek icin yazilmis yardimci (Helper) fonksiyonlardir.
    // Cunku ag (Network) haberlesmelerinde veriler standart olarak Big-Endian gonderilir.
    void appendUInt8(QByteArray& buffer, quint8 value);
    void appendUInt16(QByteArray& buffer, quint16 value);
    void appendUInt32(QByteArray& buffer, quint32 value);
    void appendUInt64(QByteArray& buffer, quint64 value);
};

#endif // CODEC8BUILDER_H

#ifndef CODEC8BUILDER_H
#define CODEC8BUILDER_H

#include <QByteArray>
#include <QDateTime>

// Teltonika FMC650 (ve benzeri) donanimlarin kullandigi "Codec 8" protokolunu
// C++ tarafinda simule etmemizi saglayan ana siniftir (Class).
// Amaci: Elimizdeki siradan 8-bytelik CAN (Controller Area Network) verisini,
// Teltonika sunucusunun anlayabilecegi resmi paket formatina (Preamble, Length, CRC) donusturmek.
class Codec8Builder
{
public:
    Codec8Builder();

    // Bu fonksiyon en onemli gorevi ustlenir. 
    // Disaridan aldigi ham 8-bytelik CAN verisini (payload) ve cihazdaki "Property ID" degerini (varsayilan 145)
    // kullanarak bastan asagi gecerli bir Codec 8 byte dizisi (QByteArray) olusturur.
    QByteArray buildPacket(const QByteArray& payload, quint16 propertyId = 145);

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

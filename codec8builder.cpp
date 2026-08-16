#include "codec8builder.h"

Codec8Builder::Codec8Builder()
{
}

QByteArray Codec8Builder::buildPacket(const QByteArray& payload, quint16 propertyId)
{
    // Teltonika paket yapisi 4 ana kisimdan olusur:
    // 1. Preamble (4 byte: 0x00000000)
    // 2. Data Field Length (4 byte)
    // 3. Codec ID (1 byte) + AVL Data (Verinin kendisi) + Number of Data 1 & 2
    // 4. CRC-16 (2 byte)

    QByteArray avlData;

    // ----------------------------------------------------
    // AVL DATA OLUSTURMA (Data Field kismidir)
    // ----------------------------------------------------
    
    // Codec ID = 0x08 (Teltonika Codec 8 standardi)
    appendUInt8(avlData, 0x08);

    // Number of Data 1 (1 adet kayit var)
    appendUInt8(avlData, 0x01);

    // Timestamp (8 byte - Milisaniye cinsinden UTC zamani)
    quint64 currentTimestamp = QDateTime::currentMSecsSinceEpoch();
    appendUInt64(avlData, currentTimestamp);

    // Priority (1 byte - 0: Low, 1: High, 2: Panic)
    appendUInt8(avlData, 0x00);

    // GPS Element (15 byte: Long, Lat, Alt, Angle, Sats, Speed)
    // Simdilik sirket merkezimizin Dummy koordinatlarini yaziyoruz.
    // Longitude ve Latitude degerleri gercek degerlerin 10^7 ile carpilmis halidir.
    appendUInt32(avlData, 289784000); // Ornek Longitude (28.9784)
    appendUInt32(avlData, 410082000); // Ornek Latitude (41.0082)
    appendUInt16(avlData, 100);       // Altitude (100 metre)
    appendUInt16(avlData, 0);         // Angle (0 derece)
    appendUInt8(avlData, 15);         // Satellites (15 uydu)
    appendUInt16(avlData, 0);         // Speed (0 km/h)

    // IO Elements Bloğu (Manuel CAN veya I/O parametreleri)
    // Event IO ID (1 byte - hangi ID logu tetikledi, 0 = hicbiri)
    appendUInt8(avlData, 0x00);
    
    // Total IO Elements Count (1 byte)
    appendUInt8(avlData, 0x01); // 1 tane IO yolluyoruz.

    // 1-Byte IO Elements Count
    appendUInt8(avlData, 0x00);
    // 2-Byte IO Elements Count
    appendUInt8(avlData, 0x00);
    // 4-Byte IO Elements Count
    appendUInt8(avlData, 0x00);
    
    // 8-Byte IO Elements Count
    appendUInt8(avlData, 0x01); 

    // ID degeri Codec 8'de 1 bytetir (1-255). Ext(Codec8Ext) olsaydi 2 byte olurdu.
    // Biz standart Codec8 kullandigimiz icin ID'yi 1 Byte (0-255) kabul ediyoruz.
    appendUInt8(avlData, static_cast<quint8>(propertyId));

    // 8 bytelik veriyi direkt kopyalayarak yapiştir. 
    // Eger 8'den kucukse sifirlarla doldur.
    QByteArray finalPayload = payload;
    while(finalPayload.size() < 8) {
        finalPayload.append('\0');
    }
    avlData.append(finalPayload.left(8));

    // Number of Data 2 (Kontrol amacli, bastakiyle ayni olmali)
    appendUInt8(avlData, 0x01);

    // ----------------------------------------------------
    // TUM PAKETI BIRLESTIRME (Preamble + Length + Data + CRC)
    // ----------------------------------------------------
    QByteArray finalPacket;

    // 1. Preamble (4 byte: 0x00000000)
    appendUInt32(finalPacket, 0x00000000);

    // 2. Data Field Length (4 byte)
    // Uzunluk; CodecID + NumberData1 + AVL Data + NumberData2 toplamina esittir.
    appendUInt32(finalPacket, static_cast<quint32>(avlData.size()));

    // 3. AVL Data Bloğunun kendisi
    finalPacket.append(avlData);

    // 4. CRC-16 (2 byte)
    // Teltonika cihazlari CRC algoritmasini Preamble ve Data Length HARIC tum AVL blogu uzerinden hesaplar.
    quint16 crc = calculateCrc16(avlData);
    appendUInt32(finalPacket, static_cast<quint32>(crc)); // CRC 4 byte alan kaplar ama ilk 2 byte 0'dir (Big Endian) 00 00 CR C1

    return finalPacket;
}

// Teltonika'nin standart hesaplama algoritmasidir (CRC-16/IBM - Polynomial 0xA001)
quint16 Codec8Builder::calculateCrc16(const QByteArray& data)
{
    quint16 crc = 0;
    for (int i = 0; i < data.size(); ++i)
    {
        crc ^= static_cast<quint8>(data[i]);
        for (int j = 0; j < 8; ++j)
        {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xA001;
            else
                crc >>= 1;
        }
    }
    return crc;
}

void Codec8Builder::appendUInt8(QByteArray& buffer, quint8 value)
{
    buffer.append(static_cast<char>(value));
}

void Codec8Builder::appendUInt16(QByteArray& buffer, quint16 value)
{
    buffer.append(static_cast<char>((value >> 8) & 0xFF));
    buffer.append(static_cast<char>(value & 0xFF));
}

void Codec8Builder::appendUInt32(QByteArray& buffer, quint32 value)
{
    buffer.append(static_cast<char>((value >> 24) & 0xFF));
    buffer.append(static_cast<char>((value >> 16) & 0xFF));
    buffer.append(static_cast<char>((value >> 8) & 0xFF));
    buffer.append(static_cast<char>(value & 0xFF));
}

void Codec8Builder::appendUInt64(QByteArray& buffer, quint64 value)
{
    buffer.append(static_cast<char>((value >> 56) & 0xFF));
    buffer.append(static_cast<char>((value >> 48) & 0xFF));
    buffer.append(static_cast<char>((value >> 40) & 0xFF));
    buffer.append(static_cast<char>((value >> 32) & 0xFF));
    buffer.append(static_cast<char>((value >> 24) & 0xFF));
    buffer.append(static_cast<char>((value >> 16) & 0xFF));
    buffer.append(static_cast<char>((value >> 8) & 0xFF));
    buffer.append(static_cast<char>(value & 0xFF));
}

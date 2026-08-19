#include "codec8builder.h"

Codec8Builder::Codec8Builder()
{
}

QByteArray Codec8Builder::buildPacket(const QMap<quint8, QByteArray>& ioElements, double lat, double lng, double speed)
{
    // Teltonika Codec 8 paket mimarisi 4 ana asmadan olusur:
    // 1. Preamble (4 byte)          : Paketin baslangicini belirten sifirlar (0x00000000).
    // 2. Data Field Length (4 byte) : Codec ID'den baslayip CRC'ye kadar olan (CRC haric) kilit verinin uzunlugu.
    // 3. Data Field (Degisken)      : Codec ID + AVL Data + Veri Sayisi. Isin gercek kalbi burasidir.
    // 4. CRC-16 (2 byte)            : Veri Field uzerinden hesaplanan hata kontrol kodu.

    QByteArray avlData;

    // ----------------------------------------------------
    // ADIM 1: AVL DATA (DATA FIELD) OLUSTURMA
    // ----------------------------------------------------
    
    // Codec ID = 0x08
    appendUInt8(avlData, 0x08);

    // Number of Data 1 (Kac tane kayit var?)
    appendUInt8(avlData, 0x01);

    // Timestamp (8 byte)
    quint64 currentTimestamp = QDateTime::currentMSecsSinceEpoch();
    appendUInt64(avlData, currentTimestamp);

    // Priority (1 byte)
    appendUInt8(avlData, 0x00);

    // GPS Elementleri (Toplam 15 byte zorunlu)
    appendUInt32(avlData, static_cast<quint32>(lng * 10000000.0)); // Guncel Longitude (x 10^7)
    appendUInt32(avlData, static_cast<quint32>(lat * 10000000.0)); // Guncel Latitude  (x 10^7)
    appendUInt16(avlData, 100);       // Altitude
    appendUInt16(avlData, 0);         // Angle
    appendUInt8(avlData, 15);         // Satellites
    appendUInt16(avlData, static_cast<quint16>(speed)); // Speed

    // IO Elements Blogu
    // Event IO ID (1 byte)
    appendUInt8(avlData, 0x00);
    
    // Total IO Elements Count (1 byte)
    appendUInt8(avlData, static_cast<quint8>(ioElements.size())); 

    // Codec 8 standardinda veriler boyutuna gore gruplanir (1-byte, 2-byte, 4-byte, 8-byte).
    appendUInt8(avlData, 0x00); // 1-Byte veri sayisi: 0
    appendUInt8(avlData, 0x00); // 2-Byte veri sayisi: 0
    appendUInt8(avlData, 0x00); // 4-Byte veri sayisi: 0
    
    // 8-Byte veri sayisi
    appendUInt8(avlData, static_cast<quint8>(ioElements.size())); 

    // Dongu ile tum 8-bytelik IO'lari (CAN mesajlarini) ekle
    for (auto it = ioElements.begin(); it != ioElements.end(); ++it) {
        // ID degeri (Property ID)
        appendUInt8(avlData, it.key());

        // CAN Payload'ini kopyala (Tam 8 byte olmak zorundadir)
        QByteArray finalPayload = it.value();
        if (finalPayload.size() < 8) {
            finalPayload.resize(8); // Eksik kisimlari 0x00 (null byte) ile doldurur.
        }
        avlData.append(finalPayload.left(8));
    }

    // Number of Data 2
    appendUInt8(avlData, 0x01);

    // ----------------------------------------------------
    // ADIM 2: TUM PAKETI BIRLESTIRME (Preamble + Length + Data + CRC)
    // ----------------------------------------------------
    QByteArray finalPacket;

    // 1. Preamble (4 byte: 0x00000000)
    appendUInt32(finalPacket, 0x00000000);

    // 2. Data Field Length (4 byte)
    appendUInt32(finalPacket, static_cast<quint32>(avlData.size()));

    // 3. AVL Data Blogunun kendisi
    finalPacket.append(avlData);

    // 4. CRC-16 Hesaplamasi ve Eklenmesi (2 byte)
    quint16 crc = calculateCrc16(avlData);
    appendUInt32(finalPacket, static_cast<quint32>(crc)); 

    return finalPacket;
}

QByteArray Codec8Builder::buildPacket(const QByteArray& payload, quint16 propertyId, double lat, double lng, double speed)
{
    QMap<quint8, QByteArray> ioElements;
    ioElements[static_cast<quint8>(propertyId)] = payload;
    return buildPacket(ioElements, lat, lng, speed);
}

QByteArray Codec8Builder::buildExtendedPacket(const QMap<quint16, QByteArray>& ioElements, double lat, double lng, double speed)
{
    QByteArray avlData;

    // Codec ID = 0x8E (Codec 8 Extended)
    appendUInt8(avlData, 0x8E);

    // Number of Data 1
    appendUInt8(avlData, 0x01);

    // Timestamp (8 byte)
    quint64 currentTimestamp = QDateTime::currentMSecsSinceEpoch();
    appendUInt64(avlData, currentTimestamp);

    // Priority (1 byte)
    appendUInt8(avlData, 0x00);

    // GPS Elementleri (Toplam 15 byte zorunlu)
    appendUInt32(avlData, static_cast<quint32>(lng * 10000000.0));
    appendUInt32(avlData, static_cast<quint32>(lat * 10000000.0));
    appendUInt16(avlData, 100);
    appendUInt16(avlData, 0);
    appendUInt8(avlData, 15);
    appendUInt16(avlData, static_cast<quint16>(speed));

    // IO Elements Blogu - Codec 8 Extended
    // Event IO ID (2 bytes)
    appendUInt16(avlData, 0x0000);
    
    // Total IO Elements Count (2 bytes)
    appendUInt16(avlData, static_cast<quint16>(ioElements.size())); 

    // N of 1-Byte (2 bytes)
    appendUInt16(avlData, 0x0000);
    // N of 2-Byte (2 bytes)
    appendUInt16(avlData, 0x0000);
    // N of 4-Byte (2 bytes)
    appendUInt16(avlData, 0x0000);
    // N of 8-Byte (2 bytes)
    appendUInt16(avlData, static_cast<quint16>(ioElements.size())); 

    for (auto it = ioElements.begin(); it != ioElements.end(); ++it) {
        // ID degeri (Property ID) -> 2 bytes in Extended
        appendUInt16(avlData, it.key());

        QByteArray finalPayload = it.value();
        if (finalPayload.size() < 8) {
            finalPayload.resize(8);
        }
        avlData.append(finalPayload.left(8));
    }

    // N of X-Byte (2 bytes)
    appendUInt16(avlData, 0x0000); 

    // Number of Data 2
    appendUInt8(avlData, 0x01);

    QByteArray finalPacket;
    appendUInt32(finalPacket, 0x00000000);
    appendUInt32(finalPacket, static_cast<quint32>(avlData.size()));
    finalPacket.append(avlData);
    quint16 crc = calculateCrc16(avlData);
    appendUInt32(finalPacket, static_cast<quint32>(crc)); 

    return finalPacket;
}


// ----------------------------------------------------
// CRC-16 (IBM/ARC) ALGORITMASI (POLYNOMIAL: 0xA001)
// ----------------------------------------------------
// Bu fonksiyon Teltonika'nin resmi dokumanlarindaki C/C++ algoritmasinin birebir aynisidir.
// Gonderilen byte'lari tek tek isleyip (XOR ve Shift operatorleri ile) 16-bit'lik (2 byte) 
// essiz bir dogrulama kodu uretir. Sunucu, paketi alinca ayni islemi kendi yapar ve 
// iki CRC eslesiyorsa "Bu paket yolda bozulmamis" der ve kabul eder.
quint16 Codec8Builder::calculateCrc16(const QByteArray& data)
{
    quint16 crc = 0; // Baslangic degeri sifir
    for (int i = 0; i < data.size(); ++i)
    {
        // Byte'i alip CRC ile XOR'luyoruz
        crc ^= static_cast<quint8>(data[i]);
        
        // Her byte 8 bit oldugu icin 8 defa dondur (Shift right islemi)
        for (int j = 0; j < 8; ++j)
        {
            if (crc & 1) // Eger en sagdaki bit 1 ise
                crc = (crc >> 1) ^ 0xA001; // 0xA001 (Polinom) ile XOR'la
            else
                crc >>= 1; // Degilse sadece 1 bit saga kaydir
        }
    }
    return crc;
}

// ----------------------------------------------------
// YARDIMCI FONKSIYONLAR (BIG-ENDIAN BYTE EKLEME)
// ----------------------------------------------------
// C++'ta (Windows/Intel islemcilerde) sayilar hafizada Little-Endian (ters) durur.
// Ama Ag (Network/TCP) haberlesmelerinde sayilarin Big-Endian (duz/dogal) sira ile 
// gonderilmesi dunya standardidir. Bu fonksiyonlar C++'in ters tuttuğu sayilari
// byte byte dogru sirayla (en anlamlidan baslayarak) QByteArray icerisine diler.

void Codec8Builder::appendUInt8(QByteArray& buffer, quint8 value)
{
    // Tek byte oldugu icin siralamanin onemi yok, direkt yapiştir.
    buffer.append(static_cast<char>(value));
}

void Codec8Builder::appendUInt16(QByteArray& buffer, quint16 value)
{
    // 2 Bytelik veriyi once ilk yarisi (>> 8), sonra ikinci yarisi (& 0xFF) olacak sekilde ekler.
    buffer.append(static_cast<char>((value >> 8) & 0xFF));
    buffer.append(static_cast<char>(value & 0xFF));
}

void Codec8Builder::appendUInt32(QByteArray& buffer, quint32 value)
{
    // 4 Bytelik veri, en buyuk byte'tan en kucuge dogru tek tek cikarilarak dizilir.
    buffer.append(static_cast<char>((value >> 24) & 0xFF));
    buffer.append(static_cast<char>((value >> 16) & 0xFF));
    buffer.append(static_cast<char>((value >> 8) & 0xFF));
    buffer.append(static_cast<char>(value & 0xFF));
}

void Codec8Builder::appendUInt64(QByteArray& buffer, quint64 value)
{
    // 8 Bytelik veri (ornegin Timestamp), ayni sekilde 56 bit (7 byte) kaydirmadan baslayarak
    // sifira kadar sirayla parcalanip eklenir.
    buffer.append(static_cast<char>((value >> 56) & 0xFF));
    buffer.append(static_cast<char>((value >> 48) & 0xFF));
    buffer.append(static_cast<char>((value >> 40) & 0xFF));
    buffer.append(static_cast<char>((value >> 32) & 0xFF));
    buffer.append(static_cast<char>((value >> 24) & 0xFF));
    buffer.append(static_cast<char>((value >> 16) & 0xFF));
    buffer.append(static_cast<char>((value >> 8) & 0xFF));
    buffer.append(static_cast<char>(value & 0xFF));
}

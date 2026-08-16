#include "codec8builder.h"

Codec8Builder::Codec8Builder()
{
}

QByteArray Codec8Builder::buildPacket(const QByteArray& payload, quint16 propertyId, double lat, double lng, double speed)
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
    // Teltonika cihazlari bircok codec destekler (Codec 8, Codec 8 Extended, Codec 16 vs.)
    // Biz standart Codec 8 (0x08) kullaniyoruz.
    appendUInt8(avlData, 0x08);

    // Number of Data 1 (Kac tane kayit var?)
    // Ayni paket icinde birden fazla log gonderebiliriz. Biz tek bir CAN mesaji yolladigimiz icin 1 yaziyoruz.
    appendUInt8(avlData, 0x01);

    // Timestamp (8 byte)
    // Olayin (logun) tam olarak ne zaman gerceklestigi. 
    // UTC saat diliminde ve milisaniye (ms) cinsinden hesaplanir.
    quint64 currentTimestamp = QDateTime::currentMSecsSinceEpoch();
    appendUInt64(avlData, currentTimestamp);

    // Priority (1 byte)
    // 0: Low (Dusuk Oncelik)
    // 1: High (Yuksek Oncelik)
    // 2: Panic (Panik/Acil Durum Onceligi)
    // CAN mesajlari genelde rutin veri oldugu icin Low (0x00) seciyoruz.
    appendUInt8(avlData, 0x00);

    // GPS Elementleri (Toplam 15 byte zorunlu)
    // Gonderilen her verinin yaninda cihaz o anki GPS konumunu da yollar.
    // Longitude ve Latitude degerleri gercek derece degerlerinin 10.000.000 (10^7) ile carpilmis halidir.
    // Teltonika cihazi koordinatlari integer (tamsayi) formatinda kabul eder.
    appendUInt32(avlData, static_cast<quint32>(lng * 10000000.0)); // Guncel Longitude (x 10^7)
    appendUInt32(avlData, static_cast<quint32>(lat * 10000000.0)); // Guncel Latitude  (x 10^7)
    appendUInt16(avlData, 100);       // Altitude (Deniz seviyesinden yukseklik - varsayilan 100 metre)
    appendUInt16(avlData, 0);         // Angle (Yon acisi - 0 derece)
    appendUInt8(avlData, 15);         // Satellites (Kac uyduya bagli - 15 uydu varsayilan)
    appendUInt16(avlData, static_cast<quint16>(speed)); // Speed (Aracin hizi - km/h)

    // IO Elements Blogu (Burasi Manual CAN verilerimizin girdigi yerdir)
    // IO demek (Input/Output), sensor veya CAN verileri demek.
    
    // Event IO ID (1 byte)
    // Bu paketin olusmasina hangi parametre sebep oldu? 0 verisek sadece periyodik log demektir.
    appendUInt8(avlData, 0x00);
    
    // Total IO Elements Count (1 byte)
    // Toplamda kac tane parametre (IO) yolluyoruz? Cevap: Sadece 1 tane (O da bizim 8-bytelik CAN)
    appendUInt8(avlData, 0x01); 

    // Codec 8 standardinda veriler boyutuna gore gruplanir (1-byte, 2-byte, 4-byte, 8-byte).
    // Bizim CAN mesajimiz (payload) tam 8 byte oldugu icin, ilk gruplari 0 geciyoruz.
    appendUInt8(avlData, 0x00); // 1-Byte veri sayisi: 0
    appendUInt8(avlData, 0x00); // 2-Byte veri sayisi: 0
    appendUInt8(avlData, 0x00); // 4-Byte veri sayisi: 0
    
    // 8-Byte veri sayisi: 1 (İste bizim verimiz burada basliyor)
    appendUInt8(avlData, 0x01); 

    // ID degeri (Property ID)
    // Teltonika tarafinda bu verinin ne anlama geldigini (ornek: Motor Devri mi? Yag sicakligi mi?)
    // bu ID (145 vb.) belirler. Codec 8'de ID degeri 1 Byte sinirlidir (0-255 arasi).
    appendUInt8(avlData, static_cast<quint8>(propertyId));

    // CAN Payload'ini kopyala (Tam 8 byte olmak zorundadir)
    // Eger 8'den kucukse eksik kismi sifirlarla (0x00) dolduruyoruz ki sunucu hata vermesin.
    QByteArray finalPayload = payload;
    while(finalPayload.size() < 8) {
        finalPayload.append('\0');
    }
    // Geri kalan kismi kesip tam 8 bytelik kismi ekliyoruz.
    avlData.append(finalPayload.left(8));

    // Number of Data 2
    // En basta belirttigimiz Number of Data 1 ile birebir AYNISI olmak zorundadir.
    // Teltonika sunucusu bu iki degeri karsilastirarak pakette kayip var mi diye bakar.
    appendUInt8(avlData, 0x01);

    // ----------------------------------------------------
    // ADIM 2: TUM PAKETI BIRLESTIRME (Preamble + Length + Data + CRC)
    // ----------------------------------------------------
    QByteArray finalPacket;

    // 1. Preamble (4 byte: 0x00000000) (Zorunlu Baslangic İnsasi)
    appendUInt32(finalPacket, 0x00000000);

    // 2. Data Field Length (4 byte)
    // Data blogunun byte cinsinden uzunlugunu ekliyoruz (avlData dizisinin boyutu).
    appendUInt32(finalPacket, static_cast<quint32>(avlData.size()));

    // 3. AVL Data Blogunun kendisi
    finalPacket.append(avlData);

    // 4. CRC-16 Hesaplamasi ve Eklenmesi (2 byte)
    // Dikkat: CRC sadece Data Field (avlData) uzerinden hesaplanir. 
    // Preamble ve Data Length bu hesaba DAHIL EDILMEZ.
    quint16 crc = calculateCrc16(avlData);
    
    // Codec 8 formatinda CRC degeri 4 byte'lik bir alan kaplar ancak degeri 2 Byte (CRC-16)'dir.
    // Bu yuzden 4 byte alan ayirip, ilk iki byte'ini 00 00 seklinde bos geciyoruz. (Big-Endian sebebiyle)
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

// CAN veriyolu (DBC) dosyalarini parse eden (ayristiran), sinyal/mesaj kurallarini cikartan asil ayristirici mantik dosyasidir.

// dbcparser.cpp
// Bu dosya, yuklenen DBC dosyalarini satir satir okuyan (parse eden) asil motor kismidir.
// Karmasik Regex (Duzenli Ifadeler) kullanarak metin icindeki verileri ayiklar.
#include "dbcparser.h"
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

DbcParser::DbcParser() {
}

bool DbcParser::parseFile(const QString &filePath) {
    QFile file(filePath);
    // Dosyayi 'Sadece Okunabilir' (ReadOnly) ve 'Metin' (Text) modunda acmaya calisiyoruz.
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "Failed to open DBC file:" << filePath;
        return false;
    }
    // Yeni bir dosya yukleniyorsa, hafizadaki eski DBC mesajlarini temizliyoruz.
    m_messages.clear();
    // Dosyayi satir satir okuyabilmek icin bir metin akisi (stream) olusturuyoruz
    QTextStream in(&file);

    // Regex for BO_ (Message)
    // BO_ satirlari, bir CAN mesajinin Kimligini (ID) ve Uzunlugunu (DLC) belirtir.
    // Regex Aciklamasi:
    // ^\\s*BO_    : Satir basindaki bosluklari atla ve "BO_" kelimesini bul.
    // (\\d+)      : 1. Yakalama Grubu -> Mesaj ID'si (Sadece rakamlari al)
    // (\\w+)      : 2. Yakalama Grubu -> Mesaj Adi (Harf ve rakamlari al)
    // (\\d+)      : 3. Yakalama Grubu -> Veri Uzunlugu / DLC (Sadece rakamlari al)
    QRegularExpression boRegex("^\\s*BO_\\s+(\\d+)\\s+(\\w+)\\s*:\\s+(\\d+)\\s+.*");

    // Regex for SG_ (Signal)
    // Regex Aciklamasi (Sirasiyla (...) icindeki Yakalama Gruplari):
    // 1: Sinyal Adi (\\w+)
    // 2: Baslangic Biti (\\d+)
    // 3: Uzunluk/BitLength (\\d+)
    // 4: Endianness - "1" (Little) veya "0" (Big) ([01])
    // 5: Isaretli/Isaretsiz - "+" (Unsigned) veya "-" (Signed) ([\\+\\-])
    // 6: Carpan / Factor ([0-9\\.\\-]+)
    // 7: Offset ([0-9\\.\\-]+)
    // 8: Minimum Deger
    // 9: Maksimum Deger
    QRegularExpression sgRegex("^\\s*SG_\\s+(\\w+).*?:\\s+(\\d+)\\|(\\d+)@([01])([\\+\\-])\\s+\\(([0-9\\.\\-]+),([0-9\\.\\-]+)\\)\\s+\\[([0-9\\.\\-]+)\\|([0-9\\.\\-]+)\\]");
    
    // Okudugumuz sinyallerin (SG_) hangi ebeveyn mesaja (BO_) ait oldugunu bilmek icin 
    // ID'yi dongu disinda gecici bir degiskende tutuyoruz.
    uint32_t currentMessageId = 0;
    
    // Dosyanin sonuna gelene kadar (atEnd) satir satir okumaya devam et.
    while (!in.atEnd()) {
        QString line = in.readLine();

        // 1. KONTROL: Bu satir bir BO_ (Mesaj) satiri mi?
        QRegularExpressionMatch boMatch = boRegex.match(line);
        if (boMatch.hasMatch()) {
            DbcMessage msg;

            // Regex icindeki parantezlerle (...) yakaladigimiz verileri index numarasiyla cekiyoruz.
            msg.id = boMatch.captured(1).toUInt(); // 1. parantez: ID (unsigned integer'a ceviriyoruz)
            msg.name = boMatch.captured(2); // 2. parantez: Isim (String olarak kaliyor)
            msg.dlc = boMatch.captured(3).toInt(); // 3. parantez: Veri Uzunlugu (DLC)
            
            currentMessageId = msg.id; // Alt satirlarda gelecek sinyaller bu mesaja ait olacak.

            // Mesaji hafizadaki listemize (QMap) ID'sini anahtar (key) yaparak ekliyoruz.
            m_messages.insert(msg.id, msg);
            qDebug() << "Parsed BO_:" << msg.id << msg.name << "DLC:" << msg.dlc;
            continue;
        }
        // 2. KONTROL: Bu satir bir SG_ (Sinyal) satiri mi?
        // Ve oncesinde gecerli bir Mesaj (currentMessageId) bulduk mu?
        QRegularExpressionMatch sgMatch = sgRegex.match(line);
        if (sgMatch.hasMatch() && currentMessageId != 0) {
            DbcSignal sig;
            // Regex'teki 9 farkli grubu sirasiyla struct icindeki degiskenlere aktariyoruz.
            sig.name = sgMatch.captured(1);
            sig.startBit = sgMatch.captured(2).toInt();
            sig.length = sgMatch.captured(3).toInt();
            sig.isLittleEndian = (sgMatch.captured(4) == "1"); // Endianness kontrolu: "1" e esitse Little-Endian (Intel), degilse Big-Endian (Motorola)
            sig.isUnsigned = (sgMatch.captured(5) == "+"); // Isaret kontrolu: "+" ise Unsigned (Sadece pozitif), "-" ise Signed (Negatif deger de alabilir)
            // Carpan, Offset, Min, Max (Ondalikli sayi icerebilecekleri icin double'a ceviriyoruz)
            sig.factor = sgMatch.captured(6).toDouble();
            sig.offset = sgMatch.captured(7).toDouble();
            sig.min = sgMatch.captured(8).toDouble();
            sig.max = sgMatch.captured(9).toDouble();
            sig.messageId = currentMessageId; // Bu sinyalin hangi ebeveyn mesaja ait oldugunu kaydediyoruz.

            m_messages[currentMessageId].msgSignals.insert(sig.name, sig); // Buldugumuz sinyali, ilgili mesajin kendi icindeki sinyal listesine dahil ediyoruz.
            qDebug() << "  Parsed SG_:" << sig.name << "StartBit:" << sig.startBit << "Length:" << sig.length << "LittleEndian:" << sig.isLittleEndian;
        }
    }

    file.close();
    
    // Senkronizasyon icin dosyayi JSON formatinda Web'in okudugu Shared klasorune aktariyoruz
    exportToJson("C:/Projeler/CanBusWebPlatform/Shared/parsed_dbc.json");
    
    return true;
}

void DbcParser::exportToJson(const QString &jsonPath) const {
    QJsonArray jsonArray;
    
    for (auto msgIt = m_messages.begin(); msgIt != m_messages.end(); ++msgIt) {
        const DbcMessage &msg = msgIt.value();
        QJsonObject msgObj;
        
        // JSON'da ID hex olarak tutulur (Orn: "0x1F4")
        msgObj["id"] = QString("0x%1").arg(msg.id, 0, 16);
        msgObj["name"] = msg.name;
        msgObj["dlc"] = msg.dlc;
        
        QJsonArray signalsArray;
        for (auto sigIt = msg.msgSignals.begin(); sigIt != msg.msgSignals.end(); ++sigIt) {
            const DbcSignal &sig = sigIt.value();
            QJsonObject sigObj;
            sigObj["name"] = sig.name;
            sigObj["startBit"] = sig.startBit;
            sigObj["length"] = sig.length;
            sigObj["factor"] = sig.factor;
            sigObj["offset"] = sig.offset;
            sigObj["min"] = sig.min;
            sigObj["max"] = sig.max;
            signalsArray.append(sigObj);
        }
        
        msgObj["signals"] = signalsArray;
        jsonArray.append(msgObj);
    }
    
    QJsonDocument doc(jsonArray);
    QFile file(jsonPath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        file.write(doc.toJson());
        file.close();
    } else {
        qDebug() << "Failed to export JSON to:" << jsonPath;
    }
}

DbcSignal DbcParser::findSignalByKeywords(const QStringList &keywords, bool &found) const {
    found = false;
    
    // First pass: look for exact matches in the order of keywords
    for (const QString &keyword : keywords) {
        for (auto msgIt = m_messages.begin(); msgIt != m_messages.end(); ++msgIt) {
            for (auto sigIt = msgIt.value().msgSignals.begin(); sigIt != msgIt.value().msgSignals.end(); ++sigIt) {
                if (sigIt.value().name.compare(keyword, Qt::CaseInsensitive) == 0) {
                    found = true;
                    return sigIt.value();
                }
            }
        }
    }

    // Second pass: look for partial matches in the order of keywords
    for (const QString &keyword : keywords) {
        for (auto msgIt = m_messages.begin(); msgIt != m_messages.end(); ++msgIt) {
            for (auto sigIt = msgIt.value().msgSignals.begin(); sigIt != msgIt.value().msgSignals.end(); ++sigIt) {
                if (sigIt.value().name.contains(keyword, Qt::CaseInsensitive)) {
                    found = true;
                    return sigIt.value();
                }
            }
        }
    }
    
    return DbcSignal();
}
// OOP (Nesne Yonelimli Programlama) - Encapsulation (Kapsulleme) geregi:
// Disaridan, sinifin gizli (private) uyesi olan 'm_messages' listesine 
// dis mudahaleyi engellemek icin sadece 'okuma' amacli bir erisim fonksiyonu (Getter).
QMap<uint32_t, DbcMessage> DbcParser::getMessages() const {
    return m_messages;
}
// DBC dosyasindan hicbir veri cikarilamadiysa veya henuz dosya yuklenmediyse durumu kontrol eden yardimci fonksiyon.
bool DbcParser::isEmpty() const {
    return m_messages.isEmpty();
}


// CAN veriyolu (DBC) dosyalar�n� parse eden (ayr��t�ran), sinyal/mesaj kurallar�n� ��kartan as�l ayr��t�r�c� mant�k dosyas�d�r.

// dbcparser.cpp
// Bu dosya, yüklenen DBC dosyalarını satır satır okuyan (parse eden) asıl motor kısmıdır.
// Karmaşık Regex (Düzenli İfadeler) kullanarak metin içindeki verileri ayıklar.
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
    // Dosyayı 'Sadece Okunabilir' (ReadOnly) ve 'Metin' (Text) modunda açmaya çalışıyoruz.
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "Failed to open DBC file:" << filePath;
        return false;
    }
    // Yeni bir dosya yükleniyorsa, hafızadaki eski DBC mesajlarını temizliyoruz.
    m_messages.clear();
    // Dosyayı satır satır okuyabilmek için bir metin akışı (stream) oluşturuyoruz
    QTextStream in(&file);

    // Regex for BO_ (Message)
    // BO_ satırları, bir CAN mesajının Kimliğini (ID) ve Uzunluğunu (DLC) belirtir.
    // Regex Açıklaması:
    // ^\\s*BO_    : Satır başındaki boşlukları atla ve "BO_" kelimesini bul.
    // (\\d+)      : 1. Yakalama Grubu -> Mesaj ID'si (Sadece rakamları al)
    // (\\w+)      : 2. Yakalama Grubu -> Mesaj Adı (Harf ve rakamları al)
    // (\\d+)      : 3. Yakalama Grubu -> Veri Uzunluğu / DLC (Sadece rakamları al)
    QRegularExpression boRegex("^\\s*BO_\\s+(\\d+)\\s+(\\w+)\\s*:\\s+(\\d+)\\s+.*");

    // Regex for SG_ (Signal)
    // Regex Açıklaması (Sırasıyla (...) içindeki Yakalama Grupları):
    // 1: Sinyal Adı (\\w+)
    // 2: Başlangıç Biti (\\d+)
    // 3: Uzunluk/BitLength (\\d+)
    // 4: Endianness - "1" (Little) veya "0" (Big) ([01])
    // 5: İşaretli/İşaretsiz - "+" (Unsigned) veya "-" (Signed) ([\\+\\-])
    // 6: Çarpan / Factor ([0-9\\.\\-]+)
    // 7: Offset ([0-9\\.\\-]+)
    // 8: Minimum Değer
    // 9: Maksimum Değer
    QRegularExpression sgRegex("^\\s*SG_\\s+(\\w+).*?:\\s+(\\d+)\\|(\\d+)@([01])([\\+\\-])\\s+\\(([0-9\\.\\-]+),([0-9\\.\\-]+)\\)\\s+\\[([0-9\\.\\-]+)\\|([0-9\\.\\-]+)\\]");
    
    // Okuduğumuz sinyallerin (SG_) hangi ebeveyn mesaja (BO_) ait olduğunu bilmek için 
    // ID'yi döngü dışında geçici bir değişkende tutuyoruz.
    uint32_t currentMessageId = 0;
    
    // Dosyanın sonuna gelene kadar (atEnd) satır satır okumaya devam et.
    while (!in.atEnd()) {
        QString line = in.readLine();

        // 1. KONTROL: Bu satır bir BO_ (Mesaj) satırı mı?
        QRegularExpressionMatch boMatch = boRegex.match(line);
        if (boMatch.hasMatch()) {
            DbcMessage msg;

            // Regex içindeki parantezlerle (...) yakaladığımız verileri index numarasıyla çekiyoruz.
            msg.id = boMatch.captured(1).toUInt(); // 1. parantez: ID (unsigned integer'a çeviriyoruz)
            msg.name = boMatch.captured(2); // 2. parantez: İsim (String olarak kalıyor)
            msg.dlc = boMatch.captured(3).toInt(); // 3. parantez: Veri Uzunluğu (DLC)
            
            currentMessageId = msg.id; // Alt satırlarda gelecek sinyaller bu mesaja ait olacak.

            // Mesajı hafızadaki listemize (QMap) ID'sini anahtar (key) yaparak ekliyoruz.
            m_messages.insert(msg.id, msg);
            qDebug() << "Parsed BO_:" << msg.id << msg.name << "DLC:" << msg.dlc;
            continue;
        }
        // 2. KONTROL: Bu satır bir SG_ (Sinyal) satırı mı?
        // Ve öncesinde geçerli bir Mesaj (currentMessageId) bulduk mu?
        QRegularExpressionMatch sgMatch = sgRegex.match(line);
        if (sgMatch.hasMatch() && currentMessageId != 0) {
            DbcSignal sig;
            // Regex'teki 9 farklı grubu sırasıyla struct içindeki değişkenlere aktarıyoruz.
            sig.name = sgMatch.captured(1);
            sig.startBit = sgMatch.captured(2).toInt();
            sig.length = sgMatch.captured(3).toInt();
            sig.isLittleEndian = (sgMatch.captured(4) == "1"); // Endianness kontrolü: "1" e eşitse Little-Endian (Intel), değilse Big-Endian (Motorola)
            sig.isUnsigned = (sgMatch.captured(5) == "+"); // İşaret kontrolü: "+" ise Unsigned (Sadece pozitif), "-" ise Signed (Negatif değer de alabilir)
            // Çarpan, Offset, Min, Max (Ondalıklı sayı içerebilecekleri için double'a çeviriyoruz)
            sig.factor = sgMatch.captured(6).toDouble();
            sig.offset = sgMatch.captured(7).toDouble();
            sig.min = sgMatch.captured(8).toDouble();
            sig.max = sgMatch.captured(9).toDouble();
            sig.messageId = currentMessageId; // Bu sinyalin hangi ebeveyn mesaja ait olduğunu kaydediyoruz.

            m_messages[currentMessageId].msgSignals.insert(sig.name, sig); // Bulduğumuz sinyali, ilgili mesajın kendi içindeki sinyal listesine dahil ediyoruz.
            qDebug() << "  Parsed SG_:" << sig.name << "StartBit:" << sig.startBit << "Length:" << sig.length << "LittleEndian:" << sig.isLittleEndian;
        }
    }

    file.close();
    
    // Senkronizasyon için dosyayı JSON formatında Web'in okuduğu Shared klasörüne aktarıyoruz
    exportToJson("C:/Projeler/CanBusWebPlatform/Shared/parsed_dbc.json");
    
    return true;
}

void DbcParser::exportToJson(const QString &jsonPath) const {
    QJsonArray jsonArray;
    
    for (auto msgIt = m_messages.begin(); msgIt != m_messages.end(); ++msgIt) {
        const DbcMessage &msg = msgIt.value();
        QJsonObject msgObj;
        
        // JSON'da ID hex olarak tutulur (Örn: "0x1F4")
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
// OOP (Nesne Yönelimli Programlama) - Encapsulation (Kapsülleme) gereği:
// Dışarıdan, sınıfın gizli (private) üyesi olan 'm_messages' listesine 
// dış müdahaleyi engellemek için sadece 'okuma' amaçlı bir erişim fonksiyonu (Getter).
QMap<uint32_t, DbcMessage> DbcParser::getMessages() const {
    return m_messages;
}
// DBC dosyasından hiçbir veri çıkarılamadıysa veya henüz dosya yüklenmediyse durumu kontrol eden yardımcı fonksiyon.
bool DbcParser::isEmpty() const {
    return m_messages.isEmpty();
}


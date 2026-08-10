// dbcparser.cpp
// Bu dosya, yüklenen DBC dosyalarını satır satır okuyan (parse eden) asıl motor kısmıdır.
// Karmaşık Regex (Düzenli İfadeler) kullanarak metin içindeki verileri ayıklar.
#include "dbcparser.h"
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QDebug>

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
    return true;
}

DbcSignal DbcParser::findSignalByKeywords(const QStringList &keywords, bool &found) const {
    // Verilen anahtar kelimelerden (örn: "Speed", "Hiz") herhangi birini içeren 
    // ilk sinyali arayıp bulur. Bu sayede program DBC'ye bağımlı kalmaz.
    found = false;
    // 1. Döngü (Dış): Hafızadaki tüm Mesajları (BO_) tek tek geziyoruz.
    for (auto msgIt = m_messages.begin(); msgIt != m_messages.end(); ++msgIt) {
        // 2. Döngü (İç): O anki mesajın içindeki tüm Sinyalleri (SG_) geziyoruz.
        for (auto sigIt = msgIt.value().msgSignals.begin(); sigIt != msgIt.value().msgSignals.end(); ++sigIt) {
            const QString &sigName = sigIt.value().name;
            // 3. Döngü: Sinyal adının içinde, bizim aradığımız anahtar kelimelerden biri var mı?
            for (const QString &keyword : keywords) {
                if (sigName.contains(keyword, Qt::CaseInsensitive)) {
                    found = true;
                    return sigIt.value(); // Eşleşmeyi bulduğumuz an arama işlemini kesip sinyali gönderiyoruz.
                }
            }
        }
    }
    return DbcSignal(); // Tüm listeyi gezdik ama hiçbir eşleşme bulamadıysak boş (default) bir sinyal objesi döndürüyoruz.
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

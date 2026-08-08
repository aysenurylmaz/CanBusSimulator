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
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "Failed to open DBC file:" << filePath;
        return false;
    }

    m_messages.clear();
    QTextStream in(&file);

    // Regex for BO_ (Message)
    // BO_ satırları, bir CAN mesajının Kimliğini (ID) ve Uzunluğunu (DLC) belirtir.
    // Örnek: BO_ 500 MessageName: 8 Vector__XXX
    QRegularExpression boRegex("^\\s*BO_\\s+(\\d+)\\s+(\\w+)\\s*:\\s+(\\d+)\\s+.*");

    // Regex for SG_ (Signal)
    // SG_ Vehicle_Speed : 16|16@1+ (1,0) [0|250] "km/h" Vector__XXX
    QRegularExpression sgRegex("^\\s*SG_\\s+(\\w+).*?:\\s+(\\d+)\\|(\\d+)@([01])([\\+\\-])\\s+\\(([0-9\\.\\-]+),([0-9\\.\\-]+)\\)\\s+\\[([0-9\\.\\-]+)\\|([0-9\\.\\-]+)\\]");

    uint32_t currentMessageId = 0;

    while (!in.atEnd()) {
        QString line = in.readLine();
        
        QRegularExpressionMatch boMatch = boRegex.match(line);
        if (boMatch.hasMatch()) {
            DbcMessage msg;
            msg.id = boMatch.captured(1).toUInt();
            msg.name = boMatch.captured(2);
            msg.dlc = boMatch.captured(3).toInt();
            currentMessageId = msg.id;
            m_messages.insert(msg.id, msg);
            qDebug() << "Parsed BO_:" << msg.id << msg.name << "DLC:" << msg.dlc;
            continue;
        }

        QRegularExpressionMatch sgMatch = sgRegex.match(line);
        if (sgMatch.hasMatch() && currentMessageId != 0) {
            DbcSignal sig;
            sig.name = sgMatch.captured(1);
            sig.startBit = sgMatch.captured(2).toInt();
            sig.length = sgMatch.captured(3).toInt();
            sig.isLittleEndian = (sgMatch.captured(4) == "1");
            sig.isUnsigned = (sgMatch.captured(5) == "+");
            sig.factor = sgMatch.captured(6).toDouble();
            sig.offset = sgMatch.captured(7).toDouble();
            sig.min = sgMatch.captured(8).toDouble();
            sig.max = sgMatch.captured(9).toDouble();
            sig.messageId = currentMessageId;

            m_messages[currentMessageId].msgSignals.insert(sig.name, sig);
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
    for (auto msgIt = m_messages.begin(); msgIt != m_messages.end(); ++msgIt) {
        for (auto sigIt = msgIt.value().msgSignals.begin(); sigIt != msgIt.value().msgSignals.end(); ++sigIt) {
            const QString &sigName = sigIt.value().name;
            for (const QString &keyword : keywords) {
                if (sigName.contains(keyword, Qt::CaseInsensitive)) {
                    found = true;
                    return sigIt.value();
                }
            }
        }
    }
    return DbcSignal();
}

QMap<uint32_t, DbcMessage> DbcParser::getMessages() const {
    return m_messages;
}

bool DbcParser::isEmpty() const {
    return m_messages.isEmpty();
}

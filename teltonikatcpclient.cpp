#include "teltonikatcpclient.h"
#include <QDebug>
#include <QTimer>

TeltonikaTcpClient::TeltonikaTcpClient(QObject *parent)
    : QObject(parent), targetPort(0)
{
    // C++ uzerinde yeni bir QTcpSocket objesi yaratiyoruz (Memory allocation).
    socket = new QTcpSocket(this);

    // Otomatik yeniden baglanma icin Timer
    reconnectTimer = new QTimer(this);
    reconnectTimer->setInterval(3000); // 3 saniyede bir dene
    connect(reconnectTimer, &QTimer::timeout, this, &TeltonikaTcpClient::attemptReconnect);

    // Qt'nin Signal/Slot mimarisi (Olay gudumlu programlama):
    // Socket'in arkasinda bir seyler oldugunda (mesaj geldiginde, baglanildiginda, hata ciktiginda)
    // arka planda QTcpSocket bir bagiris (Signal) firlatir. Biz asagidaki 'connect' satirllariyla 
    // o bagirislari kendi yazdigimiz (Slot) fonksiyonlara bagliyoruz (Yakalayici).
    
    // Veri okuma hazir oldugunda onReadyRead calissin:
    connect(socket, &QTcpSocket::readyRead, this, &TeltonikaTcpClient::onReadyRead);
    
    // Baglanti gerceklestiginde onConnected calissin:
    connect(socket, &QTcpSocket::connected, this, &TeltonikaTcpClient::onConnected);

    // Baglanti koptugunda
    connect(socket, &QTcpSocket::disconnected, this, &TeltonikaTcpClient::onClientDisconnected);

// Qt5 ve Qt6 arasindaki sinyal isimlendirme farkliliklarini (Geriye donuk uyumluluk) cozmek icin 
// ufak bir preprocessor (#if) hilesi kullaniyoruz:
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    connect(socket, &QTcpSocket::errorOccurred, this, &TeltonikaTcpClient::onErrorOccurred);
#else
    connect(socket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::error), this, &TeltonikaTcpClient::onErrorOccurred);
#endif
}

TeltonikaTcpClient::~TeltonikaTcpClient()
{
    // Nesne RAM'den silinirken (Yokedici), guvenlik acisindan baglantiyi kopariyoruz.
    disconnectFromServer();
}

void TeltonikaTcpClient::connectToServer(const QString& ip, quint16 port)
{
    targetIp = ip;
    targetPort = port;
    
    // Eger su an bir baglanti yoksa, yeni bir baglanti talebi yolla (Asenkron).
    if (socket->state() == QAbstractSocket::UnconnectedState) {
        qDebug() << "[TeltonikaTcpClient] Sunucuya baglaniliyor:" << targetIp << ":" << targetPort;
        socket->connectToHost(targetIp, targetPort);
    }
}

void TeltonikaTcpClient::attemptReconnect()
{
    if (socket->state() == QAbstractSocket::UnconnectedState && targetPort != 0) {
        qDebug() << "[TeltonikaTcpClient] Sunucuya yeniden baglanmayi deniyor...";
        socket->connectToHost(targetIp, targetPort);
    }
}

void TeltonikaTcpClient::onClientDisconnected()
{
    qDebug() << "[TeltonikaTcpClient] Sunucu baglantisi koptu! Yeniden baglanma dongusu baslatiliyor...";
    if (!reconnectTimer->isActive()) {
        reconnectTimer->start();
    }
}

void TeltonikaTcpClient::sendData(const QByteArray& packet)
{
    // Veriyi gonderebilmemiz icin durumun "Bagli" (ConnectedState) olmasi zorunludur.
    if (socket->state() == QAbstractSocket::ConnectedState) {
        qDebug() << "[TeltonikaTcpClient] Codec 8 Paketi sunucuya gonderiliyor. Boyut:" << packet.size() << "byte.";
        
        // Soketin icine bytelari dokuyoruz.
        socket->write(packet);
        
        // Cok hizli ve sik gonderimlerde veriler siraya (buffer) takilmasin diye,
        // "Bekleme yapma, itele" komutu veriyoruz (Flush).
        socket->flush();
    } else {
        qDebug() << "[TeltonikaTcpClient] HATA: Sunucuya henuz bagli degil! Paket cop kutusuna gitti.";
    }
}

void TeltonikaTcpClient::disconnectFromServer()
{
    reconnectTimer->stop();
    // Aciksa soketi kapat.
    if (socket->isOpen()) {
        socket->close();
    }
}

void TeltonikaTcpClient::onReadyRead()
{
    // Sunucu basarili bir Teltonika paketi teslim aldiginda, 
    // icinde kac adet log okudugunu belirten, 4 byte uzunlugunda (Integer) bir cevap yollar.
    // Soket uzerinden gelen tum byte'lari cekiyoruz.
    QByteArray response = socket->readAll();
    
    qDebug() << "[TeltonikaTcpClient] Sunucudan yanit (ACK) geldi! Gelen Byte sayisi:" << response.size();
    
    // Gelen veri 4-byte (Standart ACK boyutu) mi diye guvenlik kontrolu yapiyoruz.
    if (response.size() >= 4) {
        // Gelen bu 4-bytelik sayi, ag standardi geregi Big-Endian (Ters) yollanir.
        // Bunu C++'in anladigi normal 32-bit sayiya (quint32) cevirmek icin,
        // byte'lari kaydirarak (Shift '<<' ve Or '|') tek bir sayida birlestiriyoruz.
        quint32 acceptedRecords = (static_cast<quint8>(response[0]) << 24) |
                                  (static_cast<quint8>(response[1]) << 16) |
                                  (static_cast<quint8>(response[2]) << 8)  |
                                   static_cast<quint8>(response[3]);
        
        qDebug() << "[TeltonikaTcpClient] HARIKA! Sunucu gonderdigimiz veriden su kadarini basariyla veritabanina yazdi:" << acceptedRecords;
    }
}

void TeltonikaTcpClient::onConnected()
{
    qDebug() << "[TeltonikaTcpClient] Handshake bitti! Veri gonderimine haziriz.";
    if (reconnectTimer->isActive()) {
        reconnectTimer->stop();
    }
}

void TeltonikaTcpClient::onErrorOccurred(QAbstractSocket::SocketError socketError)
{
    // Sunucu cokmus olabilir, internetimiz kesilmis olabilir, IP yanlis olabilir.
    // Qt'nin urettigi hazir hata mesajini konsola basiyoruz.
    qDebug() << "[TeltonikaTcpClient] SOKET HATASI OLUSTU:" << socket->errorString();
    
    // Baglanti basarisiz olursa timer'i baslatarak tekrar denemeyi sagla
    if (!reconnectTimer->isActive() && targetPort != 0) {
        reconnectTimer->start();
    }
}

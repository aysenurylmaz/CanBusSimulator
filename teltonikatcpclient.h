#ifndef TELTONIKATCPCLIENT_H
#define TELTONIKATCPCLIENT_H

#include <QObject>
#include <QTcpSocket>

// Hazirlanan Codec 8 paketlerini uzak bir sunucuya (TCP uzerinden) ileten ve 
// sunucudan gelen basari yanitlarini (ACK) dinleyen, kargocu gorevi goren siniftir.
// Paketlerin icerigiyle ilgilenmez, sadece baglantiyi acar, veriyi basar, 
// cevabi alir ve baglantiyi kapatir. Moduler yapi geregi Codec8Builder'dan bagimsizdir.
class TeltonikaTcpClient : public QObject
{
    Q_OBJECT

public:
    explicit TeltonikaTcpClient(QObject *parent = nullptr);
    ~TeltonikaTcpClient();

    // Verilen IP (Orn: "127.0.0.1" veya "85.10.20.30") ve Porta (Orn: 12345) 
    // TCP uzerinden soket baglantisi acar. Eger zaten bagliysa tekrar acmaya calismaz.
    void connectToServer(const QString& ip, quint16 port);
    
    // Codec8Builder ile 0'lardan ve 1'lerden kule gibi insa ettigimiz (Preamble, CRC iceren)
    // o muthis paketi alir, soket uzerinden sunucuya gonderir.
    void sendData(const QByteArray& packet);
    
    // Gerekli durumlarda (ornegin uygulama kapanirken) sunucu baglantisini guvenlice koparir.
    void disconnectFromServer();

private slots:
    // QTcpSocket'in "readyRead" (Okunacak Veri Hazir) sinyaline baglanan slot fonksiyonudur.
    // Sunucu (Teltonika platformu) gonderdigimiz kaydi aldiginda, bize "Kaç kayit aldigini" soyleyen
    // 4 bytelik kucuk bir onay mesaji (ACK - Acknowledgement) yollar. Bu mesaj gelince burasi tetiklenir.
    void onReadyRead();
    
    // Sunucuya basariyla "El Sikistik" (Handshake bitti, baglandik) denildiginde tetiklenen fonksiyondur.
    void onConnected();
    
    // İnternet koptugunda, sunucu yanit vermediginde veya Time-Out oldugunda
    // soketin firlattigi "Hata" sinyalini yakalayip konsola (qDebug) yazdiran fonksiyondur.
    void onErrorOccurred(QAbstractSocket::SocketError socketError);

    void onClientDisconnected();
    void attemptReconnect();

private:
    // Arkada calisan gercek Qt soket objesinin (Pointer) referansidir.
    QTcpSocket* socket;
    
    // Otomatik yeniden baglanma (auto-reconnect) islemleri icin Timer.
    class QTimer* reconnectTimer;
    
    // Baglanilmaya calisilan hedef IP ve Port bilgileri.
    QString targetIp;
    quint16 targetPort;
};

#endif // TELTONIKATCPCLIENT_H

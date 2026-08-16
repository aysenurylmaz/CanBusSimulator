#ifndef TELTONIKATCPCLIENT_H
#define TELTONIKATCPCLIENT_H

#include <QObject>
#include <QTcpSocket>

// Hazirlanan Codec 8 paketlerini uzak bir sunucuya (TCP uzerinden) ileten ve yanitlari dinleyen siniftir.
class TeltonikaTcpClient : public QObject
{
    Q_OBJECT

public:
    explicit TeltonikaTcpClient(QObject *parent = nullptr);
    ~TeltonikaTcpClient();

    // Verilen IP ve Porta baglanir.
    void connectToServer(const QString& ip, quint16 port);
    
    // Daha onceden hazirlanmis (Codec8Builder ile) paketi sokete yazar ve sunucuya gonderir.
    void sendData(const QByteArray& packet);
    
    // Baglantiyi kapatir.
    void disconnectFromServer();

private slots:
    // Sunucudan (Teltonika platformundan) bir yanit (ACK) geldiginde tetiklenir.
    void onReadyRead();
    
    // Baglanti basariyla saglandiginda tetiklenir.
    void onConnected();
    
    // Herhangi bir soket hatasi (Baglanti kopmasi, time out) oldugunda tetiklenir.
    void onErrorOccurred(QAbstractSocket::SocketError socketError);

private:
    QTcpSocket* socket;
};

#endif // TELTONIKATCPCLIENT_H

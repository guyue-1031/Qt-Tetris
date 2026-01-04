#ifndef SERVER_H
#define SERVER_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QList>

class Server : public QObject
{
    Q_OBJECT
public:
    explicit Server(QObject *parent = nullptr);

private slots:
    void onNewConnection();
    void onReadyRead();
    void onDisconnected();

private:
    QTcpServer *tcpServer;
    QList<QTcpSocket*> clients; // 存放所有連進來的玩家

    // 輔助函式：廣播訊息給特定 socket 以外的人
    void broadcast(const QByteArray &data, QTcpSocket *excludeSocket);
};

#endif // SERVER_H

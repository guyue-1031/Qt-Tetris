#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QTcpSocket>
#include <QVector>
#include <QList>
#include <QPoint>
#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QLineEdit>

// [新增] 音樂與音效標頭檔
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QSoundEffect>

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void onLocalBattleClicked();
    void onOnlineBattleClicked();
    void onBackClicked();

    void gameLoop();
    void pieceDropped();

    void onSocketConnected();
    void onSocketReadyRead();
    void onSocketDisconnected();

private:
    void initMenu();
    QWidget *menuWidget;
    QLabel *titleLabel;
    QLineEdit *nameInput;
    QPushButton *btnLocal;
    QPushButton *btnOnline;
    QPushButton *btnBack;

    void startGame();
    void spawnPiece();
    void holdPiece();
    void rotateWithWallKick();
    bool tryMove(int newX, int newY, int newRot);
    void placePiece();
    void clearLines();
    void addGarbageLines(int count);
    void updateGameLevel();

    void refillBag();
    int getNextPieceFromBag();

    QColor getShapeColor(int shapeId);
    QVector<QPoint> getShapeCoords(int shape, int rotation);
    void drawBoard(QPainter &painter, int x, int y, const QVector<int> &targetBoard, bool isPlayer);
    void drawInstructions(QPainter &painter);
    void drawQueue(QPainter &painter, int x, int y, QString label, QList<int> shapes, bool isActive);

    void sendGameState();
    void sendAttack(int lines);
    void sendPlayerName();

    // --- 變數 ---
    bool isGameMode;
    bool isOnlineMode;
    bool isPaused;
    bool isGameOver;
    bool isWaitingForOpponent;

    int score;
    int level;
    int dropSpeed;

    QVector<int> board;
    QList<int> bag;
    QList<int> nextPieces;
    int currentShape;
    int currentRotation;
    int currentX;
    int currentY;

    int heldShape;
    bool canHold;

    QString localPlayerName;
    QString opponentName;

    QVector<int> opponentBoard;
    int opponentHold;
    QVector<int> opponentNextPieces;

    QTimer *timer;
    QTimer *lockTimer;
    QTcpSocket *socket;

    // [新增] 音樂與音效物件
    QMediaPlayer *bgmPlayer;
    QAudioOutput *bgmOutput;
    QSoundEffect *clearSound; // 用來播短音效
};

#endif // MAINWINDOW_H

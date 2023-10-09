#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSerialPort>
#include <QTimer>
#include <QQueue>
#define RowBin_SIZE 60
#define ColBin_SIZE 50

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void slot_recvData();
    void slot_errorOccurred(QSerialPort::SerialPortError error);
    void on_btnConnect_clicked();
    void on_btnSend_clicked();
    void on_btnRead_clicked();
    void on_btnAuth_clicked();
    void on_btnExit_clicked();
    void slot_timeout();
    void on_btnSend1_clicked();
    void on_btnSend2_clicked();
    void on_btnSend3_clicked();
    void on_btnSend4_clicked();
    void on_btnSend5_clicked();
    void on_btnSend6_clicked();
    void on_btnSend7_clicked();
    void on_btnAuth1_clicked();
    void on_btnAuth2_clicked();
    void on_btnAuth3_clicked();
    void on_btnAuth4_clicked();
    void on_btnAuth5_clicked();
    void on_btnAuth6_clicked();
    void on_btnAuth7_clicked();
    void on_btnReload_clicked();

private:
    void sendData(QByteArray arrayData);
    void decodeProtocol1(QByteArray readData);
    void decodeReadProtocol2Data(QByteArray readData);
    void decodeAuth(QByteArray readData);
    void decodeExit(QByteArray readData);
    void encodeProtocol1();
    void readProtocol1();
    void encodeProtocol2(QQueue<QByteArray> packageQueue);
    void packingProtocol1(quint16 regAddr,quint32 value1, quint32 value2, QByteArray &writeData);
    void packingReadProtocol1(quint16 regAddr, QByteArray &writeData);
    void packingReadProtocol2(quint16 regAddr, QByteArray &writeData);
    void packingAuth(QList<quint8> password, QByteArray &writeData);
    void packingExit(QByteArray &writeData);
    QQueue<QByteArray> packingData1();
    QQueue<QByteArray> packingData2();
    QQueue<QByteArray> packingData3();
    QQueue<QByteArray> packingData4();
    QQueue<QByteArray> packingData5();
    QQueue<QByteArray> packingData6();
    QQueue<QByteArray> packingData7();
    void readData1();
    void readData2();
    void readData3();
    void readData4();
    void readData5();
    void readData6();
    void readData7();
    void decodeFlashAddr(const QByteArray &byteArray);
    void authData1(quint16 dataType, quint16 regAddr, QList<float> numDataList);
    void authData2(quint16 dataType, quint16 regAddr, QList<float> numDataList);
    void authData3(quint16 dataType, quint16 regAddr, QList<float> numDataList);
    void authData4(quint16 dataType, quint16 regAddr, QList<float> numDataList);
    void authData5(quint16 dataType, quint16 regAddr, QList<float> numDataList);
    void authData6(quint16 dataType, quint16 regAddr, QList<float> numDataList);
    void authData7(quint16 dataType, quint16 regAddr, QList<float> numDataList);

private:
    quint32 FloatToByte4(float value);
    float Byte4ToFloat(quint32 value);
    void startProgress(int min,int max);
    void endProgress();
    void setProgress(int value);
    void delayTime(int msec);

private:
    void initUI();
    void loadFile();
    void loadFile(const QString &filePathHver,const QString &filePathAlphaRad,const QString &filePathdHC,
                  const QString &filePathVRSBeta,const QString &filePathVRCBeta,const QString &filePathRTSBeta,
                  const QString &filePathRTCBeta);
    QStringList loadData(const QString &filePath,int &row, int &col);
    bool authCrc(const QByteArray &byteArray);
    quint16 crc16(const QByteArray &byteArray, quint32 byteArraySize);
    quint16 crc16ForModbus(const QByteArray &byteArray);
    quint16 revert16(quint16 originValue);
    quint32 revert32(quint32 originValue);
    QByteArray getFlashAddr(const QByteArray &byteArray);
    bool authRowCol(int row1, int row2, int col1, int col2);
    void setAllBtnEnable();
    void setAllBtnUnenable();
    void clearAllData();

private:
    Ui::MainWindow *ui;

    QSerialPort *m_pSerialPort;
    QTimer m_reSendTimer;
    int m_timeoutCounts;
    quint16 m_readP1Addr;

    QByteArray m_sendFlashAddr; //发送地址
    QByteArray m_recvFlashAddr; //接收地址
    QByteArray m_sendingBuff;   //当前发送的数据包
    QQueue<QByteArray> m_sendingPackageData; //当前发送的数据包队列

    float Hver[RowBin_SIZE];                // max60
    float Alpha[ColBin_SIZE];                  //50
    float VRSBeta[RowBin_SIZE * ColBin_SIZE]; // 60*50
    float VRCBeta[RowBin_SIZE * ColBin_SIZE]; // 60*50
    float DHC[RowBin_SIZE * ColBin_SIZE];     // 60*50
    float RTSBeta[RowBin_SIZE * ColBin_SIZE]; // 60*50
    float RTCBeta[RowBin_SIZE * ColBin_SIZE]; // 60*50
//    double RTCBeta[RowBin_SIZE][ColBin_SIZE]; // 60*50

    int m_HverSize=0; //row
    int m_AlphaSize=0;//col
    int m_VRSBetaRow=0;
    int m_VRSBetaCol=0;
    int m_VRCBetaRow=0;
    int m_VRCBetaCol=0;
    int m_DHCRow=0;
    int m_DHCCol=0;
    int m_RTSBetaRow=0;
    int m_RTSBetaCol=0;
    int m_RTCBetaRow=0;
    int m_RTCBetaCol=0;

    int m_HverReadCnts;
    QList<int> m_HverErrorList;

    int m_AlphaReadCnts;
    QList<int> m_AlphaErrorList;

    int m_VRSBetaReadCnts;
    QList<int> m_VRSBetaErrorList;

    int m_VRCBetaReadCnts;
    QList<int> m_VRCBetaErrorList;

    int m_DHCReadCnts;
    QList<int> m_DHCErrorList;

    int m_RTSBetaReadCnts;
    QList<int> m_RTSBetaErrorList;

    int m_RTCBetaReadCnts;
    QList<int> m_RTCBetaErrorList;

};
#endif // MAINWINDOW_H

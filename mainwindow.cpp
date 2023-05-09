#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QByteArray>
#include <QSerialPortInfo>
#include <QDataStream>
#include <QFile>
#include <QBuffer>
#include <QElapsedTimer>
#include <QDebug>
#include <QFileDialog>
#include <QMessageBox>

#define EPSINON 0.000001

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setWindowTitle("上位机");
//    loadFile();  //放开以后自动加载data
    initUI();
    m_reSendTimer.setInterval(1000);
    connect(&m_reSendTimer,&QTimer::timeout,this,&MainWindow::slot_timeout);
//    setAllBtnEnable(); //放开以后自动加载data
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::slot_recvData()
{
    if(!m_pSerialPort->isOpen())
    {
        return;
    }

    QByteArray readData = m_pSerialPort->readAll();
    qDebug()<<"RecvBuff:"<<readData.toHex().toUpper() <<"RecvSize:"<<readData.size();

    if(readData.size() > 0)
    {
        //校验CRC
        bool bAuthCRC = authCrc(readData);
        if(!bAuthCRC)
        {
            return;
        }

        //protocol1
        if(readData.size() == 10)
        {
            decodeProtocol1(readData);
        }
        //protocol2
        else if(readData.size() == 6)
        {
            //解析返回数据包地址
            decodeFlashAddr(readData);
        }
        //protocol2 read
        else if(readData.size() == 26)
        {
            //解析返回数据包地址
            decodeReadProtocol2Data(readData);
        }
        //身份认证权限
        else if(readData.size() == 4)
        {
            decodeAuth(readData);
        }
        //退出
        else if(readData.size() == 8)
        {
            decodeExit(readData);
        }
    }
}

void MainWindow::slot_errorOccurred(QSerialPort::SerialPortError error)
{
    if(error != QSerialPort::NoError)
    {
        qDebug()<<error;
        ui->statusbar->showMessage(QString("连接失败：Error Code %1").arg(error),5000);
    }
}

void MainWindow::on_btnConnect_clicked()
{
    if(ui->btnConnect->text() == "连接")
    {
        QString strPortName = ui->comboBox_port->currentText();	//端口
        QString strBaud = ui->comboBox_baud->currentText();	//波特率
        QString strDataBit = ui->comboBox_dataBit->currentText();	//数据位
        QString strStopBit = ui->comboBox_stopBit->currentText();	//停止位
        QString strParityBit = ui->comboBox_parityBit->currentText();	//校验位
        QString strFlowControl = ui->comboBox_flowControl->currentText();	//控制流


        int setBaud = strBaud.toInt();	//波特率
        QSerialPort::DataBits setDataBits;	//数据位
        QSerialPort::StopBits setStopBits;	//停止位
        QSerialPort::Parity setParityBits;	//校验位
        QSerialPort::FlowControl setFlowControl;//控制流

        if(strDataBit == "5")
        {
            setDataBits = QSerialPort::Data5;
        }else if(strDataBit == "6")
        {
            setDataBits = QSerialPort::Data6;
        }else if(strDataBit == "7")
        {
            setDataBits = QSerialPort::Data7;
        }else{
            setDataBits = QSerialPort::Data8;
        }

        if(strStopBit == "1")
        {
            setStopBits = QSerialPort::OneStop;
        }else if(strStopBit == "2"){
            setStopBits = QSerialPort::TwoStop;
        }else{
            setStopBits = QSerialPort::OneAndHalfStop;
        }

        if(strParityBit == "None")
        {
            setParityBits = QSerialPort::NoParity;
        }else if(strParityBit == "Odd"){
            setParityBits = QSerialPort::OddParity;
        }else if(strParityBit == "Even"){
            setParityBits = QSerialPort::EvenParity;
        }else if(strParityBit == "Mark"){
            setParityBits = QSerialPort::MarkParity;
        }else /*if(parityBits == "Space")*/{
            setParityBits = QSerialPort::SpaceParity;
        }

        if(strFlowControl == "HardWare"){
            setFlowControl = QSerialPort::HardwareControl;
        }else if(strFlowControl == "SoftWare"){
            setFlowControl = QSerialPort::SoftwareControl;
        }else /*if(flowControl == "None")*/{
            setFlowControl = QSerialPort::NoFlowControl;
        }

        m_pSerialPort->setPortName(strPortName);
        m_pSerialPort->setBaudRate(setBaud);
        m_pSerialPort->setDataBits(setDataBits);
        m_pSerialPort->setStopBits(setStopBits);
        m_pSerialPort->setParity(setParityBits);
        m_pSerialPort->setFlowControl(setFlowControl);

        if(!m_pSerialPort->open(QIODevice::ReadWrite))
        {
            return;
        }
        ui->btnConnect->setText("断开连接");
        ui->statusbar->showMessage("串口连接成功！",5000);
    }
    else
    {
        if(m_pSerialPort->isOpen())
        {
            m_pSerialPort->clear();
            m_pSerialPort->close();
            ui->btnConnect->setText("连接");
        }
    }
}

void MainWindow::on_btnSend_clicked()
{
    if(!m_pSerialPort->isOpen())
        return;

    encodeProtocol1();
}

void MainWindow::on_btnAuth_clicked()
{
    if(!m_pSerialPort->isOpen())
        return;

    QByteArray writeData;
    QList<quint8> password;
    password.append((quint8)ui->ID1->text().toUInt(nullptr,16));
    password.append((quint8)ui->ID2->text().toUInt(nullptr,16));
    password.append((quint8)ui->ID3->text().toUInt(nullptr,16));
    password.append((quint8)ui->ID4->text().toUInt(nullptr,16));
    password.append((quint8)ui->ID5->text().toUInt(nullptr,16));
    password.append((quint8)ui->ID6->text().toUInt(nullptr,16));
    password.append((quint8)ui->ID7->text().toUInt(nullptr,16));
    password.append((quint8)ui->ID8->text().toUInt(nullptr,16));
    packingAuth(password,writeData);
    sendData(writeData);
}

void MainWindow::on_btnExit_clicked()
{
    if(!m_pSerialPort->isOpen())
        return;

    QByteArray writeData;
    packingExit(writeData);
    sendData(writeData);
}

void MainWindow::slot_timeout()
{
    m_timeoutCounts++;
    if(m_timeoutCounts > 3)
    {
        m_reSendTimer.stop();
        //清除队列
        m_sendingPackageData.clear();
        endProgress();
        qDebug()<<QStringLiteral("ReSend 3 times, end");
        ui->statusbar->showMessage("已重试3次没有回应，结束本次发送",5000);
        return;
    }
    sendData(m_sendingBuff);
    qDebug()<<QStringLiteral("ReSendCounts:")<<m_timeoutCounts;
    ui->statusbar->showMessage(QString("重发次数：%1").arg(m_timeoutCounts),5000);
}

void MainWindow::on_btnSend1_clicked()
{
    if(!m_pSerialPort->isOpen())
        return;

    encodeProtocol2(packingData1());
}

void MainWindow::on_btnSend2_clicked()
{
    if(!m_pSerialPort->isOpen())
        return;

    encodeProtocol2(packingData2());
}

void MainWindow::on_btnSend3_clicked()
{
    if(!m_pSerialPort->isOpen())
        return;

    encodeProtocol2(packingData3());
}

void MainWindow::on_btnSend4_clicked()
{
    if(!m_pSerialPort->isOpen())
        return;

    encodeProtocol2(packingData4());
}

void MainWindow::on_btnSend5_clicked()
{
    if(!m_pSerialPort->isOpen())
        return;

    encodeProtocol2(packingData5());
}

void MainWindow::on_btnSend6_clicked()
{
    if(!m_pSerialPort->isOpen())
        return;

    encodeProtocol2(packingData6());
}

void MainWindow::on_btnSend7_clicked()
{
    if(!m_pSerialPort->isOpen())
        return;

    encodeProtocol2(packingData7());
}

void MainWindow::on_btnAuth1_clicked()
{
    if(!m_pSerialPort->isOpen())
        return;

    readData1();
}

void MainWindow::on_btnAuth2_clicked()
{
    if(!m_pSerialPort->isOpen())
        return;

    readData2();
}

void MainWindow::on_btnAuth3_clicked()
{
    if(!m_pSerialPort->isOpen())
        return;

    readData3();
}

void MainWindow::on_btnAuth4_clicked()
{
    if(!m_pSerialPort->isOpen())
        return;

    readData4();
}

void MainWindow::on_btnAuth5_clicked()
{
    if(!m_pSerialPort->isOpen())
        return;

    readData5();
}

void MainWindow::on_btnAuth6_clicked()
{
    if(!m_pSerialPort->isOpen())
        return;

    readData6();
}

void MainWindow::on_btnAuth7_clicked()
{
    if(!m_pSerialPort->isOpen())
        return;

    readData7();
}

void MainWindow::on_btnReload_clicked()
{
//    loadFile();
    QString filePath1;
    QString filePath2;
    QString filePath3;
    QString filePath4;
    QString filePath5;
    QString filePath6;
    QString filePath7;

    QString defaultDir = qApp->applicationDirPath() + "/../";
    QString pathDir = QFileDialog::getExistingDirectory(this, tr("开打文件夹"),
                                                    defaultDir,
                                                    QFileDialog::ShowDirsOnly
                                                        | QFileDialog::DontResolveSymlinks);
    QDir dir(pathDir);
    if(!dir.exists())
    {
        QMessageBox msgBox(this);
        msgBox.setWindowTitle("提示");
        msgBox.setText("打开文件失败!");
        msgBox.exec();
        return;
    }

    int authCounts = 0;
    dir.setFilter(QDir::Files | QDir::NoSymLinks);
    QFileInfoList list = dir.entryInfoList();
    for(int i = 0; i < list.size(); i++)
    {
        QFileInfo fileInfo = list.at(i);
        QString curFileName = fileInfo.fileName();
//        qDebug() << fileInfo.fileName();
        if(curFileName == "Hver.dat")
        {
            filePath1 = fileInfo.absoluteFilePath();
            authCounts++;
        }
        if(curFileName == "AlphaRad.dat")
        {
            filePath2 = fileInfo.absoluteFilePath();
            authCounts++;
        }
        if(curFileName == "dHC.dat")
        {
            filePath3 = fileInfo.absoluteFilePath();
            authCounts++;
        }
        if(curFileName == "VSRBeta.dat")
        {
            filePath4 = fileInfo.absoluteFilePath();
            authCounts++;
        }
        if(curFileName == "VSCBeta.dat")
        {
            filePath5 = fileInfo.absoluteFilePath();
            authCounts++;
        }
        if(curFileName == "RTSBeta.dat")
        {
            filePath6 = fileInfo.absoluteFilePath();
            authCounts++;
        }
        if(curFileName == "RTCBeta.dat")
        {
            filePath7 = fileInfo.absoluteFilePath();
            authCounts++;
        }
    }

    if(authCounts < 7)
    {
        QMessageBox msgBox(this);
        msgBox.setWindowTitle("提示");
        msgBox.setText("文件名校验失败!");
        setAllBtnUnenable();
        clearAllData();
        msgBox.exec();
        return;
    }

    loadFile(filePath1,filePath2,filePath3,filePath4,filePath5,filePath6,filePath7);
    setAllBtnEnable();
}

void MainWindow::sendData(QByteArray arrayData)
{
    if(m_pSerialPort->isOpen())
    {
        m_pSerialPort->write(arrayData.constData(),arrayData.size());
        qDebug()<<"SendBuff:"<<arrayData.toHex().toUpper() <<"SendSize:"<<arrayData.size();
        delayTime(50);
    }
}

void MainWindow::decodeProtocol1(QByteArray readData)
{
    QDataStream in(&readData,QIODevice::ReadWrite);
    quint32 nValue = 0;

    if(m_readP1Addr == 520)
    {
        //uint32_t nMiuS
        in >> nValue;
        ui->R_1->setText(QString::number(revert32(nValue)));

        //uint32_t nPhiS
        in >> nValue;
        ui->R_2->setText(QString::number(revert32(nValue)));
    }
    else if(m_readP1Addr == 528)
    {
        //uint32_t ndPS
        in >> nValue;
        ui->R_3->setText(QString::number(revert32(nValue)));

        //uint32_t nLstaS
        in >> nValue;
        ui->R_4->setText(QString::number(revert32(nValue)));
    }
    else if(m_readP1Addr == 536)
    {
        //uint32_t nLstaW
        in >> nValue;
        ui->R_5->setText(QString::number(revert32(nValue)));

        //float K_trc
        in >> nValue;
        ui->R_6->setText(QString::number(Byte4ToFloat(revert32(nValue))));
    }
    else if(m_readP1Addr == 544)
    {
        //float TkR
        in >> nValue;
        ui->R_7->setText(QString::number(Byte4ToFloat(revert32(nValue))));

        //float TkR1;
        in >> nValue;
        ui->R_8->setText(QString::number(Byte4ToFloat(revert32(nValue))));
    }
    else if(m_readP1Addr == 552)
    {
        //float TkR2
        in >> nValue;
        ui->R_9->setText(QString::number(Byte4ToFloat(revert32(nValue))));

        //float PhiRa
        in >> nValue;
        ui->R_11->setText(QString::number(Byte4ToFloat(revert32(nValue))));
    }
    else if(m_readP1Addr == 560)
    {
        //uint32_t PhiWS
        in >> nValue;
        ui->R_12->setText(QString::number(revert32(nValue)));

        //uint32_t nLvalS
        in >> nValue;
        ui->R_13->setText(QString::number(revert32(nValue)));
    }
    else if(m_readP1Addr == 568)
    {
        //uint32_t Kc
        in >> nValue;
        ui->R_14->setText(QString::number(revert32(nValue)));

        //uint32_t Kc1
        in >> nValue;
        ui->R_15->setText(QString::number(revert32(nValue)));
    }
    else if(m_readP1Addr == 576)
    {
        //float THver
        in >> nValue;
        ui->R_16->setText(QString::number(Byte4ToFloat(revert32(nValue))));

        //uint32_t Kcal1
        in >> nValue;
        ui->R_17->setText(QString::number(revert32(nValue)));
    }
    else if(m_readP1Addr == 584)
    {
        //uint32_t Kcal2
        in >> nValue;
        ui->R_18->setText(QString::number(revert32(nValue)));

        //uint32_t Kcal3
        in >> nValue;
        ui->R_19->setText(QString::number(revert32(nValue)));
    }
    else if(m_readP1Addr == 592)
    {
        //uint32_t xhx_err_time
        in >> nValue;
        ui->R_20->setText(QString::number(revert32(nValue)));

        //uint32_t RowBin_SIZE_par
        in >> nValue;
        ui->R_10->setText(QString::number(revert32(nValue)));
    }
    else if(m_readP1Addr == 600)
    {
        //uint32_t ColBin_SIZE_par
        in >> nValue;
        ui->R_21->setText(QString::number(revert32(nValue)));
    }
}

void MainWindow::decodeReadProtocol2Data(QByteArray readData)
{
    QDataStream in(&readData,QIODevice::ReadWrite);
    quint8 deviceAddr; //设备地址
    in >> deviceAddr;

    quint8 funcCode;    //功能码
    in >> funcCode;

    quint16 regAddr;
    in >> regAddr;      //寄存器地址

    QList<float> numDataList;
    quint32 num;
    in >> num;
    numDataList.append(Byte4ToFloat(num));
    in >> num;
    numDataList.append(Byte4ToFloat(num));
    in >> num;
    numDataList.append(Byte4ToFloat(num));
    in >> num;
    numDataList.append(Byte4ToFloat(num));
    in >> num;
    numDataList.append(Byte4ToFloat(num));

    quint16 addrType = regAddr & 0xF000;
    switch(addrType)
    {
        case 0x5000: authData1(addrType, regAddr, numDataList);break;
        case 0x6000: authData2(addrType, regAddr, numDataList);break;
        case 0x7000: authData3(addrType, regAddr, numDataList);break;
        case 0x8000: authData4(addrType, regAddr, numDataList);break;
        case 0x9000: authData5(addrType, regAddr, numDataList);break;
        case 0xA000: authData6(addrType, regAddr, numDataList);break;
        case 0xB000: authData7(addrType, regAddr, numDataList);break;
        default: break;
    }
}

void MainWindow::decodeAuth(QByteArray readData)
{
    QString strHex = readData.toHex().toUpper();
    //读写权限
    if(strHex == "8866E79A")
    {
        ui->id_status->setText("已认证(读写权限)");
    }
    //只读权限
    else if(strHex == "443372A5")
    {
        ui->id_status->setText("已认证(只读权限)");
    }
    else
    {
        ui->id_status->setText("未认证");
    }
}

void MainWindow::decodeExit(QByteArray readData)
{
    QString strHex = readData.toHex().toUpper();
    //退出
    if(strHex == "66882A76")
    {
        ui->id_status->setText("未认证");
    }
}

void MainWindow::encodeProtocol1()
{
    int nCount = 0;
    startProgress(0,10);

    QByteArray writeData;
    quint16 regAddr = 0;
    quint32 value1 = 0;
    quint32 value2 = 0;

    //uint32_t nMiuS;  // 默认31
    //uint32_t nPhiS;  // 默认2000
    regAddr = 520;
    value1 = ui->W_1->text().toUInt();
    value2 = ui->W_2->text().toUInt();
    packingProtocol1(regAddr,value1,value2,writeData);
    sendData(writeData);
    writeData.clear();
    setProgress(++nCount);

    //uint32_t ndPS;  // 默认20
    //uint32_t nLstaS;  // 默认20
    regAddr = 528;
    value1 = ui->W_3->text().toUInt();
    value2 = ui->W_4->text().toUInt();
    packingProtocol1(regAddr,value1,value2,writeData);
    sendData(writeData);
    writeData.clear();
    setProgress(++nCount);

    //uint32_t nLstaW;  // 默认20
    //float K_trc;  // 默认 0.5
    regAddr = 536;
    value1 = ui->W_5->text().toUInt();
    value2 = FloatToByte4(ui->W_6->text().toFloat());
    packingProtocol1(regAddr,value1,value2,writeData);
    sendData(writeData);
    writeData.clear();
    setProgress(++nCount);

    //float TkR;  // 默认0.86
    //float TkR1;  // 默认0.86
    regAddr = 544;
    value1 = FloatToByte4(ui->W_7->text().toFloat());
    value2 = FloatToByte4(ui->W_8->text().toFloat());
    packingProtocol1(regAddr,value1,value2,writeData);
    sendData(writeData);
    writeData.clear();
    setProgress(++nCount);

    //float TkR2;  // 默认0.86
    //float PhiRa;  // 默认0.1
    regAddr = 552;
    value1 = FloatToByte4(ui->W_9->text().toFloat());
    value2 = FloatToByte4(ui->W_11->text().toFloat());
    packingProtocol1(regAddr,value1,value2,writeData);
    sendData(writeData);
    writeData.clear();
    setProgress(++nCount);

    //uint32_t PhiWS;  // 默认200
    //uint32_t nLvalS;  // 默认200
    regAddr = 560;
    value1 = ui->W_12->text().toUInt();
    value2 = ui->W_13->text().toUInt();
    packingProtocol1(regAddr,value1,value2,writeData);
    sendData(writeData);
    writeData.clear();
    setProgress(++nCount);

    //uint32_t Kc;  // 默认 1000
    //uint32_t Kc1;  // 默认 3079
    regAddr = 568;
    value1 = ui->W_14->text().toUInt();
    value2 = ui->W_15->text().toUInt();
    packingProtocol1(regAddr,value1,value2,writeData);
    sendData(writeData);
    writeData.clear();
    setProgress(++nCount);

    //float THver;  // 默认 0.647
    //uint32_t Kcal1;  // 默认 1000
    regAddr = 576;
    value1 = FloatToByte4(ui->W_16->text().toFloat());
    value2 = ui->W_17->text().toUInt();
    packingProtocol1(regAddr,value1,value2,writeData);
    sendData(writeData);
    writeData.clear();
    setProgress(++nCount);

    //uint32_t Kcal2;  // 默认 1000
    //uint32_t Kcal3;  // 默认 1000
    regAddr = 584;
    value1 = ui->W_18->text().toUInt();
    value2 = ui->W_19->text().toUInt();
    packingProtocol1(regAddr,value1,value2,writeData);
    sendData(writeData);
    writeData.clear();
    setProgress(++nCount);

    //uint32_t xhx_err_time;    // 默认10次，超过这个
    //uint32_t RowBin_SIZE_par; // 50-60 默认55
    regAddr = 592;
    value1 = ui->W_20->text().toUInt();
    value2 = ui->W_10->text().toUInt();
    packingProtocol1(regAddr,value1,value2,writeData);
    sendData(writeData);
    writeData.clear();
    setProgress(++nCount);

    //uint32_t ColBin_SIZE_par; // 50-60 默认55
    regAddr = 600;
    value1 = ui->W_21->text().toUInt();
    value2 = 0;
    packingProtocol1(regAddr,value1,value2,writeData);
    sendData(writeData);
    writeData.clear();
    setProgress(++nCount);
    endProgress();

    //读取设备数据
    readProtocol1();
}

void MainWindow::readProtocol1()
{
    m_readP1Addr = 520;
    QByteArray writeReadData;
    packingReadProtocol1(m_readP1Addr, writeReadData);
    sendData(writeReadData);
    writeReadData.clear();

    m_readP1Addr = 528;
    packingReadProtocol1(m_readP1Addr, writeReadData);
    sendData(writeReadData);
    writeReadData.clear();

    m_readP1Addr = 536;
    packingReadProtocol1(m_readP1Addr, writeReadData);
    sendData(writeReadData);
    writeReadData.clear();

    m_readP1Addr = 544;
    packingReadProtocol1(m_readP1Addr, writeReadData);
    sendData(writeReadData);
    writeReadData.clear();

    m_readP1Addr = 552;
    packingReadProtocol1(m_readP1Addr, writeReadData);
    sendData(writeReadData);
    writeReadData.clear();

    m_readP1Addr = 560;
    packingReadProtocol1(m_readP1Addr, writeReadData);
    sendData(writeReadData);
    writeReadData.clear();

    m_readP1Addr = 568;
    packingReadProtocol1(m_readP1Addr, writeReadData);
    sendData(writeReadData);
    writeReadData.clear();

    m_readP1Addr = 576;
    packingReadProtocol1(m_readP1Addr, writeReadData);
    sendData(writeReadData);
    writeReadData.clear();

    m_readP1Addr = 584;
    packingReadProtocol1(m_readP1Addr, writeReadData);
    sendData(writeReadData);
    writeReadData.clear();

    m_readP1Addr = 592;
    packingReadProtocol1(m_readP1Addr, writeReadData);
    sendData(writeReadData);
    writeReadData.clear();

    m_readP1Addr = 600;
    packingReadProtocol1(m_readP1Addr, writeReadData);
    sendData(writeReadData);
    writeReadData.clear();
}

void MainWindow::encodeProtocol2(QQueue<QByteArray> packageQueue)
{
    m_sendFlashAddr.clear();
    m_recvFlashAddr.clear();
    m_sendingPackageData.clear();
    m_sendingPackageData = packageQueue;

    startProgress(0,m_sendingPackageData.size());
    int counts = 0;
    while(m_sendingPackageData.size() > 0)
    {
        qDebug()<<"PackList:"<<m_sendingPackageData.size();
        QByteArray writeData = m_sendingPackageData.dequeue();
        m_sendFlashAddr = getFlashAddr(writeData);
        sendData(writeData);
        //test recvFlashAddr
//        m_recvFlashAddr = getFlashAddr(writeData);
        m_sendingBuff = writeData;
        m_reSendTimer.start();
        m_timeoutCounts = 0;
        while(m_reSendTimer.isActive())
        {
            if(m_sendFlashAddr == m_recvFlashAddr)
            {
                //                qDebug()<<"sendFlashAddr:"<<m_sendFlashAddr.toHex().toUpper()
                //                         <<"recvFlashAddr:"<<m_recvFlashAddr.toHex().toUpper();
                m_reSendTimer.stop();
                setProgress(++counts);
                break;
            }
            //阻塞等待
            QCoreApplication::processEvents();
        }
    }
    endProgress();
}

void MainWindow::packingProtocol1(quint16 regAddr, quint32 value1, quint32 value2, QByteArray &writeData)
{
    QDataStream out(&writeData,QIODevice::ReadWrite);
    out << (quint8)0x01; //设备地址 1Byte
    out << (quint8)0x06; //功能码 1Byte
    out << regAddr;  //寄存器地址 2Byte
    out << revert32(value1);  //兼容老协议 使用小端存储数据
    out << revert32(value2);  //兼容老协议 使用小端存储数据
//    quint16 CRC = crc16(writeData,writeData.size());
    quint16 CRC = crc16ForModbus(writeData);
//    quint16 CRC = ModbusCRC16(writeData);
    out << CRC;
}

void MainWindow::packingReadProtocol1(quint16 regAddr, QByteArray &writeData)
{
    QDataStream out(&writeData,QIODevice::ReadWrite);
    out << (quint8)0x01; //设备地址 1Byte
    out << (quint8)0x03; //功能码 1Byte
    out << (quint16)regAddr;  //寄存器地址 2Byte
    out << (quint16)8;  //读取字节长度 2Byte
    quint16 CRC = crc16ForModbus(writeData);
    out << CRC;
}

void MainWindow::packingReadProtocol2(quint16 regAddr, QByteArray &writeData)
{
    QDataStream out(&writeData,QIODevice::ReadWrite);
    out << (quint8)0x01; //设备地址 1Byte
    out << (quint8)0x11; //功能码 1Byte
    out << (quint16)regAddr;  //寄存器地址 2Byte
    out << (quint16)5;  //读取字节长度 2Byte
    quint16 CRC = crc16ForModbus(writeData);
    out << CRC;
}

void MainWindow::packingAuth(QList<quint8> password, QByteArray &writeData)
{
    QDataStream out(&writeData,QIODevice::ReadWrite);
    for(int i = 0; i < password.size(); i++)
    {
        out << password.at(i);
    }
    quint16 CRC = crc16ForModbus(writeData);
    out << CRC;
}

void MainWindow::packingExit(QByteArray &writeData)
{
    QDataStream out(&writeData,QIODevice::ReadWrite);
    out << (quint8)0xCC;
    out << (quint8)0xDD;
    out << (quint8)0xEE;
    out << (quint8)0xFF;
    out << (quint8)0x04;
    out << (quint8)0x03;
    out << (quint8)0x02;
    out << (quint8)0x01;
    quint16 CRC = crc16ForModbus(writeData);
    out << CRC;
}

QQueue<QByteArray> MainWindow::packingData1()
{
    QQueue<QByteArray> packageData;

    QList<QList<quint32>> packList;
    QList<quint32> numList;
    for(int i = 1; i <= RowBin_SIZE; i++)
    {
        numList.append(FloatToByte4(Hver[i-1]));
        if(i % 10 == 0)
        {
            packList.append(numList);
            numList.clear();
        }
    }

    for(int i = 0; i < packList.size(); i++)
    {
        QByteArray writeData;
        QDataStream out(&writeData,QIODevice::ReadWrite);
        QList<quint32> numList = packList.at(i);

        out << (quint8)0x01;
        out << (quint8)0x10;
        out << (quint16)(0x5000 + i * 0x000A);

        for(int j = 0; j < numList.size(); j++)
        {
            out << numList.at(j);
        }

        quint16 CRC = crc16ForModbus(writeData);
        out << CRC;
        packageData.enqueue(writeData);
    }

    return packageData;
}

QQueue<QByteArray> MainWindow::packingData2()
{
    QQueue<QByteArray> packageData;

    QList<QList<quint32>> packList;
    QList<quint32> numList;
    for(int i = 1; i <= ColBin_SIZE; i++)
    {
        numList.append(FloatToByte4(Alpha[i-1]));
        if(i % 10 == 0)
        {
            packList.append(numList);
            numList.clear();
        }
    }

    for(int i = 0; i < packList.size(); i++)
    {
        QByteArray writeData;
        QDataStream out(&writeData,QIODevice::ReadWrite);
        QList<quint32> numList = packList.at(i);

        out << (quint8)0x01;
        out << (quint8)0x10;
        out << (quint16)(0x6000 + i * 0x000A);

        for(int j = 0; j < numList.size(); j++)
        {
            out << numList.at(j);
        }

        quint16 CRC = crc16ForModbus(writeData);
        out << CRC;
        packageData.enqueue(writeData);
    }

    return packageData;
}

QQueue<QByteArray> MainWindow::packingData3()
{
    QQueue<QByteArray> packageData;

    QList<QList<quint32>> packList;
    QList<quint32> numList;
    for(int i = 1; i <= RowBin_SIZE*ColBin_SIZE; i++)
    {
        numList.append(FloatToByte4(VRSBeta[i-1]));
        if(i % 10 == 0)
        {
            packList.append(numList);
            numList.clear();
        }
    }

    for(int i = 0; i < packList.size(); i++)
    {
        QByteArray writeData;
        QDataStream out(&writeData,QIODevice::ReadWrite);
        QList<quint32> numList = packList.at(i);

        out << (quint8)0x01;
        out << (quint8)0x10;
        out << (quint16)(0x7000 + i * 0x000A);

        for(int j = 0; j < numList.size(); j++)
        {
            out << numList.at(j);
        }

        quint16 CRC = crc16ForModbus(writeData);
        out << CRC;
        packageData.enqueue(writeData);
    }

    return packageData;

//    QQueue<QByteArray> packageData;

//    QList<QList<quint32>> packList;
//    QList<quint32> numList;
//    for(int row = 1; row <= RowBin_SIZE; row++)
//    {
//        for(int col = 1; col <= ColBin_SIZE; col++)
//        {
//            numList.append(FloatToByte4(VRSBeta[row-1][col-1]));
//            if(col % 10 == 0)
//            {
//                packList.append(numList);
//                numList.clear();
//            }
//        }
//    }

//    for(int i = 0; i < packList.size(); i++)
//    {
//        QByteArray writeData;
//        QDataStream out(&writeData,QIODevice::ReadWrite);
//        QList<quint32> numList = packList.at(i);

//        out << (quint8)0x01;
//        out << (quint8)0x10;
//        out << (quint16)(0x7000 + i * 0x000A);

//        for(int j = 0; j < numList.size(); j++)
//        {
//            out << numList.at(j);
//        }

//        quint16 CRC = crc16ForModbus(writeData);
//        out << CRC;
//        packageData.enqueue(writeData);
//    }

//    return packageData;
}

QQueue<QByteArray> MainWindow::packingData4()
{
    QQueue<QByteArray> packageData;

    QList<QList<quint32>> packList;
    QList<quint32> numList;
    for(int i = 1; i <= RowBin_SIZE*ColBin_SIZE; i++)
    {
        numList.append(FloatToByte4(VRCBeta[i-1]));
        if(i % 10 == 0)
        {
            packList.append(numList);
            numList.clear();
        }
    }

    for(int i = 0; i < packList.size(); i++)
    {
        QByteArray writeData;
        QDataStream out(&writeData,QIODevice::ReadWrite);
        QList<quint32> numList = packList.at(i);

        out << (quint8)0x01;
        out << (quint8)0x10;
        out << (quint16)(0x8000 + i * 0x000A);

        for(int j = 0; j < numList.size(); j++)
        {
            out << numList.at(j);
        }

        quint16 CRC = crc16ForModbus(writeData);
        out << CRC;
        packageData.enqueue(writeData);
    }

    return packageData;
}

QQueue<QByteArray> MainWindow::packingData5()
{
    QQueue<QByteArray> packageData;

    QList<QList<quint32>> packList;
    QList<quint32> numList;
    for(int i = 1; i <= RowBin_SIZE*ColBin_SIZE; i++)
    {
        numList.append(FloatToByte4(DHC[i-1]));
        if(i % 10 == 0)
        {
            packList.append(numList);
            numList.clear();
        }
    }

    for(int i = 0; i < packList.size(); i++)
    {
        QByteArray writeData;
        QDataStream out(&writeData,QIODevice::ReadWrite);
        QList<quint32> numList = packList.at(i);

        out << (quint8)0x01;
        out << (quint8)0x10;
        out << (quint16)(0x9000 + i * 0x000A);

        for(int j = 0; j < numList.size(); j++)
        {
            out << numList.at(j);
        }

        quint16 CRC = crc16ForModbus(writeData);
        out << CRC;
        packageData.enqueue(writeData);
    }

    return packageData;
}

QQueue<QByteArray> MainWindow::packingData6()
{
    QQueue<QByteArray> packageData;

    QList<QList<quint32>> packList;
    QList<quint32> numList;
    for(int i = 1; i <= RowBin_SIZE*ColBin_SIZE; i++)
    {
        numList.append(FloatToByte4(RTSBeta[i-1]));
        if(i % 10 == 0)
        {
            packList.append(numList);
            numList.clear();
        }
    }

    for(int i = 0; i < packList.size(); i++)
    {
        QByteArray writeData;
        QDataStream out(&writeData,QIODevice::ReadWrite);
        QList<quint32> numList = packList.at(i);

        out << (quint8)0x01;
        out << (quint8)0x10;
        out << (quint16)(0xA000 + i * 0x000A);

        for(int j = 0; j < numList.size(); j++)
        {
            out << numList.at(j);
        }

        quint16 CRC = crc16ForModbus(writeData);
        out << CRC;
        packageData.enqueue(writeData);
    }

    return packageData;
}

QQueue<QByteArray> MainWindow::packingData7()
{
    QQueue<QByteArray> packageData;

    QList<QList<quint32>> packList;
    QList<quint32> numList;
    for(int i = 1; i <= RowBin_SIZE*ColBin_SIZE; i++)
    {
        numList.append(FloatToByte4(RTCBeta[i-1]));
        if(i % 10 == 0)
        {
            packList.append(numList);
            numList.clear();
        }
    }

    for(int i = 0; i < packList.size(); i++)
    {
        QByteArray writeData;
        QDataStream out(&writeData,QIODevice::ReadWrite);
        QList<quint32> numList = packList.at(i);

        out << (quint8)0x01;
        out << (quint8)0x10;
        out << (quint16)(0xB000 + i * 0x000A);

        for(int j = 0; j < numList.size(); j++)
        {
            out << numList.at(j);
        }

        quint16 CRC = crc16ForModbus(writeData);
        out << CRC;
        packageData.enqueue(writeData);
    }

    return packageData;
}

void MainWindow::readData1()
{
    m_HverReadCnts = 0;
    m_HverErrorList.clear();
    int counts = 0;
    quint16 regAddr = 0x5000;

    startProgress(0,RowBin_SIZE/5);

    for(int i = 0; i < RowBin_SIZE/5; i++)
    {
        QByteArray writeReadData;
        packingReadProtocol2(regAddr+i*5, writeReadData);
        sendData(writeReadData);
        setProgress(counts++);
    }
    endProgress();

    if(m_HverErrorList.size() > 0)
    {
        QString errorInfo="";
        for(int i = 0; i < m_HverErrorList.size(); i++)
        {
            errorInfo += QString("%1,").arg(m_HverErrorList.at(i));
        }

        ui->errorNum1->setToolTip(errorInfo);
        if(m_HverErrorList.size() > 10)
        {
            errorInfo = errorInfo.left(20);
            errorInfo += "...";
        }
        ui->errorNum1->setText(errorInfo);
    }
    else
    {
        ui->errorNum1->setText("无");
    }
}

void MainWindow::readData2()
{
    m_AlphaReadCnts = 0;
    m_AlphaErrorList.clear();
    int counts = 0;
    quint16 regAddr = 0x6000;

    startProgress(0,ColBin_SIZE/5);

    for(int i = 0; i < ColBin_SIZE/5; i++)
    {
        QByteArray writeReadData;
        packingReadProtocol2(regAddr+i*5, writeReadData);
        sendData(writeReadData);
        setProgress(counts++);
    }
    endProgress();

    if(m_AlphaErrorList.size() > 0)
    {
        QString errorInfo="";
        for(int i = 0; i < m_AlphaErrorList.size(); i++)
        {
            errorInfo += QString("%1,").arg(m_AlphaErrorList.at(i));
        }

        ui->errorNum2->setToolTip(errorInfo);
        if(m_AlphaErrorList.size() > 10)
        {
            errorInfo = errorInfo.left(20);
            errorInfo += "...";
        }
        ui->errorNum2->setText(errorInfo);
    }
    else
    {
        ui->errorNum2->setText("无");
    }
}

void MainWindow::readData3()
{
    m_VRSBetaReadCnts = 0;
    m_VRSBetaErrorList.clear();
    int counts = 0;
    quint16 regAddr = 0x7000;

    startProgress(0,RowBin_SIZE*ColBin_SIZE/5);

    for(int i = 0; i < RowBin_SIZE*ColBin_SIZE/5; i++)
    {
        QByteArray writeReadData;
        packingReadProtocol2(regAddr+i*5, writeReadData);
        sendData(writeReadData);
        setProgress(counts++);
    }
    endProgress();

    if(m_VRSBetaErrorList.size() > 0)
    {
        QString errorInfo="";
        for(int i = 0; i < m_VRSBetaErrorList.size(); i++)
        {
            errorInfo += QString("%1,").arg(m_VRSBetaErrorList.at(i));
        }

        ui->errorNum3->setToolTip(errorInfo);
        if(m_VRSBetaErrorList.size() > 10)
        {
            errorInfo = errorInfo.left(20);
            errorInfo += "...";
        }
        ui->errorNum3->setText(errorInfo);
    }
    else
    {
        ui->errorNum3->setText("无");
    }
}

void MainWindow::readData4()
{
    m_VRCBetaReadCnts = 0;
    m_VRCBetaErrorList.clear();
    int counts = 0;
    quint16 regAddr = 0x8000;

    startProgress(0,RowBin_SIZE*ColBin_SIZE/5);

    for(int i = 0; i < RowBin_SIZE*ColBin_SIZE/5; i++)
    {
        QByteArray writeReadData;
        packingReadProtocol2(regAddr+i*5, writeReadData);
        sendData(writeReadData);
        setProgress(counts++);
    }
    endProgress();

    if(m_VRCBetaErrorList.size() > 0)
    {
        QString errorInfo="";
        for(int i = 0; i < m_VRCBetaErrorList.size(); i++)
        {
            errorInfo += QString("%1,").arg(m_VRCBetaErrorList.at(i));
        }

        ui->errorNum4->setToolTip(errorInfo);
        if(m_VRCBetaErrorList.size() > 10)
        {
            errorInfo = errorInfo.left(20);
            errorInfo += "...";
        }
        ui->errorNum4->setText(errorInfo);
    }
    else
    {
        ui->errorNum4->setText("无");
    }
}

void MainWindow::readData5()
{
    m_DHCReadCnts = 0;
    m_DHCErrorList.clear();
    int counts = 0;
    quint16 regAddr = 0x9000;

    startProgress(0,RowBin_SIZE*ColBin_SIZE/5);

    for(int i = 0; i < RowBin_SIZE*ColBin_SIZE/5; i++)
    {
        QByteArray writeReadData;
        packingReadProtocol2(regAddr+i*5, writeReadData);
        sendData(writeReadData);
        setProgress(counts++);
    }
    endProgress();

    if(m_DHCErrorList.size() > 0)
    {
        QString errorInfo="";
        for(int i = 0; i < m_DHCErrorList.size(); i++)
        {
            errorInfo += QString("%1,").arg(m_DHCErrorList.at(i));
        }

        ui->errorNum5->setToolTip(errorInfo);
        if(m_DHCErrorList.size() > 10)
        {
            errorInfo = errorInfo.left(20);
            errorInfo += "...";
        }
        ui->errorNum5->setText(errorInfo);
    }
    else
    {
        ui->errorNum5->setText("无");
    }
}

void MainWindow::readData6()
{
    m_RTSBetaReadCnts = 0;
    m_RTSBetaErrorList.clear();
    int counts = 0;
    quint16 regAddr = 0xA000;

    startProgress(0,RowBin_SIZE*ColBin_SIZE/5);

    for(int i = 0; i < RowBin_SIZE*ColBin_SIZE/5; i++)
    {
        QByteArray writeReadData;
        packingReadProtocol2(regAddr+i*5, writeReadData);
        sendData(writeReadData);
        setProgress(counts++);
    }
    endProgress();

    if(m_RTSBetaErrorList.size() > 0)
    {
        QString errorInfo="";
        for(int i = 0; i < m_RTSBetaErrorList.size(); i++)
        {
            errorInfo += QString("%1,").arg(m_RTSBetaErrorList.at(i));
        }

        ui->errorNum6->setToolTip(errorInfo);
        if(m_RTSBetaErrorList.size() > 10)
        {
            errorInfo = errorInfo.left(20);
            errorInfo += "...";
        }
        ui->errorNum6->setText(errorInfo);
    }
    else
    {
        ui->errorNum6->setText("无");
    }
}

void MainWindow::readData7()
{
    m_RTCBetaReadCnts = 0;
    m_RTCBetaErrorList.clear();
    int counts = 0;
    quint16 regAddr = 0xB000;

    startProgress(0,RowBin_SIZE*ColBin_SIZE/5);

    for(int i = 0; i < RowBin_SIZE*ColBin_SIZE/5; i++)
    {
        QByteArray writeReadData;
        packingReadProtocol2(regAddr+i*5, writeReadData);
        sendData(writeReadData);
        setProgress(counts++);
    }
    endProgress();

    if(m_RTCBetaErrorList.size() > 0)
    {
        QString errorInfo="";
        for(int i = 0; i < m_RTCBetaErrorList.size(); i++)
        {
            errorInfo += QString("%1,").arg(m_RTCBetaErrorList.at(i));
        }

        ui->errorNum7->setToolTip(errorInfo);
        if(m_RTCBetaErrorList.size() > 10)
        {
            errorInfo = errorInfo.left(20);
            errorInfo += "...";
        }
        ui->errorNum7->setText(errorInfo);
    }
    else
    {
        ui->errorNum7->setText("无");
    }
}

void MainWindow::decodeFlashAddr(const QByteArray &byteArray)
{
    m_recvFlashAddr = getFlashAddr(byteArray);
}

void MainWindow::authData1(quint16 dataType, quint16 regAddr, QList<float> numDataList)
{
    int numIndex = regAddr - dataType;
    if(numIndex < 0 || numIndex > m_HverSize-1)
        return;

    for(int i = 0; i < numDataList.size(); i++)
    {
        float oldValue = Hver[numIndex+i];
        float newValue = numDataList.at(i);
        qDebug()<<"numIndex:"<<numIndex+i+1<<"numDataListSize:"<<numDataList.size();
        if((oldValue - newValue) >= - EPSINON && (oldValue - newValue) <= EPSINON)
        {
            if(m_HverReadCnts < m_HverSize)
                m_HverReadCnts++;
            ui->succNum1->setText(QString::number(m_HverReadCnts));
        }
        else
        {
            m_HverErrorList.append(numIndex+i+1);
//            qDebug()<<"error index:"<<m_HverErrorList;
        }
    }
}

void MainWindow::authData2(quint16 dataType, quint16 regAddr, QList<float> numDataList)
{
    int numIndex = regAddr - dataType;
    if(numIndex < 0 || numIndex > m_AlphaSize-1)
        return;

    for(int i = 0; i < numDataList.size(); i++)
    {
        float oldValue = Alpha[numIndex+i];
        float newValue = numDataList.at(i);
//        qDebug()<<"numIndex:"<<numIndex+i+1<<"numDataListSize:"<<numDataList.size();
        if((oldValue - newValue) >= - EPSINON && (oldValue - newValue) <= EPSINON)
        {
            if(m_AlphaReadCnts < m_AlphaSize)
                m_AlphaReadCnts++;
            ui->succNum2->setText(QString::number(m_AlphaReadCnts));
        }
        else
        {
            m_AlphaErrorList.append(numIndex+i+1);
//            qDebug()<<"error index:"<<m_AlphaErrorList;
        }
    }
}

void MainWindow::authData3(quint16 dataType, quint16 regAddr, QList<float> numDataList)
{
    int numIndex = regAddr - dataType;
    if(numIndex < 0 || numIndex > m_HverSize*m_AlphaSize-1)
        return;

    for(int i = 0; i < numDataList.size(); i++)
    {
        float oldValue = VRSBeta[numIndex+i];
        float newValue = numDataList.at(i);
        qDebug()<<"numIndex:"<<numIndex+i+1<<"numDataListSize:"<<numDataList.size();
        if((oldValue - newValue) >= - EPSINON && (oldValue - newValue) <= EPSINON)
        {
            if(m_VRSBetaReadCnts < m_HverSize*m_AlphaSize)
                m_VRSBetaReadCnts++;
            ui->succNum3->setText(QString::number(m_VRSBetaReadCnts));
        }
        else
        {
            m_VRSBetaErrorList.append(numIndex+i+1);
//            qDebug()<<"error index:"<<m_VRSBetaErrorList;
        }
    }
}

void MainWindow::authData4(quint16 dataType, quint16 regAddr, QList<float> numDataList)
{
    int numIndex = regAddr - dataType;
    if(numIndex < 0 || numIndex > m_HverSize*m_AlphaSize-1)
        return;

    for(int i = 0; i < numDataList.size(); i++)
    {
        float oldValue = VRCBeta[numIndex+i];
        float newValue = numDataList.at(i);
//        qDebug()<<"numIndex:"<<numIndex+i+1<<"numDataListSize:"<<numDataList.size();
        if((oldValue - newValue) >= - EPSINON && (oldValue - newValue) <= EPSINON)
        {
            if(m_VRCBetaReadCnts < m_HverSize*m_AlphaSize)
                m_VRCBetaReadCnts++;
            ui->succNum4->setText(QString::number(m_VRCBetaReadCnts));
        }
        else
        {
            m_VRCBetaErrorList.append(numIndex+i+1);
//            qDebug()<<"error index:"<<m_VRCBetaErrorList;
        }
    }
}

void MainWindow::authData5(quint16 dataType, quint16 regAddr, QList<float> numDataList)
{
    int numIndex = regAddr - dataType;
    if(numIndex < 0 || numIndex > m_HverSize*m_AlphaSize-1)
        return;

    for(int i = 0; i < numDataList.size(); i++)
    {
        float oldValue = DHC[numIndex+i];
        float newValue = numDataList.at(i);
//        qDebug()<<"numIndex:"<<numIndex+i+1<<"numDataListSize:"<<numDataList.size();
        if((oldValue - newValue) >= - EPSINON && (oldValue - newValue) <= EPSINON)
        {
            if(m_DHCReadCnts < m_HverSize*m_AlphaSize)
                m_DHCReadCnts++;
            ui->succNum5->setText(QString::number(m_DHCReadCnts));
        }
        else
        {
            m_DHCErrorList.append(numIndex+i+1);
//            qDebug()<<"error index:"<<m_DHCErrorList;
        }
    }
}

void MainWindow::authData6(quint16 dataType, quint16 regAddr, QList<float> numDataList)
{
    int numIndex = regAddr - dataType;
    if(numIndex < 0 || numIndex > m_HverSize*m_AlphaSize-1)
        return;

    for(int i = 0; i < numDataList.size(); i++)
    {
        float oldValue = RTSBeta[numIndex+i];
        float newValue = numDataList.at(i);
//        qDebug()<<"numIndex:"<<numIndex+i+1<<"numDataListSize:"<<numDataList.size();
        if((oldValue - newValue) >= - EPSINON && (oldValue - newValue) <= EPSINON)
        {
            if(m_RTSBetaReadCnts < m_HverSize*m_AlphaSize)
                m_RTSBetaReadCnts++;
            ui->succNum6->setText(QString::number(m_RTSBetaReadCnts));
        }
        else
        {
            m_RTSBetaErrorList.append(numIndex+i+1);
//            qDebug()<<"error index:"<<m_RTSBetaErrorList;
        }
    }
}

void MainWindow::authData7(quint16 dataType, quint16 regAddr, QList<float> numDataList)
{
    int numIndex = regAddr - dataType;
    if(numIndex < 0 || numIndex > m_HverSize*m_AlphaSize-1)
        return;

    for(int i = 0; i < numDataList.size(); i++)
    {
        float oldValue = RTCBeta[numIndex+i];
        float newValue = numDataList.at(i);
//        qDebug()<<"numIndex:"<<numIndex+i+1<<"numDataListSize:"<<numDataList.size();
        if((oldValue - newValue) >= - EPSINON && (oldValue - newValue) <= EPSINON)
        {
            if(m_RTCBetaReadCnts < m_HverSize*m_AlphaSize)
                m_RTCBetaReadCnts++;
            ui->succNum7->setText(QString::number(m_RTCBetaReadCnts));
        }
        else
        {
            m_RTCBetaErrorList.append(numIndex+i+1);
//            qDebug()<<"error index:"<<m_RTCBetaErrorList;
        }
    }
}

quint32 MainWindow::FloatToByte4(float value)
{
    quint32 *tmpValue = reinterpret_cast<quint32*>(&value);

//    QByteArray ba;
//    ba.append(char(*tmpValue >> 24));
//    ba.append(char(*tmpValue >> 16));
//    ba.append(char(*tmpValue >>  8));
//    ba.append(char(*tmpValue >>  0));
//    qDebug()<<ba.toHex().toUpper();

    return *tmpValue;
}

float MainWindow::Byte4ToFloat(quint32 value)
{
    float *f = reinterpret_cast<float *>(&value);
    return *f;
}

void MainWindow::startProgress(int min, int max)
{
    ui->progressBar->setRange(min,max);
    ui->progressBar->show();
}

void MainWindow::endProgress()
{
    ui->progressBar->hide();
    ui->progressBar->setRange(0,0);
}

void MainWindow::setProgress(int value)
{
    ui->progressBar->setValue(value);
}

void MainWindow::delayTime(int msec)
{
    QElapsedTimer t;
    t.start();
    while (!t.hasExpired(msec))
    {
        //阻塞等待
        QCoreApplication::processEvents();
    }
}

void MainWindow::initUI()
{
    ui->R_1->setEnabled(false);
    ui->R_2->setEnabled(false);
    ui->R_3->setEnabled(false);
    ui->R_4->setEnabled(false);
    ui->R_5->setEnabled(false);
    ui->R_6->setEnabled(false);
    ui->R_7->setEnabled(false);
    ui->R_8->setEnabled(false);
    ui->R_9->setEnabled(false);
    ui->R_10->setEnabled(false);
    ui->R_11->setEnabled(false);
    ui->R_12->setEnabled(false);
    ui->R_13->setEnabled(false);
    ui->R_14->setEnabled(false);
    ui->R_15->setEnabled(false);
    ui->R_16->setEnabled(false);
    ui->R_17->setEnabled(false);
    ui->R_18->setEnabled(false);
    ui->R_19->setEnabled(false);
    ui->R_20->setEnabled(false);
    ui->R_21->setEnabled(false);
    ui->W_10->setEnabled(false);
    ui->W_21->setEnabled(false);
    setAllBtnUnenable();

    //查找可用的串口
    foreach (const QSerialPortInfo &info,QSerialPortInfo::availablePorts())
    {
        ui->comboBox_port->addItem(info.portName());
    }

    //波特率9600、19200、38400、43000、56000、57600、115200
    ui->comboBox_baud->addItem("9600");
    ui->comboBox_baud->addItem("19200");
    ui->comboBox_baud->addItem("38400");
    ui->comboBox_baud->addItem("57600");
    ui->comboBox_baud->addItem("115200");
    ui->comboBox_baud->setCurrentIndex(4);


    //数据位
    ui->comboBox_dataBit->addItem("5");
    ui->comboBox_dataBit->addItem("6");
    ui->comboBox_dataBit->addItem("7");
    ui->comboBox_dataBit->addItem("8");
    ui->comboBox_dataBit->setCurrentIndex(3);

    //停止位
    ui->comboBox_stopBit->addItem("1");
    ui->comboBox_stopBit->addItem("2");
    ui->comboBox_stopBit->setCurrentIndex(0);


    //校验位
    ui->comboBox_parityBit->addItem("None");
    ui->comboBox_parityBit->addItem("Odd");
    ui->comboBox_parityBit->addItem("Even");
    ui->comboBox_parityBit->addItem("Mark");
    ui->comboBox_parityBit->addItem("Space");
    ui->comboBox_parityBit->setCurrentIndex(0);

    //控制流
    ui->comboBox_flowControl->addItem("HardWare");
    ui->comboBox_flowControl->addItem("SoftWare");
    ui->comboBox_flowControl->addItem("None");
    ui->comboBox_flowControl->setCurrentIndex(2);

    m_pSerialPort = new QSerialPort(this);
    m_pSerialPort->setReadBufferSize(512);
    connect(m_pSerialPort,SIGNAL(readyRead()),this,SLOT(slot_recvData()));
    connect(m_pSerialPort,&QSerialPort::errorOccurred,this,&MainWindow::slot_errorOccurred);

    endProgress();
}

void MainWindow::loadFile()
{
    memset(Hver,0,sizeof(Hver));
    memset(Alpha,0,sizeof(Alpha));
    memset(VRSBeta,0,sizeof(VRSBeta));
    memset(VRCBeta,0,sizeof(VRCBeta));
    memset(DHC,0,sizeof(DHC));
    memset(RTSBeta,0,sizeof(RTSBeta));
    memset(RTCBeta,0,sizeof(RTCBeta));

    QStringList strList;
    QString filePath = qApp->applicationDirPath() + "/../data/Hver.dat";
    int row = 0;
    strList = loadData(filePath,row,m_HverSize);
    ui->W_10->setText(QString::number(m_HverSize));
    ui->num1->setText(QString::number(strList.size()));
    for(auto i = 0; i < strList.size(); i++)
    {
        QString str = strList.at(i);
        Hver[i] = str.toFloat();
    }

    filePath = qApp->applicationDirPath() + "/../data/AlphaRad.dat";
    strList = loadData(filePath,row,m_AlphaSize);
    ui->W_21->setText(QString::number(m_AlphaSize));
    ui->num2->setText(QString::number(strList.size()));
    for(auto i = 0; i < strList.size(); i++)
    {
        QString str = strList.at(i);
        Alpha[i] = str.toFloat();
    }

    filePath = qApp->applicationDirPath() + "/../data/VSRBeta.dat";
    strList = loadData(filePath,m_VRSBetaRow,m_VRSBetaCol);
    QString info = "";
    if(authRowCol(m_VRSBetaRow,m_HverSize,m_VRSBetaCol,m_AlphaSize))
        info = QString::number(strList.size());
    else
        info = QString("%1(数据行列数不一致)").arg(strList.size());
    ui->num3->setText(info);
    for(auto i = 0; i < strList.size(); i++)
    {
        QString str = strList.at(i);
        VRSBeta[i] = str.toFloat();
    }

    filePath = qApp->applicationDirPath() + "/../data/VSCBeta.dat";
    strList = loadData(filePath,m_VRCBetaRow,m_VRCBetaCol);
    info = "";
    if(authRowCol(m_VRCBetaRow,m_HverSize,m_VRCBetaCol,m_AlphaSize))
        info = QString::number(strList.size());
    else
        info = QString("%1(数据行列数不一致)").arg(strList.size());
    ui->num4->setText(info);
    for(auto i = 0; i < strList.size(); i++)
    {
        QString str = strList.at(i);
        VRCBeta[i] = str.toFloat();
    }

    filePath = qApp->applicationDirPath() + "/../data/dHC.dat";
    strList = loadData(filePath,m_DHCRow,m_DHCCol);
    info = "";
    if(authRowCol(m_DHCRow,m_HverSize,m_DHCCol,m_AlphaSize))
        info = QString::number(strList.size());
    else
        info = QString("%1(数据行列数不一致)").arg(strList.size());
    ui->num5->setText(info);
    for(auto i = 0; i < strList.size(); i++)
    {
        QString str = strList.at(i);
        DHC[i] = str.toFloat();
    }

    filePath = qApp->applicationDirPath() + "/../data/RTSBeta.dat";
    strList = loadData(filePath,m_RTSBetaRow,m_RTSBetaCol);
    info = "";
    if(authRowCol(m_RTSBetaRow,m_HverSize,m_RTSBetaCol,m_AlphaSize))
        info = QString::number(strList.size());
    else
        info = QString("%1(数据行列数不一致)").arg(strList.size());
    ui->num6->setText(info);
    for(auto i = 0; i < strList.size(); i++)
    {
        QString str = strList.at(i);
        RTSBeta[i] = str.toFloat();
    }

    filePath = qApp->applicationDirPath() + "/../data/RTCBeta.dat";
    strList = loadData(filePath,m_RTCBetaRow,m_RTCBetaCol);
    info = "";
    if(authRowCol(m_RTCBetaRow,m_HverSize,m_RTCBetaCol,m_AlphaSize))
        info = QString::number(strList.size());
    else
        info = QString("%1(数据行列数不一致)").arg(strList.size());
    ui->num7->setText(info);
    for(auto i = 0; i < strList.size(); i++)
    {
        QString str = strList.at(i);
        RTCBeta[i] = str.toFloat();
    }
}

void MainWindow::loadFile(const QString &filePath1,const QString &filePath2,const QString &filePath3,
                          const QString &filePath4,const QString &filePath5,const QString &filePath6,
                          const QString &filePath7)
{
    memset(Hver,0,sizeof(Hver));
    memset(Alpha,0,sizeof(Alpha));
    memset(VRSBeta,0,sizeof(VRSBeta));
    memset(VRCBeta,0,sizeof(VRCBeta));
    memset(DHC,0,sizeof(DHC));
    memset(RTSBeta,0,sizeof(RTSBeta));
    memset(RTCBeta,0,sizeof(RTCBeta));

    QStringList strList;
//    QString filePath = qApp->applicationDirPath() + "/../data/Hver.dat";
    QString filePath = filePath1;
    int row = 0;
    strList = loadData(filePath,row,m_HverSize);
    ui->W_10->setText(QString::number(m_HverSize));
    ui->num1->setText(QString::number(strList.size()));
    for(auto i = 0; i < strList.size(); i++)
    {
        QString str = strList.at(i);
        Hver[i] = str.toFloat();
    }

//    filePath = qApp->applicationDirPath() + "/../data/AlphaRad.dat";
    filePath = filePath2;
    strList = loadData(filePath,row,m_AlphaSize);
    ui->W_21->setText(QString::number(m_AlphaSize));
    ui->num2->setText(QString::number(strList.size()));
    for(auto i = 0; i < strList.size(); i++)
    {
        QString str = strList.at(i);
        Alpha[i] = str.toFloat();
    }

//    filePath = qApp->applicationDirPath() + "/../data/VSRBeta.dat";
    filePath = filePath3;
    strList = loadData(filePath,m_VRSBetaRow,m_VRSBetaCol);
    QString info = "";
    if(authRowCol(m_VRSBetaRow,m_HverSize,m_VRSBetaCol,m_AlphaSize))
        info = QString::number(strList.size());
    else
        info = QString("%1(数据行列数不一致)").arg(strList.size());
    ui->num3->setText(info);
    for(auto i = 0; i < strList.size(); i++)
    {
        QString str = strList.at(i);
        VRSBeta[i] = str.toFloat();
    }
//    int row = 0;
//    int col = 0;
//    for(auto i = 1; i <= strList.size(); i++)
//    {
//        QString str = strList.at(i-1);
//        VRSBeta[row][col] = str.toFloat();
//        col++;
//        if(i % 50 == 0)
//        {
//            row++;
//            col = 0;
//        }
//    }

//    filePath = qApp->applicationDirPath() + "/../data/VSCBeta.dat";
    filePath = filePath4;
    strList = loadData(filePath,m_VRCBetaRow,m_VRCBetaCol);
    info = "";
    if(authRowCol(m_VRCBetaRow,m_HverSize,m_VRCBetaCol,m_AlphaSize))
        info = QString::number(strList.size());
    else
        info = QString("%1(数据行列数不一致)").arg(strList.size());
    ui->num4->setText(info);
    for(auto i = 0; i < strList.size(); i++)
    {
        QString str = strList.at(i);
        VRCBeta[i] = str.toFloat();
    }

//    filePath = qApp->applicationDirPath() + "/../data/dHC.dat";
    filePath = filePath5;
    strList = loadData(filePath,m_DHCRow,m_DHCCol);
    info = "";
    if(authRowCol(m_DHCRow,m_HverSize,m_DHCCol,m_AlphaSize))
        info = QString::number(strList.size());
    else
        info = QString("%1(数据行列数不一致)").arg(strList.size());
    ui->num5->setText(info);
    for(auto i = 0; i < strList.size(); i++)
    {
        QString str = strList.at(i);
        DHC[i] = str.toFloat();
    }

//    filePath = qApp->applicationDirPath() + "/../data/RTSBeta.dat";
    filePath = filePath6;
    strList = loadData(filePath,m_RTSBetaRow,m_RTSBetaCol);
    info = "";
    if(authRowCol(m_RTSBetaRow,m_HverSize,m_RTSBetaCol,m_AlphaSize))
        info = QString::number(strList.size());
    else
        info = QString("%1(数据行列数不一致)").arg(strList.size());
    ui->num6->setText(info);
    for(auto i = 0; i < strList.size(); i++)
    {
        QString str = strList.at(i);
        RTSBeta[i] = str.toFloat();
    }

//    filePath = qApp->applicationDirPath() + "/../data/RTCBeta.dat";
    filePath = filePath7;
    strList = loadData(filePath,m_RTCBetaRow,m_RTCBetaCol);
    info = "";
    if(authRowCol(m_RTCBetaRow,m_HverSize,m_RTCBetaCol,m_AlphaSize))
        info = QString::number(strList.size());
    else
        info = QString("%1(数据行列数不一致)").arg(strList.size());
    ui->num7->setText(info);
    for(auto i = 0; i < strList.size(); i++)
    {
        QString str = strList.at(i);
        RTCBeta[i] = str.toFloat();
    }
}

QStringList MainWindow::loadData(const QString &filePath, int &row, int &col)
{
    QStringList dataList;
    QFile file(filePath);
    if(!file.open(QIODevice::ReadOnly))
    {
        return dataList;
    }

    QTextStream in(&file);
    row = 0;
    col = 0;
    bool bFirst = true;
    while (!in.atEnd())
    {
//        dataList.append(in.readLine().split("\t",QString::SkipEmptyParts));
        dataList.append(in.readLine().split(" ",QString::SkipEmptyParts));
        //计算列数
        if(bFirst)
        {
            col = dataList.size();
            bFirst = false;
        }
//        //计算行数
//        row++;
//        qDebug() << "data1Size:" <<dataList.size();
    }
    //计算行数
    row = dataList.size() / col;
    file.close();
    return dataList;
}

bool MainWindow::authCrc(const QByteArray &byteArray)
{
    if(byteArray.size() < 2)
        return false;

    QByteArray recvCRC = byteArray.right(2);
    QByteArray originData = byteArray.left(byteArray.size() - 2);
    QByteArray calcCRC;
    QDataStream out(&calcCRC,QIODevice::ReadWrite);
    out << crc16ForModbus(originData);

    if(recvCRC == calcCRC)
    {
        return true;
    }
    else
    {
        qDebug()<<"recvCRC:"<<recvCRC.toHex().toUpper()
                 <<"calcCRC:"<<calcCRC.toHex().toUpper()
                 <<"originBuff:"<<byteArray.toHex().toUpper();

        QString info = QString("recvCRC:%1 calcCRC:%2 originBuff:%3")
                           .arg(QString(recvCRC.toHex().toUpper()))
                           .arg(QString(calcCRC.toHex().toUpper()))
                            .arg(QString(byteArray.toHex().toUpper()));
        ui->statusbar->showMessage(info,5000);
        return false;
    }
}

quint16 MainWindow::crc16(const QByteArray &byteArray, quint32 byteArraySize)
{
    unsigned char  auchCRCHi[] = {
        0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0,
        0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
        0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
        0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
        0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1,
        0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41,
        0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1,
        0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
        0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0,
        0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40,
        0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1,
        0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
        0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0,
        0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40,
        0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
        0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
        0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0,
        0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
        0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
        0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
        0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
        0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40,
        0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1,
        0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
        0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0,
        0x80, 0x41, 0x00, 0xC1, 0x81, 0x40
    };
    unsigned char  auchCRCLo[] = {
        0x00, 0xC0, 0xC1, 0x01, 0xC3, 0x03, 0x02, 0xC2, 0xC6, 0x06,
        0x07, 0xC7, 0x05, 0xC5, 0xC4, 0x04, 0xCC, 0x0C, 0x0D, 0xCD,
        0x0F, 0xCF, 0xCE, 0x0E, 0x0A, 0xCA, 0xCB, 0x0B, 0xC9, 0x09,
        0x08, 0xC8, 0xD8, 0x18, 0x19, 0xD9, 0x1B, 0xDB, 0xDA, 0x1A,
        0x1E, 0xDE, 0xDF, 0x1F, 0xDD, 0x1D, 0x1C, 0xDC, 0x14, 0xD4,
        0xD5, 0x15, 0xD7, 0x17, 0x16, 0xD6, 0xD2, 0x12, 0x13, 0xD3,
        0x11, 0xD1, 0xD0, 0x10, 0xF0, 0x30, 0x31, 0xF1, 0x33, 0xF3,
        0xF2, 0x32, 0x36, 0xF6, 0xF7, 0x37, 0xF5, 0x35, 0x34, 0xF4,
        0x3C, 0xFC, 0xFD, 0x3D, 0xFF, 0x3F, 0x3E, 0xFE, 0xFA, 0x3A,
        0x3B, 0xFB, 0x39, 0xF9, 0xF8, 0x38, 0x28, 0xE8, 0xE9, 0x29,
        0xEB, 0x2B, 0x2A, 0xEA, 0xEE, 0x2E, 0x2F, 0xEF, 0x2D, 0xED,
        0xEC, 0x2C, 0xE4, 0x24, 0x25, 0xE5, 0x27, 0xE7, 0xE6, 0x26,
        0x22, 0xE2, 0xE3, 0x23, 0xE1, 0x21, 0x20, 0xE0, 0xA0, 0x60,
        0x61, 0xA1, 0x63, 0xA3, 0xA2, 0x62, 0x66, 0xA6, 0xA7, 0x67,
        0xA5, 0x65, 0x64, 0xA4, 0x6C, 0xAC, 0xAD, 0x6D, 0xAF, 0x6F,
        0x6E, 0xAE, 0xAA, 0x6A, 0x6B, 0xAB, 0x69, 0xA9, 0xA8, 0x68,
        0x78, 0xB8, 0xB9, 0x79, 0xBB, 0x7B, 0x7A, 0xBA, 0xBE, 0x7E,
        0x7F, 0xBF, 0x7D, 0xBD, 0xBC, 0x7C, 0xB4, 0x74, 0x75, 0xB5,
        0x77, 0xB7, 0xB6, 0x76, 0x72, 0xB2, 0xB3, 0x73, 0xB1, 0x71,
        0x70, 0xB0, 0x50, 0x90, 0x91, 0x51, 0x93, 0x53, 0x52, 0x92,
        0x96, 0x56, 0x57, 0x97, 0x55, 0x95, 0x94, 0x54, 0x9C, 0x5C,
        0x5D, 0x9D, 0x5F, 0x9F, 0x9E, 0x5E, 0x5A, 0x9A, 0x9B, 0x5B,
        0x99, 0x59, 0x58, 0x98, 0x88, 0x48, 0x49, 0x89, 0x4B, 0x8B,
        0x8A, 0x4A, 0x4E, 0x8E, 0x8F, 0x4F, 0x8D, 0x4D, 0x4C, 0x8C,
        0x44, 0x84, 0x85, 0x45, 0x87, 0x47, 0x46, 0x86, 0x82, 0x42,
        0x43, 0x83, 0x41, 0x81, 0x80, 0x40
    };
    quint8 ucCRCHi = 0xFF;
    quint8 ucCRCLo = 0xFF;
    quint32 uIndex = 0;
    int i = 0;
    while(byteArraySize--)
    {
        uIndex =  (ucCRCHi ^ ((quint8)byteArray.at(i)));
        ucCRCHi = (ucCRCLo ^ auchCRCHi[uIndex]);
        ucCRCLo = auchCRCLo[uIndex];
        i += 1;
    }
    quint16 crc16Value = (((quint16)ucCRCHi << 8) | ucCRCLo);
    return revert16(crc16Value);
}

quint16 MainWindow::crc16ForModbus(const QByteArray &byteArray)
{
    const quint16 crc16Table[] =
    {
        0x0000, 0xC0C1, 0xC181, 0x0140, 0xC301, 0x03C0, 0x0280, 0xC241,
        0xC601, 0x06C0, 0x0780, 0xC741, 0x0500, 0xC5C1, 0xC481, 0x0440,
        0xCC01, 0x0CC0, 0x0D80, 0xCD41, 0x0F00, 0xCFC1, 0xCE81, 0x0E40,
        0x0A00, 0xCAC1, 0xCB81, 0x0B40, 0xC901, 0x09C0, 0x0880, 0xC841,
        0xD801, 0x18C0, 0x1980, 0xD941, 0x1B00, 0xDBC1, 0xDA81, 0x1A40,
        0x1E00, 0xDEC1, 0xDF81, 0x1F40, 0xDD01, 0x1DC0, 0x1C80, 0xDC41,
        0x1400, 0xD4C1, 0xD581, 0x1540, 0xD701, 0x17C0, 0x1680, 0xD641,
        0xD201, 0x12C0, 0x1380, 0xD341, 0x1100, 0xD1C1, 0xD081, 0x1040,
        0xF001, 0x30C0, 0x3180, 0xF141, 0x3300, 0xF3C1, 0xF281, 0x3240,
        0x3600, 0xF6C1, 0xF781, 0x3740, 0xF501, 0x35C0, 0x3480, 0xF441,
        0x3C00, 0xFCC1, 0xFD81, 0x3D40, 0xFF01, 0x3FC0, 0x3E80, 0xFE41,
        0xFA01, 0x3AC0, 0x3B80, 0xFB41, 0x3900, 0xF9C1, 0xF881, 0x3840,
        0x2800, 0xE8C1, 0xE981, 0x2940, 0xEB01, 0x2BC0, 0x2A80, 0xEA41,
        0xEE01, 0x2EC0, 0x2F80, 0xEF41, 0x2D00, 0xEDC1, 0xEC81, 0x2C40,
        0xE401, 0x24C0, 0x2580, 0xE541, 0x2700, 0xE7C1, 0xE681, 0x2640,
        0x2200, 0xE2C1, 0xE381, 0x2340, 0xE101, 0x21C0, 0x2080, 0xE041,
        0xA001, 0x60C0, 0x6180, 0xA141, 0x6300, 0xA3C1, 0xA281, 0x6240,
        0x6600, 0xA6C1, 0xA781, 0x6740, 0xA501, 0x65C0, 0x6480, 0xA441,
        0x6C00, 0xACC1, 0xAD81, 0x6D40, 0xAF01, 0x6FC0, 0x6E80, 0xAE41,
        0xAA01, 0x6AC0, 0x6B80, 0xAB41, 0x6900, 0xA9C1, 0xA881, 0x6840,
        0x7800, 0xB8C1, 0xB981, 0x7940, 0xBB01, 0x7BC0, 0x7A80, 0xBA41,
        0xBE01, 0x7EC0, 0x7F80, 0xBF41, 0x7D00, 0xBDC1, 0xBC81, 0x7C40,
        0xB401, 0x74C0, 0x7580, 0xB541, 0x7700, 0xB7C1, 0xB681, 0x7640,
        0x7200, 0xB2C1, 0xB381, 0x7340, 0xB101, 0x71C0, 0x7080, 0xB041,
        0x5000, 0x90C1, 0x9181, 0x5140, 0x9301, 0x53C0, 0x5280, 0x9241,
        0x9601, 0x56C0, 0x5780, 0x9741, 0x5500, 0x95C1, 0x9481, 0x5440,
        0x9C01, 0x5CC0, 0x5D80, 0x9D41, 0x5F00, 0x9FC1, 0x9E81, 0x5E40,
        0x5A00, 0x9AC1, 0x9B81, 0x5B40, 0x9901, 0x59C0, 0x5880, 0x9841,
        0x8801, 0x48C0, 0x4980, 0x8941, 0x4B00, 0x8BC1, 0x8A81, 0x4A40,
        0x4E00, 0x8EC1, 0x8F81, 0x4F40, 0x8D01, 0x4DC0, 0x4C80, 0x8C41,
        0x4400, 0x84C1, 0x8581, 0x4540, 0x8701, 0x47C0, 0x4680, 0x8641,
        0x8201, 0x42C0, 0x4380, 0x8341, 0x4100, 0x81C1, 0x8081, 0x4040
    };

    quint8 buf;
    quint16 crc16 = 0xFFFF;

    for ( auto i = 0; i < byteArray.size(); ++i )
    {
        buf = byteArray.at( i ) ^ crc16;
        crc16 >>= 8;
        crc16 ^= crc16Table[ buf ];
    }
//    qDebug()<<QString("%1").arg(crc16 , 4, 16, QLatin1Char('0'));   //拼凑成4个16进制字符，空位补0
    //	return QString::number(crc16,16).toUpper();
    return revert16(crc16);
}

quint16 MainWindow::revert16(quint16 originValue)
{
    quint8 high = originValue >> 8;
    quint8 low = originValue;
    return (low << 8) | high;
}

quint32 MainWindow::revert32(quint32 originValue)
{
    quint8 high2 = originValue >> 24;
    quint8 high1 = originValue >> 16;
    quint8 low2 = originValue >> 8;
    quint8 low1 = originValue;

    return (low1 << 24) | (low2 << 16) | (high1 << 8) | high2;
}

QByteArray MainWindow::getFlashAddr(const QByteArray &byteArray)
{
    return byteArray.mid(2,2);
}

bool MainWindow::authRowCol(int row1, int row2, int col1, int col2)
{
    if(row1 == row2 && col1 == col2)
    {
        return true;
    }
    else
        return false;
}

void MainWindow::setAllBtnEnable()
{
    ui->btnSend->setEnabled(true);
    ui->btnSend1->setEnabled(true);
    ui->btnSend2->setEnabled(true);
    ui->btnSend3->setEnabled(true);
    ui->btnSend4->setEnabled(true);
    ui->btnSend5->setEnabled(true);
    ui->btnSend6->setEnabled(true);
    ui->btnSend7->setEnabled(true);
    ui->btnAuth1->setEnabled(true);
    ui->btnAuth2->setEnabled(true);
    ui->btnAuth3->setEnabled(true);
    ui->btnAuth4->setEnabled(true);
    ui->btnAuth5->setEnabled(true);
    ui->btnAuth6->setEnabled(true);
    ui->btnAuth7->setEnabled(true);
}

void MainWindow::setAllBtnUnenable()
{
    ui->btnSend->setEnabled(false);
    ui->btnSend1->setEnabled(false);
    ui->btnSend2->setEnabled(false);
    ui->btnSend3->setEnabled(false);
    ui->btnSend4->setEnabled(false);
    ui->btnSend5->setEnabled(false);
    ui->btnSend6->setEnabled(false);
    ui->btnSend7->setEnabled(false);
    ui->btnAuth1->setEnabled(false);
    ui->btnAuth2->setEnabled(false);
    ui->btnAuth3->setEnabled(false);
    ui->btnAuth4->setEnabled(false);
    ui->btnAuth5->setEnabled(false);
    ui->btnAuth6->setEnabled(false);
    ui->btnAuth7->setEnabled(false);
}

void MainWindow::clearAllData()
{
    memset(Hver,0,sizeof(Hver));
    memset(Alpha,0,sizeof(Alpha));
    memset(VRSBeta,0,sizeof(VRSBeta));
    memset(VRCBeta,0,sizeof(VRCBeta));
    memset(DHC,0,sizeof(DHC));
    memset(RTSBeta,0,sizeof(RTSBeta));
    memset(RTCBeta,0,sizeof(RTCBeta));

    ui->num1->setText(QString("0"));
    ui->num2->setText(QString("0"));
    ui->num3->setText(QString("0"));
    ui->num4->setText(QString("0"));
    ui->num5->setText(QString("0"));
    ui->num6->setText(QString("0"));
    ui->num7->setText(QString("0"));
}

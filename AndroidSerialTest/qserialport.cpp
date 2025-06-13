#include "qserialport.h"
#include <QDebug>

static char UsbSerial_jniClassName[] {"org/qtproject/jniusbserial/JniUsbSerial"};

static void jniDeviceNewData(JNIEnv *envA, jobject thizA, jlong classPoint, jbyteArray dataA)
{
    Q_UNUSED(thizA);

    if (classPoint != 0)
    {
        jbyte *bytesL = envA->GetByteArrayElements(dataA, NULL);
        jsize lenL = envA->GetArrayLength(dataA);
        static_cast<QSerialPort *>(reinterpret_cast<void *>(classPoint))->newDataArrived(reinterpret_cast<char *>(bytesL), lenL);
        envA->ReleaseByteArrayElements(dataA, bytesL, JNI_ABORT);
    }
}

static void jniDeviceException(JNIEnv *envA, jobject thizA, jlong classPoint, jstring messageA)
{
    Q_UNUSED(thizA);

    if (classPoint != 0)
    {
        const char *stringL = envA->GetStringUTFChars(messageA, NULL);
        QString strL = QString::fromUtf8(stringL);
        envA->ReleaseStringUTFChars(messageA, stringL);
        if (envA->ExceptionCheck())
            envA->ExceptionClear();
        static_cast<QSerialPort *>(reinterpret_cast<void *>(classPoint))->exceptionArrived(strL);
    }
}


QSerialPort::QSerialPort()
{
    readBufferMaxSize = 16384;

    isConnected = false;
    m_portName = "";
    m_baudRate = Baud115200;
    m_dataBits = Data8;
    m_parity = NoParity;
    m_stopBits = OneStop;

    JNINativeMethod methodsL[] {{"nativeDeviceNewData", "(J[B)V", reinterpret_cast<void *>(jniDeviceNewData)},
                                        {"nativeDeviceException", "(JLjava/lang/String;)V", reinterpret_cast<void *>(jniDeviceException)}};

    QJniEnvironment envL;
    QJniObject javaClassL(UsbSerial_jniClassName);
    jclass objectClassL = envL->GetObjectClass(javaClassL.object<jobject>());

    jint valL = envL->RegisterNatives(objectClassL, methodsL, sizeof(methodsL) / sizeof(methodsL[0]));
    envL->DeleteLocalRef(objectClassL);

    if (envL->ExceptionCheck())
        envL->ExceptionClear();

//    if (valL < 0)
//        return false;
    Q_UNUSED(valL);
}

bool QSerialPort::setBaudRate(qint32 baudRate)
{
    m_baudRate = baudRate;

    return setParameters();
}

void QSerialPort::newDataArrived(char *bytesA, int lengthA)
{
    qDebug() << "QSerialPort::newDataArrived - Received data of length:" << lengthA;
    
    // Debug: Print the first few bytes as hex
    QString hexData;
    int bytesToShow = qMin(lengthA, 20); // Show up to 20 bytes
    for (int i = 0; i < bytesToShow; i++) {
        hexData += QString("%1 ").arg((unsigned char)bytesA[i], 2, 16, QChar('0'));
    }
    qDebug() << "QSerialPort::newDataArrived - Data (hex):" << hexData;
    
    int bytesToReadL = lengthA;

    if (readBufferMaxSize && (bytesToReadL > (readBufferMaxSize - readBuffer.size()))) {
        bytesToReadL = readBufferMaxSize - readBuffer.size();
        qDebug() << "QSerialPort::newDataArrived - Buffer limit reached, truncating to:" << bytesToReadL;
        if (bytesToReadL <= 0) {
            qDebug() << "QSerialPort::newDataArrived - Buffer full, stopping read thread";
            stopReadThread();
            return;
        }
    }

    readBuffer.open(QIODevice::ReadWrite);
    readBuffer.seek(readBuffer.size());
    qint64 bytesWritten = readBuffer.write(bytesA, bytesToReadL);
    readBuffer.close();
    
    qDebug() << "QSerialPort::newDataArrived - Wrote" << bytesWritten << "bytes to buffer, emitting readyRead()";
    
    emit readyRead();
}

void QSerialPort::exceptionArrived(QString strA)
{
    Q_UNUSED(strA);
    emit errorOccurred(ResourceError);
}

void QSerialPort::stopReadThread()
{
    QJniObject java_portName = QJniObject::fromString(m_portName);
    QJniObject::callStaticMethod<void>(UsbSerial_jniClassName,
                                              "stopIoManager",
                                              "(Ljava/lang/String;)V",
                                              java_portName.object<jstring>());
}

void QSerialPort::startReadThread()
{
    QJniObject java_portName = QJniObject::fromString(m_portName);
    QJniObject::callStaticMethod<void>(UsbSerial_jniClassName,
                                              "startIoManager",
                                              "(Ljava/lang/String;J)V",
                                              java_portName.object<jstring>(),
                                       reinterpret_cast<jlong>(this));
}

qint64 QSerialPort::bytesAvailable()
{
    return readBuffer.size();
}

QByteArray QSerialPort::read(qint64 maxlen)
{
    if (maxlen > readBuffer.size())
    {
        return readAll();
    }

    readBuffer.open(QIODevice::ReadWrite);
    const QByteArray &data = readBuffer.data();
    const QByteArray firstNBytes = data.left(maxlen);
    readBuffer.buffer().remove(0, maxlen);
    readBuffer.close();

    return firstNBytes;
}

QByteArray QSerialPort::readAll()
{
    readBuffer.open(QIODevice::ReadWrite);
    const QByteArray data = readBuffer.data();
    readBuffer.buffer().clear();
    readBuffer.close();

    return data;
}

QByteArray QSerialPort::readLine()
{
    readBuffer.open(QIODevice::ReadWrite);
    const QByteArray data = readBuffer.readLine();
    readBuffer.buffer().remove(0, data.length());
    readBuffer.close();

    return data;
}

bool QSerialPort::isOpen()
{
    return isConnected;
}

void QSerialPort::close()
{
    if (m_portName == "")
    {
        isConnected = false;
        return;
    }

    QJniObject java_portName = QJniObject::fromString(m_portName);
    jboolean resultL = QJniObject::callStaticMethod<jboolean>(UsbSerial_jniClassName,
                                                                         "close",
                                                                         "(Ljava/lang/String;)Z",
                                                                         java_portName.object<jstring>());

    if (resultL)
    {
        isConnected = false;
    }
}

bool QSerialPort::open(QIODeviceBase::OpenMode mode)
{
    Q_UNUSED(mode);
    if (m_portName == "")
    {
        qDebug() << "QSerialPort::open - Error: Port name is empty";
        return false;
    }

    qDebug() << "QSerialPort::open - Attempting to open port:" << m_portName;
    QJniObject java_portName = QJniObject::fromString(m_portName);
    jint resultL = QJniObject::callStaticMethod<jint>(UsbSerial_jniClassName,
                                                                          "open",
                                                                          "(Ljava/lang/String;J)I",
                                                                          java_portName.object<jstring>(),
                                                               reinterpret_cast<jlong>(this));

    qDebug() << "QSerialPort::open - JNI open result:" << resultL;
    
    if (resultL == 0)
    {
        qDebug() << "QSerialPort::open - Failed to open port";
        return false;
    }
    else
    {
        qDebug() << "QSerialPort::open - Port opened successfully";
        isConnected = true;
        if (!setParameters())
        {
            qDebug() << "QSerialPort::open - Failed to set parameters";
            isConnected = false;
            return false;
        }
        qDebug() << "QSerialPort::open - Parameters set successfully";
        return true;
    }
}

qint64 QSerialPort::write(const char *data, qint64 maxSize)
{
    if (m_portName == "")
    {
        emit errorOccurred(QSerialPort::NotOpenError);
        return 0;
    }

    if (!isConnected)
    {
        emit errorOccurred(QSerialPort::NotOpenError);
        return 0;
    }

    QJniEnvironment envL;
    QJniObject java_portName = QJniObject::fromString(m_portName);
    jbyteArray jarrayL = envL->NewByteArray(maxSize);
    envL->SetByteArrayRegion(jarrayL, 0, maxSize, (jbyte *)data);
    int resultL = QJniObject::callStaticMethod<jint>(UsbSerial_jniClassName,
                                                            "write",
                                                            "(Ljava/lang/String;[BI)I",
                                                            java_portName.object<jstring>(),
                                                            jarrayL,
                                                            2000);

    if (envL->ExceptionCheck())
    {
        envL->ExceptionClear();
        emit errorOccurred(QSerialPort::UnknownError);
        envL->DeleteLocalRef(jarrayL);
        return 0;
    }

    envL->DeleteLocalRef(jarrayL);

    if (resultL == 0)
    {
        emit errorOccurred(QSerialPort::WriteError);
        return 0;
    }

    return maxSize;
}

void QSerialPort::setPortName(const QString &name)
{
    m_portName = name;
}

QString QSerialPort::portName() const
{
    return m_portName;
}

bool QSerialPort::setParameters()
{
    if (isConnected == true)
    {
        QJniObject java_portName = QJniObject::fromString(m_portName);

        jboolean resultL = QJniObject::callStaticMethod<jboolean>(UsbSerial_jniClassName,
                                           "setParameters",
                                           "(Ljava/lang/String;IIII)Z",
                                            java_portName.object<jstring>(),
                                               m_baudRate,
                                               m_dataBits,
                                               m_stopBits,
                                               m_parity);

        return resultL;
    }

    return true;
}



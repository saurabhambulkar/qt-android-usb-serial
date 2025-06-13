#include "serialporthandler.h"
#include <QDateTime>

#ifdef Q_OS_ANDROID
#include <QCoreApplication>
#include <QJniEnvironment>
#include <QJniObject>
#include <QStringList>
#include <QMap>
#endif

// Define static constants with values
const int SerialPortHandler::VENDOR_ID = 0x1A86;  // CH340 vendor ID
const int SerialPortHandler::PRODUCT_ID = 0x7523; // CH340 product ID

// Static instance pointer for JNI callbacks
static SerialPortHandler* s_instance = nullptr;

SerialPortHandler::SerialPortHandler(QObject *parent)
    : QObject(parent)
    , m_serialPort(new QSerialPort())
    , m_connected(false)
    , m_lastError("")
    , m_receivedData("")
    , m_baudRate(9600)
    , m_deviceName("")
{
    // Store instance for JNI callbacks
    s_instance = this;
    
    // Connect QSerialPort signals using old-style connections
    connect(m_serialPort, SIGNAL(readyRead()), this, SLOT(onReadyRead()));
    connect(m_serialPort, SIGNAL(errorOccurred(QSerialPort::SerialPortError)), this, SLOT(onSerialError(QSerialPort::SerialPortError)));
    
    // Initialize device list
    refreshDeviceList();
    
#ifdef Q_OS_ANDROID
    // Register native methods for JNI callbacks
    JNINativeMethod methods[] = {
        {"javaResponseReady", "([B)V", reinterpret_cast<void*>(SerialPortHandler::javaResponseReady)},
        {"javaConnectedStateChanged", "(Z)V", reinterpret_cast<void*>(SerialPortHandler::javaConnectedStateChanged)},
        {"javaErrorOccurred", "(Ljava/lang/String;)V", reinterpret_cast<void*>(SerialPortHandler::javaErrorOccurred)},
        {"javaDeviceAttached", "(Z)V", reinterpret_cast<void*>(SerialPortHandler::javaDeviceAttached)}
    };

    QJniEnvironment env;
    jclass clazz = env->FindClass("org/qtproject/example/SerialHelper");
    if (clazz) {
        env->RegisterNatives(clazz, methods, sizeof(methods) / sizeof(methods[0]));
        env->DeleteLocalRef(clazz);
    } else {
        m_lastError = "Failed to find SerialHelper Java class";
        emit errorOccurred(m_lastError);
    }
#endif
}

SerialPortHandler::~SerialPortHandler()
{
    // Clean up resources
    if (m_serialPort) {
        if (m_serialPort->isOpen()) {
            m_serialPort->close();
        }
        delete m_serialPort;
    }
}

#ifdef Q_OS_ANDROID
void SerialPortHandler::initializeAndroidContext(jobject context)
{
    // Create a QJniObject from the jobject context
    QJniObject jniContext(context);
    if (!jniContext.isValid()) {
        qWarning() << "Invalid Android Context passed to SerialPortHandler";
        return;
    }
    
    qDebug() << "Initializing Android Context in SerialPortHandler";
    
    // Initialize SerialHelper class with Android Context
    bool serialHelperInitialized = false;
    
    // Call SerialHelper.init() with the Android Context
    serialHelperInitialized = QJniObject::callStaticMethod<jboolean>(
        "org/qtproject/example/SerialHelper",
        "init",
        "(Landroid/content/Context;)Z",
        context);
        
    if (serialHelperInitialized) {
        qDebug() << "Successfully initialized SerialHelper with Android Context";
    } else {
        qWarning() << "SerialHelper init returned false or failed";
    }
    
    // Also initialize JniUsbSerial with Android Context
    bool jniUsbSerialInitialized = false;
    jboolean jniUsbSerialResult = QJniObject::callStaticMethod<jboolean>(
        "org/qtproject/jniusbserial/JniUsbSerial",
        "init",
        "(Landroid/content/Context;)Z",
        context);
    
    // For JniUsbSerial, we already have the boolean result
    jniUsbSerialInitialized = jniUsbSerialResult;
    
    if (jniUsbSerialInitialized) {
        qDebug() << "Successfully initialized JniUsbSerial with Android Context";
    } else {
        qWarning() << "JniUsbSerial init returned false or failed";
    }
    
    if (!serialHelperInitialized && !jniUsbSerialInitialized) {
        qWarning() << "Failed to initialize both SerialHelper and JniUsbSerial with Android Context";
        emit errorOccurred("Failed to initialize Android USB serial components");
    }
}
#endif

QList<int> SerialPortHandler::availableBaudRates() const
{
    return QList<int>() << 9600 << 19200 << 38400 << 57600 << 115200 << 230400 << 460800 << 921600;
}

void SerialPortHandler::setBaudRate(int rate)
{
    if (m_baudRate != rate) {
        m_baudRate = rate;
        emit baudRateChanged(m_baudRate);
        
        // If already connected, update the baudrate on the device
        if (m_connected) {
#ifdef Q_OS_ANDROID
            m_serialPort->setBaudRate(m_baudRate);
#endif
        }
    }
}

void SerialPortHandler::refreshDeviceList()
{
#ifdef Q_OS_ANDROID
    // Clear previous device list
    m_availableDevices.clear();
    m_deviceMap.clear();
    m_deviceNodePaths.clear();
    m_devicePermissions.clear();
    
    qDebug() << "[" << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz") << "] Refreshing device list";
    
    // Get available devices using JniUsbSerial
    QJniObject devicesArray = QJniObject::callStaticObjectMethod(
        "org/qtproject/jniusbserial/JniUsbSerial",
        "availableDevicesInfo",
        "()[Ljava/lang/String;"
    );
    
    if (devicesArray.isValid()) {
        QJniEnvironment env;
        jobjectArray array = devicesArray.object<jobjectArray>();
        if (array) {
            jsize size = env->GetArrayLength(array);
            qDebug() << "Found" << size << "USB devices";
            
            for (jsize i = 0; i < size; i++) {
                QJniObject deviceInfo = env->GetObjectArrayElement(array, i);
                if (deviceInfo.isValid()) {
                    QString deviceInfoStr = deviceInfo.toString();
                    QStringList parts = deviceInfoStr.split(":");
                    
                    // Format: devicePath:driverType:manufacturer:productId:vendorId:deviceNodePath:hasPermission
                    if (parts.size() >= 7) {
                        QString devicePath = parts[0];
                        QString deviceType = parts[1];
                        QString manufacturer = parts[2];
                        QString productId = parts[3];
                        QString vendorId = parts[4];
                        QString deviceNodePath = parts[5];
                        bool hasPermission = (parts[6] == "true");
                        
                        // Create a user-friendly display name with device node path
                        QString displayName;
                        if (manufacturer != "Unknown" && !manufacturer.isEmpty()) {
                            displayName = manufacturer + " (" + deviceType + ", " + deviceNodePath + ")";
                        } else {
                            displayName = deviceType + " " + vendorId + ":" + productId + " (" + deviceNodePath + ")";
                        }
                        
                        qDebug() << "Device:" << displayName << "Path:" << devicePath << "Has Permission:" << hasPermission;
                        
                        m_availableDevices.append(displayName);
                        m_deviceMap[displayName] = devicePath;
                        m_deviceNodePaths[displayName] = deviceNodePath;
                        m_devicePermissions[displayName] = hasPermission;
                    } else {
                        qWarning() << "Invalid device info format:" << deviceInfoStr;
                    }
                }
            }
        } else {
            qWarning() << "Failed to get device array object";
        }
    } else {
        qWarning() << "Failed to get devices array from JniUsbSerial";
    }
    
    emit availableDevicesChanged();
    emit deviceNodePathsChanged();
#endif
}

void SerialPortHandler::setCurrentDevice(const QString &device)
{
    if (m_deviceName != device) {
        m_deviceName = device;
        emit currentDeviceChanged(device);
        
        // Also emit signals for the device node path and permission status
        emit currentDeviceNodePathChanged(currentDeviceNodePath());
        emit currentDevicePermissionChanged(currentDeviceHasPermission());
    }
}

void SerialPortHandler::connectToDevice()
{
    if (m_connected) {
        return;
    }
    
#ifdef Q_OS_ANDROID
    // Check if we have a device selected
    if (m_deviceName.isEmpty() || !m_deviceMap.contains(m_deviceName)) {
        m_lastError = "No device selected";
        emit errorOccurred(m_lastError);
        return;
    }
    
    // Get the device path from the map
    QString devicePath = m_deviceMap[m_deviceName];
    
    // Set device parameters
    m_serialPort->setPortName(devicePath);
    m_serialPort->setBaudRate(m_baudRate);
    
    // Open the device using JniUsbSerial
    if (m_serialPort->open(QIODevice::ReadWrite)) {
        m_connected = true;
        emit connectedChanged(m_connected);
    } else {
        m_lastError = "Failed to open serial port";
        emit errorOccurred(m_lastError);
    }
#else
    m_lastError = "Serial port functionality is only available on Android";
    emit errorOccurred(m_lastError);
#endif
}

void SerialPortHandler::disconnectDevice()
{
    if (!m_connected) {
        return;
    }
    
#ifdef Q_OS_ANDROID
    // Close the serial port
    m_serialPort->close();
    m_connected = false;
    emit connectedChanged(m_connected);
#endif
}

void SerialPortHandler::sendCommand(const QString &command)
{
#ifdef Q_OS_ANDROID
    if (!m_connected) {
        m_lastError = "Not connected to device";
        emit errorOccurred(m_lastError);
        return;
    }

    // Write data to the serial port
    QByteArray data = command.toUtf8();
    m_serialPort->write(data.constData(), data.size());
#else
    m_lastError = "Serial port functionality is only available on Android";
    emit errorOccurred(m_lastError);
#endif
}

// Static JNI callback methods
void SerialPortHandler::javaResponseReady(JNIEnv *env, jobject /* obj */, jbyteArray byteArray)
{
    if (!s_instance) {
        qWarning("SerialPortHandler instance not available for JNI callback");
        return;
    }

    // Convert Java byte array to QByteArray
    jsize len = env->GetArrayLength(byteArray);
    jbyte *data = env->GetByteArrayElements(byteArray, nullptr);
    QByteArray result(reinterpret_cast<const char*>(data), len);
    env->ReleaseByteArrayElements(byteArray, data, JNI_ABORT);

    // Forward to the instance method
    s_instance->onResponseReady(result);
}

void SerialPortHandler::javaConnectedStateChanged(JNIEnv * /* env */, jobject /* obj */, jboolean state)
{
    if (!s_instance) {
        qWarning("SerialPortHandler instance not available for JNI callback");
        return;
    }

    s_instance->onConnectedStateChanged(state);
}

void SerialPortHandler::javaErrorOccurred(JNIEnv *env, jobject /* obj */, jstring error)
{
    if (!s_instance) {
        qWarning("SerialPortHandler instance not available for JNI callback");
        return;
    }

    // Convert Java string to QString
    const char *errorStr = env->GetStringUTFChars(error, nullptr);
    QString qError = QString::fromUtf8(errorStr);
    env->ReleaseStringUTFChars(error, errorStr);

    s_instance->onErrorOccurred(qError);
}

void SerialPortHandler::javaDeviceAttached(JNIEnv * /* env */, jobject /* obj */, jboolean state)
{
    if (!s_instance) {
        qWarning("SerialPortHandler instance not available for JNI callback");
        return;
    }

    s_instance->onDeviceAttached(state);
}

#ifndef Q_OS_ANDROID
// Stub implementations for non-Android platforms
void SerialPortHandler::javaResponseReady(JNIEnv* env, jobject obj, jbyteArray byteArray) {}
void SerialPortHandler::javaConnectedStateChanged(JNIEnv* env, jobject obj, jboolean state) {}
void SerialPortHandler::javaErrorOccurred(JNIEnv* env, jobject obj, jstring error) {}
void SerialPortHandler::javaDeviceAttached(JNIEnv* env, jobject obj, jboolean state) {}
#endif

// Instance methods for handling JNI callbacks
void SerialPortHandler::onResponseReady(const QByteArray &data)
{
    emit dataReceived(data);
}

// New methods for QSerialPort
void SerialPortHandler::onReadyRead()
{
    // Read all available data from the serial port
    QByteArray data = m_serialPort->readAll();
    m_receivedData += QString::fromUtf8(data);
    emit dataReceived(data);
}

void SerialPortHandler::onSerialError(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::NoError) {
        return;
    }
    
    // Handle serial port errors
    switch (error) {
    case QSerialPort::DeviceNotFoundError:
        m_lastError = "Device not found";
        break;
    case QSerialPort::PermissionError:
        m_lastError = "Permission error";
        break;
    case QSerialPort::OpenError:
        m_lastError = "Failed to open device";
        break;
    case QSerialPort::NotOpenError:
        m_lastError = "Device not open";
        break;
    case QSerialPort::WriteError:
        m_lastError = "Write error";
        break;
    case QSerialPort::ReadError:
        m_lastError = "Read error";
        break;
    case QSerialPort::ResourceError:
        m_lastError = "Resource error";
        break;
    case QSerialPort::UnsupportedOperationError:
        m_lastError = "Unsupported operation";
        break;
    case QSerialPort::TimeoutError:
        m_lastError = "Timeout error";
        break;
    default:
        m_lastError = "Unknown error";
        break;
    }
    
    emit errorOccurred(m_lastError);
    
    // If we were connected, update the connection state
    if (m_connected) {
        m_connected = false;
        emit connectedChanged(m_connected);
    }
}

void SerialPortHandler::onConnectedStateChanged(bool state)
{
    if (m_connected != state) {
        m_connected = state;
        emit connectedChanged(m_connected);
    }
}

void SerialPortHandler::onErrorOccurred(const QString &error)
{
    m_lastError = error;
    emit errorOccurred(m_lastError);
}

void SerialPortHandler::onDeviceAttached(bool attached)
{
    emit deviceAttached(attached);
    
    // Automatically try to connect when device is attached
    if (attached) {
        connectToDevice();
    } else if (m_connected) {
        // Device was detached while connected
        m_connected = false;
        emit connectedChanged(false);
    }
}

QString SerialPortHandler::currentDeviceNodePath() const
{
    if (m_deviceName.isEmpty()) {
        return QString();
    }
    
    return m_deviceNodePaths.value(m_deviceName, QString());
}

bool SerialPortHandler::currentDeviceHasPermission() const
{
    if (m_deviceName.isEmpty()) {
        return false;
    }
    
    return m_devicePermissions.value(m_deviceName, false);
}

QString SerialPortHandler::getDeviceNodePath(const QString &device) const
{
    return m_deviceNodePaths.value(device, QString());
}

bool SerialPortHandler::getDeviceHasPermission(const QString &device) const
{
    return m_devicePermissions.value(device, false);
}

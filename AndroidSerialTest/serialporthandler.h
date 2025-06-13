#ifndef SERIALPORTHANDLER_H
#define SERIALPORTHANDLER_H

#include <QObject>
#include <QString>
#include <QByteArray>
#include <QList>
#include <QMap>
#include "qserialport.h"

// Forward declare JNI types to avoid moc issues
#ifdef Q_OS_ANDROID
#include <jni.h>
#include <QJniObject>
#include <QJniEnvironment>
#else
// Define stub types for non-Android platforms
typedef void* JNIEnv;
typedef void* jobject;
typedef void* jbyteArray;
typedef void* jstring;
typedef bool jboolean;
#endif

class SerialPortHandler : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool connected READ isConnected NOTIFY connectedChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY errorOccurred)
    Q_PROPERTY(int baudRate READ baudRate WRITE setBaudRate NOTIFY baudRateChanged)
    Q_PROPERTY(QStringList availableDevices READ availableDevices NOTIFY availableDevicesChanged)
    Q_PROPERTY(QString currentDevice READ currentDevice WRITE setCurrentDevice NOTIFY currentDeviceChanged)
    Q_PROPERTY(QString currentDeviceNodePath READ currentDeviceNodePath NOTIFY currentDeviceNodePathChanged)
    Q_PROPERTY(bool currentDeviceHasPermission READ currentDeviceHasPermission NOTIFY currentDevicePermissionChanged)

public:
    // USB device identifiers - customize these for your specific device
    static const int VENDOR_ID;  // CH340 vendor ID (defined in .cpp)
    static const int PRODUCT_ID; // CH340 product ID (defined in .cpp)
    // Other common USB-serial adapters:
    // FTDI: VID=0x0403, PID=0x6001
    // CP2102: VID=0x10C4, PID=0xEA60
    // PL2303: VID=0x067B, PID=0x2303

    explicit SerialPortHandler(QObject *parent = nullptr);
    ~SerialPortHandler();

    bool isConnected() const { return m_connected; }
    QString lastError() const { return m_lastError; }
    int baudRate() const { return m_baudRate; }
    void setBaudRate(int rate);
    
    // Device selection
    QStringList availableDevices() const { return m_availableDevices; }
    QString currentDevice() const { return m_deviceName; }
    void setCurrentDevice(const QString &device);
    Q_INVOKABLE void refreshDeviceList();
    
    // Device node path and permission access
    QString currentDeviceNodePath() const;
    bool currentDeviceHasPermission() const;
    Q_INVOKABLE QString getDeviceNodePath(const QString &device) const;
    Q_INVOKABLE bool getDeviceHasPermission(const QString &device) const;

    // Common baudrates
    Q_INVOKABLE QList<int> availableBaudRates() const;
    
#ifdef Q_OS_ANDROID
    // Method to initialize Android Context
    void initializeAndroidContext(jobject context);
#endif

    // Static methods for JNI callbacks - defined the same way for all platforms
    // but with different underlying types depending on platform
    static void javaResponseReady(JNIEnv *env, jobject obj, jbyteArray byteArray);
    static void javaConnectedStateChanged(JNIEnv *env, jobject obj, jboolean state);
    static void javaErrorOccurred(JNIEnv *env, jobject obj, jstring error);
    static void javaDeviceAttached(JNIEnv *env, jobject obj, jboolean state);

public slots:
    // Methods exposed to QML
    void connectToDevice();
    void disconnectDevice();
    void sendCommand(const QString &command);

signals:
    void dataReceived(const QByteArray &data);
    void connectedChanged(bool connected);
    void errorOccurred(const QString &error);
    void deviceAttached(bool attached);
    void baudRateChanged(int baudRate);
    void availableDevicesChanged();
    void currentDeviceChanged(const QString &device);
    void deviceNodePathsChanged();
    void currentDeviceNodePathChanged(const QString &nodePath);
    void currentDevicePermissionChanged(bool hasPermission);

private slots:
    // Slot for handling QSerialPort readyRead signal
    void onReadyRead();
    void onSerialError(QSerialPort::SerialPortError error);

    // Helper methods for handling JNI callbacks
    void onResponseReady(const QByteArray &data);
    void onConnectedStateChanged(bool state);
    void onErrorOccurred(const QString &error);
    void onDeviceAttached(bool attached);

private:
    // Member variables
    QSerialPort *m_serialPort;
    bool m_connected;
    QString m_lastError;
    QString m_receivedData;
    int m_baudRate;
    
    // Device selection
    QString m_deviceName;
    QStringList m_availableDevices;
    QMap<QString, QString> m_deviceMap; // Maps display names to device paths
    QMap<QString, QString> m_deviceNodePaths; // Maps display names to device node paths
    QMap<QString, bool> m_devicePermissions; // Maps display names to permission status
};

#endif // SERIALPORTHANDLER_H

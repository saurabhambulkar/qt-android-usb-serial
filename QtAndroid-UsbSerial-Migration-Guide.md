# QtAndroid-UsbSerial Migration Guide

This document outlines the step-by-step process to migrate the QtAndroid-UsbSerial library's asynchronous I/O implementation into the AndroidSerialTest project.

## Migration Plan

### 1. File Placement

Place the QtAndroid-UsbSerial files in the following locations:

- Java files:
  ```
  /Users/Workspace/AndroidSerial/serialport/AndroidSerialTest/android/src/org/qtproject/jniusbserial/JniUsbSerial.java
  /Users/Workspace/AndroidSerial/serialport/AndroidSerialTest/android/src/org/qtproject/jniusbserial/SerialInputOutputManager.java
  ```

- C++ files:
  ```
  /Users/Workspace/AndroidSerial/serialport/AndroidSerialTest/qserialport.cpp
  /Users/Workspace/AndroidSerial/serialport/AndroidSerialTest/qserialport.h
  ```

### 2. Project File Changes (AndroidSerialTest.pro)

Add the following to your project file:

```qmake
# Add the new C++ files
SOURCES += qserialport.cpp
HEADERS += qserialport.h

# Make sure Java files are included
ANDROID_PACKAGE_SOURCE_DIR = $$PWD/android

# Add JNI include path if needed
android: QT += core-private
```

### 3. SerialPortHandler Changes

Replace the current SerialPortHandler implementation with one that uses QSerialPort:

**serialporthandler.h**
```cpp
#ifndef SERIALPORTHANDLER_H
#define SERIALPORTHANDLER_H

#include <QObject>
#include <QJniObject>
#include <QList>
#include "qserialport.h" // Add this include

class SerialPortHandler : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
    Q_PROPERTY(QString receivedData READ receivedData NOTIFY receivedDataChanged)
    Q_PROPERTY(int baudRate READ baudRate WRITE setBaudRate NOTIFY baudRateChanged)

public:
    explicit SerialPortHandler(QObject *parent = nullptr);
    ~SerialPortHandler();

    Q_INVOKABLE void connectToDevice();
    Q_INVOKABLE void disconnectFromDevice();
    Q_INVOKABLE void sendCommand(const QString &command);
    Q_INVOKABLE QList<int> availableBaudRates();

    bool connected() const;
    QString receivedData() const;
    int baudRate() const;
    void setBaudRate(int baudRate);

signals:
    void connectedChanged();
    void receivedDataChanged();
    void baudRateChanged();

private slots:
    void onReadyRead(); // New slot for handling readyRead signal

private:
    QSerialPort *m_serialPort; // Use QSerialPort instead of JNI calls
    bool m_connected;
    QString m_receivedData;
    int m_baudRate;
};

#endif // SERIALPORTHANDLER_H
```

**serialporthandler.cpp**
```cpp
#include "serialporthandler.h"
#include <QJniEnvironment>
#include <QDebug>

SerialPortHandler::SerialPortHandler(QObject *parent)
    : QObject(parent)
    , m_serialPort(new QSerialPort())
    , m_connected(false)
    , m_receivedData("")
    , m_baudRate(9600)
{
    // Connect signals from QSerialPort
    connect(m_serialPort, &QSerialPort::readyRead, this, &SerialPortHandler::onReadyRead);
    connect(m_serialPort, &QSerialPort::errorOccurred, this, [this](QSerialPort::SerialPortError error) {
        qDebug() << "Serial port error:" << error;
        if (m_connected) {
            m_connected = false;
            emit connectedChanged();
        }
    });
}

SerialPortHandler::~SerialPortHandler()
{
    if (m_connected) {
        disconnectFromDevice();
    }
    delete m_serialPort;
}

void SerialPortHandler::connectToDevice()
{
    if (m_connected)
        return;

    // Get available devices - this would need to be implemented separately
    // or we could use a fixed device name for simplicity
    QString deviceName = ""; // Would need to be determined dynamically
    
    m_serialPort->setPortName(deviceName);
    m_serialPort->setBaudRate(m_baudRate);
    
    if (m_serialPort->open(QIODevice::ReadWrite)) {
        m_connected = true;
        emit connectedChanged();
    }
}

void SerialPortHandler::disconnectFromDevice()
{
    if (!m_connected)
        return;

    m_serialPort->close();
    m_connected = false;
    emit connectedChanged();
}

void SerialPortHandler::sendCommand(const QString &command)
{
    if (!m_connected)
        return;

    QByteArray data = command.toUtf8();
    m_serialPort->write(data.constData(), data.size());
}

void SerialPortHandler::onReadyRead()
{
    QByteArray data = m_serialPort->readAll();
    m_receivedData += QString::fromUtf8(data);
    emit receivedDataChanged();
}

QList<int> SerialPortHandler::availableBaudRates()
{
    return QList<int>() << 1200 << 2400 << 4800 << 9600 << 19200 << 38400 << 57600 << 115200;
}

bool SerialPortHandler::connected() const
{
    return m_connected;
}

QString SerialPortHandler::receivedData() const
{
    return m_receivedData;
}

int SerialPortHandler::baudRate() const
{
    return m_baudRate;
}

void SerialPortHandler::setBaudRate(int baudRate)
{
    if (m_baudRate == baudRate)
        return;

    m_baudRate = baudRate;
    
    if (m_connected) {
        m_serialPort->setBaudRate(m_baudRate);
    }
    
    emit baudRateChanged();
}
```

### 4. Main.cpp Changes

Update main.cpp to initialize the serial port handler:

```cpp
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include "serialporthandler.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    QQmlApplicationEngine engine;
    
    SerialPortHandler serialHandler;
    engine.rootContext()->setContextProperty("serialHandler", &serialHandler);

    const QUrl url(QStringLiteral("qrc:/main.qml"));
    engine.load(url);

    return app.exec();
}
```

### 5. Java Side Integration

1. Create the necessary directory structure:
```
mkdir -p /Users/Workspace/AndroidSerial/serialport/AndroidSerialTest/android/src/org/qtproject/jniusbserial/
```

2. Copy the Java files:
```
cp /Users/Workspace/AndroidSerial/QtAndroid-UsbSerial/android/src/org/qtproject/jniusbserial/JniUsbSerial.java /Users/Workspace/AndroidSerial/serialport/AndroidSerialTest/android/src/org/qtproject/jniusbserial/
cp /Users/Workspace/AndroidSerial/QtAndroid-UsbSerial/android/src/org/qtproject/jniusbserial/SerialInputOutputManager.java /Users/Workspace/AndroidSerial/serialport/AndroidSerialTest/android/src/org/qtproject/jniusbserial/
```

3. Update AndroidManifest.xml to ensure it has the necessary USB permissions:
```xml
<uses-feature android:name="android.hardware.usb.host" />
<uses-permission android:name="android.permission.USB_PERMISSION" />
```

### 6. Device Selection UI

Add a device selection UI in QML:

```qml
// Add to main.qml
ComboBox {
    id: deviceSelector
    model: serialHandler.availableDevices
    enabled: !serialHandler.connected
    // ...
}
```

And add the corresponding property to SerialPortHandler:

```cpp
// Add to serialporthandler.h
Q_PROPERTY(QStringList availableDevices READ availableDevices NOTIFY availableDevicesChanged)
public:
    QStringList availableDevices() const;
signals:
    void availableDevicesChanged();
```

### 7. Build.gradle Changes

Make sure your build.gradle includes the usb-serial-for-android library:

```gradle
dependencies {
    implementation 'com.github.mik3y:usb-serial-for-android:3.9.0'
}

repositories {
    maven { url "https://jitpack.io" }
}
```

## Key Benefits of Migration

1. **Asynchronous I/O**: Data is read continuously in the background instead of only after sending commands
2. **Signal-based Communication**: Uses Qt's signal/slot mechanism for event handling
3. **Multiple Device Support**: Can handle multiple USB serial devices
4. **Better Error Handling**: More comprehensive error reporting
5. **Clean Separation**: Better separation between Qt and Java layers

## Potential Challenges

1. **Device Selection**: The current implementation might need to be extended to support device selection
2. **Permissions**: USB permission handling might need adjustments
3. **Thread Safety**: Ensure thread-safe access to shared resources
4. **Error Handling**: Implement proper error handling for all operations

## Testing Strategy

After migration, test the following:

1. Device connection and disconnection
2. Baudrate changes
3. Continuous data reception
4. UI responsiveness during I/O operations
5. Error handling and recovery

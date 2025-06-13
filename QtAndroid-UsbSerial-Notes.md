# QtAndroid-UsbSerial: Asynchronous Serial I/O Implementation Notes

This document summarizes the asynchronous I/O implementation in the QtAndroid-UsbSerial library, which can be used as a reference for implementing similar functionality in the AndroidSerialTest project.

## Architecture Overview

The QtAndroid-UsbSerial library implements asynchronous, non-blocking serial port I/O using a dedicated background thread for continuous reading and writing operations. This approach prevents UI freezing while maintaining responsive serial communication.

### Key Components

#### Java Layer
1. **JniUsbSerial.java**
   - Main bridge between Qt/C++ and usb-serial-for-android
   - Manages device connections and I/O operations
   - Provides JNI callbacks to notify C++ of events

2. **SerialInputOutputManager.java**
   - Runs in a dedicated background thread
   - Continuously reads from and writes to the serial port
   - Uses a listener interface to notify of data reception and errors

#### C++ Layer
1. **qserialport.h/cpp**
   - Provides a Qt-style interface for serial port operations
   - Receives notifications from Java via JNI callbacks
   - Emits Qt signals for data reception and errors

### Asynchronous I/O Flow

1. **Initialization**
   - C++ creates QSerialPort object
   - JNI registers native methods for callbacks
   - Java creates SerialInputOutputManager

2. **Connection**
   - C++ calls open() which invokes Java via JNI
   - Java connects to USB device and starts SerialInputOutputManager
   - Background thread begins running

3. **Reading Data**
   - Background thread continuously reads from serial port
   - When data arrives, Java calls JNI callback
   - C++ processes data and emits readyRead() signal
   - Qt/QML UI updates without blocking

4. **Writing Data**
   - C++ calls write() which invokes Java via JNI
   - Java writes data to serial port
   - Background thread handles write operation

5. **Error Handling**
   - Exceptions in Java thread trigger JNI callback
   - C++ emits errorOccurred() signal
   - Qt/QML UI can display error messages

## Key Code Elements

### SerialInputOutputManager Thread Loop
```java
@Override
public void run() {
    // ...
    while (true) {
        if (getState() != State.RUNNING) {
            break;
        }
        step(); // Read/write operations
    }
    // ...
}
```

### JNI Callback for Data Reception
```java
private final static SerialInputOutputManager.Listener m_Listener =
    new SerialInputOutputManager.Listener() {
        @Override
        public void onNewData(byte[] data, long classPoint) {
            nativeDeviceNewData(classPoint, data);
        }
        
        @Override
        public void onRunError(Exception e, long classPoint) {
            nativeDeviceException(classPoint, e.getMessage());
        }
    };
```

### C++ Signal Emission
```cpp
void QSerialPort::newDataArrived(char *bytesA, int lengthA) {
    // Buffer the data
    readBuffer.write(bytesA, bytesToReadL);
    
    // Emit signal for Qt/QML
    emit readyRead();
}
```

## Implementation Strategy for AndroidSerialTest

To implement similar asynchronous I/O in the AndroidSerialTest project:

1. **Modify SerialHelper.java**
   - Add a SerialInputOutputManager class or adapt from QtAndroid-UsbSerial
   - Create a dedicated thread for continuous reading
   - Implement callbacks to notify C++ of events

2. **Update SerialPortHandler.cpp**
   - Add signal handling for asynchronous data reception
   - Implement buffer management for received data
   - Ensure thread-safe access to shared resources

3. **JNI Bridge**
   - Register native methods for callbacks
   - Pass C++ object pointer to Java for callbacks
   - Handle data conversion between Java and C++

4. **QML Interface**
   - Connect to readyRead() and error signals
   - Update UI asynchronously when data arrives

## Benefits

- **Non-blocking UI** - Serial operations don't freeze the UI thread
- **Real-time data reception** - Data is received as soon as it arrives
- **Efficient resource usage** - Thread priorities are optimized for I/O
- **Clean Qt integration** - Uses Qt's signal/slot mechanism for event handling

## References

- QtAndroid-UsbSerial repository files:
  - `/android/src/org/qtproject/jniusbserial/JniUsbSerial.java`
  - `/android/src/org/qtproject/jniusbserial/SerialInputOutputManager.java`
  - `/androidserial/qserialport.cpp`
  - `/androidserial/qserialport.h`

- usb-serial-for-android library:
  - https://github.com/mik3y/usb-serial-for-android

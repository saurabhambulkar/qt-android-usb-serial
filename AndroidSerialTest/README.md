# Android Serial Test

A Qt/QML application for Android that demonstrates asynchronous serial port connection, read, and write functionality using the usb-serial-for-android library.

## Overview

This application provides a simple interface to:
- Discover and select from available USB serial devices on Android
- Connect to USB serial devices using asynchronous I/O
- Send commands to the connected device
- Receive and display responses in real-time
- Handle USB device attachment and detachment events
- Configure serial port parameters (baudrate)

## Requirements

- Qt 6.8.3 or later
- Android SDK
- Android NDK 26.1 or later
- JDK 17 or later
- Gradle 8.10
- Android Gradle Plugin 8.6.0

## How It Works

Since QSerialPort is not officially supported on Android, this application uses a JNI bridge to connect to the [usb-serial-for-android](https://github.com/mik3y/usb-serial-for-android) library. The architecture is as follows:

1. **Java Layer**: Uses the usb-serial-for-android library to access USB serial devices and implements asynchronous I/O
2. **JNI Bridge**: Connects Java and C++ with callbacks for asynchronous data reception
3. **C++ Layer**: Exposes serial port functionality to QML using Qt signals and slots
4. **QML Layer**: Provides the user interface with real-time updates

### Detailed Code Flow

#### Device Discovery Process

1. **QML UI Initiates Device Discovery**:
   - Application starts or user clicks the "Refresh" button
   - QML calls `serialHandler.refreshDeviceList()`

2. **C++ Layer (SerialPortHandler)**:
   - `SerialPortHandler::refreshDeviceList()` is called
   - Calls Java method `JniUsbSerial.availableDevicesInfo()` using JNI

3. **Java Layer (JniUsbSerial)**:
   - Queries the Android USB manager for connected devices
   - Identifies device types (FTDI, CH340, CP210x, etc.)
   - Returns device information to C++ layer
   - C++ updates the device list model for QML

#### Connection Process

1. **QML UI Initiates Connection**:
   - User selects a device from the dropdown
   - User clicks the "Connect" button in the QML interface
   - QML calls `serialHandler.connectToDevice()`

2. **C++ Layer (SerialPortHandler)**:
   - `SerialPortHandler::connectToDevice()` is called
   - Gets the device path from the selected device
   - Calls `QSerialPort::open()` which uses JNI to open the device

3. **Java Layer (JniUsbSerial)**:
   - Opens connection to the device
   - Configures serial port parameters (baudrate, data bits, stop bits, parity)
   - Starts the SerialInputOutputManager in a background thread
   - Notifies C++ of connection status via JNI callback

4. **JNI Callback**:
   - `javaConnectedStateChanged()` is called from Java
   - Updates C++ state and emits Qt signal
   - QML UI updates to show connected status

#### Asynchronous Read Process

1. **Java Layer Receives Data in Background Thread**:
   - `SerialInputOutputManager` runs in a dedicated background thread
   - Continuously reads data from the USB serial device in a non-blocking way
   - When data is received, it calls the JNI callback `nativeDeviceNewData`

2. **JNI Bridge**:
   - `jniDeviceNewData` static method is called from Java
   - Converts Java byte array to QByteArray
   - Calls `QSerialPort::newDataArrived` method

3. **C++ Layer**:
   - `QSerialPort::newDataArrived` buffers the data and emits the `readyRead` signal
   - `SerialPortHandler::onReadyRead` slot is triggered
   - Reads the data from the buffer and emits the `dataReceived` signal

4. **QML Layer**:
   - Connection in QML receives the `dataReceived` signal
   - Updates the UI with the received data in real-time

#### Asynchronous Write Process

1. **QML UI Initiates Write**:
   - User enters a command and clicks "Send"
   - QML calls `serialHandler.sendCommand(command)`

2. **C++ Layer**:
   - `SerialPortHandler::sendCommand` is called
   - Converts the command to a QByteArray
   - Calls `QSerialPort::write()` to send the data

3. **Java Layer**:
   - `JniUsbSerial.write()` is called via JNI
   - Writes data to the USB serial device in a background thread
   - Returns control immediately without blocking

#### Reading Data

1. **Java Layer**:
   - When data is received in `sendCommand()`, it reads from the serial port
   - Data is read into a buffer with timeout
   - Received data is passed to C++ via `javaResponseReady()` JNI callback

2. **C++ Layer**:
   - `SerialPortHandler::javaResponseReady()` receives the byte array
   - Converts Java byte array to QByteArray
   - Emits `dataReceived()` signal with the response

3. **QML Layer**:
   - Connects to `dataReceived()` signal
   - Updates text area with received data

#### Writing Data

1. **QML UI Initiates Write**:
   - User enters command and clicks "Send"
   - QML calls `serialHandler.sendCommand(commandText)`

2. **C++ Layer**:
   - `SerialPortHandler::sendCommand()` is called with the command string
   - Converts QString to Java string
   - Calls Java method `SerialHelper.sendCommand()`

3. **Java Layer**:
   - Converts string to byte array
   - Writes data to serial port with timeout
   - Reads response (as described in Reading Data)

#### Baudrate Configuration

1. **QML UI**:
   - Displays dropdown with available baudrates
   - When user selects a baudrate, calls `serialHandler.setBaudRate(rate)`

2. **C++ Layer**:
   - Updates internal baudrate value
   - If connected, calls Java method to update baudrate on the device

3. **Java Layer**:
   - Updates baudrate on the active connection using `port.setParameters()`

## Supported USB Serial Adapters

The application is configured to work with common USB serial adapters including:
- CH340/CH341
- FTDI (FT232R, etc.)
- Prolific PL2303
- Silicon Labs CP210x
- Other CDC/ACM compliant devices

You can add more devices by modifying the `device_filter.xml` file.

## Integration with usb-serial-for-android

This project integrates the [usb-serial-for-android](https://github.com/mik3y/usb-serial-for-android) library to handle USB serial communication. Here's how the integration works:

### Adding the Library Dependency

The library is included as a Gradle dependency in `android/build.gradle`:

```gradle
repositories {
    google()
    mavenCentral()
    maven { url 'https://jitpack.io' }  // Required for usb-serial-for-android
}

dependencies {
    implementation 'com.github.mik3y:usb-serial-for-android:3.9.0'
}
```

### Key Classes Used from the Library

1. **UsbSerialProber**:
   - Used to discover USB serial devices
   - Custom `ProbeTable` configured to recognize specific vendor/product IDs
   - Example: `UsbSerialProber prober = new UsbSerialProber(customTable);`

2. **UsbSerialDriver**:
   - Represents a USB device that provides serial ports
   - Retrieved using `prober.findAllDrivers(manager)`

3. **UsbSerialPort**:
   - Represents an individual serial port provided by a driver
   - Used to open connections, configure parameters, and read/write data
   - Example: `port.setParameters(baudRate, 8, UsbSerialPort.STOPBITS_1, UsbSerialPort.PARITY_NONE);`

4. **FtdiSerialDriver**:
   - Driver for FTDI-based serial adapters
   - Used as a default driver class for custom device tables

### USB Permission Handling

The library requires USB permission which is handled through Android's permission system:

1. `AndroidManifest.xml` includes the USB permission:
   ```xml
   <uses-feature android:name="android.hardware.usb.host" />
   <uses-permission android:name="android.permission.USB_PERMISSION" />
   ```

2. Permission is requested in code using `PendingIntent` and `BroadcastReceiver`

3. Device filters in `res/xml/device_filter.xml` define which USB devices the app can connect to

## Building and Running

1. Open the project in Qt Creator
2. Configure the project for Android Qt 6.8.3 Clang arm64-v8a kit
3. Build and run on an Android device

## Usage

1. Connect a USB serial adapter to your Android device
2. Launch the application
3. Select the appropriate baudrate from the dropdown
4. Tap "Connect" to establish a connection
5. Enter commands in the text field and tap "Send"
6. View responses in the "Received Data" section

## Build Configuration Details

### Project Structure

- **AndroidSerialTest.pro**: Qt project file with Android-specific configurations
- **android/**: Contains Android-specific files
  - **AndroidManifest.xml**: Defines app permissions and components
  - **build.gradle**: Gradle build script with dependencies
  - **gradle.properties**: Gradle and Android build properties
  - **gradle/wrapper/**: Gradle wrapper files
  - **res/**: Android resources including device filters
  - **src/**: Java source files

### Key Build Configuration Files

#### AndroidSerialTest.pro

Important sections:
```qmake
ANDROID_PACKAGE_SOURCE_DIR = $$PWD/android
ANDROID_ABIS = arm64-v8a

DISTFILES += \
    android/AndroidManifest.xml \
    android/build.gradle \
    android/gradle.properties \
    android/gradle/wrapper/gradle-wrapper.jar \
    android/gradle/wrapper/gradle-wrapper.properties \
    android/gradlew \
    android/gradlew.bat \
    android/res/xml/device_filter.xml
```

#### android/build.gradle

Key configurations:
```gradle
plugins {
    id 'com.android.application'
}

android {
    compileSdkVersion androidCompileSdkVersion.toInteger()
    buildToolsVersion androidBuildToolsVersion
    namespace androidPackageName
    ndkVersion androidNdkVersion
    
    buildFeatures {
        aidl true
        buildConfig true
    }
    
    defaultConfig {
        minSdkVersion qtMinSdkVersion.toInteger()
        targetSdkVersion qtTargetSdkVersion.toInteger()
        ndk {
            abiFilters = qtTargetAbiList.split(',')            
        }
    }
    
    sourceSets {
        main {
            manifest.srcFile 'AndroidManifest.xml'
            java.srcDirs = [qtAndroidSourcesDir + '/src', 'src', 'java']
            aidl.srcDirs = [qtAndroidSourcesDir + '/aidl', 'aidl']
            res.srcDirs = [qtAndroidSourcesDir + '/res', 'res']
            resources.srcDirs = ['resources']
            renderscript.srcDirs = ['src']
            assets.srcDirs = ['assets']
            jniLibs.srcDirs = ['libs']
        }
    }
    
    compileOptions {
        sourceCompatibility JavaVersion.VERSION_11
        targetCompatibility JavaVersion.VERSION_11
    }
}

repositories {
    google()
    mavenCentral()
    maven { url 'https://jitpack.io' }
}

dependencies {
    implementation 'com.github.mik3y:usb-serial-for-android:3.9.0'
}
```

#### gradle.properties

Important settings:
```properties
org.gradle.jvmargs=-Xmx4096m -XX:MaxMetaspaceSize=1024m
org.gradle.parallel=true
android.useAndroidX=true
android.enableJetifier=true
```

#### gradle-wrapper.properties

```properties
distributionBase=GRADLE_USER_HOME
distributionPath=wrapper/dists
distributionUrl=https\://services.gradle.org/distributions/gradle-8.10-bin.zip
networkTimeout=10000
zipStoreBase=GRADLE_USER_HOME
zipStorePath=wrapper/dists
validateDistributionUrl=true
```

### Environment Requirements

- **ANDROID_SDK_ROOT**: Must point to Android SDK installation
- **ANDROID_NDK_ROOT**: Must point to Android NDK installation
- **JAVA_HOME**: Must point to JDK 17 or higher installation

## Customization

- Modify the vendor and product IDs in `serialporthandler.h` to match your specific USB serial device
- Add or remove baudrates in `SerialPortHandler::availableBaudRates()`
- Update the UI as needed in `main.qml`
- Add additional device filters in `android/res/xml/device_filter.xml`

## Troubleshooting

- Make sure your Android device supports USB OTG (On-The-Go)
- Check that you have the correct USB serial adapter driver
- Verify that your device is listed in the `device_filter.xml` file
- Enable USB debugging on your Android device
- Check the Android logcat for detailed error messages
- If build fails with Java errors, verify the usb-serial-for-android dependency is correctly included
- For Gradle errors, ensure you're using compatible versions of Gradle (8.10) and Android Gradle Plugin (8.6.0)
- If you encounter "incomplete type 'QVariant'" errors, ensure proper Qt headers are included

## Common Build Issues and Solutions

1. **Missing usb-serial-for-android classes**:
   - Ensure the JitPack repository is added in build.gradle
   - Verify the implementation line for the library is present

2. **Java version compatibility**:
   - Set compileOptions in build.gradle to use Java 11 compatibility
   - Use a JDK that's compatible with the Android Gradle Plugin

3. **Gradle version issues**:
   - Update gradle-wrapper.properties to use Gradle 8.10
   - Update the Android Gradle Plugin to 8.6.0

4. **Qt/JNI integration problems**:
   - Ensure proper JNI method signatures in SerialPortHandler
   - Use QJniObject and QJniEnvironment for JNI operations

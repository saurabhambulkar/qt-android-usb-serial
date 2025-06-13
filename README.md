# Qt Android USB Serial

A Qt-based Android application for USB serial communication with enhanced debugging capabilities.

## Features

- USB device detection and connection on Android
- Serial port configuration (baud rate, data bits, parity, etc.)
- Send and receive data via USB serial connection
- Enhanced logging throughout the data flow path
- Modern QML UI with:
  - Timestamped received data display
  - Auto-scroll control
  - Copy and clear functionality
  - Byte count tracking

## Architecture

The application uses a multi-layer architecture:
1. **Java Layer**: Handles Android USB Host API communication using the usb-serial-for-android library
2. **JNI Bridge**: Connects Java and C++ layers
3. **C++ Backend**: Qt-based business logic and serial port handling
4. **QML UI**: Modern user interface for interaction

## Requirements

- Qt 6.8.3 or later
- Android SDK with API level 34
- Android NDK
- JDK 8 or later

## Building

1. Open the project in Qt Creator
2. Configure for Android target
3. Build and deploy to Android device

## Usage

1. Connect a USB serial device to your Android phone/tablet using an OTG cable
2. Launch the application
3. Grant USB permission when prompted
4. Configure serial port settings
5. Connect to the device
6. Send and receive data

## Debugging

The application includes extensive logging:
- Java layer logs using Android Log
- C++ layer logs using qDebug()
- Data reception and transmission are logged with hex dumps

## License

[Your chosen license]

## Acknowledgements

- [usb-serial-for-android](https://github.com/mik3y/usb-serial-for-android) library
- Qt framework

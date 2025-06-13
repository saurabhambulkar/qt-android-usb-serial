package org.qtproject.example;

import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

import android.app.Activity;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.hardware.usb.UsbDevice;
import android.hardware.usb.UsbDeviceConnection;
import android.hardware.usb.UsbManager;
import android.util.Log;

import com.hoho.android.usbserial.driver.FtdiSerialDriver;
import com.hoho.android.usbserial.driver.UsbSerialPort;
import com.hoho.android.usbserial.driver.UsbSerialDriver;
import com.hoho.android.usbserial.driver.ProbeTable;
import com.hoho.android.usbserial.driver.UsbSerialProber;
import com.hoho.android.usbserial.util.SerialInputOutputManager;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.util.Arrays;
import java.util.List;

public class SerialHelper {
    private static final String TAG = "SerialHelper";
    
    // Static context that will be set from C++
    private static Context m_context = null;
    private static Activity m_activity = null;
    private static UsbManager m_usbManager = null;
    private static ExecutorService executorService = Executors.newSingleThreadExecutor();

    private static final String ACTION_USB_PERMISSION = "org.qtproject.example.USB_PERMISSION";
    private static UsbManager usbManager;
    private static UsbSerialPort serialPort;

    private static final int WRITE_WAIT_MILLIS = 2000;
    private static final int READ_WAIT_MILLIS = 2000;
    
    // Default baudrate, can be changed via setBaudRate
    private static int currentBaudRate = 9600;

    // Native methods that will be implemented in C++
    public static native void javaResponseReady(byte[] response);
    public static native void javaConnectedStateChanged(boolean state);
    public static native void javaErrorOccurred(String error);
    public static native void javaDeviceAttached(boolean state);

    /**
     * Initialize with Android Context from C++
     * This method will be called from C++ to set the Android Context
     * @param context Android Context from C++
     * @return Boolean indicating success
     */
    public static Boolean init(Context context) {
        if (context == null) {
            Log.e(TAG, "Context is null in SerialHelper.init");
            return false;
        }
        
        Log.i(TAG, "SerialHelper.init called with valid context");
        m_context = context;
        
        // Get UsbManager from context
        try {
            m_usbManager = (UsbManager) m_context.getSystemService(Context.USB_SERVICE);
            if (m_usbManager == null) {
                Log.e(TAG, "Failed to get UsbManager from context");
                return false;
            }
            Log.i(TAG, "Successfully got UsbManager from context");
            return true;
        } catch (Exception e) {
            Log.e(TAG, "Exception getting UsbManager: " + e.getMessage());
            return false;
        }
    }

    // Close the serial port connection
    public static void closeDeviceConnection() {
        executorService.submit(new Runnable() {
            @Override
            public void run() {
                if (serialPort == null) {
                    javaErrorOccurred("Serial port is not initialized. Nothing to close.");
                    return;
                }

                try {
                    serialPort.close();
                    serialPort = null;
                    javaConnectedStateChanged(false);
                    Log.d(TAG, "Serial port closed successfully");
                } catch (IOException e) {
                    javaErrorOccurred("Failed to close serial port: " + e.getMessage());
                    Log.e(TAG, "Error closing serial port", e);
                }
            }
        });
    }

    // Set the baudrate for the serial connection
    public static void setBaudRate(int baudRate) {
        executorService.submit(new Runnable() {
            @Override
            public void run() {
                if (serialPort == null) {
                    javaErrorOccurred("Cannot set baudrate: Serial port is not connected");
                    return;
                }
                
                try {
                    currentBaudRate = baudRate;
                    serialPort.setParameters(baudRate, 8, UsbSerialPort.STOPBITS_1, UsbSerialPort.PARITY_NONE);
                    Log.d(TAG, "Baudrate set to " + baudRate);
                } catch (IOException e) {
                    javaErrorOccurred("Failed to set baudrate: " + e.getMessage());
                    Log.e(TAG, "Error setting baudrate", e);
                } catch (UnsupportedOperationException e) {
                    javaErrorOccurred("This baudrate is not supported by the device: " + e.getMessage());
                    Log.e(TAG, "Unsupported baudrate", e);
                }
            }
        });
    }

    // Connect to a USB serial device with the given vendor and product IDs
    public static void connectToDevice(Context context, int vid, int pid, int baudRate) {
        currentBaudRate = baudRate;

        executorService.submit(new Runnable() {
            @Override
            public void run() {
                Log.d(TAG, "Attempting to connect to device VID: " + vid + ", PID: " + pid);
                
                // Create custom prober for given VID and PID
                ProbeTable customTable = new ProbeTable();
                customTable.addProduct(vid, pid, FtdiSerialDriver.class);
                UsbSerialProber prober = new UsbSerialProber(customTable);

                // Find all available drivers from attached devices
                UsbManager manager = (UsbManager) context.getSystemService(Context.USB_SERVICE);
                List<UsbSerialDriver> drivers = prober.findAllDrivers(manager);
                
                if (drivers.isEmpty()) {
                    // Try the default prober as a fallback
                    drivers = UsbSerialProber.getDefaultProber().findAllDrivers(manager);
                    if (drivers.isEmpty()) {
                        javaErrorOccurred("No USB serial devices found");
                        Log.d(TAG, "No USB serial devices found");
                        return;
                    }
                }

                // Open a connection to the first available driver
                UsbSerialDriver driver = drivers.get(0);
                UsbDevice device = driver.getDevice();
                Log.d(TAG, "Found device: " + device.getDeviceName());
                
                UsbDeviceConnection connection = manager.openDevice(device);
                if (connection == null) {
                    // Request permission if needed
                    PendingIntent permissionIntent = PendingIntent.getBroadcast(
                            context, 0, new Intent(ACTION_USB_PERMISSION), 0);
                    
                    // Register receiver for permission response
                    IntentFilter filter = new IntentFilter(ACTION_USB_PERMISSION);
                    context.registerReceiver(usbReceiver, filter);
                    
                    manager.requestPermission(device, permissionIntent);
                    javaErrorOccurred("USB permission requested. Please approve the connection.");
                    Log.d(TAG, "USB permission requested");
                    return;
                }
                
                // Permission already granted, proceed with connection
                connectWithPermission(driver, connection);
            }
        });
    }

    // Connect to the device once permission is granted
    private static void connectWithPermission(UsbSerialDriver driver, UsbDeviceConnection connection) {
        try {
            UsbSerialPort port = driver.getPorts().get(0); // Most devices have just one port
            port.open(connection);
            
            try {
                // Configure serial port parameters (baud rate, data bits, stop bits, parity)
                port.setParameters(currentBaudRate, 8, UsbSerialPort.STOPBITS_1, UsbSerialPort.PARITY_NONE);
            } catch (UnsupportedOperationException e) {
                javaErrorOccurred("Failed to set port parameters: " + e.getMessage());
                Log.e(TAG, "Unsupported operation when setting port parameters", e);
                port.close();
                return;
            }
            
            serialPort = port;
            javaConnectedStateChanged(true);
            Log.d(TAG, "Serial port connected successfully");
            
        } catch (Exception e) {
            javaErrorOccurred("Failed to open port: " + e.getMessage());
            Log.e(TAG, "Error opening serial port", e);
        }
    }

    // Send a command to the serial port and read the response
    public static void sendCommand(String command) {
        executorService.submit(new Runnable() {
            @Override
            public void run() {
                if (serialPort == null) {
                    javaConnectedStateChanged(false);
                    javaErrorOccurred("Serial port is not initialized. Call connectToDevice() first.");
                    return;
                }

                try {
                    Log.d(TAG, "Sending command: " + command);
                    byte[] data = command.getBytes();
                    serialPort.write(data, WRITE_WAIT_MILLIS);
                    
                    // Read response
                    ByteArrayOutputStream outputStream = new ByteArrayOutputStream();
                    byte[] buffer = new byte[8192];
                    
                    try {
                        // Read data with timeout
                        int len = serialPort.read(buffer, READ_WAIT_MILLIS);
                        if (len > 0) {
                            outputStream.write(buffer, 0, len);
                            byte[] response = outputStream.toByteArray();
                            Log.d(TAG, "Received response: " + new String(response));
                            javaResponseReady(response);
                        } else {
                            Log.d(TAG, "No response received within timeout");
                            javaResponseReady(new byte[0]);
                        }
                    } catch (IOException e) {
                        javaErrorOccurred("Failed to read from serial port: " + e.getMessage());
                        Log.e(TAG, "Error reading from serial port", e);
                        javaConnectedStateChanged(false);
                    }
                } catch (Exception e) {
                    javaErrorOccurred("Failed to write to serial port: " + e.getMessage());
                    Log.e(TAG, "Error writing to serial port", e);
                    javaConnectedStateChanged(false);
                }
            }
        });
    }

    // BroadcastReceiver for USB permission events
    private static final BroadcastReceiver usbReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            String action = intent.getAction();
            if (ACTION_USB_PERMISSION.equals(action)) {
                synchronized (this) {
                    UsbDevice device = intent.getParcelableExtra(UsbManager.EXTRA_DEVICE);

                    if (intent.getBooleanExtra(UsbManager.EXTRA_PERMISSION_GRANTED, false)) {
                        if (device != null) {
                            Log.d(TAG, "USB permission granted for device: " + device.getDeviceName());
                            javaDeviceAttached(true);
                            
                            // Find driver for this device
                            UsbManager manager = (UsbManager) context.getSystemService(Context.USB_SERVICE);
                            UsbSerialProber prober = UsbSerialProber.getDefaultProber();
                            List<UsbSerialDriver> drivers = prober.findAllDrivers(manager);
                            
                            for (UsbSerialDriver driver : drivers) {
                                if (driver.getDevice().equals(device)) {
                                    UsbDeviceConnection connection = manager.openDevice(device);
                                    if (connection != null) {
                                        connectWithPermission(driver, connection);
                                    }
                                    break;
                                }
                            }
                        }
                    } else {
                        Log.d(TAG, "USB permission denied");
                        javaErrorOccurred("USB permission denied");
                    }
                }
                
                // Unregister receiver after handling the permission response
                try {
                    context.unregisterReceiver(usbReceiver);
                } catch (Exception e) {
                    // Ignore if not registered
                }
            }
        }
    };
}

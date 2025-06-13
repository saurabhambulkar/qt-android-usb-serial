package org.qtproject.jniusbserial;

import java.util.HashMap;
import java.util.List;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.io.IOException;
import android.util.Log;
import android.app.PendingIntent;

import android.app.Activity;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.hardware.usb.*;
import android.widget.Toast;
import android.os.Build;
import java.nio.charset.StandardCharsets;

import com.hoho.android.usbserial.driver.*;
import org.qtproject.jniusbserial.SerialInputOutputManager;

// Import Qt Android binding
import org.qtproject.qt.android.QtNative;

public class JniUsbSerial {
    private static final String TAG = "JniUsbSerial";
    private static final String ACTION_USB_PERMISSION = "org.qtproject.jniusbserial.USB_PERMISSION";
    private static PendingIntent mPermissionIntent;
    private static UsbManager usbManager;
    private static HashMap<String, UsbSerialPort> m_usbSerialPort;
    private static HashMap<String, SerialInputOutputManager> m_usbIoManager;
    private static Context m_context = null;

    /**
     * Initialize with Android Context from C++
     * @param context Android application context
     * @return true if initialization succeeded
     */
    public static boolean init(Context context) {
        if (context == null) {
            Log.e(TAG, "init: Context is null");
            return false;
        }
        
        m_context = context;
        Log.d(TAG, "init: Context initialized with " + context.getClass().getName());
        
        try {
            usbManager = (UsbManager) m_context.getSystemService(Context.USB_SERVICE);
            if (usbManager != null) {
                Log.d(TAG, "init: UsbManager successfully obtained");
                
                // Create a PendingIntent for USB permission requests
                int flags = 0;
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                    flags = PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_MUTABLE;
                } else {
                    flags = PendingIntent.FLAG_UPDATE_CURRENT;
                }
                
                // Create an explicit intent for Android 14+ compatibility
                Intent permissionIntent = new Intent(ACTION_USB_PERMISSION);
                permissionIntent.setPackage(m_context.getPackageName());
                
                mPermissionIntent = PendingIntent.getBroadcast(m_context, 0, 
                    permissionIntent, flags);
                
                // Register a broadcast receiver for USB permission and device events
                IntentFilter filter = new IntentFilter(ACTION_USB_PERMISSION);
                filter.addAction(UsbManager.ACTION_USB_DEVICE_ATTACHED);
                filter.addAction(UsbManager.ACTION_USB_DEVICE_DETACHED);
                m_context.registerReceiver(mUsbReceiver, filter);
                
                Log.d(TAG, "init: USB permission broadcast receiver registered");
                return true;
            } else {
                Log.e(TAG, "init: Failed to get UsbManager service");
                return false;
            }
        } catch (Exception e) {
            Log.e(TAG, "init: Exception getting UsbManager: " + e.getMessage());
            e.printStackTrace();
            return false;
        }
    }

    private static native void nativeDeviceException(long classPoint, String messageA);
    private static native void nativeDeviceNewData(long classPoint, byte[] dataA);
    
    // Broadcast receiver for USB permission and device events
    private static final BroadcastReceiver mUsbReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            String action = intent.getAction();
            Log.d(TAG, "USB BroadcastReceiver received action: " + action);
            
            if (ACTION_USB_PERMISSION.equals(action)) {
                synchronized (this) {
                    UsbDevice device = intent.getParcelableExtra(UsbManager.EXTRA_DEVICE, UsbDevice.class);
                    boolean granted = intent.getBooleanExtra(UsbManager.EXTRA_PERMISSION_GRANTED, false);
                    
                    if (device != null) {
                        if (granted) {
                            Log.d(TAG, "USB Permission granted for device: " + device.getDeviceName());
                        } else {
                            Log.e(TAG, "USB Permission denied for device: " + device.getDeviceName());
                        }
                    }
                }
            } else if (UsbManager.ACTION_USB_DEVICE_ATTACHED.equals(action)) {
                UsbDevice device = intent.getParcelableExtra(UsbManager.EXTRA_DEVICE, UsbDevice.class);
                if (device != null) {
                    Log.d(TAG, "USB Device attached: " + device.getDeviceName() + 
                          " VID: " + String.format("0x%04X", device.getVendorId()) + 
                          " PID: " + String.format("0x%04X", device.getProductId()));
                    
                    // Request permission if needed
                    if (!usbManager.hasPermission(device)) {
                        usbManager.requestPermission(device, mPermissionIntent);
                    }
                }
            } else if (UsbManager.ACTION_USB_DEVICE_DETACHED.equals(action)) {
                UsbDevice device = intent.getParcelableExtra(UsbManager.EXTRA_DEVICE, UsbDevice.class);
                if (device != null) {
                    Log.d(TAG, "USB Device detached: " + device.getDeviceName());
                }
            }
        }
    };

    public JniUsbSerial() {
        m_usbIoManager = new HashMap<String, SerialInputOutputManager>();
        m_usbSerialPort = new HashMap<String, UsbSerialPort>();
    }
    
    private static boolean getCurrentDevices() {
        if (m_context == null) {
            Log.e(TAG, "getCurrentDevices: Context is null");
            return false;
        }

        if (usbManager == null) {
            Log.d(TAG, "getCurrentDevices: UsbManager is null, attempting to get it");
            usbManager = (UsbManager) m_context.getSystemService(Context.USB_SERVICE);
            if (usbManager == null) {
                Log.e(TAG, "getCurrentDevices: Failed to get UsbManager");
                return false;
            }
            Log.d(TAG, "getCurrentDevices: UsbManager obtained successfully");
        }
        
        // Log all connected USB devices for debugging
        HashMap<String, UsbDevice> deviceList = usbManager.getDeviceList();
        Log.d(TAG, "getCurrentDevices: Found " + deviceList.size() + " USB devices");
        
        for (UsbDevice device : deviceList.values()) {
            Log.d(TAG, "USB Device: " + device.getDeviceName() + 
                  " VID: " + String.format("0x%04X", device.getVendorId()) + 
                  " PID: " + String.format("0x%04X", device.getProductId()) + 
                  " Product: " + device.getProductName());
                  
            // Check if we have permission for this device
            if (!usbManager.hasPermission(device)) {
                Log.d(TAG, "Requesting permission for device: " + device.getDeviceName());
                usbManager.requestPermission(device, mPermissionIntent);
            } else {
                Log.d(TAG, "Already have permission for device: " + device.getDeviceName());
            }
        }

        return true;
    }

    public static String[] availableDevicesInfo()
    {
        //  GET THE LIST OF CURRENT DEVICES
        Log.d(TAG, "availableDevicesInfo: Scanning for available USB devices");
        if (!getCurrentDevices()) {
            Log.e(TAG, "availableDevicesInfo: Failed to get current devices");
            return null;
        }

        int deviceCount = usbManager.getDeviceList().size();
        Log.d(TAG, "availableDevicesInfo: Found " + deviceCount + " USB devices");
        
        if (deviceCount < 1) {
            Log.d(TAG, "availableDevicesInfo: No USB devices found");
            return null;
        }
        
        // Request permission for all devices that we don't have permission for
        for (UsbDevice device : usbManager.getDeviceList().values()) {
            if (!usbManager.hasPermission(device)) {
                Log.d(TAG, "Requesting permission for device in availableDevicesInfo: " + device.getDeviceName());
                usbManager.requestPermission(device, mPermissionIntent);
            }
        }

        String[] listL = new String[usbManager.getDeviceList().size()];
        String tempL;

        UsbSerialProber usbDefaultProber = UsbSerialProber.getDefaultProber();
        int countL = 0;
        for(UsbDevice deviceL : usbManager.getDeviceList().values()) {
            // Log detailed device information
            Log.d(TAG, "Device: " + deviceL.getDeviceName() + 
                  " VID: 0x" + String.format("%04X", deviceL.getVendorId()) + 
                  " PID: 0x" + String.format("%04X", deviceL.getProductId()) + 
                  " Product: " + deviceL.getProductName() + 
                  " Manufacturer: " + deviceL.getManufacturerName());

            UsbSerialDriver driverL = usbDefaultProber.probeDevice(deviceL);
            
            // Format: devicePath:driverType:manufacturer:productId:vendorId:deviceNodePath:hasPermission
            tempL = deviceL.getDeviceName() + ":";

            if (driverL == null) {
                tempL = tempL + "Unknown:";
            }
            else if (driverL instanceof CdcAcmSerialDriver)
            {
                tempL = tempL + "Cdc Acm:";
            }
            else if (driverL instanceof Ch34xSerialDriver)
            {
                tempL = tempL + "Ch34x:";
            }
            else if (driverL instanceof CommonUsbSerialPort)
            {
                tempL = tempL + "CommonUsb:";
            }
            else if (driverL instanceof Cp21xxSerialDriver)
            {
                tempL = tempL + "Cp21xx:";
            }
            else if (driverL instanceof FtdiSerialDriver)
            {
                tempL = tempL + "Ftdi:";
            }
            else if (driverL instanceof ProlificSerialDriver)
            {
                tempL = tempL + "Prolific:";
            }

            // Add manufacturer name (may be null)
            String manufacturerName = deviceL.getManufacturerName();
            tempL = tempL + (manufacturerName != null ? manufacturerName : "Unknown") + ":";

            // Add product and vendor IDs in hex format for better readability
            tempL = tempL + String.format("0x%04X", deviceL.getProductId()) + ":";
            tempL = tempL + String.format("0x%04X", deviceL.getVendorId()) + ":";
            
            // Add the device node path (e.g., /dev/bus/usb/001/002)
            tempL = tempL + deviceL.getDeviceName() + ":";
            
            // Add permission status
            tempL = tempL + (usbManager.hasPermission(deviceL) ? "true" : "false");

            listL[countL] = tempL;
            countL++;
        }

        return listL;
    }

    public static boolean setParameters(String portNameA, int baudRateA, int dataBitsA, int stopBitsA, int parityA)
    {
        if (m_usbSerialPort.size() <= 0)
            return false;

        if (m_usbSerialPort.get(portNameA) == null)
            return false;

        try
        {
            m_usbSerialPort.get(portNameA).setParameters(baudRateA, dataBitsA, stopBitsA, parityA);
            return true;
        }
        catch(IOException eA)
        {
            return false;
        }
    }

    public static void stopIoManager(String portNameA)
    {
        if (m_usbIoManager.get(portNameA) == null)
            return;

        m_usbIoManager.get(portNameA).stop();
        m_usbIoManager.remove(portNameA);
    }

    public static void startIoManager(String portNameA, long classPoint)
    {
        if (m_usbSerialPort.get(portNameA) == null)
            return;

        SerialInputOutputManager usbIoManager = new SerialInputOutputManager(m_usbSerialPort.get(portNameA), m_Listener, classPoint);

        m_usbIoManager.put(portNameA, usbIoManager);
        m_usbIoManager.get(portNameA).start();
    }

    public static boolean close(String portNameA)
    {
        if (m_usbSerialPort.get(portNameA) == null)
            return false;

        try
        {
            stopIoManager(portNameA);
            m_usbSerialPort.get(portNameA).close();
            m_usbSerialPort.remove(portNameA);

            return true;
        }
        catch (IOException eA)
        {
            return false;
        }
    }

    public static int open(String portNameA, long classPoint)
    {
        //  GET THE LIST OF CURRENT DEVICES
        if (!getCurrentDevices())
            return 0;

        if (usbManager.getDeviceList().size() < 1)
            return 0;

        if (m_usbSerialPort.get(portNameA) != null)
            return 0;

        UsbSerialProber usbDefaultProber = UsbSerialProber.getDefaultProber();
        for(UsbDevice deviceL : usbManager.getDeviceList().values()) {

            if (portNameA.equals(deviceL.getDeviceName()))
            {
            }
            else
            {
                continue;
            }

            UsbSerialDriver driverL = usbDefaultProber.probeDevice(deviceL);
            if (driverL == null)
            {
                return 0;
            }

            UsbDeviceConnection connectionL = usbManager.openDevice(driverL.getDevice());
            if (connectionL == null) {
                return 0;
            }

            UsbSerialPort usbSerialPort = driverL.getPorts().get(0);

            try{
                usbSerialPort.open(connectionL);
                m_usbSerialPort.put(portNameA ,usbSerialPort);

                startIoManager(portNameA, classPoint);

                return 1;
            }
            catch (Exception e) {
                m_usbSerialPort.remove(portNameA);
                stopIoManager(portNameA);
                return 0;
            }
        }
        return 0;
    }

    public static int write(String portNameA, byte[] sourceA, int timeoutMSecA)
    {
        if (m_usbSerialPort.get(portNameA) == null)
            return 0;

        try
        {
            System.out.println("Serial write:" + new String(sourceA, StandardCharsets.UTF_8));
            m_usbSerialPort.get(portNameA).write(sourceA, timeoutMSecA);
        }
        catch (IOException eA)
        {
            return 0;
        }

        return 1;
    }

    // SerialInputOutputManager.Listener

    private static final SerialInputOutputManager.Listener m_Listener =
            new SerialInputOutputManager.Listener() {

                @Override
                public void onRunError(Exception e, long classPoint) {
                    Log.e(TAG, "SerialInputOutputManager.onRunError: " + e.getMessage());
                    nativeDeviceException(classPoint, e.getMessage());
                }

                @Override
                public void onNewData(final byte[] data, long classPoint) {
                    StringBuilder hexData = new StringBuilder();
                    int bytesToShow = Math.min(data.length, 20); // Show up to 20 bytes
                    for (int i = 0; i < bytesToShow; i++) {
                        hexData.append(String.format("%02X ", data[i] & 0xFF));
                    }
                    
                    Log.d(TAG, "SerialInputOutputManager.onNewData: Received " + data.length + " bytes: " + hexData.toString());
                    nativeDeviceNewData(classPoint, data);
                    Log.d(TAG, "SerialInputOutputManager.onNewData: Data passed to JNI");
                }
            };
}

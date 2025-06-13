import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12

Window {
    visible: true
    width: 360
    height: 640
    title: qsTr("Android Serial Test")

    property string receivedData: ""
    property bool autoScroll: true

    // Connect to C++ signals
    Connections {
        target: serialPortHandler
        
        // Handle data received from serial port
        function onDataReceived(data) {
            // Add timestamp to received data
            var timestamp = new Date().toLocaleTimeString();
            receivedData += "[" + timestamp + "] " + data + "\n"
            responseText.text = receivedData
            
            // Auto-scroll to bottom if enabled
            if (autoScroll) {
                responseText.cursorPosition = responseText.length
            }
        }
        
        // Handle connection state changes
        function onConnectedChanged(connected) {
            connectButton.text = connected ? "Disconnect" : "Connect"
            statusLabel.text = "Status: " + (connected ? "Connected" : "Disconnected")
            statusLabel.color = connected ? "green" : "red"
        }
        
        // Handle error messages
        function onErrorOccurred(error) {
            errorLabel.text = "Error: " + error
            errorLabel.visible = true
        }
        
        // Handle device attachment
        function onDeviceAttached(attached) {
            if (attached) {
                deviceLabel.text = "USB Serial Device Attached"
                deviceLabel.color = "green"
            } else {
                deviceLabel.text = "No USB Serial Device"
                deviceLabel.color = "red"
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 16

        Label {
            id: titleLabel
            text: "Android Serial Port Test"
            font.pixelSize: 24
            font.bold: true
            Layout.alignment: Qt.AlignHCenter
        }

        Label {
            id: deviceLabel
            text: "No USB Serial Device"
            color: "red"
            font.pixelSize: 16
            Layout.alignment: Qt.AlignHCenter
        }
        
        // Device node path display
        Label {
            id: deviceNodeLabel
            text: serialPortHandler.currentDeviceNodePath !== "" ? 
                  "Device Node: " + serialPortHandler.currentDeviceNodePath : 
                  "No device node"
            color: serialPortHandler.currentDeviceNodePath !== "" ? "black" : "gray"
            font.pixelSize: 12
            Layout.alignment: Qt.AlignHCenter
            visible: serialPortHandler.currentDevice !== ""
        }

        Label {
            id: statusLabel
            text: "Status: Disconnected"
            color: "red"
            font.pixelSize: 16
            Layout.alignment: Qt.AlignHCenter
        }

        Label {
            id: errorLabel
            text: "Error: None"
            color: "red"
            visible: false
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        // Serial port settings
        GroupBox {
            title: "Serial Port Settings"
            Layout.fillWidth: true
            
            ColumnLayout {
                anchors.fill: parent
                spacing: 8
                
                // Device selection
                Label {
                    text: "Device:"
                }
                
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    
                    ComboBox {
                        id: deviceCombo
                        Layout.fillWidth: true
                        model: serialPortHandler.availableDevices
                        enabled: !serialPortHandler.connected
                        onActivated: {
                            serialPortHandler.currentDevice = currentText
                        }
                        
                        // Show tooltip with device node path on hover
                        ToolTip.visible: hovered
                        ToolTip.text: serialPortHandler.currentDeviceNodePath !== "" ? 
                                      "Device Node: " + serialPortHandler.currentDeviceNodePath : 
                                      "No device selected"
                    }
                    
                    Button {
                        text: "Refresh"
                        enabled: !serialPortHandler.connected
                        onClicked: {
                            serialPortHandler.refreshDeviceList()
                        }
                    }
                    
                    // Permission status indicator
                    Rectangle {
                        width: 16
                        height: 16
                        radius: 8
                        color: serialPortHandler.currentDeviceHasPermission ? "green" : "red"
                        visible: deviceCombo.currentIndex >= 0
                        
                        ToolTip.visible: permissionMouseArea.containsMouse
                        ToolTip.text: serialPortHandler.currentDeviceHasPermission ? 
                                      "USB Permission: Granted" : 
                                      "USB Permission: Denied"
                                      
                        MouseArea {
                            id: permissionMouseArea
                            anchors.fill: parent
                            hoverEnabled: true
                        }
                    }
                }
                
                // Baud rate selection
                Label {
                    text: "Baud Rate:"
                }
                
                ComboBox {
                    id: baudrateCombo
                    Layout.fillWidth: true
                    model: serialPortHandler.availableBaudRates()
                    currentIndex: {
                        // Find index of current baudrate
                        for (let i = 0; i < model.length; i++) {
                            if (model[i] === serialPortHandler.baudRate) {
                                return i;
                            }
                        }
                        return 0; // Default to first item if not found
                    }
                    enabled: !serialPortHandler.connected
                    onActivated: {
                        serialPortHandler.baudRate = model[currentIndex];
                    }
                }
            }
        }
        
        Button {
            id: connectButton
            text: "Connect"
            Layout.fillWidth: true
            onClicked: {
                if (serialPortHandler.connected) {
                    serialPortHandler.disconnectDevice()
                } else {
                    errorLabel.visible = false
                    serialPortHandler.connectToDevice()
                }
            }
        }

        GroupBox {
            title: "Send Command"
            Layout.fillWidth: true

            ColumnLayout {
                anchors.fill: parent
                spacing: 8

                TextField {
                    id: commandInput
                    placeholderText: "Enter command to send"
                    Layout.fillWidth: true
                    enabled: serialPortHandler.connected
                }

                Button {
                    text: "Send"
                    Layout.fillWidth: true
                    enabled: serialPortHandler.connected && commandInput.text.length > 0
                    onClicked: {
                        errorLabel.visible = false
                        serialPortHandler.sendCommand(commandInput.text)
                    }
                }
            }
        }

        GroupBox {
            title: "Received Data"
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredHeight: 200

            ColumnLayout {
                anchors.fill: parent
                spacing: 4

                // Data display area
                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    border.color: "#cccccc"
                    border.width: 1
                    radius: 4
                    color: "#f8f8f8"

                    ScrollView {
                        id: responseScrollView
                        anchors.fill: parent
                        anchors.margins: 4
                        clip: true

                        TextArea {
                            id: responseText
                            readOnly: true
                            wrapMode: TextEdit.Wrap
                            selectByMouse: true
                            font.family: "Courier"
                            font.pixelSize: 14
                            textFormat: TextEdit.PlainText
                            background: Rectangle {
                                color: "transparent"
                            }
                        }
                    }
                }

                // Controls for received data
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Button {
                        text: "Clear"
                        Layout.preferredWidth: 80
                        onClicked: {
                            receivedData = ""
                            responseText.text = ""
                            errorLabel.visible = false
                        }
                    }

                    CheckBox {
                        id: autoScrollCheckbox
                        text: "Auto-scroll"
                        checked: autoScroll
                        onCheckedChanged: {
                            autoScroll = checked
                        }
                    }

                    Button {
                        text: "Copy All"
                        Layout.preferredWidth: 80
                        onClicked: {
                            responseText.selectAll()
                            responseText.copy()
                            responseText.deselect()
                        }
                    }

                    Item { Layout.fillWidth: true } // Spacer

                    Label {
                        text: responseText.length + " bytes"
                        font.pixelSize: 12
                        color: "#666666"
                    }
                }
            }
        }
    }
}

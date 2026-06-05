import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtUi 1.0

Rectangle {
    id: root
    color: "#F8F8F5"

    property var activeFileDialog: null
    property url lastSourceDialogFolder: ""
    property url lastSaveDialogFolder: ""

    function clearActiveFileDialog(dialog) {
        if (activeFileDialog === dialog)
            activeFileDialog = null
    }

    function openFileDialog(component) {
        if (activeFileDialog) {
            const previousDialog = activeFileDialog
            activeFileDialog = null
            previousDialog.close()
            previousDialog.destroy()
        }

        const dialogParent = root.Window.window ? root.Window.window.contentItem : root
        const dialog = component.createObject(dialogParent)
        if (!dialog)
            return

        activeFileDialog = dialog
        dialog.open()
    }

    Component {
        id: openSourceDialogComponent

        LocalFileDialog {
            id: dialog
            title: "Open Source"
            nameFilters: [
                "Media files (*.png *.jpg *.jpeg *.bmp *.tiff *.tif *.gif *.webp *.mp4 *.avi *.mov *.mkv *.wmv *.flv *.webm *.m4v)",
                "Image files (*.png *.jpg *.jpeg *.bmp *.tiff *.tif *.gif *.webp)",
                "Video files (*.mp4 *.avi *.mov *.mkv *.wmv *.flv *.webm *.m4v)",
                "All files (*)"
            ]
            acceptLabel: "Open"
            saveMode: false
            initialFolder: root.lastSourceDialogFolder
            onFinished: function(accepted, fileUrl, folder) {
                root.lastSourceDialogFolder = folder
                root.clearActiveFileDialog(dialog)
                destroy()
                if (accepted)
                    ProcessingController.openSource(fileUrl)
            }
        }
    }

    Component {
        id: saveImageDialogComponent

        LocalFileDialog {
            id: dialog
            title: "Save Image"
            nameFilters: ["PNG files (*.png)", "JPEG files (*.jpg *.jpeg)", "BMP files (*.bmp)", "All files (*)"]
            acceptLabel: "Save"
            saveMode: true
            initialFolder: root.lastSaveDialogFolder
            onFinished: function(accepted, fileUrl, folder) {
                root.lastSaveDialogFolder = folder
                root.clearActiveFileDialog(dialog)
                destroy()
                if (accepted)
                    ProcessingController.saveImage(fileUrl)
            }
        }
    }

    Dialog {
        id: streamDialog
        title: "Open Stream"
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        anchors.centerIn: parent
        onOpened: {
            streamUrlField.forceActiveFocus()
            streamUrlField.selectAll()
        }
        onAccepted: {
            if (streamUrlField.text.trim().length > 0)
                ProcessingController.openStream(streamUrlField.text.trim())
        }

        ColumnLayout {
            spacing: 8
            width: 380

            TextField {
                id: streamUrlField
                Layout.fillWidth: true
                placeholderText: "rtsp://, rtmp://, or http://"
                selectByMouse: true
            }
        }
    }

    RowLayout {
        anchors {
            fill: parent
            leftMargin: 12
            rightMargin: 12
        }
        spacing: 8

        Row {
            spacing: 6

            Button {
                text: "Open Source"
                implicitHeight: 34
                onClicked: openFileDialog(openSourceDialogComponent)
                background: Rectangle {
                    color: "#6F86AB"
                    radius: 6
                }
                contentItem: Text {
                    text: parent.text
                    color: "#FFFFFF"
                    font.pixelSize: 13
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }

            Button {
                text: "Open Stream"
                implicitHeight: 34
                onClicked: streamDialog.open()
                background: Rectangle {
                    color: "#6F86AB"
                    radius: 6
                }
                contentItem: Text {
                    text: parent.text
                    color: "#FFFFFF"
                    font.pixelSize: 13
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }

        Item { Layout.fillWidth: true }

        Row {
            spacing: 6

            Button {
                text: "Before/After"
                implicitHeight: 34
                enabled: false
                background: Rectangle {
                    color: "transparent"
                    border.color: "#E5E8EB"
                    border.width: 1
                    radius: 6
                }
                contentItem: Text {
                    text: parent.text
                    color: "#B0B4B8"
                    font.pixelSize: 13
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }

            Button {
                text: "Save"
                implicitHeight: 34
                enabled: ProcessingController.canSave
                onClicked: openFileDialog(saveImageDialogComponent)
                background: Rectangle {
                    color: "transparent"
                    border.color: parent.enabled ? "#6F86AB" : "#E5E8EB"
                    border.width: 1
                    radius: 6
                }
                contentItem: Text {
                    text: parent.text
                    color: parent.enabled ? "#6F86AB" : "#6E747A"
                    font.pixelSize: 13
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }

            Button {
                text: "Reset"
                implicitHeight: 34
                enabled: ProcessingController.hasImage
                onClicked: ProcessingController.reset()
                background: Rectangle {
                    color: "transparent"
                    border.color: "#E5E8EB"
                    border.width: 1
                    radius: 6
                }
                contentItem: Text {
                    text: parent.text
                    color: parent.enabled ? "#2F3438" : "#B0B4B8"
                    font.pixelSize: 13
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }
    }
}

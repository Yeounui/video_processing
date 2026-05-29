import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QtUi 1.0

Rectangle {
    color: "#F8F8F5"

    FileDialog {
        id: openImageDialog
        title: "Open Image"
        nameFilters: ["Image files (*.png *.jpg *.jpeg *.bmp *.tiff *.gif *.webp)", "All files (*)"]
        fileMode: FileDialog.OpenFile
        onAccepted: ProcessingController.openImage(selectedFile)
    }

    FileDialog {
        id: saveImageDialog
        title: "Save Image"
        nameFilters: ["PNG files (*.png)", "JPEG files (*.jpg *.jpeg)", "BMP files (*.bmp)", "All files (*)"]
        fileMode: FileDialog.SaveFile
        onAccepted: ProcessingController.saveImage(selectedFile)
    }

    FileDialog {
        id: openVideoDialog
        title: "Open Video"
        nameFilters: ["Video files (*.mp4 *.avi *.mov *.mkv *.wmv *.flv *.webm *.m4v)", "All files (*)"]
        fileMode: FileDialog.OpenFile
        onAccepted: ProcessingController.openVideo(selectedFile)
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
                text: "Open Image"
                implicitHeight: 34
                onClicked: openImageDialog.open()
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
                text: "Open Video"
                implicitHeight: 34
                onClicked: openVideoDialog.open()
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
                onClicked: saveImageDialog.open()
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

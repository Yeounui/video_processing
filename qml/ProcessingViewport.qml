import QtQuick
import QtQuick.Controls
import QtUi 1.0

Rectangle {
    color: "#F8F8F5"

    Column {
        anchors.centerIn: parent
        spacing: 12
        visible: !ProcessingController.hasImage

        Rectangle {
            width: 48
            height: 48
            anchors.horizontalCenter: parent.horizontalCenter
            color: "#E5E8EB"
            radius: 8

            Text {
                anchors.centerIn: parent
                text: "[ ]"
                font.pixelSize: 18
                color: "#6E747A"
            }
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "Open an image or video"
            font.pixelSize: 13
            color: "#6E747A"
        }
    }

    ProcessingViewportItem {
        anchors.fill: parent
        visible: ProcessingController.hasImage
        controller: ProcessingController
    }
}

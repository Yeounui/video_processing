import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtUi 1.0

Rectangle {
    color: "#F8F8F5"

    // "No image" placeholder
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
        id: viewport
        anchors.fill: parent
        visible: ProcessingController.hasImage
        controller: ProcessingController

        onRenderErrorOccurred: (message) => errorBanner.show(message)
    }

    // Compare mode overlay toolbar — top-right corner
    Row {
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: 8
        spacing: 4
        visible: ProcessingController.hasImage

        CompareButton {
            text: "Orig"
            active: viewport.showOriginal && !viewport.splitCompare
            onClicked: {
                viewport.splitCompare = false
                viewport.showOriginal = !viewport.showOriginal
            }
        }
        CompareButton {
            text: "A|B"
            active: viewport.splitCompare
            onClicked: {
                viewport.showOriginal = false
                viewport.splitCompare = !viewport.splitCompare
            }
        }
        CompareButton {
            text: "⟲"
            active: false
            onClicked: viewport.resetView()
        }
    }

    // Render error banner
    Rectangle {
        id: errorBannerRect
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: 36
        color: "#C0392B"
        visible: false

        Text {
            id: errorBannerText
            anchors.centerIn: parent
            color: "#FFFFFF"
            font.pixelSize: 12
        }

        QtObject {
            id: errorBanner
            function show(msg) {
                errorBannerText.text = msg
                errorBannerRect.visible = true
            }
        }

        MouseArea {
            anchors.fill: parent
            onClicked: errorBannerRect.visible = false
        }
    }

    // Split line indicator
    Rectangle {
        visible: ProcessingController.hasImage && viewport.splitCompare
        x: viewport.splitPosition * parent.width - 1
        y: 0
        width: 2
        height: parent.height
        color: "#FFFFFF"
        opacity: 0.8
    }

    // Inline component for compare toolbar buttons
    component CompareButton: Rectangle {
        property string text: ""
        property bool active: false
        signal clicked

        width: btnLabel.implicitWidth + 16
        height: 26
        radius: 4
        color: active ? "#6F86AB" : "#FFFFFF"
        border.color: active ? "transparent" : "#C8D0D8"
        border.width: 1
        opacity: 0.92

        Text {
            id: btnLabel
            anchors.centerIn: parent
            text: parent.text
            font.pixelSize: 12
            color: parent.active ? "#FFFFFF" : "#2F3438"
        }
        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: parent.clicked()
        }
    }
}

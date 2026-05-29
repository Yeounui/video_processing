import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtUi 1.0

Rectangle {
    color: "#F8F8F5"

    // Helper: format seconds as M:SS
    function fmtTime(secs) {
        var s = Math.max(0, secs)
        var m = Math.floor(s / 60)
        var sec = Math.floor(s % 60)
        return m + ":" + (sec < 10 ? "0" + sec : sec)
    }

    function streamStatusText(status) {
        if (status === 1)
            return "Connecting"
        if (status === 2)
            return "Connected"
        if (status === 3)
            return "Reconnecting"
        return "Disconnected"
    }

    function streamStatusColor(status) {
        if (status === 2)
            return "#4F8A67"
        if (status === 1 || status === 3)
            return "#B9823A"
        return "#A84E4E"
    }

    // Static stub (shown when no video)
    RowLayout {
        visible: !ProcessingController.isVideoSource && !ProcessingController.isStreamSource
        anchors { fill: parent; leftMargin: 12; rightMargin: 12 }
        spacing: 8
        Rectangle { width: 8; height: 8; radius: 4; color: "#8FA9C4" }
        Text { text: "CPU"; font.pixelSize: 12; color: "#6E747A" }
        Item { Layout.fillWidth: true }
        Text { text: "Ready"; font.pixelSize: 12; color: "#6E747A" }
    }

    // Video transport (shown when video source is active)
    RowLayout {
        visible: ProcessingController.isVideoSource
        anchors { fill: parent; leftMargin: 10; rightMargin: 10 }
        spacing: 6

        // Step back
        Button {
            text: "◀◀"
            implicitWidth: 32; implicitHeight: 28
            onClicked: ProcessingController.stepBackwardVideo()
            background: Rectangle { color: "transparent"; border.color: "#C8D0D8"; border.width: 1; radius: 4 }
            contentItem: Text { text: parent.text; font.pixelSize: 12; color: "#2F3438"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
        }

        // Play/Pause
        Button {
            implicitWidth: 40; implicitHeight: 28
            text: ProcessingController.videoPlaying ? "⏸" : "▶"
            onClicked: ProcessingController.videoPlaying ? ProcessingController.pauseVideo() : ProcessingController.playVideo()
            background: Rectangle { color: "#6F86AB"; radius: 4 }
            contentItem: Text { text: parent.text; font.pixelSize: 14; color: "#FFFFFF"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
        }

        // Step forward
        Button {
            text: "▶▶"
            implicitWidth: 32; implicitHeight: 28
            onClicked: ProcessingController.stepForwardVideo()
            background: Rectangle { color: "transparent"; border.color: "#C8D0D8"; border.width: 1; radius: 4 }
            contentItem: Text { text: parent.text; font.pixelSize: 12; color: "#2F3438"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
        }

        // Current time
        Text {
            text: fmtTime(ProcessingController.videoPosition)
            font.pixelSize: 12; color: "#2F3438"
            Layout.preferredWidth: 40
        }

        // Seek slider
        Slider {
            id: seekSlider
            Layout.fillWidth: true
            from: 0
            to: Math.max(1, ProcessingController.videoDuration)
            value: ProcessingController.videoPosition
            property bool dragging: false
            onPressedChanged: dragging = pressed
            onMoved: if (dragging) ProcessingController.seekVideo(value)
            background: Rectangle {
                x: seekSlider.leftPadding; y: seekSlider.topPadding + seekSlider.availableHeight / 2 - height / 2
                width: seekSlider.availableWidth; height: 4; radius: 2; color: "#E5E8EB"
                Rectangle { width: seekSlider.visualPosition * parent.width; height: parent.height; radius: 2; color: "#6F86AB" }
            }
            handle: Rectangle {
                x: seekSlider.leftPadding + seekSlider.visualPosition * (seekSlider.availableWidth - width)
                y: seekSlider.topPadding + seekSlider.availableHeight / 2 - height / 2
                width: 14; height: 14; radius: 7; color: "#6F86AB"
            }
        }

        // Total time
        Text {
            text: fmtTime(ProcessingController.videoDuration)
            font.pixelSize: 12; color: "#6E747A"
            Layout.preferredWidth: 40
        }

        // Loop toggle
        Button {
            text: "↺"
            implicitWidth: 28; implicitHeight: 28
            checkable: true
            checked: true
            onCheckedChanged: ProcessingController.setVideoLoop(checked)
            background: Rectangle {
                color: parent.checked ? "#DDEEF4" : "transparent"
                border.color: parent.checked ? "#6F86AB" : "#C8D0D8"; border.width: 1; radius: 4
            }
            contentItem: Text { text: parent.text; font.pixelSize: 14; color: parent.checked ? "#6F86AB" : "#6E747A"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
        }

        // Effect stack size indicator
        Text {
            visible: ProcessingController.effectStackSize > 0
            text: "FX:" + ProcessingController.effectStackSize + "/3"
            font.pixelSize: 11; color: "#6F86AB"
        }
    }

    // Stream transport (shown when realtime stream source is active)
    RowLayout {
        visible: ProcessingController.isStreamSource
        anchors { fill: parent; leftMargin: 12; rightMargin: 12 }
        spacing: 8

        Rectangle {
            width: 8
            height: 8
            radius: 4
            color: streamStatusColor(ProcessingController.streamStatus)
        }

        Text {
            text: streamStatusText(ProcessingController.streamStatus)
            font.pixelSize: 12
            color: "#2F3438"
            Layout.preferredWidth: 92
        }

        Text {
            text: ProcessingController.sourceFileName
            elide: Text.ElideMiddle
            font.pixelSize: 12
            color: "#6E747A"
            Layout.fillWidth: true
        }

        Text {
            visible: ProcessingController.effectStackSize > 0
            text: "FX:" + ProcessingController.effectStackSize + "/3"
            font.pixelSize: 11
            color: "#6F86AB"
        }

        Button {
            text: "Reconnect"
            implicitHeight: 28
            enabled: ProcessingController.streamStatus !== 1 && ProcessingController.streamStatus !== 2
            onClicked: ProcessingController.reconnectStream()
            background: Rectangle {
                color: "transparent"
                border.color: parent.enabled ? "#6F86AB" : "#E5E8EB"
                border.width: 1
                radius: 4
            }
            contentItem: Text {
                text: parent.text
                font.pixelSize: 12
                color: parent.enabled ? "#6F86AB" : "#B0B4B8"
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
        }

        Button {
            text: "Disconnect"
            implicitHeight: 28
            enabled: ProcessingController.streamStatus !== 0
            onClicked: ProcessingController.disconnectStream()
            background: Rectangle {
                color: "transparent"
                border.color: parent.enabled ? "#A84E4E" : "#E5E8EB"
                border.width: 1
                radius: 4
            }
            contentItem: Text {
                text: parent.text
                font.pixelSize: 12
                color: parent.enabled ? "#A84E4E" : "#B0B4B8"
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
        }
    }
}

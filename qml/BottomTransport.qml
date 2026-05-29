import QtQuick
import QtQuick.Layouts

Rectangle {
    color: "#F8F8F5"

    RowLayout {
        anchors { fill: parent; leftMargin: 12; rightMargin: 12 }
        spacing: 8

        Rectangle {
            width: 8
            height: 8
            radius: 4
            color: "#8FA9C4"
        }

        Text {
            text: "CPU"
            font.pixelSize: 12
            color: "#6E747A"
        }

        Item { Layout.fillWidth: true }

        Text {
            text: "Ready"
            font.pixelSize: 12
            color: "#6E747A"
        }
    }
}

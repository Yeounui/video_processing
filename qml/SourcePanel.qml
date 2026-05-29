import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtUi 1.0

Rectangle {
    color: "#F8F8F5"

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            height: 32
            color: "#DDEEF4"

            Text {
                anchors { left: parent.left; leftMargin: 10; verticalCenter: parent.verticalCenter }
                text: "Source"
                font.pixelSize: 13
                font.weight: Font.DemiBold
                color: "#2F3438"
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.topMargin: 10
            Layout.leftMargin: 10
            Layout.rightMargin: 10
            height: 80
            color: "#FFFFFF"
            radius: 7
            border.color: "#E5E8EB"
            border.width: 1

            Column {
                anchors.centerIn: parent
                spacing: 4
                width: parent.width - 20

                Text {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    text: ProcessingController.hasImage ? ProcessingController.sourceFileName : "No source open"
                    font.pixelSize: 13
                    color: ProcessingController.hasImage ? "#2F3438" : "#6E747A"
                    elide: Text.ElideMiddle
                }

                Text {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    visible: ProcessingController.hasImage
                    text: ProcessingController.imageWidth + " × " + ProcessingController.imageHeight
                    font.pixelSize: 11
                    color: "#6E747A"
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.topMargin: 10
            height: 32
            color: "#DDEEF4"

            Text {
                anchors { left: parent.left; leftMargin: 10; verticalCenter: parent.verticalCenter }
                text: "History"
                font.pixelSize: 13
                font.weight: Font.DemiBold
                color: "#2F3438"
            }
        }

        Item { Layout.fillHeight: true }
    }
}

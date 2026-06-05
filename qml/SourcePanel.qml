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

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#F8F8F5"

            ListView {
                id: historyList
                anchors.fill: parent
                anchors.margins: 8
                clip: true
                model: ProcessingController.historyLabels
                boundsBehavior: Flickable.StopAtBounds
                spacing: 4

                delegate: Rectangle {
                    id: historyRow
                    required property string modelData
                    required property int index

                    width: ListView.view.width
                    height: 30
                    radius: 5
                    color: index === ProcessingController.historyIndex ? "#DDEEF4"
                           : index < ProcessingController.historyIndex ? "#FFFFFF"
                           : "#F1F2F3"
                    border.color: index <= ProcessingController.historyIndex ? "#E5E8EB" : "#ECEFF2"
                    border.width: 1

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8
                        spacing: 6

                        Text {
                            text: String(historyRow.index + 1)
                            font.pixelSize: 11
                            color: historyRow.index <= ProcessingController.historyIndex ? "#6F86AB" : "#9AA1A8"
                            horizontalAlignment: Text.AlignHCenter
                            Layout.preferredWidth: 20
                        }

                        Text {
                            text: historyRow.modelData
                            font.pixelSize: 12
                            color: historyRow.index <= ProcessingController.historyIndex ? "#2F3438" : "#9AA1A8"
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                    }
                }

                ScrollBar.vertical: ScrollBar {}
            }

            Text {
                anchors.centerIn: parent
                visible: ProcessingController.historyLabels.length === 0
                text: "No history"
                font.pixelSize: 12
                color: "#6E747A"
            }
        }
    }
}

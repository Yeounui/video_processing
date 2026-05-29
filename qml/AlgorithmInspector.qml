import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    color: "#F8F8F5"

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Category tabs
        Rectangle {
            Layout.fillWidth: true
            height: 40
            color: "#F8F8F5"
            border.color: "#E5E8EB"
            border.width: 1

            Row {
                anchors { fill: parent; leftMargin: 8; rightMargin: 8; topMargin: 6; bottomMargin: 6 }
                spacing: 4

                Repeater {
                    model: ["Point", "Geometry", "Filter", "Edge", "Morphology", "Grayscale"]
                    delegate: Rectangle {
                        width: tabLabel.implicitWidth + 12
                        height: 28
                        color: index === 0 ? "#6F86AB" : "#FFFFFF"
                        radius: 4
                        border.color: index === 0 ? "transparent" : "#E5E8EB"
                        border.width: 1

                        Text {
                            id: tabLabel
                            anchors.centerIn: parent
                            text: modelData
                            font.pixelSize: 12
                            color: index === 0 ? "#FFFFFF" : "#2F3438"
                        }
                    }
                }
            }
        }

        // Algorithm list (empty state)
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            Text {
                anchors.centerIn: parent
                text: "No algorithms loaded"
                font.pixelSize: 13
                color: "#6E747A"
            }
        }

        // Apply / Reset row
        Rectangle {
            Layout.fillWidth: true
            height: 56
            color: "#F8F8F5"
            border.color: "#E5E8EB"
            border.width: 1

            RowLayout {
                anchors { fill: parent; margins: 10 }
                spacing: 8

                Button {
                    Layout.fillWidth: true
                    text: "Apply"
                    implicitHeight: 32
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
                    Layout.preferredWidth: 72
                    text: "Reset"
                    implicitHeight: 32
                    background: Rectangle {
                        color: "transparent"
                        border.color: "#E5E8EB"
                        border.width: 1
                        radius: 6
                    }
                    contentItem: Text {
                        text: parent.text
                        color: "#2F3438"
                        font.pixelSize: 13
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }
        }
    }
}

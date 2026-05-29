import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: root
    visible: true
    width: 1280
    height: 800
    minimumWidth: 900
    minimumHeight: 600
    title: "Qt UI"
    color: "#F8F8F5"

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        TopBar {
            Layout.fillWidth: true
            Layout.preferredHeight: 52
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: "#E5E8EB"
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            SourcePanel {
                Layout.preferredWidth: 260
                Layout.minimumWidth: 220
                Layout.fillHeight: true
            }

            Rectangle {
                Layout.fillHeight: true
                width: 1
                color: "#E5E8EB"
            }

            ProcessingViewport {
                Layout.fillWidth: true
                Layout.fillHeight: true
            }

            Rectangle {
                Layout.fillHeight: true
                width: 1
                color: "#E5E8EB"
            }

            AlgorithmInspector {
                Layout.preferredWidth: 340
                Layout.minimumWidth: 300
                Layout.fillHeight: true
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: "#E5E8EB"
        }

        BottomTransport {
            Layout.fillWidth: true
            Layout.preferredHeight: 44
        }
    }
}

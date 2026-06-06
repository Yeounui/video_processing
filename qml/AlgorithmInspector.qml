import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtUi 1.0

Rectangle {
    id: root
    color: "#F8F8F5"

    property int selectedAlgorithmId: -1
    property var selectedParams: []

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: categoryRows.implicitHeight + 12
            color: "#F8F8F5"
            border.color: "#E5E8EB"
            border.width: 1

            Column {
                id: categoryRows
                anchors {
                    leftMargin: 8
                    rightMargin: 8
                    topMargin: 6
                    bottomMargin: 6
                    left: parent.left
                    right: parent.right
                    top: parent.top
                }
                spacing: 3

                Component {
                    id: categoryTabDelegate

                    Rectangle {
                        width: Math.max(44, tabLabel.implicitWidth + 10)
                        height: 25
                        radius: 4
                        color: ProcessingController.algorithmModel.category === modelData ? "#6F86AB" : "#FFFFFF"
                        border.color: ProcessingController.algorithmModel.category === modelData ? "transparent" : "#E5E8EB"
                        border.width: 1

                            Text {
                                id: tabLabel
                                anchors.centerIn: parent
                                text: modelData
                                font.pixelSize: 11
                                color: ProcessingController.algorithmModel.category === modelData ? "#FFFFFF" : "#2F3438"
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    ProcessingController.algorithmModel.category = modelData
                                    root.selectedAlgorithmId = -1
                                    root.selectedParams = []
                                }
                            }
                    }
                }

                Row {
                    spacing: 4
                    Repeater {
                        model: ["Point", "Geometry", "Filter", "Edge"]
                        delegate: categoryTabDelegate
                    }
                }

                Row {
                    spacing: 4
                    Repeater {
                        model: ["Morphology", "Grayscale"]
                        delegate: categoryTabDelegate
                    }
                }
            }
        }

        ListView {
            id: algorithmList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: ProcessingController.algorithmModel
            boundsBehavior: Flickable.StopAtBounds

            delegate: Rectangle {
                id: algorithmDelegate
                width: ListView.view.width
                height: 38
                color: root.selectedAlgorithmId === model.id ? "#DDEEF4"
                       : hoverHandler.hovered ? "#F0F4F8"
                       : "#F8F8F5"

                Text {
                    anchors {
                        left: parent.left
                        right: parent.right
                        verticalCenter: parent.verticalCenter
                        leftMargin: 14
                        rightMargin: 14
                    }
                    text: model.name
                    color: "#2F3438"
                    font.pixelSize: 13
                    elide: Text.ElideRight
                }

                HoverHandler {
                    id: hoverHandler
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        root.selectedAlgorithmId = model.id
                        root.selectedParams = model.params
                    }
                }
            }

            ScrollBar.vertical: ScrollBar {}
        }

        Rectangle {
            Layout.fillWidth: true
            visible: root.selectedAlgorithmId > 0 && root.selectedParams.length > 0
            implicitHeight: visible ? Math.min(220, paramContent.implicitHeight + 24) : 0
            color: "#FFFFFF"
            border.color: "#E5E8EB"
            border.width: 1

            ScrollView {
                anchors.fill: parent
                anchors.margins: 12
                clip: true

                ParameterEditor {
                    id: paramEditor
                    width: parent.width
                    paramSpecs: root.selectedParams
                }
            }

            Item {
                id: paramContent
                visible: false
                implicitHeight: paramEditor.implicitHeight
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 48
            color: "#F8F8F5"
            border.color: "#E5E8EB"
            border.width: 1

            RowLayout {
                anchors {
                    fill: parent
                    margins: 8
                }
                spacing: 5

                Button {
                    id: applyButton
                    Layout.preferredWidth: 56
                    text: "Apply"
                    enabled: root.selectedAlgorithmId > 0 && ProcessingController.hasImage
                    implicitHeight: 30
                    onClicked: ProcessingController.applyAlgorithm(root.selectedAlgorithmId, paramEditor.paramValues)

                    background: Rectangle {
                        color: applyButton.enabled ? "#6F86AB" : "#D8DDE3"
                        radius: 5
                    }

                    contentItem: Text {
                        text: applyButton.text
                        color: applyButton.enabled ? "#FFFFFF" : "#6E747A"
                        font.pixelSize: 12
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }

                Item {
                    Layout.fillWidth: true
                }

                Button {
                    id: undoButton
                    Layout.preferredWidth: 48
                    text: "Undo"
                    enabled: ProcessingController.canUndo
                    implicitHeight: 30
                    onClicked: ProcessingController.undo()

                    background: Rectangle {
                        color: "transparent"
                        border.color: undoButton.enabled ? "#C8D0D8" : "#E5E8EB"
                        border.width: 1
                        radius: 5
                    }

                    contentItem: Text {
                        text: undoButton.text
                        color: undoButton.enabled ? "#2F3438" : "#9AA1A8"
                        font.pixelSize: 11
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }

                Button {
                    id: redoButton
                    Layout.preferredWidth: 48
                    text: "Redo"
                    enabled: ProcessingController.canRedo
                    implicitHeight: 30
                    onClicked: ProcessingController.redo()

                    background: Rectangle {
                        color: "transparent"
                        border.color: redoButton.enabled ? "#C8D0D8" : "#E5E8EB"
                        border.width: 1
                        radius: 5
                    }

                    contentItem: Text {
                        text: redoButton.text
                        color: redoButton.enabled ? "#2F3438" : "#9AA1A8"
                        font.pixelSize: 11
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }

                Button {
                    id: resetButton
                    Layout.preferredWidth: 50
                    text: "Reset"
                    enabled: ProcessingController.hasImage
                    implicitHeight: 30
                    onClicked: ProcessingController.reset()

                    background: Rectangle {
                        color: "transparent"
                        border.color: resetButton.enabled ? "#C8D0D8" : "#E5E8EB"
                        border.width: 1
                        radius: 5
                    }

                    contentItem: Text {
                        text: resetButton.text
                        color: resetButton.enabled ? "#2F3438" : "#9AA1A8"
                        font.pixelSize: 11
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }
        }
    }
}

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root
    spacing: 10

    property var paramSpecs: []
    property var paramValues: ({})
    readonly property int paramRowHeight: 52

    function valueFor(spec, value) {
        if (spec.valueType === 0)
            return Math.round(Number(value))
        return value
    }

    function setParamValue(key, value) {
        var updated = Object.assign({}, paramValues)
        updated[key] = value
        paramValues = updated
    }

    function initDefaults() {
        var defaults = {}
        for (var i = 0; i < paramSpecs.length; ++i) {
            var spec = paramSpecs[i]
            if (!spec.isAutomatic)
                defaults[spec.key] = valueFor(spec, spec.defaultValue)
        }
        paramValues = defaults
    }

    function specForKey(key) {
        for (var i = 0; i < paramSpecs.length; ++i) {
            if (paramSpecs[i].key === key)
                return paramSpecs[i]
        }
        return ({})
    }

    function isGaussianKernelSigma() {
        return paramSpecs.length === 2
               && specForKey("kernel").key === "kernel"
               && specForKey("sigma").key === "sigma"
    }

    onParamSpecsChanged: initDefaults()
    Component.onCompleted: initDefaults()

    Item {
        id: gaussianRow
        Layout.fillWidth: true
        Layout.preferredHeight: visible ? root.paramRowHeight : 0
        implicitHeight: visible ? root.paramRowHeight : 0
        visible: root.isGaussianKernelSigma()

        property var kernelSpec: root.specForKey("kernel")
        property var sigmaSpec: root.specForKey("sigma")

        RowLayout {
            anchors.fill: parent
            spacing: 10

            ColumnLayout {
                Layout.preferredWidth: 66
                Layout.fillHeight: true
                spacing: 4

                RowLayout {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 16

                    Text {
                        Layout.fillWidth: true
                        text: "Kernel"
                        color: "#2F3438"
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }
                }

                ComboBox {
                    id: gaussianKernelCombo
                    Layout.fillWidth: true
                    Layout.preferredHeight: 34
                    implicitHeight: 34

                    model: gaussianRow.kernelSpec.enumValues || []
                    onCurrentTextChanged: root.setParamValue("kernel", currentText)

                    background: Item {
                        implicitHeight: 34

                        Rectangle {
                            anchors {
                                left: parent.left
                                right: parent.right
                                verticalCenter: parent.verticalCenter
                            }
                            height: 26
                            radius: 4
                            color: "#FFFFFF"
                            border.color: "#C8D0D8"
                            border.width: 1
                        }
                    }

                    contentItem: Text {
                        anchors {
                            left: parent.left
                            right: parent.right
                            verticalCenter: parent.verticalCenter
                            leftMargin: 8
                            rightMargin: 8
                        }
                        text: gaussianKernelCombo.displayText
                        color: "#2F3438"
                        font.pixelSize: 12
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 4

                RowLayout {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 16

                    Text {
                        Layout.fillWidth: true
                        text: "Sigma"
                        color: "#2F3438"
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }

                    Text {
                        Layout.preferredWidth: 34
                        text: Number(gaussianSigmaSlider.value).toFixed(1)
                        color: "#6E747A"
                        font.pixelSize: 12
                        horizontalAlignment: Text.AlignRight
                    }
                }

                Item {
                    id: gaussianSigmaSlider
                    Layout.fillWidth: true
                    Layout.minimumWidth: 96
                    Layout.preferredHeight: 34

                    property real from: isNaN(Number(gaussianRow.sigmaSpec.min)) ? 0.1 : Number(gaussianRow.sigmaSpec.min)
                    property real to: isNaN(Number(gaussianRow.sigmaSpec.max)) ? 5.0 : Number(gaussianRow.sigmaSpec.max)
                    property real stepSize: isNaN(Number(gaussianRow.sigmaSpec.step)) ? 0.1 : Number(gaussianRow.sigmaSpec.step)
                    property real value: isNaN(Number(gaussianRow.sigmaSpec.defaultValue)) ? 1.0 : Number(gaussianRow.sigmaSpec.defaultValue)
                    property bool pressed: false
                    readonly property real range: to - from
                    readonly property real position: range === 0 ? 0 : (value - from) / range
                    readonly property real visualPosition: Math.max(0, Math.min(1, position))

                    onValueChanged: root.setParamValue("sigma", value)

                    function setValueFromX(mouseX) {
                        if (sigmaTrack.width <= 0)
                            return

                        var rawPosition = (mouseX - sigmaTrack.x) / sigmaTrack.width
                        var clampedPosition = Math.max(0, Math.min(1, rawPosition))
                        var rawValue = from + clampedPosition * range
                        if (stepSize > 0)
                            rawValue = from + Math.round((rawValue - from) / stepSize) * stepSize
                        value = Math.max(Math.min(from, to), Math.min(Math.max(from, to), rawValue))
                    }

                    Rectangle {
                        id: sigmaTrack
                        anchors {
                            left: parent.left
                            right: parent.right
                            verticalCenter: parent.verticalCenter
                        }
                        height: 4
                        radius: 2
                        color: "#E5E8EB"

                        Rectangle {
                            width: gaussianSigmaSlider.visualPosition * parent.width
                            height: parent.height
                            radius: 2
                            color: "#6F86AB"
                        }
                    }

                    Rectangle {
                        width: 18
                        height: 18
                        x: sigmaTrack.x + gaussianSigmaSlider.visualPosition * Math.max(0, sigmaTrack.width - width)
                        y: sigmaTrack.y + sigmaTrack.height / 2 - height / 2
                        radius: 9
                        color: gaussianSigmaSlider.pressed ? "#5A73A0" : "#6F86AB"
                        border.color: "#FFFFFF"
                        border.width: 2
                    }

                    MouseArea {
                        anchors.fill: parent
                        acceptedButtons: Qt.LeftButton
                        cursorShape: Qt.PointingHandCursor
                        preventStealing: true

                        onPressed: (mouse) => {
                            gaussianSigmaSlider.pressed = true
                            gaussianSigmaSlider.setValueFromX(mouse.x)
                            mouse.accepted = true
                        }

                        onPositionChanged: (mouse) => {
                            if (pressed)
                                gaussianSigmaSlider.setValueFromX(mouse.x)
                        }

                        onReleased: gaussianSigmaSlider.pressed = false
                        onCanceled: gaussianSigmaSlider.pressed = false
                    }
                }
            }
        }
    }

    Repeater {
        model: root.isGaussianKernelSigma() ? [] : root.paramSpecs

        delegate: ColumnLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: root.paramRowHeight
            implicitHeight: root.paramRowHeight
            spacing: 4

            readonly property var spec: modelData

            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: root.paramRowHeight
                implicitHeight: root.paramRowHeight
                visible: spec.isAutomatic

                RowLayout {
                    anchors {
                        left: parent.left
                        right: parent.right
                        top: parent.top
                    }
                    height: 16

                    Text {
                        Layout.fillWidth: true
                        text: spec.label
                        color: "#2F3438"
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }

                    Text {
                        text: "Auto"
                        color: "#6E747A"
                        font.pixelSize: 12
                    }
                }

                Item {
                    anchors {
                        left: parent.left
                        right: parent.right
                        bottom: parent.bottom
                    }
                    height: 34
                }
            }

            Loader {
                Layout.fillWidth: true
                Layout.preferredHeight: root.paramRowHeight
                height: root.paramRowHeight
                active: !spec.isAutomatic
                visible: active
                sourceComponent: spec.controlType === 0 ? sliderEditor
                                 : spec.controlType === 1 ? comboEditor
                                 : spinEditor
            }

            Component {
                id: sliderEditor

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.preferredHeight: root.paramRowHeight
                    width: parent ? parent.width : implicitWidth
                    implicitHeight: root.paramRowHeight
                    spacing: 4

                    RowLayout {
                        Layout.fillWidth: true

                        Text {
                            Layout.fillWidth: true
                            text: spec.label
                            color: "#2F3438"
                            font.pixelSize: 12
                            elide: Text.ElideRight
                        }

                        Text {
                            text: spec.valueType === 0 ? Math.round(sliderControl.value).toString()
                                                       : Number(sliderControl.value).toFixed(2)
                            color: "#6E747A"
                            font.pixelSize: 12
                        }
                    }

                    Item {
                        id: sliderControl
                        Layout.fillWidth: true
                        Layout.minimumWidth: 96
                        Layout.preferredHeight: 34
                        implicitHeight: 34

                        property real from: Number(spec.min)
                        property real to: Number(spec.max)
                        property real stepSize: Number(spec.step)
                        property real value: Number(spec.defaultValue)
                        property bool pressed: false
                        readonly property real range: to - from
                        readonly property real position: range === 0 ? 0 : (value - from) / range
                        readonly property real visualPosition: Math.max(0, Math.min(1, position))

                        onValueChanged: root.setParamValue(spec.key, root.valueFor(spec, value))

                        function setValueFromX(mouseX) {
                            if (track.width <= 0)
                                return

                            var rawPosition = (mouseX - track.x) / track.width
                            var clampedPosition = Math.max(0, Math.min(1, rawPosition))
                            var rawValue = from + clampedPosition * range
                            if (stepSize > 0) {
                                rawValue = from + Math.round((rawValue - from) / stepSize) * stepSize
                            }
                            value = Math.max(Math.min(from, to), Math.min(Math.max(from, to), rawValue))
                        }

                        function stepBy(direction) {
                            var step = stepSize > 0 ? stepSize : 1
                            value = Math.max(Math.min(from, to), Math.min(Math.max(from, to), value + direction * step))
                        }

                        Keys.onDownPressed: (event) => { stepBy(-1); event.accepted = true }
                        Keys.onLeftPressed: (event) => { stepBy(-1); event.accepted = true }
                        Keys.onUpPressed: (event) => { stepBy(1); event.accepted = true }
                        Keys.onRightPressed: (event) => { stepBy(1); event.accepted = true }

                        Rectangle {
                            id: track
                            anchors {
                                left: parent.left
                                right: parent.right
                                verticalCenter: parent.verticalCenter
                            }
                            height: 4
                            radius: 2
                            color: "#E5E8EB"

                            Rectangle {
                                width: sliderControl.visualPosition * parent.width
                                height: parent.height
                                radius: 2
                                color: "#6F86AB"
                            }
                        }

                        Rectangle {
                            width: 18
                            height: 18
                            x: track.x + sliderControl.visualPosition * Math.max(0, track.width - width)
                            y: track.y + track.height / 2 - height / 2
                            radius: 9
                            color: sliderControl.pressed ? "#5A73A0" : "#6F86AB"
                            border.color: "#FFFFFF"
                            border.width: 2
                        }

                        MouseArea {
                            anchors.fill: parent
                            acceptedButtons: Qt.LeftButton
                            cursorShape: Qt.PointingHandCursor
                            preventStealing: true

                            onPressed: (mouse) => {
                                sliderControl.pressed = true
                                sliderControl.forceActiveFocus()
                                sliderControl.setValueFromX(mouse.x)
                                mouse.accepted = true
                            }

                            onPositionChanged: (mouse) => {
                                if (pressed)
                                    sliderControl.setValueFromX(mouse.x)
                            }

                            onReleased: sliderControl.pressed = false
                            onCanceled: sliderControl.pressed = false
                        }
                    }
                }
            }

            Component {
                id: comboEditor

                Item {
                    id: comboRow
                    Layout.fillWidth: true
                    Layout.preferredHeight: root.paramRowHeight
                    width: parent ? parent.width : implicitWidth
                    implicitHeight: root.paramRowHeight

                    RowLayout {
                        anchors {
                            left: parent.left
                            right: parent.right
                            top: parent.top
                        }
                        height: 16

                        Text {
                            Layout.fillWidth: true
                            text: spec.label
                            color: "#2F3438"
                            font.pixelSize: 12
                            elide: Text.ElideRight
                        }
                    }

                    ComboBox {
                        id: combo
                        anchors {
                            right: parent.right
                            bottom: parent.bottom
                        }
                        width: Math.min(72, parent.width)
                        height: 34
                        model: spec.enumValues
                        onCurrentTextChanged: root.setParamValue(spec.key, currentText)

                        Component.onCompleted: {
                            var defaultIndex = spec.enumValues.indexOf(String(spec.defaultValue))
                            currentIndex = defaultIndex >= 0 ? defaultIndex : 0
                            root.setParamValue(spec.key, currentText)
                        }
                    }
                }
            }

            Component {
                id: spinEditor

                Item {
                    id: spinRow
                    Layout.fillWidth: true
                    Layout.preferredHeight: root.paramRowHeight
                    width: parent ? parent.width : implicitWidth
                    implicitHeight: root.paramRowHeight

                    property int currentValue: Math.round(Number(spec.defaultValue))

                    function setCurrentValue(nextValue) {
                        var numericValue = Number(nextValue)
                        if (isNaN(numericValue))
                            numericValue = currentValue
                        var minValue = Math.round(Number(spec.min))
                        var maxValue = Math.round(Number(spec.max))
                        currentValue = Math.max(minValue, Math.min(maxValue, Math.round(numericValue)))
                        if (spinInput.text !== currentValue.toString())
                            spinInput.text = currentValue.toString()
                        root.setParamValue(spec.key, currentValue)
                    }

                    Component.onCompleted: setCurrentValue(currentValue)

                    RowLayout {
                        anchors {
                            left: parent.left
                            right: parent.right
                            top: parent.top
                        }
                        height: 16

                        Text {
                            Layout.fillWidth: true
                            text: spec.label
                            color: "#2F3438"
                            font.pixelSize: 12
                            elide: Text.ElideRight
                        }
                    }

                    Item {
                        id: spinControl
                        anchors {
                            right: parent.right
                            bottom: parent.bottom
                        }
                        height: 34
                        width: Math.min(88, parent.width)
                        implicitWidth: 88
                        implicitHeight: 34

                        Rectangle {
                            anchors.fill: parent
                            color: "#FFFFFF"
                            border.color: "#C8D0D8"
                            border.width: 1
                            radius: 4
                        }

                        Rectangle {
                            id: valuePane
                            anchors {
                                left: parent.left
                                top: parent.top
                                bottom: parent.bottom
                                right: buttonPane.left
                            }
                            color: "transparent"
                            clip: true
                        }

                        TextInput {
                            id: spinInput
                            anchors {
                                left: valuePane.left
                                right: valuePane.right
                                verticalCenter: parent.verticalCenter
                                leftMargin: 8
                                rightMargin: 6
                            }
                            text: spinRow.currentValue
                            color: "#2F3438"
                            font.pixelSize: 12
                            horizontalAlignment: Text.AlignRight
                            verticalAlignment: TextInput.AlignVCenter
                            selectByMouse: true
                            validator: IntValidator {
                                bottom: Math.round(Number(spec.min))
                                top: Math.round(Number(spec.max))
                            }
                            onEditingFinished: spinRow.setCurrentValue(Number(text))
                            onActiveFocusChanged: {
                                if (!activeFocus)
                                    text = spinRow.currentValue
                            }
                        }

                        Item {
                            id: buttonPane
                            anchors {
                                top: parent.top
                                right: parent.right
                                bottom: parent.bottom
                            }
                            width: 34

                            Rectangle {
                                id: upButtonCell
                                anchors {
                                    top: parent.top
                                    left: parent.left
                                    right: parent.right
                                }
                                height: parent.height / 2
                                color: upMouse.pressed ? "#E5E8EB" : "#F8F8F5"
                                border.color: "#C8D0D8"
                                border.width: 1

                                Text {
                                    anchors.centerIn: parent
                                    text: "+"
                                    color: "#2F3438"
                                    font.pixelSize: 13
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }

                                MouseArea {
                                    id: upMouse
                                    anchors.fill: parent
                                    acceptedButtons: Qt.LeftButton
                                    cursorShape: Qt.PointingHandCursor
                                    preventStealing: true
                                    onPressed: (mouse) => {
                                        mouse.accepted = true
                                        spinRow.setCurrentValue(spinRow.currentValue + Math.max(1, Math.round(Number(spec.step))))
                                    }
                                }
                            }

                            Rectangle {
                                id: downButtonCell
                                anchors {
                                    top: upButtonCell.bottom
                                    left: parent.left
                                    right: parent.right
                                    bottom: parent.bottom
                                }
                                color: downMouse.pressed ? "#E5E8EB" : "#F8F8F5"
                                border.color: "#C8D0D8"
                                border.width: 1

                                Text {
                                    anchors.centerIn: parent
                                    text: "-"
                                    color: "#2F3438"
                                    font.pixelSize: 13
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }

                                MouseArea {
                                    id: downMouse
                                    anchors.fill: parent
                                    acceptedButtons: Qt.LeftButton
                                    cursorShape: Qt.PointingHandCursor
                                    preventStealing: true
                                    onPressed: (mouse) => {
                                        mouse.accepted = true
                                        spinRow.setCurrentValue(spinRow.currentValue - Math.max(1, Math.round(Number(spec.step))))
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

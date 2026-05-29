import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root
    spacing: 10

    property var paramSpecs: []
    property var paramValues: ({})

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

    onParamSpecsChanged: initDefaults()
    Component.onCompleted: initDefaults()

    Repeater {
        model: root.paramSpecs

        delegate: ColumnLayout {
            Layout.fillWidth: true
            spacing: 4

            readonly property var spec: modelData

            RowLayout {
                Layout.fillWidth: true
                visible: spec.isAutomatic

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

            Loader {
                Layout.fillWidth: true
                active: !spec.isAutomatic
                sourceComponent: spec.controlType === 0 ? sliderEditor
                                 : spec.controlType === 1 ? comboEditor
                                 : spinEditor
            }

            Component {
                id: sliderEditor

                ColumnLayout {
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
                            text: spec.valueType === 0 ? Math.round(slider.value).toString()
                                                       : Number(slider.value).toFixed(2)
                            color: "#6E747A"
                            font.pixelSize: 12
                        }
                    }

                    Slider {
                        id: slider
                        Layout.fillWidth: true
                        from: Number(spec.min)
                        to: Number(spec.max)
                        stepSize: Number(spec.step)
                        value: Number(spec.defaultValue)
                        onValueChanged: root.setParamValue(spec.key, root.valueFor(spec, value))
                    }
                }
            }

            Component {
                id: comboEditor

                RowLayout {
                    spacing: 8

                    Text {
                        Layout.fillWidth: true
                        text: spec.label
                        color: "#2F3438"
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }

                    ComboBox {
                        id: combo
                        Layout.preferredWidth: 116
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

                RowLayout {
                    spacing: 8

                    Text {
                        Layout.fillWidth: true
                        text: spec.label
                        color: "#2F3438"
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }

                    SpinBox {
                        Layout.preferredWidth: 96
                        from: Math.round(Number(spec.min))
                        to: Math.round(Number(spec.max))
                        stepSize: Math.max(1, Math.round(Number(spec.step)))
                        value: Math.round(Number(spec.defaultValue))
                        onValueChanged: root.setParamValue(spec.key, value)
                    }
                }
            }
        }
    }
}

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt.labs.folderlistmodel
import QtCore

Dialog {
    id: root

    property url initialFolder: ""
    property var nameFilters: ["All files (*)"]
    property var modelNameFilters: ["*"]
    property string acceptLabel: "Open"
    property bool saveMode: false
    property url currentFolder: ""
    property url selectedFile: ""
    property string selectedFileName: ""
    property bool pendingAccepted: false
    property url pendingFile: ""
    property var pathEntries: []

    signal finished(bool accepted, url fileUrl, url folder)

    modal: true
    dim: true
    closePolicy: Popup.CloseOnEscape
    width: Math.min(parent ? parent.width - 48 : 760, 760)
    height: Math.min(parent ? parent.height - 72 : 540, 540)
    x: parent ? Math.round((parent.width - width) / 2) : 0
    y: parent ? Math.round((parent.height - height) / 2) : 0

    function homeFolder() {
        return normalizeFolder(StandardPaths.writableLocation(StandardPaths.HomeLocation))
    }

    function normalizeFolder(folder) {
        let value = folder ? folder.toString() : ""
        if (value.length === 0)
            value = StandardPaths.writableLocation(StandardPaths.HomeLocation).toString()
        if (value.indexOf("file://") !== 0)
            value = "file://" + encodeURI(value)
        if (value.length > 8 && value.endsWith("/"))
            value = value.slice(0, -1)
        return value
    }

    function pathToUrl(path) {
        if (path.length === 0 || path === "/")
            return "file:///"
        return "file://" + encodeURI(path)
    }

    function pathFromUrl(fileUrl) {
        let value = fileUrl ? fileUrl.toString() : ""
        if (value.indexOf("file://") === 0)
            value = value.substring(7)
        if (value.length === 0)
            return "/"
        try {
            return decodeURIComponent(value)
        } catch (e) {
            return value
        }
    }

    function fileNameFromUrl(fileUrl) {
        const path = pathFromUrl(fileUrl)
        const index = path.lastIndexOf("/")
        return index >= 0 ? path.substring(index + 1) : path
    }

    function refreshModelNameFilters() {
        let filters = []
        for (let i = 0; i < nameFilters.length; ++i) {
            const filter = nameFilters[i].toString()
            const openIndex = filter.indexOf("(")
            const closeIndex = filter.lastIndexOf(")")
            const patternText = openIndex >= 0 && closeIndex > openIndex
                    ? filter.substring(openIndex + 1, closeIndex)
                    : filter
            const patterns = patternText.split(/\s+/)
            for (let j = 0; j < patterns.length; ++j) {
                const pattern = patterns[j].trim()
                if (pattern.length > 0)
                    filters.push(pattern)
            }
        }

        if (filters.length === 0)
            filters = ["*"]
        modelNameFilters = filters
    }

    function buildPathEntries() {
        const path = pathFromUrl(currentFolder)
        const parts = path.split("/").filter(function(part) { return part.length > 0 })
        let entries = [{ "label": "/", "url": "file:///" }]
        let currentPath = ""
        for (let i = 0; i < parts.length; ++i) {
            currentPath += "/" + parts[i]
            entries.push({ "label": parts[i], "url": pathToUrl(currentPath) })
        }
        return entries
    }

    function parentFolderUrl() {
        const path = pathFromUrl(currentFolder)
        if (path === "/")
            return currentFolder
        const index = path.lastIndexOf("/")
        return pathToUrl(index <= 0 ? "/" : path.substring(0, index))
    }

    function navigateTo(folder) {
        addressPopup.close()
        currentFolder = normalizeFolder(folder)
        selectedFile = ""
        selectedFileName = ""
        fileNameField.text = ""
    }

    function fileUrlForName(name) {
        const trimmed = name.trim()
        if (trimmed.indexOf("file://") === 0)
            return trimmed
        if (trimmed.indexOf("/") === 0)
            return pathToUrl(trimmed)

        const folderPath = pathFromUrl(currentFolder)
        const separator = folderPath.endsWith("/") ? "" : "/"
        return pathToUrl(folderPath + separator + trimmed)
    }

    function acceptCurrent() {
        let fileUrl = selectedFile
        if (saveMode || fileUrl.toString().length === 0)
            fileUrl = fileUrlForName(fileNameField.text)
        if (fileUrl.toString().length === 0)
            return

        pendingAccepted = true
        pendingFile = fileUrl
        close()
    }

    onOpened: {
        refreshModelNameFilters()
        currentFolder = normalizeFolder(initialFolder)
        selectedFile = ""
        selectedFileName = ""
        pendingAccepted = false
        pendingFile = ""
        fileNameField.text = ""
        addressPopup.close()
    }

    onNameFiltersChanged: refreshModelNameFilters()
    onCurrentFolderChanged: pathEntries = buildPathEntries()

    onClosed: {
        addressPopup.close()
        const accepted = pendingAccepted
        const fileUrl = pendingFile
        pendingAccepted = false
        pendingFile = ""
        finished(accepted, fileUrl, currentFolder)
    }

    background: Rectangle {
        color: "#FFFFFF"
        radius: 7
        border.color: "#C9D3DC"
        border.width: 1
    }

    header: Rectangle {
        height: 48
        color: "#F8F8F5"
        border.color: "#E5E8EB"
        border.width: 1

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 14
            anchors.rightMargin: 10
            spacing: 8

            Label {
                text: root.title
                font.pixelSize: 14
                font.weight: Font.DemiBold
                color: "#2F3438"
                elide: Text.ElideRight
                Layout.fillWidth: true
            }

            Button {
                text: "Close"
                implicitHeight: 30
                onClicked: root.close()
            }
        }
    }

    footer: Rectangle {
        height: 58
        color: "#F8F8F5"
        border.color: "#E5E8EB"
        border.width: 1

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            spacing: 8

            Item { Layout.fillWidth: true }

            Button {
                text: root.acceptLabel
                implicitHeight: 32
                enabled: root.saveMode ? fileNameField.text.trim().length > 0 : root.selectedFile.toString().length > 0
                onClicked: root.acceptCurrent()
            }

            Button {
                text: "Close"
                implicitHeight: 32
                onClicked: root.close()
            }
        }
    }

    contentItem: ColumnLayout {
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            Button {
                text: "Up"
                implicitWidth: 54
                implicitHeight: 32
                enabled: root.pathFromUrl(root.currentFolder) !== "/"
                onClicked: root.navigateTo(root.parentFolderUrl())
            }

            Button {
                id: addressButton
                text: root.pathFromUrl(root.currentFolder)
                implicitHeight: 32
                Layout.fillWidth: true
                contentItem: Text {
                    text: parent.text
                    color: "#2F3438"
                    font.pixelSize: 13
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideMiddle
                }
                onClicked: {
                    if (addressPopup.opened)
                        addressPopup.close()
                    else
                        addressPopup.open()
                }

                Popup {
                    id: addressPopup
                    y: addressButton.height + 4
                    width: addressButton.width
                    height: Math.min(pathList.contentHeight + 8, 260)
                    z: 1000
                    modal: true
                    dim: false
                    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
                    background: Rectangle {
                        color: "#FFFFFF"
                        radius: 6
                        border.color: "#C9D3DC"
                        border.width: 1
                    }
                    contentItem: ListView {
                        id: pathList
                        anchors.fill: parent
                        anchors.margins: 4
                        clip: true
                        model: root.pathEntries
                        delegate: ItemDelegate {
                            required property var modelData
                            width: ListView.view.width
                            height: 30
                            text: modelData.label
                            onClicked: root.navigateTo(modelData.url)
                        }
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#FFFFFF"
            border.color: "#E5E8EB"
            border.width: 1
            radius: 6
            clip: true

            FolderListModel {
                id: folderModel
                folder: root.currentFolder
                nameFilters: root.modelNameFilters
                showDirs: true
                showFiles: true
                showDirsFirst: true
                showDotAndDotDot: false
                sortField: FolderListModel.Name
            }

            ListView {
                id: fileList
                anchors.fill: parent
                clip: true
                model: folderModel
                delegate: ItemDelegate {
                    id: fileRow
                    required property string fileName
                    required property url fileUrl
                    required property bool fileIsDir

                    width: ListView.view.width
                    height: 34
                    highlighted: !fileIsDir && root.selectedFile.toString() === fileUrl.toString()
                    onClicked: {
                        if (fileIsDir) {
                            root.navigateTo(fileUrl)
                        } else {
                            root.selectedFile = fileUrl
                            root.selectedFileName = fileName
                            fileNameField.text = fileName
                        }
                    }
                    onDoubleClicked: {
                        if (fileIsDir) {
                            root.navigateTo(fileUrl)
                        } else {
                            root.selectedFile = fileUrl
                            root.selectedFileName = fileName
                            fileNameField.text = fileName
                            root.acceptCurrent()
                        }
                    }
                    contentItem: RowLayout {
                        spacing: 8

                        Label {
                            text: fileRow.fileIsDir ? "Dir" : "File"
                            color: fileRow.fileIsDir ? "#6F86AB" : "#6E747A"
                            font.pixelSize: 11
                            Layout.preferredWidth: 34
                        }

                        Label {
                            text: fileRow.fileName
                            color: "#2F3438"
                            font.pixelSize: 13
                            elide: Text.ElideMiddle
                            Layout.fillWidth: true
                        }
                    }
                }
            }

            Label {
                anchors.centerIn: parent
                visible: folderModel.count === 0
                text: "No matching files"
                color: "#6E747A"
                font.pixelSize: 13
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Label {
                text: "File"
                color: "#2F3438"
                font.pixelSize: 13
            }

            TextField {
                id: fileNameField
                Layout.fillWidth: true
                implicitHeight: 32
                readOnly: !root.saveMode
                placeholderText: root.saveMode ? "File name" : "Select a file"
                selectByMouse: true
                onTextEdited: {
                    if (root.saveMode)
                        root.selectedFileName = text
                }
            }
        }
    }
}

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Dialogs as Dialogs
import QtQuick.Layouts
import QtQuick.Window
import "EditorMutations.js" as EditorMutations

ApplicationWindow {
    id: win
    width: 1280
    height: 820
    minimumWidth: 720
    minimumHeight: 520
    visible: true
    title: (backend.modified ? "* " : "") + backend.fileName + " - Omawrite"
    flags: Qt.Window

    readonly property bool darkMode: backend.darkMode
    readonly property color pageColor: backend.themeBackground
    readonly property color textColor: backend.themeForeground
    readonly property color strongTextColor: backend.themeForeground
    readonly property color mutedColor: darkMode ? "#909191" : "#aeb1b5"
    readonly property color selectionFill: backend.themeSelection
    // The desktop's text size knob (GNOME's text-scaling-factor, which
    // `omarchy display text size` drives) anchored so its 12px default leaves
    // the app at the sizes it was designed around.
    readonly property real textScale: backend.textScale
    readonly property int editorFontPixelSize: scaledSize(backend.editorFontSize)
    readonly property real headingCellWidth: writerFontMetrics.advanceWidth("#")
    readonly property real headingGutterWidth: headingCellWidth * backend.headingGutterCells
    // Never wider than the Flickable's viewport, whatever the floor asks for:
    // a tiling compositor can resize the window below its minimum width.
    readonly property int editorWidth: Math.min(
        Math.round(writerFontMetrics.averageCharacterWidth * 65),
        Math.max(360, width - fileSidebar.width - Math.round(writerFontMetrics.averageCharacterWidth * 20)),
        Math.max(0, width - fileSidebar.width - 48))
    property bool closeConfirmed: false
    property bool searchOpen: false
    property bool sidebarOpen: false
    property int sidebarLogicalWidth: backend.sidebarWidth()
    property bool searchUpdating: false
    property var searchMatches: []
    property int searchMatchIndex: -1
    property url pendingOpenUrl
    property string pendingAction: ""
    property bool replaceOpen: false
    property bool previewMode: false
    property int normalX: -1
    property int normalY: -1
    property int normalWidth: 1280
    property int normalHeight: 820
    property bool wasMaximized: false

    Material.theme: darkMode ? Material.Dark : Material.Light
    Material.accent: backend.themeAccent
    color: pageColor

    function setSidebarOpen(open) {
        sidebarOpen = open;
        if (open)
            fileSidebar.focusList();
        else
            editor.forceActiveFocus();
    }

    property var headingsList: []

    function openHeadingsDialog() {
        headingsList = backend.documentHeadings();
        headingsDialog.open();
    }

    function toggleSidebar() {
        setSidebarOpen(!sidebarOpen);
    }

    function togglePreview() {
        previewMode = !previewMode;
        if (previewMode)
            backend.renderPreview(preview.textDocument);
        else
            editor.forceActiveFocus();
    }

    onHeadingCellWidthChanged: backend.setHeadingCellWidth(headingCellWidth)

    onClosing: function(close) {
        if (closeConfirmed || !backend.modified)
            return;

        close.accepted = false;
        pendingAction = "close";
        if (!unsavedChangesDialog.opened)
            unsavedChangesDialog.open();
    }

    onXChanged: if (visibility === Window.Windowed) normalX = x
    onYChanged: if (visibility === Window.Windowed) normalY = y
    onWidthChanged: if (visibility === Window.Windowed) normalWidth = width
    onHeightChanged: if (visibility === Window.Windowed) normalHeight = height
    onVisibilityChanged: function() {
        if (win.visibility === Window.Maximized)
            wasMaximized = true;
        else if (win.visibility === Window.Windowed)
            wasMaximized = false;
    }

    function requestOpen(url) {
        if (!backend.modified) {
            backend.open(url);
            return;
        }
        pendingOpenUrl = url;
        pendingAction = "open";
        unsavedChangesDialog.open();
    }

    function completePendingAction() {
        var action = pendingAction;
        pendingAction = "";
        if (action === "close") {
            closeConfirmed = true;
            close();
        } else if (action === "open") {
            backend.open(pendingOpenUrl);
        }
    }

    FontMetrics {
        id: writerFontMetrics
        font.family: backend.fontFamily
        font.pixelSize: win.editorFontPixelSize
    }

    // Every hardcoded size in the interface is expressed at text scale 1.
    function scaledSize(pixels) {
        return Math.max(1, Math.round(pixels * win.textScale));
    }

    function toggleFullScreen() {
        win.visibility = win.visibility === Window.FullScreen
            ? Window.Windowed
            : Window.FullScreen;
    }

    function updateSearch() {
        var matches = [];
        var query = searchField.text;
        if (query.length > 0) {
            var haystack = editor.getText(0, editor.length).toLocaleLowerCase();
            var needle = query.toLocaleLowerCase();
            var position = 0;
            while ((position = haystack.indexOf(needle, position)) !== -1) {
                matches.push(position);
                position += Math.max(1, needle.length);
            }
        }
        searchMatches = matches;
        searchMatchIndex = matches.length > 0 ? 0 : -1;
        showSearchMatch();
    }

    function showSearchMatch() {
        var start = searchMatchIndex >= 0 ? searchMatches[searchMatchIndex] : -1;
        searchUpdating = true;
        backend.setSearchHighlight(searchField.text, start);
        if (start >= 0) {
            editor.select(start, start + searchField.text.length);
            editorFlick.ensureCursorVisible();
        }
        searchUpdating = false;
    }

    function moveSearch(direction) {
        if (searchMatches.length === 0)
            return;
        searchMatchIndex = (searchMatchIndex + direction + searchMatches.length)
                           % searchMatches.length;
        showSearchMatch();
    }

    function closeSearch() {
        searchOpen = false;
        searchUpdating = true;
        backend.setSearchHighlight("", -1);
        editor.deselect();
        searchUpdating = false;
        replaceOpen = false;
        editor.forceActiveFocus();
    }

    Shortcut {
        sequences: ["Ctrl++", "Ctrl+="]
        context: Qt.ApplicationShortcut
        onActivated: {
            backend.editorFontSize += 2;
            zoomOverlay.show();
        }
    }

    Shortcut {
        sequence: "Ctrl+-"
        context: Qt.ApplicationShortcut
        onActivated: {
            backend.editorFontSize -= 2;
            zoomOverlay.show();
        }
    }

    Shortcut {
        sequence: "Ctrl+0"
        context: Qt.ApplicationShortcut
        onActivated: {
            backend.resetEditorFontSize();
            zoomOverlay.show();
        }
    }

    Shortcut {
        sequence: "Ctrl+Q"
        context: Qt.ApplicationShortcut
        onActivated: win.close()
    }

    Shortcut {
        sequence: "Ctrl+W"
        context: Qt.ApplicationShortcut
        onActivated: win.close()
    }

    Shortcut {
        sequence: "Ctrl+S"
        context: Qt.ApplicationShortcut
        onActivated: backend.save()
    }

    Shortcut {
        sequence: "Ctrl+H"
        context: Qt.ApplicationShortcut
        onActivated: {
            previewMode = false;
            searchOpen = true;
            replaceOpen = true;
            searchField.forceActiveFocus();
            searchField.selectAll();
        }
    }

    Shortcut {
        sequence: "Ctrl+B"
        context: Qt.WindowShortcut
        onActivated: editor.wrapSelection("**", "**")
    }

    Shortcut {
        sequence: "Ctrl+I"
        context: Qt.WindowShortcut
        onActivated: editor.wrapSelection("*", "*")
    }

    Shortcut {
        sequence: "Ctrl+Shift+X"
        context: Qt.WindowShortcut
        onActivated: editor.wrapSelection("~~", "~~")
    }

    Shortcut {
        sequence: "Ctrl+K"
        context: Qt.WindowShortcut
        onActivated: editor.insertLink()
    }

    Shortcut {
        sequence: "Ctrl+Shift+C"
        context: Qt.WindowShortcut
        onActivated: editor.toggleTaskItemAtCurrentLine()
    }

    Shortcut {
        sequence: "Ctrl+?"
        context: Qt.ApplicationShortcut
        onActivated: shortcutsDialog.open()
    }

    Shortcut {
        sequence: "Ctrl+/"
        context: Qt.ApplicationShortcut
        onActivated: win.togglePreview()
    }

    Shortcut {
        sequence: "Ctrl+E"
        context: Qt.ApplicationShortcut
        onActivated: win.toggleSidebar()
    }

    Shortcut {
        sequence: "Ctrl+Shift+O"
        context: Qt.ApplicationShortcut
        onActivated: win.openHeadingsDialog()
    }

    Shortcut {
        sequence: "Ctrl+Shift+T"
        context: Qt.ApplicationShortcut
        onActivated: {
            backend.updateCursorPosition(editor.cursorPosition);
            backend.toggleFocusMode();
        }
    }

    Shortcut {
        sequence: "Ctrl+O"
        context: Qt.ApplicationShortcut
        onActivated: backend.openDialog()
    }

    Shortcut {
        sequence: "Ctrl+N"
        context: Qt.ApplicationShortcut
        onActivated: backend.newWindow()
    }

    Shortcut {
        sequence: "Ctrl+Shift+S"
        context: Qt.ApplicationShortcut
        onActivated: backend.saveAsDialog()
    }

    Shortcut {
        sequence: "Ctrl+P"
        context: Qt.ApplicationShortcut
        onActivated: backend.printDocument()
    }

    Shortcut {
        sequences: ["Meta+F", "F11"]
        context: Qt.ApplicationShortcut
        onActivated: toggleFullScreen()
    }

    Shortcut {
        sequence: "Ctrl+Z"
        context: Qt.WindowShortcut
        onActivated: editor.undo()
    }

    Shortcut {
        sequences: ["Ctrl+Shift+Z", "Ctrl+Y"]
        context: Qt.WindowShortcut
        onActivated: editor.redo()
    }

    Shortcut {
        sequence: "Ctrl+F"
        context: Qt.ApplicationShortcut
        onActivated: {
            previewMode = false;
            searchOpen = true;
            searchField.forceActiveFocus();
            searchField.selectAll();
        }
    }

    Shortcut {
        sequence: "Ctrl+G"
        context: Qt.ApplicationShortcut
        enabled: win.searchOpen
        onActivated: win.moveSearch(1)
    }

    Connections {
        target: backend

        function onOpenDialogRequested() {
            openFileDialog.open();
        }

        function onSaveDialogRequested(suggestedUrl) {
            saveFileDialog.selectedFile = suggestedUrl;
            saveFileDialog.open();
        }

        function onSaveSucceeded() {
            if (win.pendingAction !== "")
                win.completePendingAction();
        }

        function onExternalChangeDetected(deleted, locallyModified) {
            externalChangeDialog.deleted = deleted;
            externalChangeDialog.locallyModified = locallyModified;
            externalChangeDialog.open();
        }

        function onFileUrlChanged() {
            editor.cursorPosition = 0;
            editor.deselect();
            editorFlick.scrollTo(0);
        }

        function onFocusModeChanged() {
            Qt.callLater(editorFlick.ensureCursorVisible);
        }
    }

    Dialogs.FileDialog {
        id: openFileDialog
        objectName: "openFileDialog"
        title: "Open File"
        fileMode: Dialogs.FileDialog.OpenFile
        nameFilters: ["Markdown files (*.md *.markdown)", "All files (*)"]
        onAccepted: win.requestOpen(selectedFile)
    }

    Dialogs.FileDialog {
        id: saveFileDialog
        objectName: "saveFileDialog"
        title: "Save File"
        fileMode: Dialogs.FileDialog.SaveFile
        nameFilters: ["Markdown files (*.md *.markdown)", "All files (*)"]
        onAccepted: backend.saveAs(selectedFile)
        onRejected: {
            win.pendingAction = "";
        }
    }

    UnsavedChangesDialog {
        id: unsavedChangesDialog
        fileName: backend.fileName
        darkMode: win.darkMode
        textScale: win.textScale
        textColor: win.textColor
        strongTextColor: win.strongTextColor
        activeButtonColor: backend.themeAccent
        containerWidth: win.width
        containerHeight: win.height

        onDiscardRequested: {
            backend.discardRecovery();
            win.completePendingAction();
        }

        onSaveRequested: {
            backend.save();
        }
        onCancelRequested: win.pendingAction = ""
    }

    ExternalChangeDialog {
        id: externalChangeDialog
        darkMode: win.darkMode
        textScale: win.textScale
        textColor: win.textColor
        strongTextColor: win.strongTextColor
        activeButtonColor: backend.themeAccent
        containerWidth: win.width
        containerHeight: win.height

        onKeepRequested: backend.keepExternalVersion()
        onReloadRequested: backend.reloadFromDisk()
    }

    Dialog {
        id: shortcutsDialog
        title: "Keyboard Shortcuts"
        standardButtons: Dialog.Close
        width: Math.min(win.scaledSize(380), win.width - 48)
        x: Math.round((win.width - width) / 2)
        y: Math.round((win.height - height) / 2)
        contentItem: Label {
            text: "Ctrl+Z  Undo\nCtrl+Shift+Z / Ctrl+Y  Redo\nCtrl+S  Save\nCtrl+Shift+S  Save As\nCtrl+O  Open\nCtrl+E  Files\nCtrl+Shift+O  Headings\nCtrl+/  Preview\nCtrl+N  New Window\nCtrl+Q  Quit\nCtrl+F  Find\nCtrl+H  Find and Replace\nCtrl+B  Bold\nCtrl+I  Italic\nCtrl+Shift+X  Strikethrough\nCtrl+Shift+C  Toggle Task Checkbox\nCtrl+K  Link\nCtrl+P  Print\nCtrl+Shift+T  Focus Mode\nCtrl++ / Ctrl+-  Zoom\nCtrl+0  Reset Zoom\nF11 / Super+F  Fullscreen\nCtrl+?  Shortcuts"
            lineHeight: 1.5
        }
    }

    Dialog {
        id: headingsDialog
        title: "Document Headings"
        standardButtons: Dialog.Close
        width: Math.min(win.scaledSize(460), win.width - 48)
        height: Math.min(win.scaledSize(420), win.height - 80)
        x: Math.round((win.width - width) / 2)
        y: Math.round((win.height - height) / 2)

        contentItem: Column {
            spacing: 8

            Label {
                text: win.headingsList.length === 0 ? "No headings found in document." : ""
                visible: win.headingsList.length === 0
                color: win.mutedColor
                font.pixelSize: win.scaledSize(14)
                anchors.horizontalCenter: parent.horizontalCenter
            }

            ListView {
                id: headingsListView
                width: parent.width
                height: parent.height
                clip: true
                visible: win.headingsList.length > 0
                model: win.headingsList
                focus: true

                delegate: ItemDelegate {
                    width: headingsListView.width
                    height: win.scaledSize(34)
                    highlighted: ListView.isCurrentItem

                    contentItem: Row {
                        anchors.fill: parent
                        anchors.leftMargin: Math.max(8, (modelData.level - 1) * win.scaledSize(18) + 8)
                        anchors.rightMargin: 8
                        spacing: 8

                        Label {
                            text: "H" + modelData.level
                            color: backend.themeAccent
                            font.family: "iA Writer Mono S"
                            font.pixelSize: win.scaledSize(11)
                            font.bold: true
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        Label {
                            text: modelData.title
                            color: highlighted ? backend.themeForeground : win.textColor
                            font.family: backend.fontFamily
                            font.pixelSize: win.scaledSize(13)
                            font.bold: modelData.level <= 2
                            elide: Text.ElideRight
                            width: parent.width - win.scaledSize(40)
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }

                    onClicked: {
                        headingsDialog.close();
                        editor.cursorPosition = modelData.position;
                        editor.forceActiveFocus();
                        Qt.callLater(function() {
                            editorFlick.scrollToHeadingPosition(modelData.position);
                        });
                    }
                }

                Keys.onReturnPressed: {
                    if (currentIndex >= 0 && currentIndex < win.headingsList.length) {
                        var item = win.headingsList[currentIndex];
                        headingsDialog.close();
                        editor.cursorPosition = item.position;
                        editor.forceActiveFocus();
                        Qt.callLater(function() {
                            editorFlick.scrollToHeadingPosition(item.position);
                        });
                    }
                }
                Keys.onEscapePressed: headingsDialog.close()
            }
        }

        onOpened: {
            if (win.headingsList.length > 0) {
                headingsListView.currentIndex = 0;
                headingsListView.forceActiveFocus();
            }
        }
    }

    Rectangle {
        id: zoomOverlay
        anchors.centerIn: parent
        width: zoomLabel.width + 40
        height: zoomLabel.height + 20
        color: win.pageColor
        border.color: win.mutedColor
        border.width: 1
        radius: 8
        opacity: 0
        z: 100
        Behavior on opacity { NumberAnimation { duration: 200 } }

        Label {
            id: zoomLabel
            anchors.centerIn: parent
            text: Math.round(backend.editorFontSize / 20 * 100) + "%"
            color: win.textColor
            font.family: backend.fontFamily
            font.pixelSize: 24
            font.weight: Font.Bold
        }

        Timer {
            id: zoomTimer
            interval: 1500
            onTriggered: zoomOverlay.opacity = 0
        }

        function show() {
            opacity = 0.9;
            zoomTimer.restart();
        }
    }

    FileSidebar {
        id: fileSidebar
        objectName: "fileSidebar"
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        expanded: win.sidebarOpen
        logicalWidth: win.sidebarLogicalWidth
        darkMode: win.darkMode
        textScale: win.textScale
        pageColor: win.pageColor
        textColor: win.textColor
        mutedColor: win.mutedColor
        accentColor: backend.themeAccent
        selectionFill: win.selectionFill
        folderUrl: backend.folderUrl
        folderName: backend.folderName
        folderHasParent: backend.folderHasParent
        entries: backend.folderEntries
        currentFileUrl: backend.fileUrl

        onParentFolderRequested: backend.openParentFolder()
        onFolderRequested: function(folderUrl) { backend.setFolder(folderUrl); }
        onFileRequested: function(fileUrl) { win.requestOpen(fileUrl); }
        onCreateDocumentRequested: function(name) {
            var docUrl = backend.createDocument(name);
            if (docUrl.toString().length > 0) {
                fileSidebar.selectUrl(docUrl);
                win.requestOpen(docUrl);
            }
        }
        onCreateFolderRequested: function(name) {
            var folderUrl = backend.createFolder(name);
            if (folderUrl.toString().length > 0)
                fileSidebar.selectUrl(folderUrl);
        }
        onWidthChangeRequested: function(width) { win.sidebarLogicalWidth = width; }
        onWidthCommitted: backend.saveSidebarWidth(win.sidebarLogicalWidth)
        onDismissed: editor.forceActiveFocus()
    }

    Item {
        anchors.fill: parent
        anchors.leftMargin: fileSidebar.width

        Flickable {
            id: editorFlick
            objectName: "editorViewport"
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: footer.top
            anchors.leftMargin: 24
            anchors.rightMargin: 24
            clip: true
            property bool settlingDocument: true
            contentWidth: width
            contentHeight: Math.max(height, (win.previewMode ? preview : editor).y
                                    + (win.previewMode ? preview : editor).implicitHeight + 220)
                + (backend.focusMode && !win.previewMode ? Math.round(height / 2) : 0)
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {
                policy: ScrollBar.AsNeeded
                // Wheel scrolling moves contentY directly rather than
                // flicking the Flickable, so the bar has to be told about
                // that activity; linger briefly after the last event.
                active: hovered || pressed || wheelScroll.running || scrollLinger.running
            }

            Timer {
                id: scrollLinger
                interval: 600
            }

            // Flickable turns a wheel notch into a flick sized by the small
            // application font, which crawls next to a browser. Reproduce
            // Chromium's wheel physics instead (cc::ScrollOffsetAnimationCurve):
            // each notch moves 3 lines of 40px towards a running target, the
            // animation gets shorter as the outstanding distance grows, and a
            // notch landing mid-animation carries the current velocity into
            // the new curve, so sustained spinning keeps picking up speed.
            readonly property real wheelStep: win.scaledSize(120)

            FrameAnimation {
                id: wheelScroll
                running: false

                property real startY: 0
                property real targetY: 0
                property real duration: 0.2
                // Cubic bezier easing; ease-in-out (0.42, 0, 0.58, 1) for a
                // fresh scroll, with y1 tilted on retarget so the curve's
                // initial slope matches the velocity it inherits.
                property real cx1: 0.42
                property real cy1: 0
                readonly property real cx2: 0.58
                readonly property real cy2: 1

                onTriggered: {
                    var x = elapsedTime / duration;
                    if (x >= 1) {
                        editorFlick.contentY = editorFlick.snapToPixel(targetY);
                        stop();
                        return;
                    }
                    editorFlick.contentY = editorFlick.snapToPixel(
                        startY + (targetY - startY) * curveY(solveCurve(x)));
                }

                function begin(from, to, dur, slope) {
                    startY = from;
                    targetY = to;
                    duration = dur;
                    cx1 = 0.42;
                    cy1 = 0.42 * Math.max(-1000, Math.min(1000, slope));
                    restart();
                }

                function retarget(newTarget) {
                    var s = solveCurve(Math.min(1, elapsedTime / duration));
                    var pos = startY + (targetY - startY) * curveY(s);
                    var delta = newTarget - pos;
                    if (Math.abs(delta) < 0.5) {
                        editorFlick.contentY = newTarget;
                        stop();
                        return;
                    }

                    var velocity = curveDY(s) / Math.max(1e-6, curveDX(s))
                        * (targetY - startY) / duration;
                    var dur = editorFlick.wheelDuration(delta);
                    // When already moving faster than the eased curve would,
                    // bound the duration by the time to target at the current
                    // velocity; the 2.5x covers the ease-out tail.
                    if (velocity !== 0 && delta / velocity > 0)
                        dur = Math.min(dur, delta / velocity * 2.5);
                    begin(pos, newTarget, dur, velocity * dur / delta);
                }

                // Cubic bezier through (0,0), (cx1,cy1), (cx2,cy2), (1,1),
                // evaluated by Newton-solving the curve parameter from x.
                function curveX(s) { return 3 * s * (1 - s) * ((1 - s) * cx1 + s * cx2) + s * s * s; }
                function curveY(s) { return 3 * s * (1 - s) * ((1 - s) * cy1 + s * cy2) + s * s * s; }
                function curveDX(s) { return 3 * (1 - s) * (1 - s) * cx1 + 6 * (1 - s) * s * (cx2 - cx1) + 3 * s * s * (1 - cx2); }
                function curveDY(s) { return 3 * (1 - s) * (1 - s) * cy1 + 6 * (1 - s) * s * (cy2 - cy1) + 3 * s * s * (1 - cy2); }

                function solveCurve(x) {
                    var s = x;
                    for (var i = 0; i < 8; ++i) {
                        var error = curveX(s) - x;
                        if (Math.abs(error) < 0.001)
                            break;
                        var d = curveDX(s);
                        if (Math.abs(d) < 1e-6)
                            break;
                        s = Math.max(0, Math.min(1, s - error / d));
                    }
                    return s;
                }
            }

            WheelHandler {
                // Wayland compositors route every pointer's scroll through
                // one seat device that Qt classifies as a touchpad, so the
                // device type cannot tell a mouse wheel from two-finger
                // scrolling. Distinguish by event shape instead: discrete
                // wheel notches arrive with only angleDelta set, while
                // finger scrolling carries pixel-precise pixelDelta.
                acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                onWheel: function(wheel) {
                    scrollLinger.restart();
                    if (wheel.pixelDelta.y !== 0)
                        editorFlick.scrollTo(editorFlick.clampContentY(editorFlick.contentY - wheel.pixelDelta.y));
                    else
                        editorFlick.scrollByWheel(wheel);
                    wheel.accepted = true;
                }
            }

            onMovementStarted: wheelScroll.stop()
            onContentYChanged: editor.refreshLinkHover()

            function scrollByWheel(wheel) {
                // High-resolution wheels report fractional notches; feed
                // those through the same animated path, like Chromium does
                // for every wheel-source event.
                var notches = wheel.angleDelta.y / 120;
                if (notches === 0)
                    return;

                if (wheelScroll.running) {
                    wheelScroll.retarget(clampContentY(wheelScroll.targetY - notches * wheelStep));
                    return;
                }

                var target = clampContentY(contentY - notches * wheelStep);
                if (target !== contentY)
                    wheelScroll.begin(contentY, target, wheelDuration(target - contentY), 0);
            }

            // Chromium's inverse-delta duration: 200ms for a single notch,
            // ramping down to 100ms once 480px are outstanding.
            function wheelDuration(delta) {
                var pixels = Math.abs(delta) / win.textScale;
                return Math.max(6, Math.min(12, 14 - pixels / 60)) / 60;
            }

            function clampContentY(y) {
                return Math.max(0, Math.min(Math.max(0, contentHeight - height), y));
            }

            // Whole device pixels keep natively hinted glyphs from
            // re-rasterizing mid-animation, which reads as shimmer.
            function snapToPixel(y) {
                return Math.round(y * Screen.devicePixelRatio) / Screen.devicePixelRatio;
            }

            // Jump to a position, abandoning any wheel animation still running.
            function scrollTo(y) {
                wheelScroll.stop();
                contentY = snapToPixel(y);
                if (!settlingDocument)
                    scrollLinger.restart();
            }

            function scrollToHeadingPosition(pos) {
                var cursorRect = editor.positionToRectangle(pos);
                var cursorTop = editor.y + cursorRect.y;
                var targetY = Math.max(0, cursorTop - win.editorFontPixelSize * 2);
                scrollTo(clampContentY(targetY));
            }

            // Keep the editing caret within the viewport so writing past the
            // bottom edge scrolls the page along with the text.
            function ensureCursorVisible() {
                if (backend.focusMode) {
                    var cursorCenter = editor.y + editor.cursorRectangle.y
                        + editor.cursorRectangle.height / 2;
                    scrollTo(clampContentY(cursorCenter - height / 2));
                    return;
                }
                var margin = win.editorFontPixelSize * 2;
                var cursorTop = editor.y + editor.cursorRectangle.y;
                var cursorBottom = cursorTop + editor.cursorRectangle.height;
                var maxContentY = Math.max(0, contentHeight - height);

                if (cursorBottom + margin > contentY + height)
                    scrollTo(Math.min(maxContentY, cursorBottom + margin - height));
                else if (cursorTop - margin < contentY)
                    scrollTo(Math.max(0, cursorTop - margin));
            }

            TextEdit {
                id: editor
                objectName: "sourceEditor"
                readonly property real bodyWidth: win.editorWidth
                x: Math.round((editorFlick.width - bodyWidth) / 2 - win.headingGutterWidth)
                y: backend.focusMode
                    ? Math.round(editorFlick.height / 2)
                    : Math.max(42, Math.round(win.height * 0.05))
                width: bodyWidth + win.headingGutterWidth
                height: Math.max(editorFlick.height - y - 96, implicitHeight + 20)
                text: ""
                visible: !win.previewMode
                textFormat: TextEdit.RichText
                wrapMode: TextEdit.Wrap
                tabStopDistance: writerFontMetrics.advanceWidth("    ")
                selectByMouse: true
                persistentSelection: true
                activeFocusOnPress: true
                color: win.textColor
                selectedTextColor: win.strongTextColor
                selectionColor: win.selectionFill
                font.family: backend.fontFamily
                font.pixelSize: win.editorFontPixelSize
                font.weight: Font.Normal
                // Native rendering hints glyphs to the pixel grid, which is
                // crispest at whole scale factors but misplaces and unevenly
                // rasterizes glyphs at fractional ones (and goes stale when
                // the compositor delivers the fractional scale after the
                // first frame). Fall back to Qt's scalable renderer there.
                renderType: Screen.devicePixelRatio % 1 === 0 ? TextEdit.NativeRendering : TextEdit.QtRendering
                cursorDelegate: Rectangle {
                    width: 1
                    color: win.strongTextColor
                }
                onCursorRectangleChanged: editorFlick.ensureCursorVisible()
                onYChanged: if (backend.focusMode) editorFlick.ensureCursorVisible()
                onCursorPositionChanged: {
                    if (backend.focusMode)
                        backend.updateCursorPosition(cursorPosition);
                }

                function replaceSelectionWith(replacement) {
                    var start = Math.min(selectionStart, selectionEnd);
                    var end = Math.max(selectionStart, selectionEnd);
                    EditorMutations.replaceRange(editor, start, end, replacement);
                }

                function wrapSelection(before, after) {
                    forceActiveFocus();
                    EditorMutations.toggleWrap(editor, before, after);
                }

                function insertLink() {
                    var start = Math.min(selectionStart, selectionEnd);
                    var end = Math.max(selectionStart, selectionEnd);
                    var selected = getText(start, end);
                    var url = backend.clipboardUrl();
                    var label = selected.length > 0 ? selected : "link text";
                    var destination = url.length > 0 ? url : "https://";
                    var escapedLabel = escapeMarkdownLinkText(label);
                    var markdown = "[" + escapedLabel + "](" + escapeMarkdownLinkDestination(destination) + ")";
                    if (selected.length === 0) {
                        EditorMutations.replaceRange(editor, start, end, markdown,
                                                     1, 1 + escapedLabel.length);
                    } else if (url.length === 0) {
                        EditorMutations.replaceRange(editor, start, end, markdown,
                                                     escapedLabel.length + 3,
                                                     markdown.length - 1);
                    } else {
                        EditorMutations.replaceRange(editor, start, end, markdown);
                    }
                }

                function smartReturn(softBreak) {
                    if (softBreak) {
                        replaceSelectionWith("\n");
                        return;
                    }
                    var plainText = getText(0, length).replace(/[\u2028\u2029]/g, "\n");
                    var lineStart = plainText.lastIndexOf("\n", cursorPosition - 1) + 1;
                    var line = plainText.slice(lineStart, cursorPosition);
                    var before = plainText.slice(0, cursorPosition);
                    if (insideFence(before)) {
                        replaceSelectionWith("\n");
                        return;
                    }
                    var taskMatch = line.match(/^(\s*)([-+*])\s+\[([ xX])\]\s*(.*)$/);
                    if (taskMatch) {
                        if (taskMatch[4].length === 0) {
                            EditorMutations.replaceRange(editor, lineStart,
                                                         cursorPosition, "\n");
                        } else {
                            replaceSelectionWith("\n" + taskMatch[1] + taskMatch[2] + " [ ] ");
                        }
                        return;
                    }
                    var match = line.match(/^(\s*)([-+*]|\d+[.)]|>+)\s+(.*)$/);
                    if (match) {
                        if (match[3].length === 0) {
                            EditorMutations.replaceRange(editor, lineStart,
                                                         cursorPosition, "\n");
                        } else {
                            var marker = match[2];
                            if (/^\d/.test(marker))
                                marker = (parseInt(marker) + 1) + marker.slice(-1);
                            replaceSelectionWith("\n" + match[1] + marker + " ");
                        }
                        return;
                    }
                    var lineEnd = plainText.indexOf("\n", cursorPosition);
                    var rest = lineEnd < 0 ? plainText.slice(cursorPosition)
                                           : plainText.slice(cursorPosition, lineEnd);
                    if (/^\s*$/.test(line) && /^\s*$/.test(rest)) {
                        replaceSelectionWith("\n");
                        return;
                    }
                    replaceSelectionWith("\n\n");
                }

                // Reports whether the caret follows an unmatched Markdown fence.
                function insideFence(before) {
                    var fenceChar = "";
                    var fenceLength = 0;
                    var lines = before.split("\n");
                    for (var i = 0; i < lines.length; ++i) {
                        var match = lines[i].match(/^ {0,3}(`{3,}|~{3,})(.*)$/);
                        if (!match)
                            continue;
                        var marker = match[1];
                        if (fenceLength === 0) {
                            fenceChar = marker[0];
                            fenceLength = marker.length;
                        } else if (marker[0] === fenceChar && marker.length >= fenceLength
                                   && match[2].trim().length === 0) {
                            fenceChar = "";
                            fenceLength = 0;
                        }
                    }
                    return fenceLength > 0;
                }

                function escapeMarkdownLinkText(linkText) {
                    return linkText.replace(/\\/g, "\\\\")
                                   .replace(/\[/g, "\\[")
                                   .replace(/\]/g, "\\]");
                }

                function escapeMarkdownLinkDestination(linkUrl) {
                    return linkUrl.replace(/\\/g, "\\\\")
                                  .replace(/\(/g, "\\(")
                                  .replace(/\)/g, "\\)");
                }

                function pasteClipboardUrlAsMarkdownLink() {
                    var start = Math.min(selectionStart, selectionEnd);
                    var end = Math.max(selectionStart, selectionEnd);
                    if (start === end)
                        return false;

                    var url = backend.clipboardUrl();
                    if (url === "")
                        return false;

                    var plainText = getText(0, length).replace(/[\u2028\u2029]/g, "\n");
                    var selected = plainText.slice(start, end);
                    var leading = selected.match(/^\s*/)[0];
                    var trailing = selected.match(/\s*$/)[0];
                    var linkText = selected.slice(leading.length,
                                                  selected.length - trailing.length);
                    if (linkText === "")
                        return false;

                    replaceSelectionWith(leading + "[" + escapeMarkdownLinkText(linkText) + "]("
                                         + escapeMarkdownLinkDestination(url) + ")" + trailing);
                    return true;
                }

                function pasteClipboardAsPlainText() {
                    var pastedText = backend.clipboardText();
                    if (pastedText.length > 0)
                        replaceSelectionWith(pastedText);
                }

                function skipHiddenForward(position) {
                    var pos = position;
                    var ranges = backend.hiddenRangesAt(pos);
                    for (var i = 0; i < ranges.length; i++) {
                        if (pos >= ranges[i].start && pos < ranges[i].end) {
                            pos = ranges[i].end;
                            i = -1;
                        }
                    }
                    return pos;
                }

                function skipHiddenBackward(position) {
                    var pos = position;
                    var ranges = backend.hiddenRangesAt(pos);
                    for (var i = ranges.length - 1; i >= 0; i--) {
                        if (pos > ranges[i].start && pos <= ranges[i].end) {
                            pos = ranges[i].start;
                            i = ranges.length;
                        }
                    }
                    return pos;
                }

                function moveCursorVisibly(direction) {
                    if (selectionStart !== selectionEnd) {
                        cursorPosition = direction > 0
                            ? Math.max(selectionStart, selectionEnd)
                            : Math.min(selectionStart, selectionEnd);
                        return;
                    }

                    var pos = Math.max(0, Math.min(length, cursorPosition + direction));
                    cursorPosition = direction > 0
                        ? skipHiddenForward(pos)
                        : skipHiddenBackward(pos);
                }

                function movePage(direction, extendSelection) {
                    var pageStep = Math.max(win.editorFontPixelSize,
                                            editorFlick.height - win.editorFontPixelSize * 2);
                    var rect = cursorRectangle;
                    var targetY = rect.y + rect.height / 2 + direction * pageStep;
                    var target = positionAt(rect.x, Math.max(0, targetY));
                    if (extendSelection)
                        moveCursorSelection(target, TextEdit.SelectCharacters);
                    else
                        cursorPosition = target;
                }

                function deleteParagraphBreakBehindCursor() {
                    if (selectionStart !== selectionEnd || cursorPosition < 2)
                        return false;

                    var plainText = getText(0, length).replace(/[\u2028\u2029]/g, "\n");
                    if (plainText.slice(cursorPosition - 2, cursorPosition) !== "\n\n")
                        return false;

                    var start = cursorPosition - 2;
                    remove(start, cursorPosition);
                    cursorPosition = start;
                    return true;
                }

                function toggleTaskItemAtCurrentLine() {
                    var plainText = getText(0, length).replace(/[\u2028\u2029]/g, "\n");
                    var lineStart = plainText.lastIndexOf("\n", cursorPosition - 1) + 1;
                    var lineEnd = plainText.indexOf("\n", cursorPosition);
                    if (lineEnd < 0) lineEnd = plainText.length;
                    var line = plainText.slice(lineStart, lineEnd);

                    var checkedMatch = line.match(/^(\s*[-+*]\s+)\[[xX]\]/);
                    if (checkedMatch) {
                        var target = lineStart + checkedMatch[1].length;
                        EditorMutations.replaceRange(editor, target, target + 3, "[ ]");
                        return true;
                    }

                    var uncheckedMatch = line.match(/^(\s*[-+*]\s+)\[ \]/);
                    if (uncheckedMatch) {
                        var target = lineStart + uncheckedMatch[1].length;
                        EditorMutations.replaceRange(editor, target, target + 3, "[x]");
                        return true;
                    }

                    var bulletMatch = line.match(/^(\s*[-+*]\s+)/);
                    if (bulletMatch) {
                        var target = lineStart + bulletMatch[1].length;
                        EditorMutations.replaceRange(editor, target, target, "[ ] ");
                        return true;
                    }

                    var indentMatch = line.match(/^(\s*)/);
                    var indent = indentMatch ? indentMatch[1] : "";
                    EditorMutations.replaceRange(editor, lineStart, lineStart + indent.length, indent + "- [ ] ");
                    return true;
                }

                property string hoveredLinkUrl: ""
                property string hoveredImageUrl: ""
                property real linkHoverX: 0
                property real linkHoverY: 0
                property real linkHoverRootX: 0
                property real linkHoverRootY: 0

                function updateLinkHover(x, y) {
                    var rootPoint = editor.mapToItem(editorFlick.contentItem, x, y);
                    linkHoverRootX = rootPoint.x;
                    linkHoverRootY = rootPoint.y;
                    linkHoverX = x;
                    linkHoverY = y;
                    var pos = positionAt(x, y);
                    hoveredImageUrl = backend.imageUrlAt(pos);
                    hoveredLinkUrl = hoveredImageUrl === "" ? backend.linkUrlAt(pos) : "";
                }

                function refreshLinkHover() {
                    if (!editorHoverArea.containsMouse) {
                        clearLinkHover();
                        return;
                    }
                    var point = editor.mapFromItem(editorFlick.contentItem,
                                                   linkHoverRootX, linkHoverRootY);
                    linkHoverX = point.x;
                    linkHoverY = point.y;
                    var pos = positionAt(point.x, point.y);
                    hoveredImageUrl = backend.imageUrlAt(pos);
                    hoveredLinkUrl = hoveredImageUrl === "" ? backend.linkUrlAt(pos) : "";
                }

                function clearLinkHover() {
                    hoveredLinkUrl = "";
                    hoveredImageUrl = "";
                }

                MouseArea {
                    id: editorHoverArea
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton
                    hoverEnabled: true
                    cursorShape: {
                        if (editor.hoveredLinkUrl !== "" && (mouseModifiers & Qt.ControlModifier))
                            return Qt.PointingHandCursor;
                        return Qt.IBeamCursor;
                    }
                    property int mouseModifiers: Qt.NoModifier

                    onEntered: editor.updateLinkHover(mouseX, mouseY)
                    onPositionChanged: function(mouse) {
                        mouseModifiers = mouse.modifiers;
                        editor.updateLinkHover(mouse.x, mouse.y);
                    }
                    onExited: {
                        mouseModifiers = Qt.NoModifier;
                        editor.clearLinkHover();
                    }
                    onPressed: function(mouse) {
                        if ((mouse.modifiers & Qt.ControlModifier) && editor.hoveredLinkUrl !== "") {
                            backend.openExternalUrl(editor.hoveredLinkUrl);
                            mouse.accepted = true;
                            return;
                        }
                        mouse.accepted = false;
                    }
                }

                Keys.priority: Keys.BeforeItem
                Keys.onPressed: function(event) {
                    var pasteKey = (event.key === Qt.Key_V)
                        && (event.modifiers & Qt.ControlModifier)
                        && !(event.modifiers & (Qt.AltModifier | Qt.MetaModifier | Qt.ShiftModifier));
                    var shiftInsert = (event.key === Qt.Key_Insert)
                        && (event.modifiers & Qt.ShiftModifier)
                        && !(event.modifiers & (Qt.ControlModifier | Qt.AltModifier | Qt.MetaModifier));
                    if (pasteKey || shiftInsert) {
                        if (!pasteClipboardUrlAsMarkdownLink())
                            pasteClipboardAsPlainText();
                        event.accepted = true;
                        return;
                    }

                    var returnKey = event.key === Qt.Key_Return || event.key === Qt.Key_Enter;
                    var commandModifier = event.modifiers & (Qt.ControlModifier | Qt.AltModifier | Qt.MetaModifier);
                    if (returnKey && !commandModifier) {
                        smartReturn(event.modifiers & Qt.ShiftModifier);
                        event.accepted = true;
                        return;
                    } else if (!commandModifier && event.key === Qt.Key_Backspace) {
                        if (selectionStart === selectionEnd && cursorPosition > 0 && cursorPosition < length) {
                            var beforeChar = getText(cursorPosition - 1, cursorPosition);
                            var afterChar = getText(cursorPosition, cursorPosition + 1);
                            if ((beforeChar === "(" && afterChar === ")")
                                    || (beforeChar === "[" && afterChar === "]")
                                    || (beforeChar === "{" && afterChar === "}")
                                    || (beforeChar === "\"" && afterChar === "\"")
                                    || (beforeChar === "'" && afterChar === "'")
                                    || (beforeChar === "`" && afterChar === "`")) {
                                remove(cursorPosition - 1, cursorPosition + 1);
                                cursorPosition = cursorPosition - 1;
                                event.accepted = true;
                                return;
                            }
                        }
                        if (deleteParagraphBreakBehindCursor()) {
                            event.accepted = true;
                            return;
                        }
                    } else if (!commandModifier && !(event.modifiers & Qt.ShiftModifier)
                               && event.key === Qt.Key_Right) {
                        moveCursorVisibly(1);
                        event.accepted = true;
                        return;
                    } else if (!commandModifier && !(event.modifiers & Qt.ShiftModifier)
                               && event.key === Qt.Key_Left) {
                        moveCursorVisibly(-1);
                        event.accepted = true;
                        return;
                    } else if (!commandModifier
                               && (event.key === Qt.Key_PageDown || event.key === Qt.Key_PageUp)) {
                        movePage(event.key === Qt.Key_PageDown ? 1 : -1,
                                 event.modifiers & Qt.ShiftModifier);
                        event.accepted = true;
                        return;
                    }

                    if (!commandModifier && event.text.length === 1) {
                        var ch = event.text;
                        var pairs = {
                            "(": ")",
                            "[": "]",
                            "{": "}",
                            "\"": "\"",
                            "'": "'",
                            "`": "`"
                        };
                        if (pairs[ch]) {
                            var openChar = ch;
                            var closeChar = pairs[ch];
                            if (selectionStart !== selectionEnd) {
                                wrapSelection(openChar, closeChar);
                                event.accepted = true;
                                return;
                            }

                            var preceding = cursorPosition > 0
                                ? getText(cursorPosition - 1, cursorPosition) : "";
                            var precedingIsWord = preceding !== ""
                                && (preceding.toLocaleLowerCase() !== preceding.toLocaleUpperCase()
                                    || /[0-9]/.test(preceding));
                            if (openChar === "'" && precedingIsWord)
                                return;

                            if (openChar === "`") {
                                var beforePair = cursorPosition > 1
                                    ? getText(cursorPosition - 2, cursorPosition) : "";
                                var nextChar = cursorPosition < length
                                    ? getText(cursorPosition, cursorPosition + 1) : "";
                                if (beforePair === "``" && nextChar === "`") {
                                    cursorPosition += 1;
                                    event.accepted = true;
                                    return;
                                }
                                if (preceding === "`")
                                    return;
                            }

                            if (openChar === closeChar && cursorPosition < length
                                    && getText(cursorPosition, cursorPosition + 1) === closeChar) {
                                cursorPosition = cursorPosition + 1;
                                event.accepted = true;
                                return;
                            }
                            var pos = cursorPosition;
                            insert(pos, openChar + closeChar);
                            cursorPosition = pos + 1;
                            event.accepted = true;
                            return;
                        }
                        if ((ch === ")" || ch === "]" || ch === "}") && cursorPosition < length
                                && getText(cursorPosition, cursorPosition + 1) === ch) {
                            cursorPosition = cursorPosition + 1;
                            event.accepted = true;
                            return;
                        }
                    }
                }

                onTextChanged: {
                    if (win.searchUpdating)
                        return;
                    var contentChanged = backend.editorTextChanged();
                    if (win.previewMode && contentChanged)
                        backend.renderPreview(preview.textDocument);
                    if (win.searchOpen && contentChanged)
                        win.updateSearch();
                }

                Text {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    text: "# Start writing"
                    visible: editor.length === 0 && !editor.activeFocus
                    color: win.mutedColor
                    font.family: editor.font.family
                    font.pixelSize: editor.font.pixelSize
                    font.weight: editor.font.weight
                }

                Component.onCompleted: {
                    backend.setHeadingCellWidth(win.headingCellWidth);
                    backend.attachDocument(textDocument);
                    editorFlick.settlingDocument = false;
                    forceActiveFocus();
                }
            }

            TextEdit {
                id: preview
                objectName: "previewView"
                x: Math.round((editorFlick.width - width) / 2)
                y: editor.y
                width: editor.bodyWidth
                height: Math.max(editorFlick.height - y - 96, implicitHeight + 20)
                visible: win.previewMode
                readOnly: true
                selectByMouse: true
                textFormat: TextEdit.RichText
                wrapMode: TextEdit.Wrap
                color: win.textColor
                font.family: backend.fontFamily
                font.pixelSize: win.editorFontPixelSize
                renderType: editor.renderType
                onLinkActivated: function(link) { backend.openExternalUrl(link); }
            }
        }

        Rectangle {
            id: linkTooltip
            visible: editor.hoveredLinkUrl !== ""
            x: {
                var hoverPosition = editor.positionToRectangle(editor.positionAt(editor.linkHoverX, editor.linkHoverY));
                var mappedPosition = editor.mapToItem(editorFlick.contentItem, hoverPosition.x, hoverPosition.y);
                return Math.max(0, Math.min(mappedPosition.x, editorFlick.contentItem.width - width));
            }
            y: {
                var hoverPosition = editor.positionToRectangle(editor.positionAt(editor.linkHoverX, editor.linkHoverY));
                var mappedPosition = editor.mapToItem(editorFlick.contentItem, hoverPosition.x, hoverPosition.y);
                return Math.max(0, mappedPosition.y - height - 4);
            }
            width: Math.min(linkTooltipText.implicitWidth + 16, editorFlick.contentItem.width - 16)
            height: linkTooltipText.implicitHeight + 16
            radius: 3
            color: backend.themeBackground
            border.color: backend.themeAccent
            border.width: 1
            z: 100
            Accessible.role: Accessible.Link
            Accessible.name: editor.hoveredLinkUrl
            activeFocusOnTab: visible

            Keys.onReturnPressed: backend.openExternalUrl(editor.hoveredLinkUrl)
            Keys.onEnterPressed: backend.openExternalUrl(editor.hoveredLinkUrl)

            Text {
                id: linkTooltipText
                anchors.fill: parent
                anchors.margins: 8
                text: editor.hoveredLinkUrl
                color: backend.themeForeground
                font.family: "iA Writer Mono S"
                font.pixelSize: win.scaledSize(12)
                elide: Text.ElideMiddle
                verticalAlignment: Text.AlignVCenter
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: backend.openExternalUrl(editor.hoveredLinkUrl)
            }
        }

        Rectangle {
            id: imageTooltip
            readonly property int previewStatus: previewLoader.item
                ? previewLoader.item.status : Image.Null
            readonly property real previewImplicitWidth: previewLoader.item
                ? previewLoader.item.implicitWidth : 0
            readonly property real previewImplicitHeight: previewLoader.item
                ? previewLoader.item.implicitHeight : 0
            visible: editor.hoveredImageUrl !== ""
            x: {
                var hoverPosition = editor.positionToRectangle(editor.positionAt(editor.linkHoverX, editor.linkHoverY));
                var mappedPosition = editor.mapToItem(editorFlick.contentItem, hoverPosition.x, hoverPosition.y);
                return Math.max(0, Math.min(mappedPosition.x, editorFlick.contentItem.width - width));
            }
            y: {
                var hoverPosition = editor.positionToRectangle(editor.positionAt(editor.linkHoverX, editor.linkHoverY));
                var mappedPosition = editor.mapToItem(editorFlick.contentItem, hoverPosition.x, hoverPosition.y);
                var targetY = mappedPosition.y - height - 8;
                return targetY >= 0 ? targetY : mappedPosition.y + hoverPosition.height + 8;
            }
            width: Math.min(win.scaledSize(420), editorFlick.contentItem.width - 24)
            height: previewColumn.implicitHeight + 16
            radius: 6
            color: backend.themeBackground
            border.color: backend.themeAccent
            border.width: 1
            z: 100

            Column {
                id: previewColumn
                anchors.fill: parent
                anchors.margins: 8
                spacing: 6

                Item {
                    id: imageContainer
                    width: parent.width
                    height: imageTooltip.previewStatus === Image.Ready
                        ? Math.min(win.scaledSize(260), Math.max(win.scaledSize(80), Math.round(imageTooltip.previewImplicitHeight * width / Math.max(1, imageTooltip.previewImplicitWidth))))
                        : 0
                    visible: imageTooltip.previewStatus === Image.Ready
                    clip: true

                    Loader {
                        id: previewLoader
                        anchors.fill: parent
                        active: editor.hoveredImageUrl !== ""
                        sourceComponent: AnimatedImage {
                            anchors.fill: parent
                            fillMode: Image.PreserveAspectFit
                            source: backend.resolveImageUrl(editor.hoveredImageUrl)
                            asynchronous: true
                            smooth: true
                            mipmap: true
                            playing: true
                        }
                    }

                    Rectangle {
                        anchors.fill: parent
                        color: "transparent"
                        border.color: win.mutedColor
                        border.width: 1
                        opacity: 0.25
                    }
                }

                Label {
                    width: parent.width
                    text: imageTooltip.previewStatus === Image.Error ? "Image not found / failed to load" : (imageTooltip.previewStatus === Image.Loading ? "Loading image..." : editor.hoveredImageUrl)
                    color: imageTooltip.previewStatus === Image.Error ? "#e06c75" : win.mutedColor
                    font.family: "iA Writer Mono S"
                    font.pixelSize: win.scaledSize(11)
                    elide: Text.ElideMiddle
                    horizontalAlignment: Text.AlignHCenter
                }
            }
        }

        MouseArea {
            id: footerHoverArea
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: win.scaledSize(48)
            hoverEnabled: true
            acceptedButtons: Qt.NoButton
            visible: backend.focusMode
            z: 5
        }

        Rectangle {
            id: footer
            objectName: "footer"
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: Math.max(36, win.scaledSize(36))
            color: win.pageColor
        }

        Row {
            id: footerStatus
            anchors.left: parent.left
            anchors.bottom: parent.bottom
            anchors.leftMargin: 12
            anchors.bottomMargin: 10
            spacing: 12
            opacity: backend.focusMode
                ? (footerHoverArea.containsMouse ? 0.55 : 0)
                : 0.55
            Behavior on opacity { NumberAnimation { duration: 250 } }

            FooterIconButton {
                objectName: "saveButton"
                iconName: "save"
                iconColor: win.mutedColor
                tooltip: "Save"
                onClicked: backend.save()
            }

            FooterIconButton {
                objectName: "openButton"
                iconName: "open"
                iconColor: win.mutedColor
                tooltip: "Open"
                onClicked: backend.openDialog()
            }

            FooterIconButton {
                objectName: "filesButton"
                iconName: "files"
                iconColor: win.mutedColor
                tooltip: "Files"
                onClicked: win.toggleSidebar()
            }

            FooterIconButton {
                objectName: "previewButton"
                iconName: "preview"
                iconColor: win.previewMode ? backend.themeAccent : win.mutedColor
                tooltip: win.previewMode ? "Edit (Ctrl+/)" : "Preview (Ctrl+/)"
                onClicked: win.togglePreview()
            }

            Label {
                text: backend.status
                color: win.mutedColor
                font.family: backend.fontFamily
                font.pixelSize: win.scaledSize(11)
                visible: text !== ""
                elide: Text.ElideRight
                width: Math.min(360, win.width / 3)
                height: win.scaledSize(16)
                verticalAlignment: Text.AlignVCenter
            }
        }

        Item {
            id: wordCountContainer
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.rightMargin: 12
            anchors.bottomMargin: 8
            width: wordCountLabel.implicitWidth + 8
            height: wordCountLabel.implicitHeight + 8
            opacity: backend.focusMode
                ? (footerHoverArea.containsMouse ? 0.75 : 0)
                : 0.75
            Behavior on opacity { NumberAnimation { duration: 250 } }

            Label {
                id: wordCountLabel
                anchors.centerIn: parent
                text: backend.wordCount + (backend.wordCount === 1 ? " Word" : " Words")
                color: win.mutedColor
                font.family: backend.fontFamily
                font.pixelSize: win.scaledSize(11)
            }

            MouseArea {
                id: statsHoverArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
            }
        }

        Rectangle {
            id: statsTooltip
            visible: statsHoverArea.containsMouse
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.rightMargin: 12
            anchors.bottomMargin: win.scaledSize(36)
            width: statsColumn.implicitWidth + 24
            height: statsColumn.implicitHeight + 20
            radius: 6
            color: backend.themeBackground
            border.color: backend.themeAccent
            border.width: 1
            z: 100

            Column {
                id: statsColumn
                anchors.centerIn: parent
                spacing: 4

                Label {
                    text: backend.wordCount + (backend.wordCount === 1 ? " word" : " words") + "  •  " + backend.characterCount + " characters"
                    color: backend.themeForeground
                    font.family: "iA Writer Mono S"
                    font.pixelSize: win.scaledSize(12)
                }

                Label {
                    text: backend.paragraphCount + (backend.paragraphCount === 1 ? " paragraph" : " paragraphs") + "  •  " + backend.readingTimeMinutes + " min read"
                    color: win.mutedColor
                    font.family: "iA Writer Mono S"
                    font.pixelSize: win.scaledSize(11)
                }
            }
        }


        Pane {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.topMargin: 12
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            height: win.scaledSize(win.replaceOpen ? 104 : 56)
            visible: win.searchOpen
            z: 10
            leftPadding: 16
            rightPadding: 8
            topPadding: 0
            bottomPadding: 0
            Material.elevation: 8

            background: Rectangle {
                radius: 9
                color: win.darkMode ? "#22221f" : "#fffef2"
            }

            RowLayout {
                anchors.fill: parent
                spacing: 8

                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    TextInput {
                        id: searchField
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        height: win.replaceOpen ? parent.height / 2 : parent.height
                        verticalAlignment: TextInput.AlignVCenter
                        selectByMouse: true
                        color: win.textColor
                        selectionColor: win.selectionFill
                        selectedTextColor: win.strongTextColor
                        font.pixelSize: win.scaledSize(17)
                        clip: true
                        onTextChanged: win.updateSearch()
                        Keys.onReturnPressed: function(event) {
                            win.moveSearch((event.modifiers & Qt.ShiftModifier) ? -1 : 1);
                            event.accepted = true;
                        }
                        Keys.onEscapePressed: function(event) {
                            win.closeSearch();
                            event.accepted = true;
                        }
                    }

                    TextInput {
                        id: replaceField
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        height: parent.height / 2
                        visible: win.replaceOpen
                        verticalAlignment: TextInput.AlignVCenter
                        color: win.textColor
                        selectionColor: win.selectionFill
                        selectedTextColor: win.strongTextColor
                        font.pixelSize: win.scaledSize(17)
                        Keys.onReturnPressed: replaceCurrentButton.clicked()
                    }

                    Label {
                        anchors.verticalCenter: replaceField.verticalCenter
                        text: "Replace with"
                        visible: win.replaceOpen && replaceField.text.length === 0
                        color: win.mutedColor
                        font.pixelSize: win.scaledSize(17)
                    }

                    Label {
                        anchors.verticalCenter: searchField.verticalCenter
                        text: "Find"
                        visible: searchField.text.length === 0
                        color: win.mutedColor
                        font.pixelSize: win.scaledSize(17)
                    }
                }

                Label {
                    Layout.preferredWidth: win.scaledSize(58)
                    Layout.fillHeight: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    text: win.searchMatches.length === 0
                        ? "0/0"
                        : (win.searchMatchIndex + 1) + "/" + win.searchMatches.length
                    color: win.darkMode ? win.textColor : "#62635f"
                    font.pixelSize: win.scaledSize(16)
                }

                Button {
                    id: replaceCurrentButton
                    visible: win.replaceOpen
                    text: "Replace"
                    onClicked: {
                        if (win.searchMatchIndex < 0) return;
                        var start = win.searchMatches[win.searchMatchIndex];
                        EditorMutations.replaceRange(editor, start,
                                                     start + searchField.text.length,
                                                     replaceField.text);
                        win.updateSearch();
                    }
                }

                Button {
                    visible: win.replaceOpen
                    text: "All"
                    onClicked: {
                        if (searchField.text.length === 0) return;
                        win.searchUpdating = true;
                        for (var i = win.searchMatches.length - 1; i >= 0; --i) {
                            var start = win.searchMatches[i];
                            EditorMutations.replaceRange(editor, start,
                                                         start + searchField.text.length,
                                                         replaceField.text);
                        }
                        win.searchUpdating = false;
                        backend.editorTextChanged();
                        win.updateSearch();
                    }
                }

                Rectangle {
                    Layout.preferredWidth: 1
                    Layout.preferredHeight: 34
                    color: win.darkMode ? "#6f6f62" : "#d5d56e"
                }

                SearchIconButton {
                    iconName: "up"
                    iconColor: win.darkMode ? win.textColor : "#62635f"
                    onClicked: win.moveSearch(-1)
                }

                SearchIconButton {
                    iconName: "down"
                    iconColor: win.darkMode ? win.textColor : "#62635f"
                    onClicked: win.moveSearch(1)
                }

                SearchIconButton {
                    iconName: "close"
                    iconColor: win.darkMode ? win.textColor : "#62635f"
                    onClicked: win.closeSearch()
                }
            }
        }
    }

    Component.onCompleted: {
        var geometry = backend.windowGeometry();
        if (geometry.x >= 0) x = geometry.x;
        if (geometry.y >= 0) y = geometry.y;
        width = geometry.width;
        height = geometry.height;
        normalX = x;
        normalY = y;
        normalWidth = width;
        normalHeight = height;
        wasMaximized = geometry.maximized;
        if (geometry.maximized) showMaximized();
    }

    Component.onDestruction: backend.saveWindowGeometry(
        normalX, normalY, normalWidth, normalHeight, wasMaximized)

}

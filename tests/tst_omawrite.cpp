#include <QtTest>
#include <QFont>
#include <QColor>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickStyle>
#include <QQuickItem>
#include <QQuickTextDocument>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QTextBlock>
#include <QTextDocument>

#include "backend.h"
#include "markdownhighlighter.h"

class OmawriteTest : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() {
        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(m_settingsDirectory.isValid());
        QQuickStyle::setStyle(QStringLiteral("Material"));
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
                           m_settingsDirectory.path());
        const QString configRoot = m_settingsDirectory.filePath(QStringLiteral("config"));
        QVERIFY(QDir().mkpath(configRoot));
        QVERIFY(qputenv("XDG_CONFIG_HOME", configRoot.toUtf8()));
    }

    // A stray asterisk next to whitespace is not an emphasis delimiter.
    void keepsWhitespaceDelimitedAsterisksLiteral() {
        const auto bullet = MarkdownHighlighter::inlineMarkup(
            QStringLiteral("* first *important* point"));
        QCOMPARE(bullet.size(), 1);
        QCOMPARE(bullet.at(0).kind, MarkdownHighlighter::InlineKind::Italic);
        QCOMPARE(bullet.at(0).content.start, 9);
        QCOMPARE(bullet.at(0).content.length, 9);

        QCOMPARE(MarkdownHighlighter::inlineMarkup(QStringLiteral("2 * 3 * 4")).size(), 0);

        const auto shortWord = MarkdownHighlighter::inlineMarkup(QStringLiteral("*a*"));
        QCOMPARE(shortWord.size(), 1);
        QCOMPARE(shortWord.at(0).content.length, 1);
    }

    // Verifies caret steps cross whole emoji and combining sequences.
    void stepsCaretOverWholeGraphemes() {
        Backend backend;
        QTextDocument document(QStringLiteral("a😀b\ne\u0301x"));
        backend.attachTextDocument(&document);

        QCOMPARE(backend.cursorPositionStep(1, 1), 3);
        QCOMPARE(backend.cursorPositionStep(3, -1), 1);
        QCOMPARE(backend.cursorPositionStep(4, 1), 5);   // across the block break
        QCOMPARE(backend.cursorPositionStep(5, 1), 7);   // e + combining acute
        QCOMPARE(backend.cursorPositionStep(7, -1), 5);
        QCOMPARE(backend.cursorPositionStep(0, -1), 0);
    }

    // Verifies printing converts the editor's pixel size to points.
    void printsInPointsNotPixels() {
        QFont editorFont;
        editorFont.setPixelSize(20);
        const QFont printed = Backend::printFont(editorFont, 96.0);
        QCOMPARE(printed.pixelSize(), -1);
        QCOMPARE(printed.pointSizeF(), 15.0);
        QCOMPARE(Backend::printFont(editorFont, 0.0).pointSizeF(), 15.0);

        QFont pointFont;
        pointFont.setPointSizeF(11.0);
        QCOMPARE(Backend::printFont(pointFont, 144.0).pointSizeF(), 11.0);
    }

    void countsWords() {
        QCOMPARE(Backend::countWords(QStringLiteral("one two-three don't 42")), 4);
        QCOMPARE(Backend::countWords(QStringLiteral("你好世界")), 4);
        QCOMPARE(Backend::countWords(QStringLiteral("日本語テスト")), 6);
        QCOMPARE(Backend::countWords(QStringLiteral("foo中bar")), 3);
        QCOMPARE(Backend::countWords(QString()), 0);
    }

    // Verifies TOML values retain quoted hashes and drop trailing comments.
    void readsAnnotatedTomlValues() {
        QCOMPARE(Backend::tomlValue(QStringLiteral("\"#112233\" # accent")),
                 QStringLiteral("#112233"));
        QCOMPARE(Backend::tomlValue(QStringLiteral("1.25 # scale")),
                 QStringLiteral("1.25"));
        QCOMPARE(Backend::tomlValue(QStringLiteral("#112233")),
                 QStringLiteral("#112233"));
    }

    void normalizesLinks() {
        QCOMPARE(Backend::normalizedLinkUrl(QStringLiteral("www.example.com/path")),
                 QStringLiteral("https://www.example.com/path"));
        QCOMPARE(Backend::normalizedLinkUrl(QStringLiteral("mailto:writer@example.com")),
                 QStringLiteral("mailto:writer@example.com"));
        QVERIFY(Backend::normalizedLinkUrl(QStringLiteral("example.com")).isEmpty());
        QVERIFY(Backend::normalizedLinkUrl(QStringLiteral("file:///tmp/private")).isEmpty());
    }

    void suggestsSafeNames() {
        QCOMPARE(Backend::suggestedFileName(QStringLiteral("My first draft\nBody")),
                 QStringLiteral("My first draft.md"));
        QCOMPARE(Backend::suggestedFileName(QStringLiteral("A/B")), QStringLiteral("A-B.md"));
        QCOMPARE(Backend::suggestedFileName(QString()), QStringLiteral("Untitled.md"));
        QCOMPARE(Backend::suggestedFileName(QStringLiteral("Already.md")),
                 QStringLiteral("Already.md"));
        QCOMPARE(Backend::suggestedFileName(QStringLiteral("Already.markdown")),
                 QStringLiteral("Already.markdown"));
        QCOMPARE(Backend::suggestedFileName(QStringLiteral("A\\B:*?")),
                 QStringLiteral("A-B---.md"));
        QCOMPARE(Backend::suggestedFileName(QStringLiteral("CON")),
                 QStringLiteral("_CON.md"));
    }

    void findsInlineMarkdownRanges() {
        const auto markup = MarkdownHighlighter::inlineMarkup(
            QStringLiteral("**bold** and *italic* and [site](https://example.com) and ![photo](assets/test.png)"));
        QCOMPARE(markup.size(), 4);
        QCOMPARE(markup.at(0).content.start, 2);
        QCOMPARE(markup.at(0).content.length, 4);
        QCOMPARE(markup.at(2).content.length, 4);
        QCOMPARE(markup.at(2).markers[0].length, 1);
        QCOMPARE(markup.at(3).kind, MarkdownHighlighter::InlineKind::Image);
        QCOMPARE(markup.at(3).content.length, 25);
        QCOMPARE(markup.at(3).markers[0].length, 2);

        const QString sample = QStringLiteral("Check [link](https://foo.com) and ![pic](image.png) and <img src=\"https://example.com/art.jpg\">");
        QCOMPARE(MarkdownHighlighter::linkUrlAt(sample, 8), QStringLiteral("https://foo.com"));
        QCOMPARE(MarkdownHighlighter::imageUrlAt(sample, 8), QString());
        QCOMPARE(MarkdownHighlighter::imageUrlAt(sample, 38), QStringLiteral("image.png"));
        QCOMPARE(MarkdownHighlighter::linkUrlAt(sample, 38), QString());
        QCOMPARE(MarkdownHighlighter::imageUrlAt(sample, 70), QStringLiteral("https://example.com/art.jpg"));

        const QString edgeCases = QStringLiteral(
            "\\*literal* `**code**` [nested](https://example.com/a_(b) \"title\")");
        const auto edgeMarkup = MarkdownHighlighter::inlineMarkup(edgeCases);
        QCOMPARE(edgeMarkup.size(), 1);
        QCOMPARE(MarkdownHighlighter::linkUrlAt(edgeCases, 24),
                 QStringLiteral("https://example.com/a_(b)"));
    }

    void loadsCurrentStandaloneConfig() {
        QTemporaryDir homeDirectory;
        QVERIFY(homeDirectory.isValid());

        const QByteArray originalHome = qgetenv("HOME");
        struct HomeRestorer {
            QByteArray value;
            ~HomeRestorer() { qputenv("HOME", value); }
        } restoreHome{originalHome};
        QVERIFY(qputenv("HOME", homeDirectory.path().toUtf8()));

        const QString configDirectory = QStandardPaths::writableLocation(
            QStandardPaths::GenericConfigLocation) + QStringLiteral("/omawrite");
        QVERIFY(QDir().mkpath(configDirectory));

        QFile colorsFile(configDirectory + QStringLiteral("/config.toml"));
        QVERIFY(colorsFile.open(QIODevice::WriteOnly | QIODevice::Text));
        const QByteArray palette(
            "mode = \"light\"\n"
            "accent = \"#112233\"\n"
            "selection = \"#445566\"\n"
            "background = \"#fefefe\"\n"
            "foreground = \"#101010\"\n"
            "text_scale = 1.25\n"
            "font_family = \"Configured Mono\"\n");
        QCOMPARE(colorsFile.write(palette), qint64(palette.size()));
        colorsFile.close();

        Backend backend;
        QCOMPARE(backend.themeBackground(), QStringLiteral("#fefefe"));
        QCOMPARE(backend.themeForeground(), QStringLiteral("#101010"));
        QCOMPARE(backend.themeAccent(), QStringLiteral("#112233"));
        QCOMPARE(backend.themeSelection(), QStringLiteral("#445566"));
        QCOMPARE(backend.textScale(), 1.25);
        QCOMPARE(backend.fontFamily(), QStringLiteral("Configured Mono"));
        QVERIFY(!backend.darkMode());

        backend.setSystemTextScale(2.0);
        backend.setDarkMode(true);
        QVERIFY(!backend.darkMode());
        QCOMPARE(backend.textScale(), 1.25);
        backend.setTextScale(2.5);
        backend.resetTextScale();
        QCOMPARE(backend.textScale(), 1.25);
        QVERIFY(colorsFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
        colorsFile.close();
        QTRY_COMPARE(backend.textScale(), 2.0);
        QTRY_COMPARE(backend.fontFamily(), QStringLiteral("iA Writer Mono S"));
        QTRY_VERIFY(backend.darkMode());
    }

    void ignoresFileWatcherEventsForSavedContents() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());

        const QString path = directory.filePath(QStringLiteral("first-save.md"));
        Backend backend;
        QTextDocument document;
        document.setPlainText(QStringLiteral("saved contents"));
        backend.attachTextDocument(&document);
        QSignalSpy externalChangeSpy(&backend, &Backend::externalChangeDetected);

        backend.saveAs(QUrl::fromLocalFile(path));
        QVERIFY(QFileInfo::exists(path));

        QFile sameContents(path);
        QVERIFY(sameContents.open(QIODevice::WriteOnly | QIODevice::Truncate));
        QCOMPARE(sameContents.write("saved contents"), qint64(14));
        sameContents.close();
        QTest::qWait(100);
        QCOMPARE(externalChangeSpy.count(), 0);

        QFile changedContents(path);
        QVERIFY(changedContents.open(QIODevice::WriteOnly | QIODevice::Truncate));
        QCOMPARE(changedContents.write("changed elsewhere"), qint64(17));
        changedContents.close();
        QTRY_COMPARE(externalChangeSpy.count(), 1);
    }

    void keepsCursorAndSelectionStableAcrossInsertions() {
        const QString mutationsPath = QFINDTESTDATA("../src/EditorMutations.js");
        QVERIFY(!mutationsPath.isEmpty());

        QQmlEngine engine;
        QQmlComponent component(&engine);
        const QByteArray harness = R"QML(
            import QtQuick
            import "EditorMutations.js" as EditorMutations

            TextEdit {
                property string insertionText
                property int insertionCursor
                property string wrappedText
                property int wrappedSelectionStart
                property int wrappedSelectionEnd

                Component.onCompleted: {
                    text = "alpha omega";
                    cursorPosition = 5;
                    EditorMutations.replaceRange(this, 5, 5, "one\r\ntwo");
                    insertionText = text;
                    insertionCursor = cursorPosition;

                    text = "alpha beta omega";
                    select(6, 10);
                    EditorMutations.replaceRange(this, selectionStart, selectionEnd,
                                                 "**beta**", 2, 6);
                    wrappedText = text;
                    wrappedSelectionStart = selectionStart;
                    wrappedSelectionEnd = selectionEnd;
                }
            }
        )QML";
        const QUrl harnessUrl = QUrl::fromLocalFile(
            QFileInfo(mutationsPath).absolutePath() + QStringLiteral("/MutationHarness.qml"));
        component.setData(harness, harnessUrl);
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> editor(component.create());
        QVERIFY2(editor, qPrintable(component.errorString()));

        QCOMPARE(editor->property("insertionText").toString(),
                 QStringLiteral("alphaone\ntwo omega"));
        QCOMPARE(editor->property("insertionCursor").toInt(), 12);
        QCOMPARE(editor->property("wrappedText").toString(),
                 QStringLiteral("alpha **beta** omega"));
        QCOMPARE(editor->property("wrappedSelectionStart").toInt(), 8);
        QCOMPARE(editor->property("wrappedSelectionEnd").toInt(), 12);
    }

    void savesAndOpensFromFooterButtons() {
        const QString mainQmlPath = QFINDTESTDATA("../src/Main.qml");
        QVERIFY(!mainQmlPath.isEmpty());

        Backend backend;
        QQmlEngine engine;
        engine.rootContext()->setContextProperty(QStringLiteral("backend"), &backend);
        QQmlComponent component(&engine, QUrl::fromLocalFile(mainQmlPath));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> window(component.create());
        QVERIFY2(window, qPrintable(component.errorString()));

        QObject *sourceEditor = window->findChild<QObject *>(QStringLiteral("sourceEditor"));
        QObject *preview = window->findChild<QObject *>(QStringLiteral("previewView"));
        QVERIFY(sourceEditor);
        QVERIFY(preview);
        QVERIFY(sourceEditor->property("visible").toBool());
        QVERIFY(!preview->property("visible").toBool());

        QObject *saveButton = window->findChild<QObject *>(QStringLiteral("saveButton"));
        QObject *openButton = window->findChild<QObject *>(QStringLiteral("openButton"));
        QObject *previewButton = window->findChild<QObject *>(QStringLiteral("previewButton"));
        QVERIFY(saveButton);
        QVERIFY(openButton);
        QVERIFY(previewButton);

        auto *sourceDocument = qobject_cast<QQuickTextDocument *>(
            sourceEditor->property("textDocument").value<QObject *>());
        QVERIFY(sourceDocument);
        sourceDocument->textDocument()->setPlainText(QStringLiteral("# Preview\n\nBody"));
        QVERIFY(QMetaObject::invokeMethod(previewButton, "clicked"));
        QVERIFY(!sourceEditor->property("visible").toBool());
        QVERIFY(preview->property("visible").toBool());
        auto *previewDocument = qobject_cast<QQuickTextDocument *>(
            preview->property("textDocument").value<QObject *>());
        QVERIFY(previewDocument);
        QCOMPARE(previewDocument->textDocument()->toPlainText(),
                 QStringLiteral("Preview\nBody"));

        QSignalSpy saveDialogSpy(&backend, &Backend::saveDialogRequested);
        QVERIFY(QMetaObject::invokeMethod(saveButton, "clicked"));
        QCOMPARE(saveDialogSpy.count(), 1);
        QObject *saveDialog = window->findChild<QObject *>(QStringLiteral("saveFileDialog"));
        QVERIFY(saveDialog);
        QVERIFY(QMetaObject::invokeMethod(saveDialog, "close"));

        QSignalSpy openDialogSpy(&backend, &Backend::openDialogRequested);
        QVERIFY(QMetaObject::invokeMethod(openButton, "clicked"));
        QCOMPARE(openDialogSpy.count(), 1);
        QObject *openDialog = window->findChild<QObject *>(QStringLiteral("openFileDialog"));
        QVERIFY(openDialog);
        QVERIFY(QMetaObject::invokeMethod(openDialog, "close"));
    }

    // Verifies the editor owns apostrophes, fences, zoom, and footer layout correctly.
    void handlesEditorKeysAndFooterLayout() {
        const QString mainQmlPath = QFINDTESTDATA("../src/Main.qml");
        QVERIFY(!mainQmlPath.isEmpty());

        Backend backend;
        QQmlEngine engine;
        engine.rootContext()->setContextProperty(QStringLiteral("backend"), &backend);
        QQmlComponent component(&engine, QUrl::fromLocalFile(mainQmlPath));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> root(component.create());
        QVERIFY2(root, qPrintable(component.errorString()));

        auto *window = qobject_cast<QQuickWindow *>(root.data());
        auto *editor = root->findChild<QQuickItem *>(QStringLiteral("sourceEditor"));
        auto *footer = root->findChild<QQuickItem *>(QStringLiteral("footer"));
        auto *viewport = root->findChild<QQuickItem *>(QStringLiteral("editorViewport"));
        QVERIFY(window);
        QVERIFY(editor);
        QVERIFY(footer);
        QVERIFY(viewport);
        auto *quickDocument = qobject_cast<QQuickTextDocument *>(
            editor->property("textDocument").value<QObject *>());
        QVERIFY(quickDocument);
        window->requestActivate();
        QTRY_VERIFY(window->isActive());
        QVERIFY(QMetaObject::invokeMethod(editor, "forceActiveFocus"));

        editor->setProperty("text", QString());
        QTest::keyClick(window, Qt::Key_D);
        QTest::keyClick(window, Qt::Key_O);
        QTest::keyClick(window, Qt::Key_N);
        QTest::keyClick(window, Qt::Key_Apostrophe);
        QTest::keyClick(window, Qt::Key_T);
        QCOMPARE(quickDocument->textDocument()->toPlainText(), QStringLiteral("don't"));

        editor->setProperty("text", QString());
        QTest::keyClick(window, Qt::Key_QuoteLeft);
        QTest::keyClick(window, Qt::Key_QuoteLeft);
        QTest::keyClick(window, Qt::Key_QuoteLeft);
        QCOMPARE(quickDocument->textDocument()->toPlainText(), QStringLiteral("```"));

        quickDocument->textDocument()->setPlainText(QStringLiteral("~~~\n- literal"));
        editor->setProperty("cursorPosition", editor->property("length"));
        QTest::keyClick(window, Qt::Key_Return);
        QCOMPARE(quickDocument->textDocument()->toPlainText(),
                 QStringLiteral("~~~\n- literal\n"));

        // Tab nests list items, Shift+Tab lifts them, and plain text keeps its tab.
        auto plainText = [&] { return quickDocument->textDocument()->toPlainText(); };
        quickDocument->textDocument()->setPlainText(QStringLiteral("- one\n- two"));
        editor->setProperty("cursorPosition", 11);
        QTest::keyClick(window, Qt::Key_Tab);
        QCOMPARE(plainText(), QStringLiteral("- one\n    - two"));
        QTest::keyClick(window, Qt::Key_Backtab, Qt::ShiftModifier);
        QCOMPARE(plainText(), QStringLiteral("- one\n- two"));
        quickDocument->textDocument()->setPlainText(QStringLiteral("```\n- a\n```"));
        editor->setProperty("cursorPosition", 7);
        QTest::keyClick(window, Qt::Key_Tab);
        QCOMPARE(plainText(), QStringLiteral("```\n- a\t\n```"));

        QCOMPARE(backend.editorFontSize(), 20);
        QTest::keyClick(window, Qt::Key_Equal, Qt::ControlModifier);
        QCOMPARE(backend.editorFontSize(), 22);
        QTest::keyClick(window, Qt::Key_0, Qt::ControlModifier);
        QCOMPARE(backend.editorFontSize(), 20);

        QCOMPARE(viewport->mapToScene(QPointF(0, viewport->height())).y(),
                 footer->mapToScene(QPointF()).y());
        QCOMPARE(footer->property("color").value<QColor>(),
                 QColor(backend.themeBackground()));
    }

    // Verifies dialog letter shortcuts stay scoped to the open dialog.
    void handlesUnsavedDialogShortcuts() {
        const QString mainQmlPath = QFINDTESTDATA("../src/Main.qml");
        Backend backend;
        QQmlEngine engine;
        engine.rootContext()->setContextProperty(QStringLiteral("backend"), &backend);
        QQmlComponent component(&engine, QUrl::fromLocalFile(mainQmlPath));
        QScopedPointer<QObject> root(component.create());
        QVERIFY2(root, qPrintable(component.errorString()));
        auto *window = qobject_cast<QQuickWindow *>(root.data());
        QObject *dialog = root->findChild<QObject *>(QStringLiteral("unsavedChangesDialog"));
        QVERIFY(window);
        QVERIFY(dialog);
        QSignalSpy cancelSpy(dialog, SIGNAL(cancelRequested()));
        QVERIFY(QMetaObject::invokeMethod(dialog, "open"));
        QTRY_VERIFY(dialog->property("opened").toBool());
        QTest::keyClick(window, Qt::Key_C);
        QTRY_COMPARE(cancelSpy.count(), 1);
    }

    void scalesTextWithDesktopTextSize() {
        const QString mainQmlPath = QFINDTESTDATA("../src/Main.qml");
        QVERIFY(!mainQmlPath.isEmpty());

        QTemporaryDir homeDirectory;
        QVERIFY(homeDirectory.isValid());
        const QByteArray originalHome = qgetenv("HOME");
        struct HomeRestorer {
            QByteArray value;
            ~HomeRestorer() { qputenv("HOME", value); }
        } restoreHome{originalHome};
        QVERIFY(qputenv("HOME", homeDirectory.path().toUtf8()));

        Backend backend;
        QQmlEngine engine;
        engine.rootContext()->setContextProperty(QStringLiteral("backend"), &backend);
        QQmlComponent component(&engine, QUrl::fromLocalFile(mainQmlPath));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> window(component.create());
        QVERIFY2(window, qPrintable(component.errorString()));

        QObject *editor = window->findChild<QObject *>(QStringLiteral("sourceEditor"));
        QVERIFY(editor);
        QCOMPARE(editor->property("font").value<QFont>().pixelSize(), 20);

        // `omarchy display text size 16` sets the GNOME factor to 16/12.
        backend.setTextScale(16.0 / 12.0);
        QCOMPARE(window->property("editorFontPixelSize").toInt(), 27);
        QCOMPARE(editor->property("font").value<QFont>().pixelSize(), 27);

        backend.setTextScale(9.0 / 12.0);
        QCOMPARE(window->property("editorFontPixelSize").toInt(), 15);
        QCOMPARE(editor->property("font").value<QFont>().pixelSize(), 15);
    }

    void remembersLastSaveDirectory() {
        QTemporaryDir saveDirectory;
        QVERIFY(saveDirectory.isValid());

        const QString savedPath = saveDirectory.filePath(QStringLiteral("first.md"));
        Backend savedDocument;
        QTextDocument document;
        savedDocument.attachTextDocument(&document);
        savedDocument.saveAs(QUrl::fromLocalFile(savedPath));

        Backend nextDocument;
        QSignalSpy saveDialogSpy(&nextDocument, &Backend::saveDialogRequested);
        nextDocument.saveAsDialog();
        QCOMPARE(saveDialogSpy.count(), 1);

        const QUrl suggestedUrl = saveDialogSpy.takeFirst().constFirst().toUrl();
        QCOMPARE(QFileInfo(suggestedUrl.toLocalFile()).absolutePath(),
                 saveDirectory.path());
        QCOMPARE(QFileInfo(suggestedUrl.toLocalFile()).fileName(),
                 QStringLiteral("Untitled.md"));

        QSettings().setValue(QStringLiteral("file/lastSaveDirectory"),
                             saveDirectory.filePath(QStringLiteral("missing")));
        Backend fallbackDocument;
        QSignalSpy fallbackDialogSpy(&fallbackDocument, &Backend::saveDialogRequested);
        fallbackDocument.saveAsDialog();
        const QUrl fallbackUrl = fallbackDialogSpy.takeFirst().constFirst().toUrl();
        QCOMPARE(QFileInfo(fallbackUrl.toLocalFile()).absolutePath(), QDir::homePath());
    }

    // Verifies returning to the baseline immediately clears the dirty state.
    void clearsDirtyStateWhenTextIsReverted() {
        Backend backend;
        QTextDocument document;
        backend.attachTextDocument(&document);

        document.setPlainText(QStringLiteral("draft"));
        QVERIFY(backend.editorTextChanged());
        QVERIFY(backend.modified());
        document.setPlainText(QString());
        QVERIFY(backend.editorTextChanged());
        QVERIFY(!backend.modified());
    }

    // Verifies editor font size is bounded and restored across backend instances.
    void persistsEditorFontSize() {
        QSettings().remove(QStringLiteral("editor/fontSize"));
        {
            Backend backend;
            backend.setEditorFontSize(28);
            QCOMPARE(backend.editorFontSize(), 28);
        }
        Backend restored;
        QCOMPARE(restored.editorFontSize(), 28);
        restored.setEditorFontSize(100);
        QCOMPARE(restored.editorFontSize(), 48);
    }

    // Verifies external deletion immediately protects the in-memory document.
    void protectsDocumentAfterExternalDeletion() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("deleted.md"));

        Backend backend;
        QTextDocument document;
        document.setPlainText(QStringLiteral("important text"));
        backend.attachTextDocument(&document);
        backend.saveAs(QUrl::fromLocalFile(path));
        QVERIFY(!backend.modified());

        QSignalSpy externalChangeSpy(&backend, &Backend::externalChangeDetected);
        QVERIFY(QFile::remove(path));
        QTRY_COMPARE(externalChangeSpy.count(), 1);
        QVERIFY(backend.modified());
        QCOMPARE(externalChangeSpy.constFirst().at(0).toBool(), true);
        QCOMPARE(externalChangeSpy.constFirst().at(1).toBool(), false);
    }

    // Verifies persistent sidebar width is exposed to the next backend instance.
    void restoresSidebarWidth() {
        Backend first;
        first.saveSidebarWidth(387);
        Backend second;
        QCOMPARE(second.sidebarWidth(), 387);
    }

    // Verifies statistics treat adjacent non-empty lines as one paragraph.
    void countsParagraphsAndEmptyReadingTime() {
        Backend backend;
        QTextDocument document;
        backend.attachTextDocument(&document);
        QCOMPARE(backend.readingTimeMinutes(), 0);

        document.setPlainText(QStringLiteral("first line\nwrapped line\n\nsecond paragraph"));
        QVERIFY(backend.editorTextChanged());
        QTRY_COMPARE(backend.paragraphCount(), 2);
        QCOMPARE(backend.wordCount(), 6);
    }

    // Verifies C++ and tilde fences retain language and heading boundaries.
    void recognizesFencedCodeAndHeadings() {
        QTextDocument highlighted(QStringLiteral("```c++\nclass Example {};\n```"));
        MarkdownHighlighter highlighter(&highlighted);
        highlighter.rehighlight();
        QCOMPARE(highlighted.firstBlock().userState() % 100, 8);

        Backend backend;
        QTextDocument outlined(QStringLiteral("# Visible\n~~~cpp\n# Hidden\n~~~\n   ## Also visible"));
        backend.attachTextDocument(&outlined);
        const QVariantList headings = backend.documentHeadings();
        QCOMPARE(headings.size(), 2);
        QCOMPARE(headings.at(0).toMap().value(QStringLiteral("title")).toString(),
                 QStringLiteral("Visible"));
        QCOMPARE(headings.at(1).toMap().value(QStringLiteral("title")).toString(),
                 QStringLiteral("Also visible"));
    }

    // Verifies heading markers outdent while body blocks retain the full gutter.
    void outdentsHeadingMarkersWithoutBreakingUndo() {
        Backend backend;
        QTextDocument document(QStringLiteral("# Heading\nBody"));
        document.documentLayout();  // contentsChange only fires once a layout exists
        backend.setHeadingCellWidth(10);
        backend.attachTextDocument(&document);

        QCOMPARE(document.indentWidth(), 10.0);
        QCOMPARE(document.firstBlock().blockFormat().indent(), 5);
        QCOMPARE(document.firstBlock().next().blockFormat().indent(), 7);

        QTextCursor cursor(document.firstBlock().next());
        cursor.beginEditBlock();  // the editor wraps typed input the same way
        cursor.insertText(QStringLiteral("# "));
        cursor.endEditBlock();
        QVERIFY(backend.editorTextChanged());
        QCOMPARE(document.firstBlock().next().blockFormat().indent(), 5);
        document.undo();
        QCOMPARE(document.toPlainText(), QStringLiteral("# Heading\nBody"));
    }

    // Verifies comment lines inside fenced code keep the body indent.
    void doesNotOutdentHashLinesInsideFences() {
        Backend backend;
        QTextDocument document(QStringLiteral("```sh\n# comment\n```\n# Heading"));
        document.documentLayout();
        backend.setHeadingCellWidth(10);
        backend.attachTextDocument(&document);

        QCOMPARE(document.findBlockByNumber(1).blockFormat().indent(), 7);
        QCOMPARE(document.findBlockByNumber(3).blockFormat().indent(), 5);
    }

    // Verifies the backend refuses to create an empty file without an editor document.
    void rejectsSaveWithoutDocument() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("missing-document.md"));
        Backend backend;
        backend.saveAs(QUrl::fromLocalFile(path));
        QVERIFY(!QFileInfo::exists(path));
        QCOMPARE(backend.status(), QStringLiteral("There is no document to save."));
    }

    // Verifies invalid UTF-8 never replaces or later corrupts the active document.
    void rejectsInvalidUtf8() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("invalid.md"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(QByteArray(1, char(0xff))), qint64(1));
        file.close();

        Backend backend;
        QTextDocument document(QStringLiteral("safe contents"));
        backend.attachTextDocument(&document);
        backend.open(QUrl::fromLocalFile(path));
        QCOMPARE(document.toPlainText(), QStringLiteral("safe contents"));
        QVERIFY(backend.fileUrl().isEmpty());
        QVERIFY(backend.status().contains(QStringLiteral("not valid UTF-8")));
    }

    // Verifies geometry from a removed monitor is not restored off-screen.
    void rejectsOffscreenWindowGeometry() {
        Backend backend;
        backend.saveWindowGeometry(999999, 999999, 1280, 820, false);
        const QVariantMap geometry = backend.windowGeometry();
        QCOMPARE(geometry.value(QStringLiteral("x")).toInt(), -1);
        QCOMPARE(geometry.value(QStringLiteral("y")).toInt(), -1);
        QCOMPARE(geometry.value(QStringLiteral("width")).toInt(), 1280);
        QCOMPARE(geometry.value(QStringLiteral("height")).toInt(), 820);
    }

private:
    QTemporaryDir m_settingsDirectory;
};

QTEST_MAIN(OmawriteTest)
#include "tst_omawrite.moc"

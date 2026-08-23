#include <QtTest>
#include <QFont>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickStyle>
#include <QStandardPaths>
#include <QTextBlock>
#include <QTextDocument>

#include "backend.h"
#include "markdownhighlighter.h"

class OmawriteTest : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() {
        QVERIFY(m_settingsDirectory.isValid());
        QQuickStyle::setStyle(QStringLiteral("Material"));
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
                           m_settingsDirectory.path());
        const QString configRoot = m_settingsDirectory.filePath(QStringLiteral("config"));
        QVERIFY(QDir().mkpath(configRoot));
        QVERIFY(qputenv("XDG_CONFIG_HOME", configRoot.toUtf8()));
    }

    void countsWords() {
        QCOMPARE(Backend::countWords(QStringLiteral("one two-three don't 42")), 4);
        QCOMPARE(Backend::countWords(QStringLiteral("你好 世界")), 2);
        QCOMPARE(Backend::countWords(QString()), 0);
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

        QVERIFY(window->findChild<QObject *>(QStringLiteral("sourceEditor")));
        QVERIFY(!window->findChild<QObject *>(QStringLiteral("renderedPreview")));
        QVERIFY(!window->findChild<QObject *>(QStringLiteral("modeToggle")));

        QObject *saveButton = window->findChild<QObject *>(QStringLiteral("saveButton"));
        QObject *openButton = window->findChild<QObject *>(QStringLiteral("openButton"));
        QVERIFY(saveButton);
        QVERIFY(openButton);

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

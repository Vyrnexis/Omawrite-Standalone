#include "backend.h"

#include <QClipboard>
#include <QColor>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDesktopServices>
#include <QGuiApplication>
#include <QMimeData>
#include <QProcess>
#include <QPrintDialog>
#include <QPrinter>
#include <QQuickTextDocument>
#include <QRegularExpression>
#include <QScreen>
#include <QSettings>
#include <QStandardPaths>
#include <QStringConverter>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QSaveFile>
#include <QTextBlock>
#include <QTextBlockFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextLayout>
#include <QTextList>
#include <QTextStream>
#include <QUrl>
#include <QVariantMap>
#include <QWindow>

#include <algorithm>

#include "markdownhighlighter.h"

constexpr qreal typoraLineHeightPercent = 140;
const QString lastSaveDirectorySetting = QStringLiteral("file/lastSaveDirectory");
const QString browseDirectorySetting = QStringLiteral("file/browseDirectory");
const QString sidebarWidthSetting = QStringLiteral("file/sidebarWidth");
const QString editorFontSizeSetting = QStringLiteral("editor/fontSize");
constexpr int defaultEditorFontSize = 20;
constexpr int minimumEditorFontSize = 10;
constexpr int maximumEditorFontSize = 48;

enum class AtomicWrite { Written, Failed, NotOpened };

// Blends a subtle code-panel color from the active page and text colors.
QString blendedCodeBackground(const QString &background, const QString &foreground) {
    const QColor page(background);
    const QColor text(foreground);
    return QColor::fromRgbF(page.redF() * 0.94 + text.redF() * 0.06,
                            page.greenF() * 0.94 + text.greenF() * 0.06,
                            page.blueF() * 0.94 + text.blueF() * 0.06).name();
}

// Writes a document atomically and distinguishes failure before opening.
AtomicWrite writeDocumentAtomically(const QString &path, const QByteArray &contents) {
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return AtomicWrite::NotOpened;
    if (file.write(contents) != contents.size()) {
        file.cancelWriting();
        return AtomicWrite::Failed;
    }
    return file.commit() ? AtomicWrite::Written : AtomicWrite::Failed;
}

// Writes directly only when atomic creation is unsupported by the filesystem.
bool writeDocumentDirectly(const QString &path, const QByteArray &contents) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
        return false;
    return file.write(contents) == contents.size() && file.flush();
}

// Returns the platform-native Omawrite configuration directory.
QString configurationDirectory() {
    const QString root = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    const QString configRoot = root.isEmpty()
        ? QDir::home().filePath(QStringLiteral(".config")) : root;
    return QDir(configRoot).filePath(QStringLiteral("omawrite"));
}

QString Backend::normalizedLinkUrl(const QString &clipboardText) {
    QString candidate = clipboardText.trimmed();
    static const QRegularExpression lineBreakRe(QStringLiteral("[\\r\\n]"));
    const int lineBreak = candidate.indexOf(lineBreakRe);
    if (lineBreak >= 0)
        candidate = candidate.left(lineBreak).trimmed();

    if (candidate.isEmpty())
        return {};

    if (candidate.startsWith(QStringLiteral("www."), Qt::CaseInsensitive))
        candidate.prepend(QStringLiteral("https://"));

    static const QRegularExpression schemeRe(
        QStringLiteral("^[A-Za-z][A-Za-z0-9+.-]*:"));
    if (!schemeRe.match(candidate).hasMatch())
        return {};

    const QUrl url(candidate);
    if (!url.isValid() || url.scheme().isEmpty())
        return {};

    const QString scheme = url.scheme().toLower();
    const bool webUrl = scheme == QStringLiteral("http")
        || scheme == QStringLiteral("https")
        || scheme == QStringLiteral("ftp");
    if (webUrl && url.host().isEmpty())
        return {};

    if (!webUrl && scheme != QStringLiteral("mailto"))
        return {};

    return url.toString();
}

Backend::Backend(QObject *parent) : QObject(parent) {
    m_editorFontSize = qBound(minimumEditorFontSize,
                              QSettings().value(editorFontSizeSetting,
                                                defaultEditorFontSize).toInt(),
                              maximumEditorFontSize);
    const QString stateDirectory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(stateDirectory);
    // Claim an orphaned snapshot before taking an empty slot. This ensures a
    // crash in window 2 is still recovered even if window 1 exited normally.
    for (int pass = 0; pass < 2 && !m_recoveryLock; ++pass) {
        for (int slot = 0; slot < 100; ++slot) {
            const QString base = QDir(stateDirectory).filePath(
                QStringLiteral("recovery-%1").arg(slot));
            const bool snapshotExists = QFileInfo::exists(base + QStringLiteral(".json"));
            if ((pass == 0) != snapshotExists)
                continue;
            auto lock = std::make_unique<QLockFile>(base + QStringLiteral(".lock"));
            if (lock->tryLock()) {
                m_recoveryPath = base + QStringLiteral(".json");
                m_recoveryLock = std::move(lock);
                break;
            }
        }
    }
    m_wordCountTimer.setSingleShot(true);
    m_wordCountTimer.setInterval(120);
    connect(&m_wordCountTimer, &QTimer::timeout, this, &Backend::refreshWordCount);
    m_recoveryTimer.setSingleShot(true);
    m_recoveryTimer.setInterval(750);
    connect(&m_recoveryTimer, &QTimer::timeout, this, &Backend::writeRecovery);
    connect(&m_fileWatcher, &QFileSystemWatcher::fileChanged, this,
            [this](const QString &path) {
                if (path != m_fileUrl.toLocalFile())
                    return;

                const bool deleted = !QFileInfo::exists(path);
                if (!deleted && m_hasKnownFileContents) {
                    QFile file(path);
                    if (file.open(QIODevice::ReadOnly)
                            && file.readAll() == m_lastKnownFileContents) {
                        // Atomic saves can replace the watched inode. Re-arm the
                        // watcher, but do not report our own save as an outside edit.
                        watchCurrentFile();
                        return;
                    }
                }

                if (!deleted) {
                    m_hasKnownFileContents = false;
                    m_lastKnownFileText.clear();
                }
                const bool locallyModified = m_modified;
                if (deleted) {
                    setKnownFileContents(QByteArray(), false);
                    setModified(true);
                    scheduleRecovery();
                }
                emit externalChangeDetected(deleted, locallyModified);
            });

    connect(&m_folderWatcher, &QFileSystemWatcher::directoryChanged, this,
            [this]() { emit folderChanged(); });
    const QString remembered = QSettings().value(browseDirectorySetting).toString();
    applyFolder(QDir(remembered).exists() ? remembered
                                          : defaultDirectory().absolutePath(),
                false);

    loadThemeConfig();
    watchThemeConfig();
    connect(&m_themeWatcher, &QFileSystemWatcher::fileChanged, this, [this]() {
        loadThemeConfig();
        watchThemeConfig();
    });
    connect(&m_themeWatcher, &QFileSystemWatcher::directoryChanged, this, [this]() {
        loadThemeConfig();
        watchThemeConfig();
    });
}

Backend::~Backend() = default;

void Backend::setParentWindow(QWindow *window) {
    m_parentWindow = window;
}

QString Backend::fileName() const {
    if (!m_fileUrl.isValid() || m_fileUrl.isEmpty())
        return QStringLiteral("Untitled.md");

    if (m_fileUrl.isLocalFile()) {
        const QFileInfo info(m_fileUrl.toLocalFile());
        if (!info.fileName().isEmpty())
            return info.fileName();
    }

    const QString name = m_fileUrl.fileName();
    return name.isEmpty() ? QStringLiteral("Untitled.md") : name;
}

void Backend::setDarkMode(bool darkMode) {
    if (m_systemDarkMode == darkMode)
        return;
    m_systemDarkMode = darkMode;
    loadThemeConfig();
}

void Backend::setTextScale(qreal textScale) {
    applyTextScale(qBound<qreal>(0.5, textScale, 3.0));
}

// Persists the editor's base font size within its supported range.
void Backend::setEditorFontSize(int editorFontSize) {
    const int boundedSize = qBound(minimumEditorFontSize, editorFontSize,
                                   maximumEditorFontSize);
    if (m_editorFontSize == boundedSize)
        return;
    m_editorFontSize = boundedSize;
    QSettings().setValue(editorFontSizeSetting, m_editorFontSize);
    emit editorFontSizeChanged();
}

// Restores the shipped editor font size.
void Backend::resetEditorFontSize() {
    setEditorFontSize(defaultEditorFontSize);
}

// Applies desktop scaling unless the configuration supplies an explicit scale.
void Backend::setSystemTextScale(qreal textScale) {
    m_systemTextScale = qBound<qreal>(0.5, textScale, 3.0);
    if (!m_hasConfiguredTextScale)
        applyTextScale(m_systemTextScale);
}

// Restores zoom to the configured scale or the current desktop scale.
void Backend::resetTextScale() {
    applyTextScale(m_hasConfiguredTextScale ? m_configuredTextScale : m_systemTextScale);
}

void Backend::attachDocument(QObject *textDocument) {
    auto *quickDocument = qobject_cast<QQuickTextDocument *>(textDocument);
    if (!quickDocument || !quickDocument->textDocument()) {
        setStatus(QStringLiteral("Could not attach the Markdown renderer."));
        return;
    }
    attachTextDocument(quickDocument->textDocument());
}

// Attaches the editable document and initializes highlighting, metrics, and recovery.
void Backend::attachTextDocument(QTextDocument *document) {
    if (!document) {
        setStatus(QStringLiteral("Could not attach the Markdown renderer."));
        return;
    }
    if (m_highlighter)
        delete m_highlighter.data();

    m_document = document;
    m_highlighter = new MarkdownHighlighter(m_document);
    m_highlighter->setDarkMode(m_darkMode);
    m_highlighter->setColors(m_themeBackground, m_themeForeground, m_themeAccent,
                             m_themeCodeBackground);

    connect(m_document, &QTextDocument::contentsChange, this,
            [this](int position, int, int charsAdded) {
                if (m_formattingTypography || m_loading)
                    return;
                m_lastChangePos = position;
                m_lastChangeAdded = charsAdded;
            });

    applyDocumentTypography();
    restoreRecovery();
}

void Backend::openDialog() {
    emit openDialogRequested();
}

void Backend::setFolder(const QUrl &url) {
    if (!url.isLocalFile())
        return;

    applyFolder(url.toLocalFile(), true);
}

void Backend::openParentFolder() {
    QDir directory(m_folderUrl.toLocalFile());
    if (!directory.cdUp())
        return;

    applyFolder(directory.absolutePath(), true);
}

QUrl Backend::createDocument(const QString &name) {
    const QDir directory(m_folderUrl.toLocalFile());
    const QString fileName = suggestedFileName(name);
    const QString path = directory.filePath(fileName);
    if (QFileInfo::exists(path)) {
        setStatus(QStringLiteral("%1 already exists.").arg(fileName));
        return {};
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        setStatus(QStringLiteral("Could not create %1.").arg(fileName));
        return {};
    }
    file.close();

    setStatus(QStringLiteral("Created %1").arg(fileName));
    emit folderChanged();
    return QUrl::fromLocalFile(path);
}

QUrl Backend::createFolder(const QString &name) {
    QDir directory(m_folderUrl.toLocalFile());
    const QString folderName = sanitizedEntryName(name);
    if (directory.exists(folderName)) {
        setStatus(QStringLiteral("%1 already exists.").arg(folderName));
        return {};
    }

    if (!directory.mkdir(folderName)) {
        setStatus(QStringLiteral("Could not create %1.").arg(folderName));
        return {};
    }

    setStatus(QStringLiteral("Created %1").arg(folderName));
    emit folderChanged();
    return QUrl::fromLocalFile(directory.filePath(folderName));
}

int Backend::sidebarWidth() const {
    return QSettings().value(sidebarWidthSetting, 240).toInt();
}

void Backend::saveSidebarWidth(int width) {
    QSettings().setValue(sidebarWidthSetting, width);
}

void Backend::open(const QUrl &url) {
    if (!url.isLocalFile()) {
        setStatus(QStringLiteral("Only local files can be opened."));
        return;
    }

    const QString targetName = QFileInfo(url.toLocalFile()).fileName();
    QFile file(url.toLocalFile());

    if (!file.exists()) {
        loadDocumentText(QString());
        clearRecovery();
        setKnownFileContents(QByteArray(), false);
        setFileUrl(url);
        setModified(false);
        setStatus(QStringLiteral("New file %1").arg(targetName));
        return;
    }
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        setStatus(QStringLiteral("Could not open %1.").arg(targetName));
        return;
    }

    const QByteArray contents = file.readAll();
    QStringDecoder decoder(QStringDecoder::Utf8);
    const QString text = decoder.decode(contents);
    if (decoder.hasError()) {
        setStatus(QStringLiteral("Could not open %1: the file is not valid UTF-8.").arg(targetName));
        return;
    }
    loadDocumentText(text);
    clearRecovery();
    setKnownFileContents(contents, true);
    setFileUrl(url);
    watchCurrentFile();
    setModified(false);
    setStatus(QStringLiteral("Opened %1").arg(fileName()));
}

void Backend::save() {
    if (!m_fileUrl.isValid() || m_fileUrl.isEmpty()) {
        saveAsDialog();
        return;
    }

    saveTo(m_fileUrl);
}

void Backend::saveAsDialog() {
    emit saveDialogRequested(suggestedSaveUrl());
}

void Backend::saveAs(const QUrl &url) {
    saveTo(url);
}

void Backend::discardRecovery() {
    clearRecovery();
}

void Backend::reloadFromDisk() {
    if (m_fileUrl.isLocalFile())
        open(m_fileUrl);
}

void Backend::keepExternalVersion() {
    QFile file(m_fileUrl.toLocalFile());
    if (file.open(QIODevice::ReadOnly)) {
        setKnownFileContents(file.readAll(), true);
    } else {
        setKnownFileContents(QByteArray(), false);
    }
    setModified(true);
    scheduleRecovery();
    watchCurrentFile();
    setStatus(QStringLiteral("Kept your version"));
}

QFont Backend::printFont(const QFont &editorFont, qreal screenDpi) {
    // The editor sizes its font in pixels, which a printer takes as device dots.
    // Convert to points at the screen's DPI so the page matches the editor.
    QFont font = editorFont;
    if (font.pixelSize() <= 0)
        return font;

    const qreal dpi = screenDpi > 0.0 ? screenDpi : 96.0;
    font.setPointSizeF(font.pixelSize() * 72.0 / dpi);
    return font;
}

void Backend::printDocument() {
    if (!m_document) {
        setStatus(QStringLiteral("There is no document to print."));
        return;
    }

    QPrinter printer(QPrinter::HighResolution);
    QPrintDialog dialog(&printer);
    dialog.setWindowTitle(QStringLiteral("Print %1").arg(fileName()));
    dialog.winId();
    if (dialog.windowHandle() && m_parentWindow)
        dialog.windowHandle()->setTransientParent(m_parentWindow);

    if (dialog.exec() == QDialog::Accepted) {
        QTextDocument rendered;
        const QScreen *screen = m_parentWindow ? m_parentWindow->screen()
                                               : QGuiApplication::primaryScreen();
        rendered.setDefaultFont(printFont(m_document->defaultFont(),
                                          screen ? screen->logicalDotsPerInchY() : 0.0));
        rendered.setMarkdown(currentDocumentText());
        rendered.print(&printer);
    }
}

void Backend::copyAsHtml() {
    QTextDocument rendered;
    rendered.setMarkdown(currentDocumentText());

    // Drop Qt's fixed fonts and margins from the body and blocks, so the pasted
    // text takes on the destination's own typeface and paragraph spacing.
    QString html = rendered.toHtml();
    static const QRegularExpression blockStyle(
        QStringLiteral("<(body|p|h[1-6]|ul|ol|li)\\s+style=\"[^\"]*\""));
    html.replace(blockStyle, QStringLiteral("<\\1"));

    auto *mimeData = new QMimeData;
    mimeData->setHtml(html);
    mimeData->setText(rendered.toPlainText());
    QGuiApplication::clipboard()->setMimeData(mimeData);
    setStatus(QStringLiteral("Copied as HTML."));
}

void Backend::newWindow() {
    const bool started = QProcess::startDetached(QCoreApplication::applicationFilePath(),
                                                 QStringList());
    if (!started)
        setStatus(QStringLiteral("Could not open a new window."));
}

QString Backend::clipboardUrl() const {
    const QClipboard *clipboard = QGuiApplication::clipboard();
    if (!clipboard)
        return {};

    const QMimeData *mimeData = clipboard->mimeData();
    if (!mimeData)
        return {};

    if (mimeData->hasUrls()) {
        const QList<QUrl> urls = mimeData->urls();
        for (const QUrl &url : urls) {
            const QString normalized = normalizedLinkUrl(url.toString());
            if (!normalized.isEmpty())
                return normalized;
        }
    }

    if (!mimeData->hasText())
        return {};

    return normalizedLinkUrl(mimeData->text());
}

QString Backend::clipboardText() const {
    const QClipboard *clipboard = QGuiApplication::clipboard();
    if (!clipboard)
        return {};

    const QMimeData *mimeData = clipboard->mimeData();
    return mimeData && mimeData->hasText() ? mimeData->text() : QString();
}

bool Backend::editorTextChanged() {
    if (m_loading || m_formattingTypography)
        return false;

    if (m_document)
        reapplyTypographyToChange();

    scheduleWordCount();
    const QString text = currentDocumentText();
    const bool baselineKnown = m_hasKnownFileContents
        || !m_fileUrl.isValid() || m_fileUrl.isEmpty();
    if (baselineKnown && text == m_lastKnownFileText) {
        setModified(false);
        clearRecovery();
        setStatus(m_hasKnownFileContents ? QStringLiteral("Saved") : QString());
    } else {
        setModified(true);
        setStatus(QStringLiteral("Unsaved"));
        scheduleRecovery();
    }
    return true;
}

// The emphasis span of the marker's kind that the caret sits in or against,
// in document positions, so a toggle with nothing selected can act on it.
QVariantMap Backend::emphasisSpanAt(int position, const QString &marker) const {
    using Kind = MarkdownHighlighter::InlineKind;
    Kind kind;
    if (marker == QStringLiteral("**"))
        kind = Kind::Bold;
    else if (marker == QStringLiteral("~~"))
        kind = Kind::Strikethrough;
    else if (marker == QStringLiteral("*"))
        kind = Kind::Italic;
    else
        return {};
    if (!m_document)
        return {};

    const QTextBlock block =
        m_document->findBlock(qBound(0, position, m_document->characterCount() - 1));
    if (!block.isValid() || block.userState() > 0 || block.previous().userState() > 0)
        return {};

    const int offset = position - block.position();
    const QList<MarkdownHighlighter::InlineMarkup> markup =
        MarkdownHighlighter::inlineMarkup(block.text());
    for (const auto &item : markup) {
        const int start = item.markers[0].start;
        const int end = item.markers[1].start + item.markers[1].length;
        if (item.kind != kind || offset < start || offset > end)
            continue;
        // The visible text ends before the chain of nested emphasis closing
        // with it, as the italic "*" inside "***word***".
        const int contentEnd = item.content.start + item.content.length;
        int textEnd = contentEnd;
        for (bool moved = true; moved;) {
            moved = false;
            for (const auto &nested : markup) {
                const auto &closing = nested.markers[1];
                if (closing.start >= item.content.start && closing.length > 0
                        && closing.start + closing.length == textEnd) {
                    textEnd = closing.start;
                    moved = true;
                }
            }
        }
        const int base = block.position();
        return {{QStringLiteral("start"), base + start},
                {QStringLiteral("end"), base + end},
                {QStringLiteral("contentStart"), base + item.content.start},
                {QStringLiteral("contentEnd"), base + contentEnd},
                {QStringLiteral("textEnd"), base + textEnd}};
    }
    return {};
}

// Steps one grapheme cluster from a caret position, so emoji and combining
// sequences are crossed whole instead of being split mid-cluster.
int Backend::cursorPositionStep(int position, int direction) const {
    if (!m_document || direction == 0)
        return position;

    const int documentLength = m_document->characterCount() - 1;
    position = qBound(0, position, documentLength);
    if (direction > 0 ? position >= documentLength : position <= 0)
        return direction > 0 ? documentLength : 0;

    const QTextBlock block = m_document->findBlock(position);
    if (!block.isValid())
        return position + (direction > 0 ? 1 : -1);

    const int blockStart = block.position();
    const int blockTextLength = block.text().size();
    // The paragraph separator is the next caret position past a block's text.
    if (direction > 0 ? position >= blockStart + blockTextLength : position <= blockStart)
        return position + (direction > 0 ? 1 : -1);

    // An unformatted layout over the block text is enough to step by clusters.
    QTextLayout layout(block.text());
    const int local = position - blockStart;
    const int stepped = direction > 0
        ? layout.nextCursorPosition(local, QTextLayout::SkipCharacters)
        : layout.previousCursorPosition(local, QTextLayout::SkipCharacters);
    return blockStart + qBound(0, stepped, blockTextLength);
}

QVariantList Backend::hiddenRangesAt(int position) const {
    QVariantList ranges;
    if (!m_document)
        return ranges;

    const QTextBlock block =
        m_document->findBlock(qBound(0, position, m_document->characterCount() - 1));
    if (!block.isValid())
        return ranges;

    const int lineStart = block.position();
    QList<QPair<int, int>> spans;
    const QList<MarkdownHighlighter::InlineMarkup> markup =
        MarkdownHighlighter::inlineMarkup(block.text());
    for (const MarkdownHighlighter::InlineMarkup &item : markup) {
        for (const MarkdownHighlighter::Span &marker : item.markers) {
            spans.append({lineStart + marker.start,
                          lineStart + marker.start + marker.length});
        }
    }
    std::sort(spans.begin(), spans.end());

    for (const auto &span : spans) {
        ranges.append(QVariantMap{{QStringLiteral("start"), span.first},
                                  {QStringLiteral("end"), span.second}});
    }
    return ranges;
}

QString Backend::linkUrlAt(int position) const {
    if (!m_document)
        return {};

    const int maxPosition = m_document->characterCount() - 1;
    const QTextBlock block =
        m_document->findBlock(qBound(0, position, maxPosition));
    if (!block.isValid())
        return {};

    return MarkdownHighlighter::linkUrlAt(block.text(), position - block.position());
}

QString Backend::imageUrlAt(int position) const {
    if (!m_document)
        return {};

    const int maxPosition = m_document->characterCount() - 1;
    const QTextBlock block =
        m_document->findBlock(qBound(0, position, maxPosition));
    if (!block.isValid())
        return {};

    return MarkdownHighlighter::imageUrlAt(block.text(), position - block.position());
}

QUrl Backend::resolveImageUrl(const QString &rawUrl) const {
    const QString trimmed = rawUrl.trimmed();
    if (trimmed.isEmpty())
        return {};

    if (QDir::isAbsolutePath(trimmed))
        return QUrl::fromLocalFile(QDir::cleanPath(trimmed));

    const QUrl parsed(trimmed);
    if (parsed.isValid() && !parsed.scheme().isEmpty()) {
        const QString scheme = parsed.scheme().toLower();
        if (scheme == QStringLiteral("http") || scheme == QStringLiteral("https")
                || scheme == QStringLiteral("file") || scheme == QStringLiteral("qrc")) {
            return parsed;
        }
        return {};
    }

    if (m_fileUrl.isLocalFile()) {
        const QFileInfo docInfo(m_fileUrl.toLocalFile());
        const QDir docDir = docInfo.dir();
        const QString resolvedPath = docDir.filePath(trimmed);
        return QUrl::fromLocalFile(resolvedPath);
    }

    return QUrl::fromLocalFile(defaultDirectory().filePath(trimmed));
}

void Backend::setSearchHighlight(const QString &query, int currentMatchStart) {
    if (m_highlighter)
        m_highlighter->setSearch(query, currentMatchStart);
}

void Backend::openExternalUrl(const QUrl &url) {
    const QString scheme = url.scheme().toLower();
    if (scheme == QStringLiteral("http") || scheme == QStringLiteral("https")
            || scheme == QStringLiteral("ftp") || scheme == QStringLiteral("mailto"))
        QDesktopServices::openUrl(url);
}

QVariantList Backend::documentHeadings() const {
    QVariantList list;
    if (!m_document)
        return list;

    static const QRegularExpression headingRe(
        QStringLiteral("^ {0,3}(#{1,6})[ \\t]+(.+?)(?:[ \\t]+#+[ \\t]*)?$"));
    QTextBlock block = m_document->firstBlock();
    int lineNumber = 1;
    QChar fenceChar;
    int fenceLength = 0;

    while (block.isValid()) {
        const QString text = block.text();
        int leadingSpaces = 0;
        while (leadingSpaces < text.length() && text.at(leadingSpaces) == QLatin1Char(' '))
            ++leadingSpaces;
        const QString trimmed = text.mid(leadingSpaces).trimmed();
        int markerLength = 0;
        if (leadingSpaces <= 3 && !trimmed.isEmpty()
                && (trimmed.front() == QLatin1Char('`') || trimmed.front() == QLatin1Char('~'))) {
            while (markerLength < trimmed.length()
                    && trimmed.at(markerLength) == trimmed.front()) {
                ++markerLength;
            }
        }
        if (fenceLength == 0 && markerLength >= 3) {
            fenceChar = trimmed.front();
            fenceLength = markerLength;
        } else if (fenceLength > 0 && !trimmed.isEmpty() && trimmed.front() == fenceChar
                   && markerLength >= fenceLength
                   && trimmed.mid(markerLength).trimmed().isEmpty()) {
            fenceLength = 0;
        }

        if (fenceLength == 0) {
            const QRegularExpressionMatch match = headingRe.match(text);
            if (match.hasMatch()) {
                const int level = match.captured(1).length();
                const QString title = match.captured(2).trimmed();
                list.append(QVariantMap{
                    {QStringLiteral("title"), title},
                    {QStringLiteral("level"), level},
                    {QStringLiteral("position"), block.position()},
                    {QStringLiteral("lineNumber"), lineNumber}
                });
            }
        }

        block = block.next();
        ++lineNumber;
    }

    return list;
}

// Renders the current Markdown source into a read-only Qt text document.
void Backend::renderPreview(QObject *textDocument) const {
    auto *quickDocument = qobject_cast<QQuickTextDocument *>(textDocument);
    if (!quickDocument || !quickDocument->textDocument())
        return;

    QTextDocument *preview = quickDocument->textDocument();
    if (m_document)
        preview->setDefaultFont(m_document->defaultFont());
    preview->setMarkdown(currentDocumentText());

    for (QTextBlock block = preview->firstBlock(); block.isValid(); block = block.next()) {
        QTextBlockFormat format = block.blockFormat();
        const bool heading = format.headingLevel() > 0;
        const bool compact = heading || block.textList()
            || format.intProperty(QTextFormat::BlockQuoteLevel) > 0;
        format.setTopMargin(heading ? 18 : 0);
        format.setBottomMargin(compact ? 8 : 14);
        QTextCursor(block).setBlockFormat(format);
    }
}

// Sets the monospace cell used by document indentation and heading outdents.
void Backend::setHeadingCellWidth(qreal width) {
    const qreal boundedWidth = qMax<qreal>(1.0, width);
    if (qFuzzyCompare(m_headingCellWidth, boundedWidth))
        return;
    m_headingCellWidth = boundedWidth;
    if (m_document)
        m_document->setIndentWidth(m_headingCellWidth);
}

QVariantMap Backend::windowGeometry() const {
    QSettings settings;
    int x = settings.value(QStringLiteral("window/x"), -1).toInt();
    int y = settings.value(QStringLiteral("window/y"), -1).toInt();
    const int width = qMax(720, settings.value(QStringLiteral("window/width"), 1280).toInt());
    const int height = qMax(520, settings.value(QStringLiteral("window/height"), 820).toInt());
    bool visible = x < 0 || y < 0;
    const QRect stored(x, y, width, height);
    for (const QScreen *screen : QGuiApplication::screens()) {
        if (screen && screen->availableGeometry().intersects(stored)) {
            visible = true;
            break;
        }
    }
    if (!visible) {
        x = -1;
        y = -1;
    }
    return {{QStringLiteral("x"), x},
            {QStringLiteral("y"), y},
            {QStringLiteral("width"), width},
            {QStringLiteral("height"), height},
            {QStringLiteral("maximized"), settings.value(QStringLiteral("window/maximized"), false)}};
}

void Backend::saveWindowGeometry(int x, int y, int width, int height, bool maximized) {
    QSettings settings;
    if (!maximized) {
        settings.setValue(QStringLiteral("window/x"), x);
        settings.setValue(QStringLiteral("window/y"), y);
        settings.setValue(QStringLiteral("window/width"), width);
        settings.setValue(QStringLiteral("window/height"), height);
    }
    settings.setValue(QStringLiteral("window/maximized"), maximized);
}

void Backend::toggleFocusMode() {
    m_focusMode = !m_focusMode;
    if (m_highlighter)
        m_highlighter->setFocusMode(m_focusMode);
    emit focusModeChanged();
}

void Backend::updateCursorPosition(int position) {
    if (m_highlighter)
        m_highlighter->setFocusCursorPosition(position);
}

void Backend::loadDocumentText(const QString &text) {
    if (!m_document) {
        setStatus(QStringLiteral("Could not attach the Markdown renderer."));
        return;
    }

    m_loading = true;
    m_document->setPlainText(text);
    m_loading = false;

    applyDocumentTypography();
    QTextCursor cursor(m_document);
    cursor.setPosition(0);
    m_wordCountTimer.stop();
    setWordCount(countWords(text), text);
}

void Backend::setFileUrl(const QUrl &url) {
    if (m_fileUrl == url)
        return;

    m_fileUrl = url;
    emit fileUrlChanged();
    watchCurrentFile();
    if (m_fileUrl.isLocalFile())
        applyFolder(QFileInfo(m_fileUrl.toLocalFile()).absolutePath(), true);
}

void Backend::setModified(bool modified) {
    if (m_modified == modified)
        return;

    m_modified = modified;
    emit modifiedChanged();
}

void Backend::setStatus(const QString &status) {
    if (m_status == status)
        return;

    m_status = status;
    emit statusChanged();
}

void Backend::saveTo(const QUrl &url) {
    if (!m_document) {
        setStatus(QStringLiteral("There is no document to save."));
        return;
    }
    if (!url.isLocalFile()) {
        setStatus(QStringLiteral("Only local files can be saved."));
        return;
    }

    const QString path = url.toLocalFile();
    const QString targetName = QFileInfo(path).fileName();
    const QByteArray contents = currentDocumentText().toUtf8();

    // QSaveFile commits by replacing the target. Stop watching the old inode
    // before that replacement so our own write is not classified as external.
    const QStringList watched = m_fileWatcher.files();
    if (!watched.isEmpty())
        m_fileWatcher.removePaths(watched);

    const AtomicWrite atomic = writeDocumentAtomically(path, contents);
    if (atomic == AtomicWrite::Failed
            || (atomic == AtomicWrite::NotOpened
                && !writeDocumentDirectly(path, contents))) {
        watchCurrentFile();
        setStatus(QStringLiteral("Could not save %1.").arg(targetName));
        return;
    }

    setKnownFileContents(contents, true);
    setFileUrl(url);
    watchCurrentFile();
    QSettings().setValue(lastSaveDirectorySetting,
                         QFileInfo(url.toLocalFile()).absolutePath());
    setModified(false);
    setStatus(QStringLiteral("Saved %1").arg(fileName()));
    clearRecovery();
    emit saveSucceeded();
}

void Backend::scheduleRecovery() {
    m_recoveryTimer.start();
}

QString Backend::recoveryPath() const {
    return m_recoveryPath;
}

void Backend::setKnownFileContents(const QByteArray &contents, bool known) {
    m_lastKnownFileContents = contents;
    m_hasKnownFileContents = known;
    m_lastKnownFileText = known
        ? QString::fromUtf8(contents).replace(QStringLiteral("\r\n"), QStringLiteral("\n"))
        : QString();
}

void Backend::writeRecovery() {
    if (!m_modified)
        return;
    const QString path = recoveryPath();
    if (path.isEmpty())
        return;
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return;
    const QJsonObject recovery{{QStringLiteral("version"), 1},
                               {QStringLiteral("savedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
                               {QStringLiteral("fileUrl"), m_fileUrl.toString()},
                               {QStringLiteral("text"), currentDocumentText()}};
    const QByteArray contents = QJsonDocument(recovery).toJson(QJsonDocument::Compact);
    if (file.write(contents) != contents.size() || !file.commit())
        qWarning() << "Could not write recovery snapshot" << path;
}

void Backend::restoreRecovery() {
    const QString path = recoveryPath();
    if (path.isEmpty())
        return;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return;
    const QJsonDocument json = QJsonDocument::fromJson(file.readAll());
    if (!json.isObject() || !json.object().value(QStringLiteral("text")).isString()) {
        file.close();
        QFile::remove(recoveryPath());
        setStatus(QStringLiteral("Discarded an invalid recovery snapshot"));
        return;
    }
    const QJsonObject recovery = json.object();
    loadDocumentText(recovery.value(QStringLiteral("text")).toString());
    const QUrl recoveredUrl(recovery.value(QStringLiteral("fileUrl")).toString());
    QFile diskFile(recoveredUrl.toLocalFile());
    if (recoveredUrl.isLocalFile() && diskFile.open(QIODevice::ReadOnly)) {
        setKnownFileContents(diskFile.readAll(), true);
    } else {
        setKnownFileContents(QByteArray(), false);
    }
    setFileUrl(recoveredUrl);
    setModified(true);
    setStatus(QStringLiteral("Recovered unsaved changes"));
}

void Backend::clearRecovery() {
    m_recoveryTimer.stop();
    const QString path = recoveryPath();
    if (!path.isEmpty())
        QFile::remove(path);
}

void Backend::watchCurrentFile() {
    const QStringList watched = m_fileWatcher.files();
    if (!watched.isEmpty())
        m_fileWatcher.removePaths(watched);
    if (m_fileUrl.isLocalFile() && QFileInfo::exists(m_fileUrl.toLocalFile()))
        m_fileWatcher.addPath(m_fileUrl.toLocalFile());
}

void Backend::applyFolder(const QString &path, bool remember) {
    const QDir directory = QDir(path).exists() ? QDir(path) : defaultDirectory();
    const QUrl folderUrl = QUrl::fromLocalFile(directory.absolutePath());
    if (m_folderUrl == folderUrl)
        return;

    m_folderUrl = folderUrl;
    watchCurrentFolder();
    if (remember)
        QSettings().setValue(browseDirectorySetting, directory.absolutePath());
    emit folderChanged();
}

void Backend::watchCurrentFolder() {
    const QStringList watched = m_folderWatcher.directories();
    if (!watched.isEmpty())
        m_folderWatcher.removePaths(watched);
    if (m_folderUrl.isLocalFile())
        m_folderWatcher.addPath(m_folderUrl.toLocalFile());
}

QString Backend::folderName() const {
    const QDir directory(m_folderUrl.toLocalFile());
    // The root directory has no name of its own; show its path instead.
    return directory.isRoot() ? directory.absolutePath() : directory.dirName();
}

bool Backend::folderHasParent() const {
    return !QDir(m_folderUrl.toLocalFile()).isRoot();
}

QVariantList Backend::folderEntries() const {
    static const QStringList markdownFilter{QStringLiteral("*.md"),
                                            QStringLiteral("*.markdown")};
    // AllDirs exempts folders from the name filter so an empty one can still
    // be walked into, while files stay narrowed to what Omawrite can open:
    // this is a view of a writing folder, not a file manager.
    QVariantList entries;
    const QFileInfoList infos = QDir(m_folderUrl.toLocalFile())
        .entryInfoList(markdownFilter, QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot,
                       QDir::DirsFirst | QDir::Name | QDir::IgnoreCase);
    entries.reserve(infos.size());
    for (const QFileInfo &info : infos) {
        entries.append(QVariantMap{
            {QStringLiteral("name"), info.fileName()},
            {QStringLiteral("url"), QUrl::fromLocalFile(info.absoluteFilePath())},
            {QStringLiteral("isDir"), info.isDir()}});
    }
    return entries;
}

void Backend::loadThemeConfig() {
    const QString configDir = configurationDirectory();
    const QString configPath = configDir + QStringLiteral("/config.toml");
    
    // Ensure the config directory exists so the user can populate it
    if (!QDir(configDir).exists()) {
        QDir().mkpath(configDir);
    }

    QString themeMode;
    QString background;
    QString foreground;
    QString accent;
    QString selection;
    QString codeBackground;
    QString configuredFontFamily = QStringLiteral("iA Writer Mono S");
    qreal configuredTextScale = m_systemTextScale;
    bool hasConfiguredTextScale = false;
    QFile file(configPath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        while (!in.atEnd()) {
            const QString line = in.readLine().trimmed();
            if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
                continue;

            const int equals = line.indexOf(QLatin1Char('='));
            if (equals < 0)
                continue;

            const QString key = line.left(equals).trimmed();
            const QString value = tomlValue(line.mid(equals + 1));

            if (key == QStringLiteral("background")) {
                if (QColor(value).isValid())
                    background = value;
            } else if (key == QStringLiteral("foreground")) {
                if (QColor(value).isValid())
                    foreground = value;
            } else if (key == QStringLiteral("accent")) {
                if (QColor(value).isValid())
                    accent = value;
            } else if (key == QStringLiteral("selection")) {
                if (QColor(value).isValid())
                    selection = value;
            } else if (key == QStringLiteral("code_background")
                       || key == QStringLiteral("lighter_background")) {
                if (QColor(value).isValid())
                    codeBackground = value;
            } else if (key == QStringLiteral("text_scale")) {
                bool ok;
                const qreal scale = value.toDouble(&ok);
                if (ok && scale >= 0.5 && scale <= 3.0) {
                    configuredTextScale = scale;
                    hasConfiguredTextScale = true;
                }
            } else if (key == QStringLiteral("font_family")) {
                if (!value.isEmpty())
                    configuredFontFamily = value;
            } else if (key == QStringLiteral("mode")) {
                themeMode = value;
            }
        }
    }

    m_configuredTextScale = configuredTextScale;
    m_hasConfiguredTextScale = hasConfiguredTextScale;
    applyTextScale(configuredTextScale);
    if (m_fontFamily != configuredFontFamily) {
        m_fontFamily = configuredFontFamily;
        emit fontFamilyChanged();
    }

    bool themeIsDark = m_systemDarkMode;
    if (themeMode == QStringLiteral("dark")) {
        themeIsDark = true;
    } else if (themeMode == QStringLiteral("light")) {
        themeIsDark = false;
    } else {
        const QColor parsedBackground(background);
        if (parsedBackground.isValid()) {
            const double luminance = 0.299 * parsedBackground.redF()
                + 0.587 * parsedBackground.greenF() + 0.114 * parsedBackground.blueF();
            themeIsDark = luminance < 0.5;
        }
    }

    m_themeBackground = themeIsDark ? QStringLiteral("#101010") : QStringLiteral("#ffffff");
    m_themeForeground = themeIsDark ? QStringLiteral("#eeeeee") : QStringLiteral("#222324");
    m_themeAccent = themeIsDark ? QStringLiteral("#5584aa") : QStringLiteral("#2077b2");
    m_themeSelection = themeIsDark ? QStringLiteral("#186a9a") : QStringLiteral("#2077b2");
    if (!background.isEmpty()) m_themeBackground = background;
    if (!foreground.isEmpty()) m_themeForeground = foreground;
    if (!accent.isEmpty()) m_themeAccent = accent;
    if (!selection.isEmpty()) m_themeSelection = selection;
    m_themeCodeBackground = codeBackground.isEmpty()
        ? blendedCodeBackground(m_themeBackground, m_themeForeground)
        : codeBackground;
    if (themeIsDark != m_darkMode) {
        m_darkMode = themeIsDark;
        emit darkModeChanged();
    }

    if (m_highlighter) {
        m_highlighter->setDarkMode(m_darkMode);
        m_highlighter->setColors(m_themeBackground, m_themeForeground, m_themeAccent,
                                 m_themeCodeBackground);
    }

    emit themeColorsChanged();
}

void Backend::watchThemeConfig() {
    const QStringList watched = m_themeWatcher.files() + m_themeWatcher.directories();
    if (!watched.isEmpty())
        m_themeWatcher.removePaths(watched);

    const QString configDir = configurationDirectory();
    const QString configPath = configDir + QStringLiteral("/config.toml");

    if (QDir(configDir).exists())
        m_themeWatcher.addPath(configDir);
    if (QFile::exists(configPath))
        m_themeWatcher.addPath(configPath);
}

QUrl Backend::suggestedSaveUrl() const {
    if (m_fileUrl.isLocalFile())
        return m_fileUrl;

    return QUrl::fromLocalFile(
        defaultDirectory().filePath(suggestedFileName(currentDocumentText())));
}

QDir Backend::defaultDirectory() const {
    const QString savedDirectory = QSettings().value(lastSaveDirectorySetting).toString();
    return savedDirectory.isEmpty() || !QDir(savedDirectory).exists()
        ? QDir::home()
        : QDir(savedDirectory);
}

QString Backend::currentDocumentText() const {
    return m_document ? m_document->toPlainText() : QString();
}

int Backend::countWords(const QString &text) {
    static const QRegularExpression cjkRe(
        QStringLiteral("(?=[\\p{L}\\p{N}])[\\p{Han}\\p{Hiragana}\\p{Katakana}]"));
    static const QRegularExpression wordRe(
        QStringLiteral("[\\p{L}\\p{N}]+(?:['-][\\p{L}\\p{N}]+)*"));
    int count = 0;
    QRegularExpressionMatchIterator cjkIt = cjkRe.globalMatch(text);
    while (cjkIt.hasNext()) {
        cjkIt.next();
        ++count;
    }
    QString nonCjkText = text;
    nonCjkText.replace(cjkRe, QStringLiteral(" "));
    QRegularExpressionMatchIterator it = wordRe.globalMatch(nonCjkText);
    while (it.hasNext()) {
        it.next();
        ++count;
    }
    return count;
}

// Extracts a scalar TOML value while discarding a trailing inline comment.
QString Backend::tomlValue(const QString &text) {
    const QString value = text.trimmed();
    if (value.isEmpty())
        return value;
    const QChar quote = value.front();
    if (quote == QLatin1Char('"') || quote == QLatin1Char('\'')) {
        bool escaped = false;
        for (int i = 1; i < value.length(); ++i) {
            if (quote == QLatin1Char('"') && !escaped && value.at(i) == QLatin1Char('\\')) {
                escaped = true;
            } else if (!escaped && value.at(i) == quote) {
                return value.mid(1, i - 1);
            } else {
                escaped = false;
            }
        }
        return value;
    }
    static const QRegularExpression commentRe(QStringLiteral("\\s+#"));
    const int comment = value.indexOf(commentRe);
    return comment < 0 ? value : value.left(comment).trimmed();
}

QString Backend::sanitizedEntryName(const QString &text) {
    QString name = text.section(QLatin1Char('\n'), 0, 0).trimmed();
    name.replace(QLatin1Char('\\'), QLatin1Char('-'));
    name.replace(QRegularExpression(QStringLiteral("[<>:\"/|?*\\x00-\\x1f\\x7f]")),
                 QStringLiteral("-"));
    name = name.left(120).trimmed();
    while (name.endsWith(QLatin1Char('.')) || name.endsWith(QLatin1Char(' ')))
        name.chop(1);
    if (name.isEmpty() || name == QStringLiteral(".") || name == QStringLiteral(".."))
        name = QStringLiteral("Untitled");
    static const QRegularExpression reservedWindowsName(
        QStringLiteral("^(?:con|prn|aux|nul|com[1-9]|lpt[1-9])(?:\\..*)?$"),
        QRegularExpression::CaseInsensitiveOption);
    if (reservedWindowsName.match(name).hasMatch())
        name.prepend(QLatin1Char('_'));
    return name;
}

QString Backend::suggestedFileName(const QString &text) {
    QString name = sanitizedEntryName(text);
    if (!name.endsWith(QStringLiteral(".md"), Qt::CaseInsensitive)
            && !name.endsWith(QStringLiteral(".markdown"), Qt::CaseInsensitive))
        name += QStringLiteral(".md");
    return name;
}

void Backend::setWordCount(int words, const QString &text) {
    const int characters = text.length();
    
    int paragraphs = 0;
    bool inParagraph = false;
    const QStringList lines = text.split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        const bool hasText = !line.trimmed().isEmpty();
        if (hasText && !inParagraph)
            ++paragraphs;
        inParagraph = hasText;
    }

    if (m_wordCount == words && m_characterCount == characters && m_paragraphCount == paragraphs)
        return;

    m_wordCount = words;
    m_characterCount = characters;
    m_paragraphCount = paragraphs;
    emit wordCountChanged();
}

void Backend::refreshWordCount() {
    const QString text = currentDocumentText();
    setWordCount(countWords(text), text);
    if (m_hasKnownFileContents && text == m_lastKnownFileText) {
        setModified(false);
        setStatus(QStringLiteral("Saved"));
        clearRecovery();
    }
}

void Backend::scheduleWordCount() {
    m_wordCountTimer.start();
}

// Updates the effective interface scale and emits only when it changes.
void Backend::applyTextScale(qreal textScale) {
    if (qFuzzyCompare(m_textScale, textScale))
        return;
    m_textScale = textScale;
    emit textScaleChanged();
}

void Backend::applyDocumentTypography() {
    if (!m_document)
        return;

    // A full pass is only used for freshly loaded/attached documents, so it is
    // safe to drop undo history here (re-enabling clears the stack anyway).
    const bool undoEnabled = m_document->isUndoRedoEnabled();
    m_document->setUndoRedoEnabled(false);

    m_formattingTypography = true;
    m_document->setIndentWidth(m_headingCellWidth);
    for (QTextBlock block = m_document->firstBlock(); block.isValid(); block = block.next())
        QTextCursor(block).mergeBlockFormat(blockFormatWithTypography(block));
    m_formattingTypography = false;

    m_document->setUndoRedoEnabled(undoEnabled);
}

void Backend::reapplyTypographyToChange() {
    if (!m_document)
        return;

    // Format only the block(s) touched by the last edit instead of the whole
    // document, and fold the change into the preceding edit command so a single
    // undo reverts both the text and its formatting.
    const int maxPos = m_document->characterCount() - 1;
    const int start = qBound(0, m_lastChangePos, maxPos);
    const int end = qBound(start, m_lastChangePos + m_lastChangeAdded, maxPos);

    // Opening or closing a fence changes which later blocks are code, so those
    // blocks need their heading indent recomputed too.
    bool touchesFence = false;
    for (QTextBlock block = m_document->findBlock(start);
         block.isValid() && block.position() <= end; block = block.next()) {
        const QString text = block.text();
        if (text.contains(QStringLiteral("```")) || text.contains(QStringLiteral("~~~"))) {
            touchesFence = true;
            break;
        }
    }

    m_formattingTypography = true;
    QTextCursor cursor(m_document);
    cursor.joinPreviousEditBlock();
    for (QTextBlock block = m_document->findBlock(start); block.isValid(); block = block.next()) {
        cursor.setPosition(block.position());
        cursor.mergeBlockFormat(blockFormatWithTypography(block));
        if (block.position() + block.length() > end && !touchesFence)
            break;
    }
    cursor.endEditBlock();
    m_formattingTypography = false;
}

// Builds the shared line-height and hanging heading-indent format for a block.
QTextBlockFormat Backend::blockFormatWithTypography(const QTextBlock &block) const {
    QTextBlockFormat format;
    format.setLineHeight(typoraLineHeightPercent, QTextBlockFormat::ProportionalHeight);
    // Lines inside fenced code are never headings (the highlighter marks them with state > 0).
    const MarkdownHighlighter::HeadingMarkup heading = block.userState() > 0
        ? MarkdownHighlighter::HeadingMarkup{}
        : MarkdownHighlighter::headingMarkup(block.text());
    format.setIndent(heading.level > 0 ? qMax(0, headingGutterCells - heading.prefixLength)
                                       : headingGutterCells);
    return format;
}

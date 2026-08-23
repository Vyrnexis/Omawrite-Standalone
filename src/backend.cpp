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
    m_highlighter->setColors(m_themeBackground, m_themeForeground, m_themeAccent);

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
        rendered.setDefaultFont(m_document->defaultFont());
        rendered.setMarkdown(currentDocumentText());
        rendered.print(&printer);
    }
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

    if (m_document) {
        const int blockCount = m_document->blockCount();
        if (blockCount > m_formattedBlockCount)
            reapplyTypographyToChange();
        m_formattedBlockCount = blockCount;
    }

    scheduleWordCount();
    setModified(true);
    setStatus(QStringLiteral("Unsaved"));
    scheduleRecovery();
    return true;
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

    const QString targetName = QFileInfo(url.toLocalFile()).fileName();
    QSaveFile file(url.toLocalFile());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        setStatus(QStringLiteral("Could not save %1.").arg(targetName));
        return;
    }

    const QByteArray contents = currentDocumentText().toUtf8();
    file.write(contents);

    // QSaveFile commits by replacing the target. Stop watching the old inode
    // before that replacement so our own write is not classified as external.
    const QStringList watched = m_fileWatcher.files();
    if (!watched.isEmpty())
        m_fileWatcher.removePaths(watched);

    // commit() flushes, fsyncs, and atomically renames the temp file into place,
    // returning false (and leaving the original untouched) on any write error.
    if (!file.commit()) {
        watchCurrentFile();
        setStatus(QStringLiteral("Could not write %1.").arg(targetName));
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
    m_themeBackground = m_systemDarkMode ? QStringLiteral("#101010") : QStringLiteral("#ffffff");
    m_themeForeground = m_systemDarkMode ? QStringLiteral("#eeeeee") : QStringLiteral("#222324");
    m_themeAccent = m_systemDarkMode ? QStringLiteral("#5584aa") : QStringLiteral("#2077b2");
    m_themeSelection = m_systemDarkMode ? QStringLiteral("#186a9a") : QStringLiteral("#2077b2");

    const QString configDir = configurationDirectory();
    const QString configPath = configDir + QStringLiteral("/config.toml");
    
    // Ensure the config directory exists so the user can populate it
    if (!QDir(configDir).exists()) {
        QDir().mkpath(configDir);
    }

    QString themeMode;
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
            QString value = line.mid(equals + 1).trimmed();
            if (value.size() >= 2
                    && ((value.front() == QLatin1Char('"') && value.back() == QLatin1Char('"'))
                        || (value.front() == QLatin1Char('\'') && value.back() == QLatin1Char('\'')))) {
                value = value.mid(1, value.size() - 2);
            }

            if (key == QStringLiteral("background")) {
                if (QColor(value).isValid())
                    m_themeBackground = value;
            } else if (key == QStringLiteral("foreground")) {
                if (QColor(value).isValid())
                    m_themeForeground = value;
            } else if (key == QStringLiteral("accent")) {
                if (QColor(value).isValid())
                    m_themeAccent = value;
            } else if (key == QStringLiteral("selection")) {
                if (QColor(value).isValid())
                    m_themeSelection = value;
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

    bool themeModeKnown = false;
    bool themeIsDark = m_systemDarkMode;
    if (themeMode == QStringLiteral("dark")) {
        themeIsDark = true;
        themeModeKnown = true;
    } else if (themeMode == QStringLiteral("light")) {
        themeIsDark = false;
        themeModeKnown = true;
    } else {
        const QColor background(m_themeBackground);
        if (background.isValid()) {
            const double luminance = 0.299 * background.redF()
                + 0.587 * background.greenF() + 0.114 * background.blueF();
            themeIsDark = luminance < 0.5;
            themeModeKnown = true;
        }
    }
    if (themeModeKnown && themeIsDark != m_darkMode) {
        m_darkMode = themeIsDark;
        emit darkModeChanged();
    }

    if (m_highlighter) {
        m_highlighter->setDarkMode(m_darkMode);
        m_highlighter->setColors(m_themeBackground, m_themeForeground, m_themeAccent);
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
    static const QRegularExpression wordRe(
        QStringLiteral("[\\p{L}\\p{N}]+(?:['-][\\p{L}\\p{N}]+)*"));
    int count = 0;
    QRegularExpressionMatchIterator it = wordRe.globalMatch(text);
    while (it.hasNext()) {
        it.next();
        ++count;
    }
    return count;
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

    QTextBlockFormat blockFormat;
    blockFormat.setLineHeight(typoraLineHeightPercent, QTextBlockFormat::ProportionalHeight);

    // A full pass is only used for freshly loaded/attached documents, so it is
    // safe to drop undo history here (re-enabling clears the stack anyway).
    const bool undoEnabled = m_document->isUndoRedoEnabled();
    m_document->setUndoRedoEnabled(false);

    m_formattingTypography = true;
    QTextCursor cursor(m_document);
    cursor.select(QTextCursor::Document);
    cursor.mergeBlockFormat(blockFormat);
    m_formattingTypography = false;

    m_document->setUndoRedoEnabled(undoEnabled);

    m_formattedBlockCount = m_document->blockCount();
}

void Backend::reapplyTypographyToChange() {
    if (!m_document)
        return;

    QTextBlockFormat blockFormat;
    blockFormat.setLineHeight(typoraLineHeightPercent, QTextBlockFormat::ProportionalHeight);

    // Format only the block(s) touched by the last edit instead of the whole
    // document, and fold the change into the preceding edit command so a single
    // undo reverts both the text and its formatting.
    const int maxPos = m_document->characterCount() - 1;
    const int start = qBound(0, m_lastChangePos, maxPos);
    const int end = qBound(start, m_lastChangePos + m_lastChangeAdded, maxPos);

    m_formattingTypography = true;
    QTextCursor cursor(m_document);
    cursor.joinPreviousEditBlock();
    cursor.setPosition(start);
    cursor.setPosition(end, QTextCursor::KeepAnchor);
    cursor.mergeBlockFormat(blockFormat);
    cursor.endEditBlock();
    m_formattingTypography = false;
}

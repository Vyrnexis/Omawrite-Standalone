#pragma once

#include <QObject>
#include <QPointer>
#include <QByteArray>
#include <QDir>
#include <QFileSystemWatcher>
#include <QString>
#include <QTimer>
#include <QUrl>
#include <QVariantList>
#include <memory>

class MarkdownHighlighter;
class QTextDocument;
class QWindow;
class QLockFile;

class Backend : public QObject {
    Q_OBJECT
    Q_PROPERTY(QUrl fileUrl READ fileUrl NOTIFY fileUrlChanged)
    Q_PROPERTY(QString fileName READ fileName NOTIFY fileUrlChanged)
    Q_PROPERTY(bool modified READ modified NOTIFY modifiedChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(int wordCount READ wordCount NOTIFY wordCountChanged)
    Q_PROPERTY(int characterCount READ characterCount NOTIFY wordCountChanged)
    Q_PROPERTY(int paragraphCount READ paragraphCount NOTIFY wordCountChanged)
    Q_PROPERTY(int readingTimeMinutes READ readingTimeMinutes NOTIFY wordCountChanged)
    Q_PROPERTY(bool darkMode READ darkMode WRITE setDarkMode NOTIFY darkModeChanged)
    Q_PROPERTY(qreal textScale READ textScale WRITE setTextScale NOTIFY textScaleChanged)
    Q_PROPERTY(int editorFontSize READ editorFontSize WRITE setEditorFontSize
               NOTIFY editorFontSizeChanged)
    Q_PROPERTY(QString fontFamily READ fontFamily NOTIFY fontFamilyChanged)
    Q_PROPERTY(QString themeBackground READ themeBackground NOTIFY themeColorsChanged)
    Q_PROPERTY(QString themeForeground READ themeForeground NOTIFY themeColorsChanged)
    Q_PROPERTY(QString themeAccent READ themeAccent NOTIFY themeColorsChanged)
    Q_PROPERTY(QString themeSelection READ themeSelection NOTIFY themeColorsChanged)
    Q_PROPERTY(bool focusMode READ focusMode NOTIFY focusModeChanged)
    Q_PROPERTY(QUrl folderUrl READ folderUrl NOTIFY folderChanged)
    Q_PROPERTY(QString folderName READ folderName NOTIFY folderChanged)
    Q_PROPERTY(bool folderHasParent READ folderHasParent NOTIFY folderChanged)
    Q_PROPERTY(QVariantList folderEntries READ folderEntries NOTIFY folderChanged)

public:
    explicit Backend(QObject *parent = nullptr);
    ~Backend() override;

    void setParentWindow(QWindow *window);
    void attachTextDocument(QTextDocument *document);

    QUrl fileUrl() const { return m_fileUrl; }
    QString fileName() const;

    bool modified() const { return m_modified; }
    QString status() const { return m_status; }
    int wordCount() const { return m_wordCount; }
    int characterCount() const { return m_characterCount; }
    int paragraphCount() const { return m_paragraphCount; }
    int readingTimeMinutes() const { return (m_wordCount + 199) / 200; }
    bool darkMode() const { return m_darkMode; }
    void setDarkMode(bool darkMode);
    qreal textScale() const { return m_textScale; }
    void setTextScale(qreal textScale);
    int editorFontSize() const { return m_editorFontSize; }
    void setEditorFontSize(int editorFontSize);
    void setSystemTextScale(qreal textScale);
    QString fontFamily() const { return m_fontFamily; }
    QString themeBackground() const { return m_themeBackground; }
    QString themeForeground() const { return m_themeForeground; }
    QString themeAccent() const { return m_themeAccent; }
    QString themeSelection() const { return m_themeSelection; }
    bool focusMode() const { return m_focusMode; }
    QUrl folderUrl() const { return m_folderUrl; }
    QString folderName() const;
    bool folderHasParent() const;
    QVariantList folderEntries() const;
    static int countWords(const QString &text);
    static QString normalizedLinkUrl(const QString &clipboardText);
    static QString suggestedFileName(const QString &text);
    static QString sanitizedEntryName(const QString &text);
    static QString tomlValue(const QString &text);

    Q_INVOKABLE void attachDocument(QObject *textDocument);
    Q_INVOKABLE void openDialog();
    Q_INVOKABLE void setFolder(const QUrl &url);
    Q_INVOKABLE void openParentFolder();
    Q_INVOKABLE QUrl createDocument(const QString &name);
    Q_INVOKABLE QUrl createFolder(const QString &name);
    Q_INVOKABLE int sidebarWidth() const;
    Q_INVOKABLE void saveSidebarWidth(int width);
    Q_INVOKABLE void open(const QUrl &url);
    Q_INVOKABLE void save();
    Q_INVOKABLE void saveAsDialog();
    Q_INVOKABLE void saveAs(const QUrl &url);
    Q_INVOKABLE void discardRecovery();
    Q_INVOKABLE void reloadFromDisk();
    Q_INVOKABLE void keepExternalVersion();
    Q_INVOKABLE void printDocument();
    Q_INVOKABLE void newWindow();
    Q_INVOKABLE QString clipboardUrl() const;
    Q_INVOKABLE QString clipboardText() const;
    Q_INVOKABLE bool editorTextChanged();
    Q_INVOKABLE QVariantList hiddenRangesAt(int position) const;
    Q_INVOKABLE QString linkUrlAt(int position) const;
    Q_INVOKABLE QString imageUrlAt(int position) const;
    Q_INVOKABLE QUrl resolveImageUrl(const QString &rawUrl) const;
    Q_INVOKABLE void setSearchHighlight(const QString &query, int currentMatchStart);
    Q_INVOKABLE void openExternalUrl(const QUrl &url);
    Q_INVOKABLE QVariantMap windowGeometry() const;
    Q_INVOKABLE void saveWindowGeometry(int x, int y, int width, int height, bool maximized);
    Q_INVOKABLE void resetTextScale();
    Q_INVOKABLE void resetEditorFontSize();
    Q_INVOKABLE void toggleFocusMode();
    Q_INVOKABLE void updateCursorPosition(int position);
    Q_INVOKABLE QVariantList documentHeadings() const;

signals:
    void fileUrlChanged();
    void modifiedChanged();
    void statusChanged();
    void wordCountChanged();
    void darkModeChanged();
    void textScaleChanged();
    void editorFontSizeChanged();
    void fontFamilyChanged();
    void themeColorsChanged();
    void openDialogRequested();
    void saveDialogRequested(const QUrl &suggestedUrl);
    void saveSucceeded();
    void externalChangeDetected(bool deleted, bool locallyModified);
    void focusModeChanged();
    void folderChanged();

private:
    void loadDocumentText(const QString &text);
    void setFileUrl(const QUrl &url);
    void setModified(bool modified);
    void setStatus(const QString &status);
    void saveTo(const QUrl &url);
    QUrl suggestedSaveUrl() const;
    QDir defaultDirectory() const;
    void applyFolder(const QString &path, bool remember);
    void watchCurrentFolder();
    QString currentDocumentText() const;
    void setWordCount(int words, const QString &text);
    void refreshWordCount();
    void scheduleWordCount();
    void applyDocumentTypography();
    void reapplyTypographyToChange();
    void scheduleRecovery();
    void writeRecovery();
    void restoreRecovery();
    void clearRecovery();
    QString recoveryPath() const;
    void setKnownFileContents(const QByteArray &contents, bool known);
    void watchCurrentFile();
    void loadThemeConfig();
    void watchThemeConfig();
    void applyTextScale(qreal textScale);

    QUrl m_fileUrl;
    bool m_modified = false;
    QString m_status;
    int m_wordCount = 0;
    int m_characterCount = 0;
    int m_paragraphCount = 0;
    bool m_darkMode = true;
    bool m_systemDarkMode = true;
    qreal m_textScale = 1.0;
    int m_editorFontSize = 20;
    qreal m_systemTextScale = 1.0;
    qreal m_configuredTextScale = 1.0;
    bool m_hasConfiguredTextScale = false;
    QString m_fontFamily = QStringLiteral("iA Writer Mono S");
    bool m_loading = false;
    bool m_formattingTypography = false;
    int m_formattedBlockCount = 0;
    int m_lastChangePos = 0;
    int m_lastChangeAdded = 0;
    QTimer m_wordCountTimer;
    QTimer m_recoveryTimer;
    QFileSystemWatcher m_fileWatcher;
    QUrl m_folderUrl;
    QFileSystemWatcher m_folderWatcher;
    QPointer<QTextDocument> m_document;
    QPointer<QWindow> m_parentWindow;
    QPointer<MarkdownHighlighter> m_highlighter;
    QByteArray m_lastKnownFileContents;
    QString m_lastKnownFileText;
    bool m_hasKnownFileContents = false;
    QString m_recoveryPath;
    std::unique_ptr<QLockFile> m_recoveryLock;
    bool m_focusMode = false;

    QString m_themeBackground;
    QString m_themeForeground;
    QString m_themeAccent;
    QString m_themeSelection;
    QFileSystemWatcher m_themeWatcher;
};

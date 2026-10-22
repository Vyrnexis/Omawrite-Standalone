#pragma once

#include <QRegularExpression>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>

class MarkdownHighlighter : public QSyntaxHighlighter {
    Q_OBJECT

public:
    explicit MarkdownHighlighter(QTextDocument *document);

    void setDarkMode(bool darkMode);
    void setColors(const QString &background, const QString &foreground, const QString &accent,
                   const QString &codeBackground);
    void setSearch(const QString &query, int currentMatchStart);
    void setFocusMode(bool enabled);
    void setFocusCursorPosition(int position);

    void applyFormat(int start, int count, const QTextCharFormat &format) {
        setFormat(start, count, format);
    }

    struct Span {
        int start;
        int length;
    };

    enum class InlineKind { Bold, Italic, Strikethrough, Link, Image, CodeBlockMarker };

    struct InlineMarkup {
        InlineKind kind;
        Span content;
        Span markers[2];
        Span destination{};
    };

    struct HeadingMarkup {
        int level = 0;
        int prefixLength = 0;
    };

    // Single source of truth for inline markdown spans: the highlighter uses it
    // to style content and hide markers, and the editor uses it (via
    // Backend::hiddenRangesAt) to skip the caret over the hidden markers.
    static QList<InlineMarkup> inlineMarkup(const QString &text);
    static HeadingMarkup headingMarkup(const QString &text);
    static QString linkUrlAt(const QString &text, int position);
    static QString imageUrlAt(const QString &text, int position);

    // Where query occurs in text, ignoring case and, for the query's plain
    // Latin and Greek letters, diacritics: "tara" finds "țară". An accented
    // letter in the query matches only itself, though ş and ș, ţ and ț count
    // as one. Folding keeps one UTF-16 unit per unit, so offsets and lengths
    // are the text's own.
    static QList<int> findMatches(const QString &text, const QString &query);

protected:
    void highlightBlock(const QString &text) override;

private:
    void rebuildFormats();
    void highlightMarkers(const QString &text);
    void highlightInline(const QString &text);
    void highlightSearch(const QString &text);
    void applyFocusDimming(const QString &text);

    bool m_darkMode = true;
    QString m_customBackground;
    QString m_customForeground;
    QString m_customAccent;
    QString m_customCodeBackground;
    QTextCharFormat m_markerFormat;
    QTextCharFormat m_hiddenMarkerFormat;
    QTextCharFormat m_headingFormat;
    QTextCharFormat m_boldFormat;
    QTextCharFormat m_italicFormat;
    QTextCharFormat m_strikethroughFormat;
    QTextCharFormat m_codeFormat;
    QTextCharFormat m_blockCodeFormat;
    QTextCharFormat m_keywordFormat;
    QTextCharFormat m_stringFormat;
    QTextCharFormat m_commentFormat;
    QTextCharFormat m_invisibleTextFormat;
    QTextCharFormat m_codeLanguageLabelFormat;
    QTextCharFormat m_quoteFormat;
    QTextCharFormat m_linkFormat;
    QString m_searchQuery;
    int m_currentMatchStart = -1;
    QTextCharFormat m_searchFormat;
    QTextCharFormat m_currentSearchFormat;
    bool m_focusMode = false;
    int m_focusCursorPosition = -1;
    QColor m_dimmedColor;
};

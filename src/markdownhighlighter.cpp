#include "markdownhighlighter.h"
#include "codehighlighter.h"

#include <QColor>
#include <QFont>
#include <QFontMetricsF>
#include <QTextDocument>

MarkdownHighlighter::MarkdownHighlighter(QTextDocument *document)
    : QSyntaxHighlighter(document) {
    rebuildFormats();
}

void MarkdownHighlighter::setDarkMode(bool darkMode) {
    if (m_darkMode == darkMode)
        return;

    m_darkMode = darkMode;
    rebuildFormats();
    rehighlight();
}

void MarkdownHighlighter::setColors(const QString &background, const QString &foreground,
                                    const QString &accent) {
    if (m_customBackground == background && m_customForeground == foreground
            && m_customAccent == accent)
        return;

    m_customBackground = background;
    m_customForeground = foreground;
    m_customAccent = accent;
    rebuildFormats();
    rehighlight();
}

void MarkdownHighlighter::setSearch(const QString &query, int currentMatchStart) {
    if (m_searchQuery == query && m_currentMatchStart == currentMatchStart)
        return;
    m_searchQuery = query;
    m_currentMatchStart = currentMatchStart;
    rehighlight();
}

void MarkdownHighlighter::rebuildFormats() {
    const QColor marker = m_darkMode ? QColor(QStringLiteral("#4f525a"))
                                     : QColor(QStringLiteral("#aeb1b5"));
    const QColor background = !m_customBackground.isEmpty() ? QColor(m_customBackground)
        : (m_darkMode ? QColor(QStringLiteral("#101010")) : QColor(QStringLiteral("#ffffff")));
    const QColor text = !m_customForeground.isEmpty() ? QColor(m_customForeground)
        : (m_darkMode ? QColor(QStringLiteral("#eeeeee")) : QColor(QStringLiteral("#222324")));
    const QColor link = !m_customAccent.isEmpty() ? QColor(m_customAccent)
        : (m_darkMode ? QColor(QStringLiteral("#5584aa")) : QColor(QStringLiteral("#2077b2")));
    const QColor quote = marker;
    const QColor inlineCodeBackground = m_darkMode ? QColor(QStringLiteral("#1c1a1a"))
                                                   : QColor(QStringLiteral("#f8f8f8"));
    const QColor codeBackground = background;

    m_markerFormat = QTextCharFormat();
    m_markerFormat.setForeground(marker);

    // A sub-pixel font size combined with a stretch factor used to make these
    // markers occupy (close to) zero space, but that combination deadlocks Qt's
    // font metrics engine on some platforms. Instead, use a normal font size and
    // cancel out its advance width with negative absolute letter-spacing.
    m_hiddenMarkerFormat = QTextCharFormat();
    m_hiddenMarkerFormat.setForeground(background);
    m_hiddenMarkerFormat.setFontPointSize(1.0);

    QFont hiddenFont = document() ? document()->defaultFont() : QFont();
    hiddenFont.setPointSizeF(1.0);
    const qreal charWidth = QFontMetricsF(hiddenFont).horizontalAdvance(QLatin1Char('['));

    m_hiddenMarkerFormat.setFontLetterSpacingType(QFont::AbsoluteSpacing);
    m_hiddenMarkerFormat.setFontLetterSpacing(-charWidth);

    m_headingFormat = QTextCharFormat();
    m_headingFormat.setForeground(text);
    m_headingFormat.setFontWeight(QFont::Bold);

    m_boldFormat = QTextCharFormat();
    m_boldFormat.setFontWeight(QFont::Bold);
    m_boldFormat.setForeground(text);

    m_italicFormat = QTextCharFormat();
    m_italicFormat.setFontItalic(true);
    m_italicFormat.setForeground(text);

    const QColor keyword = m_darkMode ? QColor(QStringLiteral("#c678dd")) : QColor(QStringLiteral("#a626a4"));
    const QColor string = m_darkMode ? QColor(QStringLiteral("#98c379")) : QColor(QStringLiteral("#50a14f"));
    const QColor comment = m_darkMode ? QColor(QStringLiteral("#5c6370")) : QColor(QStringLiteral("#a0a1a7"));

    m_codeFormat = QTextCharFormat();
    m_codeFormat.setForeground(text);
    m_codeFormat.setBackground(inlineCodeBackground);

    m_blockCodeFormat = QTextCharFormat();
    m_blockCodeFormat.setForeground(text);
    m_blockCodeFormat.setBackground(codeBackground);
    m_blockCodeFormat.setProperty(QTextFormat::FullWidthSelection, true);

    m_keywordFormat = QTextCharFormat();
    m_keywordFormat.setForeground(keyword);
    m_keywordFormat.setBackground(codeBackground);
    m_keywordFormat.setFontWeight(QFont::Bold);

    m_stringFormat = QTextCharFormat();
    m_stringFormat.setForeground(string);
    m_stringFormat.setBackground(codeBackground);

    m_commentFormat = QTextCharFormat();
    m_commentFormat.setForeground(comment);
    m_commentFormat.setBackground(codeBackground);
    m_commentFormat.setFontItalic(true);

    m_invisibleTextFormat = QTextCharFormat();
    m_invisibleTextFormat.setForeground(codeBackground);
    m_invisibleTextFormat.setBackground(codeBackground);
    m_invisibleTextFormat.setProperty(QTextFormat::FullWidthSelection, true);

    m_codeLanguageLabelFormat = QTextCharFormat();
    m_codeLanguageLabelFormat.setForeground(text);
    m_codeLanguageLabelFormat.setBackground(codeBackground);
    m_codeLanguageLabelFormat.setFontWeight(QFont::Bold);

    m_quoteFormat = QTextCharFormat();
    m_quoteFormat.setForeground(quote);
    m_quoteFormat.setFontItalic(true);

    m_linkFormat = QTextCharFormat();
    m_linkFormat.setForeground(link);
    m_linkFormat.setFontUnderline(true);

    m_searchFormat = QTextCharFormat();
    m_searchFormat.setBackground(m_darkMode ? QColor(QStringLiteral("#725b18"))
                                            : QColor(QStringLiteral("#ffe58a")));
    m_currentSearchFormat = QTextCharFormat();
    m_currentSearchFormat.setBackground(m_darkMode ? QColor(QStringLiteral("#b36b20"))
                                                   : QColor(QStringLiteral("#ffad42")));
}

void MarkdownHighlighter::highlightBlock(const QString &text) {
    int state = previousBlockState();
    if (state == -1) state = 0;

    static const QRegularExpression codeBlockStartRe(QStringLiteral("^```\\s*(\\w*)"));
    if (state == 0) {
        QRegularExpressionMatch match = codeBlockStartRe.match(text);
        if (match.hasMatch()) {
            QString lang = match.captured(1).toLower();
            if (lang == QLatin1String("bash") || lang == QLatin1String("sh")) {
                setCurrentBlockState(2);
            } else if (lang == QLatin1String("nim")) {
                setCurrentBlockState(3);
            } else if (lang == QLatin1String("python") || lang == QLatin1String("py")) {
                setCurrentBlockState(4);
            } else if (lang == QLatin1String("toml")) {
                setCurrentBlockState(5);
            } else if (lang == QLatin1String("json")) {
                setCurrentBlockState(6);
            } else if (lang == QLatin1String("c")) {
                setCurrentBlockState(7);
            } else if (lang == QLatin1String("cpp") || lang == QLatin1String("c++")) {
                setCurrentBlockState(8);
            } else if (lang == QLatin1String("js") || lang == QLatin1String("javascript")) {
                setCurrentBlockState(9);
            } else if (lang == QLatin1String("ruby") || lang == QLatin1String("rb")) {
                setCurrentBlockState(10);
            } else {
                setCurrentBlockState(1);
            }
            setFormat(0, text.length() + 1, m_blockCodeFormat);
            
            // Hide the '```'
            setFormat(0, 3, m_hiddenMarkerFormat);
            // Format the language tag if present
            if (match.capturedLength(1) > 0) {
                setFormat(match.capturedStart(1), match.capturedLength(1), m_codeLanguageLabelFormat);
            }
            
            return;
        }
    } else if (state > 0) {
        if (text.startsWith(QStringLiteral("```"))) {
            setCurrentBlockState(0);
            setFormat(0, text.length() + 1, m_blockCodeFormat);
            setFormat(0, text.length(), m_invisibleTextFormat);
            return;
        } else {
            setCurrentBlockState(state);
            setFormat(0, text.length() + 1, m_blockCodeFormat);
            CodeHighlighter::Formats formats = {m_keywordFormat, m_stringFormat, m_commentFormat};
            CodeHighlighter::highlight(text, state, this, formats);
        }
        highlightSearch(text);
        return;
    }

    setCurrentBlockState(0);
    if (!text.isEmpty()) {
        highlightMarkers(text);
        if (text.contains(QLatin1Char('`')) || text.contains(QLatin1Char('*'))
            || text.contains(QLatin1Char('_')) || text.contains(QLatin1Char('['))) {
            highlightInline(text);
        }
    }
    highlightSearch(text);
}

void MarkdownHighlighter::highlightSearch(const QString &text) {
    if (m_searchQuery.isEmpty())
        return;

    int from = 0;
    while ((from = text.indexOf(m_searchQuery, from, Qt::CaseInsensitive)) >= 0) {
        const int documentStart = currentBlock().position() + from;
        QTextCharFormat format = this->format(from);
        format.setBackground(documentStart == m_currentMatchStart
                                 ? m_currentSearchFormat.background()
                                 : m_searchFormat.background());
        setFormat(from, m_searchQuery.length(), format);
        from += qMax(1, m_searchQuery.length());
    }
}

void MarkdownHighlighter::highlightMarkers(const QString &text) {
    int first = 0;
    while (first < text.length() && text.at(first).isSpace())
        ++first;
    if (first >= text.length())
        return;

    const QChar firstChar = text.at(first);
    if (first == 0 && firstChar == QLatin1Char('#')) {
        static const QRegularExpression headingRe(QStringLiteral("^(#{1,6})(\\s+)(.*)$"));
        const QRegularExpressionMatch heading = headingRe.match(text);
        if (heading.hasMatch()) {
            setFormat(0, heading.capturedLength(1) + heading.capturedLength(2),
                      m_markerFormat);
            setFormat(heading.capturedStart(3), heading.capturedLength(3),
                      m_headingFormat);
            return;
        }
    }

    if (firstChar == QLatin1Char('>')) {
        static const QRegularExpression quoteRe(QStringLiteral("^(\\s*>+\\s?)(.*)$"));
        const QRegularExpressionMatch quote = quoteRe.match(text);
        if (quote.hasMatch()) {
            setFormat(0, quote.capturedLength(1), m_markerFormat);
            setFormat(quote.capturedStart(2), quote.capturedLength(2), m_quoteFormat);
        }
    }

    if (firstChar == QLatin1Char('-') || firstChar == QLatin1Char('+')
            || firstChar == QLatin1Char('*') || firstChar.isDigit()) {
        static const QRegularExpression listRe(
            QStringLiteral("^(\\s*(?:[-+*]|\\d+[.)])\\s+)(.*)$"));
        const QRegularExpressionMatch list = listRe.match(text);
        if (list.hasMatch())
            setFormat(0, list.capturedLength(1), m_markerFormat);
    }

    if (firstChar == QLatin1Char('-') || firstChar == QLatin1Char('*')
            || firstChar == QLatin1Char('_')) {
        static const QRegularExpression ruleRe(QStringLiteral("^\\s{0,3}([-*_])(?:\\s*\\1){2,}\\s*$"));
        const QRegularExpressionMatch rule = ruleRe.match(text);
        if (rule.hasMatch())
            setFormat(0, text.length(), m_markerFormat);
    }
}

void MarkdownHighlighter::highlightInline(const QString &text) {
    if (text.contains(QLatin1Char('`'))) {
        static const QRegularExpression codeRe(QStringLiteral("`([^`]+)`"));
        QRegularExpressionMatchIterator codeMatches = codeRe.globalMatch(text);
        while (codeMatches.hasNext()) {
            const QRegularExpressionMatch match = codeMatches.next();
            setFormat(match.capturedStart(0), match.capturedLength(0), m_codeFormat);
        }
    }

    const QList<InlineMarkup> markup = inlineMarkup(text);
    for (const InlineMarkup &item : markup) {
        const QTextCharFormat &contentFormat =
            item.kind == InlineKind::Bold ? m_boldFormat
            : item.kind == InlineKind::Italic ? m_italicFormat
                                              : m_linkFormat;
        setFormat(item.content.start, item.content.length, contentFormat);
        for (const Span &marker : item.markers)
            setFormat(marker.start, marker.length, m_hiddenMarkerFormat);
    }
}

QList<MarkdownHighlighter::InlineMarkup> MarkdownHighlighter::inlineMarkup(const QString &text) {
    QList<InlineMarkup> markup;
    if (text.startsWith(QStringLiteral("```"))) {
        QString trimmed = text.trimmed();
        if (trimmed == QStringLiteral("```")) {
            markup.append({InlineKind::CodeBlockMarker, {0, 0}, {{0, static_cast<int>(text.length())}, {0, 0}}});
        } else {
            markup.append({InlineKind::CodeBlockMarker, {3, static_cast<int>(text.length()) - 3}, {{0, 3}, {0, 0}}});
        }
    }

    if (!text.contains(QLatin1Char('*')) && !text.contains(QLatin1Char('_'))
            && !text.contains(QLatin1Char('['))) {
        return markup;
    }

    const auto span = [](const QRegularExpressionMatch &match, int group) {
        return Span{int(match.capturedStart(group)), int(match.capturedLength(group))};
    };

    static const QRegularExpression boldRe(QStringLiteral("(\\*\\*|__)(.+?)(\\1)"));
    QRegularExpressionMatchIterator boldMatches = boldRe.globalMatch(text);
    while (boldMatches.hasNext()) {
        const QRegularExpressionMatch match = boldMatches.next();
        markup.append({InlineKind::Bold, span(match, 2),
                       {span(match, 1), span(match, 3)}});
    }

    static const QRegularExpression italicRe(
        QStringLiteral("(?<!\\*)\\*([^*\\n]+)\\*(?!\\*)|(?<!_)_([^_\\n]+)_(?!_)"));
    QRegularExpressionMatchIterator italicMatches = italicRe.globalMatch(text);
    while (italicMatches.hasNext()) {
        const QRegularExpressionMatch match = italicMatches.next();
        const Span whole = span(match, 0);
        const int contentIndex = match.capturedStart(1) >= 0 ? 1 : 2;
        markup.append({InlineKind::Italic, span(match, contentIndex),
                       {{whole.start, 1}, {whole.start + whole.length - 1, 1}}});
    }

    static const QRegularExpression linkRe(
        QStringLiteral("\\[([^\\]]+)\\]\\(((?:\\\\.|[^)])+)\\)"));
    QRegularExpressionMatchIterator linkMatches = linkRe.globalMatch(text);
    while (linkMatches.hasNext()) {
        const QRegularExpressionMatch match = linkMatches.next();
        const Span whole = span(match, 0);
        const Span content = span(match, 1);
        const int contentEnd = content.start + content.length;
        markup.append({InlineKind::Link, content,
                       {{whole.start, 1},
                        {contentEnd, whole.start + whole.length - contentEnd}},
                       span(match, 2)});
    }

    return markup;
}

QString MarkdownHighlighter::linkUrlAt(const QString &text, int position) {
    if (position < 0 || position >= text.length())
        return {};

    const QList<InlineMarkup> markup = inlineMarkup(text);
    for (const InlineMarkup &item : markup) {
        if (item.kind != InlineKind::Link
                || position < item.content.start
                || position >= item.content.start + item.content.length) {
            continue;
        }
        return text.mid(item.destination.start, item.destination.length);
    }
    return {};
}



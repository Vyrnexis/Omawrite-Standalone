#include "markdownhighlighter.h"
#include "codehighlighter.h"

#include <QColor>
#include <QFont>
#include <QFontMetricsF>
#include <QTextDocument>

namespace {
// Removes Markdown backslash escapes from a parsed link destination.
QString unescapeDestination(const QString &value) {
    QString result;
    result.reserve(value.length());
    bool escaped = false;
    for (const QChar ch : value) {
        if (escaped) {
            result.append(ch);
            escaped = false;
        } else if (ch == QLatin1Char('\\')) {
            escaped = true;
        } else {
            result.append(ch);
        }
    }
    if (escaped)
        result.append(QLatin1Char('\\'));
    return result;
}
}

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
                                    const QString &accent, const QString &codeBackground) {
    if (m_customBackground == background && m_customForeground == foreground
            && m_customAccent == accent && m_customCodeBackground == codeBackground)
        return;

    m_customBackground = background;
    m_customForeground = foreground;
    m_customAccent = accent;
    m_customCodeBackground = codeBackground;
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

void MarkdownHighlighter::setFocusMode(bool enabled) {
    if (m_focusMode == enabled)
        return;
    m_focusMode = enabled;
    rehighlight();
}

void MarkdownHighlighter::setFocusCursorPosition(int position) {
    if (position == m_focusCursorPosition)
        return;
    QTextDocument *doc = document();
    if (!doc) {
        m_focusCursorPosition = position;
        return;
    }
    QTextBlock oldBlock = doc->findBlock(m_focusCursorPosition);
    QTextBlock newBlock = doc->findBlock(position);
    m_focusCursorPosition = position;
    if (!m_focusMode)
        return;
    if (oldBlock != newBlock) {
        if (oldBlock.isValid())
            rehighlightBlock(oldBlock);
        if (newBlock.isValid())
            rehighlightBlock(newBlock);
    }
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
    const QColor codeBackground = !m_customCodeBackground.isEmpty()
        ? QColor(m_customCodeBackground)
        : background;

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

    m_strikethroughFormat = QTextCharFormat();
    m_strikethroughFormat.setFontStrikeOut(true);
    m_strikethroughFormat.setForeground(text);

    const QColor keyword = m_darkMode ? QColor(QStringLiteral("#c678dd")) : QColor(QStringLiteral("#a626a4"));
    const QColor string = m_darkMode ? QColor(QStringLiteral("#98c379")) : QColor(QStringLiteral("#50a14f"));
    const QColor comment = m_darkMode ? QColor(QStringLiteral("#5c6370")) : QColor(QStringLiteral("#a0a1a7"));

    m_codeFormat = QTextCharFormat();
    m_codeFormat.setForeground(text);
    m_codeFormat.setBackground(codeBackground);

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

    m_dimmedColor = QColor();
    m_dimmedColor.setRedF(background.redF() * 0.7 + text.redF() * 0.3);
    m_dimmedColor.setGreenF(background.greenF() * 0.7 + text.greenF() * 0.3);
    m_dimmedColor.setBlueF(background.blueF() * 0.7 + text.blueF() * 0.3);
}

void MarkdownHighlighter::highlightBlock(const QString &text) {
    int state = previousBlockState();
    if (state == -1) state = 0;

    static const QRegularExpression codeBlockStartRe(
        QStringLiteral("^ {0,3}(`{3,}|~{3,})[ \\t]*([^ \\t]*)"));
    if (state == 0) {
        const QRegularExpressionMatch match = codeBlockStartRe.match(text);
        if (match.hasMatch()) {
            const QString fence = match.captured(1);
            const int languageState = CodeHighlighter::languageStateForName(match.captured(2));
            const int fenceKind = fence.startsWith(QLatin1Char('~')) ? 100 : 0;
            setCurrentBlockState(languageState + fenceKind + qMin(99, fence.length()) * 1000);
            setFormat(0, text.length() + 1, m_blockCodeFormat);
            
            // Hide the '```'
            setFormat(match.capturedStart(1), fence.length(), m_hiddenMarkerFormat);
            // Format the language tag if present
            if (match.capturedLength(2) > 0) {
                setFormat(match.capturedStart(2), match.capturedLength(2), m_codeLanguageLabelFormat);
            }
            applyFocusDimming(text);
            highlightSearch(text);
            return;
        }
    } else if (state > 0) {
        const int fenceLength = state / 1000;
        const bool tildeFence = ((state / 100) % 10) == 1;
        const QChar fenceChar = tildeFence ? QLatin1Char('~') : QLatin1Char('`');
        const QString trimmed = text.trimmed();
        int closingLength = 0;
        while (closingLength < trimmed.length() && trimmed.at(closingLength) == fenceChar)
            ++closingLength;
        const bool closesFence = closingLength >= fenceLength
            && trimmed.mid(closingLength).trimmed().isEmpty();
        if (closesFence) {
            setCurrentBlockState(0);
            setFormat(0, text.length() + 1, m_blockCodeFormat);
            setFormat(0, text.length(), m_invisibleTextFormat);
            applyFocusDimming(text);
            highlightSearch(text);
            return;
        } else {
            setCurrentBlockState(state);
            setFormat(0, text.length() + 1, m_blockCodeFormat);
            CodeHighlighter::Formats formats = {m_keywordFormat, m_stringFormat, m_commentFormat};
            CodeHighlighter::highlight(text, state % 100, this, formats);
        }
        applyFocusDimming(text);
        highlightSearch(text);
        return;
    }

    setCurrentBlockState(0);
    if (!text.isEmpty()) {
        highlightMarkers(text);
        if (text.contains(QLatin1Char('`')) || text.contains(QLatin1Char('*'))
            || text.contains(QLatin1Char('_')) || text.contains(QLatin1Char('['))
            || text.contains(QLatin1Char('~')) || text.contains(QLatin1Char('<'))
            || text.contains(QLatin1Char('!'))) {
            highlightInline(text);
        }
    }
    applyFocusDimming(text);
    highlightSearch(text);
}

namespace {

// Each UTF-16 unit case-folded and, for Latin and Greek letters, stripped of
// its canonical diacritics: 'Ț' becomes 't'. Other scripts keep their marks,
// since there they tell letters apart (и and й, か and が), and Hangul
// syllables would fold to their first jamo. Surrogates map to themselves here
// and are folded in pairs by searchFolded.
const char16_t *searchFoldTable() {
    static const std::vector<char16_t> table = [] {
        std::vector<char16_t> folded(0x10000);
        for (int unit = 0; unit < 0x10000; ++unit) {
            const QChar original{char16_t(unit)};
            if (original.isSurrogate()) {
                folded[unit] = char16_t(unit);
                continue;
            }
            const QChar caseFolded = original.toCaseFolded();
            QChar base = caseFolded;
            while (base.decompositionTag() == QChar::Canonical) {
                const QChar next = base.decomposition().at(0);
                if (next.isSurrogate())
                    break;
                base = next.toCaseFolded();
            }
            const bool strips = base.script() == QChar::Script_Latin
                || base.script() == QChar::Script_Greek;
            folded[unit] = strips ? base.unicode() : caseFolded.unicode();
        }
        // A stroke or a missing dot is part of the letter, not a mark on it,
        // so these have no decomposition to strip.
        const std::pair<char16_t, char16_t> plain[] = {
            {u'\u0142', u'l'}, {u'\u0111', u'd'}, {u'\u00f8', u'o'},
            {u'\u0127', u'h'}, {u'\u0131', u'i'}};
        for (char16_t &unit : folded) {
            for (const auto &[from, to] : plain) {
                if (unit == from)
                    unit = to;
            }
        }
        return folded;
    }();
    return table.data();
}

// Romanian is written with ș and ț but much text has the older ş and ţ.
QChar commaBelow(QChar c) {
    if (c == QChar(0x015F))
        return QChar(0x0219);
    if (c == QChar(0x0163))
        return QChar(0x021B);
    return c;
}

// The text folded unit by unit through the table, then each surrogate pair
// case-folded as the one letter it encodes, which stays a pair: 𐐀 becomes 𐐨.
QString searchFolded(const QString &text) {
    const char16_t *fold = searchFoldTable();
    QString folded(text.size(), Qt::Uninitialized);
    const QChar *source = text.constData();
    QChar *target = folded.data();
    for (qsizetype i = 0; i < text.size(); ++i)
        target[i] = QChar(fold[source[i].unicode()]);
    for (qsizetype i = 0; i + 1 < text.size(); ++i) {
        if (!source[i].isHighSurrogate() || !source[i + 1].isLowSurrogate())
            continue;
        const char32_t caseFolded = QChar::toCaseFolded(QChar::surrogateToUcs4(source[i], source[i + 1]));
        if (QChar::requiresSurrogates(caseFolded)) {
            target[i] = QChar(QChar::highSurrogate(caseFolded));
            target[i + 1] = QChar(QChar::lowSurrogate(caseFolded));
        }
        ++i;
    }
    return folded;
}

} // namespace

QList<int> MarkdownHighlighter::findMatches(const QString &text, const QString &query) {
    QList<int> matches;
    const qsizetype length = query.size();
    if (length == 0 || text.size() < length)
        return matches;

    const QString foldedQuery = searchFolded(query);
    // The query's accented letters, case-folded, which the text must match
    // as written rather than folded.
    QList<std::pair<qsizetype, QChar>> exact;
    for (qsizetype i = 0; i < length; ++i) {
        const QChar unit = query.at(i);
        const QChar caseFolded = unit.toCaseFolded();
        if (!unit.isSurrogate() && foldedQuery.at(i) != caseFolded)
            exact.append({i, commaBelow(caseFolded)});
    }

    const QString foldedText = searchFolded(text);

    qsizetype from = 0;
    while ((from = foldedText.indexOf(foldedQuery, from)) >= 0) {
        const bool accentsMatch = std::all_of(exact.cbegin(), exact.cend(), [&](const auto &letter) {
            return commaBelow(text.at(from + letter.first).toCaseFolded()) == letter.second;
        });
        if (accentsMatch) {
            matches.append(int(from));
            from += length;
        } else {
            ++from;
        }
    }
    return matches;
}

void MarkdownHighlighter::highlightSearch(const QString &text) {
    if (m_searchQuery.isEmpty())
        return;

    for (const int from : findMatches(text, m_searchQuery)) {
        const int documentStart = currentBlock().position() + from;
        QTextCharFormat format = this->format(from);
        format.setBackground(documentStart == m_currentMatchStart
                                 ? m_currentSearchFormat.background()
                                 : m_searchFormat.background());
        setFormat(from, m_searchQuery.length(), format);
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
        const HeadingMarkup heading = headingMarkup(text);
        if (heading.level > 0) {
            setFormat(0, heading.prefixLength, m_markerFormat);
            setFormat(heading.prefixLength, text.length() - heading.prefixLength,
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

// Parses a leading ATX heading marker and its required separator.
MarkdownHighlighter::HeadingMarkup MarkdownHighlighter::headingMarkup(const QString &text) {
    static const QRegularExpression headingRe(QStringLiteral("^(#{1,6})(\\s+)(.*)$"));
    const QRegularExpressionMatch match = headingRe.match(text);
    if (!match.hasMatch())
        return {};
    const int level = int(match.capturedLength(1));
    return {level, level + int(match.capturedLength(2))};
}

void MarkdownHighlighter::highlightInline(const QString &text) {
    if (text.contains(QLatin1Char('`'))) {
        static const QRegularExpression codeRe(
            QStringLiteral("(?<!\\\\)(`+)(.+?)\\1"));
        QRegularExpressionMatchIterator codeMatches = codeRe.globalMatch(text);
        while (codeMatches.hasNext()) {
            const QRegularExpressionMatch match = codeMatches.next();
            setFormat(match.capturedStart(0), match.capturedLength(0), m_codeFormat);
        }
    }

    const QList<InlineMarkup> markup = inlineMarkup(text);
    for (const InlineMarkup &item : markup) {
        if (item.kind == InlineKind::Image && item.markers[0].length == 0)
            continue;
        const QTextCharFormat &contentFormat =
            item.kind == InlineKind::Bold ? m_boldFormat
            : item.kind == InlineKind::Italic ? m_italicFormat
            : item.kind == InlineKind::Strikethrough ? m_strikethroughFormat
            : m_linkFormat;
        setFormat(item.content.start, item.content.length, contentFormat);
        for (const Span &marker : item.markers)
            setFormat(marker.start, marker.length, m_hiddenMarkerFormat);
    }
}

QList<MarkdownHighlighter::InlineMarkup> MarkdownHighlighter::inlineMarkup(const QString &text) {
    QList<InlineMarkup> markup;
    if (!text.contains(QLatin1Char('*')) && !text.contains(QLatin1Char('_'))
            && !text.contains(QLatin1Char('[')) && !text.contains(QLatin1Char('~'))
            && !text.contains(QLatin1Char('<')) && !text.contains(QLatin1Char('!'))) {
        return markup;
    }
    if (text.startsWith(QStringLiteral("```"))) {
        QString trimmed = text.trimmed();
        if (trimmed == QStringLiteral("```")) {
            markup.append({InlineKind::CodeBlockMarker, {0, 0}, {{0, static_cast<int>(text.length())}, {0, 0}}});
        } else {
            markup.append({InlineKind::CodeBlockMarker, {3, static_cast<int>(text.length()) - 3}, {{0, 3}, {0, 0}}});
        }
    }



    const auto span = [](const QRegularExpressionMatch &match, int group) {
        return Span{int(match.capturedStart(group)), int(match.capturedLength(group))};
    };

    QList<Span> codeSpans;
    static const QRegularExpression protectedCodeRe(
        QStringLiteral("(?<!\\\\)(`+)(.+?)\\1"));
    QRegularExpressionMatchIterator protectedCodeMatches = protectedCodeRe.globalMatch(text);
    while (protectedCodeMatches.hasNext())
        codeSpans.append(span(protectedCodeMatches.next(), 0));
    const auto overlapsCode = [&codeSpans](const QRegularExpressionMatch &match) {
        const int start = match.capturedStart(0);
        const int end = start + match.capturedLength(0);
        for (const Span &code : codeSpans) {
            if (start < code.start + code.length && end > code.start)
                return true;
        }
        return false;
    };

    static const QRegularExpression boldRe(
        QStringLiteral("(?<!\\\\)(\\*\\*|__)(.+?)(\\1)"));
    QRegularExpressionMatchIterator boldMatches = boldRe.globalMatch(text);
    while (boldMatches.hasNext()) {
        const QRegularExpressionMatch match = boldMatches.next();
        if (overlapsCode(match))
            continue;
        markup.append({InlineKind::Bold, span(match, 2),
                       {span(match, 1), span(match, 3)}});
    }

    static const QRegularExpression italicRe(
        QStringLiteral("(?<![\\\\*])\\*(?![\\s*])([^*\\n]*[^\\s*])\\*(?!\\*)|(?<![\\\\_])_([^_\\n]+)_(?!_)"));
    QRegularExpressionMatchIterator italicMatches = italicRe.globalMatch(text);
    while (italicMatches.hasNext()) {
        const QRegularExpressionMatch match = italicMatches.next();
        if (overlapsCode(match))
            continue;
        const Span whole = span(match, 0);
        const int contentIndex = match.capturedStart(1) >= 0 ? 1 : 2;
        markup.append({InlineKind::Italic, span(match, contentIndex),
                       {{whole.start, 1}, {whole.start + whole.length - 1, 1}}});
    }

    static const QRegularExpression strikethroughRe(
        QStringLiteral("(?<!\\\\)(~~)(.+?)(~~)"));
    QRegularExpressionMatchIterator strikethroughMatches = strikethroughRe.globalMatch(text);
    while (strikethroughMatches.hasNext()) {
        const QRegularExpressionMatch match = strikethroughMatches.next();
        if (overlapsCode(match))
            continue;
        markup.append({InlineKind::Strikethrough, span(match, 2),
                       {span(match, 1), span(match, 3)}});
    }

    static const QRegularExpression linkRe(
        QStringLiteral("(?<![!\\\\])\\[([^\\]]+)\\]\\(((?:\\\\.|[^()\\s]|\\([^()]*\\))+)(?:\\s+(?:\"[^\"]*\"|'[^']*'|\\([^)]*\\)))?\\)"));
    QRegularExpressionMatchIterator linkMatches = linkRe.globalMatch(text);
    while (linkMatches.hasNext()) {
        const QRegularExpressionMatch match = linkMatches.next();
        if (overlapsCode(match))
            continue;
        const Span whole = span(match, 0);
        const Span content = span(match, 1);
        const int contentEnd = content.start + content.length;
        markup.append({InlineKind::Link, content,
                       {{whole.start, 1},
                        {contentEnd, whole.start + whole.length - contentEnd}},
                       span(match, 2)});
    }

    static const QRegularExpression imageRe(
        QStringLiteral("(?<!\\\\)!\\[([^\\]]*)\\]\\(((?:\\\\.|[^()\\s]|\\([^()]*\\))+)(?:\\s+(?:\"[^\"]*\"|'[^']*'|\\([^)]*\\)))?\\)"));
    QRegularExpressionMatchIterator imageMatches = imageRe.globalMatch(text);
    while (imageMatches.hasNext()) {
        const QRegularExpressionMatch match = imageMatches.next();
        if (overlapsCode(match))
            continue;
        const Span whole = span(match, 0);
        const Span content = span(match, 1);
        const int contentEnd = content.start + content.length;
        markup.append({InlineKind::Image, whole,
                       {{whole.start, 2},
                        {contentEnd, whole.start + whole.length - contentEnd}},
                       span(match, 2)});
    }

    static const QRegularExpression htmlImgRe(
        QStringLiteral("<img\\b[^>]*?\\bsrc\\s*=\\s*[\"']([^\"']+)[\"'][^>]*>"),
        QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatchIterator htmlImgMatches = htmlImgRe.globalMatch(text);
    while (htmlImgMatches.hasNext()) {
        const QRegularExpressionMatch match = htmlImgMatches.next();
        if (overlapsCode(match))
            continue;
        const Span whole = span(match, 0);
        markup.append({InlineKind::Image, whole,
                       {{0, 0}, {0, 0}},
                       span(match, 1)});
    }

    return markup;
}

QString MarkdownHighlighter::linkUrlAt(const QString &text, int position) {
    if (position < 0 || position >= text.length())
        return {};

    const QList<InlineMarkup> markup = inlineMarkup(text);
    for (const InlineMarkup &item : markup) {
        if (item.kind != InlineKind::Link)
            continue;
        const int linkStart = item.markers[0].length > 0 ? item.markers[0].start : item.content.start;
        const int linkEnd = item.markers[1].length > 0 ? item.markers[1].start + item.markers[1].length : item.content.start + item.content.length;
        if (position >= linkStart && position < linkEnd)
            return unescapeDestination(text.mid(item.destination.start, item.destination.length));
    }
    return {};
}

QString MarkdownHighlighter::imageUrlAt(const QString &text, int position) {
    if (position < 0 || position >= text.length())
        return {};

    const QList<InlineMarkup> markup = inlineMarkup(text);
    for (const InlineMarkup &item : markup) {
        if (item.kind != InlineKind::Image)
            continue;
        const int imgStart = item.markers[0].length > 0 ? item.markers[0].start : item.content.start;
        const int imgEnd = item.markers[1].length > 0 ? item.markers[1].start + item.markers[1].length : item.content.start + item.content.length;
        if (position >= imgStart && position < imgEnd)
            return unescapeDestination(text.mid(item.destination.start, item.destination.length));
    }
    return {};
}

void MarkdownHighlighter::applyFocusDimming(const QString &text) {
    if (!m_focusMode || text.isEmpty())
        return;
    const int blockStart = currentBlock().position();
    if (m_focusCursorPosition >= blockStart
        && m_focusCursorPosition < blockStart + currentBlock().length())
        return;
    const QColor hidden = m_hiddenMarkerFormat.foreground().color();
    int pos = 0;
    while (pos < text.length()) {
        QTextCharFormat fmt = format(pos);
        int run = 1;
        while (pos + run < text.length() && format(pos + run) == fmt)
            ++run;
        // Inline markers are hidden by painting them in the background colour,
        // so dimming them would make them reappear.
        if (fmt.foreground().color() != hidden) {
            fmt.setForeground(m_dimmedColor);
            setFormat(pos, run, fmt);
        }
        pos += run;
    }
}

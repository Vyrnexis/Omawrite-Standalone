#include "codehighlighter.h"
#include "markdownhighlighter.h"
#include <QRegularExpression>

void CodeHighlighter::highlight(const QString &text, int languageState, 
                                MarkdownHighlighter *highlighter, const Formats &formats) {
    if (languageState == 2) {
        highlightBash(text, highlighter, formats);
    } else if (languageState == 3) {
        highlightNim(text, highlighter, formats);
    } else if (languageState == 4) {
        highlightPython(text, highlighter, formats);
    } else if (languageState == 5) {
        highlightToml(text, highlighter, formats);
    } else if (languageState == 6) {
        highlightJson(text, highlighter, formats);
    } else if (languageState == 7) {
        highlightC(text, highlighter, formats);
    } else if (languageState == 8) {
        highlightCpp(text, highlighter, formats);
    } else if (languageState == 9) {
        highlightJs(text, highlighter, formats);
    } else if (languageState == 10) {
        highlightRuby(text, highlighter, formats);
    }
}

void CodeHighlighter::highlightBash(const QString &text, MarkdownHighlighter *highlighter, const Formats &formats) {
    static const QStringList keywordPatterns = {
        QStringLiteral("\\bif\\b"), QStringLiteral("\\bthen\\b"), QStringLiteral("\\belse\\b"), 
        QStringLiteral("\\belif\\b"), QStringLiteral("\\bfi\\b"), QStringLiteral("\\bfor\\b"), 
        QStringLiteral("\\bwhile\\b"), QStringLiteral("\\bin\\b"), QStringLiteral("\\bdo\\b"), 
        QStringLiteral("\\bdone\\b"), QStringLiteral("\\becho\\b"), QStringLiteral("\\bsudo\\b"),
        QStringLiteral("\\beopkg\\b"), QStringLiteral("\\bup\\b")
    };
    for (const QString &pattern : keywordPatterns) {
        QRegularExpression re(pattern);
        QRegularExpressionMatchIterator i = re.globalMatch(text);
        while (i.hasNext()) {
            QRegularExpressionMatch match = i.next();
            highlighter->applyFormat(match.capturedStart(), match.capturedLength(), formats.keyword);
        }
    }

    static const QRegularExpression stringRe(QStringLiteral("(\"[^\"]*\"|'[^']*')"));
    QRegularExpressionMatchIterator i = stringRe.globalMatch(text);
    while (i.hasNext()) {
        QRegularExpressionMatch match = i.next();
        highlighter->applyFormat(match.capturedStart(), match.capturedLength(), formats.string);
    }

    static const QRegularExpression commentRe(QStringLiteral("#.*"));
    QRegularExpressionMatch match = commentRe.match(text);
    if (match.hasMatch()) {
        highlighter->applyFormat(match.capturedStart(), match.capturedLength(), formats.comment);
    }
}

void CodeHighlighter::highlightNim(const QString &text, MarkdownHighlighter *highlighter, const Formats &formats) {
    static const QStringList keywordPatterns = {
        QStringLiteral("\\blet\\b"), QStringLiteral("\\bvar\\b"), QStringLiteral("\\bconst\\b"), 
        QStringLiteral("\\bproc\\b"), QStringLiteral("\\bfunc\\b"), QStringLiteral("\\bmacro\\b"), 
        QStringLiteral("\\btemplate\\b"), QStringLiteral("\\bif\\b"), QStringLiteral("\\belse\\b"), 
        QStringLiteral("\\belif\\b"), QStringLiteral("\\bwhen\\b"), QStringLiteral("\\bcase\\b"), 
        QStringLiteral("\\bof\\b"), QStringLiteral("\\breturn\\b"), QStringLiteral("\\bdiscard\\b"),
        QStringLiteral("\\becho\\b")
    };
    for (const QString &pattern : keywordPatterns) {
        QRegularExpression re(pattern);
        QRegularExpressionMatchIterator i = re.globalMatch(text);
        while (i.hasNext()) {
            QRegularExpressionMatch match = i.next();
            highlighter->applyFormat(match.capturedStart(), match.capturedLength(), formats.keyword);
        }
    }

    static const QRegularExpression stringRe(QStringLiteral("\"[^\"]*\""));
    QRegularExpressionMatchIterator i = stringRe.globalMatch(text);
    while (i.hasNext()) {
        QRegularExpressionMatch match = i.next();
        highlighter->applyFormat(match.capturedStart(), match.capturedLength(), formats.string);
    }

    static const QRegularExpression commentRe(QStringLiteral("#.*"));
    QRegularExpressionMatch match = commentRe.match(text);
    if (match.hasMatch()) {
        highlighter->applyFormat(match.capturedStart(), match.capturedLength(), formats.comment);
    }
}

void CodeHighlighter::highlightPython(const QString &text, MarkdownHighlighter *highlighter, const Formats &formats) {
    static const QStringList keywordPatterns = {
        QStringLiteral("\\bdef\\b"), QStringLiteral("\\bclass\\b"), QStringLiteral("\\bif\\b"), 
        QStringLiteral("\\belif\\b"), QStringLiteral("\\belse\\b"), QStringLiteral("\\bwhile\\b"), 
        QStringLiteral("\\bfor\\b"), QStringLiteral("\\bin\\b"), QStringLiteral("\\breturn\\b"), 
        QStringLiteral("\\bimport\\b"), QStringLiteral("\\bfrom\\b"), QStringLiteral("\\bas\\b"), 
        QStringLiteral("\\btry\\b"), QStringLiteral("\\bexcept\\b"), QStringLiteral("\\bfinally\\b"), 
        QStringLiteral("\\bwith\\b"), QStringLiteral("\\bpass\\b"), QStringLiteral("\\bbreak\\b"), 
        QStringLiteral("\\bcontinue\\b"), QStringLiteral("\\byield\\b"), QStringLiteral("\\blambda\\b"), 
        QStringLiteral("\\band\\b"), QStringLiteral("\\bor\\b"), QStringLiteral("\\bnot\\b"), 
        QStringLiteral("\\bis\\b"), QStringLiteral("\\bNone\\b"), QStringLiteral("\\bTrue\\b"), QStringLiteral("\\bFalse\\b")
    };
    for (const QString &pattern : keywordPatterns) {
        QRegularExpression re(pattern);
        QRegularExpressionMatchIterator i = re.globalMatch(text);
        while (i.hasNext()) {
            QRegularExpressionMatch match = i.next();
            highlighter->applyFormat(match.capturedStart(), match.capturedLength(), formats.keyword);
        }
    }

    static const QRegularExpression stringRe(QStringLiteral("(\"[^\"]*\"|'[^']*')"));
    QRegularExpressionMatchIterator i = stringRe.globalMatch(text);
    while (i.hasNext()) {
        QRegularExpressionMatch match = i.next();
        highlighter->applyFormat(match.capturedStart(), match.capturedLength(), formats.string);
    }

    static const QRegularExpression commentRe(QStringLiteral("#.*"));
    QRegularExpressionMatch match = commentRe.match(text);
    if (match.hasMatch()) {
        highlighter->applyFormat(match.capturedStart(), match.capturedLength(), formats.comment);
    }
}

void CodeHighlighter::highlightToml(const QString &text, MarkdownHighlighter *highlighter, const Formats &formats) {
    static const QStringList keywordPatterns = {
        QStringLiteral("\\btrue\\b"), QStringLiteral("\\bfalse\\b")
    };
    for (const QString &pattern : keywordPatterns) {
        QRegularExpression re(pattern);
        QRegularExpressionMatchIterator i = re.globalMatch(text);
        while (i.hasNext()) {
            QRegularExpressionMatch match = i.next();
            highlighter->applyFormat(match.capturedStart(), match.capturedLength(), formats.keyword);
        }
    }
    
    static const QRegularExpression tableRe(QStringLiteral("^\\s*\\[.*\\]"));
    QRegularExpressionMatch tableMatch = tableRe.match(text);
    if (tableMatch.hasMatch()) {
        highlighter->applyFormat(tableMatch.capturedStart(), tableMatch.capturedLength(), formats.keyword);
    }

    static const QRegularExpression stringRe(QStringLiteral("(\"[^\"]*\"|'[^']*')"));
    QRegularExpressionMatchIterator i = stringRe.globalMatch(text);
    while (i.hasNext()) {
        QRegularExpressionMatch match = i.next();
        highlighter->applyFormat(match.capturedStart(), match.capturedLength(), formats.string);
    }

    static const QRegularExpression commentRe(QStringLiteral("#.*"));
    QRegularExpressionMatch match = commentRe.match(text);
    if (match.hasMatch()) {
        highlighter->applyFormat(match.capturedStart(), match.capturedLength(), formats.comment);
    }
}

void CodeHighlighter::highlightJson(const QString &text, MarkdownHighlighter *highlighter, const Formats &formats) {
    static const QStringList keywordPatterns = {
        QStringLiteral("\\btrue\\b"), QStringLiteral("\\bfalse\\b"), QStringLiteral("\\bnull\\b")
    };
    for (const QString &pattern : keywordPatterns) {
        QRegularExpression re(pattern);
        QRegularExpressionMatchIterator i = re.globalMatch(text);
        while (i.hasNext()) {
            QRegularExpressionMatch match = i.next();
            highlighter->applyFormat(match.capturedStart(), match.capturedLength(), formats.keyword);
        }
    }

    static const QRegularExpression stringRe(QStringLiteral("\"[^\"]*\""));
    QRegularExpressionMatchIterator i = stringRe.globalMatch(text);
    while (i.hasNext()) {
        QRegularExpressionMatch match = i.next();
        highlighter->applyFormat(match.capturedStart(), match.capturedLength(), formats.string);
    }
}

void CodeHighlighter::highlightC(const QString &text, MarkdownHighlighter *highlighter, const Formats &formats) {
    static const QStringList keywordPatterns = {
        QStringLiteral("\\bint\\b"), QStringLiteral("\\bchar\\b"), QStringLiteral("\\bfloat\\b"), 
        QStringLiteral("\\bdouble\\b"), QStringLiteral("\\bvoid\\b"), QStringLiteral("\\bstruct\\b"), 
        QStringLiteral("\\bunion\\b"), QStringLiteral("\\benum\\b"), QStringLiteral("\\bif\\b"), 
        QStringLiteral("\\belse\\b"), QStringLiteral("\\bwhile\\b"), QStringLiteral("\\bfor\\b"), 
        QStringLiteral("\\bdo\\b"), QStringLiteral("\\breturn\\b"), QStringLiteral("\\bswitch\\b"), 
        QStringLiteral("\\bcase\\b"), QStringLiteral("\\bbreak\\b"), QStringLiteral("\\bcontinue\\b"), 
        QStringLiteral("\\bdefault\\b"), QStringLiteral("\\btypedef\\b"), QStringLiteral("\\bstatic\\b"), 
        QStringLiteral("\\bextern\\b"), QStringLiteral("\\bconst\\b"), QStringLiteral("\\bsizeof\\b")
    };
    for (const QString &pattern : keywordPatterns) {
        QRegularExpression re(pattern);
        QRegularExpressionMatchIterator i = re.globalMatch(text);
        while (i.hasNext()) {
            QRegularExpressionMatch match = i.next();
            highlighter->applyFormat(match.capturedStart(), match.capturedLength(), formats.keyword);
        }
    }

    static const QRegularExpression stringRe(QStringLiteral("\"[^\"]*\""));
    QRegularExpressionMatchIterator i = stringRe.globalMatch(text);
    while (i.hasNext()) {
        QRegularExpressionMatch match = i.next();
        highlighter->applyFormat(match.capturedStart(), match.capturedLength(), formats.string);
    }

    static const QRegularExpression commentRe(QStringLiteral("//.*"));
    QRegularExpressionMatch match = commentRe.match(text);
    if (match.hasMatch()) {
        highlighter->applyFormat(match.capturedStart(), match.capturedLength(), formats.comment);
    }
}

void CodeHighlighter::highlightCpp(const QString &text, MarkdownHighlighter *highlighter, const Formats &formats) {
    highlightC(text, highlighter, formats);

    static const QStringList keywordPatterns = {
        QStringLiteral("\\bclass\\b"), QStringLiteral("\\bpublic\\b"), QStringLiteral("\\bprivate\\b"), 
        QStringLiteral("\\bprotected\\b"), QStringLiteral("\\bvirtual\\b"), QStringLiteral("\\boverride\\b"), 
        QStringLiteral("\\btemplate\\b"), QStringLiteral("\\btypename\\b"), QStringLiteral("\\bauto\\b"), 
        QStringLiteral("\\bconstexpr\\b"), QStringLiteral("\\bnew\\b"), QStringLiteral("\\bdelete\\b"), 
        QStringLiteral("\\bthis\\b"), QStringLiteral("\\bfriend\\b"), QStringLiteral("\\binline\\b"), 
        QStringLiteral("\\bnamespace\\b"), QStringLiteral("\\busing\\b"), QStringLiteral("\\btry\\b"), 
        QStringLiteral("\\bcatch\\b"), QStringLiteral("\\bthrow\\b")
    };
    for (const QString &pattern : keywordPatterns) {
        QRegularExpression re(pattern);
        QRegularExpressionMatchIterator i = re.globalMatch(text);
        while (i.hasNext()) {
            QRegularExpressionMatch match = i.next();
            highlighter->applyFormat(match.capturedStart(), match.capturedLength(), formats.keyword);
        }
    }
}

void CodeHighlighter::highlightJs(const QString &text, MarkdownHighlighter *highlighter, const Formats &formats) {
    static const QStringList keywordPatterns = {
        QStringLiteral("\\blet\\b"), QStringLiteral("\\bconst\\b"), QStringLiteral("\\bvar\\b"), 
        QStringLiteral("\\bfunction\\b"), QStringLiteral("\\breturn\\b"), QStringLiteral("\\bif\\b"), 
        QStringLiteral("\\belse\\b"), QStringLiteral("\\bwhile\\b"), QStringLiteral("\\bfor\\b"), 
        QStringLiteral("\\bdo\\b"), QStringLiteral("\\bswitch\\b"), QStringLiteral("\\bcase\\b"), 
        QStringLiteral("\\bbreak\\b"), QStringLiteral("\\bcontinue\\b"), QStringLiteral("\\bdefault\\b"), 
        QStringLiteral("\\btry\\b"), QStringLiteral("\\bcatch\\b"), QStringLiteral("\\bfinally\\b"), 
        QStringLiteral("\\bthrow\\b"), QStringLiteral("\\bclass\\b"), QStringLiteral("\\bextends\\b"), 
        QStringLiteral("\\bnew\\b"), QStringLiteral("\\bthis\\b"), QStringLiteral("\\bimport\\b"), 
        QStringLiteral("\\bexport\\b"), QStringLiteral("\\bfrom\\b"), QStringLiteral("\\bawait\\b"), 
        QStringLiteral("\\basync\\b"), QStringLiteral("\\byield\\b")
    };
    for (const QString &pattern : keywordPatterns) {
        QRegularExpression re(pattern);
        QRegularExpressionMatchIterator i = re.globalMatch(text);
        while (i.hasNext()) {
            QRegularExpressionMatch match = i.next();
            highlighter->applyFormat(match.capturedStart(), match.capturedLength(), formats.keyword);
        }
    }

    static const QRegularExpression stringRe(QStringLiteral("(\"[^\"]*\"|'[^']*'|`[^`]*`)"));
    QRegularExpressionMatchIterator i = stringRe.globalMatch(text);
    while (i.hasNext()) {
        QRegularExpressionMatch match = i.next();
        highlighter->applyFormat(match.capturedStart(), match.capturedLength(), formats.string);
    }

    static const QRegularExpression commentRe(QStringLiteral("//.*"));
    QRegularExpressionMatch match = commentRe.match(text);
    if (match.hasMatch()) {
        highlighter->applyFormat(match.capturedStart(), match.capturedLength(), formats.comment);
    }
}

void CodeHighlighter::highlightRuby(const QString &text, MarkdownHighlighter *highlighter, const Formats &formats) {
    static const QStringList keywordPatterns = {
        QStringLiteral("\\bdef\\b"), QStringLiteral("\\bclass\\b"), QStringLiteral("\\bmodule\\b"), 
        QStringLiteral("\\bif\\b"), QStringLiteral("\\belse\\b"), QStringLiteral("\\belsif\\b"), 
        QStringLiteral("\\bunless\\b"), QStringLiteral("\\bwhile\\b"), QStringLiteral("\\buntil\\b"), 
        QStringLiteral("\\bfor\\b"), QStringLiteral("\\bin\\b"), QStringLiteral("\\bdo\\b"), 
        QStringLiteral("\\bend\\b"), QStringLiteral("\\breturn\\b"), QStringLiteral("\\byield\\b"), 
        QStringLiteral("\\bbreak\\b"), QStringLiteral("\\bnext\\b"), QStringLiteral("\\bredo\\b"), 
        QStringLiteral("\\bretry\\b"), QStringLiteral("\\btrue\\b"), QStringLiteral("\\bfalse\\b"), 
        QStringLiteral("\\bnil\\b"), QStringLiteral("\\bself\\b"), QStringLiteral("\\bsuper\\b"), 
        QStringLiteral("\\brequire\\b"), QStringLiteral("\\binclude\\b"), QStringLiteral("\\bextend\\b")
    };
    for (const QString &pattern : keywordPatterns) {
        QRegularExpression re(pattern);
        QRegularExpressionMatchIterator i = re.globalMatch(text);
        while (i.hasNext()) {
            QRegularExpressionMatch match = i.next();
            highlighter->applyFormat(match.capturedStart(), match.capturedLength(), formats.keyword);
        }
    }

    static const QRegularExpression stringRe(QStringLiteral("(\"[^\"]*\"|'[^']*')"));
    QRegularExpressionMatchIterator i = stringRe.globalMatch(text);
    while (i.hasNext()) {
        QRegularExpressionMatch match = i.next();
        highlighter->applyFormat(match.capturedStart(), match.capturedLength(), formats.string);
    }

    static const QRegularExpression commentRe(QStringLiteral("#.*"));
    QRegularExpressionMatch match = commentRe.match(text);
    if (match.hasMatch()) {
        highlighter->applyFormat(match.capturedStart(), match.capturedLength(), formats.comment);
    }
}

#pragma once

#include <QString>
#include <QTextCharFormat>

class MarkdownHighlighter;

class CodeHighlighter {
public:
    struct Formats {
        QTextCharFormat keyword;
        QTextCharFormat string;
        QTextCharFormat comment;
    };

    static void highlight(const QString &text, int languageState, 
                          MarkdownHighlighter *highlighter, const Formats &formats);

private:
    static void highlightBash(const QString &text, MarkdownHighlighter *highlighter, const Formats &formats);
    static void highlightNim(const QString &text, MarkdownHighlighter *highlighter, const Formats &formats);
    static void highlightPython(const QString &text, MarkdownHighlighter *highlighter, const Formats &formats);
    static void highlightToml(const QString &text, MarkdownHighlighter *highlighter, const Formats &formats);
    static void highlightJson(const QString &text, MarkdownHighlighter *highlighter, const Formats &formats);
    static void highlightC(const QString &text, MarkdownHighlighter *highlighter, const Formats &formats);
    static void highlightCpp(const QString &text, MarkdownHighlighter *highlighter, const Formats &formats);
    static void highlightJs(const QString &text, MarkdownHighlighter *highlighter, const Formats &formats);
    static void highlightRuby(const QString &text, MarkdownHighlighter *highlighter, const Formats &formats);
};

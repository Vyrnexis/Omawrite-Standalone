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

    static int languageStateForName(const QString &name);
    static void highlight(const QString &text, int languageState, 
                          MarkdownHighlighter *highlighter, const Formats &formats);
    static QStringList supportedLanguages();
};

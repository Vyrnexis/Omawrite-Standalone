#include "codehighlighter.h"
#include "markdownhighlighter.h"
#include <QHash>
#include <QRegularExpression>

namespace {
// Finds a line-comment marker while ignoring quoted and escaped occurrences.
int commentStartOutsideStrings(const QString &text, const QString &marker,
                               bool singleQuotes, bool backticks) {
    QChar quote;
    bool escaped = false;
    for (int i = 0; i < text.length(); ++i) {
        const QChar ch = text.at(i);
        if (!quote.isNull()) {
            if (escaped) {
                escaped = false;
            } else if (ch == QLatin1Char('\\')) {
                escaped = true;
            } else if (ch == quote) {
                quote = QChar();
            }
        } else if (ch == QLatin1Char('"')
                   || (singleQuotes && ch == QLatin1Char('\''))
                   || (backticks && ch == QLatin1Char('`'))) {
            quote = ch;
        } else if (text.mid(i, marker.length()) == marker) {
            return i;
        }
    }
    return -1;
}

// Applies a line-comment format only when its marker appears outside a string.
void highlightLineComment(const QString &text, const QString &marker,
                          bool singleQuotes, bool backticks,
                          MarkdownHighlighter *highlighter,
                          const CodeHighlighter::Formats &formats) {
    const int start = commentStartOutsideStrings(text, marker, singleQuotes, backticks);
    if (start >= 0)
        highlighter->applyFormat(start, text.length() - start, formats.comment);
}

// Highlights a cached group of keyword patterns without recompiling each token expression.
void highlightKeywords(const QString &text, const QStringList &patterns,
                       MarkdownHighlighter *highlighter,
                       const CodeHighlighter::Formats &formats) {
    const QString key = patterns.join(QLatin1Char('|'));
    static QHash<QString, QRegularExpression> expressions;
    auto expression = expressions.constFind(key);
    if (expression == expressions.cend())
        expression = expressions.insert(key, QRegularExpression(key));
    QRegularExpressionMatchIterator matches = expression->globalMatch(text);
    while (matches.hasNext()) {
        const QRegularExpressionMatch match = matches.next();
        highlighter->applyFormat(match.capturedStart(), match.capturedLength(), formats.keyword);
    }
}
}

namespace {
struct LanguageDef {
    QStringList keywords;
    QString commentMarker;
    bool supportsSingleQuotes;
    bool supportsBackticks;
    bool isToml;
};

const QHash<int, LanguageDef> &languageDefinitions() {
    static const QHash<int, LanguageDef> defs = {
        // 2: Bash / Shell
        {2, {
            {QStringLiteral("\\bif\\b"), QStringLiteral("\\bthen\\b"), QStringLiteral("\\belse\\b"),
             QStringLiteral("\\belif\\b"), QStringLiteral("\\bfi\\b"), QStringLiteral("\\bfor\\b"),
             QStringLiteral("\\bwhile\\b"), QStringLiteral("\\bin\\b"), QStringLiteral("\\bdo\\b"),
             QStringLiteral("\\bdone\\b"), QStringLiteral("\\becho\\b"), QStringLiteral("\\bsudo\\b"),
             QStringLiteral("\\bcase\\b"), QStringLiteral("\\besac\\b"), QStringLiteral("\\bfunction\\b"),
             QStringLiteral("\\breturn\\b"), QStringLiteral("\\bexit\\b"), QStringLiteral("\\beopkg\\b"),
             QStringLiteral("\\bapt\\b"), QStringLiteral("\\bpacman\\b"), QStringLiteral("\\bdnf\\b"),
             QStringLiteral("\\bzypper\\b")},
            QStringLiteral("#"), true, true, false
        }},
        // 3: Nim
        {3, {
            {QStringLiteral("\\blet\\b"), QStringLiteral("\\bvar\\b"), QStringLiteral("\\bconst\\b"),
             QStringLiteral("\\bproc\\b"), QStringLiteral("\\bfunc\\b"), QStringLiteral("\\bmacro\\b"),
             QStringLiteral("\\btemplate\\b"), QStringLiteral("\\biterator\\b"), QStringLiteral("\\btype\\b"),
             QStringLiteral("\\bif\\b"), QStringLiteral("\\belse\\b"), QStringLiteral("\\belif\\b"),
             QStringLiteral("\\bwhen\\b"), QStringLiteral("\\bcase\\b"), QStringLiteral("\\bof\\b"),
             QStringLiteral("\\bwhile\\b"), QStringLiteral("\\bfor\\b"), QStringLiteral("\\bin\\b"),
             QStringLiteral("\\breturn\\b"), QStringLiteral("\\byield\\b"), QStringLiteral("\\bdiscard\\b"),
             QStringLiteral("\\bimport\\b"), QStringLiteral("\\bexport\\b"), QStringLiteral("\\bfrom\\b"),
             QStringLiteral("\\binclude\\b"), QStringLiteral("\\becho\\b"), QStringLiteral("\\bnil\\b"),
             QStringLiteral("\\btrue\\b"), QStringLiteral("\\bfalse\\b")},
            QStringLiteral("#"), false, false, false
        }},
        // 4: Python
        {4, {
            {QStringLiteral("\\bdef\\b"), QStringLiteral("\\bclass\\b"), QStringLiteral("\\bif\\b"),
             QStringLiteral("\\belif\\b"), QStringLiteral("\\belse\\b"), QStringLiteral("\\bwhile\\b"),
             QStringLiteral("\\bfor\\b"), QStringLiteral("\\bin\\b"), QStringLiteral("\\breturn\\b"),
             QStringLiteral("\\bimport\\b"), QStringLiteral("\\bfrom\\b"), QStringLiteral("\\bas\\b"),
             QStringLiteral("\\btry\\b"), QStringLiteral("\\bexcept\\b"), QStringLiteral("\\bfinally\\b"),
             QStringLiteral("\\bwith\\b"), QStringLiteral("\\bpass\\b"), QStringLiteral("\\bbreak\\b"),
             QStringLiteral("\\bcontinue\\b"), QStringLiteral("\\byield\\b"), QStringLiteral("\\blambda\\b"),
             QStringLiteral("\\band\\b"), QStringLiteral("\\bor\\b"), QStringLiteral("\\bnot\\b"),
             QStringLiteral("\\bis\\b"), QStringLiteral("\\bNone\\b"), QStringLiteral("\\bTrue\\b"),
             QStringLiteral("\\bFalse\\b"), QStringLiteral("\\bself\\b"), QStringLiteral("\\basync\\b"),
             QStringLiteral("\\bawait\\b")},
            QStringLiteral("#"), true, false, false
        }},
        // 5: TOML
        {5, {
            {QStringLiteral("\\btrue\\b"), QStringLiteral("\\bfalse\\b")},
            QStringLiteral("#"), true, false, true
        }},
        // 6: JSON
        {6, {
            {QStringLiteral("\\btrue\\b"), QStringLiteral("\\bfalse\\b"), QStringLiteral("\\bnull\\b")},
            QString(), false, false, false
        }},
        // 7: C
        {7, {
            {QStringLiteral("\\bint\\b"), QStringLiteral("\\bchar\\b"), QStringLiteral("\\bfloat\\b"),
             QStringLiteral("\\bdouble\\b"), QStringLiteral("\\bvoid\\b"), QStringLiteral("\\bstruct\\b"),
             QStringLiteral("\\bunion\\b"), QStringLiteral("\\benum\\b"), QStringLiteral("\\bif\\b"),
             QStringLiteral("\\belse\\b"), QStringLiteral("\\bwhile\\b"), QStringLiteral("\\bfor\\b"),
             QStringLiteral("\\bdo\\b"), QStringLiteral("\\breturn\\b"), QStringLiteral("\\bswitch\\b"),
             QStringLiteral("\\bcase\\b"), QStringLiteral("\\bbreak\\b"), QStringLiteral("\\bcontinue\\b"),
             QStringLiteral("\\bdefault\\b"), QStringLiteral("\\btypedef\\b"), QStringLiteral("\\bstatic\\b"),
             QStringLiteral("\\bextern\\b"), QStringLiteral("\\bconst\\b"), QStringLiteral("\\bsizeof\\b"),
             QStringLiteral("\\bNULL\\b"), QStringLiteral("\\bbool\\b"), QStringLiteral("\\btrue\\b"),
             QStringLiteral("\\bfalse\\b")},
            QStringLiteral("//"), true, false, false
        }},
        // 8: C++
        {8, {
            {QStringLiteral("\\bint\\b"), QStringLiteral("\\bchar\\b"), QStringLiteral("\\bfloat\\b"),
             QStringLiteral("\\bdouble\\b"), QStringLiteral("\\bvoid\\b"), QStringLiteral("\\bstruct\\b"),
             QStringLiteral("\\bunion\\b"), QStringLiteral("\\benum\\b"), QStringLiteral("\\bif\\b"),
             QStringLiteral("\\belse\\b"), QStringLiteral("\\bwhile\\b"), QStringLiteral("\\bfor\\b"),
             QStringLiteral("\\bdo\\b"), QStringLiteral("\\breturn\\b"), QStringLiteral("\\bswitch\\b"),
             QStringLiteral("\\bcase\\b"), QStringLiteral("\\bbreak\\b"), QStringLiteral("\\bcontinue\\b"),
             QStringLiteral("\\bdefault\\b"), QStringLiteral("\\btypedef\\b"), QStringLiteral("\\bstatic\\b"),
             QStringLiteral("\\bextern\\b"), QStringLiteral("\\bconst\\b"), QStringLiteral("\\bsizeof\\b"),
             QStringLiteral("\\bclass\\b"), QStringLiteral("\\bpublic\\b"), QStringLiteral("\\bprivate\\b"),
             QStringLiteral("\\bprotected\\b"), QStringLiteral("\\bvirtual\\b"), QStringLiteral("\\boverride\\b"),
             QStringLiteral("\\btemplate\\b"), QStringLiteral("\\btypename\\b"), QStringLiteral("\\bauto\\b"),
             QStringLiteral("\\bconstexpr\\b"), QStringLiteral("\\bnew\\b"), QStringLiteral("\\bdelete\\b"),
             QStringLiteral("\\bthis\\b"), QStringLiteral("\\bfriend\\b"), QStringLiteral("\\binline\\b"),
             QStringLiteral("\\bnamespace\\b"), QStringLiteral("\\busing\\b"), QStringLiteral("\\btry\\b"),
             QStringLiteral("\\bcatch\\b"), QStringLiteral("\\bthrow\\b"), QStringLiteral("\\bnullptr\\b"),
             QStringLiteral("\\bbool\\b"), QStringLiteral("\\btrue\\b"), QStringLiteral("\\bfalse\\b")},
            QStringLiteral("//"), true, false, false
        }},
        // 9: JavaScript / TypeScript
        {9, {
            {QStringLiteral("\\blet\\b"), QStringLiteral("\\bconst\\b"), QStringLiteral("\\bvar\\b"),
             QStringLiteral("\\bfunction\\b"), QStringLiteral("\\breturn\\b"), QStringLiteral("\\bif\\b"),
             QStringLiteral("\\belse\\b"), QStringLiteral("\\bwhile\\b"), QStringLiteral("\\bfor\\b"),
             QStringLiteral("\\bdo\\b"), QStringLiteral("\\bswitch\\b"), QStringLiteral("\\bcase\\b"),
             QStringLiteral("\\bbreak\\b"), QStringLiteral("\\bcontinue\\b"), QStringLiteral("\\bdefault\\b"),
             QStringLiteral("\\btry\\b"), QStringLiteral("\\bcatch\\b"), QStringLiteral("\\bfinally\\b"),
             QStringLiteral("\\bthrow\\b"), QStringLiteral("\\bclass\\b"), QStringLiteral("\\bextends\\b"),
             QStringLiteral("\\bnew\\b"), QStringLiteral("\\bthis\\b"), QStringLiteral("\\bimport\\b"),
             QStringLiteral("\\bexport\\b"), QStringLiteral("\\bfrom\\b"), QStringLiteral("\\bawait\\b"),
             QStringLiteral("\\basync\\b"), QStringLiteral("\\byield\\b"), QStringLiteral("\\btypeof\\b"),
             QStringLiteral("\\binstanceof\\b"), QStringLiteral("\\binterface\\b"), QStringLiteral("\\btype\\b"),
             QStringLiteral("\\bnull\\b"), QStringLiteral("\\bundefined\\b"), QStringLiteral("\\btrue\\b"),
             QStringLiteral("\\bfalse\\b")},
            QStringLiteral("//"), true, true, false
        }},
        // 10: Ruby
        {10, {
            {QStringLiteral("\\bdef\\b"), QStringLiteral("\\bclass\\b"), QStringLiteral("\\bmodule\\b"),
             QStringLiteral("\\bif\\b"), QStringLiteral("\\belse\\b"), QStringLiteral("\\belsif\\b"),
             QStringLiteral("\\bunless\\b"), QStringLiteral("\\bwhile\\b"), QStringLiteral("\\buntil\\b"),
             QStringLiteral("\\bfor\\b"), QStringLiteral("\\bin\\b"), QStringLiteral("\\bdo\\b"),
             QStringLiteral("\\bend\\b"), QStringLiteral("\\breturn\\b"), QStringLiteral("\\byield\\b"),
             QStringLiteral("\\brequire\\b"), QStringLiteral("\\binclude\\b"), QStringLiteral("\\battr_accessor\\b"),
             QStringLiteral("\\battr_reader\\b"), QStringLiteral("\\battr_writer\\b"), QStringLiteral("\\bnil\\b"),
             QStringLiteral("\\btrue\\b"), QStringLiteral("\\bfalse\\b")},
            QStringLiteral("#"), true, false, false
        }},
        // 11: Go
        {11, {
            {QStringLiteral("\\bpackage\\b"), QStringLiteral("\\bimport\\b"), QStringLiteral("\\bfunc\\b"),
             QStringLiteral("\\breturn\\b"), QStringLiteral("\\bvar\\b"), QStringLiteral("\\btype\\b"),
             QStringLiteral("\\bstruct\\b"), QStringLiteral("\\binterface\\b"), QStringLiteral("\\bchan\\b"),
             QStringLiteral("\\bgo\\b"), QStringLiteral("\\bselect\\b"), QStringLiteral("\\bdefer\\b"),
             QStringLiteral("\\bif\\b"), QStringLiteral("\\belse\\b"), QStringLiteral("\\bfor\\b"),
             QStringLiteral("\\brange\\b"), QStringLiteral("\\bswitch\\b"), QStringLiteral("\\bcase\\b"),
             QStringLiteral("\\bdefault\\b"), QStringLiteral("\\bbreak\\b"), QStringLiteral("\\bcontinue\\b"),
             QStringLiteral("\\bfallthrough\\b"), QStringLiteral("\\bconst\\b"), QStringLiteral("\\bmap\\b"),
             QStringLiteral("\\bnil\\b"), QStringLiteral("\\btrue\\b"), QStringLiteral("\\bfalse\\b"),
             QStringLiteral("\\biota\\b")},
            QStringLiteral("//"), true, true, false
        }},
        // 12: Rust
        {12, {
            {QStringLiteral("\\bfn\\b"), QStringLiteral("\\blet\\b"), QStringLiteral("\\bmut\\b"),
             QStringLiteral("\\bstruct\\b"), QStringLiteral("\\benum\\b"), QStringLiteral("\\bimpl\\b"),
             QStringLiteral("\\btrait\\b"), QStringLiteral("\\bpub\\b"), QStringLiteral("\\buse\\b"),
             QStringLiteral("\\bmod\\b"), QStringLiteral("\\bmatch\\b"), QStringLiteral("\\bif\\b"),
             QStringLiteral("\\belse\\b"), QStringLiteral("\\bloop\\b"), QStringLiteral("\\bwhile\\b"),
             QStringLiteral("\\bfor\\b"), QStringLiteral("\\bin\\b"), QStringLiteral("\\breturn\\b"),
             QStringLiteral("\\bself\\b"), QStringLiteral("\\bSelf\\b"), QStringLiteral("\\btype\\b"),
             QStringLiteral("\\bwhere\\b"), QStringLiteral("\\basync\\b"), QStringLiteral("\\bawait\\b"),
             QStringLiteral("\\bunsafe\\b"), QStringLiteral("\\bconst\\b"), QStringLiteral("\\bstatic\\b"),
             QStringLiteral("\\btrue\\b"), QStringLiteral("\\bfalse\\b")},
            QStringLiteral("//"), true, false, false
        }},
        // 13: Zig
        {13, {
            {QStringLiteral("\\bconst\\b"), QStringLiteral("\\bvar\\b"), QStringLiteral("\\bfn\\b"),
             QStringLiteral("\\bpub\\b"), QStringLiteral("\\btest\\b"), QStringLiteral("\\bstruct\\b"),
             QStringLiteral("\\benum\\b"), QStringLiteral("\\bunion\\b"), QStringLiteral("\\bif\\b"),
             QStringLiteral("\\belse\\b"), QStringLiteral("\\bswitch\\b"), QStringLiteral("\\bwhile\\b"),
             QStringLiteral("\\bfor\\b"), QStringLiteral("\\breturn\\b"), QStringLiteral("\\btry\\b"),
             QStringLiteral("\\bcatch\\b"), QStringLiteral("\\bdefer\\b"), QStringLiteral("\\berrdefer\\b"),
             QStringLiteral("\\bunreachable\\b"), QStringLiteral("\\basm\\b"), QStringLiteral("\\bcomptime\\b"),
             QStringLiteral("\\bnull\\b"), QStringLiteral("\\btrue\\b"), QStringLiteral("\\bfalse\\b")},
            QStringLiteral("//"), true, false, false
        }},
        // 14: Pascal / Free Pascal
        {14, {
            {QStringLiteral("\\bprogram\\b"), QStringLiteral("\\bunit\\b"), QStringLiteral("\\binterface\\b"),
             QStringLiteral("\\bimplementation\\b"), QStringLiteral("\\bvar\\b"), QStringLiteral("\\btype\\b"),
             QStringLiteral("\\bconst\\b"), QStringLiteral("\\bprocedure\\b"), QStringLiteral("\\bfunction\\b"),
             QStringLiteral("\\bbegin\\b"), QStringLiteral("\\bend\\b"), QStringLiteral("\\bif\\b"),
             QStringLiteral("\\bthen\\b"), QStringLiteral("\\belse\\b"), QStringLiteral("\\bcase\\b"),
             QStringLiteral("\\bof\\b"), QStringLiteral("\\bwhile\\b"), QStringLiteral("\\bdo\\b"),
             QStringLiteral("\\bfor\\b"), QStringLiteral("\\bto\\b"), QStringLiteral("\\bdownto\\b"),
             QStringLiteral("\\brepeat\\b"), QStringLiteral("\\buntil\\b"), QStringLiteral("\\buses\\b"),
             QStringLiteral("\\bclass\\b"), QStringLiteral("\\brecord\\b"), QStringLiteral("\\bnil\\b"),
             QStringLiteral("\\btrue\\b"), QStringLiteral("\\bfalse\\b")},
            QStringLiteral("//"), true, false, false
        }},
        // 15: Lua
        {15, {
            {QStringLiteral("\\bfunction\\b"), QStringLiteral("\\blocal\\b"), QStringLiteral("\\bend\\b"),
             QStringLiteral("\\bif\\b"), QStringLiteral("\\bthen\\b"), QStringLiteral("\\belse\\b"),
             QStringLiteral("\\belseif\\b"), QStringLiteral("\\bwhile\\b"), QStringLiteral("\\bdo\\b"),
             QStringLiteral("\\brepeat\\b"), QStringLiteral("\\buntil\\b"), QStringLiteral("\\bfor\\b"),
             QStringLiteral("\\bin\\b"), QStringLiteral("\\breturn\\b"), QStringLiteral("\\bbreak\\b"),
             QStringLiteral("\\bnil\\b"), QStringLiteral("\\btrue\\b"), QStringLiteral("\\bfalse\\b"),
             QStringLiteral("\\band\\b"), QStringLiteral("\\bor\\b"), QStringLiteral("\\bnot\\b")},
            QStringLiteral("--"), true, false, false
        }},
        // 16: SQL
        {16, {
            {QStringLiteral("\\bselect\\b"), QStringLiteral("\\bfrom\\b"), QStringLiteral("\\bwhere\\b"),
             QStringLiteral("\\binsert\\b"), QStringLiteral("\\binto\\b"), QStringLiteral("\\bupdate\\b"),
             QStringLiteral("\\bdelete\\b"), QStringLiteral("\\bcreate\\b"), QStringLiteral("\\btable\\b"),
             QStringLiteral("\\balter\\b"), QStringLiteral("\\bdrop\\b"), QStringLiteral("\\bjoin\\b"),
             QStringLiteral("\\binner\\b"), QStringLiteral("\\bleft\\b"), QStringLiteral("\\bright\\b"),
             QStringLiteral("\\bouter\\b"), QStringLiteral("\\bgroup\\b"), QStringLiteral("\\bby\\b"),
             QStringLiteral("\\border\\b"), QStringLiteral("\\bhaving\\b"), QStringLiteral("\\blimit\\b"),
             QStringLiteral("\\bas\\b"), QStringLiteral("\\band\\b"), QStringLiteral("\\bor\\b"),
             QStringLiteral("\\bnot\\b"), QStringLiteral("\\bnull\\b"), QStringLiteral("\\btrue\\b"),
             QStringLiteral("\\bfalse\\b")},
            QStringLiteral("--"), true, true, false
        }},
        // 17: YAML
        {17, {
            {QStringLiteral("\\btrue\\b"), QStringLiteral("\\bfalse\\b"), QStringLiteral("\\bnull\\b"),
             QStringLiteral("\\byes\\b"), QStringLiteral("\\bno\\b"), QStringLiteral("\\bon\\b"),
             QStringLiteral("\\boff\\b")},
            QStringLiteral("#"), true, false, false
        }},
        // 18: Java / C#
        {18, {
            {QStringLiteral("\\bpublic\\b"), QStringLiteral("\\bprivate\\b"), QStringLiteral("\\bprotected\\b"),
             QStringLiteral("\\bclass\\b"), QStringLiteral("\\binterface\\b"), QStringLiteral("\\benum\\b"),
             QStringLiteral("\\bextends\\b"), QStringLiteral("\\bimplements\\b"), QStringLiteral("\\bstatic\\b"),
             QStringLiteral("\\bfinal\\b"), QStringLiteral("\\bvoid\\b"), QStringLiteral("\\bint\\b"),
             QStringLiteral("\\bdouble\\b"), QStringLiteral("\\bfloat\\b"), QStringLiteral("\\bchar\\b"),
             QStringLiteral("\\bboolean\\b"), QStringLiteral("\\bbool\\b"), QStringLiteral("\\bstring\\b"),
             QStringLiteral("\\bString\\b"), QStringLiteral("\\bif\\b"), QStringLiteral("\\belse\\b"),
             QStringLiteral("\\bwhile\\b"), QStringLiteral("\\bfor\\b"), QStringLiteral("\\breturn\\b"),
             QStringLiteral("\\bnew\\b"), QStringLiteral("\\bthis\\b"), QStringLiteral("\\bsuper\\b"),
             QStringLiteral("\\btry\\b"), QStringLiteral("\\bcatch\\b"), QStringLiteral("\\bfinally\\b"),
             QStringLiteral("\\bthrow\\b"), QStringLiteral("\\bnull\\b"), QStringLiteral("\\btrue\\b"),
             QStringLiteral("\\bfalse\\b"), QStringLiteral("\\bnamespace\\b"), QStringLiteral("\\busing\\b"),
             QStringLiteral("\\bvar\\b"), QStringLiteral("\\basync\\b"), QStringLiteral("\\bawait\\b")},
            QStringLiteral("//"), true, false, false
        }},
        // 19: PHP
        {19, {
            {QStringLiteral("\\bfunction\\b"), QStringLiteral("\\bclass\\b"), QStringLiteral("\\bpublic\\b"),
             QStringLiteral("\\bprivate\\b"), QStringLiteral("\\bprotected\\b"), QStringLiteral("\\bstatic\\b"),
             QStringLiteral("\\bif\\b"), QStringLiteral("\\belse\\b"), QStringLiteral("\\belseif\\b"),
             QStringLiteral("\\bwhile\\b"), QStringLiteral("\\bfor\\b"), QStringLiteral("\\bforeach\\b"),
             QStringLiteral("\\bas\\b"), QStringLiteral("\\breturn\\b"), QStringLiteral("\\becho\\b"),
             QStringLiteral("\\bprint\\b"), QStringLiteral("\\bnew\\b"), QStringLiteral("\\bnull\\b"),
             QStringLiteral("\\btrue\\b"), QStringLiteral("\\bfalse\\b"), QStringLiteral("\\buse\\b"),
             QStringLiteral("\\bnamespace\\b")},
            QStringLiteral("//"), true, false, false
        }}
    };
    return defs;
}
}

int CodeHighlighter::languageStateForName(const QString &name) {
    const QString lang = name.toLower().trimmed();
    if (lang.isEmpty()) return 1;

    if (lang == QLatin1String("bash") || lang == QLatin1String("sh") || lang == QLatin1String("shell")
            || lang == QLatin1String("zsh") || lang == QLatin1String("fish")) {
        return 2;
    }
    if (lang == QLatin1String("nim")) return 3;
    if (lang == QLatin1String("python") || lang == QLatin1String("py")) return 4;
    if (lang == QLatin1String("toml")) return 5;
    if (lang == QLatin1String("json")) return 6;
    if (lang == QLatin1String("c")) return 7;
    if (lang == QLatin1String("cpp") || lang == QLatin1String("c++") || lang == QLatin1String("cxx")
            || lang == QLatin1String("cc")) return 8;
    if (lang == QLatin1String("js") || lang == QLatin1String("javascript") || lang == QLatin1String("ts")
            || lang == QLatin1String("typescript")) return 9;
    if (lang == QLatin1String("ruby") || lang == QLatin1String("rb")) return 10;
    if (lang == QLatin1String("go") || lang == QLatin1String("golang")) return 11;
    if (lang == QLatin1String("rust") || lang == QLatin1String("rs")) return 12;
    if (lang == QLatin1String("zig")) return 13;
    if (lang == QLatin1String("pascal") || lang == QLatin1String("pas") || lang == QLatin1String("fpc")) return 14;
    if (lang == QLatin1String("lua")) return 15;
    if (lang == QLatin1String("sql")) return 16;
    if (lang == QLatin1String("yaml") || lang == QLatin1String("yml")) return 17;
    if (lang == QLatin1String("java") || lang == QLatin1String("cs") || lang == QLatin1String("csharp")) return 18;
    if (lang == QLatin1String("php")) return 19;

    return 1;
}

QStringList CodeHighlighter::supportedLanguages() {
    return {
        QStringLiteral("Bash"), QStringLiteral("C"), QStringLiteral("C++"),
        QStringLiteral("C#"), QStringLiteral("Go"), QStringLiteral("Java"),
        QStringLiteral("JavaScript"), QStringLiteral("JSON"), QStringLiteral("Lua"),
        QStringLiteral("Nim"), QStringLiteral("Pascal"), QStringLiteral("PHP"),
        QStringLiteral("Python"), QStringLiteral("Ruby"), QStringLiteral("Rust"),
        QStringLiteral("SQL"), QStringLiteral("TOML"), QStringLiteral("TypeScript"),
        QStringLiteral("YAML"), QStringLiteral("Zig")
    };
}

void CodeHighlighter::highlight(const QString &text, int languageState, 
                                MarkdownHighlighter *highlighter, const Formats &formats) {
    const auto &defs = languageDefinitions();
    auto it = defs.constFind(languageState);
    if (it == defs.cend())
        return;

    const LanguageDef &def = it.value();

    if (!def.keywords.isEmpty())
        highlightKeywords(text, def.keywords, highlighter, formats);

    if (def.isToml) {
        static const QRegularExpression tableRe(QStringLiteral("^\\s*\\[.*\\]"));
        const QRegularExpressionMatch tableMatch = tableRe.match(text);
        if (tableMatch.hasMatch()) {
            highlighter->applyFormat(tableMatch.capturedStart(), tableMatch.capturedLength(), formats.keyword);
        }
    }

    // String literals
    static const QRegularExpression doubleQuoteRe(QStringLiteral("\"[^\"]*\""));
    static const QRegularExpression singleDoubleQuoteRe(QStringLiteral("(\"[^\"]*\"|'[^']*')"));
    static const QRegularExpression allQuoteRe(QStringLiteral("(\"[^\"]*\"|'[^']*'|`[^`]*`)"));

    const QRegularExpression &stringRe = def.supportsBackticks ? allQuoteRe
        : (def.supportsSingleQuotes ? singleDoubleQuoteRe : doubleQuoteRe);

    QRegularExpressionMatchIterator stringMatches = stringRe.globalMatch(text);
    while (stringMatches.hasNext()) {
        const QRegularExpressionMatch match = stringMatches.next();
        highlighter->applyFormat(match.capturedStart(), match.capturedLength(), formats.string);
    }

    if (!def.commentMarker.isEmpty()) {
        highlightLineComment(text, def.commentMarker, def.supportsSingleQuotes, def.supportsBackticks,
                             highlighter, formats);
    }
}

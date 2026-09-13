#include <QFont>
#include <QFontDatabase>
#include <QApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlError>
#include <QQuickStyle>
#include <QSettings>
#include <QUrl>
#include <QVariant>
#include <QWindow>
#include <QFile>
#include <QByteArray>

#include <cstdio>

#include "backend.h"
#include "systemtheme.h"

namespace {
constexpr const char usage[] =
    "Omawrite Standalone - a minimal Markdown writing application.\n\n"
    "Usage: omawrite [FILE]\n\n"
    "Options:\n"
    "  -h, --help    Show this help and exit\n";

// Handles terminal-only options before the graphical application starts.
int commandLineExitCode(int argc, char *argv[]) {
    bool optionsEnded = false;
    for (int i = 1; i < argc; ++i) {
        const QByteArray argument(argv[i]);
        if (!optionsEnded && argument == "--") {
            optionsEnded = true;
        } else if (!optionsEnded && (argument == "-h" || argument == "--help")) {
            std::fputs(usage, stdout);
            return 0;
        } else if (!optionsEnded && argument.startsWith('-')) {
            std::fprintf(stderr, "omawrite: unrecognized option '%s'\n\n%s",
                         argument.constData(), usage);
            return 1;
        }
    }
    return -1;
}

// Copies upstream settings once so existing standalone users keep their workspace preferences.
void migrateLegacySettings() {
    QSettings current;
    if (!current.allKeys().isEmpty())
        return;
    QSettings legacy(QStringLiteral("Omacom"), QStringLiteral("omawrite"));
    const QStringList keys{
        QStringLiteral("file/lastSaveDirectory"), QStringLiteral("file/browseDirectory"),
        QStringLiteral("file/sidebarWidth"), QStringLiteral("window/x"),
        QStringLiteral("window/y"), QStringLiteral("window/width"),
        QStringLiteral("window/height"), QStringLiteral("window/maximized")};
    for (const QString &key : keys) {
        if (legacy.contains(key))
            current.setValue(key, legacy.value(key));
    }
}
}

int main(int argc, char *argv[]) {
    const int earlyExit = commandLineExitCode(argc, argv);
    if (earlyExit >= 0)
        return earlyExit;

    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("omawrite-standalone"));
    app.setApplicationDisplayName(QStringLiteral("Omawrite Standalone"));
    app.setDesktopFileName(QStringLiteral("omawrite"));
    app.setWindowIcon(QIcon::fromTheme(QStringLiteral("omawrite")));

    QFontDatabase::addApplicationFont(QStringLiteral(":/fonts/iAWriterMonoS-Regular.ttf"));
    QFontDatabase::addApplicationFont(QStringLiteral(":/fonts/iAWriterMonoS-Italic.ttf"));
    QFontDatabase::addApplicationFont(QStringLiteral(":/fonts/iAWriterMonoS-Bold.ttf"));
    QFontDatabase::addApplicationFont(QStringLiteral(":/fonts/iAWriterMonoS-BoldItalic.ttf"));
    app.setOrganizationName(QStringLiteral("Vyrnexis"));
    app.setOrganizationDomain(QStringLiteral("vyrnexis.github.io"));
    migrateLegacySettings();

    QQuickStyle::setStyle(QStringLiteral("Material"));

    Backend backend(&app);
    SystemTheme systemTheme(&app);
    backend.setDarkMode(systemTheme.darkMode());
    QObject::connect(&systemTheme, &SystemTheme::darkModeChanged, &backend,
                     &Backend::setDarkMode);

    // Carry the desktop's text scale into the default font, so the chrome that
    // inherits it (dialog titles, buttons) grows along with the writing area.
    const QFont interfaceFont(QStringLiteral("iA Writer Mono S"));
    const qreal basePointSize = interfaceFont.pointSizeF() > 0
        ? interfaceFont.pointSizeF()
        : app.font().pointSizeF();
    const auto applyInterfaceFont = [&app, interfaceFont, basePointSize](qreal textScale) {
        QFont scaled = interfaceFont;
        scaled.setPointSizeF(basePointSize * textScale);
        app.setFont(scaled);
    };
    backend.setSystemTextScale(systemTheme.textScale());
    applyInterfaceFont(backend.textScale());
    QObject::connect(&backend, &Backend::textScaleChanged, &app,
                     [&backend, applyInterfaceFont]() {
        applyInterfaceFont(backend.textScale());
    });
    QObject::connect(&systemTheme, &SystemTheme::textScaleChanged, &backend,
                     [&backend](qreal textScale) {
        backend.setSystemTextScale(textScale);
    });

    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlApplicationEngine::warnings, &app,
                     [](const QList<QQmlError> &warnings) {
        for (const QQmlError &warning : warnings)
            qWarning().noquote() << warning.toString();
    });
    engine.rootContext()->setContextProperty(QStringLiteral("backend"), &backend);

    engine.load(QUrl(QStringLiteral("qrc:/Main.qml")));
    if (engine.rootObjects().isEmpty()) {
        qCritical() << "Could not load the Omawrite interface; resource available:"
                    << QFile::exists(QStringLiteral(":/Main.qml"));
        return -1;
    }

    backend.setParentWindow(qobject_cast<QWindow *>(engine.rootObjects().constFirst()));

    QString requestedPath;
    const QStringList args = app.arguments();
    for (int i = 1; i < args.size(); ++i) {
        if (args.at(i) == QStringLiteral("--"))
            continue;
        requestedPath = args.at(i);
        break;
    }
    if (!requestedPath.isEmpty()) {
        const QUrl requestedFile = QUrl::fromLocalFile(requestedPath);
        if (backend.modified()) {
            QMetaObject::invokeMethod(engine.rootObjects().constFirst(), "requestOpen",
                                      Q_ARG(QVariant, QVariant::fromValue(requestedFile)));
        } else {
            backend.open(requestedFile);
        }
    }

    return app.exec();
}

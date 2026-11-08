// riffer — slow down music to practice along.

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QFileInfo>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QUrl>
#include <cstdio>

#include "backend.h"
#include "theme.h"
#include "translator.h"
#include "waveformitem.h"

// Prints the sections saved for a track so agents and scripts can read them
// without a display. Exits 0 or 1 with errors on stderr.
static int sectionsCli(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    app.setApplicationName("riffer");
    app.setApplicationVersion("0.1.0");

    QCommandLineParser parser;
    parser.setApplicationDescription("Print the sections saved for a track.");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({{"j", "json"}, "Print JSON instead of a table."});
    parser.addPositionalArgument("file", "The audio file to read sections for.");
    // The subcommand token rides along in the arguments; drop it before
    // parsing so only the file remains positional.
    QStringList arguments = QCoreApplication::arguments();
    arguments.removeAt(1);
    parser.process(arguments);

    const QStringList positional = parser.positionalArguments();
    if (positional.isEmpty()) {
        std::fputs("usage: riffer sections <file> [--json]\n", stderr);
        return 1;
    }
    const QString file = positional.first();
    const QJsonObject saved = Backend::readSidecar(file);
    const QJsonArray sections = saved.value(QStringLiteral("sections")).toArray();
    if (saved.isEmpty() || sections.isEmpty()) {
        std::fputs(qUtf8Printable(QStringLiteral("No saved sections for %1\n").arg(file)), stderr);
        return 1;
    }
    if (parser.isSet("json")) {
        std::fputs(QJsonDocument(sections).toJson(QJsonDocument::Compact).constData(), stdout);
        std::fputc('\n', stdout);
    } else {
        for (const QJsonValue &value : sections) {
            const QJsonObject object = value.toObject();
            std::fprintf(stdout, "%9.3f %9.3f  %s\n", object.value("start").toDouble(),
                         object.value("end").toDouble(),
                         qUtf8Printable(object.value("name").toString()));
        }
    }
    return 0;
}

int main(int argc, char *argv[]) {
    // Handled before any QApplication exists, so they never open a window.
    for (int i = 1; i < argc; ++i) {
        if (qstrcmp(argv[i], "--version") == 0 || qstrcmp(argv[i], "-v") == 0) {
            std::printf("riffer 0.1.0\n");
            return 0;
        }
        if (qstrcmp(argv[i], "--help") == 0 || qstrcmp(argv[i], "-h") == 0) {
            std::printf(
                "riffer — slow down music to practice along.\n\n"
                "usage: riffer [file]        open a track in the window\n"
                "       riffer open [file]   the same, explicitly\n"
                "       riffer sections <file> [--json]\n"
                "                            print the sections saved for a track\n"
                "       riffer --help        this help\n"
                "       riffer --version     version\n\n"
                "Space plays and loops; press ? in the window for every shortcut.\n");
            return 0;
        }
    }
    if (argc > 1 && qstrcmp(argv[1], "sections") == 0)
        return sectionsCli(argc, argv);

    QGuiApplication app(argc, argv);
    app.setApplicationName("riffer");
    app.setApplicationVersion("0.1.0");

    // Matches the window to riffer.desktop, so the compositor (Wayland
    // app_id) and the launcher pick up the installed icon.
    app.setDesktopFileName("riffer");
    app.setWindowIcon(QIcon::fromTheme("riffer"));

    QQuickStyle::setStyle("Material");
    qmlRegisterType<WaveformView>("Riffer", 1, 0, "WaveformView");

    // Installed before anything is translated, and every qsTr() binding is
    // re-evaluated when the Omarchy language changes.
    Translator translator;
    app.installTranslator(&translator);

    Theme theme;
    Backend backend;

    QQmlApplicationEngine engine;
    QObject::connect(&translator, &Translator::changed, &engine, &QQmlEngine::retranslate);
    engine.rootContext()->setContextProperty("translator", &translator);
    engine.rootContext()->setContextProperty("theme", &theme);
    engine.rootContext()->setContextProperty("backend", &backend);
    engine.load(QUrl("qrc:/Main.qml"));
    if (engine.rootObjects().isEmpty())
        return 1;

    // riffer <file>, riffer open <file>, or bare for the picker.
    const QStringList arguments = app.arguments();
    QString fileToOpen;
    if (arguments.size() > 1 && arguments.at(1) != QStringLiteral("open"))
        fileToOpen = arguments.at(1);
    else if (arguments.size() > 2)
        fileToOpen = arguments.at(2);
    if (!fileToOpen.isEmpty() && QFileInfo::exists(fileToOpen))
        backend.openPath(QUrl::fromLocalFile(QFileInfo(fileToOpen).absoluteFilePath()));

    return app.exec();
}

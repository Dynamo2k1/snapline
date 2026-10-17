#include "app/Application.h"
#include "app/SingleInstance.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QIcon>
#include <QTimer>

#include <cstdio>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("snapline"));
    QApplication::setOrganizationName(QStringLiteral("snapline"));
    QApplication::setApplicationDisplayName(QStringLiteral("Snapline"));
    QApplication::setApplicationVersion(QStringLiteral(SNAPLINE_VERSION));
    QApplication::setDesktopFileName(QStringLiteral(SNAPLINE_APP_ID));
    QApplication::setQuitOnLastWindowClosed(false);
    QApplication::setWindowIcon(QIcon::fromTheme(QStringLiteral(SNAPLINE_APP_ID), QIcon(QStringLiteral(":/icons/snapline.svg"))));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Region screenshot tool with annotation and upload."));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(QStringLiteral("command"),
                                 QStringLiteral("gui          select an area (default hotkey action)\n"
                                                "full         save the full screen immediately\n"
                                                "full-upload  upload the full screen immediately\n"
                                                "settings     open the settings window\n"
                                                "quit         stop the running instance\n"
                                                "(none)       start in the system tray"),
                                 QStringLiteral("[command]"));
    parser.process(app);

    const QString command = parser.positionalArguments().value(0);
    static const QStringList known = {QString(), QStringLiteral("gui"), QStringLiteral("full"),
                                      QStringLiteral("full-upload"), QStringLiteral("settings"), QStringLiteral("quit")};
    if (!known.contains(command)) {
        std::fprintf(stderr, "snapline: unknown command '%s'\n", qPrintable(command));
        parser.showHelp(1);
    }

    snapline::SingleInstance instance;
    // Launching again while running (e.g. from the app menu) takes a screenshot.
    if (instance.sendToRunning(command.isEmpty() ? QStringLiteral("gui") : command))
        return 0;
    if (command == QLatin1String("quit"))
        return 0;
    if (!instance.listen())
        std::fprintf(stderr, "snapline: could not create the instance socket; commands from other launches won't work\n");

    snapline::Application application;
    QObject::connect(&instance, &snapline::SingleInstance::commandReceived, &application,
                     &snapline::Application::handleCommand);
    application.start();
    if (!command.isEmpty())
        QTimer::singleShot(0, &application, [&application, command]() { application.handleCommand(command); });

    return app.exec();
}

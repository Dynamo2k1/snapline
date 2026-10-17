#include "overlay/CaptureController.h"
#include "overlay/CaptureSession.h"
#include "overlay/OverlayWindow.h"
#include "overlay/Toolbars.h"

#include <QApplication>
#include <QGuiApplication>
#include <QLinearGradient>
#include <QPainter>
#include <QScreen>
#include <QSignalSpy>
#include <QtTest>

using namespace snapline;

namespace {

Capture syntheticCapture()
{
    QScreen *screen = QGuiApplication::primaryScreen();
    QImage image(screen->geometry().size() * 2, QImage::Format_RGB32); // pretend 2x HiDPI
    QPainter p(&image);
    QLinearGradient g(0, 0, image.width(), image.height());
    g.setColorAt(0, QColor(30, 90, 160));
    g.setColorAt(1, QColor(220, 230, 240));
    p.fillRect(image.rect(), g);
    p.setPen(Qt::black);
    QFont f = p.font();
    f.setPixelSize(40);
    p.setFont(f);
    for (int y = 60; y < image.height(); y += 120)
        p.drawText(40, y, QStringLiteral("Sample document text line for blur testing 0123456789"));
    p.end();
    Capture c;
    c.screens.push_back({screen->name(), screen->geometry(), image});
    return c;
}

} // namespace

class OverlayTest : public QObject {
    Q_OBJECT

private slots:
    void rendersSelectionAnnotationsAndToolbars()
    {
        CaptureSession session(syntheticCapture(), nullptr);
        OverlayWindow window(&session, 0, QGuiApplication::primaryScreen());
        window.setAttribute(Qt::WA_DeleteOnClose, false);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));

        // Drag a selection with the mouse like a user would.
        QTest::mousePress(&window, Qt::LeftButton, {}, QPoint(100, 80));
        QTest::mouseMove(&window, QPoint(300, 200));
        QTest::mouseMove(&window, QPoint(500, 380));
        QTest::mouseRelease(&window, Qt::LeftButton, {}, QPoint(500, 380));
        QCOMPARE(session.selection(), QRect(QPoint(100, 80), QPoint(500, 380)));
        QCOMPARE(session.resultPixelSize(), QSize(802, 602));

        session.setTool(Tool::Arrow);
        QTest::mousePress(&window, Qt::LeftButton, {}, QPoint(140, 120));
        QTest::mouseMove(&window, QPoint(260, 200));
        QTest::mouseRelease(&window, Qt::LeftButton, {}, QPoint(260, 200));

        session.setTool(Tool::Rect);
        QTest::mousePress(&window, Qt::LeftButton, {}, QPoint(280, 110));
        QTest::mouseMove(&window, QPoint(460, 170));
        QTest::mouseRelease(&window, Qt::LeftButton, {}, QPoint(460, 170));

        session.setTool(Tool::Blur);
        QTest::mousePress(&window, Qt::LeftButton, {}, QPoint(120, 260));
        QTest::mouseMove(&window, QPoint(480, 360));
        QTest::mouseRelease(&window, Qt::LeftButton, {}, QPoint(480, 360));

        session.setColor(QColor(0, 160, 255));
        session.setTool(Tool::Marker);
        QTest::mousePress(&window, Qt::LeftButton, {}, QPoint(140, 230));
        QTest::mouseMove(&window, QPoint(300, 232));
        QTest::mouseRelease(&window, Qt::LeftButton, {}, QPoint(300, 232));

        QCOMPARE(session.annotations().size(), 4);
        QVERIFY(session.canUndo());

        const auto bars = window.findChildren<QFrame *>(QStringLiteral("snaplineBar"));
        QCOMPARE(bars.size(), 2);
        for (QFrame *bar : bars)
            QVERIFY2(bar->isVisible(), "toolbars should be shown next to a finished selection");

        const QImage result = session.result();
        QCOMPARE(result.size(), QSize(802, 602));

        const QString out = qEnvironmentVariable("SNAPLINE_TEST_OUT");
        if (!out.isEmpty()) {
            window.grab().save(out + QStringLiteral("/overlay.png"));
            result.save(out + QStringLiteral("/result.png"));
        }

        // Undo removes the last annotation; Escape cancels the capture.
        QSignalSpy actions(&window, &OverlayWindow::action);
        QTest::keyClick(&window, Qt::Key_Z, Qt::ControlModifier);
        QTest::keyClick(&window, Qt::Key_Escape);
        QCOMPARE(actions.count(), 2);
        QCOMPARE(actions.at(0).at(0).value<Action>(), Action::Undo);
        QCOMPARE(actions.at(1).at(0).value<Action>(), Action::Close);
    }

    void copyActionDeliversAnnotatedImage()
    {
        CaptureController controller(syntheticCapture(), QRect(50, 50, 200, 100), nullptr);
        QSignalSpy finished(&controller, &CaptureController::finished);
        controller.show();
        QTest::qWait(50);

        auto windows = QApplication::topLevelWidgets();
        OverlayWindow *overlay = nullptr;
        for (QWidget *w : windows) {
            if (auto *o = qobject_cast<OverlayWindow *>(w); o && o->isVisible())
                overlay = o;
        }
        QVERIFY(overlay);
        QTest::keyClick(overlay, Qt::Key_C, Qt::ControlModifier);

        QCOMPARE(finished.count(), 1);
        QCOMPARE(finished.at(0).at(0).value<Action>(), Action::Copy);
        QCOMPARE(finished.at(0).at(1).value<QImage>().size(), QSize(400, 200));
        QCOMPARE(finished.at(0).at(2).toRect(), QRect(50, 50, 200, 100));
    }
};

QTEST_MAIN(OverlayTest)
#include "test_overlay.moc"

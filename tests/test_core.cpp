#include "capture/Capture.h"
#include "core/FileNameTemplate.h"
#include "core/Hotkeys.h"
#include "core/JsonPath.h"
#include "tools/AnnotationStack.h"

#include <QTemporaryDir>
#include <QFile>
#include <QtTest>

using namespace snapline;

class CoreTest : public QObject {
    Q_OBJECT

private slots:
    void jsonPath_data()
    {
        QTest::addColumn<QByteArray>("body");
        QTest::addColumn<QString>("path");
        QTest::addColumn<QString>("expected");
        QTest::addColumn<bool>("ok");

        QTest::newRow("imgur") << QByteArray("{\"data\":{\"link\":\"https://i.imgur.com/x.png\"},\"success\":true}")
                               << "data.link" << "https://i.imgur.com/x.png" << true;
        QTest::newRow("array") << QByteArray("{\"files\":[{\"url\":\"https://a/1\"},{\"url\":\"https://a/2\"}]}")
                               << "files[1].url" << "https://a/2" << true;
        QTest::newRow("bare body") << QByteArray("  https://example.com/a.png\n") << "" << "https://example.com/a.png" << true;
        QTest::newRow("missing") << QByteArray("{\"data\":{}}") << "data.link" << "" << false;
        QTest::newRow("not json") << QByteArray("<html>") << "data.link" << "" << false;
        QTest::newRow("index out of range") << QByteArray("{\"a\":[]}") << "a[0]" << "" << false;
    }

    void jsonPath()
    {
        QFETCH(QByteArray, body);
        QFETCH(QString, path);
        QFETCH(QString, expected);
        QFETCH(bool, ok);
        bool actualOk = !ok;
        QCOMPARE(extractJsonPath(body, path, &actualOk), expected);
        QCOMPARE(actualOk, ok);
    }

    void fileNameTemplate()
    {
        const QDateTime t(QDate(2026, 10, 17), QTime(9, 5, 3));
        QCOMPARE(expandFileNameTemplate("Screenshot_%Y-%m-%d_%H-%M-%S", t, 1), QString("Screenshot_2026-10-17_09-05-03"));
        QCOMPARE(expandFileNameTemplate("shot_%n", t, 42), QString("shot_42"));
        QCOMPARE(expandFileNameTemplate("100%% a/b", t, 0), QString("100% a_b"));
        QCOMPARE(expandFileNameTemplate("   ", t, 0), QString("Screenshot"));
    }

    void uniquePath()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString first = uniqueFilePath(dir.path(), "a", "png");
        QVERIFY(first.endsWith("/a.png"));
        QFile file(first);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();
        QVERIFY(uniqueFilePath(dir.path(), "a", "png").endsWith("/a_1.png"));
    }

    void hotkeyStrings()
    {
        QCOMPARE(portalTrigger(QKeySequence(Qt::Key_Print)), QString("Print"));
        QCOMPARE(portalTrigger(QKeySequence(Qt::SHIFT | Qt::Key_Print)), QString("SHIFT+Print"));
        QCOMPARE(portalTrigger(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_S)), QString("CTRL+ALT+s"));
        QCOMPARE(gtkAccelerator(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F5)), QString("<Control><Shift>F5"));
        QVERIFY(portalTrigger(QKeySequence()).isEmpty());
    }

    void hotkeyValidation()
    {
        QString id;
        QList<HotkeyBinding> ok = {{"main", QKeySequence(Qt::Key_Print), true},
                                   {"save_full", QKeySequence(Qt::SHIFT | Qt::Key_Print), true},
                                   {"upload_full", QKeySequence(), false}};
        QCOMPARE(validateHotkeys(ok), HotkeyError::None);

        QList<HotkeyBinding> invalid = ok;
        invalid[2].enabled = true;
        QCOMPARE(validateHotkeys(invalid, &id), HotkeyError::Invalid);
        QCOMPARE(id, QString("upload_full"));

        QList<HotkeyBinding> dup = ok;
        dup[1].sequence = QKeySequence(Qt::Key_Print);
        QCOMPARE(validateHotkeys(dup, &id), HotkeyError::Duplicate);
        QCOMPARE(id, QString("save_full"));
    }

    void annotationUndoRedo()
    {
        AnnotationStack stack;
        Style style;
        auto line = beginAnnotation(Tool::Line, {0, 0}, style);
        line->update({10, 10}, false);
        stack.push(line);
        auto empty = beginAnnotation(Tool::Rect, {5, 5}, style);
        stack.push(empty); // zero-size shapes are dropped
        QCOMPARE(stack.size(), 1);

        QVERIFY(stack.undo());
        QCOMPARE(stack.size(), 0);
        QVERIFY(stack.canRedo());
        QVERIFY(stack.redo());
        QCOMPARE(stack.size(), 1);

        stack.undo();
        auto pen = beginAnnotation(Tool::Pen, {1, 1}, style);
        stack.push(pen);
        QVERIFY(!stack.canRedo()); // new edits discard the redo history
    }

    void arrowHeadPointsAtTarget()
    {
        const QPolygonF head = arrowHead({0, 0}, {100, 0}, 3);
        QCOMPARE(head.size(), 3);
        QCOMPARE(head[0], QPointF(100, 0));
        QVERIFY(head[1].x() < 100 && head[2].x() < 100);
        QCOMPARE(head[1].y(), -head[2].y());
    }

    void captureRenderStitchesMixedDpr()
    {
        // Left screen 100x100 @1x (red), right screen 100x100 @2x (blue).
        QImage left(100, 100, QImage::Format_RGB32);
        left.fill(Qt::red);
        QImage right(200, 200, QImage::Format_RGB32);
        right.fill(Qt::blue);
        Capture c;
        c.screens = {{"L", QRect(0, 0, 100, 100), left}, {"R", QRect(100, 0, 100, 100), right}};

        QCOMPARE(c.virtualGeometry(), QRect(0, 0, 200, 100));
        QCOMPARE(c.maxDprIn(QRect(90, 0, 20, 20)), 2.0);

        const QImage out = c.render(QRect(90, 10, 20, 10), 2.0);
        QCOMPARE(out.size(), QSize(40, 20));
        QCOMPARE(QColor(out.pixel(5, 5)), QColor(Qt::red));
        QCOMPARE(QColor(out.pixel(35, 5)), QColor(Qt::blue));
    }

    void captureFromDesktopImage()
    {
        QImage desktop(400, 200, QImage::Format_RGB32); // whole desktop at 2x
        desktop.fill(Qt::green);
        const Capture c = Capture::fromDesktopImage(desktop, {{"A", QRect(0, 0, 100, 100)}, {"B", QRect(100, 0, 100, 100)}});
        QCOMPARE(c.screens.size(), 2);
        QCOMPARE(c.screens[1].image.size(), QSize(200, 200));
        QCOMPARE(c.screens[1].dpr(), 2.0);
    }
};

QTEST_MAIN(CoreTest)
#include "test_core.moc"

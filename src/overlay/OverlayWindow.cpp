#include "overlay/OverlayWindow.h"

#include "overlay/CaptureSession.h"
#include "overlay/Toolbars.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPlainTextEdit>
#include <QScreen>
#include <QWindow>
#include <array>

namespace snapline {

class TextEditor : public QPlainTextEdit {
    Q_OBJECT

public:
    TextEditor(const QColor &color, int pixelSize, QWidget *parent) : QPlainTextEdit(parent)
    {
        setFrameShape(QFrame::NoFrame);
        setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setLineWrapMode(QPlainTextEdit::NoWrap);
        document()->setDocumentMargin(0);
        setContentsMargins(0, 0, 0, 0);
        viewport()->setAutoFillBackground(false);
        setStyleSheet(QStringLiteral("QPlainTextEdit { background: transparent; border: 1px dashed %1; color: %2; }")
                          .arg(QColor(255, 255, 255, 160).name(QColor::HexArgb), color.name()));
        QFont f = font();
        f.setPixelSize(pixelSize);
        setFont(f);
        connect(this, &QPlainTextEdit::textChanged, this, &TextEditor::fit);
        fit();
    }

signals:
    void commit();
    void cancel();

protected:
    void keyPressEvent(QKeyEvent *e) override
    {
        if (e->key() == Qt::Key_Escape) {
            emit cancel();
            return;
        }
        if ((e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) && !(e->modifiers() & Qt::ShiftModifier)) {
            emit commit();
            return;
        }
        QPlainTextEdit::keyPressEvent(e);
    }

private:
    void fit()
    {
        const QFontMetrics fm(font());
        int width = fm.horizontalAdvance(QStringLiteral("M")) * 4;
        const QStringList lines = toPlainText().split(QLatin1Char('\n'));
        for (const QString &line : lines)
            width = std::max(width, fm.horizontalAdvance(line) + fm.horizontalAdvance(QStringLiteral("M")) * 2);
        setFixedSize(width + 2, fm.lineSpacing() * int(lines.size()) + 4);
    }
};

namespace {

constexpr int kHandle = 8;
const QColor kDim(0, 0, 0, 115);
const QColor kAccent(80, 150, 255);

std::array<QRect, 8> handleRects(const QRect &sel)
{
    const int h = kHandle;
    const int cx = sel.center().x() - h / 2;
    const int cy = sel.center().y() - h / 2;
    const int l = sel.left() - h / 2, t = sel.top() - h / 2;
    const int r = sel.right() + 1 - h / 2, b = sel.bottom() + 1 - h / 2;
    // TL, T, TR, R, BR, B, BL, L
    return {QRect(l, t, h, h), QRect(cx, t, h, h), QRect(r, t, h, h), QRect(r, cy, h, h),
            QRect(r, b, h, h), QRect(cx, b, h, h), QRect(l, b, h, h), QRect(l, cy, h, h)};
}

Qt::CursorShape handleCursor(int handle)
{
    switch (handle) {
    case 0: case 4: return Qt::SizeFDiagCursor;
    case 2: case 6: return Qt::SizeBDiagCursor;
    case 1: case 5: return Qt::SizeVerCursor;
    default: return Qt::SizeHorCursor;
    }
}

} // namespace

OverlayWindow::OverlayWindow(CaptureSession *session, int screenIndex, QScreen *screen)
    : QWidget(nullptr, Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint),
      m_session(session), m_index(screenIndex), m_screen(screen)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setWindowTitle(tr("Snapline capture"));

    winId();
    windowHandle()->setScreen(screen);
    setGeometry(screen->geometry());

    m_tools = new ToolsBar(session, this);
    m_actions = new ActionsBar(this);
    m_tools->hide();
    m_actions->hide();
    connect(m_tools, &ToolsBar::action, this, &OverlayWindow::action);
    connect(m_actions, &ActionsBar::action, this, &OverlayWindow::action);

    connect(session, &CaptureSession::changed, this, [this]() {
        m_tools->sync();
        updateToolbars();
        update();
    });
}

OverlayWindow::~OverlayWindow() = default;

QRect OverlayWindow::screenGeometry() const
{
    return m_session->capture().screens.at(m_index).geometry;
}

QPoint OverlayWindow::toGlobal(const QPointF &local) const
{
    // Wayland does not expose window positions; the window covers its screen, so offset by that.
    return screenGeometry().topLeft() + local.toPoint();
}

int OverlayWindow::handleAt(const QPoint &global) const
{
    if (!m_session->hasSelection())
        return -1;
    const auto rects = handleRects(m_session->selection());
    for (int i = 0; i < int(rects.size()); ++i) {
        if (rects[i].adjusted(-4, -4, 4, 4).contains(global))
            return i;
    }
    return -1;
}

void OverlayWindow::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    const ScreenImage &shot = m_session->capture().screens.at(m_index);
    p.drawImage(rect(), shot.image);

    const QRect screen = screenGeometry();
    const QRect sel = m_session->selection();
    p.translate(-screen.topLeft());

    QRegion dim(screen);
    if (!sel.isEmpty())
        dim -= QRegion(sel);
    p.save();
    p.setClipRegion(dim);
    p.fillRect(screen, kDim);
    p.restore();

    if (sel.isEmpty()) {
        if (screen.contains(m_cursorGlobal)) {
            const QString hint = tr("Select area");
            const QFontMetrics fm = p.fontMetrics();
            QRect box(m_cursorGlobal + QPoint(16, 16), QSize(fm.horizontalAdvance(hint) + 12, fm.height() + 6));
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(0, 0, 0, 170));
            p.drawRoundedRect(box, 3, 3);
            p.setPen(Qt::white);
            p.drawText(box, Qt::AlignCenter, hint);
        }
        return;
    }

    p.save();
    p.setClipRect(sel);
    p.setRenderHint(QPainter::Antialiasing);
    m_session->annotations().paint(p);
    if (m_session->draft())
        m_session->draft()->paint(p);
    p.restore();

    p.setPen(QPen(kAccent, 1, Qt::DashLine));
    p.setBrush(Qt::NoBrush);
    p.drawRect(sel.adjusted(0, 0, -1, -1));
    p.setPen(QPen(Qt::white, 1));
    p.setBrush(kAccent);
    for (const QRect &h : handleRects(sel))
        p.drawRect(h);

    const QSize px = m_session->resultPixelSize();
    const QString label = QStringLiteral("%1x%2").arg(px.width()).arg(px.height());
    const QFontMetrics fm = p.fontMetrics();
    QRect box(QPoint(sel.left(), sel.top() - fm.height() - 10), QSize(fm.horizontalAdvance(label) + 10, fm.height() + 4));
    if (box.top() < screen.top())
        box.moveTop(sel.top() + 4);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 170));
    p.drawRoundedRect(box, 3, 3);
    p.setPen(Qt::white);
    p.drawText(box, Qt::AlignCenter, label);
}

void OverlayWindow::mousePressEvent(QMouseEvent *event)
{
    const QPoint g = toGlobal(event->position());
    m_cursorGlobal = g;

    if (event->button() == Qt::RightButton) {
        if (m_session->hasSelection())
            m_session->setSelection({});
        else
            emit action(Action::Close);
        return;
    }
    if (event->button() != Qt::LeftButton)
        return;

    if (m_text)
        commitText();

    m_pressGlobal = g;
    m_startSelection = m_session->selection();
    const QRect sel = m_session->selection();
    const int handle = handleAt(g);

    if (!sel.isEmpty() && handle >= 0 && m_session->tool() == Tool::None) {
        m_drag = Drag::Resize;
        m_handle = handle;
    } else if (!sel.isEmpty() && sel.contains(g)) {
        switch (m_session->tool()) {
        case Tool::None:
            m_drag = Drag::Move;
            break;
        case Tool::Text:
            beginText(g);
            break;
        default:
            m_drag = Drag::Draw;
            m_session->beginDraft(g);
        }
    } else {
        m_drag = Drag::NewSelection;
        m_session->setSelection(QRect(g, QSize(1, 1)));
    }
    updateToolbars();
}

void OverlayWindow::mouseMoveEvent(QMouseEvent *event)
{
    const QPoint g = toGlobal(event->position());
    m_cursorGlobal = g;
    const QPoint delta = g - m_pressGlobal;

    switch (m_drag) {
    case Drag::NewSelection:
        m_session->setSelection(QRect(m_pressGlobal, g).normalized());
        break;
    case Drag::Move: {
        QRect moved = m_startSelection.translated(delta);
        const QRect v = m_session->virtualGeometry();
        moved.moveLeft(qBound(v.left(), moved.left(), v.right() - moved.width() + 1));
        moved.moveTop(qBound(v.top(), moved.top(), v.bottom() - moved.height() + 1));
        m_session->setSelection(moved);
        break;
    }
    case Drag::Resize: {
        QRect r = m_startSelection;
        if (m_handle == 0 || m_handle == 6 || m_handle == 7) r.setLeft(r.left() + delta.x());
        if (m_handle == 2 || m_handle == 3 || m_handle == 4) r.setRight(r.right() + delta.x());
        if (m_handle == 0 || m_handle == 1 || m_handle == 2) r.setTop(r.top() + delta.y());
        if (m_handle == 4 || m_handle == 5 || m_handle == 6) r.setBottom(r.bottom() + delta.y());
        m_session->setSelection(r.normalized());
        break;
    }
    case Drag::Draw:
        m_session->updateDraft(g, event->modifiers() & Qt::ShiftModifier);
        break;
    case Drag::None:
        updateCursor(g);
        if (!m_session->hasSelection())
            update();
        break;
    }
}

void OverlayWindow::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
        return;
    const Drag drag = m_drag;
    m_drag = Drag::None;
    if (drag == Drag::Draw) {
        m_session->commitDraft();
    } else if (drag == Drag::NewSelection) {
        const QRect sel = m_session->selection();
        if (sel.width() < 3 || sel.height() < 3)
            m_session->setSelection({});
    }
    updateToolbars();
    updateCursor(toGlobal(event->position()));
}

void OverlayWindow::keyPressEvent(QKeyEvent *event)
{
    const auto mods = event->modifiers();
    const bool ctrl = mods & Qt::ControlModifier;
    const bool shift = mods & Qt::ShiftModifier;
    const int step = shift ? 10 : 1;

    if (event->key() == Qt::Key_Escape) {
        emit action(Action::Close);
    } else if (ctrl && event->key() == Qt::Key_C) {
        emit action(Action::Copy);
    } else if (ctrl && event->key() == Qt::Key_S) {
        emit action(Action::Save);
    } else if (ctrl && event->key() == Qt::Key_D) {
        emit action(Action::Upload);
    } else if (ctrl && event->key() == Qt::Key_P) {
        emit action(Action::Print);
    } else if (ctrl && event->key() == Qt::Key_A) {
        emit action(Action::SelectFullScreen);
    } else if ((ctrl && shift && event->key() == Qt::Key_Z) || (ctrl && event->key() == Qt::Key_Y)) {
        emit action(Action::Redo);
    } else if (ctrl && event->key() == Qt::Key_Z) {
        emit action(Action::Undo);
    } else if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        emit action(Action::Copy);
    } else if (event->key() == Qt::Key_Left) {
        nudgeSelection(-step, 0);
    } else if (event->key() == Qt::Key_Right) {
        nudgeSelection(step, 0);
    } else if (event->key() == Qt::Key_Up) {
        nudgeSelection(0, -step);
    } else if (event->key() == Qt::Key_Down) {
        nudgeSelection(0, step);
    } else {
        QWidget::keyPressEvent(event);
    }
}

void OverlayWindow::wheelEvent(QWheelEvent *event)
{
    const int steps = event->angleDelta().y() / 120;
    if (steps != 0)
        m_session->setStrokeWidth(m_session->strokeWidth() + steps);
}

void OverlayWindow::enterEvent(QEnterEvent *event)
{
    // Keyboard shortcuts should follow the monitor the pointer is on.
    activateWindow();
    setFocus();
    QWidget::enterEvent(event);
}

void OverlayWindow::nudgeSelection(int dx, int dy)
{
    if (!m_session->hasSelection())
        return;
    const QRect sel = m_session->selection().translated(dx, dy);
    if (m_session->virtualGeometry().contains(sel))
        m_session->setSelection(sel);
}

void OverlayWindow::updateCursor(const QPoint &global)
{
    const QRect sel = m_session->selection();
    if (sel.isEmpty() || !sel.contains(global)) {
        const int handle = m_session->tool() == Tool::None ? handleAt(global) : -1;
        setCursor(handle >= 0 ? handleCursor(handle) : Qt::CrossCursor);
        return;
    }
    switch (m_session->tool()) {
    case Tool::None: {
        const int handle = handleAt(global);
        setCursor(handle >= 0 ? handleCursor(handle) : Qt::SizeAllCursor);
        break;
    }
    case Tool::Text:
        setCursor(Qt::IBeamCursor);
        break;
    default:
        setCursor(Qt::CrossCursor);
    }
}

void OverlayWindow::updateToolbars()
{
    const QRect sel = m_session->selection();
    const QRect screen = screenGeometry();
    // Toolbars live on the monitor holding the selection's bottom-right corner.
    const bool visible = !sel.isEmpty() && m_drag != Drag::NewSelection && screen.contains(sel.bottomRight());
    if (!visible) {
        m_tools->hide();
        m_actions->hide();
        return;
    }

    const QRect local = sel.translated(-screen.topLeft());
    const QRect area = rect();
    const int gap = 6;

    m_actions->adjustSize();
    QSize a = m_actions->size();
    int ax = local.right() - a.width() + 1;
    int ay = local.bottom() + gap;
    if (ay + a.height() > area.bottom())
        ay = local.bottom() - a.height() - gap;
    ax = qBound(area.left(), ax, area.right() - a.width());
    m_actions->move(ax, ay);

    m_tools->adjustSize();
    QSize t = m_tools->size();
    int tx = local.right() + gap;
    if (tx + t.width() > area.right())
        tx = local.left() - t.width() - gap;
    if (tx < area.left())
        tx = local.right() - t.width() - gap;
    int ty = qBound(area.top(), local.bottom() - t.height() + 1, area.bottom() - t.height());
    m_tools->move(tx, ty);

    m_actions->show();
    m_tools->show();
    m_actions->raise();
    m_tools->raise();
}

void OverlayWindow::beginText(const QPoint &global)
{
    m_textGlobal = global;
    m_text = new TextEditor(m_session->color(), textPixelSize(m_session->strokeWidth()), this);
    m_text->move(global - screenGeometry().topLeft());
    connect(m_text, &TextEditor::commit, this, &OverlayWindow::commitText);
    connect(m_text, &TextEditor::cancel, this, [this]() {
        m_text->deleteLater();
        m_text = nullptr;
        setFocus();
    });
    m_text->show();
    m_text->setFocus();
    emit textEditingStarted(this);
}

void OverlayWindow::commitText()
{
    if (!m_text)
        return;
    const QString text = m_text->toPlainText();
    const QColor color = m_session->color();
    const int size = m_text->font().pixelSize();
    m_text->deleteLater();
    m_text = nullptr;
    m_session->addAnnotation(makeTextAnnotation(m_textGlobal + QPoint(1, 2), text, color, size));
    setFocus();
}

} // namespace snapline

#include "OverlayWindow.moc"

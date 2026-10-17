#include "overlay/Toolbars.h"

#include "overlay/CaptureSession.h"

#include <QBoxLayout>
#include <QColorDialog>
#include <QIcon>
#include <QMenu>
#include <QPainter>
#include <QPixmap>
#include <QToolButton>

namespace snapline {

namespace {

const char *kStyle = R"(
QFrame#snaplineBar { background: #f6f6f6; border: 1px solid #b9b9b9; border-radius: 4px; }
QToolButton { border: none; padding: 3px; border-radius: 3px; }
QToolButton:hover { background: #dedede; }
QToolButton:checked { background: #c9dcff; }
QToolButton::menu-indicator { image: none; }
)";

QToolButton *makeButton(QWidget *parent, const QString &icon, const QString &tip)
{
    auto *button = new QToolButton(parent);
    button->setIcon(QIcon(QStringLiteral(":/icons/%1.svg").arg(icon)));
    button->setIconSize(QSize(20, 20));
    button->setToolTip(tip);
    button->setFocusPolicy(Qt::NoFocus);
    button->setCursor(Qt::ArrowCursor);
    return button;
}

QIcon swatch(const QColor &color)
{
    QPixmap pixmap(40, 40);
    pixmap.fill(Qt::transparent);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(QColor(60, 60, 60), 2));
    p.setBrush(color);
    p.drawRoundedRect(QRectF(5, 5, 30, 30), 6, 6);
    return QIcon(pixmap);
}

void setupFrame(QFrame *frame)
{
    frame->setObjectName(QStringLiteral("snaplineBar"));
    frame->setStyleSheet(QString::fromLatin1(kStyle));
    frame->setAutoFillBackground(true);
    frame->setCursor(Qt::ArrowCursor);
}

} // namespace

ToolsBar::ToolsBar(CaptureSession *session, QWidget *parent) : QFrame(parent), m_session(session)
{
    setupFrame(this);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(3, 3, 3, 3);
    layout->setSpacing(1);

    const struct {
        Tool tool;
        const char *icon;
        QString tip;
    } tools[] = {
        {Tool::Pen, "pen", tr("Pen")},
        {Tool::Line, "line", tr("Line (Shift: snap to 45°)")},
        {Tool::Arrow, "arrow", tr("Arrow (Shift: snap to 45°)")},
        {Tool::Rect, "rect", tr("Rectangle (Shift: square)")},
        {Tool::Circle, "circle", tr("Circle (Shift: round)")},
        {Tool::Marker, "marker", tr("Marker")},
        {Tool::Text, "text", tr("Text")},
        {Tool::Blur, "blur", tr("Blur")},
    };
    for (const auto &t : tools) {
        QToolButton *button = makeButton(this, QString::fromLatin1(t.icon), t.tip + tr(" — mouse wheel changes size"));
        button->setCheckable(true);
        const Tool tool = t.tool;
        connect(button, &QToolButton::clicked, this, [this, tool](bool checked) {
            m_session->setTool(checked ? tool : Tool::None);
        });
        layout->addWidget(button);
        m_tools.emplace_back(tool, button);
    }

    m_color = makeButton(this, QString(), tr("Color"));
    connect(m_color, &QToolButton::clicked, this, &ToolsBar::pickColor);
    layout->addWidget(m_color);

    m_undo = makeButton(this, QStringLiteral("undo"), tr("Undo (Ctrl+Z)"));
    connect(m_undo, &QToolButton::clicked, this, [this]() { emit action(Action::Undo); });
    layout->addWidget(m_undo);

    m_redo = makeButton(this, QStringLiteral("redo"), tr("Redo (Ctrl+Shift+Z)"));
    connect(m_redo, &QToolButton::clicked, this, [this]() { emit action(Action::Redo); });
    layout->addWidget(m_redo);

    sync();
}

void ToolsBar::sync()
{
    for (auto &[tool, button] : m_tools)
        button->setChecked(m_session->tool() == tool);
    m_color->setIcon(swatch(m_session->color()));
    m_undo->setEnabled(m_session->canUndo());
    m_redo->setEnabled(m_session->canRedo());
}

void ToolsBar::pickColor()
{
    static const QColor presets[] = {QColor(255, 0, 0), QColor(255, 140, 0), QColor(255, 221, 0),
                                     QColor(0, 200, 0),  QColor(0, 160, 255), QColor(40, 60, 255),
                                     QColor(160, 0, 255), QColor(255, 0, 170), Qt::black, Qt::white};
    QMenu menu(this);
    for (const QColor &color : presets) {
        QAction *a = menu.addAction(swatch(color), color.name());
        connect(a, &QAction::triggered, this, [this, color]() { m_session->setColor(color); });
    }
    menu.addSeparator();
    menu.addAction(tr("Custom…"), this, [this]() {
        const QColor color = QColorDialog::getColor(m_session->color(), window(), tr("Color"));
        if (color.isValid())
            m_session->setColor(color);
    });
    menu.exec(m_color->mapToGlobal(QPoint(m_color->width(), 0)));
}

ActionsBar::ActionsBar(QWidget *parent) : QFrame(parent)
{
    setupFrame(this);
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(3, 3, 3, 3);
    layout->setSpacing(1);

    auto add = [this, layout](const char *icon, const QString &tip, Action a) {
        QToolButton *button = makeButton(this, QString::fromLatin1(icon), tip);
        connect(button, &QToolButton::clicked, this, [this, a]() { emit action(a); });
        layout->addWidget(button);
        return button;
    };

    add("upload", tr("Upload (Ctrl+D)"), Action::Upload);

    QToolButton *share = makeButton(this, QStringLiteral("share"), tr("Share"));
    share->setPopupMode(QToolButton::InstantPopup);
    auto *menu = new QMenu(share);
    const struct {
        QString label;
        Action action;
    } shares[] = {
        {tr("Search similar images on Google"), Action::ShareGoogle},
        {tr("Search similar images on TinEye"), Action::ShareTinEye},
        {tr("Send via email"), Action::ShareEmail},
        {tr("Share on X (Twitter)"), Action::ShareTwitter},
        {tr("Share on Facebook"), Action::ShareFacebook},
        {tr("Share on VK"), Action::ShareVk},
        {tr("Share on Pinterest"), Action::SharePinterest},
    };
    for (const auto &s : shares) {
        const Action a = s.action;
        menu->addAction(s.label, this, [this, a]() { emit action(a); });
    }
    share->setMenu(menu);
    layout->addWidget(share);

    add("print", tr("Print (Ctrl+P)"), Action::Print);
    add("copy", tr("Copy (Ctrl+C)"), Action::Copy);
    add("save", tr("Save (Ctrl+S)"), Action::Save);
    add("close", tr("Close (Esc)"), Action::Close);
}

} // namespace snapline

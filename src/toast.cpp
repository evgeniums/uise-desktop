/**
@copyright Evgeny Sidorov 2025

This software is dual-licensed. Choose the appropriate license for your project.

1. The GNU GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-GPLv3.md](LICENSE-GPLv3.md) or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. The GNU LESSER GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-LGPLv3.md](LICENSE-LGPLv3.md) or copy at https://www.gnu.org/licenses/lgpl-3.0.txt).

You may select, at your option, one of the above-listed licenses.

*/

/****************************************************************************/

/** @file uise/desktop/src/toast.cpp
*
*  Defines Toast.
*
*/

/****************************************************************************/

#include <QPainter>
#include <QScreen>
#include <QApplication>
#include <QGuiApplication>
#include <QDebug>
#include <QGraphicsOpacityEffect>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/utils/destroywidget.hpp>
#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/label.hpp>
#include <uise/desktop/roundedimage.hpp>
#include <uise/desktop/toast.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

//--------------------------------------------------------------------------

Toast::Toast(QWidget *parent)
    : Toast({},DefaultDuration,parent)
{
}

//--------------------------------------------------------------------------

Toast::Toast(const QString &message, QWidget *parent)
    : Toast(message,DefaultDuration,parent)
{
}

//--------------------------------------------------------------------------

Toast::Toast(const QString &message, int duration, QWidget *parent)
    : QWidget(parent, Qt::Tool | Qt::FramelessWindowHint),
      m_layout(nullptr),
      m_iconFrame(nullptr),
      m_icon(nullptr),
      m_label(nullptr),
      m_timer(nullptr),
      m_animation(nullptr),
      m_duration(duration),
      m_verticalPosition(VerticalPosition::Center),
      m_horizontalPosition(HorizontalPosition::Center),
      m_verticalOffset(DefaultMargin),
      m_horizontalOffset(DefaultMargin),
      m_deleteOnClose(false),
      m_autoSize(false),
      m_maxWidth(DefaultMaxWidth),
      m_iconSize(DefaultIconSize,DefaultIconSize),
      m_drawInParent(false),
      m_opacityEffect(new QGraphicsOpacityEffect(this)),
      m_currentOpacity(0.0)
{
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAttribute(Qt::WA_TranslucentBackground);

    m_iconFrame=new WithRoundedImage(this);
    m_iconFrame->setObjectName("icon");
    m_icon=m_iconFrame->image();
    m_icon->setDisableHover(true);
    // C++ default so the icon is visible with no style sheet at all; a QSS min/max-width
    // rule on "uise--Toast #icon uise--RoundedImage" overrides this at polish time
    m_icon->setImageSize(m_iconSize);
    m_iconFrame->setVisible(false);

    m_label = new Label(message,this);
    m_label->setObjectName("text");
    m_label->setAlignment(Qt::AlignCenter);
    m_label->setWordWrap(true);

    m_layout = Layout::horizontal(this);
    m_layout->addWidget(m_iconFrame,0,Qt::AlignVCenter);
    m_layout->addWidget(m_label,1);

    m_timer = new QTimer(this);
    m_timer->setSingleShot(true);
    connect(m_timer, &QTimer::timeout, this, &Toast::fadeOut);

    m_animation = new QPropertyAnimation(this, "windowOpacity");
    connect(m_animation, &QPropertyAnimation::finished, this, &Toast::finished);

    connect(m_animation, &QPropertyAnimation::valueChanged, this,
        [this](const QVariant& value)
        {
            m_currentOpacity=value.toReal();
            m_opacityEffect->setOpacity(m_currentOpacity);
            if (m_drawInParent && parentWidget()!=nullptr)
            {
                repaint();
            }
        }
    );
}

//--------------------------------------------------------------------------

void Toast::setPosition(Position position)
{
    switch (position)
    {
        case TopLeft:
            setVerticalPosition(VerticalPosition::Top,DefaultMargin);
            setHorizontalPosition(HorizontalPosition::Left,DefaultMargin);
            break;
        case TopCenter:
            setVerticalPosition(VerticalPosition::Top,DefaultMargin);
            setHorizontalPosition(HorizontalPosition::Center);
            break;
        case TopRight:
            setVerticalPosition(VerticalPosition::Top,DefaultMargin);
            setHorizontalPosition(HorizontalPosition::Right,DefaultMargin);
            break;
        case BottomLeft:
            setVerticalPosition(VerticalPosition::Bottom,DefaultMargin);
            setHorizontalPosition(HorizontalPosition::Left,DefaultMargin);
            break;
        case BottomCenter:
            setVerticalPosition(VerticalPosition::Bottom,DefaultMargin);
            setHorizontalPosition(HorizontalPosition::Center);
            break;
        case BottomRight:
            setVerticalPosition(VerticalPosition::Bottom,DefaultMargin);
            setHorizontalPosition(HorizontalPosition::Right,DefaultMargin);
            break;
        case Center:
            setVerticalPosition(VerticalPosition::Center);
            setHorizontalPosition(HorizontalPosition::Center);
            break;
    }
}

//--------------------------------------------------------------------------

void Toast::setVerticalPositionName(const QString& name)
{
    if (name==QLatin1String("top"))
    {
        m_verticalPosition=VerticalPosition::Top;
    }
    else if (name==QLatin1String("bottom"))
    {
        m_verticalPosition=VerticalPosition::Bottom;
    }
    else
    {
        m_verticalPosition=VerticalPosition::Center;
    }
}

//--------------------------------------------------------------------------

QString Toast::verticalPositionName() const
{
    switch (m_verticalPosition)
    {
        case VerticalPosition::Top:
            return QStringLiteral("top");
        case VerticalPosition::Bottom:
            return QStringLiteral("bottom");
        case VerticalPosition::Center:
            break;
    }
    return QStringLiteral("vcenter");
}

//--------------------------------------------------------------------------

void Toast::setHorizontalPositionName(const QString& name)
{
    if (name==QLatin1String("left"))
    {
        m_horizontalPosition=HorizontalPosition::Left;
    }
    else if (name==QLatin1String("right"))
    {
        m_horizontalPosition=HorizontalPosition::Right;
    }
    else
    {
        m_horizontalPosition=HorizontalPosition::Center;
    }
}

//--------------------------------------------------------------------------

QString Toast::horizontalPositionName() const
{
    switch (m_horizontalPosition)
    {
        case HorizontalPosition::Left:
            return QStringLiteral("left");
        case HorizontalPosition::Right:
            return QStringLiteral("right");
        case HorizontalPosition::Center:
            break;
    }
    return QStringLiteral("hcenter");
}

//--------------------------------------------------------------------------

void Toast::setSvgIcon(std::shared_ptr<SvgIcon> icon)
{
    const bool hasIcon=static_cast<bool>(icon);   // latch before the moved-from reset
    m_icon->setSvgIcon(std::move(icon));
    m_iconFrame->setVisible(hasIcon);
    // without an icon, keep the historical look: text centered in the whole toast
    m_label->setAlignment(hasIcon ? (Qt::AlignLeft|Qt::AlignVCenter) : Qt::AlignCenter);
    m_layout->invalidate();
    updateGeometry();
}

//--------------------------------------------------------------------------

std::shared_ptr<SvgIcon> Toast::svgIcon() const
{
    return m_icon->svgIcon();
}

//--------------------------------------------------------------------------

void Toast::setIconSize(const QSize& size)
{
    if (!size.isValid() || size.isNull())
    {
        return;
    }
    m_iconSize=size;
    m_icon->setImageSize(size);
    m_layout->invalidate();
    updateGeometry();
}

//--------------------------------------------------------------------------

void Toast::setMessage(const QString& message)
{
    m_label->setText(message);
}

//--------------------------------------------------------------------------

QString Toast::message() const
{
    return m_label->text();
}

//--------------------------------------------------------------------------

QSize Toast::autoSizeHint(const QRect& boundingRect)
{
    // QSS icon min/max-width and the #text font are installed in polish()
    ensurePolished();

    // drop the constraints installed by the previous show() and the cached layout geometry,
    // otherwise we measure the previous message
    setMinimumSize(0,0);
    setMaximumSize(QWIDGETSIZE_MAX,QWIDGETSIZE_MAX);
    m_layout->invalidate();
    m_label->updateGeometry();

    const auto margins=m_layout->contentsMargins();
    const bool withIcon=m_iconFrame->isVisibleTo(this);      // NOT isVisible(): toast is hidden
    const QSize iconHint=withIcon ? m_iconFrame->sizeHint() : QSize{0,0};
    const int spacing=withIcon ? qMax(m_layout->spacing(),0) : 0;
    const int extraW=margins.left()+margins.right()+iconHint.width()+spacing;
    const int extraH=margins.top()+margins.bottom();

    // natural text width: QLabel::sizeHint() with word wrap on returns a
    // QTextDocument::adjustSize() heuristic block rather than the natural width,
    // so measure with wrap temporarily off
    const bool wrap=m_label->wordWrap();
    m_label->setWordWrap(false);
    const int naturalTextW=m_label->sizeHint().width();
    m_label->setWordWrap(wrap);

    int maxW=(m_maxWidth>0) ? m_maxWidth : QWIDGETSIZE_MAX;
    maxW=qMax(qMin(maxW,qRound(boundingRect.width()*0.9)),1);
    const int minW=qMin(DefaultMinAutoWidth,maxW);
    const int w=qBound(minW,naturalTextW+extraW,maxW);

    // QLabel::setWordWrap(true) does not set the heightForWidth size-policy bit, so
    // layout()->heightForWidth() would return -1; ask the label directly
    const int textW=qMax(w-extraW,1);
    int textH=m_label->heightForWidth(textW);
    if (textH<0)
    {
        textH=m_label->sizeHint().height();
    }
    int h=qMax(textH,iconHint.height())+extraH;
    h=qMax(qMin(h,qRound(boundingRect.height()*0.9)),1);

    return QSize{w,h};
}

//--------------------------------------------------------------------------

void Toast::show()
{
    auto parent=parentWidget();
    const bool inParent=m_drawInParent && parent!=nullptr;
    const QRect parentRect = inParent ? parent->rect()
                                      : QGuiApplication::primaryScreen()->availableGeometry();

    int w=DefaultWidth;
    int h=DefaultHeight;

    if (m_autoSize)
    {
        const auto sz=autoSizeHint(parentRect);
        w=sz.width();
        h=sz.height();
    }
    else
    {
        ensurePolished();
        if (inParent)
        {
            if (w>parentRect.width())
            {
                w=parentRect.width()*0.9;
            }
            if (h>parentRect.height())
            {
                h=parentRect.height()*0.9;
            }
        }
    }

    setFixedSize(w,h);

    int x=0;
    int y=0;

    switch (m_horizontalPosition)
    {
        case HorizontalPosition::Left:
            x=parentRect.left()+m_horizontalOffset;
            break;
        case HorizontalPosition::Right:
            x=parentRect.right()-width()+1-m_horizontalOffset;
            break;
        case HorizontalPosition::Center:
            x=parentRect.left()+(parentRect.width()-width())/2;
            break;
    }

    switch (m_verticalPosition)
    {
        case VerticalPosition::Top:
            y=parentRect.top()+m_verticalOffset;
            break;
        case VerticalPosition::Bottom:
            y=parentRect.bottom()-height()+1-m_verticalOffset;
            break;
        case VerticalPosition::Center:
            y=parentRect.top()+(parentRect.height()-height())/2;
            break;
    }

    move(x,y);

    m_animation->setDuration(300);
    m_animation->setStartValue(0.0);
    m_animation->setEndValue(0.9);
    m_animation->start();

    setVisible(true);

    m_timer->start(m_duration);
}

//--------------------------------------------------------------------------

void Toast::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    auto color=palette().color(QPalette::Window);
    painter.setBrush(color);
    painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(rect(), 10, 10);
}

//--------------------------------------------------------------------------

void Toast::fadeOut()
{
    m_animation->setDuration(500);
    m_animation->setStartValue(m_currentOpacity);
    m_animation->setEndValue(0.0);
    m_animation->start();
}

//--------------------------------------------------------------------------

void Toast::finished()
{
    if (m_animation->currentValue()!=0.0)
    {
        return;
    }

    if (m_deleteOnClose)
    {
        destroyWidget(this);
    }
    else
    {
        hide();
    }
}

//--------------------------------------------------------------------------

void Toast::setDrawInParent(bool enable)
{
    m_drawInParent=enable;
    if (m_drawInParent && parentWidget()!=nullptr)
    {
        setWindowFlag(Qt::Tool,false);
        setWindowFlag(Qt::FramelessWindowHint,false);
        setGraphicsEffect(m_opacityEffect);
        setVisible(false);
    }
    else
    {
        setGraphicsEffect(nullptr);
        setWindowFlag(Qt::Tool,true);
        setWindowFlag(Qt::FramelessWindowHint,true);
    }
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END

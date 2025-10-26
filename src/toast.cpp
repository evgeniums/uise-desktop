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
      m_duration(duration),
      m_position(Center),
      m_deleteOnClose(false),
      m_drawInParent(false),
      m_opacityEffect(new QGraphicsOpacityEffect(this)),
      m_currentOpacity(0.0)
{
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAttribute(Qt::WA_TranslucentBackground);

    m_label = new QLabel(message);
    m_label->setAlignment(Qt::AlignCenter);
    m_label->setWordWrap(true);

    auto layout = Layout::vertical(this);
    layout->addWidget(m_label,0,Qt::AlignCenter);

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
    m_position = position;
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

void Toast::show()
{
    auto w=DefaultWidth;
    auto h=DefaultHeight;
    QRect parentRect;
    auto parent=parentWidget();
    if (m_drawInParent && parent!=nullptr)
    {
        parentRect = parent->rect();
        if (w>parent->width())
        {
            w=parent->width()*0.9;
        }
        if (h>parent->height())
        {
            h=parent->height()*0.9;
        }
    }
    else
    {
        parentRect = QGuiApplication::primaryScreen()->availableGeometry();
    }

    int x = 0;
    int y = 0;
    const int margin = DefaultMargin;

    setFixedSize(w,h);

    switch (m_position)
    {
        case TopLeft:
            x = margin;
            y = margin;
            break;
        case TopCenter:
            x = (parentRect.width() - width()) / 2;
            y = margin;
            break;
        case TopRight:
            x = parentRect.width() - width() - margin;
            y = margin;
            break;
        case BottomLeft:
            x = margin;
            y = parentRect.height() - height() - margin;
            break;
        case BottomCenter:
            x = (parentRect.width() - width()) / 2;
            y = parentRect.height() - height() - margin;
            break;
        case BottomRight:
            x = parentRect.width() - width() - margin;
            y = parentRect.height() - height() - margin;
            break;
        case Center:
            x = parentRect.center().x() - w/2;
            y = parentRect.center().y() - h/2;
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

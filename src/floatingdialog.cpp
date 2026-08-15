/**
@copyright Evgeny Sidorov 2026

This software is dual-licensed. Choose the appropriate license for your project.

1. The GNU GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-GPLv3.md](LICENSE-GPLv3.md) or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. The GNU LESSER GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-LGPLv3.md](LICENSE-LGPLv3.md) or copy at https://www.gnu.org/licenses/lgpl-3.0.txt).

You may select, at your option, one of the above-listed licenses.

*/

/****************************************************************************/

/** @file uise/desktop/src/floatingdialog.cpp
*
*  Defines FloatingDialogFrame.
*
*/

/****************************************************************************/

#include <QEvent>
#include <QCloseEvent>
#include <QMouseEvent>
#include <QShortcut>
#include <QGuiApplication>
#include <QScreen>
#include <QCursor>
#include <QPainter>
#include <QStyleOption>
#include <QStyle>
#include <QPropertyAnimation>
#include <QLayout>

#include <uise/desktop/utils/destroywidget.hpp>
#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/abstractdialog.hpp>
#include <uise/desktop/floatingdialog.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

//--------------------------------------------------------------------------

class FloatingDialogFrame_p
{
    public:

        QBoxLayout* layout=nullptr;

        QPointer<QWidget> content;
        bool contentAutoDestroy=true;

        QPointer<QWidget> dragHandle;
        bool explicitDragHandle=false;
        bool dragging=false;
        QPoint dragOffset;

        QPointer<QObject> pseudoParent;

        QPointer<QWidget> hostWindow;
        bool followHostVisibility=true;
        bool hiddenByHost=false;

        int minVisibleMargin=FloatingDialogFrame::DefaultMinVisibleMargin;

        bool shortcutEnabled=true;
        QShortcut* escShortcut=nullptr;

        bool hasPosition=false;

        QPropertyAnimation* fadeAnimation=nullptr;
        int fadeDurationMs=FloatingDialogFrame::DefaultFadeDurationMs;
        int easingCurveType=static_cast<int>(QEasingCurve::OutCubic);
        bool closing=false;             // a close is in flight (fade running) or mid-finishClose()
        bool pendingAutoDestroy=false;
};

//--------------------------------------------------------------------------

//! Drag handle to use when the caller has not set an explicit one: the content's own dialog
//! title bar, if it has one, otherwise the content widget itself.
static QWidget* resolveDragHandle(QWidget* content)
{
    if (content==nullptr)
    {
        return nullptr;
    }

    auto* dialog=qobject_cast<AbstractDialog*>(content);
    if (dialog!=nullptr && dialog->titleBar()!=nullptr)
    {
        return dialog->titleBar();
    }
    return content;
}

//--------------------------------------------------------------------------

//! (Re)install the event filter that watches the current host window; called before every
//! popup() / popupAt() so a parent set or changed after construction is always picked up.
static void updateHostWindowTracking(FloatingDialogFrame* self, FloatingDialogFrame_p* p)
{
    auto* hostWindow=self->parentWidget()!=nullptr ? self->parentWidget()->window() : nullptr;
    if (p->hostWindow.data()==hostWindow)
    {
        return;
    }

    if (!p->hostWindow.isNull())
    {
        p->hostWindow->removeEventFilter(self);
    }
    p->hostWindow=hostWindow;
    if (hostWindow!=nullptr)
    {
        hostWindow->installEventFilter(self);
    }
}

//--------------------------------------------------------------------------

static void showFrame(FloatingDialogFrame* self, FloatingDialogFrame_p* p)
{
    updateHostWindowTracking(self,p);

    p->hiddenByHost=false;
    p->escShortcut->setEnabled(p->shortcutEnabled);

    // Cancel any in-flight close fade so reopening never shows a half-transparent window, and
    // reset closing before show() -- a fade-out's finished() firing after this point (its
    // animation was just stopped, so it won't, but belt-and-braces) must not tear content down
    // out from under the frame that is being shown again.
    p->fadeAnimation->stop();
    p->closing=false;
    self->setWindowOpacity(p->fadeDurationMs>0 ? 0.0 : 1.0);

    self->show();
    self->raise();
    self->activateWindow();

    if (p->fadeDurationMs>0)
    {
        p->fadeAnimation->setDuration(p->fadeDurationMs);
        p->fadeAnimation->setEasingCurve(static_cast<QEasingCurve::Type>(p->easingCurveType));
        p->fadeAnimation->setStartValue(0.0);
        p->fadeAnimation->setEndValue(1.0);
        p->fadeAnimation->start();
    }

    if (!p->content.isNull())
    {
        auto* dialog=qobject_cast<AbstractDialog*>(p->content.data());
        if (dialog!=nullptr)
        {
            dialog->setDialogFocus();
        }
        else
        {
            p->content->setFocus();
        }
    }
}

//--------------------------------------------------------------------------

FloatingDialogFrame::FloatingDialogFrame(QWidget* parent)
    : QFrame(parent),
      pimpl(std::make_unique<FloatingDialogFrame_p>())
{
    // a real top-level window, not embedded into the host: it can therefore be dragged past
    // the host window's edges and never blocks interaction with the host, unlike ModalPopup.
    // Qt::Dialog (rather than the Qt::Tool used by DropdownFrame) keeps the frame visible when
    // the application loses activation on macOS, which is what a dialog meant to stay open
    // across window switches needs; FramelessWindowHint drops the native title bar so the
    // dialog's own AbstractDialog title bar is the only one, and WA_TranslucentBackground lets
    // its QSS-drawn rounded corners show through instead of an opaque square.
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setVisible(false);

    pimpl->layout=Layout::vertical(this);

    pimpl->escShortcut=new QShortcut(Qt::Key_Escape,this);
    pimpl->escShortcut->setContext(Qt::WindowShortcut);
    connect(
        pimpl->escShortcut,
        &QShortcut::activated,
        this,
        [this]()
        {
            close(pimpl->contentAutoDestroy);
        }
    );

    // Drives both the fade-out (close()) and the fade-in (showFrame()) -- finished() only acts
    // on the fade-out, guarded by pimpl->closing, since the fade-in has nothing left to do.
    pimpl->fadeAnimation=new QPropertyAnimation(this,"windowOpacity",this);
    connect(
        pimpl->fadeAnimation,
        &QPropertyAnimation::finished,
        this,
        [this]()
        {
            if (pimpl->closing)
            {
                finishClose();
            }
        }
    );
}

//--------------------------------------------------------------------------

FloatingDialogFrame::~FloatingDialogFrame()
{}

//--------------------------------------------------------------------------

void FloatingDialogFrame::setWidget(QWidget* widget, bool autoDestroy)
{
    if (!pimpl->content.isNull())
    {
        if (!pimpl->explicitDragHandle && !pimpl->dragHandle.isNull())
        {
            pimpl->dragHandle->removeEventFilter(this);
            pimpl->dragHandle=nullptr;
        }

        if (pimpl->contentAutoDestroy)
        {
            destroyWidget(pimpl->content);
        }
        else
        {
            pimpl->layout->removeWidget(pimpl->content);
            pimpl->content->setParent(nullptr);
        }
        pimpl->content=nullptr;
    }

    pimpl->content=widget;
    pimpl->contentAutoDestroy=autoDestroy;

    if (widget==nullptr)
    {
        return;
    }

    widget->setParent(this);
    pimpl->layout->addWidget(widget);

    // An AbstractDialog can opt out of mouse-drag resizing (AbstractDialog::isResizable()) --
    // SetFixedSize keeps this frame's own minimumSize()/maximumSize() continuously pinned to
    // the layout's sizeHint() as the content's natural size changes, which is also what
    // disables the OS-level resize affordance on a frameless top-level window; non-dialog
    // content (no titleBar()/isResizable() to ask) keeps the ordinary default constraint.
    auto* dialog=qobject_cast<AbstractDialog*>(widget);
    pimpl->layout->setSizeConstraint(
        (dialog!=nullptr && !dialog->isResizable()) ? QLayout::SetFixedSize
                                                      : QLayout::SetDefaultConstraint
    );

    if (!pimpl->explicitDragHandle)
    {
        pimpl->dragHandle=resolveDragHandle(widget);
        if (!pimpl->dragHandle.isNull())
        {
            pimpl->dragHandle->installEventFilter(this);
        }
    }
}

//--------------------------------------------------------------------------

QWidget* FloatingDialogFrame::widget() const
{
    return pimpl->content;
}

//--------------------------------------------------------------------------

QWidget* FloatingDialogFrame::takeWidget()
{
    auto* widget=pimpl->content.data();
    if (widget==nullptr)
    {
        return nullptr;
    }

    if (!pimpl->explicitDragHandle && !pimpl->dragHandle.isNull())
    {
        pimpl->dragHandle->removeEventFilter(this);
        pimpl->dragHandle=nullptr;
    }

    pimpl->layout->removeWidget(widget);
    widget->setParent(nullptr);
    pimpl->content=nullptr;

    return widget;
}

//--------------------------------------------------------------------------

void FloatingDialogFrame::setPseudoParent(QObject* obj)
{
    if (!pimpl->pseudoParent.isNull())
    {
        disconnect(pimpl->pseudoParent,nullptr,this,nullptr);
    }

    pimpl->pseudoParent=obj;

    if (obj!=nullptr)
    {
        // the pseudo parent is tracked purely for lifetime, not ownership: this frame is not
        // reparented to it, so QObject's own parent-child destruction cannot do this for us --
        // `this` as the connection's receiver/context is what makes Qt auto-disconnect if this
        // frame is destroyed first, so the lambda never fires on a dangling FloatingDialogFrame
        connect(
            obj,
            &QObject::destroyed,
            this,
            [this]()
            {
                hide();
                deleteLater();
            }
        );
    }
}

//--------------------------------------------------------------------------

QObject* FloatingDialogFrame::pseudoParent() const
{
    return pimpl->pseudoParent;
}

//--------------------------------------------------------------------------

void FloatingDialogFrame::setDragHandle(QWidget* handle)
{
    if (!pimpl->dragHandle.isNull())
    {
        pimpl->dragHandle->removeEventFilter(this);
    }

    if (handle!=nullptr)
    {
        pimpl->explicitDragHandle=true;
        pimpl->dragHandle=handle;
    }
    else
    {
        pimpl->explicitDragHandle=false;
        pimpl->dragHandle=resolveDragHandle(pimpl->content);
    }

    if (!pimpl->dragHandle.isNull())
    {
        pimpl->dragHandle->installEventFilter(this);
    }
}

//--------------------------------------------------------------------------

QWidget* FloatingDialogFrame::dragHandle() const
{
    return pimpl->dragHandle;
}

//--------------------------------------------------------------------------

void FloatingDialogFrame::setMinVisibleMargin(int val) noexcept
{
    pimpl->minVisibleMargin=val;
}

//--------------------------------------------------------------------------

int FloatingDialogFrame::minVisibleMargin() const noexcept
{
    return pimpl->minVisibleMargin;
}

//--------------------------------------------------------------------------

void FloatingDialogFrame::setShortcutEnabled(bool enable)
{
    pimpl->shortcutEnabled=enable;
    pimpl->escShortcut->setEnabled(enable);
}

//--------------------------------------------------------------------------

bool FloatingDialogFrame::isShortcutEnabled() const noexcept
{
    return pimpl->shortcutEnabled;
}

//--------------------------------------------------------------------------

void FloatingDialogFrame::setFollowHostVisibility(bool enable) noexcept
{
    pimpl->followHostVisibility=enable;
}

//--------------------------------------------------------------------------

bool FloatingDialogFrame::isFollowHostVisibility() const noexcept
{
    return pimpl->followHostVisibility;
}

//--------------------------------------------------------------------------

void FloatingDialogFrame::setFadeDurationMs(int val) noexcept
{
    pimpl->fadeDurationMs=val;
}

//--------------------------------------------------------------------------

int FloatingDialogFrame::fadeDurationMs() const noexcept
{
    return pimpl->fadeDurationMs;
}

//--------------------------------------------------------------------------

void FloatingDialogFrame::setEasingCurveType(int val) noexcept
{
    pimpl->easingCurveType=val;
}

//--------------------------------------------------------------------------

int FloatingDialogFrame::easingCurveType() const noexcept
{
    return pimpl->easingCurveType;
}

//--------------------------------------------------------------------------

void FloatingDialogFrame::popup()
{
    adjustSize();

    if (!pimpl->hasPosition)
    {
        QPoint pos;
        auto* hostWindow=parentWidget()!=nullptr ? parentWidget()->window() : nullptr;
        if (hostWindow!=nullptr)
        {
            auto hostRect=QRect(hostWindow->mapToGlobal(QPoint(0,0)),hostWindow->size());
            pos=hostRect.center()-QPoint(width()/2,height()/2);
        }
        else
        {
            auto* screen=QGuiApplication::screenAt(QCursor::pos());
            if (screen==nullptr)
            {
                screen=QGuiApplication::primaryScreen();
            }
            auto avail=screen!=nullptr ? screen->availableGeometry() : QRect(0,0,width(),height());
            pos=avail.center()-QPoint(width()/2,height()/2);
        }
        clampToScreen(pos);
        move(pos);
    }

    showFrame(this,pimpl.get());
}

//--------------------------------------------------------------------------

void FloatingDialogFrame::popupAt(const QPoint& globalPos)
{
    adjustSize();

    auto pos=globalPos;
    clampToScreen(pos);
    move(pos);
    pimpl->hasPosition=true;

    showFrame(this,pimpl.get());
}

//--------------------------------------------------------------------------

void FloatingDialogFrame::popupAt(const QPoint& globalPos, Qt::Corner anchorCorner)
{
    adjustSize();

    auto pos=globalPos;
    if (anchorCorner==Qt::TopRightCorner || anchorCorner==Qt::BottomRightCorner)
    {
        pos.setX(pos.x()-width());
    }
    if (anchorCorner==Qt::BottomLeftCorner || anchorCorner==Qt::BottomRightCorner)
    {
        pos.setY(pos.y()-height());
    }

    clampFullyToScreen(pos);
    move(pos);
    pimpl->hasPosition=true;

    showFrame(this,pimpl.get());
}

//--------------------------------------------------------------------------

void FloatingDialogFrame::close(bool autoDestroy)
{
    // With a fade the close becomes asynchronous, so closed() -- and therefore any
    // closeDialog()/closeRequested() round trip a host wires from it (see
    // FloatingDialog::openDialog()) -- now fires after this call has already returned. This
    // guard, rather than the blockSignals() window openDialog() still (harmlessly) uses, is
    // what makes a re-entrant close() during that round trip, a plain double close(), or a
    // closeEvent() arriving mid-fade all safe no-ops.
    if (pimpl->closing || !isVisible())
    {
        return;
    }

    pimpl->escShortcut->setEnabled(false);
    pimpl->closing=true;
    pimpl->pendingAutoDestroy=autoDestroy;

    if (pimpl->fadeDurationMs<=0)
    {
        finishClose();
        return;
    }

    pimpl->fadeAnimation->stop();
    pimpl->fadeAnimation->setDuration(pimpl->fadeDurationMs);
    pimpl->fadeAnimation->setEasingCurve(static_cast<QEasingCurve::Type>(pimpl->easingCurveType));
    pimpl->fadeAnimation->setStartValue(windowOpacity());
    pimpl->fadeAnimation->setEndValue(0.0);
    pimpl->fadeAnimation->start();
}

//--------------------------------------------------------------------------

void FloatingDialogFrame::finishClose()
{
    pimpl->fadeAnimation->stop();
    hide();
    setWindowOpacity(1.0);          // restored before the next popup()
    emit closed();                  // pimpl->closing is still true here, so a close() re-entered
                                     // from a closed()/closeRequested() round trip is a no-op

    if (pimpl->pendingAutoDestroy)
    {
        if (!pimpl->explicitDragHandle && !pimpl->dragHandle.isNull())
        {
            pimpl->dragHandle->removeEventFilter(this);
            pimpl->dragHandle=nullptr;
        }
        destroyWidget(pimpl->content);
        pimpl->content=nullptr;
    }

    pimpl->closing=false;
}

//--------------------------------------------------------------------------

void FloatingDialogFrame::paintEvent(QPaintEvent* event)
{
    // QFrame's default CE_ShapedFrame drawing is not reliably composited on a
    // WA_TranslucentBackground top-level window -- explicitly invoking PE_Widget is the
    // standard technique to make the QSS background/border/radius box model apply anyway (see
    // the identical idiom, and its longer explanation, in DropdownFrame::paintEvent()).
    Q_UNUSED(event)

    QStyleOption opt;
    opt.initFrom(this);
    QPainter painter(this);
    style()->drawPrimitive(QStyle::PE_Widget,&opt,&painter,this);
}

//--------------------------------------------------------------------------

bool FloatingDialogFrame::eventFilter(QObject* obj, QEvent* event)
{
    if (obj==pimpl->dragHandle.data())
    {
        switch (event->type())
        {
            case (QEvent::MouseButtonPress):
            {
                auto* ev=static_cast<QMouseEvent*>(event);
                if (ev->buttons() & Qt::LeftButton)
                {
                    pimpl->dragOffset=ev->globalPosition().toPoint()-pos();
                    pimpl->dragging=true;
                }
            }
            break;

            case (QEvent::MouseMove):
            {
                auto* ev=static_cast<QMouseEvent*>(event);
                if (pimpl->dragging && (ev->buttons() & Qt::LeftButton))
                {
                    auto newPos=ev->globalPosition().toPoint()-pimpl->dragOffset;
                    clampToScreen(newPos);
                    move(newPos);
                    pimpl->hasPosition=true;
                }
            }
            break;

            case (QEvent::MouseButtonRelease):
            {
                if (pimpl->dragging)
                {
                    pimpl->dragging=false;
                    emit moved(pos());
                }
            }
            break;

            default:
            break;
        }
    }
    else if (obj==pimpl->hostWindow.data())
    {
        switch (event->type())
        {
            case (QEvent::Hide): [[fallthrough]];
            case (QEvent::Close):
            {
                if (pimpl->closing)
                {
                    // the host is going away -- there is nothing left to fade against, so jump
                    // straight to the end state instead of leaving an orphaned animation running
                    finishClose();
                }
                if (pimpl->followHostVisibility && isVisible())
                {
                    pimpl->hiddenByHost=true;
                    hide();
                }
            }
            break;

            case (QEvent::Show):
            {
                if (pimpl->followHostVisibility && pimpl->hiddenByHost)
                {
                    showFrame(this,pimpl.get());
                }
            }
            break;

            case (QEvent::WindowActivate):
            {
                // Qt::Dialog plus a parent already asks the OS to keep this frame above its
                // host, but that relationship is not automatically re-asserted every time the
                // host itself is brought forward by the user -- a click on it, Cmd+Tab, Mission
                // Control, clicking a different window of the same app and back, ... raise()
                // (never activateWindow(), which would steal focus right back from the host and
                // fight the user -- this is "stay above its own parent", not "always on top")
                // restacks the frame above the host without taking focus away from it.
                if (isVisible() && !pimpl->closing)
                {
                    raise();
                }
            }
            break;

            default:
            break;
        }
    }

    return QFrame::eventFilter(obj,event);
}

//--------------------------------------------------------------------------

void FloatingDialogFrame::closeEvent(QCloseEvent* event)
{
    // A WM-initiated close (titlebar / Cmd+W) must not hide the window immediately -- that
    // would preempt the fade. close() starts the fade (or, with fadeDurationMs()==0, has
    // already run finishClose() synchronously by the time this returns) and the frame hides
    // itself once that finishes; ignore() lets it stay showing meanwhile.
    close(pimpl->contentAutoDestroy);
    event->ignore();
}

//--------------------------------------------------------------------------

void FloatingDialogFrame::clampToScreen(QPoint& pos) const
{
    auto* screen=QGuiApplication::screenAt(pos);
    if (screen==nullptr)
    {
        screen=this->screen();
    }
    if (screen==nullptr)
    {
        screen=QGuiApplication::primaryScreen();
    }
    if (screen==nullptr)
    {
        return;
    }

    auto avail=screen->availableGeometry();
    auto margin=pimpl->minVisibleMargin;

    auto minX=avail.left()-width()+margin;
    auto maxX=avail.right()-margin;
    if (pos.x()<minX)
    {
        pos.setX(minX);
    }
    else if (pos.x()>maxX)
    {
        pos.setX(maxX);
    }

    auto minY=avail.top()-height()+margin;
    auto maxY=avail.bottom()-margin;
    if (pos.y()<minY)
    {
        pos.setY(minY);
    }
    else if (pos.y()>maxY)
    {
        pos.setY(maxY);
    }
}

//--------------------------------------------------------------------------

void FloatingDialogFrame::clampFullyToScreen(QPoint& pos) const
{
    auto* screen=QGuiApplication::screenAt(pos);
    if (screen==nullptr)
    {
        screen=this->screen();
    }
    if (screen==nullptr)
    {
        screen=QGuiApplication::primaryScreen();
    }
    if (screen==nullptr)
    {
        return;
    }

    auto avail=screen->availableGeometry();

    // Unlike clampToScreen() (written for dragging, where only minVisibleMargin needs to stay
    // on screen), a popup anchored to a UI control should stay fully visible when it fits --
    // falling back to the ordinary margin-based clamp only if the frame itself is larger than
    // the available screen area.
    if (width()<=avail.width())
    {
        auto minX=avail.left();
        auto maxX=avail.right()-width()+1;
        if (pos.x()<minX)
        {
            pos.setX(minX);
        }
        else if (pos.x()>maxX)
        {
            pos.setX(maxX);
        }
    }

    if (height()<=avail.height())
    {
        auto minY=avail.top();
        auto maxY=avail.bottom()-height()+1;
        if (pos.y()<minY)
        {
            pos.setY(minY);
        }
        else if (pos.y()>maxY)
        {
            pos.setY(maxY);
        }
    }

    clampToScreen(pos);
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END

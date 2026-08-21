/**
@copyright Evgeny Sidorov 2022

This software is dual-licensed. Choose the appropriate license for your project.

1. The GNU GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-GPLv3.md](LICENSE-GPLv3.md) or copy at https://www.gnu.org/licenses/gpl-3.0.txt)
    
2. The GNU LESSER GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-LGPLv3.md](LICENSE-LGPLv3.md) or copy at https://www.gnu.org/licenses/lgpl-3.0.txt).

You may select, at your option, one of the above-listed licenses.

*/

/****************************************************************************/

/** @file uise/desktop/src/modalpopup.cpp
*
*  Defines FrameWithModalPopup.
*
*/

/****************************************************************************/

#include <QEvent>
#include <QResizeEvent>
#include <QMouseEvent>
#include <QShortcut>
#include <QPalette>
#include <QBoxLayout>
#include <QPointer>

#include <uise/desktop/utils/destroywidget.hpp>
#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/modalpopup.hpp>
#include <uise/desktop/abstractdialog.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/**********************************ModalPopup********************************/

//--------------------------------------------------------------------------

class ModalPopup_p
{
    public:

        QWidget* widget=nullptr;
        FrameWithModalPopup* parent=nullptr;
        QShortcut* shortcut=nullptr;

        bool shortcutEnabled=true;
        bool outsideClickEnabled=true;
        bool autoDestroy=false;
        bool inUpdate=false;
};

//--------------------------------------------------------------------------

ModalPopup::ModalPopup(FrameWithModalPopup* parent)
    : QFrame(parent),
      pimpl(std::make_unique<ModalPopup_p>())
{
    pimpl->parent=parent;
    pimpl->shortcut=new QShortcut(Qt::Key_Escape, this);
    pimpl->shortcut->setContext(Qt::WindowShortcut);
    connect(
        pimpl->shortcut,
        &QShortcut::activated,
        this,
        [this]()
        {
            close(pimpl->autoDestroy);
        }
    );
    setVisible(false);
}

//--------------------------------------------------------------------------

ModalPopup::~ModalPopup()
{}

//--------------------------------------------------------------------------

void ModalPopup::setWidget(QWidget* widget, bool autoDestroy)
{
    pimpl->autoDestroy=autoDestroy;
    pimpl->widget=widget;
    pimpl->widget->setParent(this);
    pimpl->widget->installEventFilter(this);
}

//--------------------------------------------------------------------------

bool ModalPopup::eventFilter(QObject* watched, QEvent* event)
{
    if (watched==pimpl->widget
        && event->type()==QEvent::LayoutRequest
        && pimpl->parent->isPopupAutoHeight()
        && isVisible()
        && !pimpl->inUpdate)
    {
        // content requested a new layout (e.g. a wrapped multiline error appeared) -
        // refit the popup to the new content height
        updateWidgetGeometry();
    }
    return QFrame::eventFilter(watched,event);
}

//--------------------------------------------------------------------------

void ModalPopup::popup()
{
    QPalette pal = pimpl->parent->palette();
    auto background=pal.color(QPalette::Window);

    if (pimpl->parent->isAutoColor())
    {
        QString css("uise--ModalPopup {background-color: rgba(%1,%2,%3,%4);}");
        css=css.arg(255-background.red()).arg(255-background.green()).arg(255-background.blue()).arg(pimpl->parent->getPopupAlpha());
        setStyleSheet(css);
    }

    pimpl->shortcut->setEnabled(pimpl->shortcutEnabled);

    // Prime the widget BEFORE it becomes visible, so it is measured once at its final size
    // instead of appearing empty/undersized and then visibly refitting as content settles.
    // Polish first (so any QSS qproperty-* driven geometry is applied), then let the dialog
    // settle whatever content-driven geometry it owns (e.g. FileUploadWidget's list area).
    polishWidgetTree();
    if (auto* dialog=qobject_cast<AbstractDialog*>(pimpl->widget))
    {
        dialog->prepareToShow();
    }

    updateWidgetGeometry();

    show();
    raise();
    pimpl->widget->setVisible(true);
    pimpl->widget->raise();
    pimpl->widget->setFocus();
}

//--------------------------------------------------------------------------

void ModalPopup::polishWidgetTree()
{
    if (pimpl->widget==nullptr)
    {
        return;
    }

    // ensurePolished() early-returns on an already-polished widget and does not recurse into
    // its children, so walk the subtree explicitly -- same reasoning as
    // DropdownFrame::measureContentSize().
    pimpl->widget->ensurePolished();
    const auto descendants=pimpl->widget->findChildren<QWidget*>();
    for (auto* w : descendants)
    {
        w->ensurePolished();
    }
}

//--------------------------------------------------------------------------

void ModalPopup::close(bool autoDestroy)
{
    hide();
    pimpl->shortcut->setEnabled(false);
    pimpl->parent->setPopupHidden();
    if (autoDestroy)
    {
        destroyWidget(pimpl->widget);
        pimpl->widget=nullptr;
    }
}

//--------------------------------------------------------------------------

void ModalPopup::resizeEvent(QResizeEvent *event)
{
    std::ignore=event;
    updateWidgetGeometry();
}

//--------------------------------------------------------------------------

void ModalPopup::mousePressEvent(QMouseEvent* event)
{
    // This handler does NOT run only for presses that land on this frame's own backdrop: a
    // press a descendant widget leaves unaccepted (QWidget::mousePressEvent's default impl
    // calls event->ignore(), e.g. any control that reacts on release/clicked rather than press,
    // such as AccountSelectButton) is redelivered by Qt to each ancestor up the parent chain
    // until something accepts it or it reaches the top-level widget -- see DropdownFrame's own
    // eventFilter() for the same mechanism spelled out in detail. Without the geometry check
    // below, an unaccepted press anywhere inside the dialog would bubble all the way up to here
    // and be misread as a click on the backdrop, closing the dialog out from under the user.
    if (pimpl->outsideClickEnabled
        && (pimpl->widget==nullptr || !pimpl->widget->geometry().contains(event->pos())))
    {
        close(pimpl->autoDestroy);
        return;
    }
    QFrame::mousePressEvent(event);
}

//--------------------------------------------------------------------------

void ModalPopup::updateWidgetGeometry()
{
    if (pimpl->widget==nullptr)
    {
        return;
    }

    auto w=width();
    auto h=height();    

    auto margins=contentsMargins();
    w-=(margins.left()+margins.right());
    h-=(margins.top()+margins.bottom());

    auto setPos=[h,w,&margins,this](int width, int height)
    {
        auto x=(w-width)/2+margins.left();
        if (x<margins.left())
        {
            x=margins.left();
        }
        auto y=(h-height)/2+margins.top()-20;
        if (y<margins.top())
        {
            y=margins.top();
        }
        pimpl->widget->move(x,y);
    };

    auto minSize=pimpl->widget->minimumSize();
    auto maxSize=pimpl->widget->maximumSize();
    if (minSize==maxSize && minSize.isValid())
    {
        // no resize needed
        setPos(minSize.width(),minSize.height());
        return;
    }

    auto newW=w * pimpl->parent->maxWidthPercent()/100;
    auto newH=h * pimpl->parent->maxHeightPercent()/100;

    if (maxSize.width()>0 && newW>maxSize.width())
    {
        newW=maxSize.width();
    }
    if (minSize.width()>0 && newW<minSize.width())
    {
        newW=minSize.width();
    }

    // Measure at the width we are about to use, not at whatever stale width the widget still
    // has (e.g. a construction default, or the width of a previous, differently-sized dialog
    // reusing this popup) -- otherwise sizeHint().height() below answers for the wrong width.
    // Guarded on newW>0: with a host frame not yet laid out, w (and so newW) is 0, and locking
    // that in as the widget's real geometry is what once shrank the whole popup to a tiny
    // top-left rectangle (see FileUploadWidget::doUpdateListAreaHeight()). The widget is still
    // hidden at this point, so resize() alone only records the new geometry -- activate() is
    // what actually re-lays out its children at the new width so sizeHint() reflects it.
    if (newW>0 && pimpl->widget->width()!=newW && pimpl->widget->layout()!=nullptr)
    {
        pimpl->inUpdate=true;
        pimpl->widget->resize(newW,pimpl->widget->height());
        pimpl->widget->layout()->activate();
        pimpl->inUpdate=false;

        // re-read: the width change just activated above may have published a new
        // SetDefaultConstraint minimum/maximum height, which the height clamps below need to
        // see.
        minSize=pimpl->widget->minimumSize();
        maxSize=pimpl->widget->maximumSize();
    }

    if (pimpl->parent->isPopupAutoHeight())
    {
        // fit height to content at the resolved width
        auto contentH=pimpl->widget->heightForWidth(newW);
        if (contentH<=0)
        {
            contentH=pimpl->widget->sizeHint().height();
        }
        newH=contentH;

        // upper bound: percent of parent (screen safety), or the explicit max height if smaller
        auto cap=h * pimpl->parent->maxHeightPercent()/100;
        if (maxSize.height()>0 && maxSize.height()<cap)
        {
            cap=maxSize.height();
        }
        if (newH>cap)
        {
            newH=cap;
        }
    }
    else if (maxSize.height()>0 && newH>maxSize.height())
    {
        newH=maxSize.height();
    }
    if (minSize.height()>0 && newH<minSize.height())
    {
        newH=minSize.height();
    }

    pimpl->inUpdate=true;
    pimpl->widget->resize(newW,newH);
    if (auto* l=pimpl->widget->layout())
    {
        // The widget is hidden on the popup() path, so this resize would otherwise only be
        // delivered as a pending QResizeEvent once show() runs -- relayouting, and possibly
        // posting a LayoutRequest that refits the popup, only after it is already on screen.
        // Activating now settles that before the first paint.
        l->activate();
    }
    pimpl->inUpdate=false;
    setPos(newW,newH);
}

//--------------------------------------------------------------------------

void ModalPopup::setShortcutEnabled(bool enable)
{
    pimpl->shortcutEnabled=enable;
    pimpl->shortcut->setEnabled(pimpl->shortcutEnabled);
}

//--------------------------------------------------------------------------

bool ModalPopup::isShortcutEnabled() const
{
    return pimpl->shortcutEnabled;
}

//--------------------------------------------------------------------------

void ModalPopup::setOutsideClickEnabled(bool enable)
{
    pimpl->outsideClickEnabled=enable;
}

//--------------------------------------------------------------------------

bool ModalPopup::isOutsideClickEnabled() const
{
    return pimpl->outsideClickEnabled;
}

/****************************FrameWithModalPopup******************************/

//--------------------------------------------------------------------------

class FrameWithModalPopup_p
{
    public:

        ModalPopup* popup;
        bool locked=false;
        bool autoDestroy=true;
        bool autoColor=false;
        bool autoHeight=false;
        QBoxLayout* layout=nullptr;
        QPointer<QWidget> contentWidget;

        int maxWidthPercent=FrameWithModalPopup::DefaultMaxWidthPercent;
        int maxHeightPercent=FrameWithModalPopup::DefaultMaxHeightPercent;
        int popupAlpha=FrameWithModalPopup::DefaultPopupAlpha;
};

//--------------------------------------------------------------------------

FrameWithModalPopup::FrameWithModalPopup(QWidget* parent)
    : QFrame(parent),
      pimpl(std::make_unique<FrameWithModalPopup_p>())
{
    pimpl->popup=new ModalPopup(this);
}

//--------------------------------------------------------------------------

FrameWithModalPopup::~FrameWithModalPopup()
{}

//--------------------------------------------------------------------------

void FrameWithModalPopup::setPopupWidget(QWidget* widget, bool autoDestroy)
{
    pimpl->popup->close();
    pimpl->popup->setWidget(widget,autoDestroy);
    pimpl->autoDestroy=autoDestroy;
    setMinimumWidth(widget->minimumWidth()+40);
}

//--------------------------------------------------------------------------

void FrameWithModalPopup::popup()
{
    pimpl->locked=true;
    pimpl->popup->popup();
}

//--------------------------------------------------------------------------

void FrameWithModalPopup::closePopup()
{
    pimpl->popup->close(pimpl->autoDestroy);
}

//--------------------------------------------------------------------------

bool FrameWithModalPopup::isPopupLocked() const
{
    return pimpl->locked;
}

//--------------------------------------------------------------------------

void FrameWithModalPopup::setPopupHidden()
{
    pimpl->locked=false;
    emit popupHidden();
}

//--------------------------------------------------------------------------

void FrameWithModalPopup::resizeEvent(QResizeEvent *event)
{
    QFrame::resizeEvent(event);

    pimpl->popup->resize(event->size());
    auto margins=contentsMargins();
    pimpl->popup->move(margins.left(),margins.top());
}

//--------------------------------------------------------------------------

void FrameWithModalPopup::setMaxWidthPercent(int val)
{
    pimpl->maxWidthPercent=val;
}

//--------------------------------------------------------------------------

int FrameWithModalPopup::maxWidthPercent() const
{
    return pimpl->maxWidthPercent;
}

//--------------------------------------------------------------------------

void FrameWithModalPopup::setMaxHeightPercent(int val)
{
    pimpl->maxHeightPercent=val;
}

//--------------------------------------------------------------------------

int FrameWithModalPopup::maxHeightPercent() const
{
    return pimpl->maxHeightPercent;
}

//--------------------------------------------------------------------------

void FrameWithModalPopup::setPopupAlpha(int val)
{
    pimpl->popupAlpha=val;
}

//--------------------------------------------------------------------------

int FrameWithModalPopup::getPopupAlpha() const
{
    return pimpl->popupAlpha;
}

//--------------------------------------------------------------------------

void FrameWithModalPopup::setShortcutEnabled(bool enable)
{
    pimpl->popup->setShortcutEnabled(enable);
}

//--------------------------------------------------------------------------

bool FrameWithModalPopup::isShortcutEnabled() const
{
    return pimpl->popup->isShortcutEnabled();
}

//--------------------------------------------------------------------------

void FrameWithModalPopup::setOutsideClickEnabled(bool enable)
{
    pimpl->popup->setOutsideClickEnabled(enable);
}

//--------------------------------------------------------------------------

bool FrameWithModalPopup::isOutsideClickEnabled() const
{
    return pimpl->popup->isOutsideClickEnabled();
}

//--------------------------------------------------------------------------

void FrameWithModalPopup::setAutoColor(bool enable)
{
    pimpl->autoColor=enable;
}

//--------------------------------------------------------------------------

bool FrameWithModalPopup::isAutoColor() const
{
    return pimpl->autoColor;
}

//--------------------------------------------------------------------------

void FrameWithModalPopup::setPopupAutoHeight(bool enable)
{
    pimpl->autoHeight=enable;
}

//--------------------------------------------------------------------------

bool FrameWithModalPopup::isPopupAutoHeight() const
{
    return pimpl->autoHeight;
}

//--------------------------------------------------------------------------

void FrameWithModalPopup::setContentWidget(QWidget* widget)
{
    destroyWidget(pimpl->contentWidget);
    if (pimpl->layout)
    {
        pimpl->layout->deleteLater();
    }

    pimpl->layout=Layout::vertical(this);
    pimpl->layout->addWidget(widget);
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END

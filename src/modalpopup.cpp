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

#include <vector>

#include <QEvent>
#include <QResizeEvent>
#include <QMouseEvent>
#include <QShortcut>
#include <QPalette>
#include <QBoxLayout>
#include <QPointer>
#include <QAbstractScrollArea>
#include <QScrollBar>

#include <uise/desktop/utils/destroywidget.hpp>
#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/modalpopup.hpp>
#include <uise/desktop/abstractdialog.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

namespace {

/**
 * @brief Nearest ancestor scroll area that actually SCROLLS the given widget.
 *
 * The test is deliberately not "is any ancestor a QAbstractScrollArea": a scroll area is an
 * ancestor of its scrollbars, its corner widget and of anything parented directly to it, none
 * of which it scrolls, and clipping those to the viewport would be wrong. Only the chain that
 * passes THROUGH viewport() is scrolled content, so the loop remembers the child it came from
 * and accepts a scroll area only when that child is its viewport.
 *
 * This also keeps every scroll area that lives INSIDE a popup out of the picture, since those
 * are descendants and are never walked: FrameWithModalPopup hosts whose popup widget is itself
 * a ScrollArea (AddAccountNode's config panel) or whose content contains one (AddContact's info
 * panel) must keep centring against their own rect.
 */
QAbstractScrollArea* ancestorScrollArea(const QWidget* widget)
{
    const QWidget* child=widget;
    auto* parent=widget->parentWidget();
    while (parent!=nullptr)
    {
        auto* area=qobject_cast<QAbstractScrollArea*>(parent);
        if (area!=nullptr && area->viewport()==child)
        {
            return area;
        }
        child=parent;
        parent=parent->parentWidget();
    }
    return nullptr;
}

}

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

        //! Scrolling ancestor this popup currently follows, resolved in bindScrollTracking().
        //! QPointer because the scroll area belongs to the host's widget tree, which can be
        //! torn down independently of this popup (e.g. an HTree node destroying its content
        //! while a dialog is still nominally open), and unbindScrollTracking() dereferences it
        //! to reach viewport().
        QPointer<QAbstractScrollArea> scrollArea;

        //! Scrollbar connections made in bindScrollTracking(). Qt would drop them automatically
        //! when either end dies, but they must also be dropped on close() -- a closed popup
        //! must not keep repositioning a hidden widget on every scroll tick -- and re-made
        //! against a possibly different scroll area on the next popup().
        std::vector<QMetaObject::Connection> scrollConnections;
};

//--------------------------------------------------------------------------

ModalPopup::ModalPopup(FrameWithModalPopup* parent)
    : QFrame(parent),
      pimpl(std::make_unique<ModalPopup_p>())
{
    pimpl->parent=parent;
    pimpl->shortcut=new QShortcut(Qt::Key_Escape, this);
    pimpl->shortcut->setContext(Qt::WindowShortcut);
    // QShortcut defaults to enabled -- popup()/close() toggle it correctly around actual
    // show/hide, but nothing disabled it in between construction and the first popup()/close()
    // call, so a ModalPopup that has never been opened yet still carries a LIVE, Qt::
    // WindowShortcut-scoped Escape shortcut in the same top-level window as setVisible(false)
    // just below suggests it shouldn't. Found live: a ChatPage eagerly constructs several
    // FrameWithModalPopup hosts (fileUploadFrame/replyFrame/forwardFrame) up front, none of
    // which the user may ever have opened -- each contributes one more enabled WindowShortcut
    // Escape shortcut to the window regardless, and as soon as 2+ such shortcuts (any
    // combination of these, or a THIRD popup like ModalChatSelectDialog opened on top) are
    // simultaneously enabled, Qt treats Escape as ambiguous and it does nothing for ANY of
    // them -- not even the intended, actually-visible top popup. Disabling here, matching
    // setVisible(false), closes that gap; popup() still explicitly re-enables per
    // shortcutEnabled on every actual open, so this has no effect once a popup has been shown
    // at least once.
    pimpl->shortcut->setEnabled(false);
    connect(
        pimpl->shortcut,
        &QShortcut::activated,
        this,
        [this]()
        {
            closeByUser();
        }
    );
    // Same reasoning DropdownFrame's own Escape shortcut already documents (dropdownframe.cpp):
    // when 2+ enabled Qt::WindowShortcut-context shortcuts share the same key sequence in the
    // same top-level window, Qt fires activatedAmbiguously() on ALL of them instead of
    // activated() on any -- disabling this shortcut whenever this popup itself is not the one
    // open (see setEnabled(false) above/close()) narrows that window a lot, but does not close
    // it entirely: this popup can legitimately be the CURRENTLY VISIBLE, topmost one while
    // another ModalPopup instance elsewhere in the same window is ALSO legitimately open at
    // once (e.g. this app's own nested reply/forward/file-upload dialog chain within one
    // ChatPage, or a second independent popup opened on top, like the window-level chat-select
    // dialog). Ambiguous or not, Escape should still close the actual visible popup -- treat
    // both signals identically, exactly like DropdownFrame's own escShortcut does.
    connect(
        pimpl->shortcut,
        &QShortcut::activatedAmbiguously,
        this,
        [this]()
        {
            closeByUser();
        }
    );
    setVisible(false);
}

//--------------------------------------------------------------------------

ModalPopup::~ModalPopup()
{
    // QObject's own destructor would drop both the connections and the installed event filter,
    // but close() is not guaranteed to have run (a host can be destroyed with its popup still
    // up), and being explicit keeps the pairing with bindScrollTracking() obvious.
    unbindScrollTracking();
}

//--------------------------------------------------------------------------

void ModalPopup::closeByUser()
{
    // A dialog the caller marked non-closable is dismissed by its own buttons only, so the
    // host must not act on Escape or on a click outside it. The flag is read here rather than
    // mirrored into shortcutEnabled/outsideClickEnabled so that it stays independent of what
    // the host itself configured, and so that a setClosable() call arriving after the popup is
    // already on screen takes effect immediately.
    auto* dialog=qobject_cast<AbstractDialog*>(pimpl->widget);
    if (dialog!=nullptr && !dialog->isClosable())
    {
        return;
    }
    close(pimpl->autoDestroy);
}

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
    else if (!pimpl->scrollArea.isNull()
             && watched==pimpl->scrollArea->viewport()
             && event->type()==QEvent::Resize
             && isVisible())
    {
        // See bindScrollTracking(): a viewport resize can change the rect this dialog is sized
        // and centred against without producing a resizeEvent here -- with
        // setWidgetResizable(true) and content taller than the viewport, the scrolled widget
        // keeps its content-driven height across a window resize, so this frame's own size (and
        // thus resizeEvent()) never changes even though the visible band inside it just did.
        // Full update rather than a bare reposition, because the maxWidthPercent/
        // maxHeightPercent budget just changed too.
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

    // Resolved fresh on every open, never cached: a host can be reparented into or out of a
    // scroll area between opens, and re-binding is just a couple of pointer walks.
    bindScrollTracking();

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
    // A closed popup must stop following the scroll area: the connections would otherwise keep
    // firing repositionWidget() on a hidden widget for the rest of the host's life, and they are
    // re-made against a freshly resolved ancestor on the next popup() anyway.
    unbindScrollTracking();
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
        closeByUser();
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

    // Everything below is computed against the VISIBLE part of this frame rather than its full
    // rect. This frame always covers its whole host (FrameWithModalPopup::resizeEvent()), but
    // when that host is the scrolled widget of a setWidgetResizable(true) scroll area, its
    // height is the entire canvas: centring in it put status/confirmation dialogs far below the
    // viewport, and a maxHeightPercent of 50 meant 50% of the canvas rather than 50% of what the
    // user can see. visibleContentsRect() returns contentsRect() unchanged when there is no such
    // ancestor, so unscrolled hosts are unaffected.
    const auto vis=visibleContentsRect();
    auto w=vis.width();
    auto h=vis.height();

    auto minSize=pimpl->widget->minimumSize();
    auto maxSize=pimpl->widget->maximumSize();
    if (minSize==maxSize && minSize.isValid())
    {
        // no resize needed
        repositionWidget();
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
    repositionWidget();
}

//--------------------------------------------------------------------------

void ModalPopup::repositionWidget()
{
    if (pimpl->widget==nullptr)
    {
        return;
    }

    const auto vis=visibleContentsRect();
    const auto size=pimpl->widget->size();

    auto x=(vis.width()-size.width())/2+vis.left();
    if (x<vis.left())
    {
        x=vis.left();
    }
    // The -20 nudges the dialog slightly above the true centre: a dialog centred on the exact
    // geometric middle reads as sitting low, and this is the long-standing optical correction.
    auto y=(vis.height()-size.height())/2+vis.top()-20;
    if (y<vis.top())
    {
        y=vis.top();
    }
    pimpl->widget->move(x,y);
}

//--------------------------------------------------------------------------

QRect ModalPopup::visibleContentsRect() const
{
    const auto base=contentsRect();

    auto* area=ancestorScrollArea(this);
    if (area==nullptr)
    {
        return base;
    }

    auto* viewport=area->viewport();
    if (viewport==nullptr)
    {
        return base;
    }

    // mapFrom() walks the parent chain, and ancestorScrollArea() guarantees the viewport IS an
    // ancestor of this frame, which is exactly its precondition. The resulting origin is
    // positive-y once the user has scrolled past the top of this frame -- i.e. the visible band
    // slides down through this frame's coordinate system, which is the whole point.
    const QRect visible{mapFrom(viewport,QPoint{0,0}),viewport->size()};

    const auto r=base.intersected(visible);
    if (r.isEmpty())
    {
        // The host is scrolled entirely out of view. Nothing sensible to centre in, and an
        // empty rect would collapse the dialog to nothing, so fall back to the old behaviour and
        // let the next scroll tick put it right.
        return base;
    }
    return r;
}

//--------------------------------------------------------------------------

void ModalPopup::bindScrollTracking()
{
    unbindScrollTracking();

    auto* area=ancestorScrollArea(this);
    if (area==nullptr)
    {
        return;
    }
    pimpl->scrollArea=area;

    // Reposition ONLY. Scrolling never changes the size of the visible band this dialog is
    // centred in, only its offset within this frame, so there is nothing to resize or relayout
    // -- and doing either here would run a full measure pass on every scrollbar tick.
    //
    // Runs after the scroll area has already moved its scrolled widget: QAbstractScrollArea
    // connects its own scrollContentsBy() handler to these signals in its constructor, long
    // before this connection is made, and Qt delivers direct connections in connection order. So
    // mapFrom() in visibleContentsRect() already sees the post-scroll offset.
    auto reposition=[this]()
    {
        if (isVisible())
        {
            repositionWidget();
        }
    };
    pimpl->scrollConnections.push_back(
        connect(area->verticalScrollBar(),&QAbstractSlider::valueChanged,this,reposition)
    );
    pimpl->scrollConnections.push_back(
        connect(area->horizontalScrollBar(),&QAbstractSlider::valueChanged,this,reposition)
    );

    // The viewport can change size without anything else telling us: with
    // setWidgetResizable(true) and content taller than the viewport, the scrolled widget keeps
    // its content-driven height when the window is resized, so this frame gets no resizeEvent
    // and the scrollbars do not move -- yet the rect we size and centre against just changed.
    area->viewport()->installEventFilter(this);
}

//--------------------------------------------------------------------------

void ModalPopup::unbindScrollTracking()
{
    for (const auto& connection : pimpl->scrollConnections)
    {
        disconnect(connection);
    }
    pimpl->scrollConnections.clear();

    if (!pimpl->scrollArea.isNull() && pimpl->scrollArea->viewport()!=nullptr)
    {
        pimpl->scrollArea->viewport()->removeEventFilter(this);
    }
    pimpl->scrollArea.clear();
}

//--------------------------------------------------------------------------

void ModalPopup::setShortcutEnabled(bool enable)
{
    pimpl->shortcutEnabled=enable;
    // Only a popup that is actually ON SCREEN may hold a LIVE Escape shortcut -- see this class's
    // own ctor comment: 2+ simultaneously enabled Qt::WindowShortcut Escape shortcuts in one
    // window make Escape ambiguous, and Qt then hands a press to just one of them instead of
    // firing activated() on the intended one. The ctor closes that gap for a popup that has never
    // been opened, but arming the shortcut unconditionally here re-opened it immediately for
    // every dialog whose OWN constructor re-enables the shortcut to undo ModalDialog<>'s
    // disabled-by-default (ModalFileUploadDialog, ModalReplyDialog, ModalForwardDialog all do) --
    // a ChatPage constructs all three of those hosts eagerly, so it carried three armed Escape
    // shortcuts for popups that were not showing and might never be shown, on top of its own
    // window-level one. Costs nothing to defer: popup() applies shortcutEnabled on every open and
    // close() clears it again, while an already-visible popup still takes the change immediately,
    // which is what FrameWithModalStatus's live closable/cancellable updates rely on.
    pimpl->shortcut->setEnabled(pimpl->shortcutEnabled && isVisible());
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

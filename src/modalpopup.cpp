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

#include <QResizeEvent>
#include <QShortcut>
#include <QPalette>

#include <uise/desktop/utils/destroywidget.hpp>
#include <uise/desktop/modalpopup.hpp>

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
        bool autoDestroy=false;
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
}

//--------------------------------------------------------------------------

void ModalPopup::popup()
{
    QPalette pal = pimpl->parent->palette();
    auto background=pal.color(QPalette::Window);

    QString css("uise--ModalPopup {background-color: rgba(%1,%2,%3,%4);}");
    css=css.arg(255-background.red()).arg(255-background.green()).arg(255-background.blue()).arg(pimpl->parent->getPopupAlpha());
    setStyleSheet(css);

    pimpl->shortcut->setEnabled(pimpl->shortcutEnabled);
    updateWidgetGeometry();

    show();
    raise();
    pimpl->widget->setVisible(true);
    pimpl->widget->raise();
    pimpl->widget->setFocus();
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
        auto y=(h-height)/2+margins.top();
        if (y<margins.top())
        {
            y=margins.top();
        }
        pimpl->widget->move(x,y);
    };

    auto minSize=pimpl->widget->minimumSize();
    if (minSize.isNull())
    {
        minSize=pimpl->widget->minimumSizeHint();
    }
    auto maxSize=pimpl->widget->maximumSize();
    if (minSize==maxSize && minSize.isValid())
    {
        // no resize needed
        setPos(minSize.width(),minSize.height());
        return;
    }

    auto newW=w * pimpl->parent->maxWidthPercent()/100;
    auto newH=h * pimpl->parent->maxHeightPercent()/100;

    if (maxSize.width()!=0 && newW>maxSize.width())
    {
        newW=maxSize.width();
    }
    if (minSize.width()!=0 && newW<minSize.width())
    {
        newW=minSize.width();
    }

    if (maxSize.height()!=0 && newH>maxSize.height())
    {
        newH=maxSize.height();
    }
    if (minSize.height()!=0 && newH<minSize.height())
    {
        newH=minSize.height();
    }

    pimpl->widget->resize(newW,newH);
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

/****************************FrameWithModalPopup******************************/

//--------------------------------------------------------------------------

class FrameWithModalPopup_p
{
    public:

        ModalPopup* popup;
        bool locked=false;
        bool autoDestroy=true;

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

UISE_DESKTOP_NAMESPACE_END

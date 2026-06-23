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

/** @file uise/desktop/src/loadingframe.cpp
*
*  Defines LoadingFrame.
*
*/

/****************************************************************************/

#include <QPointer>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/utils/destroywidget.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/abstractloadingwidget.hpp>
#include <uise/desktop/pushbutton.hpp>
#include <uise/desktop/loadingframe.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/************************** LoadingFrame ****************************/

//--------------------------------------------------------------------------

class LoadingFrame_p
{
    public:

        bool cancellableBusyWaiting=false;

        QFrame* popupWidget=nullptr;
        QBoxLayout* popupLayout=nullptr;
        QPointer<AbstractLoadingWidget> loadingWidget;

        bool busyWaitingMode=false;
        QBoxLayout* layout=nullptr;
        QPointer<QWidget> content;
};

//--------------------------------------------------------------------------

LoadingFrame::LoadingFrame(QWidget* parent)
    : FrameWithModalPopup(parent),
      pimpl(std::make_unique<LoadingFrame_p>())
{
}

//--------------------------------------------------------------------------

void LoadingFrame::reconfigurePopupLayout()
{
    if (pimpl->popupLayout==nullptr || pimpl->loadingWidget==nullptr)
    {
        return;
    }

    // Clear every item (spacers are deleted; widgets are only removed).
    while (pimpl->popupLayout->count()>0)
    {
        delete pimpl->popupLayout->takeAt(0);
    }

    if (pimpl->loadingWidget->isCenterAligned())
    {
        // Spinner-style: center the widget inside the (size-constrained) popup.
        pimpl->popupLayout->addStretch(1000);
        pimpl->popupLayout->addWidget(pimpl->loadingWidget,0,Qt::AlignCenter);
        pimpl->popupLayout->addStretch(1000);
        setMaxWidthPercent(FrameWithModalPopup::DefaultMaxWidthPercent);
        setMaxHeightPercent(FrameWithModalPopup::DefaultMaxHeightPercent);
    }
    else
    {
        // Skeleton-style: widget expands to fill the entire LoadingFrame.
        pimpl->popupLayout->setContentsMargins(0,0,0,0);
        pimpl->popupLayout->setSpacing(0);
        pimpl->popupLayout->addWidget(pimpl->loadingWidget,1);
        setMaxWidthPercent(100);
        setMaxHeightPercent(100);
    }
}

//--------------------------------------------------------------------------

void LoadingFrame::construct()
{
    pimpl->popupWidget=new QFrame();
    pimpl->popupWidget->setObjectName("popupFrame");
    pimpl->popupLayout=Layout::vertical(pimpl->popupWidget);

    if (pimpl->loadingWidget==nullptr)
    {
        pimpl->loadingWidget=makeWidget<AbstractLoadingWidget>(pimpl->popupWidget);
    }
    else
    {
        pimpl->loadingWidget->setParent(pimpl->popupWidget);
    }
    Q_ASSERT(pimpl->loadingWidget);
    reconfigurePopupLayout();
    setPopupWidget(pimpl->popupWidget);

    connect(
        this,
        &LoadingFrame::popupHidden,
        this,
        [this]()
        {
            if (pimpl->busyWaitingMode && pimpl->cancellableBusyWaiting)
            {
                cancel();
            }
        }
    );
}

//--------------------------------------------------------------------------

LoadingFrame::~LoadingFrame()
{}

//--------------------------------------------------------------------------

void LoadingFrame::setCancellableBusyWaiting(bool enable)
{
    pimpl->cancellableBusyWaiting=enable;
}

//--------------------------------------------------------------------------

bool LoadingFrame::isCancellableBusyWaiting() const
{
    return pimpl->cancellableBusyWaiting;
}

//--------------------------------------------------------------------------

void LoadingFrame::popupBusyWaiting()
{
    if (pimpl->busyWaitingMode)
    {
        return;
    }

    pimpl->busyWaitingMode=true;
    setShortcutEnabled(pimpl->cancellableBusyWaiting);
    pimpl->loadingWidget->start();
    popup();
}

//--------------------------------------------------------------------------

QSize LoadingFrame::sizeHint() const
{
    if (pimpl->content!=nullptr)
    {
        auto m=pimpl->content->contentsMargins();
        auto sz=pimpl->content->sizeHint();
        sz.setWidth(sz.width()+m.left()+m.right());
        sz.setHeight(sz.height()+m.top()+m.bottom());
        return sz;
    }
    return FrameWithModalPopup::sizeHint();
}

//--------------------------------------------------------------------------

void LoadingFrame::cancel()
{
    finish();
    emit cancelled();
}

//--------------------------------------------------------------------------

void LoadingFrame::finish()
{
    if (!pimpl->busyWaitingMode)
    {
        return;
    }

    pimpl->busyWaitingMode=false;
    pimpl->loadingWidget->stop();
    closePopup();
}

//--------------------------------------------------------------------------

AbstractLoadingWidget* LoadingFrame::loadingWidget() const
{
    return pimpl->loadingWidget;
}

//--------------------------------------------------------------------------

void LoadingFrame::setLoadingWidget(AbstractLoadingWidget* widget)
{
    if (widget==pimpl->loadingWidget)
    {
        return;
    }

    if (pimpl->loadingWidget!=nullptr)
    {
        if (pimpl->popupLayout!=nullptr)
        {
            pimpl->popupLayout->removeWidget(pimpl->loadingWidget);
        }
        pimpl->loadingWidget->deleteLater();
    }

    pimpl->loadingWidget=widget;

    if (pimpl->loadingWidget==nullptr)
    {
        return;
    }

    if (pimpl->popupWidget!=nullptr)
    {
        pimpl->loadingWidget->setParent(pimpl->popupWidget);
    }

    reconfigurePopupLayout();
}

//--------------------------------------------------------------------------

void LoadingFrame::setWidget(QWidget* widget)
{
    destroyWidget(pimpl->content);

    pimpl->content=new QFrame(this);
    pimpl->layout=Layout::vertical(pimpl->content.get());
    pimpl->layout->addWidget(widget);
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END

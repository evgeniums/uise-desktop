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

/** @file uise/desktop/src/dialog.ipp
*
*  Defines Dialog.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_DIALOG_IPP
#define UISE_DESKTOP_DIALOG_IPP

#include <QPointer>
#include <QLabel>
#include <QSignalMapper>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/utils/destroywidget.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/pushbutton.hpp>
#include <uise/desktop/dialog.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/**************************** Dialog ***********************************/

//--------------------------------------------------------------------------

template <typename BaseT>
class Dialog_p
{
    public:

        Dialog<BaseT>* widget;

        QBoxLayout* layout;

        QFrame* titleFrame;
        QBoxLayout* titleLayout;
        QLabel* title;
        PushButton* titleClose;

        QFrame* contentFrame;
        QBoxLayout* contentLayout;

        QFrame* buttonsFrame;
        QBoxLayout* buttonLayout;

        QSignalMapper* buttonGroup;

        std::map<int,PushButton*> buttons;

        const auto& buttonsStyle() const
        {
            return Style::instance().buttonsStyle("Dialog",widget);
        }
};

//--------------------------------------------------------------------------

template <typename BaseT>
Dialog<BaseT>::Dialog(QWidget* parent)
    : BaseT(parent),
      pimpl(std::make_unique<Dialog_p<BaseT>>())
{
    pimpl->widget=this;

    pimpl->layout=Layout::vertical(this);

    pimpl->titleFrame=new QFrame(this);
    pimpl->titleFrame->setObjectName("titleFrame");
    auto tl=Layout::horizontal(pimpl->titleFrame);
    pimpl->layout->addWidget(pimpl->titleFrame);
    pimpl->titleClose=new PushButton(Style::instance().svgIconLocator().icon("DialogTitle::close",this),pimpl->titleFrame);
    pimpl->titleClose->setToolTip(AbstractDialog::tr("Close","dialog"));
    pimpl->title=new QLabel(pimpl->titleFrame);
    pimpl->title->setAlignment(Qt::AlignCenter);
    pimpl->title->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Preferred);
#ifdef Q_OS_MACOS
    tl->addWidget(pimpl->titleClose,0);
    tl->addWidget(pimpl->title,1);
    auto placeHolder=new QLabel();
    placeHolder->setObjectName("placeHolder");
    tl->addWidget(placeHolder);
#else
    tl->addWidget(pimpl->title,1);
    tl->addWidget(pimpl->titleClose);
#endif

    pimpl->contentFrame=new QFrame(this);
    pimpl->contentFrame->setObjectName("contentFrame");
    pimpl->contentLayout=Layout::horizontal(pimpl->contentFrame);
    pimpl->layout->addWidget(pimpl->contentFrame);

    pimpl->buttonGroup=new QSignalMapper(this);
    QObject::connect(
        pimpl->buttonGroup,
        &QSignalMapper::mappedInt,
        this,
        [this](int id)
        {
            emit AbstractDialog::buttonClicked(id);
            if (id==static_cast<int>(AbstractDialog::StandardButton::Close))
            {
                emit AbstractDialog::closeRequested();
            }
        }
    );

    QObject::connect(
        pimpl->titleClose,
        &PushButton::clicked,
        this,
        [this]()
        {
            emit AbstractDialog::closeRequested();
        }
    );

    doSetButtons({AbstractDialog::standardButton(AbstractDialog::StandardButton::Close,this)});
}

//--------------------------------------------------------------------------

template <typename BaseT>
Dialog<BaseT>::~Dialog()
{
}

//--------------------------------------------------------------------------

template <typename BaseT>
void Dialog<BaseT>::setWidget(QWidget* widget)
{
    return pimpl->contentLayout->addWidget(widget);
}

//--------------------------------------------------------------------------

template <typename BaseT>
void Dialog<BaseT>::setButtons(std::vector<AbstractDialog::ButtonConfig> buttons)
{
    doSetButtons(std::move(buttons));
}

//--------------------------------------------------------------------------

template <typename BaseT>
void Dialog<BaseT>::doSetButtons(std::vector<AbstractDialog::ButtonConfig> buttons)
{
    for (auto& button: pimpl->buttons)
    {
        destroyWidget(button.second);
    }
    pimpl->buttons.clear();

    destroyWidget(pimpl->buttonsFrame);

    const auto& buttonsStyle=pimpl->buttonsStyle();
    pimpl->buttonsFrame=new QFrame(this);
    pimpl->buttonsFrame->setObjectName("dialogButtonsFrame");
    pimpl->buttonLayout=Layout::horizontal(pimpl->buttonsFrame);
    pimpl->layout->addWidget(pimpl->buttonsFrame,0,buttonsStyle.alignment);
    pimpl->buttonsFrame->setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);

    for (const auto& button: buttons)
    {
        auto bt=new PushButton(button.text,button.icon,pimpl->buttonsFrame);
        bt->setObjectName(button.name);
        pimpl->buttonLayout->addWidget(bt,0,buttonsStyle.alignment);
        pimpl->buttonGroup->setMapping(bt,button.id);
        QObject::connect(
            bt,
            SIGNAL(clicked()),
            pimpl->buttonGroup,
            SLOT(map())
        );
        pimpl->buttons.emplace(button.id,bt);
    }
}

//--------------------------------------------------------------------------

template <typename BaseT>
void Dialog<BaseT>::doActivateButton(int id)
{
    auto it=pimpl->buttons.find(id);
    if (it!=pimpl->buttons.end())
    {
        it->second->click();
    }
}

//--------------------------------------------------------------------------

template <typename BaseT>
void Dialog<BaseT>::setTitle(const QString& title)
{
    pimpl->title->setText(title);
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_DIALOG_IPP

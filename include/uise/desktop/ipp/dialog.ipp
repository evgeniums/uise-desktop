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
        QLabel* placeHolder=nullptr;
        QPointer<QWidget> titleControl;

        QFrame* contentFrame;
        QBoxLayout* contentLayout;

        QFrame* buttonsFrame;
        QBoxLayout* buttonLayout;

        QSignalMapper* buttonGroup;

        std::map<int,PushButton*> buttons;

        QFrame* dialogFrame;
        PushButton* icon;
        QBoxLayout* dialogLayout;

        std::optional<ButtonsStyle> forceButtonsStyle;

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
    pimpl->titleLayout=tl;
    pimpl->layout->addWidget(pimpl->titleFrame);
    pimpl->titleClose=new PushButton(Style::instance().svgIconLocator().icon("DialogTitle::close",this),pimpl->titleFrame);
    pimpl->titleClose->setToolTip(AbstractDialog::tr("Close","dialog"));
    pimpl->title=new QLabel(pimpl->titleFrame);
    pimpl->title->setAlignment(Qt::AlignCenter);
    pimpl->titleFrame->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
#ifdef Q_OS_MACOS
    tl->addWidget(pimpl->titleClose,0);
    tl->addWidget(pimpl->title,1);
    pimpl->placeHolder=new QLabel();
    pimpl->placeHolder->setObjectName("placeHolder");
    tl->addWidget(pimpl->placeHolder);
#else
    tl->addWidget(pimpl->title,1);
    tl->addWidget(pimpl->titleClose);
#endif

    pimpl->contentFrame=new QFrame(this);
    pimpl->contentFrame->setObjectName("contentFrame");
    pimpl->contentLayout=Layout::horizontal(pimpl->contentFrame);
    pimpl->layout->addWidget(pimpl->contentFrame);

    pimpl->icon=new PushButton(this);
    pimpl->contentLayout->addWidget(pimpl->icon);
    pimpl->icon->setObjectName("dialogIcon");
    pimpl->icon->setVisible(false);

    pimpl->dialogFrame=new QFrame(this);
    pimpl->dialogFrame->setObjectName("dialogFrame");
    pimpl->dialogLayout=Layout::vertical(pimpl->dialogFrame);
    pimpl->contentLayout->addWidget(pimpl->dialogFrame,1);

    pimpl->buttonGroup=new QSignalMapper(this);
    QObject::connect(
        pimpl->buttonGroup,
        &QSignalMapper::mappedInt,
        this,
        [this](int id)
        {
            emit AbstractDialog::buttonClicked(id);
            if (AbstractDialog::isButton(id,AbstractDialog::StandardButton::Close) || AbstractDialog::isButton(id,AbstractDialog::StandardButton::Cancel))
            {
                this->closeDialog();
            }
        }
    );

    QObject::connect(
        pimpl->titleClose,
        &PushButton::clicked,
        this,
        [this]()
        {
            this->closeDialog();
        }
    );

    doSetButtons({AbstractDialog::standardButton(AbstractDialog::StandardButton::Close,this)});

    this->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Fixed);
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
    pimpl->dialogLayout->addWidget(widget);
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

    const auto* buttonsStyle=&pimpl->buttonsStyle();

    if (pimpl->forceButtonsStyle)
    {
        buttonsStyle=&pimpl->forceButtonsStyle.value();
    }

    pimpl->buttonsFrame=new QFrame(this);
    pimpl->buttonsFrame->setObjectName("dialogButtonsFrame");
    pimpl->buttonLayout=Layout::horizontal(pimpl->buttonsFrame);
    pimpl->layout->addWidget(pimpl->buttonsFrame,0,buttonsStyle->alignment);
    pimpl->buttonsFrame->setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);

    for (const auto& button: buttons)
    {
        auto bt=new PushButton(button.text,button.icon,pimpl->buttonsFrame);
        bt->setObjectName(button.name);
        pimpl->buttonLayout->addWidget(bt,0,buttonsStyle->alignment | Qt::AlignBottom);
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
void Dialog<BaseT>::doSetButtonEnabled(int id, bool enable)
{
    auto it=pimpl->buttons.find(id);
    if (it!=pimpl->buttons.end())
    {
        it->second->setEnabled(enable);
    }
}

//--------------------------------------------------------------------------

template <typename BaseT>
void Dialog<BaseT>::doSetButtonVisible(int id, bool enable)
{
    auto it=pimpl->buttons.find(id);
    if (it!=pimpl->buttons.end())
    {
        it->second->setVisible(enable);
    }
}

//--------------------------------------------------------------------------

template <typename BaseT>
void Dialog<BaseT>::setTitle(const QString& title)
{
    pimpl->title->setText(title);
}

//--------------------------------------------------------------------------

template <typename BaseT>
void Dialog<BaseT>::setClosable(bool enable)
{
    pimpl->titleClose->setVisible(enable);
}

//--------------------------------------------------------------------------

template <typename BaseT>
void Dialog<BaseT>::setButtonsStyle(ButtonsStyle style)
{
    pimpl->forceButtonsStyle=std::move(style);
}

//--------------------------------------------------------------------------

template <typename BaseT>
void Dialog<BaseT>::resetButtonsStyle()
{
    pimpl->forceButtonsStyle.reset();
}

//--------------------------------------------------------------------------

template <typename BaseT>
void Dialog<BaseT>::setSvgIcon(std::shared_ptr<SvgIcon> icon)
{
    pimpl->icon->setVisible(static_cast<bool>(icon));
    pimpl->icon->setSvgIcon(std::move(icon));
}

//--------------------------------------------------------------------------

template <typename BaseT>
std::shared_ptr<SvgIcon> Dialog<BaseT>::svgIcon() const
{
    return pimpl->icon->svgIcon();
}

//--------------------------------------------------------------------------

template <typename BaseT>
void Dialog<BaseT>::setTitleControl(QWidget* widget)
{
    if (!pimpl->titleControl.isNull())
    {
        destroyWidget(pimpl->titleControl);
    }
    pimpl->titleControl=widget;

#ifdef Q_OS_MACOS
    // placeHolder exists solely to balance titleClose's width on the other side so the title
    // stays visually centered (see the ctor); a real title control sits at the same trailing
    // position and is close enough in size to serve that same balancing role on its own, so
    // the placeholder would otherwise just be dead space stacked after it, pushing it further
    // from the actual right edge than necessary. QBoxLayout gives a hidden widget zero space
    // (see doUpdateListAreaHeight()'s header-height comment for the same rule elsewhere in
    // this library), so hiding it -- rather than removing it from the layout -- is enough.
    pimpl->placeHolder->setVisible(widget==nullptr);
#endif

    if (widget==nullptr)
    {
        return;
    }

    // inserted just before the layout's last item -- titleClose on most platforms, the
    // (now hidden) placeHolder on macOS -- so it always lands right after the title text,
    // flush against the trailing edge on either layout
    widget->setParent(pimpl->titleFrame);
    pimpl->titleLayout->insertWidget(pimpl->titleLayout->count()-1,widget);
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_DIALOG_IPP

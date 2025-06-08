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

/** @file uise/desktop/src/statusdialog.cpp
*
*  Defines StatusDialog.
*
*/

/****************************************************************************/

#include <QPointer>
#include <QLabel>
#include <QSignalMapper>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/utils/destroywidget.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/pushbutton.hpp>
#include <uise/desktop/statusdialog.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/**************************** StatusDialog ***********************************/

//--------------------------------------------------------------------------

class StatusDialog_p
{
    public:

        StatusDialog* widget;

        QBoxLayout* layout;

        QFrame* titleFrame;
        QBoxLayout* titleLayout;
        QLabel* title;
        PushButton* titleClose;

        QFrame* statusFrame;
        PushButton* icon;
        QLabel* text;

        QFrame* buttonsFrame;
        QBoxLayout* buttonLayout;

        QSignalMapper* buttonGroup;

        std::map<int,PushButton*> buttons;

        const auto& buttonsStyle() const
        {
            return Style::instance().buttonsStyle("StatusDialog",widget);
        }
};

//--------------------------------------------------------------------------

StatusDialog::ButtonConfig StatusDialog::standardButton(StandardButton button, QWidget* parent)
{
    const auto& buttonsStyle=Style::instance().buttonsStyle("StatusDialog",parent);
    auto make=[parent,&buttonsStyle](auto id, QString text, QString iconName)
    {
        std::shared_ptr<SvgIcon> icon;
        QString txt;
        if (buttonsStyle.showIcon)
        {
            icon=Style::instance().svgIconLocator().icon("StatusDialog::"+iconName,parent);
        }
        if (buttonsStyle.showText)
        {
            txt=std::move(text);
        }
        auto btn=ButtonConfig{static_cast<int>(id),std::move(txt),std::move(icon)};
        btn.name=iconName;
        return btn;
    };

    switch (button)
    {
        case(StandardButton::Close):
        {
            return make(button,tr("Close"),"close");
        }
        break;
        case(StandardButton::Cancel):
        {
            return make(button,tr("Cancel"),"cancel");
        }
        break;
        case(StandardButton::Accept):
        {
            return make(button,tr("Accept"),"accept");
        }
        break;
        case(StandardButton::Ignore):
        {
            return make(button,tr("Ignore"),"ignore");
        }
        break;
        case(StandardButton::OK):
        {
            return make(button,tr("OK"),"ok");
        }
        break;
        case(StandardButton::Yes):
        {
            return make(button,tr("Yes"),"yes");
        }
        break;
        case(StandardButton::No):
        {
            return make(button,tr("No"),"no");
        }
        break;
        case(StandardButton::Skip):
        {
            return make(button,tr("Skip"),"skip");
        }
        break;
        case(StandardButton::Retry):
        {
            return make(button,tr("Retry"),"retry");
        }
        break;
    }

    return ButtonConfig{-1,tr("Unknown")};
}

//--------------------------------------------------------------------------

StatusDialog::StatusDialog(QWidget* parent)
    : QFrame(parent),
      pimpl(std::make_unique<StatusDialog_p>())
{
    pimpl->widget=this;

    pimpl->layout=Layout::vertical(this);

    pimpl->titleFrame=new QFrame(this);
    pimpl->titleFrame->setObjectName("titleFrame");
    auto tl=Layout::horizontal(pimpl->titleFrame);
    pimpl->layout->addWidget(pimpl->titleFrame);
    pimpl->titleClose=new PushButton(Style::instance().svgIconLocator().icon("StatusDialogTitle::close",this),pimpl->titleFrame);
    pimpl->titleClose->setToolTip(tr("Close"));
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

    pimpl->statusFrame=new QFrame(this);
    pimpl->statusFrame->setObjectName("statusFrame");
    auto sl=Layout::horizontal(pimpl->statusFrame);
    pimpl->layout->addWidget(pimpl->statusFrame);

    pimpl->icon=new PushButton(this);
    sl->addWidget(pimpl->icon);
    pimpl->icon->setVisible(false);
    pimpl->text=new QLabel(this);
    sl->addWidget(pimpl->text,1);
    pimpl->text->setTextInteractionFlags(Qt::TextBrowserInteraction);
    pimpl->text->setTextFormat(Qt::RichText);
    pimpl->text->setWordWrap(true);

    pimpl->buttonGroup=new QSignalMapper(this);
    connect(
        pimpl->buttonGroup,
        &QSignalMapper::mappedInt,
        this,
        [this](int id)
        {
            emit buttonClicked(id);
            if (id==static_cast<int>(StandardButton::Close))
            {
                emit closeRequested();
            }
        }
    );

    connect(
        pimpl->titleClose,
        &PushButton::clicked,
        this,
        [this]()
        {
            emit closeRequested();
        }
    );

    setButtons({standardButton(StandardButton::Close,this)});
}

//--------------------------------------------------------------------------

StatusDialog::~StatusDialog()
{
}

//--------------------------------------------------------------------------

QLabel* StatusDialog::textWidget() const
{
    return pimpl->text;
}

//--------------------------------------------------------------------------

void StatusDialog::setStatus(const QString& message, const QString& title, std::shared_ptr<SvgIcon> icon)
{
    pimpl->text->setText(message);
    if (icon)
    {
        pimpl->icon->setVisible(true);
    }
    pimpl->icon->setSvgIcon(std::move(icon));
    pimpl->title->setText(title);
}

//--------------------------------------------------------------------------

void StatusDialog::setStatus(const QString& message, Type type, const QString& title)
{
    auto titleText=title;

    switch (type)
    {
        case(Type::Error):
        {
            if (titleText.isEmpty())
            {
                titleText=tr("Error");
            }

            setStatus(message,titleText,Style::instance().svgIconLocator().icon("StatusDialog::error",this));
        }
        break;

        case(Type::Warning):
        {
            if (titleText.isEmpty())
            {
                titleText=tr("Warning");
            }

            setStatus(message,titleText,Style::instance().svgIconLocator().icon("StatusDialog::warning",this));
        }
        break;

        case(Type::Info):
        {
            if (titleText.isEmpty())
            {
                titleText=tr("Information");
            }
            setStatus(message,titleText,Style::instance().svgIconLocator().icon("StatusDialog::info",this));
        }
        break;

        case(Type::Question):
        {
            if (titleText.isEmpty())
            {
                titleText=tr("Question");
            }
            setStatus(message,titleText,Style::instance().svgIconLocator().icon("StatusDialog::question",this));
        }
        break;

        default:
        {
            setStatus(message,tr("Notice"));
        }
        break;
    }
}

//--------------------------------------------------------------------------

void StatusDialog::setButtons(std::vector<ButtonConfig> buttons)
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
        connect(
            bt,
            SIGNAL(clicked()),
            pimpl->buttonGroup,
            SLOT(map())
        );
        pimpl->buttons.emplace(button.id,bt);
    }
}

//--------------------------------------------------------------------------

void StatusDialog::activateButton(int id)
{
    auto it=pimpl->buttons.find(id);
    if (it!=pimpl->buttons.end())
    {
        it->second->click();
    }
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END

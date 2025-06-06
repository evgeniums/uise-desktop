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
};

//--------------------------------------------------------------------------

StatusDialog::ButtonConfig StatusDialog::standardButton(StandardButton button, QWidget* parent)
{
    auto make=[parent](auto id, QString text, QString iconName)
    {
        return ButtonConfig{static_cast<int>(id),std::move(text),Style::instance().svgIconLocator().icon("StatusDialog::"+iconName,parent)};
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
    }

    return ButtonConfig{-1,tr("Unknown")};
}

//--------------------------------------------------------------------------

StatusDialog::StatusDialog(QWidget* parent)
    : QFrame(parent),
      pimpl(std::make_unique<StatusDialog_p>())
{
    auto l=Layout::vertical(this);

    pimpl->titleFrame=new QFrame(this);
    pimpl->titleFrame->setObjectName("titleFrame");
    auto tl=Layout::horizontal(pimpl->titleFrame);
    l->addWidget(pimpl->titleFrame);
    pimpl->titleClose=new PushButton(Style::instance().svgIconLocator().icon("StatusDialog::close",this),pimpl->titleFrame);
    pimpl->titleClose->setToolTip(tr("Close"));
    pimpl->title=new QLabel(pimpl->titleFrame);
    pimpl->title->setAlignment(Qt::AlignCenter);
    pimpl->title->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Preferred);
#ifdef Q_OS_MACOS
    tl->addWidget(pimpl->titleClose,0);
    tl->addWidget(pimpl->title,1);
#else
    tl->addWidget(pimpl->title,1);
    tl->addWidget(pimpl->titleClose);
#endif

    pimpl->statusFrame=new QFrame(this);
    pimpl->statusFrame->setObjectName("statusFrame");
    auto sl=Layout::horizontal(pimpl->statusFrame);
    l->addWidget(pimpl->statusFrame);

    pimpl->icon=new PushButton(this);
    sl->addWidget(pimpl->icon);
    pimpl->icon->setVisible(false);
    pimpl->text=new QLabel(this);
    sl->addWidget(pimpl->text);
    pimpl->text->setTextInteractionFlags(Qt::TextBrowserInteraction);
    pimpl->text->setTextFormat(Qt::RichText);
    pimpl->text->setWordWrap(true);

    pimpl->buttonsFrame=new QFrame(this);
    pimpl->buttonsFrame->setObjectName("buttonsFrame");
    pimpl->buttonLayout=Layout::horizontal(pimpl->buttonsFrame);

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

void StatusDialog::setStatus(const QString& message, std::shared_ptr<SvgIcon> icon)
{
    pimpl->text->setText(message);
    if (icon)
    {
        pimpl->icon->setVisible(true);
    }
    pimpl->icon->setSvgIcon(std::move(icon));
}

//--------------------------------------------------------------------------

void StatusDialog::setStatus(const QString& message, Type type)
{
    switch (type)
    {
        case(Type::Error):
        {
            setStatus(message,Style::instance().svgIconLocator().icon("StatusDialog::error",this));
        }
        break;

        case(Type::Warning):
        {
            setStatus(message,Style::instance().svgIconLocator().icon("StatusDialog::warning",this));
        }
        break;

        case(Type::Info):
        {
            setStatus(message,Style::instance().svgIconLocator().icon("StatusDialog::info",this));
        }
        break;

        case(Type::Question):
        {
            setStatus(message,Style::instance().svgIconLocator().icon("StatusDialog::question",this));
        }
        break;

        default:
        {
            setStatus(message);
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

    auto alignment=Style::instance().dialogButtonsAlignment(this);
    for (const auto& button: buttons)
    {
        auto bt=new PushButton(button.text,button.icon,pimpl->buttonsFrame);
        pimpl->buttonLayout->addWidget(bt,0,alignment);
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

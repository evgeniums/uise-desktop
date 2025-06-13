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

#include <uise/desktop/ipp/dialog.ipp>

UISE_DESKTOP_NAMESPACE_BEGIN

/**************************** StatusDialog ***********************************/

//--------------------------------------------------------------------------

class StatusDialog_p
{
    public:

        QFrame* statusFrame;
        PushButton* icon;
        QLabel* text;
};

//--------------------------------------------------------------------------

StatusDialog::StatusDialog(QWidget* parent)
    : Base(parent),
      pimpl(std::make_unique<StatusDialog_p>())
{
    pimpl->statusFrame=new QFrame(this);
    pimpl->statusFrame->setObjectName("statusFrame");
    auto sl=Layout::horizontal(pimpl->statusFrame);

    pimpl->icon=new PushButton(this);
    sl->addWidget(pimpl->icon);
    pimpl->icon->setVisible(false);
    pimpl->text=new QLabel(this);
    sl->addWidget(pimpl->text,1);
    pimpl->text->setTextInteractionFlags(Qt::TextBrowserInteraction);
    pimpl->text->setTextFormat(Qt::RichText);
    pimpl->text->setWordWrap(true);

    setWidget(pimpl->statusFrame);
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
    setTitle(title);
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

UISE_DESKTOP_NAMESPACE_END

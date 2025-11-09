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

/**************************** Status ***********************************/

//--------------------------------------------------------------------------

QString StatusBase::statusString(int status)
{
    switch (status)
    {
        case (static_cast<int>(Type::Error)): return "error"; break;
        case (static_cast<int>(Type::Warning)): return "warning"; break;
        case (static_cast<int>(Type::Info)): return "info"; break;
        case (static_cast<int>(Type::Question)): return "question"; break;
        case (static_cast<int>(Type::Attention)): return "attention"; break;
        case (static_cast<int>(Type::None)): return ""; break;
        default: break;
    }

    return QString{};
}

//--------------------------------------------------------------------------

QString StatusBase::statusTitle(int status)
{
    switch (status)
    {
        case (static_cast<int>(Type::Error)): return QObject::tr("Error","Status"); break;
        case (static_cast<int>(Type::Warning)): return QObject::tr("Warning","Status"); break;
        case (static_cast<int>(Type::Info)): return QObject::tr("Information","Status"); break;
        case (static_cast<int>(Type::Question)): return QObject::tr("Question","Status"); break;
        case (static_cast<int>(Type::Attention)): return QObject::tr("Attention","Status"); break;
        case (static_cast<int>(Type::None)): return ""; break;
        default: break;
    }

        return QString{};
}

//--------------------------------------------------------------------------

QString Status::statusString(int status) const
{
    auto str=StatusBase::statusString(status);
    if (!str.isEmpty())
    {
        return str;
    }
    return customStatusString(status);
}

//--------------------------------------------------------------------------

QString Status::statusTitle(int status) const
{
    auto str=StatusBase::statusTitle(status);
    if (!str.isEmpty())
    {
        return str;
    }
    return customStatusTitle(status);
}

/**************************** StatusDialog ***********************************/

//--------------------------------------------------------------------------

class StatusDialog_p
{
    public:

        QLabel* text;
};

//--------------------------------------------------------------------------

StatusDialog::StatusDialog(QWidget* parent)
    : Base(parent),
      pimpl(std::make_unique<StatusDialog_p>())
{
    pimpl->text=new QLabel(this);
    pimpl->text->setTextInteractionFlags(Qt::TextBrowserInteraction);
    pimpl->text->setTextFormat(Qt::RichText);
    pimpl->text->setWordWrap(true);
    pimpl->text->setObjectName("text");

    setWidget(pimpl->text);
    setMinimumWidth(400);
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
    setSvgIcon(std::move(icon));
    setTitle(title);
}

//--------------------------------------------------------------------------

void StatusDialog::setStatus(const QString& message, Type type, const QString& title)
{
    auto titleText=title;
    pimpl->text->setProperty("status",statusString(type));

    switch (type)
    {
        case(Type::Error):
        {
            if (titleText.isEmpty())
            {
                titleText=tr("Error");
            }

            setButtons(
                {
                    AbstractDialog::standardButton(AbstractDialog::StandardButton::Close,this)
                }
            );
            setStatus(message,titleText,Style::instance().svgIconLocator().icon("StatusDialog::error",this));
        }
        break;

        case(Type::Warning):
        {
            if (titleText.isEmpty())
            {
                titleText=tr("Warning");
            }

            setButtons(
                {
                    AbstractDialog::standardButton(AbstractDialog::StandardButton::Close,this)
                }
            );
            setStatus(message,titleText,Style::instance().svgIconLocator().icon("StatusDialog::warning",this));
        }
        break;

        case(Type::Info):
        {
            if (titleText.isEmpty())
            {
                titleText=tr("Information");
            }

            setButtons(
                {
                    AbstractDialog::standardButton(AbstractDialog::StandardButton::Close,this)
                }
            );
            setStatus(message,titleText,Style::instance().svgIconLocator().icon("StatusDialog::info",this));
        }
        break;

        case(Type::Question):
        {
            if (titleText.isEmpty())
            {
                titleText=tr("Question");
            }
            setButtons(
                {
                    AbstractDialog::standardButton(AbstractDialog::StandardButton::Yes,this),
                    AbstractDialog::standardButton(AbstractDialog::StandardButton::Cancel,this)
                }
            );
            setStatus(message,titleText,Style::instance().svgIconLocator().icon("StatusDialog::question",this));
        }
        break;

        case(Type::Attention):
        {
            if (titleText.isEmpty())
            {
                titleText=tr("Attention");
            }
            setButtons(
                {
                    AbstractDialog::standardButton(AbstractDialog::StandardButton::Yes,this),
                    AbstractDialog::standardButton(AbstractDialog::StandardButton::Cancel,this)
                }
            );
            setStatus(message,titleText,Style::instance().svgIconLocator().icon("StatusDialog::attention",this));
        }
        break;

        default:
        {
            setButtons(
                {
                    AbstractDialog::standardButton(AbstractDialog::StandardButton::Close,this)
                }
            );
            setStatus(message,tr("Notice"));
        }
        break;
    }

    Style::updateWidgetStyle(pimpl->text);
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END

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
#include <QFrame>
#include <QSignalMapper>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/utils/destroywidget.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/pushbutton.hpp>
#include <uise/desktop/checkbox.hpp>
#include <uise/desktop/label.hpp>
#include <uise/desktop/statusdialog.hpp>

#include <uise/desktop/ipp/dialog.ipp>

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

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

        Label* text;
        CheckBox* checkBox;
        QFrame* checkBoxRow;
};

//--------------------------------------------------------------------------

StatusDialog::StatusDialog(QWidget* parent)
    : Base(parent),
      pimpl(std::make_unique<StatusDialog_p>())
{
    // Container holds the message text and an optional "Don't ask again" checkbox.
    auto* container = new QFrame(this);
    auto* cl = Layout::vertical(container);

    pimpl->text=new Label(container);
    // Label::init() forces Qt::PlainText -- override it back to RichText since status messages
    // can contain markup (e.g. characternode.cpp's remove-identity confirmation uses <br><br>).
    pimpl->text->setTextFormat(Qt::RichText);
    pimpl->text->setWordWrap(true);
    pimpl->text->setObjectName("text");
    cl->addWidget(pimpl->text);

    pimpl->checkBox=new CheckBox(container);
    pimpl->checkBox->setObjectName("optionCheckBox");
    pimpl->checkBox->setVisible(false);
    // Center the checkbox horizontally via a wrapper row.
    pimpl->checkBoxRow = new QFrame(container);
    pimpl->checkBoxRow->setObjectName("checkBoxRow");
    pimpl->checkBoxRow->setVisible(false);
    auto* rl = Layout::horizontal(pimpl->checkBoxRow);
    rl->addWidget(pimpl->checkBox);
    cl->addWidget(pimpl->checkBoxRow);

    setWidget(container);
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

void StatusDialog::setOptionCheckBox(const QString& text)
{
    pimpl->checkBox->setText(text);
    pimpl->checkBox->setChecked(false);
    pimpl->checkBox->setVisible(true);
    pimpl->checkBoxRow->setVisible(true);
}

//--------------------------------------------------------------------------

bool StatusDialog::isOptionChecked() const
{
    return pimpl->checkBoxRow->isVisible() && pimpl->checkBox->isChecked();
}

//--------------------------------------------------------------------------

void StatusDialog::clearOptionCheckBox()
{
    pimpl->checkBoxRow->setVisible(false);
    pimpl->checkBox->setVisible(false);
    pimpl->checkBox->setChecked(false);
}

//--------------------------------------------------------------------------

void StatusDialog::setStatus(const QString& message, const QString& title, std::shared_ptr<SvgIcon> icon)
{
    // One dialog instance is reused for every status its frame shows (see
    // FrameWithModalStatus), so a caller that made one status non-closable must not leave the
    // next one stuck that way -- same reasoning as the clearOptionCheckBox() call in the
    // Type overload.
    setClosable(true);
    pimpl->text->setText(message);
    setSvgIcon(std::move(icon));
    setTitle(title);
}

//--------------------------------------------------------------------------

void StatusDialog::setStatus(const QString& message, Type type, const QString& title)
{
    // Always start with the checkbox hidden so callers that don't use the
    // "Don't ask anymore" option never see a stray checkbox.
    clearOptionCheckBox();

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

}

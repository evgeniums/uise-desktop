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

/** @file uise/desktop/src/abstractdialog.cpp
*
*  Defines AbstractDialog.
*
*/

/****************************************************************************/

#include <QPointer>
#include <QSignalMapper>

#include <uise/desktop/style.hpp>
#include <uise/desktop/abstractdialog.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/**************************** AbstractDialog ***********************************/

//--------------------------------------------------------------------------

AbstractDialog::ButtonConfig AbstractDialog::standardButton(StandardButton button, QWidget* parent)
{
    const auto& buttonsStyle=Style::instance().buttonsStyle("AbstractDialog",parent);
    auto make=[parent,&buttonsStyle](auto id, QString text, QString iconName)
    {
        std::shared_ptr<SvgIcon> icon;
        QString txt;
        if (buttonsStyle.showIcon)
        {
            icon=Style::instance().svgIconLocator().icon("AbstractDialog::"+iconName,parent);
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
        case(StandardButton::Apply):
        {
            return make(button,tr("Apply"),"apply");
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
        case(StandardButton::Next):
        {
            return make(button,tr("Next"),"mext");
        }
        break;
        case(StandardButton::Back):
        {
            return make(button,tr("Back"),"back");
        }
        break;
        case(StandardButton::Start):
        {
            return make(button,tr("Start"),"start");
        }
        break;
        case(StandardButton::Finish):
        {
            return make(button,tr("Finish"),"finish");
        }
        break;
        case(StandardButton::Complete):
        {
            return make(button,tr("Complete"),"complete");
        }
        break;
        case(StandardButton::Done):
        {
            return make(button,tr("Done"),"done");
        }
        break;
    }

    return ButtonConfig{-1,tr("Unknown")};
}

//--------------------------------------------------------------------------

void AbstractDialog::activateButton(int id)
{
    doActivateButton(id);
}

//--------------------------------------------------------------------------

void AbstractDialog::setButtonEnabled(int id, bool enable)
{
    doSetButtonEnabled(id,enable);
}

//--------------------------------------------------------------------------

void AbstractDialog::setButtonVisible(int id, bool enable)
{
    doSetButtonVisible(id,enable);
}

//--------------------------------------------------------------------------

void AbstractDialog::closeDialog()
{
    emit closeRequested();
}

//--------------------------------------------------------------------------

void AbstractDialog::setButtonsStyle(ButtonsStyle style)
{
    m_forceButtonsStyle=std::move(style);
    scheduleButtonsLayoutUpdate();
}

//--------------------------------------------------------------------------

void AbstractDialog::resetButtonsStyle()
{
    m_forceButtonsStyle.reset();
    scheduleButtonsLayoutUpdate();
}

//--------------------------------------------------------------------------

ButtonsStyle AbstractDialog::effectiveButtonsStyle() const
{
    // 1) global default / per-context entry
    ButtonsStyle style=Style::instance().buttonsStyle(buttonsStyleContext(),this);
    // 2) whole-struct per-dialog override
    if (m_forceButtonsStyle)
    {
        style=m_forceButtonsStyle.value();
    }
    // 3) per-axis overrides (typed C++ setters AND QSS qproperty- share this storage)
    if (m_buttonsOrientation)
    {
        style.orientation=m_buttonsOrientation.value();
    }
    if (m_buttonsAlignment)
    {
        style.alignment=m_buttonsAlignment.value();
    }
    return style;
}

//--------------------------------------------------------------------------

void AbstractDialog::setButtonsOrientation(Qt::Orientation orientation)
{
    if (m_buttonsOrientation && m_buttonsOrientation.value()==orientation)
    {
        return;
    }
    m_buttonsOrientation=orientation;
    scheduleButtonsLayoutUpdate();
}

//--------------------------------------------------------------------------

void AbstractDialog::resetButtonsOrientation()
{
    if (!m_buttonsOrientation)
    {
        return;
    }
    m_buttonsOrientation.reset();
    scheduleButtonsLayoutUpdate();
}

//--------------------------------------------------------------------------

Qt::Orientation AbstractDialog::buttonsOrientation() const
{
    return effectiveButtonsStyle().orientation;
}

//--------------------------------------------------------------------------

void AbstractDialog::setButtonsAlignment(Qt::Alignment alignment)
{
    if (m_buttonsAlignment && m_buttonsAlignment.value()==alignment)
    {
        return;
    }
    m_buttonsAlignment=alignment;
    scheduleButtonsLayoutUpdate();
}

//--------------------------------------------------------------------------

void AbstractDialog::resetButtonsAlignment()
{
    if (!m_buttonsAlignment)
    {
        return;
    }
    m_buttonsAlignment.reset();
    scheduleButtonsLayoutUpdate();
}

//--------------------------------------------------------------------------

Qt::Alignment AbstractDialog::buttonsAlignment() const
{
    return effectiveButtonsStyle().alignment;
}

//--------------------------------------------------------------------------

void AbstractDialog::setButtonsOrientationName(const QString& name)
{
    if (isDefaultStyleToken(name))
    {
        resetButtonsOrientation();
        return;
    }

    bool ok=false;
    auto orientation=orientationFromString(name,&ok);
    if (!ok)
    {
        // orientationFromString() already warned; keep the previous value rather than
        // silently resetting to the default on a typo
        return;
    }
    setButtonsOrientation(orientation);
}

//--------------------------------------------------------------------------

QString AbstractDialog::buttonsOrientationName() const
{
    return orientationToString(buttonsOrientation());
}

//--------------------------------------------------------------------------

void AbstractDialog::setButtonsAlignmentName(const QString& name)
{
    if (isDefaultStyleToken(name))
    {
        resetButtonsAlignment();
        return;
    }

    bool ok=false;
    auto alignment=alignmentFromString(name,&ok);
    if (!ok && alignment==Qt::Alignment{})
    {
        // every token failed to parse -- nothing usable, keep the previous value
        return;
    }
    setButtonsAlignment(alignment);
}

//--------------------------------------------------------------------------

QString AbstractDialog::buttonsAlignmentName() const
{
    return alignmentToString(buttonsAlignment());
}

//--------------------------------------------------------------------------

void AbstractDialog::scheduleButtonsLayoutUpdate()
{
    if (m_buttonsLayoutUpdateScheduled)
    {
        return;
    }
    m_buttonsLayoutUpdateScheduled=true;

    QPointer<AbstractDialog> guard(this);
    QMetaObject::invokeMethod(
        this,
        [guard]()
        {
            if (guard.isNull())
            {
                return;
            }
            // cleared BEFORE the update so a legitimate later request is not swallowed;
            // updateButtonsLayout() itself must never call back into here synchronously
            guard->m_buttonsLayoutUpdateScheduled=false;
            guard->updateButtonsLayout();
        },
        Qt::QueuedConnection
    );
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END

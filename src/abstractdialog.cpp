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

void AbstractDialog::activateButton(int id)
{
    doActivateButton(id);
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END

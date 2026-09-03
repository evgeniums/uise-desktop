/**
@copyright Evgeny Sidorov 2026

This software is dual-licensed. Choose the appropriate license for your project.

1. The GNU GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-GPLv3.md](LICENSE-GPLv3.md) or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. The GNU LESSER GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-LGPLv3.md](LICENSE-LGPLv3.md) or copy at https://www.gnu.org/licenses/lgpl-3.0.txt).

You may select, at your option, one of the above-listed licenses.

*/

/****************************************************************************/

/** @file uise/desktop/src/abstractforwarddialog.cpp
*
*  Defines AbstractForwardDialog.
*
*/

/****************************************************************************/

#include <uise/desktop/style.hpp>
#include <uise/desktop/abstractforwarddialog.hpp>

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

/**************************** ForwardDialogActionConfig ***********************************/

//--------------------------------------------------------------------------

ForwardDialogActionConfig::ForwardDialogActionConfig(ForwardDialogAction action, QWidget* parent)
    : ForwardDialogActionConfig(AbstractForwardDialog::standardAction(action,parent))
{}

/**************************** AbstractForwardDialog ***********************************/

//--------------------------------------------------------------------------

ForwardDialogActionConfig AbstractForwardDialog::standardAction(ForwardDialogAction action, QWidget* parent)
{
    switch (action)
    {
        case (ForwardDialogAction::ChangeRecipient):
        {
            return ForwardDialogActionConfig{
                static_cast<int>(action),
                tr("Change recipient"),
                Style::instance().svgIconLocator().icon("ForwardDialog::changeRecipient",parent)
            };
        }
        break;

        case (ForwardDialogAction::ShowInChat):
        {
            return ForwardDialogActionConfig{
                static_cast<int>(action),
                tr("Show in chat"),
                Style::instance().svgIconLocator().icon("ForwardDialog::showInChat",parent)
            };
        }
        break;

        case (ForwardDialogAction::DoNotForward):
        {
            // Its own icon-color context (ForwardDialogDanger), not ForwardDialog::* like the
            // other actions -- a context's colour modes apply uniformly to every alias under
            // it, so a destructive action needing a different (red) palette from its siblings
            // needs a context of its own, see forwardpreview.json.
            return ForwardDialogActionConfig{
                static_cast<int>(action),
                tr("Do not forward"),
                Style::instance().svgIconLocator().icon("ForwardDialogDanger::doNotForward",parent)
            };
        }
        break;
    }

    return ForwardDialogActionConfig{static_cast<int>(action),QString{}};
}

//--------------------------------------------------------------------------

}

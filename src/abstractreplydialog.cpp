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

/** @file uise/desktop/src/abstractreplydialog.cpp
*
*  Defines AbstractReplyDialog.
*
*/

/****************************************************************************/

#include <uise/desktop/style.hpp>
#include <uise/desktop/abstractreplydialog.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/**************************** ReplyDialogActionConfig ***********************************/

//--------------------------------------------------------------------------

ReplyDialogActionConfig::ReplyDialogActionConfig(ReplyDialogAction action, QWidget* parent)
    : ReplyDialogActionConfig(AbstractReplyDialog::standardAction(action,parent))
{}

/**************************** AbstractReplyDialog ***********************************/

//--------------------------------------------------------------------------

ReplyDialogActionConfig AbstractReplyDialog::standardAction(ReplyDialogAction action, QWidget* parent)
{
    switch (action)
    {
        case (ReplyDialogAction::ShowInChat):
        {
            return ReplyDialogActionConfig{
                static_cast<int>(action),
                tr("Show in chat"),
                Style::instance().svgIconLocator().icon("ReplyDialog::showInChat",parent)
            };
        }
        break;

        case (ReplyDialogAction::DoNotReply):
        {
            // Its own icon-color context (ReplyDialogDanger), not ReplyDialog::* like the
            // other actions -- a context's colour modes apply uniformly to every alias under
            // it, so a destructive action needing a different (red) palette from its siblings
            // needs a context of its own, see replypreview.json.
            return ReplyDialogActionConfig{
                static_cast<int>(action),
                tr("Do not reply"),
                Style::instance().svgIconLocator().icon("ReplyDialogDanger::doNotReply",parent)
            };
        }
        break;
    }

    return ReplyDialogActionConfig{static_cast<int>(action),QString{}};
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END

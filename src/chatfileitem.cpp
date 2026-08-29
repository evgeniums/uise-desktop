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

/** @file uise/desktop/src/chatfileitem.cpp
*
*  Defines ChatFileItem.
*
*/

/****************************************************************************/

#include <QFileInfo>
#include <QMimeDatabase>
#include <QCoreApplication>

#include <uise/desktop/style.hpp>
#include <uise/desktop/chatfileitem.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

namespace {

std::shared_ptr<SvgIcon> chatFileMenuIcon(const QString& alias, QWidget* context)
{
    return Style::instance().svgIconLocator().icon(QString("ChatMessageFiles::%1").arg(alias),context);
}

}

//--------------------------------------------------------------------------

QString ChatFileItem::suffix() const
{
    return QFileInfo(m_fileName).suffix();
}

//--------------------------------------------------------------------------

QString ChatFileItem::mimeType() const
{
    if (!m_mimeType.isEmpty())
    {
        return m_mimeType;
    }

    QMimeDatabase db;
    return db.mimeTypeForFile(m_fileName,QMimeDatabase::MatchExtension).name();
}

//--------------------------------------------------------------------------

AbstractLoadControl::State chatFileLoadControlState(ChatFileTransferState state, bool incoming)
{
    switch (state)
    {
        case (ChatFileTransferState::NotLoaded):
        case (ChatFileTransferState::Paused):
        {
            // deliberately the same mapping for both: a click means "start/continue in this
            // direction" either way, and progress() (0 vs partial) already tells a not-yet-
            // started item apart from a paused one visually -- see AbstractLoadControl::State::
            // Download's own docs for why there's no separate "Resume" state to map Paused to
            return incoming ? AbstractLoadControl::State::Download : AbstractLoadControl::State::Upload;
        }
        break;

        case (ChatFileTransferState::Pending):
        {
            return AbstractLoadControl::State::Waiting;
        }
        break;

        case (ChatFileTransferState::Running):
        {
            return AbstractLoadControl::State::Running;
        }
        break;

        case (ChatFileTransferState::Failed):
        {
            return AbstractLoadControl::State::Failed;
        }
        break;

        case (ChatFileTransferState::Complete):
        {
            return AbstractLoadControl::State::Complete;
        }
        break;

        case (ChatFileTransferState::Cancelled):
        {
            return AbstractLoadControl::State::Cancelled;
        }
        break;

        case (ChatFileTransferState::Ready):
        case (ChatFileTransferState::Unresolved):
        {
        }
        break;
    }

    // Ready/Unresolved have no load control at all -- the caller must hide it rather than map
    // it to a state; None is returned only as a harmless default for those cases.
    return AbstractLoadControl::State::None;
}

//--------------------------------------------------------------------------

bool isChatFileCancellable(ChatFileTransferState state) noexcept
{
    switch (state)
    {
        case (ChatFileTransferState::Pending):
        case (ChatFileTransferState::Running):
        case (ChatFileTransferState::Paused):
        case (ChatFileTransferState::Failed):
            return true;

        case (ChatFileTransferState::Ready):
        case (ChatFileTransferState::NotLoaded):
        case (ChatFileTransferState::Complete):
        case (ChatFileTransferState::Cancelled):
        case (ChatFileTransferState::Unresolved):
            return false;
    }

    return false;
}

//--------------------------------------------------------------------------

bool isChatFileLoadControlClickable(ChatFileTransferState state) noexcept
{
    return state!=ChatFileTransferState::Failed
        && state!=ChatFileTransferState::Cancelled
        && state!=ChatFileTransferState::Unresolved;
}

//--------------------------------------------------------------------------

std::vector<MenuItem> buildChatFileMenuItems(const ChatFileItem& item, bool imageItem, bool incoming, QWidget* context)
{
    // imageItem is currently unused -- reserved for a future per-kind divergence, see the
    // header doc. Named rather than dropped so the call sites at both ChatMessageFileItem and
    // ChatMessageImageItem stay symmetric if/when one is needed.
    Q_UNUSED(imageItem)

    std::vector<ChatFileMenuAction> actions;
    if (!item.menuActions().empty())
    {
        actions=item.menuActions();
    }
    else
    {
        actions.push_back(ChatFileMenuAction::Open);
        actions.push_back(ChatFileMenuAction::SaveAs);
        actions.push_back(ChatFileMenuAction::Forward);
        if (item.isShowInFolderAvailable())
        {
            actions.push_back(ChatFileMenuAction::ShowInFolder);
        }
    }

    std::vector<MenuItem> items;
    for (auto action : actions)
    {
        // Pause/Resume are mutually exclusive by transfer state even when a host lists both --
        // showing a "Resume" entry for a file that is actively running (or vice versa) would be
        // a nonsensical menu row rather than a merely redundant one.
        if (action==ChatFileMenuAction::Pause &&
            item.state()!=ChatFileTransferState::Running && item.state()!=ChatFileTransferState::Pending)
        {
            continue;
        }
        if (action==ChatFileMenuAction::Resume &&
            item.state()!=ChatFileTransferState::Paused && item.state()!=ChatFileTransferState::Failed)
        {
            continue;
        }
        if (action==ChatFileMenuAction::Cancel && !isChatFileCancellable(item.state()))
        {
            continue;
        }
        if (action==ChatFileMenuAction::Download && item.state()!=ChatFileTransferState::NotLoaded)
        {
            continue;
        }

        QString text;
        QString alias;

        switch (action)
        {
            case (ChatFileMenuAction::Open):
                text=QCoreApplication::translate("ChatFileItem","Open");
                alias=QStringLiteral("open");
                break;

            case (ChatFileMenuAction::OpenWith):
                // Label reads "Open in system app", not "Open with" -- this entry no longer
                // promises a choice (todos/todo-image-files-handling-followups.md item 2): it
                // is unconditionally the OS default-application path, same as it always was,
                // now named for what it actually does. The enum value/alias/signal names are
                // unchanged, only the user-visible text.
                text=QCoreApplication::translate("ChatFileItem","Open in system app");
                alias=QStringLiteral("openWith");
                break;

            case (ChatFileMenuAction::SaveAs):
                text=QCoreApplication::translate("ChatFileItem","Save as");
                alias=QStringLiteral("saveAs");
                break;

            case (ChatFileMenuAction::Forward):
                text=QCoreApplication::translate("ChatFileItem","Forward");
                alias=QStringLiteral("forward");
                break;

            case (ChatFileMenuAction::ShowInFolder):
                text=QCoreApplication::translate("ChatFileItem","Show in folder");
                alias=QStringLiteral("showInFolder");
                break;

            case (ChatFileMenuAction::CopyFileName):
                text=QCoreApplication::translate("ChatFileItem","Copy filename");
                alias=QStringLiteral("copyFileName");
                break;

            case (ChatFileMenuAction::Pause):
                text=incoming
                    ? QCoreApplication::translate("ChatFileItem","Pause downloading")
                    : QCoreApplication::translate("ChatFileItem","Pause sending");
                alias=QStringLiteral("pause");
                break;

            case (ChatFileMenuAction::Resume):
                // One action, two meanings by state: continuing a transfer the user paused is
                // "Resume", but picking up a permanently failed one is a fresh attempt, not a
                // continuation -- "Resume sending" would misdescribe it (and, since the load
                // control is a pure indicator in that state, see
                // isChatFileLoadControlClickable(), this entry is the only way to trigger it).
                if (item.state()==ChatFileTransferState::Failed)
                {
                    text=incoming
                        ? QCoreApplication::translate("ChatFileItem","Retry downloading")
                        : QCoreApplication::translate("ChatFileItem","Retry sending");
                    alias=QStringLiteral("retry");
                }
                else
                {
                    text=incoming
                        ? QCoreApplication::translate("ChatFileItem","Resume downloading")
                        : QCoreApplication::translate("ChatFileItem","Resume sending");
                    alias=QStringLiteral("resume");
                }
                break;

            case (ChatFileMenuAction::Cancel):
                text=incoming
                    ? QCoreApplication::translate("ChatFileItem","Cancel downloading")
                    : QCoreApplication::translate("ChatFileItem","Cancel sending");
                alias=QStringLiteral("cancel");
                break;

            case (ChatFileMenuAction::Download):
                // No incoming/outgoing split, unlike every action above -- NotLoaded (the
                // only state this entry appears for, per the gate above) never occurs for an
                // outgoing item, so there is no "outgoing" wording to write.
                text=QCoreApplication::translate("ChatFileItem","Download");
                alias=QStringLiteral("download");
                break;
        }

        items.push_back(MenuItem(static_cast<int>(action),text,chatFileMenuIcon(alias,context)));
    }

    return items;
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END

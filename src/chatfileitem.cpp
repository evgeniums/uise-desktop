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
        {
            return incoming ? AbstractLoadControl::State::CanDownload : AbstractLoadControl::State::CanUpload;
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

        case (ChatFileTransferState::Paused):
        {
            return AbstractLoadControl::State::Paused;
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

        case (ChatFileTransferState::Ready):
        {
        }
        break;
    }

    // Ready has no load control at all -- the caller must hide it rather than map it to a
    // state; None is returned only as a harmless default for that case.
    return AbstractLoadControl::State::None;
}

//--------------------------------------------------------------------------

std::vector<MenuItem> buildChatFileMenuItems(const ChatFileItem& item, bool imageItem, QWidget* context)
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

        QString text;
        QString alias;

        switch (action)
        {
            case (ChatFileMenuAction::Open):
                text=QCoreApplication::translate("ChatFileItem","Open");
                alias=QStringLiteral("open");
                break;

            case (ChatFileMenuAction::OpenWith):
                text=QCoreApplication::translate("ChatFileItem","Open with");
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
                text=QCoreApplication::translate("ChatFileItem","Pause");
                alias=QStringLiteral("pause");
                break;

            case (ChatFileMenuAction::Resume):
                text=QCoreApplication::translate("ChatFileItem","Resume");
                alias=QStringLiteral("resume");
                break;
        }

        items.push_back(MenuItem(static_cast<int>(action),text,chatFileMenuIcon(alias,context)));
    }

    return items;
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END

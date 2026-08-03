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

#include <uise/desktop/chatfileitem.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

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
        case (ChatFileTransferState::Failed):
        {
            return AbstractLoadControl::State::Paused;
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

UISE_DESKTOP_NAMESPACE_END

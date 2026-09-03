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

/** @file uise/desktop/abstractreplybar.hpp
*
*  Declares AbstractReplyBar.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_ABSTRACTREPLYBAR_HPP
#define UISE_DESKTOP_ABSTRACTREPLYBAR_HPP

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/frame.hpp>
#include <uise/desktop/replypreviewdata.hpp>

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

class AbstractReplyPreview;

/**
 * @brief Interface of the short reply-preview bar shown above the chat message editor, before
 *  the reply is actually sent.
 *
 * A plain, standalone frame -- there is no ChatPage/editor-owning widget in this library
 * (MessageEditor is a bare layout around one EnhancedTextEdit), so the host inserts this bar
 * into its own layout immediately above AbstractMessageEditor::qWidget(), and is responsible
 * for showing/hiding it as the pending reply comes and goes -- this widget only ever renders
 * whatever replyData() it is given.
 *
 * Layout, left to right: an icon-only "configure" button (opens AbstractReplyDialog, see
 * configureRequested()), the shared AbstractReplyPreview block (see preview()), and an
 * icon-only "cancel" button (see cancelRequested()).
 */
class UISE_DESKTOP_EXPORT AbstractReplyBar : public WidgetQFrame
{
    Q_OBJECT

    public:

        using WidgetQFrame::WidgetQFrame;

        virtual void setReplyData(ReplyPreviewData data) =0;
        virtual const ReplyPreviewData& replyData() const =0;

        //! Equivalent to setReplyData(ReplyPreviewData{}). Does not hide the bar itself --
        //! showing/hiding it as a reply is set up or cancelled is the host's job.
        virtual void clear() =0;

        //! The shared preview block this bar hosts -- see AbstractReplyPreview.
        virtual AbstractReplyPreview* preview() const =0;

        //! Forwarded to preview()->setTextTrimLength()/textTrimLength().
        virtual void setTextTrimLength(int length) =0;
        virtual int textTrimLength() const =0;

    signals:

        //! The configure button was clicked -- a host typically opens an AbstractReplyDialog
        //! here.
        void configureRequested();

        //! The cancel button was clicked -- a host typically drops the pending reply and hides
        //! this bar.
        void cancelRequested();

        //! Forwarded from preview()'s own AbstractReplyPreview::clicked() -- a click on the
        //! preview block itself, distinct from either button.
        void clicked();
};

}

#endif // UISE_DESKTOP_ABSTRACTREPLYBAR_HPP

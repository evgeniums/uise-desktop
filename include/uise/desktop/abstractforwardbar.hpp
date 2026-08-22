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

/** @file uise/desktop/abstractforwardbar.hpp
*
*  Declares AbstractForwardBar.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_ABSTRACTFORWARDBAR_HPP
#define UISE_DESKTOP_ABSTRACTFORWARDBAR_HPP

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/frame.hpp>
#include <uise/desktop/replypreviewdata.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class AbstractReplyPreview;

/**
 * @brief Interface of the short forward-preview bar shown above the chat message editor, before
 *  the forward is actually sent -- task-message-forwarding.md's "forwarding preview".
 *
 * A plain, standalone frame, structured exactly like AbstractReplyBar: there is no ChatPage/
 * editor-owning widget in this library, so the host inserts this bar into its own layout
 * immediately above AbstractMessageEditor::qWidget(), and is responsible for showing/hiding it
 * as the pending forward comes and goes -- this widget only ever renders whatever forwardData()/
 * messageCount() it is given.
 *
 * Layout, left to right: an icon-only "configure" button (opens AbstractForwardDialog, see
 * configureRequested()), then EITHER the shared AbstractReplyPreview block (single message,
 * see preview()) OR a plain count label (multi-message, see setMessageCount()), and an icon-only
 * "cancel" button (see cancelRequested()).
 */
class UISE_DESKTOP_EXPORT AbstractForwardBar : public WidgetQFrame
{
    Q_OBJECT

    public:

        using WidgetQFrame::WidgetQFrame;

        virtual void setForwardData(ReplyPreviewData data) =0;
        virtual const ReplyPreviewData& forwardData() const =0;

        //! Equivalent to setForwardData(ReplyPreviewData{}). Does not hide the bar itself --
        //! showing/hiding it as a forward is set up or cancelled is the host's job.
        virtual void clear() =0;

        //! The shared preview block this bar hosts when messageCount()<=1 -- see
        //! AbstractReplyPreview. Exposed for per-instance tuning (textTrimLength()/
        //! maxWidthHint()) that QSS alone cannot express.
        virtual AbstractReplyPreview* preview() const =0;

        //! Forwarded to preview()->setTitleFormat()/titleFormat() -- default
        //! tr("Forwarded from %1"). preview()'s own titleFormat is a plain virtual setter, not a
        //! Q_PROPERTY, so QSS cannot set it; use this instead of reaching through preview().
        virtual void setTitleFormat(const QString& format) =0;
        virtual QString titleFormat() const =0;

        //! Forwarded to preview()->setTextTrimLength()/textTrimLength().
        virtual void setTextTrimLength(int length) =0;
        virtual int textTrimLength() const =0;

        /**
         * @brief Set how many messages are pending forward.
         * @param count 0 or 1 shows preview() as usual (single-message mode); >1 hides preview()
         *  and shows countFormat() instead (multi-message mode) -- task-message-forwarding.md:
         *  "If there are multiple messages selected for forwarding then message preview differs.
         *  It shows only number of messages to forward."
         */
        virtual void setMessageCount(int count) =0;
        virtual int messageCount() const =0;

        /**
         * @brief Override the multi-message count label's text.
         * @param format Unset (the default) uses the built-in tr("%n messages to forward","",n)
         *  plural form, re-evaluated against the CURRENT messageCount() on every change, so it
         *  reacts correctly under Qt's plural rules. A custom format is stored as plain text and
         *  substitutes %1 with messageCount() via QString::arg() -- not %n -- since it cannot be
         *  re-run through Qt's translation engine after the fact; a host wanting proper plural
         *  handling for a custom string should call tr() itself and pass the already-resolved
         *  result in here instead.
         */
        virtual void setCountFormat(const QString& format) =0;
        virtual QString countFormat() const =0;

    signals:

        //! The configure button was clicked -- a host typically opens an AbstractForwardDialog
        //! here.
        void configureRequested();

        //! The cancel button was clicked -- a host typically drops the pending forward and hides
        //! this bar.
        void cancelRequested();

        //! Forwarded from preview()'s own AbstractReplyPreview::clicked() -- a click on the
        //! preview block itself (single-message mode only), distinct from either button.
        void clicked();
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_ABSTRACTFORWARDBAR_HPP

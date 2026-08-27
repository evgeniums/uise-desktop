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

/** @file uise/desktop/abstracteditbar.hpp
*
*  Declares AbstractEditBar.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_ABSTRACTEDITBAR_HPP
#define UISE_DESKTOP_ABSTRACTEDITBAR_HPP

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/frame.hpp>
#include <uise/desktop/replypreviewdata.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class AbstractReplyPreview;

/**
 * @brief Interface of the short edit-preview bar shown above the chat message editor while an
 *  existing message's text is being edited.
 *
 * A plain, standalone frame, structured like AbstractReplyBar/AbstractForwardBar: there is no
 * ChatPage/editor-owning widget in this library, so the host inserts this bar into its own
 * layout immediately above AbstractMessageEditor::qWidget(), and is responsible for
 * showing/hiding it as the pending edit comes and goes, and for loading the message's current
 * text into the editor itself -- this widget only ever renders whatever editData() it is given.
 *
 * Layout, left to right: an icon-only "edit" button (jumps to the message being edited, see
 * jumpRequested()), the shared AbstractReplyPreview block (see preview()) -- title format
 * defaults to tr("Edit message, %2"), a datetime with no sender, since there is nothing to
 * attribute an edit of one's own message to -- and an icon-only "cancel" button (see
 * cancelRequested()). Unlike AbstractReplyBar/AbstractForwardBar there is no separate configure
 * button: there is nothing to configure about an edit.
 */
class UISE_DESKTOP_EXPORT AbstractEditBar : public WidgetQFrame
{
    Q_OBJECT

    //! QSS: qproperty-closeOnEscape: false; -- whether this bar closes itself (emitting
    //! cancelRequested()) on Escape while visible. Default true so a standalone host (this
    //! library's own demo, or any host with no window-level Escape of its own) works out of the
    //! box. A host that already owns a window-level Qt::WindowShortcut on Escape (as
    //! whitemdesktop's ChatPage does, to build its own close/selection/pending-bar ladder) MUST
    //! set this false: two Qt::WindowShortcut instances bound to the same key in one window fire
    //! Qt's activatedAmbiguously() instead of activated() on EITHER of them, so both would
    //! silently stop working. Such a host instead adds this bar to its own Escape ladder and
    //! reacts to cancelRequested()/calls this bar's cancel path directly.
    Q_PROPERTY(bool closeOnEscape READ isCloseOnEscape WRITE setCloseOnEscape)

    public:

        using WidgetQFrame::WidgetQFrame;

        virtual void setEditData(ReplyPreviewData data) =0;
        virtual const ReplyPreviewData& editData() const =0;

        //! Equivalent to setEditData(ReplyPreviewData{}). Does not hide the bar itself --
        //! showing/hiding it as an edit is set up or cancelled is the host's job.
        virtual void clear() =0;

        //! The shared preview block this bar hosts -- see AbstractReplyPreview.
        virtual AbstractReplyPreview* preview() const =0;

        //! Forwarded to preview()->setTitleFormat()/titleFormat() -- default
        //! tr("Edit message, %2").
        virtual void setTitleFormat(const QString& format) =0;
        virtual QString titleFormat() const =0;

        //! Forwarded to preview()->setTextTrimLength()/textTrimLength().
        virtual void setTextTrimLength(int length) =0;
        virtual int textTrimLength() const =0;

        void setCloseOnEscape(bool enable)
        {
            m_closeOnEscape=enable;
            updateCloseOnEscape();
        }

        bool isCloseOnEscape() const noexcept
        {
            return m_closeOnEscape;
        }

    signals:

        //! The edit icon button was clicked, OR preview()'s own AbstractReplyPreview::clicked()
        //! fired (a click anywhere else on the bar) -- both halves of this bar mean the same
        //! thing: a host typically scrolls/jumps to the message being edited and highlights it.
        void jumpRequested();

        //! The cancel button was clicked, or Escape was pressed while isCloseOnEscape() is true
        //! -- a host typically drops the pending edit, clears the composer and hides this bar.
        void cancelRequested();

    protected:

        //! Called after setCloseOnEscape() -- no-op in the base class.
        virtual void updateCloseOnEscape() {}

    private:

        bool m_closeOnEscape=true;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_ABSTRACTEDITBAR_HPP

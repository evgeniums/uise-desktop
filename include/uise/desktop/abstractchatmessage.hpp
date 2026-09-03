/**
@copyright Evgeny Sidorov 2021

This software is dual-licensed. Choose the appropriate license for your project.

1. The GNU GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-GPLv3.md](LICENSE-GPLv3.md) or copy at https://www.gnu.org/licenses/gpl-3.0.txt)
    
2. The GNU LESSER GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-LGPLv3.md](LICENSE-LGPLv3.md) or copy at https://www.gnu.org/licenses/lgpl-3.0.txt).

You may select, at your option, one of the above-listed licenses.

*/

/****************************************************************************/

/** @file uise/desktop/abstractchatmessage.hpp
*
*  Declares AbstractChatMessage.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_ABSTRACTCHATMESSAGE_HPP
#define UISE_DESKTOP_ABSTRACTCHATMESSAGE_HPP

#include <QPointer>
#include <QDateTime>
#include <QColor>
#include <QUrl>
#include <QPoint>
#include <QUuid>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/utils/destroywidget.hpp>
#include <uise/desktop/svgicon.hpp>
#include <uise/desktop/avatar.hpp>
#include <uise/desktop/frame.hpp>
#include <uise/desktop/utils/withpathandsize.hpp>
#include <uise/desktop/replypreviewdata.hpp>
#include <uise/desktop/abstractmessageeditor.hpp> // TextFormat, shared with loadText()/setComment()

class QVariantAnimation;

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

class AvatarWidget;
class SingleShotTimer;

class AbstractChatMessage;

class UISE_DESKTOP_EXPORT AbstractChatMessageChild : public WidgetQFrame
{
    Q_OBJECT

    public:

        using WidgetQFrame::WidgetQFrame;

        inline void setChatMessage(AbstractChatMessage* chatMessage);

        AbstractChatMessage* chatMessage() const noexcept
        {
            return m_chatMessage;
        }

        virtual void clearContentSelection() {}

    protected:

        virtual void updateChatMessage() {}

    private:

        AbstractChatMessage* m_chatMessage=nullptr;
};

class UISE_DESKTOP_EXPORT AbstractChatSeparatorSection : public AbstractChatMessageChild
{
    Q_OBJECT

    public:

        constexpr static const char* TypeDate="date";
        constexpr static const char* TypeFolder="folder";
        constexpr static const char* TypeUnreadMessages="unread";

        using AbstractChatMessageChild::AbstractChatMessageChild;

        virtual void setHLineVisible(bool enable) =0;
        virtual bool isHLineVisible() const=0;

        virtual void setText(const QString& text) =0;
        virtual QString text() const =0;

        virtual void setIconPath(WithPath path) =0;
        virtual WithPath iconPath() const =0;

        virtual void setIconSource(std::shared_ptr<AvatarSource> avatarSource) =0;
        virtual std::shared_ptr<AvatarSource> iconSource() const =0;

        virtual void setTailIcon(std::shared_ptr<SvgIcon> icon) =0;
        virtual std::shared_ptr<SvgIcon> tailIcon() const =0;

        virtual void setClickable(bool enable) =0;
        virtual bool isClickable() const=0;

        //! Widget that actually receives the click -- the visible pill, not the full-width
        //! section. Used to anchor a popup at the pill's own geometry (e.g. a calendar dialog
        //! opened from a date separator). Default is the section itself, so an implementation
        //! with no separate inner widget needs no override.
        virtual QWidget* clickableWidget()
        {
            return this;
        }

        void setType(QString type)
        {            
            setProperty("separator_type",type);
            m_type=std::move(type);
        }

        const QString& type() const
        {
            return m_type;
        }

    signals:

        void clicked();

    private:

        QString m_type;
};

class UISE_DESKTOP_EXPORT AbstractChatSeparator : public AbstractChatMessageChild
{
    Q_OBJECT

    public:

        using AbstractChatMessageChild::AbstractChatMessageChild;

        void insertSection(AbstractChatSeparatorSection* section, int index=-1)
        {
            if (index>=0 && index<static_cast<int>(m_sections.size()))
            {
                auto pos=m_sections.begin()+index;
                m_sections.emplace(pos,section);
            }
            else
            {
                m_sections.emplace_back(section);
            }
            section->setChatMessage(chatMessage());
            doInsertSection(section,index);
        }

        void appendSection(AbstractChatSeparatorSection* section)
        {
            insertSection(section);
        }

        AbstractChatSeparatorSection* section(int index) const
        {
            if (index<0 || index>=static_cast<int>(m_sections.size()))
            {
                return nullptr;
            }

            return m_sections.at(index);
        }

        size_t sectionCount() const noexcept
        {
            return m_sections.size();
        }

        AbstractChatSeparatorSection* section(const QString& type) const
        {
            for (const auto& section : m_sections)
            {
                if (section->type()==type)
                {
                    return section;
                }
            }
            return nullptr;
        }

    protected:

        virtual void doInsertSection(AbstractChatSeparatorSection* section, int index) =0;

    private:

        std::vector<QPointer<AbstractChatSeparatorSection>> m_sections;
};

class AbstractChatMessageContent;

class UISE_DESKTOP_EXPORT ChatMessageContentSection : public AbstractChatMessageChild
{
    Q_OBJECT

    public:

        using AbstractChatMessageChild::AbstractChatMessageChild;

        void setChatContent(AbstractChatMessageContent* chatContent)
        {
            m_content=chatContent;
        }

        AbstractChatMessageContent* chatContent() const noexcept
        {
            return m_content;
        }

        virtual int bubbleWidthHint(int /*forMaxWidth*/)
        {
            return sizeHint().width();
        }

        virtual void updateMaximumBubbleWidth()
        {
            updateGeometry();
        }

        virtual void setSelected(bool /*enable*/) {}
        virtual void setSent(bool /*enable*/) {}

    private:

        AbstractChatMessageContent* m_content=nullptr;        
};

//! Hosts whatever sits at the very top of the bubble -- currently ChatMessageForwardHeader
//! ("Forwarded from <author>", see chatmessageforwardheader.hpp). A later group-chat
//! sender-name header would extend the same concrete section rather than claim a new slot.
class UISE_DESKTOP_EXPORT AbstractChatMessageHeader : public ChatMessageContentSection
{
    Q_OBJECT

    public:

        using ChatMessageContentSection::ChatMessageContentSection;
};

class AbstractReplyPreview;

/**
 * @brief Section shown between the bubble header and the bubble body, previewing the message
 *  this message is a reply to.
 *
 * One of AbstractChatMessageContent's 5 slots, deliberately kept separate from
 * AbstractChatMessageHeader -- this section gets its own QSS type selector plus its own
 * setSelected()/setSent() forwarding, independent of the header's.
 * Decoration is entirely QSS-driven -- see replypreview.qss for the vertical accent bar +
 * tinted background convention.
 */
class UISE_DESKTOP_EXPORT AbstractChatMessageReply : public ChatMessageContentSection
{
    Q_OBJECT

    public:

        using ChatMessageContentSection::ChatMessageContentSection;

        virtual void setReplyData(ReplyPreviewData data) =0;
        virtual const ReplyPreviewData& replyData() const =0;

        /**
         * @brief The shared AbstractReplyPreview block hosted by this section -- the same
         *  block reused by AbstractReplyBar and AbstractReplyDialog.
         *
         * Exposed for per-instance tuning (textTrimLength()/maxWidthHint()) that QSS alone
         * cannot express; most callers never need this, setReplyData() is enough.
         */
        virtual AbstractReplyPreview* preview() const =0;

        //! Convenience equivalent to replyData().isDeleted()/ReplyPreviewData::setDeleted() --
        //! see that setter's own docs for what this changes in the rendered block.
        virtual void setOriginalDeleted(bool enable) =0;
        virtual bool isOriginalDeleted() const =0;

    signals:

        //! The user clicked this section -- a host typically scrolls/jumps to the original
        //! message ("Show in chat"). Forwarded from preview()'s own
        //! AbstractReplyPreview::clicked().
        void clicked();
};

/**
 * @brief Section shown between the bubble body and the bubble bottom, carrying the forwarding
 *  sender's own comments on a forwarded message ("cited and quoted" form, see
 *  tasks/task-message-forwarding.md).
 *
 * A genuine 5th AbstractChatMessageContent slot, deliberately kept as a sibling of
 * AbstractChatMessageBody rather than a subclass of it -- a subclass would implicitly convert
 * to setWidgets()'s `body` parameter, a silent mis-wiring footgun. Duplicates
 * AbstractChatMessageBody's selection quartet (selectedText()/hasSelectableText()/
 * setCopyable()/selectText()) and its selectionChanged() signal rather than inheriting them,
 * for the same reason.
 */
class UISE_DESKTOP_EXPORT AbstractChatMessageComment : public ChatMessageContentSection
{
    Q_OBJECT

    public:

        using ChatMessageContentSection::ChatMessageContentSection;

        virtual void setComment(const QString& text, TextFormat format=TextFormat::Markdown) =0;
        virtual void clearComment() =0;
        virtual QString comment() const =0;

        virtual QString selectedText() const {return QString{};}

        //! See AbstractChatMessageBody::hasSelectableText() -- same contract, applied to this
        //! section's own embedded text instead of a body's.
        virtual bool hasSelectableText() const {return false;}

        //! See AbstractChatMessageBody::setCopyable().
        virtual void setCopyable(bool enable) {std::ignore=enable;}

        //! See AbstractChatMessageBody::setOwnContextMenuEnabled().
        virtual void setOwnContextMenuEnabled(bool enable) {std::ignore=enable;}

        //! See AbstractChatMessageBody::selectText().
        virtual void selectText(const QString& /*text*/) {}

        //! See AbstractChatMessageBody::linkAt().
        virtual QString linkAt(const QPoint& /*pos*/) const {return QString{};}

    signals:

        //! See AbstractChatMessageBody::selectionChanged().
        void selectionChanged();

        //! See AbstractChatMessageBody::linkActivated().
        void linkActivated(const QUrl& url);
};

class UISE_DESKTOP_EXPORT AbstractChatMessageBody : public ChatMessageContentSection
{
    Q_OBJECT

    public:

        using ChatMessageContentSection::ChatMessageContentSection;

        virtual QString selectedText() const {return QString{};}

        //! Check whether this body currently holds any text a user could select (and so quote
        //! via AbstractReplyDialog's "Quote selected" -- see ReplyDialog::updateCommentVisibility(),
        //! which uses this to decide whether the "you can select a part of the text" comment
        //! makes sense to show at all). False by default -- a body with no text at all
        //! (ChatMessageCall, whose selectedText() returns a synthesized summary purely for Copy's
        //! benefit) never overrides this. A text body overrides it based on whatever content it
        //! currently holds; ChatMessageFiles/ChatMessageImages forward to their optional embedded
        //! comment (a ChatMessageText), answering false when they carry none.
        virtual bool hasSelectableText() const {return false;}

        /**
         * @brief Make this body's text focusable and enable its standard text-edit context
         *  menu (Copy, Select All) plus the Ctrl+C/Cmd+C shortcut.
         * @param enable Off by default (see ChatMessageText's own ctor): the live chat page
         *  handles its own selection-mode gesture and right-click menu instead, and a focusable
         *  text body there would steal keyboard focus from the message editor. A host showing a
         *  message OUTSIDE that context (e.g. AbstractReplyDialog's static preview) opts in via
         *  this. No-op for a body with no text at all (ChatMessageCall); ChatMessageFiles/
         *  ChatMessageImages forward to their optional comment, if any.
         */
        virtual void setCopyable(bool enable) {std::ignore=enable;}

        /**
         * @brief Suppress this body's own built-in Copy/Select All context menu.
         * @param enable On by default -- a host showing this body's own message-level context
         *  menu instead (e.g. a static preview bubble embedded in a dialog, which offers its own
         *  reduced "Copy selected text"/"Clear selection" menu) turns this off so the two menus
         *  don't compete over the same right-click. Independent of setCopyable(): with this off,
         *  the body stays focusable/selectable (Ctrl+C still works), it just never pops its own
         *  menu. No-op for a body with no text at all (ChatMessageCall); ChatMessageFiles/
         *  ChatMessageImages forward to their optional comment, if any -- same forwarding pattern
         *  as setCopyable() above.
         */
        virtual void setOwnContextMenuEnabled(bool enable) {std::ignore=enable;}

        /**
         * @brief Select the first occurrence of `text` within this body's own content, if any.
         * @param text Text to find and select -- typically a quote already picked once (e.g. a
         *  message context menu's "Quote and reply"), being re-applied to a fresh preview of the
         *  same message (e.g. AbstractReplyDialog's own static bubble) so the dialog opens with
         *  that same fragment already highlighted instead of nothing selected.
         *
         * No-op by default -- a body with no selectable content at all (ChatMessageCall) never
         * overrides this; a text body overrides it to search its own rendered plain text.
         * ChatMessageFiles/ChatMessageImages forward to their optional comment, if any. Best-
         * effort: silently does nothing if `text` isn't found (e.g. content changed since the
         * quote was picked) or the body has no comment to search.
         */
        virtual void selectText(const QString& /*text*/) {}

        /**
         * @brief Href of the hyperlink rendered at widget-local position `pos`, or empty if
         *  there is none there.
         *
         * task-urls-and characters-in-messages.md follow-up: lets a host offer a "Copy Link"
         * action in its OWN context menu (e.g. whitemdesktop's per-message
         * ChatMessage::showMessageContextMenu()) without that host needing to know this is a
         * QTextBrowser-backed widget under the hood -- deliberately a plain query rather than a
         * second context-menu mechanism of this widget's own. No-op by default (ChatMessageCall,
         * which renders no links at all); a text body overrides it against its own rendered
         * anchors, and ChatMessageFiles/ChatMessageImages forward to their optional embedded
         * comment, if any -- same forwarding pattern as hasSelectableText()/setCopyable()/
         * selectText() above.
         */
        virtual QString linkAt(const QPoint& /*pos*/) const {return QString{};}

        /**
         * @brief Id of the per-item child (a file row / an image tile) rendered at widget-local
         *  position `pos`, or a null QUuid if none is there.
         *
         * Same rationale as linkAt() just above: lets a host build its OWN per-item submenu into
         * its message-level context menu (whitemdesktop's ChatMessage::showMessageContextMenu())
         * without knowing this is a ChatMessageFiles/ChatMessageImages under the hood. No-op by
         * default; ChatMessageFiles/ChatMessageImages override it against their own rows/tiles.
         */
        virtual QUuid fileItemAt(const QPoint& /*pos*/) const {return QUuid{};}

    signals:

        //! Never emitted by the base class -- a body with genuine text selection (e.g.
        //! ChatMessageText, whose underlying QTextEdit already has this exact signal) connects
        //! it here, so a host (e.g. ReplyDialog's Save/"Quote selected" button swap) can react
        //! to selection changes without depending on a concrete body type. ChatMessageFiles/
        //! ChatMessageImages relay it from their optional embedded comment, if any. A body with no
        //! selectable content at all (ChatMessageCall) simply never fires it.
        void selectionChanged();

        //! A hyperlink rendered inside this body's text was clicked (task-urls-and characters-in-
        //! messages.md, Stage 1). Never emitted by the base class -- ChatMessageText relays its
        //! ChatMessageTextBrowser's own anchorClicked() here, exactly like selectionChanged()
        //! above. Activation itself (opening the OS browser, or -- once Stage 3 lands -- routing an
        //! in-app mention scheme) is deliberately NOT decided here; the host owns that policy, see
        //! whitemdesktop's ChatMessages::onLinkActivated().
        void linkActivated(const QUrl& url);
};

class UISE_DESKTOP_EXPORT AbstractChatMessageBottom : public ChatMessageContentSection
{
    Q_OBJECT

    Q_PROPERTY(int narrowBodyWidth READ narrowBodyWidth WRITE setNarrowBodyWidth)

    public:

        constexpr static const int DefaultNarrowBodyWidth=200;

        using ChatMessageContentSection::ChatMessageContentSection;

        virtual void setSeen(const QString& /*text*/, const QString& /*tooltip*/={}) {}
        virtual void setEdited(const QString& /*text*/, const QString& /*tooltip*/={}) {}
        virtual void setTimeString(const QString& /*time*/, const QString& /*tooltip*/={}) {}
        virtual void setStatusIcon(std::shared_ptr<SvgIcon> /*icon*/ ={}, const QString& /*tooltip*/={}) {}

        //! Body widths below this are considered "too narrow to host the time/status row":
        //! for those the bubble is widened to body + bottom, otherwise the body alone
        //! governs the bubble width. Settable from QSS via qproperty-narrowBodyWidth.
        void setNarrowBodyWidth(int width) noexcept
        {
            m_narrowBodyWidth=width;
        }

        int narrowBodyWidth() const noexcept
        {
            return m_narrowBodyWidth;
        }

    private:

        int m_narrowBodyWidth=DefaultNarrowBodyWidth;
};

class UISE_DESKTOP_EXPORT AbstractChatMessageContent : public AbstractChatMessageChild
{
    Q_OBJECT

    public:

        using AbstractChatMessageChild::AbstractChatMessageChild;

        /**
         * @brief Set this bubble's up-to-5 content sections.
         * @param reply Defaulted, so every call site that predates the reply section keeps
         *  compiling unchanged.
         * @param comment Defaulted, so every call site that predates the comment section keeps
         *  compiling unchanged. Display order is header / reply / body / comment / bottom
         *  regardless of these parameters' position -- see ChatMessageContent::updateWidgets().
         */
        void setWidgets(AbstractChatMessageBody* body, AbstractChatMessageHeader* header=nullptr,
                        AbstractChatMessageBottom* bottom=nullptr, AbstractChatMessageReply* reply=nullptr,
                        AbstractChatMessageComment* comment=nullptr)
        {
            destroyWidget(m_header);
            m_header=header;
            destroyWidget(m_body);
            m_body=body;
            destroyWidget(m_bottom);
            m_bottom=bottom;
            destroyWidget(m_reply);
            m_reply=reply;
            destroyWidget(m_comment);
            m_comment=comment;
            rebuildSections();
            updateWidgets();
            wireSelectionExclusivity();
        }

        /**
         * @brief Attach, replace or remove the reply section after construction, without
         *  touching header()/body()/comment()/bottom().
         * @param reply New section, or nullptr to remove it -- see clearReply(). Destroys
         *  whatever reply section was previously set.
         *
         * For a reply target resolved asynchronously (looked up after the bubble is already on
         * screen) or a reply whose original message is deleted while the bubble is visible --
         * setWidgets() itself would also work, but would needlessly repeat header()/body()/
         * comment()/bottom().
         */
        void setReply(AbstractChatMessageReply* reply)
        {
            destroyWidget(m_reply);
            m_reply=reply;
            rebuildSections();
            updateWidgets();
            // A section attached here (rather than via the initial setWidgets()) would
            // otherwise never learn the bubble's current selected/sent state -- see
            // setSelected()'s doc comment.
            if (m_reply!=nullptr)
            {
                m_reply->setSelected(isContentSelected());
                m_reply->setSent(isContentSent());
            }
        }

        void clearReply()
        {
            setReply(nullptr);
        }

        /**
         * @brief Attach, replace or remove the comment section after construction, without
         *  touching header()/body()/reply()/bottom().
         * @param comment New section, or nullptr to remove it -- see clearComment(). Destroys
         *  whatever comment section was previously set.
         *
         * For a forwarded message whose sender comments are edited/resolved after the bubble is
         * already on screen -- setWidgets() itself would also work, but would needlessly repeat
         * header()/body()/reply()/bottom().
         */
        void setComment(AbstractChatMessageComment* comment)
        {
            destroyWidget(m_comment);
            m_comment=comment;
            rebuildSections();
            updateWidgets();
            wireSelectionExclusivity();
            // See setReply()'s identical re-application and setSelected()'s doc comment for why.
            if (m_comment!=nullptr)
            {
                m_comment->setSelected(isContentSelected());
                m_comment->setSent(isContentSent());
            }
        }

        void clearComment()
        {
            setComment(nullptr);
        }

        AbstractChatMessageHeader* header() const noexcept
        {
            return m_header;
        }

        AbstractChatMessageBody* body() const noexcept
        {
            return m_body;
        }

        AbstractChatMessageBottom* bottom() const noexcept
        {
            return m_bottom;
        }

        AbstractChatMessageReply* reply() const noexcept
        {
            return m_reply;
        }

        AbstractChatMessageComment* comment() const noexcept
        {
            return m_comment;
        }

        int maximumBubbleWidth() const noexcept
        {
            return m_maximumBubbleWidth;
        }

        //! Returns body()->bubbleWidthHint(forMaxWidth), memoized against the CURRENT negotiation
        //! pass's forMaxWidth. updateBubbleWidth() below populates the memo before querying any
        //! section, so ChatMessageBottom::bubbleWidthHint() (the only other caller, needing the
        //! body's width to decide whether to widen the bubble) gets a plain lookup instead of a
        //! second real measurement -- for a body like ChatMessageImages that means one
        //! rebuildGrid() per negotiation pass instead of two. A stale forMaxWidth (call from
        //! outside a negotiation pass) falls back to a real, unmemoized call.
        int bodyWidthHint(int forMaxWidth)
        {
            if (m_body==nullptr)
            {
                return 0;
            }
            if (!m_bodyWidthHintValid || m_bodyWidthHintForMaxWidth!=forMaxWidth)
            {
                return m_body->bubbleWidthHint(forMaxWidth);
            }
            return m_bodyWidthHint;
        }

        /**
         * @brief Set whether this bubble is currently selected (selection-mode checkbox).
         * @param enable New state.
         *
         * An override MUST also call rememberSelected(enable) (typically first) so that
         * setReply()/setComment() can re-apply the current state to a section attached AFTER
         * this was last called -- otherwise a section attached asynchronously (a reply target
         * resolved late, or forwarded comments arriving after the bubble is already on screen)
         * silently never receives the state at all, since AbstractChatMessage calls this once
         * per state change, not once per section.
         */
        virtual void setSelected(bool enable) =0;

        //! See setSelected() -- same "must call rememberSent()" contract, for the [sent="..."]
        //! QSS state instead of [selected="..."].
        virtual void setSent(bool enable) =0;

        //! Bubble tail/corner-radius geometry side -- independent of setSent()'s colour, so a
        //! sent message aligned to the left still gets a left-pointing tail (see chat.qss
        //! [right=...] rules). Default no-op: unlike setSent()/setSelected(), no section other
        //! than the bubble itself keys QSS on this, so there is nothing to fan out.
        virtual void setRight(bool /*enable*/) {}

        void updateBubbleWidth(int forMaxWidthIn);

        //! Re-runs updateBubbleWidth() against the SAME forMaxWidthIn it was last called with --
        //! for a section whose own natural size changed without any resize/scroll (e.g. an
        //! image tile's dimensions or available-resolution ceiling resolving asynchronously
        //! after the section was first laid out from placeholder/conservative values). A no-op
        //! before the first real updateBubbleWidth() call (ChatMessagesView::adjustMessagesSizes()
        //! always runs at least one before any section could have real content to react to).
        //! Unlike the normal top-down flow (ChatMessagesView drives every updateBubbleWidth()
        //! call), this lets a BODY request its own re-layout -- see ChatMessageImages::
        //! updateItem() for the motivating case.
        void renegotiateBubbleWidth();

        const auto& sections() const
        {
            return m_sections;
        }

        QSize sizeHint() const override;

    signals:

        void bubbleWidthUpdated();

    protected:

        virtual void updateWidgets() =0;

        //! Record the current selected/sent state -- see setSelected()'s doc comment for why an
        //! override must call these. isContentSelected()/isContentSent() below read it back.
        void rememberSelected(bool enable) noexcept
        {
            m_selected=enable;
        }

        void rememberSent(bool enable) noexcept
        {
            m_sent=enable;
        }

        bool isContentSelected() const noexcept
        {
            return m_selected;
        }

        bool isContentSent() const noexcept
        {
            return m_sent;
        }

    private:

        //! Rebuilds m_sections from whichever of m_header/m_reply/m_body/m_comment/m_bottom are
        //! currently non-null, in DISPLAY order, wiring each via setChatMessage()/
        //! setChatContent(). Called by setWidgets(), setReply() and setComment() -- previously
        //! setWidgets() only ever ran once per instance and just push_back()'d into
        //! m_sections without clearing it first, which left stale (dangling, past their
        //! destroyWidget() calls) entries the moment it ran a second time; setReply() makes a
        //! second run reachable, so this clears first.
        void rebuildSections();

        //! Body and comment are two independent text widgets (see AbstractChatMessageComment's
        //! own class doc comment on why comment is a sibling of body, not a subclass) -- without
        //! this, both could hold a live selection at once and a quote would silently come out as
        //! "body text\ncomment text" (see ChatMessage::selectedText()'s join). Connects each
        //! section's selectionChanged so that acquiring a NEW selection in one clears the other,
        //! matching how ChatMessagesView::clearOtherContentsSelection() already treats selection
        //! as one-at-a-time across whole messages. Re-run (and so re-wired against the current
        //! m_body/m_comment) by setWidgets() and setComment() -- setReply() never touches either,
        //! so it has no need to call this.
        void wireSelectionExclusivity()
        {
            if (m_body==nullptr || m_comment==nullptr)
            {
                return;
            }
            // setComment() can re-run this later against an UNCHANGED body (only the comment
            // section itself was replaced/reattached) -- guard against piling up a fresh pair of
            // connections on that same body every time, which would fire the (harmlessly
            // idempotent, but pointless) clear callback multiple times per selection change.
            if (m_body==m_wiredSelectionBody && m_comment==m_wiredSelectionComment)
            {
                return;
            }
            m_wiredSelectionBody=m_body;
            m_wiredSelectionComment=m_comment;
            connect(m_body,&AbstractChatMessageBody::selectionChanged,this,
                [this]()
                {
                    if (m_clearingSelection || m_body==nullptr || m_comment==nullptr)
                    {
                        return;
                    }
                    if (!m_body->selectedText().isEmpty())
                    {
                        m_clearingSelection=true;
                        m_comment->clearContentSelection();
                        m_clearingSelection=false;
                    }
                }
            );
            connect(m_comment,&AbstractChatMessageComment::selectionChanged,this,
                [this]()
                {
                    if (m_clearingSelection || m_body==nullptr || m_comment==nullptr)
                    {
                        return;
                    }
                    if (!m_comment->selectedText().isEmpty())
                    {
                        m_clearingSelection=true;
                        m_body->clearContentSelection();
                        m_clearingSelection=false;
                    }
                }
            );
        }

        QPointer<AbstractChatMessageHeader> m_header=nullptr;
        QPointer<AbstractChatMessageReply> m_reply=nullptr;
        QPointer<AbstractChatMessageBody> m_body=nullptr;
        QPointer<AbstractChatMessageComment> m_comment=nullptr;
        QPointer<AbstractChatMessageBottom> m_bottom=nullptr;

        //! The (body, comment) pair wireSelectionExclusivity() last connected -- see its own doc
        //! comment.
        QPointer<AbstractChatMessageBody> m_wiredSelectionBody=nullptr;
        QPointer<AbstractChatMessageComment> m_wiredSelectionComment=nullptr;

        bool m_selected=false;
        bool m_sent=false;
        bool m_clearingSelection=false;

        std::vector<ChatMessageContentSection*> m_sections;
        int m_maximumBubbleWidth=0;
        int m_lastForMaxWidth=0;
        bool m_everNegotiated=false;

        int m_bodyWidthHint=0;
        int m_bodyWidthHintForMaxWidth=0;
        bool m_bodyWidthHintValid=false;

        void setMaximumBubbleWidth(int width);
};

class UISE_DESKTOP_EXPORT AbstractChatMessage : public WidgetQFrame
{
    Q_OBJECT

    Q_PROPERTY(bool selectorPositionLeft READ isSelectorOnLeft WRITE setSelectorOnLeft NOTIFY selectorPositionChanged FINAL)

    //! Transient jump-to-message highlight -- see startHighlight()/highlightRect() below.
    //! highlightColor's own alpha combines multiplicatively with highlightOpacity, same
    //! convention as RippleOverlay::rippleColor/rippleOpacity (see ripple.hpp): a fully opaque
    //! QSS colour plus a fractional opacity is the usual way to tune this.
    Q_PROPERTY(QColor highlightColor READ highlightColor WRITE setHighlightColor)
    Q_PROPERTY(qreal highlightOpacity READ highlightOpacity WRITE setHighlightOpacity)
    Q_PROPERTY(int highlightHoldMs READ highlightHoldMs WRITE setHighlightHoldMs)
    Q_PROPERTY(int highlightFadeMs READ highlightFadeMs WRITE setHighlightFadeMs)
    //! QEasingCurve::Type as int (e.g. 6=OutCubic) -- same qproperty-*EasingCurveType idiom as
    //! RippleOverlay::rippleEasingCurveType (ripple.hpp).
    Q_PROPERTY(int highlightEasingCurveType READ highlightEasingCurveType WRITE setHighlightEasingCurveType)

    public:

        enum class Direction
        {
            Sent,
            Received
        };

        enum class AlignSent
        {
            Right,
            Left
        };

        using WidgetQFrame::WidgetQFrame;

        void setDirection(Direction direction, AlignSent alignSent=AlignSent::Left)
        {
            m_direction=direction;
            m_alignSent=alignSent;
            updateRight();
        }

        //! Change only the alignment of sent messages, e.g. in response to a live app setting or
        //! an AbstractChatMessagesView Auto-mode flip -- direction() (sent/received) is untouched.
        //! No-op if unchanged. Always re-runs updateAlignment() (not just when isRight() itself
        //! flips): a Received message's isRight() never changes regardless of alignSent (see
        //! updateRight()), but ChatMessage::updateAlignment() also reads alignSent() directly
        //! (forcing the avatar visible once sent and received messages share the same side), so
        //! it must still be recomputed for a Received message.
        void setAlignSent(AlignSent alignSent)
        {
            if (m_alignSent==alignSent)
            {
                return;
            }
            m_alignSent=alignSent;
            updateRight();
            updateAlignment();
        }

        AbstractChatSeparator* topSeparator() const noexcept
        {
            return m_topSeparator;
        }

        void setTopSeparator(AbstractChatSeparator* sep)
        {
            destroyWidget(m_topSeparator);
            m_topSeparator=sep;
            updateTopSeparator();
            emit topSeparatorUpdated();
        }

        bool isSelectionMode() const noexcept
        {
            return m_selectionMode;
        }

        bool isSelected() const noexcept
        {
            return m_selected;
        }

        AlignSent alignSent() const noexcept
        {
            return m_alignSent;
        }

        virtual void setAvatarPath(WithPath /*path*/) {}
        virtual WithPath avatarPath() const { return WithPath{};}

        virtual void setAvatarSource(std::shared_ptr<AvatarSource> /*avatarSource*/) {}
        virtual std::shared_ptr<AvatarSource> avatarSource() const {return std::shared_ptr<AvatarSource>{};}

        //! Sender name behind the avatar's initials fallback, drawn when the path resolves to no
        //! uploaded photo -- without it every avatar renders the generic "no name" placeholder
        //! instead (see AvatarWidget::setAvatarName()). Set it alongside setAvatarPath().
        virtual void setAvatarName(std::string /*name*/) {}

        //! The sender-avatar IMAGE widget itself (not the column it sits in). Used by
        //! ChatFloatingAvatar to copy path/source/name/size onto its own floating copy, and by
        //! ChatMessagesView::updateFloatingAvatar() to test this batch's anchored avatar's own
        //! on-screen rect -- only while it is still in its NATURAL (non-obscured) state, since
        //! setAvatarObscured(true) hides it and its geometry is not meaningful to query
        //! afterwards (see that method's own doc comment). isAvatarVisible() alone is not enough
        //! for that on-screen test -- it is setVisible(false) on every row except the batch's
        //! last, so isVisibleTo() against the viewport is what actually distinguishes "this row's
        //! avatar is on screen" from "row loaded but its avatar suppressed" (by either cause).
        //! Its size() stays correct even while hidden (a fixed size resizes eagerly), but its
        //! POSITION does not participate in row layout while hidden -- use avatarColumnWidget()
        //! for horizontal placement, never this. Null on a message type that renders no avatar.
        virtual AvatarWidget* avatarWidget() const {return nullptr;}

        //! The avatar COLUMN widget -- the fixed-width strip reserved beside the bubble, laid out
        //! on EVERY row of a batch regardless of whether that row's own avatar image is shown
        //! (its width tracks only the current avatar-visibility mode, not isLastInBatch()). The
        //! only reliable source of the avatar's horizontal position on a row whose own avatar
        //! image is suppressed. Null on a message type that renders no avatar.
        virtual QWidget* avatarColumnWidget() const {return nullptr;}

        //! Hide this row's own anchored avatar image (true), or show it again (false), INSTANTLY
        //! in both directions -- no animation either way. Used by
        //! ChatMessagesView::updateFloatingAvatar() so that at most one avatar is ever rendered
        //! at a given screen position: rather than fading ChatFloatingAvatar in/out as it nears
        //! this row's own anchored one (which flickered right at the handoff), the anchored one
        //! is switched off while the floating copy is over it and back on once it has moved off,
        //! with the floating copy itself just tracking smoothly throughout. The two sit in
        //! exactly the same place, so animating either side of the swap would be visible as a
        //! blink -- hence no animation.
        //!
        //! Implementations must leave the widget in its layout (ChatMessage does this with
        //! opacity, not setVisible()): updateFloatingAvatar() keeps reading the avatar's rect
        //! while it is obscured, both to park the floating copy and to test whether it is still
        //! covering it. No-op on a message type that renders no avatar.
        virtual void setAvatarObscured(bool /*obscured*/) {}

        bool isAvatarVisible() const noexcept
        {
            return m_avatarVisible;
        }

        Direction direction() const noexcept
        {
            return m_direction;
        }

        bool isIncoming() const noexcept
        {
            return m_direction==Direction::Received;
        }

        bool isContentVisible() const noexcept
        {
            return m_contentVisible;
        }

        void setContent(AbstractChatMessageContent* content)
        {
            destroyWidget(m_content);
            m_content=content;
            m_content->setChatMessage(this);
            updateContent();
            emit contentUpdated();
        }

        AbstractChatMessageContent* content() const noexcept
        {
            return m_content;
        }

        /**
         * @brief Widget the content bubble is ultimately parented to.
         *
         * Callers that build the content (and everything they nest inside it) should use this as
         * the parent rather than the message itself: the content ends up under this widget either
         * way, and building it elsewhere first means the whole subtree is reparented later, which
         * with an app-wide stylesheet runs QWidgetPrivate::inheritStyle() over every descendant
         * and re-polishes it against the QSS.
         *
         * Defaults to the message widget itself; subclasses that wrap the content in an
         * intermediate frame override this to return that frame.
         */
        virtual QWidget* contentParentWidget()
        {
            return this;
        }

        QDateTime datetime() const
        {
            return m_dateTime;
        }

        //! When this message was last edited, or an invalid QDateTime if it never was -- which is
        //! how "not edited" is expressed, there is no separate flag. Drives both the bottom row's
        //! "edited" marker and the created/edited tooltip shared by that marker and the time
        //! label, see ChatMessage::updateDateTime().
        QDateTime editedDatetime() const
        {
            return m_editedDateTime;
        }

        void setSelectDetectionBlocked(bool enable)
        {
            m_blockSelectDetection=enable;
        }

        bool isSelectDetectionBlocked() const noexcept
        {
            return m_blockSelectDetection;
        }

        void detectMouseSelection(std::optional<bool> select={});

        void setSelectorOnLeft(bool value)
        {
            m_selectorPositionLeft=value;
            emit selectorPositionChanged(value);
        }

        bool isSelectorOnLeft() const noexcept
        {
            return m_selectorPositionLeft;
        }

        virtual int bubbleOuterWidth() const =0;

        virtual QString selectedText() const {return QString{};}

        bool isFirstInBatch() const
        {
            return m_firstInBatch;
        }

        bool isLastInBatch() const
        {
            return m_lastInBatch;
        }

        bool isRight() const
        {
            return m_right;
        }

        QColor highlightColor() const noexcept
        {
            return m_highlightColor;
        }

        void setHighlightColor(const QColor& color) noexcept
        {
            m_highlightColor=color;
        }

        qreal highlightOpacity() const noexcept
        {
            return m_highlightOpacity;
        }

        void setHighlightOpacity(qreal value) noexcept
        {
            m_highlightOpacity=value;
        }

        int highlightHoldMs() const noexcept
        {
            return m_highlightHoldMs;
        }

        void setHighlightHoldMs(int value) noexcept
        {
            m_highlightHoldMs=value;
        }

        int highlightFadeMs() const noexcept
        {
            return m_highlightFadeMs;
        }

        void setHighlightFadeMs(int value) noexcept
        {
            m_highlightFadeMs=value;
        }

        int highlightEasingCurveType() const noexcept
        {
            return m_highlightEasingCurveType;
        }

        void setHighlightEasingCurveType(int value) noexcept
        {
            m_highlightEasingCurveType=value;
        }

        //! True for as long as any part of the highlight (hold or fade) is still visible.
        bool isHighlighted() const noexcept
        {
            return m_highlightFactor>0.0;
        }

    public slots:

        void toggleSelected()
        {
            setSelected(!isSelected());
        }

        void clearContentSelection()
        {
            if (m_content)
            {
                m_content->clearContentSelection();
            }
        }

        void setSelectionMode(bool enable)
        {
            if (m_selectionMode==enable)
            {
                return;
            }

            if (!enable)
            {
                setSelected(false);
                setSelectDetectionBlocked(false);
            }
            m_selectionMode=enable;
            updateSelectionMode();
            emit selectionModeUpdated();
        }

        void setSelected(bool enable)
        {
            if (m_selected==enable)
            {
                return;
            }
            clearContentSelection();
            m_selected=enable;
            updateSelection();
            emit selectionUpdated(m_selected);
        }

        void setLastInBatch(bool enable)
        {
            m_lastInBatch=enable;
            updateLastInBatch();
            emit lastInBatchUpdated();
        }

        void setFirstInBatch(bool enable)
        {
            m_firstInBatch=enable;
            updateFirstInBatch();
            emit firstInBatchUpdated();
        }

        void setContentVisible(bool enable)
        {
            m_contentVisible=enable;
            updateContentVisible();
            emit contentVisibilityUpdated();
        }

        void setAvatarVisible(bool enable)
        {
            m_avatarVisible=enable;
            updateAvatarVisible();
            emit avatarVisibilityUpdated();
        }

        void setDateTime(const QDateTime& dt)
        {
            m_dateTime=dt;
            updateDateTime();
            emit dateTimeUpdated();
        }

        //! Pass an invalid QDateTime (the default) for a message that has never been edited --
        //! see editedDatetime(). Routed through the same updateDateTime() hook as setDateTime()
        //! because the two are rendered together: the marker's visibility and the tooltip both
        //! depend on this value, and the tooltip is shared with the time label. Reuses
        //! dateTimeUpdated() rather than a new signal -- nothing needs to distinguish the two.
        void setEditedDateTime(const QDateTime& dt)
        {
            m_editedDateTime=dt;
            updateDateTime();
            emit dateTimeUpdated();
        }

        //! (Re)start the transient jump-to-message highlight: snaps to fully highlighted, holds
        //! for highlightHoldMs(), then fades to transparent over highlightFadeMs(). Safe to call
        //! again while already highlighted or fading -- restarts from full rather than stacking.
        //! Only an EXPLICIT jump (ChatMessages::jumpToMessage()) should ever call this -- see its
        //! own doc comment for why implicit jumps (jumpToDate()/jumpToFirstUnread()/
        //! jumpToEdge()/openLoad()) must not.
        void startHighlight();

        //! Cancel any running/pending highlight immediately, with no fade.
        void clearHighlight();

    signals:

        void topSeparatorUpdated();
        void selectionModeUpdated();
        void selectionUpdated(bool selected);
        void firstInBatchUpdated();
        void lastInBatchUpdated();
        void contentVisibilityUpdated();
        void contentUpdated();
        void avatarVisibilityUpdated();

        void dateTimeUpdated();

        void clicked();
        void selectionModeRequested();

        //! The sender's avatar itself was clicked (not the message row) -- a host typically opens
        //! that sender's character node. Reachable either while the anchored avatar is visible
        //! (see setAvatarVisible()/ChatMessage::updateAvatarForced()) or via a floating copy of it
        //! (ChatFloatingAvatar, forwarded by ChatMessagesView::updateFloatingAvatar()) while the
        //! anchored one has scrolled out of the viewport.
        void avatarClicked();

        void selectorPositionChanged(bool);

    protected:

        virtual void updateTopSeparator()
        {}

        virtual void updateSelectionMode()
        {}

        virtual void updateSelection()
        {}

        virtual void updateFirstInBatch()
        {}

        virtual void updateLastInBatch()
        {}

        virtual void updateContentVisible()
        {}

        virtual void updateContent()
        {
        }

        //! Side-dependent half of updateContent() -- left/right ordering of avatar and bubble,
        //! and the bubble/avatar "right" style property. Split out so setAlignSent() (which may
        //! fire repeatedly, e.g. on every resize in AbstractChatMessagesView Auto mode) can
        //! re-run only this, not the whole of updateContent() (which re-runs setContent() and
        //! would stack duplicate ChatMessageContentWrapper connections -- see chatmessage.cpp).
        virtual void updateAlignment()
        {}

        virtual void updateAvatarVisible()
        {}

        virtual void updateDateTime()
        {}

        //! Rect the highlight paints into, in this widget's own coordinates -- defaults to the
        //! whole widget. ChatMessage overrides this to exclude its top separator band (the
        //! date/unread pill), see ChatMessage::highlightRect().
        virtual QRect highlightRect() const
        {
            return rect();
        }

        void paintEvent(QPaintEvent* event) override;

    private:

        void ensureHighlightAnimation();
        void setHighlightFactor(qreal value);

        //! Recomputes m_right from the current m_direction/m_alignSent -- shared by
        //! setDirection() and setAlignSent(), neither of which relayouts on its own (see
        //! updateAlignment()).
        void updateRight() noexcept
        {
            m_right = (m_direction==Direction::Sent && m_alignSent==AlignSent::Right);
        }

        AbstractChatSeparator* m_topSeparator=nullptr;
        AbstractChatMessageContent* m_content=nullptr;
        bool m_selectionMode=false;
        bool m_selected=false;
        AlignSent m_alignSent=AlignSent::Left;
        Direction m_direction=Direction::Sent;
        bool m_firstInBatch=true;
        bool m_lastInBatch=true;
        bool m_contentVisible=true;
        bool m_avatarVisible=false;

        bool m_blockSelectDetection=false;
        bool m_selectorPositionLeft=true;

        bool m_right=false;

        QDateTime m_dateTime;
        QDateTime m_editedDateTime;

        QColor m_highlightColor{0,0,0};
        qreal m_highlightOpacity=0.05;
        int m_highlightHoldMs=400;
        int m_highlightFadeMs=1200;
        int m_highlightEasingCurveType=6; // QEasingCurve::OutCubic
        qreal m_highlightFactor=0.0;
        QVariantAnimation* m_highlightAnim=nullptr;
        SingleShotTimer* m_highlightHoldTimer=nullptr;
};

class UISE_DESKTOP_EXPORT AbstractChatMessageText : public AbstractChatMessageBody
{
    Q_OBJECT

    Q_PROPERTY(int maxBubbleWidth READ maxBubbleWidth WRITE setMaxBubbleWidth)

    public:

        constexpr static const int DefaultMaxBubbleWidth=600;

        using AbstractChatMessageBody::AbstractChatMessageBody;

        virtual void loadText(const QString& text, TextFormat format=TextFormat::Markdown) =0;

        virtual void clearText() =0;

        //! Hard cap on the width of rendered text, regardless of how much room the view offers --
        //! long lines are hard to read in a wide window. 0 disables the cap. Settable from QSS
        //! via qproperty-maxBubbleWidth (see chat.qss). A plain setter like
        //! AbstractChatMessageBottom::setNarrowBodyWidth(): changing it after the first layout
        //! takes effect on the next negotiation pass, or immediately via
        //! chatContent()->renegotiateBubbleWidth().
        void setMaxBubbleWidth(int width) noexcept
        {
            m_maxBubbleWidth=width;
        }

        int maxBubbleWidth() const noexcept
        {
            return m_maxBubbleWidth;
        }

    protected:

        //! Clamp a negotiation budget by maxBubbleWidth(), pass-through when the cap is disabled.
        int clampToMaxBubbleWidth(int width) const noexcept
        {
            return (m_maxBubbleWidth>0 && width>m_maxBubbleWidth) ? m_maxBubbleWidth : width;
        }

    private:

        int m_maxBubbleWidth=DefaultMaxBubbleWidth;
};

class UISE_DESKTOP_EXPORT AbstractChatMessageSelector : public WidgetQFrame
{
    Q_OBJECT

    public:

        using WidgetQFrame::WidgetQFrame;

        virtual bool isChecked() const =0;

    public slots:

        virtual void setChecked(bool enable) =0;

        void toggle()
        {
            setChecked(!isChecked());
        }

    signals:

        void toggled(bool checked);
};

inline void AbstractChatMessageChild::setChatMessage(AbstractChatMessage* chatMessage)
{
    // Deliberately does NOT reparent onto chatMessage. It used to, and every single caller then
    // had to undo it -- ChatMessageContent::updateWidgets() by re-adding the section to its own
    // layout, ChatSeparator::doInsertSection() likewise, ChatMessageComment/ChatMessageFiles::
    // updateChatMessage() likewise, and ChatMessageImages::updateChatMessage()/ensureComment()
    // with an explicit setParent(this). So the widget was moved up to the message and straight
    // back down again, and with an app-wide stylesheet each of those moves runs
    // QWidgetPrivate::inheritStyle() over the widget's whole descendant subtree, re-polishing
    // every widget in it against the QSS. Profiling put the pointless round trip at ~7% of total
    // process CPU while loading a chat.
    //
    // Widgets are created under the parent they belong to (see ui::ChatMessage::doInit(), which
    // builds every section under the content) and put into their final layout by the caller, so
    // this setter now only records the association it is named for.
    m_chatMessage=chatMessage;
    updateChatMessage();
}

}

#endif // UISE_DESKTOP_ABSTRACTCHATMESSAGE_HPP

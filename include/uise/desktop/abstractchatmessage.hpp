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

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/utils/destroywidget.hpp>
#include <uise/desktop/svgicon.hpp>
#include <uise/desktop/avatar.hpp>
#include <uise/desktop/frame.hpp>
#include <uise/desktop/utils/withpathandsize.hpp>
#include <uise/desktop/replypreviewdata.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class AvatarWidget;

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

        virtual void setComment(const QString& text, bool markdown=true) =0;
        virtual void clearComment() =0;
        virtual QString comment() const =0;

        virtual QString selectedText() const {return QString{};}

        //! See AbstractChatMessageBody::hasSelectableText() -- same contract, applied to this
        //! section's own embedded text instead of a body's.
        virtual bool hasSelectableText() const {return false;}

        //! See AbstractChatMessageBody::setCopyable().
        virtual void setCopyable(bool enable) {std::ignore=enable;}

        //! See AbstractChatMessageBody::selectText().
        virtual void selectText(const QString& /*text*/) {}

    signals:

        //! See AbstractChatMessageBody::selectionChanged().
        void selectionChanged();
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

    signals:

        //! Never emitted by the base class -- a body with genuine text selection (e.g.
        //! ChatMessageText, whose underlying QTextEdit already has this exact signal) connects
        //! it here, so a host (e.g. ReplyDialog's Save/"Quote selected" button swap) can react
        //! to selection changes without depending on a concrete body type. ChatMessageFiles/
        //! ChatMessageImages relay it from their optional embedded comment, if any. A body with no
        //! selectable content at all (ChatMessageCall) simply never fires it.
        void selectionChanged();
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

        QPointer<AbstractChatMessageHeader> m_header=nullptr;
        QPointer<AbstractChatMessageReply> m_reply=nullptr;
        QPointer<AbstractChatMessageBody> m_body=nullptr;
        QPointer<AbstractChatMessageComment> m_comment=nullptr;
        QPointer<AbstractChatMessageBottom> m_bottom=nullptr;

        bool m_selected=false;
        bool m_sent=false;

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

    public:

        enum class Direction : int
        {
            Sent,
            Received
        };

        enum class AlignSent : int
        {
            Right,
            Left
        };

        using WidgetQFrame::WidgetQFrame;

        void setDirection(Direction direction, AlignSent alignSent=AlignSent::Left)
        {
            m_direction=direction;
            m_alignSent=alignSent;

            m_right=false;
            if (m_direction==Direction::Sent && m_alignSent==AlignSent::Right)
            {
                m_right=true;
            }
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

        virtual void updateAvatarVisible()
        {}

        virtual void updateDateTime()
        {}

    private:

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
};

class UISE_DESKTOP_EXPORT AbstractChatMessageText : public AbstractChatMessageBody
{
    Q_OBJECT

    Q_PROPERTY(int maxBubbleWidth READ maxBubbleWidth WRITE setMaxBubbleWidth)

    public:

        constexpr static const int DefaultMaxBubbleWidth=600;

        using AbstractChatMessageBody::AbstractChatMessageBody;

        virtual void loadText(const QString& text, bool markdown=true) =0;

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

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_ABSTRACTCHATMESSAGE_HPP

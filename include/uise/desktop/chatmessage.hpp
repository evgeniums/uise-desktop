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

/** @file uise/desktop/chatmessage.hpp
*
*  Declares ChatMessage.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_CHATMESSAGE_HPP
#define UISE_DESKTOP_CHATMESSAGE_HPP

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/abstractchatmessage.hpp>

class QBoxLayout;
class QPainterPath;

UISE_DESKTOP_NAMESPACE_BEGIN

class CheckBox;

//--------------------------------------------------------------------------

class ChatSeparatorSection_p;

class UISE_DESKTOP_EXPORT ChatSeparatorSection : public AbstractChatSeparatorSection
{
    Q_OBJECT

    public:

        explicit ChatSeparatorSection(QWidget* parent=nullptr);

        ~ChatSeparatorSection();
        ChatSeparatorSection(const ChatSeparatorSection&)=delete;
        ChatSeparatorSection& operator=(const ChatSeparatorSection&)=delete;
        ChatSeparatorSection(ChatSeparatorSection&&)=delete;
        ChatSeparatorSection& operator=(ChatSeparatorSection&&)=delete;

        void setHLineVisible(bool enable) override;
        bool isHLineVisible() const override;

        void setText(const QString& text) override;
        QString text() const override;

        void setIconPath(WithPath path) override;
        WithPath iconPath() const override;

        void setIconSource(std::shared_ptr<AvatarSource> source) override;
        std::shared_ptr<AvatarSource> iconSource() const override;

        void setTailIcon(std::shared_ptr<SvgIcon> icon) override;
        std::shared_ptr<SvgIcon> tailIcon() const override;

        void setClickable(bool enable) override;
        bool isClickable() const override;

        QWidget* clickableWidget() override;

    private:

        std::unique_ptr<ChatSeparatorSection_p> pimpl;
};

//--------------------------------------------------------------------------

class UISE_DESKTOP_EXPORT ChatSeparator : public AbstractChatSeparator
{
    public:

        ChatSeparator(QWidget* parent=nullptr);

    protected:

        void doInsertSection(AbstractChatSeparatorSection* section, int index=-1) override;

    private:

        QBoxLayout* m_layout;
};

//--------------------------------------------------------------------------

class ChatMessageBottom_p;

class UISE_DESKTOP_EXPORT ChatMessageBottom : public AbstractChatMessageBottom
{
    Q_OBJECT

    public:

        explicit ChatMessageBottom(QWidget* parent=nullptr);

        ~ChatMessageBottom();
        ChatMessageBottom(const ChatMessageBottom&)=delete;
        ChatMessageBottom& operator=(const ChatMessageBottom&)=delete;
        ChatMessageBottom(ChatMessageBottom&&)=delete;
        ChatMessageBottom& operator=(ChatMessageBottom&&)=delete;

        void setSeen(const QString& text, const QString& tooltip={}) override;
        void setEdited(const QString& text, const QString& tooltip={}) override;
        void setTimeString(const QString& time, const QString& tooltip={}) override;
        void setStatusIcon(std::shared_ptr<SvgIcon> icon ={}, const QString& tooltip={}) override;

        int bubbleWidthHint(int forMaxWidth) override;

        QSize sizeHint() const override;

        virtual void setSelected(bool enable) override;
        virtual void setSent(bool enable) override;

    private:

        std::unique_ptr<ChatMessageBottom_p> pimpl;
};

//--------------------------------------------------------------------------

class UISE_DESKTOP_EXPORT ChatMessageContent : public AbstractChatMessageContent
{
    Q_OBJECT

    public:

        explicit ChatMessageContent(QWidget* parent=nullptr);

        void clearContentSelection() override;

        void setSelected(bool enable) override;
        void setSent(bool enable) override;

        //! Bubble corner-radius geometry (tail side) -- independent of setSent()'s colour, so a
        //! Sent message aligned to the left still gets a left-pointing tail. See chat.qss
        //! [right=...] rules.
        void setRight(bool enable) override;

    protected:

        void updateChatMessage() override;
        void updateWidgets() override;

    private slots:

        void updateFirstInBatch();
        void updateLastInBatch();

    private:

        QBoxLayout* m_layout;
};

class UISE_DESKTOP_EXPORT ChatMessageSelector : public AbstractChatMessageSelector
{
    Q_OBJECT

    public:

        explicit ChatMessageSelector(QWidget* parent=nullptr);

        bool isChecked() const override;

    public slots:

        void setChecked(bool enable) override;

    private:

        QBoxLayout* m_layout;
        CheckBox* m_checkBox;
};

class UISE_DESKTOP_EXPORT ChatMessageContentWrapper : public QFrame
{
    Q_OBJECT

    public:

        explicit ChatMessageContentWrapper(QWidget* parent=nullptr);

        AbstractChatMessageContent* content() const
        {
            return m_content;
        }

        void setContent(AbstractChatMessageContent* content);

        void setRight(bool enable)
        {
            m_right=enable;
            updatePosition();
        }

        bool isRight() const noexcept
        {
            return m_right;
        }

        QSize sizeHint() const override;

    public slots:

        void updatePosition();

    protected:

        bool eventFilter(QObject *obj, QEvent *event) override;

        void resizeEvent(QResizeEvent *event) override;

        void showEvent(QShowEvent *event) override;

    private:

        //! Move m_content to its aligned position without touching its size.
        //! Safe to call from resizeEvent() -- unlike updatePosition(), it can never trigger
        //! another resize of m_content, so it cannot re-enter this wrapper's own resizeEvent().
        void applyContentPosition();

        AbstractChatMessageContent* m_content=nullptr;
        bool m_right=false;
};

//--------------------------------------------------------------------------

//! Paints the chat bubble's "tail" itself (either a QPainterPath teardrop hook, or the original
//! quarter-disc crescent, selectable via tailShape) rather than faking it with an opaque #mask
//! child painted in the chat background's colour -- that trick only worked as long as
//! uise--AbstractChatMessagesView's own background stayed a single flat colour kept in lockstep
//! with #mask's (see light/chat.qss, dark/chat.qss); any texture, gradient or per-chat
//! background image on the view would show a mismatched slice through that opaque column instead
//! of tiling through it. tailColor/tailShape/tailWidth/tailHeight are qproperty-* knobs, same
//! convention as qproperty-highlightColor on AbstractChatMessage -- tailWidth in particular is
//! the TailShapeHook shape's thickness control (it has no effect on TailShapeRounded, whose
//! radius is tailHeight).
class UISE_DESKTOP_EXPORT ChatMessageAvatar : public QFrame
{
    Q_OBJECT

    Q_PROPERTY(QColor tailColor READ tailColor WRITE setTailColor)
    Q_PROPERTY(int tailShape READ tailShape WRITE setTailShape)
    Q_PROPERTY(int tailWidth READ tailWidth WRITE setTailWidth)
    Q_PROPERTY(int tailHeight READ tailHeight WRITE setTailHeight)
    //! Side of the (square) avatar image itself -- NOT this column's width, which
    //! ChatMessage::updateAvatarForced() sets separately (avatar plus its horizontal margins).
    //! Kept as a genuine qproperty so QSS can supply the baseline the way tailWidth/tailHeight do.
    Q_PROPERTY(int avatarSize READ avatarSize WRITE setAvatarSize)
    //! Distance in px from the message's bottom edge to the avatar's bottom edge -- i.e. how far
    //! ABOVE the tail the avatar sits. Keep it >= tailHeight or the avatar overlaps the tail.
    Q_PROPERTY(int avatarBottomOffset READ avatarBottomOffset WRITE setAvatarBottomOffset)

    public:

        //! Teardrop hook with a concave underside, hooking off the bubble's square corner.
        constexpr static const int TailShapeHook=0;
        //! The original crescent this class used to fake with an opaque #mask child: a plain
        //! concave quarter disc of radius tailHeight (tailWidth has no effect on this shape).
        constexpr static const int TailShapeRounded=1;

        constexpr static const int DefaultTailShape=TailShapeHook;
        constexpr static const int DefaultTailWidth=10;
        constexpr static const int DefaultTailHeight=16;
        constexpr static const int DefaultAvatarSize=16;
        //! Clears the default tail band (DefaultTailHeight, the bottom of this column -- see
        //! tailPath()) plus a small gap, so the avatar sits a little ABOVE the tail rather than
        //! overlapping it. chat.qss overrides this to match the tailHeight it actually sets.
        constexpr static const int DefaultAvatarBottomOffset=DefaultTailHeight+4;

        explicit ChatMessageAvatar(QWidget* parent=nullptr);

        void setRight(bool enable);
        void setSent(bool enable);
        void setSelected(bool enable);
        void setLastInBatch(bool enable);

        AvatarWidget* avatar() const
        {
            return m_avatar;
        }

        QColor tailColor() const noexcept
        {
            return m_tailColor;
        }

        void setTailColor(const QColor& color)
        {
            if (m_tailColor!=color)
            {
                m_tailColor=color;
                update();
            }
        }

        int tailShape() const noexcept
        {
            return m_tailShape;
        }

        void setTailShape(int value)
        {
            if (m_tailShape!=value)
            {
                m_tailShape=value;
                update();
            }
        }

        int tailWidth() const noexcept
        {
            return m_tailWidth;
        }

        void setTailWidth(int value)
        {
            if (m_tailWidth!=value)
            {
                m_tailWidth=value;
                update();
            }
        }

        int tailHeight() const noexcept
        {
            return m_tailHeight;
        }

        void setTailHeight(int value)
        {
            if (m_tailHeight!=value)
            {
                m_tailHeight=value;
                update();
            }
        }

        int avatarSize() const noexcept
        {
            return m_avatarSize;
        }

        //! Sizes the (square) avatar image only -- this column's own width is the caller's
        //! business (ChatMessage::updateAvatarForced() sets it to the avatar plus its margins).
        void setAvatarSize(int value)
        {
            if (m_avatarSize!=value)
            {
                m_avatarSize=value;
                m_avatar->setFixedSize(value,value);
                updateGeometry();
            }
        }

        int avatarBottomOffset() const noexcept
        {
            return m_avatarBottomOffset;
        }

        //! See the avatarBottomOffset property: distance from the message's bottom edge to the
        //! avatar's, applied as this frame's own bottom inset.
        void setAvatarBottomOffset(int value)
        {
            if (m_avatarBottomOffset!=value)
            {
                m_avatarBottomOffset=value;
                updateAvatarOffset();
                updateGeometry();
            }
        }

    protected:

        void paintEvent(QPaintEvent* event) override;

    private:

        void setStyleProperty(const char* name, bool enable);
        QPainterPath tailPath() const;

        //! Applies avatarBottomOffset() as this frame's bottom inset.
        void updateAvatarOffset();

        AvatarWidget* m_avatar;

        QColor m_tailColor;
        int m_tailShape=DefaultTailShape;
        int m_tailWidth=DefaultTailWidth;
        int m_tailHeight=DefaultTailHeight;
        //! 0/-1, not the Default* values: the ctor calls setAvatarSize()/setAvatarBottomOffset()
        //! explicitly to actually apply them (both setters no-op when the value is unchanged), so
        //! the stored defaults must start out different from what the ctor passes.
        int m_avatarSize=0;
        int m_avatarBottomOffset=-1;
        bool m_right=false;
        bool m_last=true;
};

//--------------------------------------------------------------------------

class ChatMessage_p;

class UISE_DESKTOP_EXPORT ChatMessage : public AbstractChatMessage
{
    Q_OBJECT

    public:

        //! Avatar image side (see ChatMessageAvatar::avatarSize()) once the avatar is forced
        //! visible -- updateAvatarForced() forces it whenever alignSent()==Left (sent and received
        //! messages share the same side, so position alone no longer distinguishes them) on the
        //! last message of a batch.
        constexpr static const int ForcedAvatarSize=32;
        //! Horizontal breathing room on EACH side of a forced-visible avatar: the column is this
        //! much wider than ForcedAvatarSize on both sides, and the avatar is centred in it.
        constexpr static const int ForcedAvatarMargin=6;

        explicit ChatMessage(QWidget* parent=nullptr);

        ~ChatMessage();
        ChatMessage(const ChatMessage&)=delete;
        ChatMessage& operator=(const ChatMessage&)=delete;
        ChatMessage(ChatMessage&&)=delete;
        ChatMessage& operator=(ChatMessage&&)=delete;

        int bubbleOuterWidth() const override;

        //! Returns the ChatMessageContentWrapper the content bubble lives in, so callers can
        //! build the content there directly instead of having it reparented by setContent().
        QWidget* contentParentWidget() override;

        void setAvatarPath(WithPath path) override;
        WithPath avatarPath() const override;

        void setAvatarSource(std::shared_ptr<AvatarSource> avatarSource) override;
        std::shared_ptr<AvatarSource> avatarSource() const override;

        void setAvatarName(std::string name) override;

        QString selectedText() const override;

    protected:

        //! Excludes #separatorFrame's band (the date/unread separator pill) -- covers
        //! #mainMessageFrame plus #bottomSpace only, see AbstractChatMessage::highlightRect()'s
        //! own doc comment.
        QRect highlightRect() const override;

        void updateTopSeparator() override;

        void updateSelectionMode() override;

        void updateSelection() override;

        void updateFirstInBatch() override;

        void updateLastInBatch() override;

        void updateContentVisible() override;

        void updateContent() override;

        void updateAlignment() override;

        void updateAvatarVisible() override;

        void updateDateTime() override;

        void mousePressEvent(QMouseEvent* event) override;

        //! Settles this row's own geometry the first time it is actually shown -- see this
        //! method's own doc comment (chatmessage.cpp) for why a message built/resized while its
        //! page was hidden can otherwise paint one frame too tall.
        void showEvent(QShowEvent* event) override;

        //! Re-derives the avatar's forced size/visibility after a QSS repolish -- see this
        //! method's own doc comment (chatmessage.cpp) for why chat.qss's qproperty-avatarSize
        //! default would otherwise silently win back over updateAvatarForced()'s own value.
        void changeEvent(QEvent* event) override;

        void construct() override;

    private:

        //! Builds the selection checkbox on first use and places it in the main layout.
        //!
        //! Deliberately NOT built in construct(): the selector is only ever shown in multi-select
        //! mode, which is off for the overwhelming majority of a message's life, while building it
        //! means building a whole uise::CheckBox (indicator parts, ripple overlay, animation,
        //! label, SVG icon lookup). Profiling a chat load found that hidden checkbox to be the
        //! single most expensive part of constructing a message bubble -- more than its entire
        //! content body -- at ~5% of total process CPU.
        void ensureSelector();

        //! Re-derives whether the sender's avatar is shown and how wide its column is.
        //! Visible only when alignSent()==Left (own and received messages share a side, so
        //! position alone no longer distinguishes them) AND isLastInBatch() (one avatar per
        //! batch, beside the bubble that carries the tail). The COLUMN's width tracks only the
        //! former, so bubbles stay aligned across a whole batch. Called from both
        //! updateAlignment() and updateLastInBatch(), the two inputs it reads.
        void updateAvatarForced();

        std::unique_ptr<ChatMessage_p> pimpl;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_CHATMESSAGE_HPP

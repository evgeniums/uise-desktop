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

    public:

        //! Teardrop hook with a concave underside, hooking off the bubble's square corner.
        constexpr static const int TailShapeHook=0;
        //! The original crescent this class used to fake with an opaque #mask child: a plain
        //! concave quarter disc of radius tailHeight (tailWidth has no effect on this shape).
        constexpr static const int TailShapeRounded=1;

        constexpr static const int DefaultTailShape=TailShapeHook;
        constexpr static const int DefaultTailWidth=10;
        constexpr static const int DefaultTailHeight=16;

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

    protected:

        void paintEvent(QPaintEvent* event) override;

    private:

        void setStyleProperty(const char* name, bool enable);
        QPainterPath tailPath() const;

        AvatarWidget* m_avatar;

        QColor m_tailColor;
        int m_tailShape=DefaultTailShape;
        int m_tailWidth=DefaultTailWidth;
        int m_tailHeight=DefaultTailHeight;
        bool m_right=false;
        bool m_last=true;
};

//--------------------------------------------------------------------------

class ChatMessage_p;

class UISE_DESKTOP_EXPORT ChatMessage : public AbstractChatMessage
{
    Q_OBJECT

    public:

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

        std::unique_ptr<ChatMessage_p> pimpl;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_CHATMESSAGE_HPP

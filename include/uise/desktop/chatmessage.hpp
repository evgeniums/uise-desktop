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
class QGraphicsOpacityEffect;
class QResizeEvent;
class QSpacerItem;

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

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

        //! Re-runs the owning bubble's negotiation after a content change here altered this
        //! row's own naturalSize() -- see its definition for why re-positioning alone is not
        //! enough. Shared by all four content setters.
        void refreshPlacement();

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

        //! Reacts to a CHANGE in AbstractChatMessageContent::isBubbleTransparent(): repolishes
        //! this bubble to [transparent=...] (chat.qss drops the background/border-radius rule for
        //! it), puts bottom() into/out of chip mode, and re-derives the row's hover visibility --
        //! see updateBottomVisibility().
        void applyBubbleTransparency(bool enable) override;

        //! Moves bottom() in/out of m_layout when isBottomInline() actually flipped since the
        //! last call -- see AbstractChatMessageContent::updateBottomPlacement()'s own doc
        //! comment for when/why this runs.
        void updateBottomPlacement() override;

        //! Places bottom() at its manual inline geometry, a no-op unless isBottomInline().
        void positionBottom() override;

        //! resize(sizeHint()) in setMaximumBubbleWidth() is a no-op when the size didn't change
        //! (e.g. inline mode with an unchanged trailing line, just a different QSS state) --
        //! this keeps the inline bottom's position current on every OTHER geometry change too
        //! (e.g. the wrapper resizing this bubble as part of alignment), same idiom as
        //! ChatMessageContentWrapper::resizeEvent()'s own move-only re-application.
        void resizeEvent(QResizeEvent* event) override;

        //! Resizes m_avatarSyncSpacer (index 0 of m_layout, ahead of header/reply/body/comment)
        //! to `pad` px -- see AbstractChatMessageContent::applyAvatarSyncPad()'s own doc comment.
        void applyAvatarSyncPad(int pad) override;

        //! Reveals bottom() while the pointer is over THIS bubble -- deliberately the bubble
        //! itself, not the full-width message row, so hovering the empty column beside a
        //! narrow/transparent bubble does not pop the chip. Moving from this widget onto one of
        //! its own children (an image tile, the chip itself) does not fire leaveEvent() -- Qt
        //! only sends Leave to a widget that stops being an ancestor of whatever is now under the
        //! pointer -- so no extra bookkeeping is needed to keep the chip up while hovering the
        //! content it sits over. Same idiom as ChatMessageReactionChip::enterEvent()/leaveEvent().
        void enterEvent(QEnterEvent* event) override;
        void leaveEvent(QEvent* event) override;

    private slots:

        void updateFirstInBatch();
        void updateLastInBatch();

    private:

        //! Shows/hides bottom() -- a no-op (always visible) unless isBubbleTransparent(), in
        //! which case it is visible only while m_hovered. Geometry is untouched either way:
        //! bottom() is never a layout item (see updateBottomPlacement()), and
        //! AbstractChatMessageBottom::placedSize() already reserves chip padding purely from
        //! isChipMode() -- a per-message state applyBubbleTransparency() sets once, not from
        //! whether the row happens to be visible right now -- so revealing/hiding it here can
        //! never shift the bubble.
        void updateBottomVisibility();

        //! Which side this bubble sits/points to -- read straight off the message rather than
        //! cached from setRight(), whose only two callers both pass exactly this (see
        //! updateChatMessage() and ChatMessage::updateAlignment()).
        bool isRightAligned() const;

        /**
         * @brief Alignment every section is given inside m_layout.
         *
         * Qt::AlignLeft as it always was, EXCEPT for a right-aligned transparent bubble, where it
         * is Qt::AlignRight. The bubble reserves room for the bottom row beside the content when
         * the row goes inline (AbstractChatMessageContent::evaluateInlineBottom()), and with no
         * background painted that reserved strip is simply invisible -- so on a right-aligned
         * message a left-aligned content would appear to float away from the margin every other
         * message lines up against, with a blank gap where the (hover-only) chip will be. Flipping
         * both the content and the chip puts that gap on the INSIDE of the conversation, where it
         * reads as ordinary spacing.
         *
         * Only ever affects body(): a transparent bubble by definition has no header/reply/comment
         * and an empty, hidden reactions row (see updateBubbleTransparency()).
         */
        Qt::Alignment sectionAlignment() const;

        //! Re-applies sectionAlignment() to every section currently in m_layout, for a change of
        //! transparency or side after updateWidgets() built it. bottom() is skipped -- it is
        //! never a layout item (see updateBottomPlacement()) and is placed by positionBottom().
        void applySectionAlignment();

        QBoxLayout* m_layout;

        //! Whether the pointer is currently over this bubble -- see enterEvent()/leaveEvent()
        //! and updateBottomVisibility(). Meaningless (never consulted) unless
        //! isBubbleTransparent().
        bool m_hovered=false;

        //! Whether bottom() is currently a child item of m_layout (row mode) or has been taken
        //! out of it and is positioned manually instead (inline mode) -- see
        //! updateBottomPlacement(). Starts true: updateWidgets() always (re-)adds bottom() to
        //! the layout on a fresh build, before any negotiation pass has had a chance to decide
        //! isBottomInline().
        bool m_bottomInLayout=true;

        //! Index-0 item of m_layout, reserving avatarSyncPad() px of blank space ahead of
        //! header/reply/body/comment -- see applyAvatarSyncPad(). Recreated by updateWidgets()
        //! (which clears m_layout entirely, along with every other item) at whatever pad
        //! avatarSyncPad() currently reports, so a rebuild triggered by setReply()/setComment()
        //! does not silently lose an already-settled pad; resized in place by
        //! applyAvatarSyncPad() the rest of the time.
        QSpacerItem* m_avatarSyncSpacer=nullptr;
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
    //! Side of the (square) avatar image CURRENTLY applied -- NOT this column's width, which
    //! ChatMessage::updateAvatarForced() sets separately (avatar plus its horizontal margins).
    //!
    //! Deliberately NOT the knob to tune from QSS: updateAvatarForced() rewrites this on every
    //! call with either forcedAvatarSize() (avatar forced visible) or DefaultAvatarSize (the
    //! narrow, avatar-hidden column), so a qproperty-avatarSize in a stylesheet only survives
    //! until the first of those runs -- which is once per message, before it is ever shown. It
    //! stays a qproperty only so the narrow column's own baseline is styleable and so a repolish
    //! restores a sane value; to change how big the avatar actually LOOKS, set
    //! qproperty-forcedAvatarSize below.
    Q_PROPERTY(int avatarSize READ avatarSize WRITE setAvatarSize)
    //! Distance in px from the message's bottom edge to the avatar's bottom edge -- i.e. how far
    //! ABOVE the tail the avatar sits. See the invariants on forcedAvatarSize below: together
    //! with it this sets the whole column's HEIGHT, which is what decides whether a short
    //! last-in-batch bubble gets padded at the top.
    Q_PROPERTY(int avatarBottomOffset READ avatarBottomOffset WRITE setAvatarBottomOffset)
    //! Side of the (square) avatar image once ChatMessage::updateAvatarForced() forces it
    //! visible -- i.e. the size actually seen in a chat, and the real QSS knob (this class reads
    //! it and never writes it, so unlike avatarSize a repolish can only ever restore it).
    //!
    //! Three invariants tie it to the rest of the geometry; chat.qss ships values that satisfy
    //! all three, and a host retuning any of them should re-check the others:
    //!
    //!   - forcedAvatarSize + avatarBottomOffset >= tailHeight, or tailPath() clamps the tail to
    //!     this column's own rect and the tip is cut short.
    //!   - forcedAvatarSize + avatarBottomOffset <= the natural height of a one-line bubble
    //!     (~33px at the default chat font), or the column is taller than the bubble beside it
    //!     and AbstractChatMessageContent::setMinimumBubbleHeight() reserves the shortfall as
    //!     blank space at the bubble's top -- which reads as a bigger gap in front of the last
    //!     message of every batch.
    //!   - for TailShapeRounded, the round avatar must stay INSIDE the concave disc tailPath()
    //!     carves (radius tailHeight, centred at the column's tail-side corner inset by it):
    //!     hypot(width/2-(width-tailHeight), tailHeight-avatarBottomOffset-forcedAvatarSize/2)
    //!     + forcedAvatarSize/2 <= tailHeight.
    Q_PROPERTY(int forcedAvatarSize READ forcedAvatarSize WRITE setForcedAvatarSize)
    //! Horizontal breathing room on EACH side of a forced-visible avatar: the column
    //! updateAvatarForced() builds is forcedAvatarSize+2*forcedAvatarMargin wide, with the
    //! avatar centred in it. Raise it together with a lowered forcedAvatarSize to shrink the
    //! avatar without moving the bubbles beside it.
    Q_PROPERTY(int forcedAvatarMargin READ forcedAvatarMargin WRITE setForcedAvatarMargin)

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

        //! Fallbacks for the two forcedAvatar* qproperties, used only by a host that loads no
        //! stylesheet rule for this class -- the shipped chat.qss supplies both.
        //!
        //! Note these C++ defaults alone do NOT satisfy the "column no taller than a one-line
        //! bubble" invariant documented on forcedAvatarSize: paired with
        //! DefaultAvatarBottomOffset (20, sized for the 16px DefaultTailHeight) the column is
        //! 44px tall. chat.qss is what brings the offset down to 6 and the column to 30. That
        //! pairing is deliberate: lowering DefaultAvatarBottomOffset here would clip the tail
        //! for any host running on the C++ defaults, which is the worse failure of the two.
        constexpr static const int DefaultForcedAvatarSize=24;
        constexpr static const int DefaultForcedAvatarMargin=6;

        explicit ChatMessageAvatar(QWidget* parent=nullptr);

        void setRight(bool enable);
        void setSent(bool enable);
        void setSelected(bool enable);
        void setLastInBatch(bool enable);

        //! Suppresses the tail (paintEvent() returns before filling tailPath()) while the bubble
        //! this avatar sits beside is transparent -- see AbstractChatMessageContent::
        //! isBubbleTransparent(). Deliberately a plain C++ member, not a [transparent=...] QSS
        //! rule on qproperty-tailColor: Qt never restores a qproperty whose rule stops matching
        //! (the same reason m_last, not a QSS rule, already gates the tail on [last=...] -- see
        //! this class' own doc comment above), so a recycled row would keep a stale tail colour.
        void setBubbleTransparent(bool enable);

        /**
         * @brief Hide/show the avatar image because a ChatFloatingAvatar is (or is no longer)
         * covering it -- see AbstractChatMessage::setAvatarObscured().
         *
         * Instant in both directions, no animation: the floating copy sits in exactly this
         * avatar's place, so the swap must not be visible at all.
         *
         * Implemented as opacity, NOT setVisible(): the widget must stay in its layout either
         * way, because ChatMessagesView::updateFloatingAvatar() keeps reading this avatar's
         * on-screen rect -- both to park the floating copy (it never floats BELOW the spot this
         * avatar occupies) and to test whether it is covering it -- and a hidden widget is
         * dropped from its layout and stops reporting a meaningful geometry. Testing a rect we
         * ourselves collapsed would read "not covered", release it, and re-cover it a frame
         * later, which is exactly the flicker this whole mechanism exists to remove.
         */
        void setAvatarObscured(bool obscured);

        bool isAvatarObscured() const noexcept
        {
            return m_avatarObscured;
        }

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
        //! business (ChatMessage::updateAvatarForced() sets it to forcedAvatarSize() plus
        //! forcedAvatarMargin() on each side). Called by that same method on every pass, so a
        //! value set from anywhere else (QSS included) does not survive -- see the avatarSize
        //! property's own doc comment.
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
                emit forcedAvatarGeometryChanged();
            }
        }

        int forcedAvatarSize() const noexcept
        {
            return m_forcedAvatarSize;
        }

        //! See the forcedAvatarSize property. Records the request only -- applying it (to the
        //! avatar image and to this column's width) is ChatMessage::updateAvatarForced()'s job,
        //! which the signal below asks it to redo.
        void setForcedAvatarSize(int value)
        {
            if (m_forcedAvatarSize!=value)
            {
                m_forcedAvatarSize=value;
                emit forcedAvatarGeometryChanged();
            }
        }

        int forcedAvatarMargin() const noexcept
        {
            return m_forcedAvatarMargin;
        }

        //! See the forcedAvatarMargin property -- same record-only contract as
        //! setForcedAvatarSize() above.
        void setForcedAvatarMargin(int value)
        {
            if (m_forcedAvatarMargin!=value)
            {
                m_forcedAvatarMargin=value;
                emit forcedAvatarGeometryChanged();
            }
        }

    signals:

        //! Any of the three inputs ChatMessage::updateAvatarForced() derives this column's
        //! geometry from (forcedAvatarSize, forcedAvatarMargin, avatarBottomOffset -- the last
        //! one because it is half of the column's height) has changed, typically because a
        //! stylesheet supplied a new qproperty value.
        //!
        //! Emitted rather than applied here because this class has no back-pointer to the
        //! ChatMessage that owns the column width and the bubble's minimum height. ChatMessage::
        //! construct() connects it QUEUED -- see the connection's own comment for why a direct
        //! one would read a half-applied QSS rule.
        void forcedAvatarGeometryChanged();

    protected:

        void paintEvent(QPaintEvent* event) override;

    private:

        void setStyleProperty(const char* name, bool enable);
        QPainterPath tailPath() const;

        //! Applies avatarBottomOffset() as this frame's bottom inset.
        void updateAvatarOffset();

        //! Builds the opacity effect setAvatarObscured() drives, on first use only -- the
        //! overwhelming majority of rows are never obscured, and a QGraphicsOpacityEffect forces
        //! its widget onto an offscreen-composited paint path for the rest of its life.
        void ensureOpacityEffect();

        AvatarWidget* m_avatar;

        QGraphicsOpacityEffect* m_opacityEffect=nullptr;
        bool m_avatarObscured=false;

        QColor m_tailColor;
        int m_tailShape=DefaultTailShape;
        int m_tailWidth=DefaultTailWidth;
        int m_tailHeight=DefaultTailHeight;
        //! 0/-1, not the Default* values: the ctor calls setAvatarSize()/setAvatarBottomOffset()
        //! explicitly to actually apply them (both setters no-op when the value is unchanged), so
        //! the stored defaults must start out different from what the ctor passes.
        int m_avatarSize=0;
        int m_avatarBottomOffset=-1;
        //! The Default* values, unlike the two above: these two are pure INPUTS -- nothing in
        //! C++ ever writes them, only a host or a stylesheet does -- so there is no ctor call
        //! whose no-op guard has to be dodged, and starting them anywhere else would just mean
        //! the first read is wrong. It is also why the qproperty-avatarSize trap documented in
        //! ChatMessage::changeEvent() cannot happen to them: a repolish that re-applies the QSS
        //! value restores exactly what updateAvatarForced() was already using.
        int m_forcedAvatarSize=DefaultForcedAvatarSize;
        int m_forcedAvatarMargin=DefaultForcedAvatarMargin;
        bool m_right=false;
        bool m_last=true;
        bool m_bubbleTransparent=false;
};

//--------------------------------------------------------------------------

class ChatMessage_p;

class UISE_DESKTOP_EXPORT ChatMessage : public AbstractChatMessage
{
    Q_OBJECT

    public:

        //! Compile-time DEFAULT for the avatar image's side once the avatar is forced visible --
        //! updateAvatarForced() forces it whenever alignSent()==Left (sent and received messages
        //! share the same side, so position alone no longer distinguishes them) on the last
        //! message of a batch.
        //!
        //! The value actually used is ChatMessageAvatar::forcedAvatarSize(), i.e. whatever
        //! qproperty-forcedAvatarSize the stylesheet supplies (chat.qss does); these two
        //! constants are only the no-stylesheet fallback, kept here under their original names
        //! because they are public API. See that property for the invariants they must satisfy.
        constexpr static const int ForcedAvatarSize=ChatMessageAvatar::DefaultForcedAvatarSize;
        //! Compile-time default for the horizontal breathing room on EACH side of a
        //! forced-visible avatar: the column is this much wider than the avatar on both sides,
        //! and the avatar is centred in it. Effective value:
        //! ChatMessageAvatar::forcedAvatarMargin().
        constexpr static const int ForcedAvatarMargin=ChatMessageAvatar::DefaultForcedAvatarMargin;

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

        AvatarWidget* avatarWidget() const override;
        QWidget* avatarColumnWidget() const override;
        void setAvatarObscured(bool obscured) override;

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

        void updateSenderHeaderVisible() override;

        void updateAlignment() override;

        void updateAvatarVisible() override;

        void updateDateTime() override;

        void mousePressEvent(QMouseEvent* event) override;

        //! Settles this row's own geometry the first time it is actually shown -- see this
        //! method's own doc comment (chatmessage.cpp) for why a message built/resized while its
        //! page was hidden can otherwise paint one frame too tall.
        void showEvent(QShowEvent* event) override;

        //! Re-derives the avatar's forced size/visibility after a QSS repolish, for two
        //! reasons -- see this method's own doc comment (chatmessage.cpp): chat.qss's
        //! qproperty-avatarSize default would otherwise silently win back over
        //! updateAvatarForced()'s own value, and a RELOADED stylesheet's new
        //! qproperty-forcedAvatarSize/forcedAvatarMargin has to reach this row (the avatar's own
        //! forcedAvatarGeometryChanged() covers every other path, but its setters no-op when a
        //! repolish re-applies an unchanged value).
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
        //!
        //! Its two SIZE inputs come from the avatar column's own qproperties --
        //! ChatMessageAvatar::forcedAvatarSize()/forcedAvatarMargin(), supplied by chat.qss --
        //! read after an ensurePolished() so a stylesheet value is never missed; changes to them
        //! arrive via ChatMessageAvatar::forcedAvatarGeometryChanged(), connected in construct().
        void updateAvatarForced();

        std::unique_ptr<ChatMessage_p> pimpl;
};

}

#endif // UISE_DESKTOP_CHATMESSAGE_HPP

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
#include <uise/desktop/frame.hpp>
#include <uise/desktop/utils/withpathandsize.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class AvatarSource;
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

        void setType(QString type)
        {
            m_type=std::move(type);
            setProperty("separator_type",type);
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
            if (index>=0 && index<m_sections.size())
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
            if (index<0 || index>=m_sections.size())
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

    private:

        AbstractChatMessageContent* m_content=nullptr;        
};

class UISE_DESKTOP_EXPORT AbstractChatMessageHeader : public ChatMessageContentSection
{
    Q_OBJECT

    public:

        using ChatMessageContentSection::ChatMessageContentSection;
};

class UISE_DESKTOP_EXPORT AbstractChatMessageBody : public ChatMessageContentSection
{
    Q_OBJECT

    public:

        using ChatMessageContentSection::ChatMessageContentSection;
};

class UISE_DESKTOP_EXPORT AbstractChatMessageBottom : public ChatMessageContentSection
{
    Q_OBJECT

    public:

        using ChatMessageContentSection::ChatMessageContentSection;

        virtual void setSeen(const QString& text, const QString& /*tooltip*/={}) {}
        virtual void setEdited(const QString& text, const QString& /*tooltip*/={}) {}
        virtual void setTimeString(const QString& /*time*/, const QString& /*tooltip*/={}) {}
        virtual void setStatusIcon(std::shared_ptr<SvgIcon> /*icon*/ ={}, const QString& /*tooltip*/={}) {}
};

inline int horizontalTotalMargin(const QFrame* frame)
{
    QMargins margins = frame->contentsMargins();
    return margins.left()+margins.right();
}

inline int horizontalTotalPadding(const QFrame* frame)
{
    QRect fullRect = frame->rect();
    QRect contentRect = frame->contentsRect();

    int paddingLeft = contentRect.left() - fullRect.left();
    int paddingRight = contentRect.right() - fullRect.right();

    return paddingLeft+paddingRight;
}

class UISE_DESKTOP_EXPORT AbstractChatMessageContent : public AbstractChatMessageChild
{
    Q_OBJECT

    public:

        using AbstractChatMessageChild::AbstractChatMessageChild;

        void setWidgets(AbstractChatMessageBody* body, AbstractChatMessageHeader* header=nullptr, AbstractChatMessageBottom* bottom=nullptr)
        {
            destroyWidget(m_header);
            m_header=header;
            if (m_header!=nullptr)
            {
                m_header->setChatMessage(chatMessage());
                m_header->setChatContent(this);
                m_sections.push_back(m_header);
            }
            destroyWidget(m_body);
            m_body=body;
            if (m_body!=nullptr)
            {
                m_body->setChatMessage(chatMessage());
                m_body->setChatContent(this);
                m_sections.push_back(m_body);
            }
            destroyWidget(m_bottom);
            m_bottom=bottom;
            if (m_bottom!=nullptr)
            {
                m_bottom->setChatMessage(chatMessage());
                m_bottom->setChatContent(this);
                m_sections.push_back(m_bottom);
            }
            updateWidgets();
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

        int maximumBubbleWidth() const noexcept
        {
            return m_maximumBubbleWidth;
        }

        virtual void setSelected(bool enable) =0;

        void updateBubbleWidth(int forMaxWidthIn);

        const auto& sections() const
        {
            return m_sections;
        }

        QSize sizeHint() const override;

    protected:

        virtual void updateWidgets() =0;

    private:

        QPointer<AbstractChatMessageHeader> m_header=nullptr;
        QPointer<AbstractChatMessageBody> m_body=nullptr;
        QPointer<AbstractChatMessageBottom> m_bottom=nullptr;

        std::vector<ChatMessageContentSection*> m_sections;
        int m_maximumBubbleWidth=0;

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

        virtual void setAvatarPath(const WithPath& /*path*/) {}
        virtual WithPath avatarPath() { return WithPath{};}

        virtual void setAvatarSource(std::shared_ptr<AvatarSource> /*avatarSource*/) {}
        virtual std::shared_ptr<AvatarSource> avatarSource() {return std::shared_ptr<AvatarSource>{};}

        bool isAvatarVisible(bool enable) const noexcept
        {
            return m_avatarVisible;
        }

        Direction direction() const noexcept
        {
            return m_direction;
        }

        void setTopSpaceVisible(bool enable)
        {
            m_topSpaceVisible=enable;
            updateTopSpaceVisible();
            emit topSpaceVisibilityUpdated();
        }

        bool isTopSpaceVisible() const noexcept
        {
            return m_topSpaceVisible;
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

        virtual int avatarWidth() const =0;
        virtual int selectorWidth() const =0;

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
        void topSpaceVisibilityUpdated();
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

        virtual void updateTopSpaceVisible()
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
        bool m_topSpaceVisible=true;
        bool m_lastInBatch=true;
        bool m_contentVisible=true;
        bool m_avatarVisible=false;

        bool m_blockSelectDetection=false;
        bool m_selectorPositionLeft=true;

        QDateTime m_dateTime;
};

class UISE_DESKTOP_EXPORT AbstractChatMessageText : public AbstractChatMessageBody
{
    Q_OBJECT

    public:

        using AbstractChatMessageBody::AbstractChatMessageBody;

        virtual void loadText(const QString& text, bool markdown=true) =0;

        virtual void clearText() =0;
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
    setParent(chatMessage);
    m_chatMessage=chatMessage;
    updateChatMessage();
}

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_ABSTRACTCHATMESSAGE_HPP

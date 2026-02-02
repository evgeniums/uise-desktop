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

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/utils/destroywidget.hpp>
#include <uise/desktop/svgicon.hpp>
#include <uise/desktop/widget.hpp>
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

    protected:

        virtual void updateChatMessage() {}

    private:

        AbstractChatMessage* m_chatMessage=nullptr;
};

class UISE_DESKTOP_EXPORT AbstractChatSeparatorSection : public AbstractChatMessageChild
{
    Q_OBJECT

    public:

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

    signals:

        void clicked();
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

    protected:

        virtual void doInsertSection(AbstractChatSeparatorSection* section, int index) =0;

    private:

        std::vector<QPointer<AbstractChatSeparatorSection>> m_sections;
};

class UISE_DESKTOP_EXPORT AbstractChatMessageHeader : public AbstractChatMessageChild
{
    Q_OBJECT

    public:

        using AbstractChatMessageChild::AbstractChatMessageChild;
};

class UISE_DESKTOP_EXPORT AbstractChatMessageBody : public AbstractChatMessageChild
{
    Q_OBJECT

    public:

        using AbstractChatMessageChild::AbstractChatMessageChild;
};

class UISE_DESKTOP_EXPORT AbstractChatMessageBottom : public AbstractChatMessageChild
{
    Q_OBJECT

    public:

        using AbstractChatMessageChild::AbstractChatMessageChild;
};

class UISE_DESKTOP_EXPORT AbstractChatMessageContent : public AbstractChatMessageChild
{
    Q_OBJECT

    public:

        using AbstractChatMessageChild::AbstractChatMessageChild;

        void setHeader(AbstractChatMessageHeader* header)
        {
            destroyWidget(m_header);
            m_header=header;
            if (m_header!=nullptr)
            {
                m_header->setChatMessage(chatMessage());
            }
            updateHeader();
        }

        AbstractChatMessageHeader* header() const noexcept
        {
            return m_header;
        }

        void setBody(AbstractChatMessageBody* body)
        {
            destroyWidget(m_body);
            m_body=body;
            if (m_body!=nullptr)
            {
                m_body->setChatMessage(chatMessage());
            }
            updateBody();
        }

        AbstractChatMessageBody* body() const noexcept
        {
            return m_body;
        }

        void setBottom(AbstractChatMessageBottom* bottom)
        {
            destroyWidget(m_bottom);
            m_bottom=bottom;
            if (m_bottom!=nullptr)
            {
                m_bottom->setChatMessage(chatMessage());
            }
            updateBottom();
        }

        AbstractChatMessageBottom* bottom() const noexcept
        {
            return m_bottom;
        }

    protected:

        virtual void updateHeader() =0;
        virtual void updateBody() =0;
        virtual void updateBottom() =0;

    private:

        QPointer<AbstractChatMessageHeader> m_header=nullptr;
        QPointer<AbstractChatMessageBody> m_body=nullptr;
        QPointer<AbstractChatMessageBottom> m_bottom=nullptr;
};

class UISE_DESKTOP_EXPORT AbstractChatMessage : public WidgetQFrame
{
    Q_OBJECT

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

        AbstractChatSeparator* topSeparator() const noexcept
        {
            return m_topSeparator;
        }

        void setTopSeparator(AbstractChatSeparator* header)
        {
            destroyWidget(m_topSeparator);
            m_topSeparator=header;
            updateTopSeparator();
            emit topSeparatorUpdated();
        }

        bool isSelectable() const noexcept
        {
            return m_selectable;
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

        void setDirection(Direction direction)
        {
            m_direction=direction;
            updateDirection();
            emit directionUpdated();
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

    public slots:

        void setSelectable(bool enable)
        {
            if (!enable)
            {
                setSelected(false);
            }
            m_selectable=enable;
            updateSelectable();
            emit selectableUpdated();
        }

        void setSelected(bool enable)
        {
            m_selected=enable;
            updateSelection();
            emit selectionUpdated();
        }

        void setAlignSent(AlignSent alignment)
        {
            m_alignSent=alignment;
            updateAlignSent();
            emit alignSentUpdated();;
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

    signals:

        void topSeparatorUpdated();
        void selectableUpdated();
        void selectionUpdated();
        void directionUpdated();
        void alignSentUpdated();
        void topSpaceVisibilityUpdated();
        void lastInBatchUpdated();
        void contentVisibilityUpdated();
        void contentUpdated();
        void avatarVisibilityUpdated();

    protected:

        virtual void updateTopSeparator()
        {}

        virtual void updateSelectable()
        {}

        virtual void updateSelection()
        {}

        virtual void updateAlignSent()
        {}

        virtual void updateDirection()
        {}

        virtual void updateTopSpaceVisible()
        {}

        virtual void updateLastInBatch()
        {}

        virtual void updateContentVisible()
        {}

        virtual void updateContent()
        {}

        virtual void updateAvatarVisible()
        {}

    private:

        AbstractChatSeparator* m_topSeparator=nullptr;
        AbstractChatMessageContent* m_content=nullptr;
        bool m_selectable=false;
        bool m_selected=false;
        AlignSent m_alignSent=AlignSent::Right;
        Direction m_direction=Direction::Received;
        bool m_topSpaceVisible=true;
        bool m_lastInBatch=true;
        bool m_contentVisible=true;
        bool m_avatarVisible=false;
};

class UISE_DESKTOP_EXPORT AbstractChatMessageText : public AbstractChatMessageBody
{
    Q_OBJECT

    public:

        using AbstractChatMessageBody::AbstractChatMessageBody;

        virtual void loadText(const QString& text, bool markdown=true) =0;
};

inline void AbstractChatMessageChild::setChatMessage(AbstractChatMessage* chatMessage)
{
    setParent(chatMessage);
    m_chatMessage=chatMessage;
    updateChatMessage();
}

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_ABSTRACTCHATMESSAGE_HPP

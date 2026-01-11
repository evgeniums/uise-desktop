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

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/svgicon.hpp>
#include <uise/desktop/widget.hpp>
#include <uise/desktop/utils/withpathandsize.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class AvatarSource;
class AvatarWidget;

class UISE_DESKTOP_EXPORT AbstractChatSeparatorSection : public WidgetQFrame
{
    Q_OBJECT

    public:

        using WidgetQFrame::WidgetQFrame;

        virtual void setHLineVisible(bool enable) =0;
        virtual bool isHLineVisible() const=0;

        virtual void setText(const QString& text) =0;
        virtual QString text() const =0;

        virtual void setIconPath(const WithPath& path) =0;
        virtual WithPath iconPath() =0;

        virtual void setIconSource(std::shared_ptr<AvatarSource> avatarSource) =0;
        virtual std::shared_ptr<AvatarSource> avatarSource() =0;

        virtual void setTailIcon(std::shared_ptr<SvgIcon> icon) =0;
        virtual std::shared_ptr<SvgIcon> tailIcon() const =0;

        virtual void setClickable(bool enable) =0;
        virtual bool isClickable() const=0;

    signals:

        void clicked();
};

class UISE_DESKTOP_EXPORT AbstractChatSeparator : public WidgetQFrame
{
    Q_OBJECT

    public:

        using WidgetQFrame::WidgetQFrame;

        virtual void insertSection(AbstractChatSeparatorSection* section, int index=-1) =0;

        void appendSection(AbstractChatSeparatorSection* section)
        {
            insertSection(section);
        }

        virtual AbstractChatSeparatorSection* section(int index) const =0;
};

class AbstractChatMessage;

class UISE_DESKTOP_EXPORT AbstractChatContent : public WidgetQFrame
{
    Q_OBJECT

    public:

        using WidgetQFrame::WidgetQFrame;

        void setParentMessage(AbstractChatMessage* parentMessage)
        {
            m_parentMessage=parentMessage;
            updateParentMessage();
        }

    protected:

        virtual void updateParentMessage() =0;

    private:

        AbstractChatMessage* m_parentMessage=nullptr;
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

        using WidgetQFrame::WidgetQFrame;

        AbstractChatSeparator* topSeparator() const noexcept
        {
            return m_topSeparator;
        }

        void setTopSeparator(AbstractChatSeparator* header)
        {
            delete m_topSeparator;
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

        Qt::Alignment alignment() const noexcept
        {
            return m_alignment;
        }

        virtual void setAvatarPath(const WithPath& path) =0;
        virtual WithPath avatarPath() =0;

        virtual void setAvatarSource(std::shared_ptr<AvatarSource> avatarSource) =0;
        virtual std::shared_ptr<AvatarSource> avatarSource() =0;

        virtual bool isAvatarVisible(bool enable) const =0;

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

        bool isBodyVisible() const noexcept
        {
            return m_topSpaceVisible;
        }

        void setContent(AbstractChatContent* content)
        {
            delete m_content;
            m_content=content;
            m_content->setParentMessage(this);
            updateContent();
            emit contentUpdated();
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

        void setAlignment(Qt::Alignment alignment)
        {
            m_alignment=alignment;
            updateAlignment();
            emit alignmentUpdated();;
        }

        void setLastInBatch(bool enable)
        {
            m_lastInBatch=enable;
            updateLastInBatch();
            emit lastInBatchUpdated();
        }

        void setBodyVisible(bool enable)
        {
            m_bodyVisible=enable;
            updateBodyVisible();
            emit bodyVisibilityUpdated();
        }

        virtual void setAvatarVisible(bool enable) =0;

    signals:

        void topSeparatorUpdated();
        void selectableUpdated();
        void selectionUpdated();
        void directionUpdated();
        void alignmentUpdated();
        void topSpaceVisibilityUpdated();
        void lastInBatchUpdated();
        void bodyVisibilityUpdated();
        void contentUpdated();

    protected:

        virtual void updateTopSeparator()
        {}

        virtual void updateSelectable()
        {}

        virtual void updateSelection()
        {}

        virtual void updateAlignment()
        {}

        virtual void updateDirection()
        {}

        virtual void updateTopSpaceVisible()
        {}

        virtual void updateLastInBatch()
        {}

        virtual void updateBodyVisible()
        {}

        virtual void updateContent()
        {}

    private:

        AbstractChatSeparator* m_topSeparator=nullptr;
        bool m_selectable=false;
        bool m_selected=false;
        Qt::Alignment m_alignment=Qt::AlignLeft;
        Direction m_direction=Direction::Received;
        bool m_topSpaceVisible=true;
        bool m_lastInBatch=true;
        bool m_bodyVisible=true;
        AbstractChatContent* m_content=nullptr;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_ABSTRACTCHATMESSAGE_HPP

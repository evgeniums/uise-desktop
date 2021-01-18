/**
@copyright Evgeny Sidorov 2021

This software is dual-licensed. Choose the appropriate license for your project.

1. For open source projects this software is distrubuted under the GNU GENERAL PUBLIC LICENSE, Version 3.0.
(See accompanying file LICENSE-GPLv3.md or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. For proprietary license contact evgeniums@dracosha.com.

*/

/****************************************************************************/

/** \file uise/desktop/linkedlistviewitem.hpp
*
*  Defines LinkedListViewItem.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_LINKEDLISTVIEWITEM_HPP
#define UISE_DESKTOP_LINKEDLISTVIEWITEM_HPP

#include <memory>

#include <QPointer>
#include <QWidget>

#include <uise/desktop/uisedesktop.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class UISE_DESKTOP_EXPORT LinkedListViewItem : public std::enable_shared_from_this<LinkedListViewItem>
{
    public:

        constexpr static const char* Property="uise_dt_LinkedListViewItem";

        LinkedListViewItem(QWidget* widget) : m_widget(widget), m_pos(0)
        {
        }

        void keepInWidgetProperty();
        static std::shared_ptr<LinkedListViewItem> getFromWidgetProperty(QObject* widget);
        static void clearWidgetProperty(QObject* widget);

        auto next() const noexcept
        {
            return m_next.lock();
        }

        void setNext(std::shared_ptr<LinkedListViewItem> item) noexcept
        {
            m_next=std::move(item);
        }

        auto prev() const noexcept
        {
            return m_prev.lock();
        }

        void setPrev(std::shared_ptr<LinkedListViewItem> item) noexcept
        {
            m_prev=std::move(item);
        }

        void setNextAuto(std::shared_ptr<LinkedListViewItem> item) noexcept
        {
            auto next=m_next.lock();
            if (next)
            {
                next->setPrev(std::shared_ptr<LinkedListViewItem>());
            }
            m_next=std::move(item);
            next=m_next.lock();
            if (next)
            {
                next->setPrev(shared_from_this());
            }
        }

        void setPrevAuto(std::shared_ptr<LinkedListViewItem> item) noexcept
        {
            auto prev=m_prev.lock();
            if (prev)
            {
                prev->setNext(std::shared_ptr<LinkedListViewItem>());
            }
            m_prev=std::move(item);
            prev=m_prev.lock();
            if (prev)
            {
                prev->setNext(shared_from_this());
            }
        }

        QWidget* widget() noexcept
        {
            return m_widget;
        }

        size_t pos() const noexcept
        {
            return m_pos;
        }

        void setPos(size_t pos) noexcept
        {
            m_pos=pos;
        }

        void incPos() noexcept
        {
            ++m_pos;
        }

        void decPos() noexcept
        {
            if (m_pos!=0)
            {
                --m_pos;
            }
        }

        void reset()
        {
            m_pos=0;
            m_next.reset();
            m_prev.reset();;
        }

    private:

        QPointer<QWidget> m_widget;
        std::weak_ptr<LinkedListViewItem> m_next;
        std::weak_ptr<LinkedListViewItem> m_prev;
        size_t m_pos;
};

using LinkedListViewItemSharedPtr=std::shared_ptr<LinkedListViewItem>;

UISE_DESKTOP_NAMESPACE_EMD

Q_DECLARE_METATYPE(UISE_DESKTOP_NAMESPACE::LinkedListViewItemSharedPtr)

#endif // UISE_DESKTOP_LINKEDLISTVIEWITEM_HPP

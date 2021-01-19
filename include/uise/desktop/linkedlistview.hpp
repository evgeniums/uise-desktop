/**
@copyright Evgeny Sidorov 2021

This software is dual-licensed. Choose the appropriate license for your project.

1. For open source projects this software is distrubuted under the GNU GENERAL PUBLIC LICENSE, Version 3.0.
(See accompanying file LICENSE-GPLv3.md or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. For proprietary license contact evgeniums@dracosha.com.

*/

/****************************************************************************/

/** \file uise/desktop/linkedlistview.hpp
*
*  Defines LinkedListView.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_LINKEDLISTVIEW_HPP
#define UISE_DESKTOP_LINKEDLISTVIEW_HPP

#include <memory>
#include <vector>

#include <QFrame>

#include <uise/desktop/uisedesktop.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

namespace detail
{
    class LinkedListView_p;
}

class UISE_DESKTOP_EXPORT LinkedListView : public QFrame
{
    Q_OBJECT

    public:

        explicit LinkedListView(QWidget* parent=nullptr, Qt::Orientation orientation=Qt::Vertical);

        ~LinkedListView();
        LinkedListView(const LinkedListView&)=delete;
        LinkedListView& operator= (const LinkedListView&)=delete;
        LinkedListView(LinkedListView&&)=delete;
        LinkedListView& operator= (LinkedListView&&)=delete;

        void insertWidgetAfter(QWidget* newWidget, QWidget* existingWidget=nullptr);
        void insertWidgetBefore(QWidget* newWidget, QWidget* existingWidget=nullptr);

        void insertWidgetsAfter(const std::vector<QWidget*>& newWidgets, QWidget* existingWidget=nullptr);
        void insertWidgetsBefore(const std::vector<QWidget*>& newWidgets, QWidget* existingWidget=nullptr);

        void takeWidget(QObject* widget);

        void clear();

        Qt::Orientation orientation() const noexcept;
        void setOrientation(Qt::Orientation orientation);

    signals:

        void resized();

    protected:

        void resizeEvent(QResizeEvent* event) override;

    private slots:

        void itemDestroyed(QObject* widget);

    private:

        std::unique_ptr<detail::LinkedListView_p> pimpl;

        friend class detail::LinkedListView_p;
};

UISE_DESKTOP_NAMESPACE_EMD

#endif // UISE_DESKTOP_LINKEDLISTVIEW_HPP

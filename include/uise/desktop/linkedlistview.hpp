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

/** @file uise/desktop/linkedlistview.hpp
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
#include <uise/desktop/utils/enums.hpp>
#include <uise/desktop/utils/destroywidget.hpp>

// Comparison switch: define this to build the original QBoxLayout-based
// implementation of LinkedListView instead of the manual-positioning one.
// Undefine (default) to build the current implementation. Must be defined
// consistently for linkedlistview.hpp and linkedlistview.cpp -- since the
// .cpp includes this header, toggling the line below is enough.
#define UISE_DESKTOP_LINKEDLISTVIEW_LEGACY_LAYOUT

UISE_DESKTOP_NAMESPACE_BEGIN

namespace detail
{
    class LinkedListView_p;
}

class UISE_DESKTOP_EXPORT LinkedListView : public QFrame
{
    Q_OBJECT

    public:

        using DropWidgetHandler=std::function<void (QWidget*)>;

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

        void takeWidget(QObject* widget, bool destroyed=false);
        size_t widgetSeqPos(QObject* widget) const;
        QWidget* widgetAtSeqPos(size_t pos) const;

        void clear(const DropWidgetHandler& dropWidget=destroyWidget);

        Qt::Orientation orientation() const noexcept;
        void setOrientation(Qt::Orientation orientation);

        void setAlignment(Qt::Alignment alignment) noexcept;
        Qt::Alignment alignment() const noexcept;

#ifndef UISE_DESKTOP_LINKEDLISTVIEW_LEGACY_LAYOUT
        QSize sizeHint() const override;
        QSize minimumSizeHint() const override;
#endif

    signals:

        void resized();

    protected:

        void resizeEvent(QResizeEvent* event) override;
#ifndef UISE_DESKTOP_LINKEDLISTVIEW_LEGACY_LAYOUT
        bool event(QEvent* event) override;
#endif

    private slots:

        void itemDestroyed(QObject* widget);

    private:

        std::unique_ptr<detail::LinkedListView_p> pimpl;

        friend class detail::LinkedListView_p;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_LINKEDLISTVIEW_HPP

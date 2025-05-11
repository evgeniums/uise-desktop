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

/** @file uise/desktop/stackwithnavigationbar.hpp
*
*  Declares stacked widgets container with navigation bar.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_STACKED_NAVBAR_HPP
#define UISE_DESKTOP_STACKED_NAVBAR_HPP

#include <memory>

#include <QFrame>

#include <uise/desktop/uisedesktop.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class FrameWithRefresh;
class NavigationBar;

class StackWithNavigationBar_p;

/**
 * @brief Stacked widget with navigation bar.
 *
 * Stacked widget contains ordered widget that are referenced by navigation bar.
 * Selecting item in the navigation bar raises corresponding item to the top.
 *
 * Widgets can be added only sequentially. Removing a widget truncates all subsequent widgets from the stack.
 */
class UISE_DESKTOP_EXPORT StackWithNavigationBar : public QFrame
{
    Q_OBJECT

    public:

        static const int DefaultMaxWidthPercent=50;
        static const int DefaultMaxHeightPercent=50;
        static const int DefaultPopupAlpha=150;

        /**
         * @brief Constructor.
         * @param parent Parent widget.
         */
        StackWithNavigationBar(QWidget* parent=nullptr);

        /**
         * @brief Destructor.
         */
        ~StackWithNavigationBar();

        StackWithNavigationBar(const StackWithNavigationBar&)=delete;
        StackWithNavigationBar(StackWithNavigationBar&&)=delete;
        StackWithNavigationBar& operator=(const StackWithNavigationBar&)=delete;
        StackWithNavigationBar& operator=(StackWithNavigationBar&&)=delete;

        /**
         * @brief Add widget to the stack.
         * @param widget Widget to add.
         * @param name Name of the widget to display in corresponding item of the navigation bar.
         * @param tooltip Tooltip to display when hovering over corresponding item of the navigation bar.
         */
        void addWidget(FrameWithRefresh* widget, const QString& name, const QString& tooltip=QString());

        /**
         * @brief Replace a widget in the stack.
         *        Replacing a widget truncates all subsequent widgets starting from the given index.
         *        If index exceeds index of the last widget in the stack then new widget will be added to the end of the stack.
         * @param widget New widget.
         * @param name Name of the widget to display in corresponding item of the navigation bar.
         * @param index Index of the widget to replace.
         * @param tooltip Tooltip to display when hovering over corresponding item of the navigation bar.
         */
        void replaceWidget(FrameWithRefresh* widget, const QString& name, int index, const QString& tooltip=QString());

        /**
         * @brief Get number of widgets in the stack.
         * @return Operation result.
         */
        size_t count() const;

        /**
         * @brief Get navigation bar.
         * @return Navigation bar.
         */
        NavigationBar* navigationBar() const;

        /**
         * @brief Truncate widgets from the stack and destroy them.
         * @param index Inclusive index to truncate items starting from.
         */
        void truncate(int index);

        /**
         * @brief Get index of currently selected widget.
         * @return Operation result.
         */
        int currentIndex() const;

        /**
         * @brief Get currently selected widget.
         * @return Operation result.
         */
        FrameWithRefresh* currentWidget() const;

        /**
         * @brief Find widget by index.
         * @param index Index to find widget for.
         * @return Found widget or nullptr if index exceeds count of widgets.
         */
        FrameWithRefresh* widget(int index) const;

        /**
         * @brief Set refresh on select mode. In that mode refresh() slot of the widget will be called when it is selected.
         * @param enable
         */
        void setRefreshOnSelect(bool enable) noexcept;
        bool isRefreshOnSelect() const noexcept;

    public slots:

        /**
         * @brief Close widget. Same as truncate().
         * @param index Index of the widget to close.
         */
        void closeWidget(int index);

        /**
         * @brief Raise widget with selected index.
         * @param index Index to select.
         */
        void setCurrentIndex(int index);

        /**
         * @brief Raise widget.
         * @param widget Wisget to select.
         */
        void setCurrentWidget(UISE_DESKTOP_NAMESPACE::FrameWithRefresh* widget);

        /**
         * @brief Remove all widgets from the stack and destroy them.
         */
        void clear();

    signals:

        void widgetSelected(UISE_DESKTOP_NAMESPACE::FrameWithRefresh* widget);

        void currentChanged(int index);

    private:

        friend class NavigationBar;

        std::unique_ptr<StackWithNavigationBar_p> pimpl;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_STACKED_NAVBAR_HPP

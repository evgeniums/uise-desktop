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
#include <QToolButton>
#include <QLabel>

#include <uise/desktop/uisedesktop.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class UISE_DESKTOP_EXPORT FrameWithRefresh : public QFrame
{
    Q_OBJECT

    public:

        using QFrame::QFrame;

        void refresh()
        {
            emit refreshRequested();
        }

    signals:

        void refreshRequested();
};

class UISE_DESKTOP_EXPORT NavigationBarPanel : public QFrame
{
    Q_OBJECT

    public:

        using QFrame::QFrame;
};

class UISE_DESKTOP_EXPORT NavigationBarButton : public QToolButton
{
    Q_OBJECT

    public:

        NavigationBarButton(QWidget* parent=nullptr);

    signals:

        void shown();

    protected:

        void enterEvent(QEnterEvent * event) override;
        void showEvent(QShowEvent * event) override;

    private:

        bool m_firstShow;
};

class UISE_DESKTOP_EXPORT NavigationBarSeparator : public QLabel
{
    Q_OBJECT

    public:

        NavigationBarSeparator(QWidget* parent=nullptr);
};

class NavigationBar_p;

class StackWithNavigationBar;

class UISE_DESKTOP_EXPORT NavigationBar : public QFrame
{
    Q_OBJECT

    public:

        /**
         * @brief Constructor.
         * @param parent Parent frame whith popup widget.
         */
        NavigationBar(StackWithNavigationBar* parent=nullptr);

        ~NavigationBar();

        NavigationBar(const NavigationBar&)=delete;
        NavigationBar(NavigationBar&&)=delete;
        NavigationBar& operator=(const NavigationBar&)=delete;
        NavigationBar& operator=(NavigationBar&&)=delete;

        void addWidget(const QString& name, int index, const QString& tooltip=QString());

        void setWidgetName(int index, const QString& name);
        void setWidgetTooltip(int index, const QString& tooltip);

        void setCurrentIndex(int index);

        void clear();

        void clearFromIndex(int index);

    signals:

        void indexSelected(int index);

    protected:

        void resizeEvent(QResizeEvent* event) override;

    private:

        std::unique_ptr<NavigationBar_p> pimpl;
};

class StackWithNavigationBar_p;

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

        void addWidget(FrameWithRefresh* widget, const QString& name, const QString& tooltip=QString());

        void replaceWidget(FrameWithRefresh* widget, const QString& name, int index, const QString& tooltip=QString());

        size_t count() const;

        void setWidgetName(int index, const QString& name);

        void setWidgetTooltip(int index, const QString& tooltip);

        void clearFromIndex(int index);

        int currentIndex() const;

        FrameWithRefresh* currentWidget() const;
        FrameWithRefresh* widget(int index) const;

        void setRefreshOnSelect(bool enable) noexcept;
        bool isRefreshOnSelect() const noexcept;

    public slots:

        void closeWidget(int index);

        void setCurrentIndex(int index);

        void setCurrentWidget(UISE_DESKTOP_NAMESPACE::FrameWithRefresh* widget);

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

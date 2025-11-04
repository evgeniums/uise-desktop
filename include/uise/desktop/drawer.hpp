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

/** @file uise/desktop/drawer.hpp
*
*  Declares frame with drawer control.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_DRAWER_HPP
#define UISE_DESKTOP_DRAWER_HPP

#include <memory>

#include <QFrame>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/utils/orientationinvariant.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class Drawer_p;

class FrameWithDrawer;

class UISE_DESKTOP_EXPORT Drawer : public QFrame
{
    Q_OBJECT

    public:

        /**
         * @brief Constructor.
         * @param parent Parent frame whith drawer.
         */
        Drawer(FrameWithDrawer* parent=nullptr);

        ~Drawer();

        Drawer(const Drawer&)=delete;
        Drawer(Drawer&&)=delete;
        Drawer& operator=(const Drawer&)=delete;
        Drawer& operator=(Drawer&&)=delete;

        void setWidget(QWidget* widget);

        void openDrawer();
        void closeDrawer(bool immediate=false);

    protected:

        void resizeEvent(QResizeEvent *event) override;
        void mousePressEvent(QMouseEvent *event) override;

    private:

        friend class FrameWithDrawer;

        void updateDrawerGeometry();

        std::unique_ptr<Drawer_p> pimpl;
};

class FrameWithDrawer_p;

/**
 * @brief FrameWithDrawer enables drawer control over widget.
 */
class UISE_DESKTOP_EXPORT FrameWithDrawer : public QFrame,
                                            public OrientationInvariant
{
    Q_OBJECT

    public:

        static const int DefaultSizePercent=30;
        static const int DefaultAlpha=30;
        static const int DefaultSlideDurationMs=150;

        /**
         * @brief Constructor.
         * @param parent Parent widget.
         */
        FrameWithDrawer(QWidget* parent=nullptr);

        /**
         * @brief Destructor.
         */
        ~FrameWithDrawer();

        FrameWithDrawer(const FrameWithDrawer&)=delete;
        FrameWithDrawer(FrameWithDrawer&&)=delete;
        FrameWithDrawer& operator=(const FrameWithDrawer&)=delete;
        FrameWithDrawer& operator=(FrameWithDrawer&&)=delete;

        /**
         * @brief Set drawer widget.
         * @param widget Widget to use within drawer.
         */
        void setDrawerWidget(QWidget* widget);

        /**
         * @brief Show drawer.
         */
        void openDrawer();

        /**
         * @brief Close drawer.
         * @param immediate If true then close immediately wothout animation effect.
         */
        void closeDrawer(bool immediate=false);

        /**
         * @brief Toggle visibility of drawer.
         * @param enable If true then show drawer.
         */
        void setDrawerVisible(bool enable);

        /**
         * @brief Check if drawer is visible.
         * @return Operation result.
         */
        bool isDrawerVisible() const;

        /**
         * @brief Set percent of parent window size the drawer can occupy.
         * @param val
         */
        void setSizePercent(int val);

        /**
         * @brief Get percent of parent window size the drawer can occupy.
         * @return Requested value.
         */
        int sizePercent() const;

        /**
         * @brief Set duration of drawer sliding.
         * @param val Duration in milliseconds.
         */
        void setSlideDurationMs(int val);

        /**
         * @brief Get duration of drawer sliding.
         * @return Duration in milliseconds.
         */
        int slideDurationMs() const;

        /**
         * @brief Set alpha channel of drawer background color.
         * @param val
         */
        void setDrawerAlpha(int val);

        /**
         * @brief Get alpha channel of drawer background color.
         * @return Operation result.
         */
        int drawerAlpha() const;

        /**
         * @brief Check if drawer is in horizontal oriantation.
         * @return Operation result.
         */
        virtual bool isHorizontal() const noexcept override;

        /**
         * @brief Set widget edge the drawer must be shown at.
         * @param val Widget edge.
         */
        void setDrawerEdge(Qt::Edge val);

        /**
         * @brief Get widget edge the drawer must be shown at.
         * @return Operation result.
         */
        Qt::Edge drawerEdge() const noexcept;

    signals:

        void drawerShown();
        void drawerHidden();
        void drawerVisibilityChanged(bool val);

    protected:

        void resizeEvent(QResizeEvent *event) override;

    private:

        void afterDrawerHidden();
        void afterDrawerVisible();

        friend class Drawer;

        std::unique_ptr<FrameWithDrawer_p> pimpl;
};


UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_DRAWER_HPP

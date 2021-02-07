/**
@copyright Evgeny Sidorov 2021

This software is dual-licensed. Choose the appropriate license for your project.

1. For open source projects this software is distrubuted under the GNU GENERAL PUBLIC LICENSE, Version 3.0.
(See accompanying file LICENSE-GPLv3.md or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. For proprietary license contact evgeniums@dracosha.com.

*/

/****************************************************************************/

/** @file uise/desktop/alignedstretchingwidget.hpp
*
*  Declares AlignedStretchingWidget.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_ALIGNEDSTRETCHINGWIDGET_HPP
#define UISE_DESKTOP_ALIGNEDSTRETCHINGWIDGET_HPP

#include <QFrame>
#include <QPointer>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/utils/orientationinvariant.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/**
 * @brief Container of a single widget that supports alignment and stretching at the same time.
 *
 * Standard Qt box layout does not support stretching and alignment and the same time: only unligned widgets can be stretched,
 * and if alignment is set then stretching is truned off. This container can align widget and strech it as requested at the same time.
 *
 */
class UISE_DESKTOP_EXPORT AlignedStretchingWidget : public QFrame,
                                                    public OrientationInvariant
{
    Q_OBJECT

    public:

        /**
         * @brief Constructor.
         * @param parent Parent widget.
         */
        AlignedStretchingWidget(QWidget* parent=nullptr);

        /**
         * @brief Set widget to container.
         * @param widget Widget with content.
         * @param orientation Container orientation.
         * @param alignment Widget alignment.
         *
         * This object will own the widget. Added widgets holds property "AlignedStretchingWidget"=true.
         */
        void setWidget(QWidget* widget, Qt::Orientation orientation, Qt::Alignment alignment=Qt::Alignment());

        /**
         * @brief Take widget from container.
         * @return Taken widget.
         *
         * This object will not own the widget any more.
         */
        QWidget* takeWidget();

        /**
         * @brief Get content widget.
         * @return COntent widget.
         */
        inline QWidget* widget() const noexcept
        {
            return m_widget.data();
        }

        /**
         * @brief Check if container's orientation is horizontal.
         * @return True for horizontal orientation, false otherwise.
         */
        bool isHorizontal() const noexcept override;

    public slots:

        void updateMinMaxSize();

    protected:

        void resizeEvent(QResizeEvent* event) override;
        void updateSize();

    private:

        QPointer<QWidget> m_widget;
        Qt::Orientation m_orientation;
        Qt::Alignment m_alignment;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_ALIGNEDSTRETCHINGWIDGET_HPP

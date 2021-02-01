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
#include <QVBoxLayout>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/utils/orientationinvariant.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class UISE_DESKTOP_EXPORT AlignedStretchingWidget : public QFrame,
                                                    public OrientationInvariant
{
    Q_OBJECT

    public:

        AlignedStretchingWidget(QWidget* parent=nullptr);

        void setWidget(QWidget* widget, Qt::Orientation orientation, Qt::Alignment alignment=Qt::Alignment());
        QWidget* takeWidget();

        inline QWidget* widget() const noexcept
        {
            return m_widget.data();
        }

        bool isHorizontal() const noexcept override;

    public slots:

        void updateSize();

    protected:

        void resizeEvent(QResizeEvent* event) override;

    private:

        QPointer<QWidget> m_widget;
        Qt::Orientation m_orientation;
        Qt::Alignment m_alignment;
};

UISE_DESKTOP_NAMESPACE_EMD

#endif // UISE_DESKTOP_ALIGNEDSTRETCHINGWIDGET_HPP

/**
@copyright Evgeny Sidorov 2021

This software is dual-licensed. Choose the appropriate license for your project.

1. For open source projects this software is distrubuted under the GNU GENERAL PUBLIC LICENSE, Version 3.0.
(See accompanying file LICENSE-GPLv3.md or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. For proprietary license contact evgeniums@dracosha.com.

*/

/****************************************************************************/

/** \file uise/desktop/flyweightlistview.hpp
*
*  Defines FlyWeightListView.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_FLYWEIGHTLISTVIEW_HPP
#define UISE_DESKTOP_FLYWEIGHTLISTVIEW_HPP

#include <QFrame>

#include <uise/desktop/uisedesktop.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/**
 * @brief Flyweight list of widgets.
 */
class UISE_DESKTOP_EXPORT FlyweightListView : public QFrame
{
    Q_OBJECT

    public:

        /**
         * @brief Constructor.
         * @param parent Parent widget.
         */
        FlyweightListView(QWidget* parent=nullptr);
};

UISE_DESKTOP_NAMESPACE_EMD


#endif // UISE_DESKTOP_FLYWEIGHTLISTVIEW_HPP

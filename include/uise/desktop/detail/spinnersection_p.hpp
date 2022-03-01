/**
@copyright Evgeny Sidorov 2022

This software is dual-licensed. Choose the appropriate license for your project.

1. For open source projects this software is distrubuted under the GNU GENERAL PUBLIC LICENSE, Version 3.0.
(See accompanying file LICENSE-GPLv3.md or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. For proprietary license contact evgeniums@dracosha.com.

*/

/****************************************************************************/

/** @file uise/desktop/include/detail/spinnersection_p.hpp
*
*  Defines SpinnerSection.
*
*/

/****************************************************************************/

#include <QWidget>
#include <QVariantAnimation>
#include <QPointer>

#include <uise/desktop/utils/singleshottimer.hpp>

class Spinner;

UISE_DESKTOP_NAMESPACE_BEGIN

namespace detail
{

class SpinnerSection_p
{
    public:

        int index=-1;
        int itemsWidth=0;
        int leftBarWidth=0;
        int rightBarWidth=0;
        int currentOffset=0;
        int previousItemIndex=-1;
        int currentItemIndex=-1;
        int currentItemPosition=-1;
        bool circular=false;
        int animationVal=0;

        Spinner *spinner;

        QPointer<QWidget> leftBarLabel;
        QPointer<QWidget> rightBarLabel;

        QPointer<SingleShotTimer> adjustTimer;
        QPointer<QVariantAnimation> animation;
        QPointer<SingleShotTimer> selectionTimer;

        QList<QWidget*> items;
};

}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END

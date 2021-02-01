/**
@copyright Evgeny Sidorov 2021

This software is dual-licensed. Choose the appropriate license for your project.

1. For open source projects this software is distrubuted under the GNU GENERAL PUBLIC LICENSE, Version 3.0.
(See accompanying file LICENSE-GPLv3.md or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. For proprietary license contact evgeniums@dracosha.com.

*/

/****************************************************************************/

/** @file uise/desktop/flyweightlistframe.hpp
*
*  Defines FlyWeightListFrame.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_FLYWEIGHTLISTFRAME_HPP
#define UISE_DESKTOP_FLYWEIGHTLISTFRAME_HPP

#include <QFrame>

#include <uise/desktop/uisedesktop.hpp>

class QScrollArea;

UISE_DESKTOP_NAMESPACE_BEGIN

class UISE_DESKTOP_EXPORT FlyweightListFrame : public QFrame
{
    Q_OBJECT

    public:

        using QFrame::QFrame;

    signals:

        void homeRequested();
        void endRequested();
};

UISE_DESKTOP_NAMESPACE_EMD

#endif // UISE_DESKTOP_FLYWEIGHTLISTFRAME_HPP

/**
@copyright Evgeny Sidorov 2022

This software is dual-licensed. Choose the appropriate license for your project.

1. The GNU GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-GPLv3.md](LICENSE-GPLv3.md) or copy at https://www.gnu.org/licenses/gpl-3.0.txt)
    
2. The GNU LESSER GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-LGPLv3.md](LICENSE-LGPLv3.md) or copy at https://www.gnu.org/licenses/lgpl-3.0.txt).

You may select, at your option, one of the above-listed licenses.

*/

/****************************************************************************/

/** @file uise/desktop/src/wheeleventhandler.cpp
*
*  Defines WheelEventHandler.
*
*/

/****************************************************************************/

#include <QWheelEvent>
#include <QApplication>

#include <uise/desktop/utils/wheeleventhandler.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

//--------------------------------------------------------------------------
WheelEventHandler::WheelEventHandler(
        int scrollStep
    ) : m_scrollStep(scrollStep),
        m_offsetAccumulated(0)
{}

//--------------------------------------------------------------------------
int WheelEventHandler::handleWheelEvent(QWheelEvent *event)
{
    auto numPixels = event->pixelDelta();
    auto angleDelta = event->angleDelta();

    int scrollDelta=0;

#ifndef Q_WS_X11 // Qt documentation says that on X11 pixelDelta() is unreliable and should not be used
   if (!numPixels.isNull())
   {
       scrollDelta=numPixels.y();
   }
   else if (!angleDelta.isNull())
#endif
   {
       auto evalOffset=[this,&angleDelta](float& accumulated)
       {
           auto deltaPos=static_cast<qreal>(angleDelta.y());
           auto scrollLines=QApplication::wheelScrollLines();
           auto numStepsU = m_scrollStep * scrollLines * deltaPos / 120;
           if (qAbs(accumulated)>std::numeric_limits<float>::epsilon()
               &&
               (numStepsU/accumulated)<0
               )
           {
               accumulated=0.0f;
           }
           accumulated+=numStepsU;
           auto numSteps=static_cast<int>(accumulated);
           accumulated-=numSteps;

           return numSteps;
       };

       scrollDelta=evalOffset(m_offsetAccumulated);
   }

   return scrollDelta;
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END

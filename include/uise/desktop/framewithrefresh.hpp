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

/** @file uise/desktop/framewithrefresh.hpp
*
*  Declares base frame with refresh() slot and refreshRequested() signal.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_FRAME_WITH_REFRESH_HPP
#define UISE_DESKTOP_FRAME_WITH_REFRESH_HPP

#include <QFrame>

#include <uise/desktop/uisedesktop.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/**
 * @brief Base frame with refresh() slot and refreshRequested() signal.
 *        Virtual method doRefresh() can be reimplemented in derived class.
 */
class UISE_DESKTOP_EXPORT FrameWithRefresh : public QFrame
{
    Q_OBJECT

    public:

        using QFrame::QFrame;

    public slots:

        void refresh();

    signals:

        void refreshRequested();

    protected:

        virtual void doRefresh();

};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_FRAME_WITH_REFRESH_HPP

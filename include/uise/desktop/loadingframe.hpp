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

/** @file uise/desktop/loadingframe.hpp
*
*  Declares LoadingFrame.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_LOADING_FRAME_HPP
#define UISE_DESKTOP_LOADING_FRAME_HPP

#include <memory>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/modalpopup.hpp>
#include <uise/desktop/widget.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class BusyWaiting;
class LoadingFrame_p;

/**
 * @brief Base class for frames with infinite loading spinner.
 */
class UISE_DESKTOP_EXPORT LoadingFrame : public FrameWithModalPopup,
                                         public Widget
{
    Q_OBJECT

    public:

        /**
             * @brief Constructor.
             * @param parent Parent widget.
             */
        LoadingFrame(QWidget* parent=nullptr);

        /**
         * @brief Destructor.
         */
        ~LoadingFrame();

        LoadingFrame(const LoadingFrame&)=delete;
        LoadingFrame(LoadingFrame&&)=delete;
        LoadingFrame& operator=(const LoadingFrame&)=delete;
        LoadingFrame& operator=(LoadingFrame&&)=delete;

        void setCancellableBusyWaiting(bool enable);
        bool isCancellableBusyWaiting() const;

        BusyWaiting* busyWaitingWidget() const;

        virtual void construct() override;

        QWidget* qWidget() override
        {
            return this;
        }

    signals:

        void cancelled();

    public slots:

        virtual void popupBusyWaiting();

        virtual void cancel();
        virtual void finish();

    private:

        std::unique_ptr<LoadingFrame_p> pimpl;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_LOADING_FRAME_HPP

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

/** @file uise/desktop/framewithmodalstatus.hpp
*
*  Declares FrameWithModalStatus.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_MODAL_STATUS_HPP
#define UISE_DESKTOP_MODAL_STATUS_HPP

#include <memory>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/modalpopup.hpp>
#include <uise/desktop/statusdialog.hpp>

class QLabel;

UISE_DESKTOP_NAMESPACE_BEGIN

class BusyWaiting;
class FrameWithModalStatus_p;

/**
 * @brief Base class for frames with modal busy/error status.
 */
class UISE_DESKTOP_EXPORT FrameWithModalStatus : public FrameWithModalPopup
{
    Q_OBJECT

    public:

        /**
             * @brief Constructor.
             * @param parent Parent widget.
             */
        FrameWithModalStatus(QWidget* parent=nullptr);

        /**
         * @brief Destructor.
         */
        ~FrameWithModalStatus();

        FrameWithModalStatus(const FrameWithModalStatus&)=delete;
        FrameWithModalStatus(FrameWithModalStatus&&)=delete;
        FrameWithModalStatus& operator=(const FrameWithModalStatus&)=delete;
        FrameWithModalStatus& operator=(FrameWithModalStatus&&)=delete;

        void setCancellableBusyWaiting(bool enable);
        bool isCancellableBusyWaiting() const;

        BusyWaiting* busyWaitingWidget() const;
        StatusDialog* statusDialog() const;

    signals:

        void cancelled();

    public slots:

        void popupBusyWaiting();
        void popupStatus(const QString& message, const QString& title, std::shared_ptr<UISE_DESKTOP_NAMESPACE::SvgIcon> icon={});
        void popupStatus(const QString& message, StatusDialog::Type type, const QString& title={});

        void cancel();
        void finish();

    private:

        void showStatus();

        std::unique_ptr<FrameWithModalStatus_p> pimpl;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_MODAL_STATUS_HPP

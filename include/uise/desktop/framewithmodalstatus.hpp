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
#include <uise/desktop/abstractloadingwidget.hpp>

class QLabel;

UISE_DESKTOP_NAMESPACE_BEGIN

class FrameWithModalStatus_p;

/**
 * @brief Base class for frames with modal busy/error status.
 */
class UISE_DESKTOP_EXPORT FrameWithModalStatus : public FrameWithModalPopup,
                                                 public Widget
{
    Q_OBJECT

    public:

        constexpr static const int DefaultPopupMaxWidth=500;
        constexpr static const int DefaultMaxWidthPercent=80;

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

        /**
         * @brief Return the loading widget for the given slot.
         * @param forLoading false = operation-wait slot (spinner); true = panel-loading slot (skeleton).
         */
        AbstractLoadingWidget* loadingWidget(bool forLoading=false) const;

        /**
         * @brief Replace the loading widget in the given slot.
         * @param forLoading false = operation-wait slot; true = panel-loading slot.
         */
        void setLoadingWidget(AbstractLoadingWidget* widget, bool forLoading=false);

        AbstractStatusDialog* statusDialog() const;

        virtual void construct() override;

        QWidget* qWidget() override
        {
            return this;
        }

        /**
         * @brief Show or hide the loading overlay for the given slot.
         * @param forLoading false = operation-wait slot; true = panel-loading slot.
         */
        void setBusyWaiting(bool enable, bool forLoading=false)
        {
            if (enable)
                popupBusyWaiting(forLoading);
            else
                finish();
        }

    signals:

        void cancelled();

    public slots:

        void popupBusyWaiting();

        /**
         * @brief Show the loading overlay, selecting the widget by slot type.
         * @param forLoading false = operation-wait widget (default); true = panel-loading widget.
         */
        void popupBusyWaiting(bool forLoading);

        void popupStatus(const QString& message, const QString& title, std::shared_ptr<UISE_DESKTOP_NAMESPACE::SvgIcon> icon={});
        void popupStatus(const QString& message, UISE_DESKTOP_NAMESPACE::StatusDialog::Type type, const QString& title={});

        void cancel();
        void finish();

    private:

        AbstractLoadingWidget* activeLoadingWidget() const;
        AbstractLoadingWidget* inactiveLoadingWidget() const;
        void showStatus();
        void reconfigureForLoading();
        void reconfigureForStatus();

        std::unique_ptr<FrameWithModalStatus_p> pimpl;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_MODAL_STATUS_HPP

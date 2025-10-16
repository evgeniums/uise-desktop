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

/** @file uise/desktop/modaldialog.hpp
*
*  Declares ModalDialog.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_MODAL_DIALOG_HPP
#define UISE_DESKTOP_MODAL_DIALOG_HPP

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/widget.hpp>
#include <uise/desktop/modalpopup.hpp>
#include <uise/desktop/abstractdialog.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

struct DefaultDialogWidgetExtractor
{
    template <typename T>
    static auto dialogWidget(T dialog)
    {
        return dialog;
    }
};

constexpr const int ModalDialogDefaultPopupMaxWidth=500;
constexpr const int ModalDialogDefaultMaxWidthPercent=80;
constexpr const int ModalDialogDefaultPopupMaxHeight=500;
constexpr const int ModalDialogDefaultMaxHeightPercent=80;

template <typename AbstractDialogT, typename DefaultImplT,
         int DefaultPopupMaxWidthT=ModalDialogDefaultPopupMaxWidth,
         int DefaultMaxWidthPercentT=ModalDialogDefaultMaxWidthPercent,
         int DefaultPopupMaxHeightT=ModalDialogDefaultPopupMaxHeight,
         int DefaultMaxHeightPercentT=ModalDialogDefaultMaxHeightPercent,
         typename DialogWidgetExtractor=DefaultDialogWidgetExtractor>
class ModalDialog : public FrameWithModalPopup,
                    public Widget
{
    public:

        ModalDialog(QWidget* parent=nullptr,
                    int defaultMaxWidthPercent=DefaultMaxWidthPercentT,
                    int defaultPopupMaxWidth=DefaultPopupMaxWidthT,
                    int defaultMaxHeightPercent=DefaultMaxHeightPercentT,
                    int defaultPopupMaxHeight=DefaultPopupMaxHeightT
                    )
            : FrameWithModalPopup(parent),
              m_defaultMaxWidthPercent(defaultMaxWidthPercent),
              m_defaultPopupMaxWidth(defaultPopupMaxWidth),
              m_defaultMaxHeightPercent(defaultMaxHeightPercent),
              m_defaultPopupMaxHeight(defaultPopupMaxHeight)
        {
            setShortcutEnabled(false);
        }

        QWidget* qWidget() override
        {
            return this;
        }

        /**
         * @brief Open modal dialog.
         * @parame destroyOnCancel Destroy dialog if cancelled.
         * @return Returns true if new dialog is created, false if dialog already existed.
         */
        bool openDialog(bool destroyOnCancel=true)
        {
            if (m_dialog)
            {
                popup();
                return false;
            }

            m_dialog=makeWidget<AbstractDialogT,DefaultImplT>();

            if (m_defaultMaxWidthPercent>0)
            {
                setMaxWidthPercent(m_defaultMaxWidthPercent);
            }
            if (m_defaultMaxHeightPercent>0)
            {
                setMaxHeightPercent(m_defaultMaxHeightPercent);
            }

            auto dialogWidget=DialogWidgetExtractor::dialogWidget(m_dialog);

            if (m_defaultPopupMaxWidth>0)
            {
                dialogWidget->setMaximumWidth(m_defaultPopupMaxWidth);
            }
            if (m_defaultPopupMaxHeight>0)
            {
                dialogWidget->setMaximumHeight(m_defaultPopupMaxHeight);
            }

            setPopupWidget(dialogWidget,destroyOnCancel);
            connect(
                dialogWidget,
                &AbstractDialog::closeRequested,
                this,
                [this]()
                {
                    blockSignals(true);
                    closePopup();
                    blockSignals(false);
                }
                );
            connect(
                this,
                &FrameWithModalPopup::popupHidden,
                dialogWidget,
                &AbstractDialog::closeDialog
            );

            popup();
            dialogWidget->setDialogFocus();
            return true;
        }

        QPointer<AbstractDialogT> dialog() const
        {
            return m_dialog;
        }

    private:

        QPointer<AbstractDialogT> m_dialog;

        int m_defaultMaxWidthPercent;
        int m_defaultPopupMaxWidth;
        int m_defaultMaxHeightPercent;
        int m_defaultPopupMaxHeight;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_MODAL_DIALOG_HPP

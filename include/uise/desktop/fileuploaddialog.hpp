/**
@copyright Evgeny Sidorov 2026

This software is dual-licensed. Choose the appropriate license for your project.

1. The GNU GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-GPLv3.md](LICENSE-GPLv3.md) or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. The GNU LESSER GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-LGPLv3.md](LICENSE-LGPLv3.md) or copy at https://www.gnu.org/licenses/lgpl-3.0.txt).

You may select, at your option, one of the above-listed licenses.

*/

/****************************************************************************/

/** @file uise/desktop/fileuploaddialog.hpp
*
*  Declares FileUploadDialog.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_FILEUPLOADDIALOG_HPP
#define UISE_DESKTOP_FILEUPLOADDIALOG_HPP

#include <QPointer>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/dialog.hpp>
#include <uise/desktop/modaldialog.hpp>
#include <uise/desktop/abstractfileuploadwidget.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/**
 * @brief Interface of a modal wrapper around AbstractFileUploadWidget.
 *
 * The widget owns its own header and button row (see AbstractFileUploadWidget); this dialog
 * hides both and drops Dialog<>'s own default Close button in construct(), relaying the
 * widget's cancelled()/sendRequested() as buttonClicked(Cancel/Accept) instead so the
 * ModalDialog/buttonClicked(int) plumbing stays uniform with every other dialog in this
 * library.
 */
class UISE_DESKTOP_EXPORT AbstractFileUploadDialog : public AbstractDialog
{
    Q_OBJECT

    public:

        using AbstractDialog::AbstractDialog;

        virtual AbstractFileUploadWidget* fileUploadWidget() const=0;

        /**
         * @brief Close the dialog automatically once fileUploadWidget()->items() empties out.
         * @param enable Default false: an emptied list otherwise just stays open, so removing
         *  the last file never silently discards a typed comment.
         */
        void setCloseWhenEmpty(bool enable) noexcept
        {
            m_closeWhenEmpty=enable;
        }

        bool isCloseWhenEmpty() const noexcept
        {
            return m_closeWhenEmpty;
        }

    public slots:

        /**
         * @brief Reset fileUploadWidget() to its just-constructed state, for reuse across
         *  openings of a dialog kept alive with openDialog(destroyOnCancel=false). Does NOT
         *  close the dialog.
         */
        virtual void reset()=0;

    private:

        bool m_closeWhenEmpty=false;
};

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4661)
#endif

class UISE_DESKTOP_EXPORT FileUploadDialog : public Dialog<AbstractFileUploadDialog>
{
    Q_OBJECT

    public:

        using Base=Dialog<AbstractFileUploadDialog>;
        using Base::Base;

        AbstractFileUploadWidget* fileUploadWidget() const override;

        void reset() override;

        virtual void construct() override;

    private:

        AbstractFileUploadWidget* m_widget=nullptr;
};

using ModalFileUploadDialogType=ModalDialog<AbstractFileUploadDialog,FileUploadDialog,-1,90,-1,90>;

class UISE_DESKTOP_EXPORT ModalFileUploadDialog : public ModalFileUploadDialogType
{
    Q_OBJECT

    public:

        /**
         * @brief Constructor. Same parameters/defaults as ModalFileUploadDialogType, plus
         *  popup auto-height enabled unconditionally (see below).
         */
        explicit ModalFileUploadDialog(
                QWidget* parent=nullptr,
                int defaultMaxWidthPercent=90,
                int defaultPopupMaxWidth=-1,
                int defaultMaxHeightPercent=90,
                int defaultPopupMaxHeight=-1
            ) : ModalFileUploadDialogType(parent,defaultMaxWidthPercent,defaultPopupMaxWidth,defaultMaxHeightPercent,defaultPopupMaxHeight)
        {
            // FileUploadWidget's preview list genuinely grows/shrinks with content (see
            // FileUploadWidget::updateListAreaHeight()), so a fixed percentage-of-host-frame
            // popup height cannot track it: content ends up with more (or less) room in the
            // widget's own layout than the popup actually occupies on screen, which is exactly
            // what produces corrupted-looking rendering (list content painted behind the rest
            // of the widget). setPopupAutoHeight() makes the popup reflow from the dialog's
            // real sizeHint()/heightForWidth() on every QEvent::LayoutRequest it bubbles up
            // (see ModalPopup::eventFilter/updateWidgetGeometry), so growth/shrinkage is
            // reflected in the popup's actual on-screen size instead of a stale allocation.
            setPopupAutoHeight(true);

            // ModalDialog<>'s own ctor calls setShortcutEnabled(false), so Escape does
            // nothing by default (see ModalPopup's internal QShortcut). Re-enable it here:
            // closing via Escape goes through the exact same ModalPopup::close() ->
            // popupHidden -> AbstractDialog::closeDialog() -> closeRequested() chain as the
            // Cancel button, so it already resets the widget (see the closeRequested
            // connection in FileUploadDialog::construct()) and tears down the popup the same
            // way -- nothing dialog-specific needed beyond turning the shortcut back on.
            setShortcutEnabled(true);
        }
};

#ifdef _MSC_VER
#pragma warning(pop)
#endif

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_FILEUPLOADDIALOG_HPP

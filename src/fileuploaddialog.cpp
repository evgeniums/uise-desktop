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

/** @file uise/desktop/src/fileuploaddialog.cpp
*
*  Defines FileUploadDialog.
*
*/

/****************************************************************************/

#include <QMetaObject>

#include <uise/desktop/style.hpp>
#include <uise/desktop/icontextbutton.hpp>
#include <uise/desktop/fileuploaddialog.hpp>
#include <uise/desktop/fileuploadwidget.hpp>
#include <uise/desktop/ipp/dialog.ipp>

UISE_DESKTOP_NAMESPACE_BEGIN

/**************************** FileUploadDialog ***********************************/

//--------------------------------------------------------------------------

void FileUploadDialog::construct()
{
    m_widget=makeWidget<AbstractFileUploadWidget,FileUploadWidget>();

    setWidget(m_widget->qWidget());

    // the widget owns its own header (caption + menu) and its own button row (Add/Cancel/
    // Send) -- see AbstractFileUploadWidget -- so this dialog hides Dialog<>'s title-driven
    // caption in favor of the widget's, and drops the inherited default Close button. The
    // caption is genuinely redundant with setTitle() below, but the menu button has no
    // dialog-level equivalent -- it is the only way to reach "Full quality" (menu-only by
    // design, unlike SendAsDocuments/GroupItems which also have checkboxes) -- so instead of
    // disappearing along with the rest of the header, it is relocated into the dialog's own
    // title bar. Reparenting leaves its DropdownMenu attachment and visibility logic (see
    // FileUploadWidget::updateMenuVisibility()) untouched, since both track the button by
    // reference, not by its position in the widget tree.
    m_widget->setHeaderVisible(false);
    m_widget->setButtonsVisible(true);
    setTitleControl(m_widget->menuButton());

    // the vertical dots used inside the widget's own header (see fileupload.json's "menu"
    // alias) read as too small once relocated into the dialog's title bar next to the title
    // text -- swap in the horizontal variant for this button only, leaving every other
    // dots-vertical use (standalone header, ChatMessageFiles, HTree placeholders) untouched.
    // Reuse DialogTitle's icon colors (same context as titleClose, see dialog.ipp) rather than
    // FileUpload's -- those are tuned for the widget's own light header, not the dialog's
    // always-dark title bar, and would render near-invisible there in the light theme.
    m_widget->menuButton()->setSvgIcon(
        Style::instance().svgIconLocator().icon(QStringLiteral("DialogTitle::menu"),m_widget->qWidget())
    );

    setButtons({});

    setTitle(m_widget->caption());
    connect(
        m_widget,
        &AbstractFileUploadWidget::captionChanged,
        this,
        [this](const QString& caption)
        {
            setTitle(caption);
        }
    );

    connect(
        m_widget,
        &AbstractFileUploadWidget::cancelled,
        this,
        [this]()
        {
            emit AbstractDialog::buttonClicked(static_cast<int>(AbstractDialog::StandardButton::Cancel));
            closeDialog();
        }
    );
    connect(
        m_widget,
        &AbstractFileUploadWidget::sendRequested,
        this,
        [this]()
        {
            // unlike Cancel, Send does not close the dialog on its own -- the host may need
            // to keep it open (e.g. showing a busy state) until an async upload finishes,
            // and calls closeDialog() itself once it is done
            emit AbstractDialog::buttonClicked(static_cast<int>(AbstractDialog::StandardButton::Accept));
        }
    );

    connect(
        m_widget,
        &AbstractFileUploadWidget::emptied,
        this,
        [this]()
        {
            if (!isCloseWhenEmpty())
            {
                return;
            }
            // emptied() is emitted from inside a FileUploadListItem's own removeRequested/
            // menu-triggered handler; closing (and so destroying) the dialog synchronously
            // here would tear down that very item widget while its signal is still on the
            // call stack. Defer to the next event loop iteration instead.
            QMetaObject::invokeMethod(this,[this](){ closeDialog(); },Qt::QueuedConnection);
        }
    );

    // closeDialog() unconditionally emits closeRequested() (see AbstractDialog::closeDialog()),
    // and every way this dialog can close funnels through it -- the Cancel handler above calls
    // it directly, and ModalDialog::openDialog() wires FrameWithModalPopup::popupHidden back to
    // it too, which covers paths that bypass this widget entirely (Escape, outside click). So
    // this single connection resets the widget on every close, not just Cancel -- including
    // after a host-driven Send completion, which also ends by calling closeDialog(). Connected
    // here (construct() runs before ModalDialog::openDialog() wires its own closeRequested
    // listener) so reset() always runs before closePopup() tears anything down.
    connect(
        this,
        &AbstractDialog::closeRequested,
        this,
        [this]()
        {
            reset();
        }
    );
}

//--------------------------------------------------------------------------

AbstractFileUploadWidget* FileUploadDialog::fileUploadWidget() const
{
    return m_widget;
}

//--------------------------------------------------------------------------

void FileUploadDialog::reset()
{
    if (m_widget!=nullptr)
    {
        m_widget->reset();
    }
}

//--------------------------------------------------------------------------

void FileUploadDialog::prepareToShow()
{
    // Runs while the popup/frame hosting this dialog is still invisible (see
    // ModalPopup::popup()/FloatingDialogFrame::preparePopup()), after polish -- settle the
    // widget's content-driven list-area height now so a dialog pre-populated with items before
    // being shown is measured once, at its final size, instead of visibly refitting afterwards.
    if (m_widget!=nullptr)
    {
        m_widget->settleLayout();
    }
}

//--------------------------------------------------------------------------

template class UISE_DESKTOP_EXPORT Dialog<AbstractFileUploadDialog>;

UISE_DESKTOP_NAMESPACE_END

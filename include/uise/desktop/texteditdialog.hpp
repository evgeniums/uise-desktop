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

/** @file uise/desktop/texteditdialog.hpp
*
*  Declares TextEditDialog.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_TEXTEDIT_DIALOG_HPP
#define UISE_DESKTOP_TEXTEDIT_DIALOG_HPP

#include <QString>
#include <QPointer>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/dialog.hpp>
#include <uise/desktop/modaldialog.hpp>
#include <uise/desktop/textviewer.hpp>

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

/**
 * @brief Interface of a dialog around a TextViewer in editing mode -- the text counterpart of
 *  AbstractImageEditDialog.
 *
 * Meant to be a window-level modal (ModalTextEditDialog), not nested in the file upload dialog's
 * popup, so that it is not confined to that popup's rect. Nothing here knows about upload items:
 * the host loads the text into viewer(), calls viewer()->startEditing(), and writes the result
 * back itself on textApplied().
 */
class UISE_DESKTOP_EXPORT AbstractTextEditDialog : public AbstractDialog
{
    Q_OBJECT

    public:

        using AbstractDialog::AbstractDialog;

        virtual TextViewer* viewer() const=0;

    signals:

        /**
         * @brief Apply was pressed. Relays TextViewer::editApplied(); buttonClicked(Apply) is
         *  emitted right after it. The dialog does not close itself on Apply -- the host may want
         *  to check the text first -- so it calls closeDialog() once it has used it.
         */
        void textApplied(const QString& text, bool modified);
};

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4661)
#endif

/**
 * @brief The default implementation: the dialog's own title bar and button row are hidden, since
 *  the viewer's header is the title and its bottom bar has Cancel / Apply.
 *
 * Cancel, and the viewer's Close, ask before discarding edits (see TextViewer) and then emit
 * buttonClicked(Cancel) and close the dialog.
 */
class UISE_DESKTOP_EXPORT TextEditDialog : public Dialog<AbstractTextEditDialog>
{
    Q_OBJECT

    public:

        using Base=Dialog<AbstractTextEditDialog>;
        using Base::Base;

        TextViewer* viewer() const override;

        virtual void construct() override;

    private:

        void cancelled();

        TextViewer* m_viewer=nullptr;
        bool m_closing=false;
};

using ModalTextEditDialogType=ModalDialog<AbstractTextEditDialog,TextEditDialog,-1,95,-1,95>;

class UISE_DESKTOP_EXPORT ModalTextEditDialog : public ModalTextEditDialogType
{
    Q_OBJECT

    public:

        using ModalTextEditDialogType::ModalTextEditDialogType;
};

#ifdef _MSC_VER
#pragma warning(pop)
#endif

}

#endif // UISE_DESKTOP_TEXTEDIT_DIALOG_HPP

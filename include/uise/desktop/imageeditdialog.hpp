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

/** @file uise/desktop/imageeditdialog.hpp
*
*  Declares ImageEditDialog.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_IMAGEEDIT_DIALOG_HPP
#define UISE_DESKTOP_IMAGEEDIT_DIALOG_HPP

#include <QFrame>
#include <QPointer>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/modalpopup.hpp>
#include <uise/desktop/dialog.hpp>
#include <uise/desktop/frame.hpp>
#include <uise/desktop/modaldialog.hpp>
#include <uise/desktop/abstractimageeditor.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class ImageEditDialog_p;

class UISE_DESKTOP_EXPORT AbstractImageEditDialog : public AbstractDialog
{
    Q_OBJECT

    public:

        using AbstractDialog::AbstractDialog;

        virtual AbstractImageEditor* editor() const=0;
};

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4661)
#endif

class UISE_DESKTOP_EXPORT ImageEditDialog : public Dialog<AbstractImageEditDialog>
{
    Q_OBJECT

    public:

        using Base=Dialog<AbstractImageEditDialog>;
        using Base::Base;

        virtual AbstractImageEditor* editor() const override;

        virtual void construct() override;

    private:

        AbstractImageEditor* m_editor;
};

using ModalImageEditDialogType=ModalDialog<AbstractImageEditDialog,ImageEditDialog,-1,95,-1,95>;

class UISE_DESKTOP_EXPORT ModalImageEditDialog : public ModalImageEditDialogType
{
    Q_OBJECT

    public:

        using ModalImageEditDialogType::ModalImageEditDialogType;
};

#ifdef _MSC_VER
#pragma warning(pop)
#endif

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_IMAGEEDIT_DIALOG_HPP

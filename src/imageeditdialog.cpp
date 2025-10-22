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

/** @file uise/desktop/src/imageeditdialog.cpp
*
*  Defines ImageEditDialog.
*
*/

/****************************************************************************/

#include <uise/desktop/imageeditdialog.hpp>
#include <uise/desktop/simpleimageeditor.hpp>
#include <uise/desktop/ipp/dialog.ipp>

UISE_DESKTOP_NAMESPACE_BEGIN

/**************************** ImageEditDialog ***********************************/

//--------------------------------------------------------------------------

void ImageEditDialog::construct()
{
    m_editor=makeWidget<AbstractImageEditor,SimpleImageEditor>();

    setWidget(m_editor->qWidget());

    ButtonsStyle bstyle{};
    bstyle.showIcon=false;
    bstyle.showText=true;
    bstyle.alignment=Qt::AlignCenter;
    setButtonsStyle(bstyle);
    setButtons(
        {
            AbstractDialog::standardButton(AbstractDialog::StandardButton::Apply,this),
            AbstractDialog::standardButton(AbstractDialog::StandardButton::Cancel,this)
        }
    );
}

//--------------------------------------------------------------------------

AbstractImageEditor*  ImageEditDialog::editor() const
{
    return m_editor;
}

//--------------------------------------------------------------------------

template class UISE_DESKTOP_EXPORT Dialog<AbstractImageEditDialog>;

UISE_DESKTOP_NAMESPACE_END

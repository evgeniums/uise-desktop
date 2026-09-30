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

/** @file uise/desktop/src/texteditdialog.cpp
*
*  Defines TextEditDialog.
*
*/

/****************************************************************************/

#include <QMetaObject>

#include <uise/desktop/texteditdialog.hpp>
#include <uise/desktop/ipp/dialog.ipp>

UISE_DESKTOP_NAMESPACE_BEGIN

/**************************** TextEditDialog ***********************************/

//--------------------------------------------------------------------------

void TextEditDialog::construct()
{
    m_viewer=new TextViewer();
    setWidget(m_viewer);

    // The viewer's header is the title (file name, size, buttons) and its bottom bar has Cancel /
    // Apply, so neither of the dialog's own is wanted.
    titleBar()->setVisible(false);
    setButtons({});

    connect(
        m_viewer,
        &TextViewer::editApplied,
        this,
        [this](const QString& text, bool modified)
        {
            emit textApplied(text,modified);
            emit AbstractDialog::buttonClicked(static_cast<int>(AbstractDialog::StandardButton::Apply));
        }
    );

    connect(m_viewer,&TextViewer::editCancelled,this,[this](){cancelled();});
    // Close of a viewer that has no frame of its own. After a confirmed discard it follows
    // editCancelled(), which has closed already -- cancelled() only acts once.
    connect(m_viewer,&TextViewer::closeRequested,this,[this](){cancelled();});
}

//--------------------------------------------------------------------------

void TextEditDialog::cancelled()
{
    if (m_closing)
    {
        return;
    }
    m_closing=true;

    emit AbstractDialog::buttonClicked(static_cast<int>(AbstractDialog::StandardButton::Cancel));
    closeDialog();

    // Not reset right here: a Close that discarded edits emits editCancelled() and then
    // closeRequested(), and the second must find this still set. A dialog kept alive for the next
    // opening starts from a clean slate once the current signal chain has unwound.
    QMetaObject::invokeMethod(this,[this](){m_closing=false;},Qt::QueuedConnection);
}

//--------------------------------------------------------------------------

TextViewer* TextEditDialog::viewer() const
{
    return m_viewer;
}

//--------------------------------------------------------------------------

template class UISE_DESKTOP_EXPORT Dialog<AbstractTextEditDialog>;

UISE_DESKTOP_NAMESPACE_END

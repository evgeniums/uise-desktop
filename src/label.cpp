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

/** @file uise/desktop/label.cpp
*
*  Defines Label.
*
*/

/****************************************************************************/

#include <QMenu>
#include <QAction>
#include <QContextMenuEvent>
#include <QGuiApplication>
#include <QClipboard>

#include <uise/desktop/label.hpp>

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

//--------------------------------------------------------------------------

Label::Label(QWidget *parent, Qt::WindowFlags f) : QLabel(parent,f)
{
    init();
}

//--------------------------------------------------------------------------

Label::Label(const QString &text, QWidget *parent, Qt::WindowFlags f) : QLabel(text,parent,f)
{
    init();
}

//--------------------------------------------------------------------------

void Label::init()
{
    setTextFormat(Qt::PlainText);
    setTextInteractionFlags(Qt::TextSelectableByMouse);
}

//--------------------------------------------------------------------------

void Label::contextMenuEvent(QContextMenuEvent* event)
{
    if (!(textInteractionFlags() & (Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard)))
    {
        event->ignore();
        return;
    }

    // Built explicitly instead of relying on QLabel's own standard menu so that "Copy Link
    // Location" -- which Qt adds whenever the label's rich text contains a hyperlink, even an
    // in-app anchor with no meaningful external location to copy -- never shows up. Same pattern
    // as ChatMessageTextBrowser::showCopyMenu().
    QMenu menu(this);

    auto copyAction = menu.addAction(tr("Copy"));
    copyAction->setEnabled(!selectedText().isEmpty());
    connect(copyAction, &QAction::triggered, this,
        [this]() { QGuiApplication::clipboard()->setText(selectedText()); });

    auto selectAllAction = menu.addAction(tr("Select All"));
    connect(selectAllAction, &QAction::triggered, this,
        [this]() { setSelection(0, text().length()); });

    menu.exec(event->globalPos());
    event->accept();
}

//--------------------------------------------------------------------------

}

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

/** @file uise/desktop/src/editbar.cpp
*
*  Defines EditBar.
*
*/

/****************************************************************************/

#include <QShortcut>
#include <QShowEvent>
#include <QHideEvent>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/icontextbutton.hpp>
#include <uise/desktop/abstractreplypreview.hpp>
#include <uise/desktop/replypreview.hpp>
#include <uise/desktop/editbar.hpp>

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

class EditBar_p
{
    public:

        QBoxLayout* layout;

        IconTextButton* editButton;
        AbstractReplyPreview* preview;
        IconTextButton* cancelButton;

        //! Created disabled, enabled only while the bar is actually shown -- see
        //! EditBar::showEvent()/hideEvent() and AbstractEditBar::closeOnEscape's own doc
        //! comment on why a hidden bar must never claim the window's Escape. Same lifetime
        //! idiom as FileDropOverlay's own escShortcut.
        QShortcut* escShortcut;
};

//--------------------------------------------------------------------------

EditBar::EditBar(QWidget* parent)
    : AbstractEditBar(parent),
      pimpl(std::make_unique<EditBar_p>())
{
    pimpl->layout=Layout::horizontal(this);

    pimpl->editButton=new IconTextButton(
        Style::instance().svgIconLocator().icon(QStringLiteral("EditBar::edit"),this),
        this,
        IconTextButton::IconPosition::BeforeText
    );
    pimpl->editButton->setObjectName("editButton");
    pimpl->editButton->setText(QString());
    pimpl->editButton->setCursor(Qt::PointingHandCursor);
    pimpl->editButton->setToolTip(tr("Go to message"));
    pimpl->layout->addWidget(pimpl->editButton);
    connect(pimpl->editButton,&IconTextButton::clicked,this,&AbstractEditBar::jumpRequested);

    pimpl->preview=makeWidget<AbstractReplyPreview,ReplyPreview>(this);
    pimpl->preview->setTitleFormat(tr("Edit message, %2"));
    pimpl->preview->setQuoteIconVisible(false);
    pimpl->layout->addWidget(pimpl->preview,1);
    connect(pimpl->preview,&AbstractReplyPreview::clicked,this,&AbstractEditBar::jumpRequested);

    pimpl->cancelButton=new IconTextButton(
        Style::instance().svgIconLocator().icon(QStringLiteral("EditBar::cancel"),this),
        this,
        IconTextButton::IconPosition::BeforeText
    );
    pimpl->cancelButton->setObjectName("cancelButton");
    pimpl->cancelButton->setText(QString());
    pimpl->cancelButton->setCursor(Qt::PointingHandCursor);
    pimpl->cancelButton->setToolTip(tr("Cancel editing"));
    pimpl->layout->addWidget(pimpl->cancelButton);
    connect(pimpl->cancelButton,&IconTextButton::clicked,this,&AbstractEditBar::cancelRequested);

    pimpl->escShortcut=new QShortcut(Qt::Key_Escape,this);
    pimpl->escShortcut->setContext(Qt::WindowShortcut);
    pimpl->escShortcut->setEnabled(false);
    connect(pimpl->escShortcut,&QShortcut::activated,this,&AbstractEditBar::cancelRequested);

    setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Fixed);
}

//--------------------------------------------------------------------------

EditBar::~EditBar()
{}

//--------------------------------------------------------------------------

void EditBar::setEditData(ReplyPreviewData data)
{
    pimpl->preview->setData(std::move(data));
}

//--------------------------------------------------------------------------

const ReplyPreviewData& EditBar::editData() const
{
    return pimpl->preview->data();
}

//--------------------------------------------------------------------------

void EditBar::clear()
{
    pimpl->preview->clear();
}

//--------------------------------------------------------------------------

AbstractReplyPreview* EditBar::preview() const
{
    return pimpl->preview;
}

//--------------------------------------------------------------------------

void EditBar::setTitleFormat(const QString& format)
{
    pimpl->preview->setTitleFormat(format);
}

//--------------------------------------------------------------------------

QString EditBar::titleFormat() const
{
    return pimpl->preview->titleFormat();
}

//--------------------------------------------------------------------------

void EditBar::setTextTrimLength(int length)
{
    pimpl->preview->setTextTrimLength(length);
}

//--------------------------------------------------------------------------

int EditBar::textTrimLength() const
{
    return pimpl->preview->textTrimLength();
}

//--------------------------------------------------------------------------

IconTextButton* EditBar::editButton() const
{
    return pimpl->editButton;
}

//--------------------------------------------------------------------------

IconTextButton* EditBar::cancelButton() const
{
    return pimpl->cancelButton;
}

//--------------------------------------------------------------------------

void EditBar::updateCloseOnEscape()
{
    // Only takes effect while actually visible -- showEvent()/hideEvent() are what flip the
    // shortcut's real enabled state; this re-applies isCloseOnEscape() to whichever state the
    // bar is currently in (e.g. a QSS-driven qproperty write arriving while already shown).
    pimpl->escShortcut->setEnabled(isCloseOnEscape() && isVisible());
}

//--------------------------------------------------------------------------

void EditBar::showEvent(QShowEvent* event)
{
    pimpl->escShortcut->setEnabled(isCloseOnEscape());
    AbstractEditBar::showEvent(event);
}

//--------------------------------------------------------------------------

void EditBar::hideEvent(QHideEvent* event)
{
    pimpl->escShortcut->setEnabled(false);
    AbstractEditBar::hideEvent(event);
}

//--------------------------------------------------------------------------

}

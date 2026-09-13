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

/** @file uise/desktop/src/emojigallerydialog.cpp
*
*  Defines EmojiGalleryDialog.
*
*/

/****************************************************************************/

#include <QEvent>
#include <QPointer>

#include <uise/desktop/emojigallerydialog.hpp>
#include <uise/desktop/chatreactiongallery.hpp>
#include <uise/desktop/chatreaction.hpp>
#include <uise/desktop/reactioniconpack.hpp>

#include <uise/desktop/ipp/dialog.ipp>

namespace uise {

//--------------------------------------------------------------------------

class EmojiGalleryDialog_p
{
    public:

        QPointer<ChatReactionGallery> gallery;
};

//--------------------------------------------------------------------------

EmojiGalleryDialog::EmojiGalleryDialog(QWidget* parent)
    : Base(parent),
      pimpl(std::make_unique<EmojiGalleryDialog_p>())
{
}

//--------------------------------------------------------------------------

EmojiGalleryDialog::~EmojiGalleryDialog()
{
}

//--------------------------------------------------------------------------

void EmojiGalleryDialog::construct()
{
    setObjectName("emojiGalleryDialog");

    pimpl->gallery=new ChatReactionGallery(this);
    pimpl->gallery->setObjectName("emojiGallery");

    // Same widget as the reactions popup, different vocabulary -- see
    // ChatReactionGallery::setSearchPlaceholderText(). "Recently used" is left at its default:
    // it names a behaviour, not a payload, and reads correctly for emoji as it stands.
    pimpl->gallery->setSearchPlaceholderText(tr("Search emoji"));
    pimpl->gallery->setEmptyText(tr("No matching emoji"));

    connect(pimpl->gallery,&ChatReactionGallery::reactionPicked,this,
        [this](const QString& iconId)
        {
            // ChatReactionGallery emits the ICON ID only -- recombine it with the pack's URI so
            // subscribers get the full, pack-qualified reaction id this interface promises. The
            // difference is invisible for the default pack (empty URI => make() returns the bare
            // icon id) and load-bearing for every other one.
            auto p=pimpl->gallery->pack();
            Q_EMIT emojiPicked(ChatReactionId::make(iconId,p ? p->uri() : QString{}));
        }
    );

    setWidget(pimpl->gallery);
    setTitle(tr("Emoji"));

    // No bottom button row at all -- see the class doc comment. Dialog<>'s own constructor
    // installs a Close row by default; this replaces it with nothing. The (now empty) buttons
    // frame still exists and is still in the layout, so chatreactions.qss zeroes its margins to
    // keep it from reserving a strip of blank space under the grid.
    setButtons({});
}

//--------------------------------------------------------------------------

void EmojiGalleryDialog::setPack(std::shared_ptr<AbstractReactionIconPack> pack)
{
    if (pimpl->gallery.isNull())
    {
        return;
    }
    pimpl->gallery->setPack(std::move(pack));

    // Picking an emoji to TYPE has nothing to do with which reactions the current user has
    // already set on some message -- a checked cell here would read as a bug.
    pimpl->gallery->setOwnReactionIds({});
}

//--------------------------------------------------------------------------

std::shared_ptr<AbstractReactionIconPack> EmojiGalleryDialog::pack() const
{
    if (pimpl->gallery.isNull())
    {
        return {};
    }
    return pimpl->gallery->pack();
}

//--------------------------------------------------------------------------

void EmojiGalleryDialog::resetSearch()
{
    if (!pimpl->gallery.isNull())
    {
        pimpl->gallery->resetSearch();
    }
}

//--------------------------------------------------------------------------

void EmojiGalleryDialog::prepareToShow()
{
    // resetSearch() rebuilds the grid, which re-derives the viewport clamp from a live cell --
    // see this method's own doc comment for why that has to happen here rather than earlier.
    resetSearch();
}

//--------------------------------------------------------------------------

void EmojiGalleryDialog::changeEvent(QEvent* event)
{
    Base::changeEvent(event);
    if (event->type()==QEvent::LanguageChange)
    {
        setTitle(tr("Emoji"));
        if (!pimpl->gallery.isNull())
        {
            // The gallery re-applies whatever override it holds rather than reverting to its own
            // reaction defaults, but re-TRANSLATING that override is this dialog's job -- the
            // stored string is still in the previous language until we overwrite it here.
            pimpl->gallery->setSearchPlaceholderText(tr("Search emoji"));
            pimpl->gallery->setEmptyText(tr("No matching emoji"));
        }
    }
}

//--------------------------------------------------------------------------

FloatingEmojiGalleryDialog::FloatingEmojiGalleryDialog(QWidget* parent)
    : FloatingEmojiGalleryDialogType(parent)
{
    // Re-typed from the base's Qt::Dialog -- see the class doc comment for why a picker wants to
    // be a utility window. FramelessWindowHint is kept because the frame paints its own chrome
    // (and its content supplies the title bar it is dragged by), exactly as the base sets it.
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint);
}

//--------------------------------------------------------------------------

template class UISE_DESKTOP_EXPORT Dialog<AbstractEmojiGalleryDialog>;

//--------------------------------------------------------------------------

}

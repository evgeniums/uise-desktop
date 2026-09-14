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

/** @file uise/desktop/src/chatreactiongallery.cpp
*
*  Defines ChatReactionQuickBar, ChatReactionGallery and ChatReactionGalleryDropdown.
*
*/

/****************************************************************************/

#include <QLabel>
#include <QGridLayout>
#include <QEvent>

#include <uise/desktop/chatreactiongallery.hpp>
#include <uise/desktop/pushbutton.hpp>
#include <uise/desktop/searchlineedit.hpp>
#include <uise/desktop/scrollarea.hpp>
#include <uise/desktop/reactioniconpack.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/svgiconlocator.hpp>
#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/utils/destroywidget.hpp>

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot
// expand a macro-opened namespace, so it records tr() calls in this file under an unqualified
// context that does not match what moc (a real preprocessor) resolves at runtime -- translations
// for every string here would silently stay in English. Do not revert to the macro form. See
// task-localization-framework.md.
namespace uise {

//--------------------------------------------------------------------------

ChatReactionQuickBar::ChatReactionQuickBar(QWidget* parent)
    : Frame(parent)
{
    setObjectName("chatReactionQuickBar");

    auto* layout=Layout::horizontal(this);
    layout->setSpacing(4);

    m_expandButton=new PushButton(
        Style::instance().svgIconLocator().icon(QStringLiteral("ChatReactions::expand"),this),
        this
    );
    m_expandButton->setObjectName("expandButton");
    connect(m_expandButton,&PushButton::clicked,this,&ChatReactionQuickBar::expandRequested);
    layout->addWidget(m_expandButton);
    // Moved to its final (trailing) position once the icon buttons are built -- see rebuild().
}

//--------------------------------------------------------------------------

ChatReactionQuickBar::~ChatReactionQuickBar()
{
}

//--------------------------------------------------------------------------

void ChatReactionQuickBar::setPack(std::shared_ptr<AbstractReactionIconPack> pack)
{
    m_pack=std::move(pack);
    rebuild();
}

//--------------------------------------------------------------------------

void ChatReactionQuickBar::setOwnReactionIds(QStringList ids)
{
    m_ownReactionIds=std::move(ids);
    for (auto* button : m_buttons)
    {
        auto id=button->property("reactionId").toString();
        button->setChecked(m_ownReactionIds.contains(id));
    }
}

//--------------------------------------------------------------------------

void ChatReactionQuickBar::setChevronVisible(bool enable)
{
    m_expandButton->setVisible(enable);
}

//--------------------------------------------------------------------------

void ChatReactionQuickBar::rebuild()
{
    for (auto* button : m_buttons)
    {
        destroyWidget(button);
    }
    m_buttons.clear();

    if (!m_pack)
    {
        return;
    }

    auto* layout=static_cast<QHBoxLayout*>(this->layout());
    // Drop the trailing stretch/expand button from the layout without destroying them -- they
    // are re-added below, after the fresh icon buttons.
    layout->removeWidget(m_expandButton);

    for (const auto& iconId : m_pack->basicIconIds())
    {
        const auto* info=m_pack->find(iconId);
        if (info==nullptr)
        {
            continue;
        }

        auto* button=new PushButton(info->icon,this);
        button->setObjectName("quickButton");
        button->setProperty("reactionId",iconId);
        button->setCheckable(true);
        button->setChecked(m_ownReactionIds.contains(iconId));
        connect(button,&PushButton::clicked,this,
                [this,iconId]()
                {
                    Q_EMIT reactionPicked(iconId);
                });

        layout->addWidget(button);
        m_buttons.push_back(button);
    }

    layout->addWidget(m_expandButton);
}

//--------------------------------------------------------------------------

ChatReactionGallery::ChatReactionGallery(QWidget* parent)
    : Frame(parent)
{
    setObjectName("chatReactionGallery");

    auto* layout=Layout::vertical(this);

    m_recentFrame=new QFrame(this);
    m_recentFrame->setObjectName("recentFrame");
    auto* recentLayout=Layout::vertical(m_recentFrame);

    m_recentTitle=new QLabel(tr("Recently used"),m_recentFrame);
    m_recentTitle->setObjectName("recentTitle");
    recentLayout->addWidget(m_recentTitle);

    m_recentBar=new ChatReactionQuickBar(m_recentFrame);
    m_recentBar->setObjectName("recentBar");
    m_recentBar->setChevronVisible(false);
    connect(m_recentBar,&ChatReactionQuickBar::reactionPicked,this,&ChatReactionGallery::reactionPicked);
    recentLayout->addWidget(m_recentBar);

    layout->addWidget(m_recentFrame);

    m_searchEdit=new SearchLineEdit(this);
    m_searchEdit->setObjectName("searchEdit");
    m_searchEdit->setPlaceholderText(tr("Search reactions"));
    // The gallery's own dismissal (via the chained DropdownFrame it lives in) must win over the
    // search box's own Qt::WidgetShortcut Escape -- see SearchLineEdit::setCancelShortcutEnabled()'s
    // own doc comment. Without this, pressing Escape while the search box has focus would only
    // clear the search text instead of closing the gallery.
    m_searchEdit->setCancelShortcutEnabled(false);
    connect(m_searchEdit,&QLineEdit::textChanged,this,&ChatReactionGallery::onSearchTextChanged);
    layout->addWidget(m_searchEdit);

    m_galleryGrid=new QFrame();
    m_galleryGrid->setObjectName("galleryGrid");
    m_gridLayout=Layout::grid(m_galleryGrid);

    m_emptyLabel=new QLabel(tr("No matching reactions"),m_galleryGrid);
    m_emptyLabel->setObjectName("emptyLabel");
    m_emptyLabel->setVisible(false);

    m_galleryScroll=new ScrollArea(this);
    m_galleryScroll->setObjectName("galleryScroll");
    m_galleryScroll->setWidgetResizable(true);
    m_galleryScroll->setWidget(m_galleryGrid);
    layout->addWidget(m_galleryScroll,1);
}

//--------------------------------------------------------------------------

ChatReactionGallery::~ChatReactionGallery()
{
}

//--------------------------------------------------------------------------

void ChatReactionGallery::setPack(std::shared_ptr<AbstractReactionIconPack> pack)
{
    m_pack=std::move(pack);
    m_recentBar->setPack(m_pack);
    resetSearch();
}

//--------------------------------------------------------------------------

void ChatReactionGallery::setOwnReactionIds(QStringList ids)
{
    m_ownReactionIds=std::move(ids);
    m_recentBar->setOwnReactionIds(m_ownReactionIds);
    for (auto* cell : m_gridCells)
    {
        if (cell->isVisible())
        {
            auto id=cell->property("reactionId").toString();
            cell->setChecked(m_ownReactionIds.contains(id));
        }
    }
}

//--------------------------------------------------------------------------

void ChatReactionGallery::resetSearch()
{
    m_searchEdit->clear();
    rebuildGrid(QString{});
}

//--------------------------------------------------------------------------

void ChatReactionGallery::onSearchTextChanged(const QString& text)
{
    rebuildGrid(text);
}

//--------------------------------------------------------------------------

PushButton* ChatReactionGallery::ensureGridCell(size_t index)
{
    while (m_gridCells.size()<=index)
    {
        auto* cell=new PushButton(m_galleryGrid);
        cell->setObjectName("galleryButton");
        cell->setCheckable(true);
        connect(cell,&PushButton::clicked,this,
                [this,cell]()
                {
                    Q_EMIT reactionPicked(cell->property("reactionId").toString());
                });
        m_gridCells.push_back(cell);
    }
    return m_gridCells[index];
}

//--------------------------------------------------------------------------

void ChatReactionGallery::rebuildGrid(const QString& searchPrefix)
{
    // Wholesale clear-and-rebuild, mirroring ChatMessageContent::updateWidgets()'s own idiom:
    // take every item out of the layout (without destroying the pooled cell widgets) before
    // re-adding whichever ones currently match, at their new row/column positions. A pack of a
    // few dozen to a few hundred icons makes this trivial -- see the class's own "no
    // virtualization" doc comment.
    while (m_gridLayout->count()>0)
    {
        // takeAt() detaches the item but does not delete it (nor, for a widget item, the widget
        // itself) -- matches ChatMessageContent::updateWidgets()'s identical clear loop.
        delete m_gridLayout->takeAt(0);
    }

    std::vector<size_t> matches;
    if (m_pack)
    {
        matches=m_pack->search(searchPrefix);
    }

    for (size_t i=0; i<matches.size(); ++i)
    {
        const auto* info=m_pack->at(matches[i]);
        if (info==nullptr)
        {
            continue;
        }

        auto* cell=ensureGridCell(i);
        cell->setSvgIcon(info->icon);
        cell->setProperty("reactionId",info->iconId);
        cell->setChecked(m_ownReactionIds.contains(info->iconId));
        cell->setVisible(true);

        auto row=static_cast<int>(i)/m_galleryColumns;
        auto col=static_cast<int>(i)%m_galleryColumns;
        m_gridLayout->addWidget(cell,row,col);
    }

    for (size_t i=matches.size(); i<m_gridCells.size(); ++i)
    {
        m_gridCells[i]->setVisible(false);
    }

    m_emptyLabel->setVisible(matches.empty());
    if (matches.empty())
    {
        m_gridLayout->addWidget(m_emptyLabel,0,0);
    }

    applyVisibleRowsHeight();

    Q_EMIT sizeChanged();
}

//--------------------------------------------------------------------------

void ChatReactionGallery::applyVisibleRowsHeight()
{
    // galleryVisibleRows used to be stored and never read by anything: the scroll area had no
    // height limit at all, so the gallery was as tall as its whole pack (50 icons -> 7 rows) and
    // grew without bound with a bigger one. chatreactions.qss has always documented this
    // property as sizing the viewport; this is what finally makes that true.
    //
    // Measured from a live cell rather than from a QSS number: the cell's own size hint already
    // carries whatever icon size and padding the current theme gives it, so the clamp tracks the
    // theme instead of duplicating its numbers here.
    if (m_galleryVisibleRows<=0 || m_gridCells.empty())
    {
        m_galleryScroll->setMaximumHeight(QWIDGETSIZE_MAX);
        return;
    }

    const auto cellHeight=m_gridCells.front()->sizeHint().height();
    if (cellHeight<=0)
    {
        m_galleryScroll->setMaximumHeight(QWIDGETSIZE_MAX);
        return;
    }

    const auto spacing=m_gridLayout->verticalSpacing()>0 ? m_gridLayout->verticalSpacing() : 0;
    auto margins=m_gridLayout->contentsMargins();
    const auto frame=2*m_galleryScroll->frameWidth()+margins.top()+margins.bottom();

    m_galleryScroll->setMaximumHeight(
        m_galleryVisibleRows*cellHeight+(m_galleryVisibleRows-1)*spacing+frame
    );
}

//--------------------------------------------------------------------------

void ChatReactionGallery::setSearchPlaceholderText(const QString& text)
{
    m_searchPlaceholderText=text;
    m_searchEdit->setPlaceholderText(text.isEmpty() ? tr("Search reactions") : text);
}

//--------------------------------------------------------------------------

void ChatReactionGallery::setEmptyText(const QString& text)
{
    m_emptyText=text;
    m_emptyLabel->setText(text.isEmpty() ? tr("No matching reactions") : text);
}

//--------------------------------------------------------------------------

void ChatReactionGallery::setRecentTitleText(const QString& text)
{
    m_recentTitleText=text;
    m_recentTitle->setText(text.isEmpty() ? tr("Recently used") : text);
}

//--------------------------------------------------------------------------

void ChatReactionGallery::setGalleryColumns(int value)
{
    if (m_galleryColumns==value) return;
    m_galleryColumns=value;
    rebuildGrid(m_searchEdit->text());
}

//--------------------------------------------------------------------------

void ChatReactionGallery::setGalleryVisibleRows(int value)
{
    if (m_galleryVisibleRows==value) return;
    m_galleryVisibleRows=value;
    applyVisibleRowsHeight();
    Q_EMIT sizeChanged();
}

//--------------------------------------------------------------------------

void ChatReactionGallery::changeEvent(QEvent* event)
{
    Frame::changeEvent(event);
    if (event->type()==QEvent::LanguageChange)
    {
        // Through the setters, not straight to the widgets: a host that supplied its own wording
        // (an emoji picker says "Search emoji", not "Search reactions") must not have it silently
        // reverted to the reaction defaults by a language switch. Re-translating the override
        // itself is the host's job, from its own changeEvent -- see EmojiGalleryDialog.
        setRecentTitleText(m_recentTitleText);
        setSearchPlaceholderText(m_searchPlaceholderText);
        setEmptyText(m_emptyText);
        if (m_pack)
        {
            m_pack->retranslate();
        }
        rebuildGrid(m_searchEdit->text());
    }
}

//--------------------------------------------------------------------------

ChatReactionGalleryDropdown::ChatReactionGalleryDropdown(QWidget* parent)
    : DropdownFrame(parent)
{
    setObjectName("chatReactionGalleryDropdown");

    auto* content=new QFrame();
    auto* layout=Layout::vertical(content);

    // Picking a reaction (either page) is a genuine "activation" of this popup, exactly like a
    // DropdownMenu row's own click handler (emit itemTriggered(id); notifyActivated(...);) --
    // notifyActivated() closes chainRoot() (see its own doc comment), so a gallery chained above
    // a context menu takes that menu down with it too, instead of leaving it open behind an
    // already-closed gallery. Forwarding reactionPicked() first, so the host's own handler still
    // sees it before anything starts closing.
    auto onReactionPicked=[this](const QString& reactionId)
    {
        Q_EMIT reactionPicked(reactionId);
        notifyActivated();
    };

    m_quickBar=new ChatReactionQuickBar(content);
    connect(m_quickBar,&ChatReactionQuickBar::reactionPicked,this,onReactionPicked);
    connect(m_quickBar,&ChatReactionQuickBar::expandRequested,this,
            [this]()
            {
                setExpanded(true);
            });
    layout->addWidget(m_quickBar);

    m_gallery=new ChatReactionGallery(content);
    m_gallery->setVisible(false);
    connect(m_gallery,&ChatReactionGallery::reactionPicked,this,onReactionPicked);
    connect(m_gallery,&ChatReactionGallery::sizeChanged,this,
            [this,content]()
            {
                content->updateGeometry();
                // Keep the collapsed bar's own on-screen position fixed and grow/shrink the
                // panel below it -- see setExpanded()'s identical reasoning. A search result
                // count change is exactly the same "content changed shape while already
                // expanded" case setExpanded() itself handles.
                remeasureKeepingTopLeft(false);
            });
    layout->addWidget(m_gallery);

    setContent(content);
}

//--------------------------------------------------------------------------

ChatReactionGalleryDropdown::~ChatReactionGalleryDropdown()
{
}

//--------------------------------------------------------------------------

void ChatReactionGalleryDropdown::setPack(std::shared_ptr<AbstractReactionIconPack> pack)
{
    m_quickBar->setPack(pack);
    m_gallery->setPack(std::move(pack));
}

//--------------------------------------------------------------------------

void ChatReactionGalleryDropdown::setOwnReactionIds(QStringList ids)
{
    m_quickBar->setOwnReactionIds(ids);
    m_gallery->setOwnReactionIds(std::move(ids));
}

//--------------------------------------------------------------------------

void ChatReactionGalleryDropdown::setExpanded(bool enable)
{
    if (m_expanded==enable)
    {
        return;
    }
    m_expanded=enable;

    m_quickBar->setVisible(!m_expanded);
    m_gallery->setVisible(m_expanded);
    if (m_expanded)
    {
        m_gallery->resetSearch();
    }

    content()->updateGeometry();
    // Keeps the collapsed quick bar's own on-screen position fixed and unfolds the gallery
    // downward beneath it (or folds it back away) -- see remeasureKeepingTopLeft()'s own doc
    // comment for why remeasure() alone is the wrong tool here: it would re-derive position from
    // the ORIGINAL anchor (popupAboveRect()'s upward growth), moving the header itself on every
    // expand/collapse instead of leaving it in place. A no-op if the frame is not currently open
    // (e.g. setExpanded() called before the first popupX()); the NEXT opening measures whichever
    // page is visible at that time from scratch.
    remeasureKeepingTopLeft(false);
}

}

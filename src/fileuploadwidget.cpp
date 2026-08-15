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

/** @file uise/desktop/src/fileuploadwidget.cpp
*
*  Defines FileUploadWidget.
*
*/

/****************************************************************************/

#include <algorithm>

#include <QLabel>
#include <QCheckBox>
#include <QBoxLayout>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QPointer>
#include <QFileDialog>
#include <QFileInfo>
#include <QApplication>
#include <QClipboard>
#include <QMimeData>
#include <QUrl>
#include <QKeyEvent>
#include <QKeySequence>
#include <QTimer>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QShowEvent>

#include <uise/desktop/style.hpp>
#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/utils/destroywidget.hpp>
#include <uise/desktop/utils/mimedatautils.hpp>
#include <uise/desktop/icontextbutton.hpp>
#include <uise/desktop/pushbutton.hpp>
#include <uise/desktop/scrollarea.hpp>
#include <uise/desktop/dropdownmenu.hpp>
#include <uise/desktop/messageeditor.hpp>
#include <uise/desktop/fileuploadlistitem.hpp>
#include <uise/desktop/fileuploadwidget.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

namespace {

std::shared_ptr<SvgIcon> fileUploadIcon(const QString& alias, QWidget* context)
{
    return Style::instance().svgIconLocator().icon(QString("FileUpload::%1").arg(alias),context);
}

// QWidget::setMinimumHeight()/setMaximumHeight() (what updateListAreaHeight() below uses to
// resize listArea) call updateGeometry(), which does not resize anything synchronously: it
// posts a QEvent::LayoutRequest to the parent layout, processed on the NEXT event loop
// iteration. So listArea itself gets the right constraints immediately, but the ACTUAL
// on-screen size of this widget (and everything above it) stays stale until that deferred
// event is processed -- which is exactly why an unrelated later interaction (a checkbox
// click, a theme toggle) appeared to "fix" a resize that a plain addItems() call did not:
// those pump the event loop and catch up the backlog, they don't cause it. Force every
// ancestor's layout to re-activate synchronously instead of waiting for it (same fix as
// FastSwitchButton's own activateLayoutsUpward(), for the same underlying Qt behavior).
void activateLayoutsUpward(QWidget* widget)
{
    for (auto* w=widget; w!=nullptr; w=w->parentWidget())
    {
        if (w->layout()!=nullptr)
        {
            w->layout()->invalidate();
            w->layout()->activate();
        }

        // layout()->activate() moves/resizes children synchronously via setGeometry(), which
        // schedules a repaint of the affected regions -- but only once this event loop turn is
        // processed. Force it explicitly instead of trusting that to happen before the next
        // paint: several widgets in this tree (headerFrame, list items, checkboxes) have an
        // explicitly transparent QSS background (see fileupload.qss), so a region vacated by a
        // shrinking/relocating listArea composites the new, transparent sibling directly over
        // whatever pixels are still sitting in the backing store from listArea's previous,
        // larger paint -- exactly the "preview list behind the rest of the content" symptom
        // seen when the widget's first real size settles well after construction, as it does
        // inside the modal dialog.
        w->update();
    }
}

// True duplicate: same file name AND same content as an item already present (either already
// in the list, or already accepted earlier in this same batch). An item that shares a name but
// carries different bytes -- e.g. an image that was edited and re-added/re-dropped under its
// original name -- is NOT a duplicate: it is a distinct piece of content that happens to share
// a name, and is kept. Items with no name yet (freshly pasted/dropped raw image data, named
// only later via ensureFileName()) never match anything here, by design.
bool isDuplicateItem(const FileUploadItem& candidate, const QByteArray& candidateData,
                      const FileUploadItems& existing, const FileUploadItems& acceptedSoFar)
{
    auto name=candidate.fileName();
    auto matches=[&](const FileUploadItem& it)
    {
        return it.fileName()==name && it.encodedData()==candidateData;
    };
    return std::any_of(existing.begin(),existing.end(),matches)
           || std::any_of(acceptedSoFar.begin(),acceptedSoFar.end(),matches);
}

FileUploadItems filterDuplicates(const FileUploadItems& existing, FileUploadItems newItems)
{
    FileUploadItems result;
    result.reserve(newItems.size());
    for (auto& candidate : newItems)
    {
        if (candidate.fileName().isEmpty())
        {
            result.push_back(std::move(candidate));
            continue;
        }

        // read the candidate's bytes once -- Type::File re-reads from disk on every
        // encodedData() call, and isDuplicateItem() may otherwise be called against several
        // same-named entries
        auto data=candidate.encodedData();
        if (!isDuplicateItem(candidate,data,existing,result))
        {
            result.push_back(std::move(candidate));
        }
    }
    return result;
}

}

//--------------------------------------------------------------------------

class FileUploadWidget_p
{
    public:

        FileUploadItems items;

        bool highQuality=false;
        bool sendAsDocuments=false;
        bool groupItems=true;
        bool rememberChoice=false;
        uint32_t maxImageAspectRatio=FileUploadWidget::DefaultMaxImageAspectRatio;

        QFrame* headerFrame=nullptr;
        QLabel* captionLabel=nullptr;
        IconTextButton* headerMenuButton=nullptr;
        QPointer<DropdownMenu> headerMenu;

        QCheckBox* groupItemsCheck=nullptr;
        QCheckBox* sendAsDocumentsCheck=nullptr;
        QCheckBox* rememberChoiceCheck=nullptr;

        ScrollArea* listArea=nullptr;
        QFrame* listContent=nullptr;
        QBoxLayout* listLayout=nullptr;
        std::vector<FileUploadListItem*> listItems;

        QLabel* commentsTitle=nullptr;
        AbstractMessageEditor* messageEditor=nullptr;

        QFrame* buttonsFrame=nullptr;
        PushButton* addButton=nullptr;
        PushButton* cancelButton=nullptr;
        PushButton* sendButton=nullptr;

        bool headerVisible=true;
        bool buttonsVisible=true;
        bool commentsVisible=true;

        int minListAreaHeight=FileUploadWidget::DefaultMinListAreaHeight;
        int maxListAreaHeight=FileUploadWidget::DefaultMaxListAreaHeight;
        int maxCommentsHeight=FileUploadWidget::DefaultMaxCommentsHeight;
        int maxCommentLength=FileUploadWidget::DefaultMaxCommentLength;

        // Deferred safety re-runs of doUpdateListAreaHeight()/comments-area activation (see
        // updateListAreaHeight()/updateCommentsAreaHeight()). Single-shot and restarted, not
        // fired-and-forgotten, so N calls in a row coalesce into one pending re-run instead of
        // N independent QTimer::singleShot() timers. Parented to the widget in the constructor,
        // so no QPointer guard is needed in the connected lambdas.
        QTimer* heightUpdateTimer=nullptr;
        QTimer* commentsUpdateTimer=nullptr;
};

//--------------------------------------------------------------------------

FileUploadWidget::FileUploadWidget(QWidget* parent)
    : AbstractFileUploadWidget(parent),
      pimpl(std::make_unique<FileUploadWidget_p>())
{
    setAcceptDrops(true);

    // paste (see keyPressEvent()/showEvent() below) must work over the whole widget, not just
    // after the user happens to click something focusable first -- an unhandled key event
    // bubbles up from whatever child currently has focus to this widget automatically (that
    // part already works for the checkboxes/buttons with no code needed here), but if NOTHING
    // in the subtree has focus yet -- e.g. right after the widget/dialog is first shown --
    // there is no key event target at all. StrongFocus plus grabbing focus on show (below)
    // gives paste a default target immediately, without requiring an initial click anywhere.
    setFocusPolicy(Qt::StrongFocus);

    pimpl->heightUpdateTimer=new QTimer(this);
    pimpl->heightUpdateTimer->setSingleShot(true);
    pimpl->heightUpdateTimer->setInterval(50);
    connect(pimpl->heightUpdateTimer,&QTimer::timeout,this,&FileUploadWidget::doUpdateListAreaHeight);

    pimpl->commentsUpdateTimer=new QTimer(this);
    pimpl->commentsUpdateTimer->setSingleShot(true);
    pimpl->commentsUpdateTimer->setInterval(50);
    connect(
        pimpl->commentsUpdateTimer,
        &QTimer::timeout,
        this,
        [this]()
        {
            activateLayoutsUpward(pimpl->messageEditor->qWidget());
        }
    );

    auto* topLayout=Layout::vertical(this);

    // --- header: caption + drop-down menu (visible only while items() has an image) ---

    pimpl->headerFrame=new QFrame(this);
    pimpl->headerFrame->setObjectName("headerFrame");
    auto* headerLayout=Layout::horizontal(pimpl->headerFrame);
    topLayout->addWidget(pimpl->headerFrame);

    pimpl->captionLabel=new QLabel(pimpl->headerFrame);
    pimpl->captionLabel->setObjectName("caption");
    headerLayout->addWidget(pimpl->captionLabel,1);
    connect(this,&AbstractFileUploadWidget::captionChanged,pimpl->captionLabel,&QLabel::setText);

    pimpl->headerMenuButton=new IconTextButton(
        fileUploadIcon(QStringLiteral("menu"),pimpl->headerFrame),
        pimpl->headerFrame,
        IconTextButton::IconPosition::BeforeText
    );
    pimpl->headerMenuButton->setObjectName("headerMenuButton");
    pimpl->headerMenuButton->setText(QString());
    pimpl->headerMenuButton->setVisible(false);
    headerLayout->addWidget(pimpl->headerMenuButton);

    // DropdownMenu is constructed parentless -- DropdownFrame reparents itself lazily to the
    // trigger's actual window() on first opening (see FileUploadListItem for the same pattern)
    pimpl->headerMenu=new DropdownMenu();
    pimpl->headerMenu->setItems({
        MenuItem::checkable(static_cast<int>(FileUploadMenuAction::HighQuality),tr("High quality"),pimpl->highQuality,fileUploadIcon(QStringLiteral("highQuality"),this)),
        MenuItem::checkable(static_cast<int>(FileUploadMenuAction::SendAsDocuments),tr("Send as documents"),pimpl->sendAsDocuments,fileUploadIcon(QStringLiteral("documents"),this)),
        MenuItem::checkable(static_cast<int>(FileUploadMenuAction::GroupItems),tr("Group items"),pimpl->groupItems,fileUploadIcon(QStringLiteral("group"),this))
    });
    pimpl->headerMenu->attachTo(pimpl->headerMenuButton);
    connect(
        pimpl->headerMenu,
        &DropdownMenu::itemToggled,
        this,
        [this](int id, bool checked)
        {
            switch (static_cast<FileUploadMenuAction>(id))
            {
                case (FileUploadMenuAction::HighQuality): setHighQuality(checked); break;
                case (FileUploadMenuAction::SendAsDocuments): setSendAsDocuments(checked); break;
                case (FileUploadMenuAction::GroupItems): setGroupItems(checked); break;
            }
        }
    );

    // --- preview list ---

    pimpl->listArea=new ScrollArea(this);
    pimpl->listArea->setObjectName("listArea");
    pimpl->listArea->setWidgetResizable(true);
    pimpl->listArea->setFocusPolicy(Qt::StrongFocus);
    topLayout->addWidget(pimpl->listArea,1);

    pimpl->listContent=new QFrame(pimpl->listArea);
    pimpl->listLayout=Layout::vertical(pimpl->listContent);
    pimpl->listLayout->addStretch(1);
    pimpl->listArea->setWidget(pimpl->listContent);

    connect(
        pimpl->listArea->verticalScrollBar(),
        &QScrollBar::valueChanged,
        this,
        [this](int)
        {
            for (auto* row : pimpl->listItems)
            {
                row->closeMenu();
            }
        }
    );

    // --- checkboxes ---

    pimpl->groupItemsCheck=new QCheckBox(tr("Group items"),this);
    pimpl->groupItemsCheck->setObjectName("groupItemsCheck");
    pimpl->groupItemsCheck->setChecked(pimpl->groupItems);
    topLayout->addWidget(pimpl->groupItemsCheck);
    connect(pimpl->groupItemsCheck,&QCheckBox::toggled,this,&FileUploadWidget::setGroupItems);

    pimpl->sendAsDocumentsCheck=new QCheckBox(tr("Send as documents"),this);
    pimpl->sendAsDocumentsCheck->setObjectName("sendAsDocumentsCheck");
    pimpl->sendAsDocumentsCheck->setVisible(false);
    topLayout->addWidget(pimpl->sendAsDocumentsCheck);
    connect(pimpl->sendAsDocumentsCheck,&QCheckBox::toggled,this,&FileUploadWidget::setSendAsDocuments);

    pimpl->rememberChoiceCheck=new QCheckBox(tr("Remember this choice"),this);
    pimpl->rememberChoiceCheck->setObjectName("rememberChoiceCheck");
    topLayout->addWidget(pimpl->rememberChoiceCheck);
    connect(pimpl->rememberChoiceCheck,&QCheckBox::toggled,this,&FileUploadWidget::setRememberChoice);

    // --- comments ---

    pimpl->commentsTitle=new QLabel(tr("Comments"),this);
    pimpl->commentsTitle->setObjectName("commentsTitle");
    topLayout->addWidget(pimpl->commentsTitle);

    // AbstractMessageEditor is not registered in defaultwidgetfactory.cpp, so the two-arg
    // form (with MessageEditor as the fallback) is mandatory here
    pimpl->messageEditor=makeWidget<AbstractMessageEditor,MessageEditor>(this);
    pimpl->messageEditor->setPlaceHolderText(tr("Add a comment..."));
    // this is a multi-line comment field, not a single-line chat box: Enter should insert a
    // newline like any normal text editor, not submit -- there is no "submit" concept here at
    // all, sending only ever happens via the Send button
    pimpl->messageEditor->setFinishOnEnter(false);
    // AbstractMessageEditor's concrete editor grows unbounded with its content (see
    // EnhancedTextEdit::sizeHint()) and has no max-length of its own; both are capped here,
    // from outside, rather than by reaching into MessageEditor's internals
    pimpl->messageEditor->qWidget()->setMaximumHeight(pimpl->maxCommentsHeight);
    topLayout->addWidget(pimpl->messageEditor->qWidget());
    connect(
        pimpl->messageEditor,
        &AbstractMessageEditor::textChanged,
        this,
        [this]()
        {
            enforceMaxCommentLength();
            updateCommentsAreaHeight();
        }
    );

    // --- buttons: Add on the left, Cancel/Send on the right ---
    //
    // Dialog<>::doSetButtons() lays out every button in one QHBoxLayout with a single
    // alignment (see ipp/dialog.ipp), so it cannot split Add-left / Cancel-Send-right --
    // this widget keeps its own button row instead, in both standalone and dialog-embedded
    // use (see FileUploadDialog).

    pimpl->buttonsFrame=new QFrame(this);
    pimpl->buttonsFrame->setObjectName("buttonsFrame");
    auto* buttonsLayout=Layout::horizontal(pimpl->buttonsFrame);
    topLayout->addWidget(pimpl->buttonsFrame);

    pimpl->addButton=new PushButton(tr("Add"),pimpl->buttonsFrame);
    pimpl->addButton->setObjectName("addButton");
    buttonsLayout->addWidget(pimpl->addButton);
    connect(pimpl->addButton,&PushButton::clicked,this,[this](){ emit addRequested(); requestAddFiles(); });

    buttonsLayout->addStretch(1);

    pimpl->cancelButton=new PushButton(tr("Cancel"),pimpl->buttonsFrame);
    pimpl->cancelButton->setObjectName("cancelButton");
    buttonsLayout->addWidget(pimpl->cancelButton);
    connect(pimpl->cancelButton,&PushButton::clicked,this,&AbstractFileUploadWidget::cancelled);

    pimpl->sendButton=new PushButton(tr("Send"),pimpl->buttonsFrame);
    pimpl->sendButton->setObjectName("sendButton");
    buttonsLayout->addWidget(pimpl->sendButton);
    connect(pimpl->sendButton,&PushButton::clicked,this,&AbstractFileUploadWidget::sendRequested);

    updateCaption();
    updateSendEnabled();
    updateAddEnabled();
    updateMenuVisibility();
    updateListAreaHeight();
}

//--------------------------------------------------------------------------

FileUploadWidget::~FileUploadWidget()
{
    if (!pimpl->headerMenu.isNull())
    {
        destroyWidget(pimpl->headerMenu);
    }
}

//--------------------------------------------------------------------------

const FileUploadItems& FileUploadWidget::items() const
{
    return pimpl->items;
}

//--------------------------------------------------------------------------

void FileUploadWidget::addRowFor(const FileUploadItem& source)
{
    auto it=source;
    it.setMaxImageAspectRatio(pimpl->maxImageAspectRatio);
    pimpl->items.push_back(it);

    auto* row=new FileUploadListItem(pimpl->listContent);
    row->setItem(it);
    applyViewToRow(row,static_cast<int>(pimpl->items.size())-1);
    pimpl->listLayout->insertWidget(pimpl->listLayout->count()-1,row);
    pimpl->listItems.push_back(row);

    connect(
        row,
        &FileUploadListItem::editRequested,
        this,
        [this,row]()
        {
            auto idx=indexOfRow(row);
            if (idx>=0)
            {
                emit editImageRequested(idx);
            }
        }
    );
    connect(
        row,
        &FileUploadListItem::previewClicked,
        this,
        [this,row]()
        {
            auto idx=indexOfRow(row);
            if (idx>=0)
            {
                emit editImageRequested(idx);
            }
        }
    );
    connect(
        row,
        &FileUploadListItem::renameRequested,
        this,
        [this,row](const QString& name)
        {
            auto idx=indexOfRow(row);
            if (idx>=0)
            {
                pimpl->items[static_cast<size_t>(idx)].setFileName(name);
            }
        }
    );
    connect(
        row,
        &FileUploadListItem::removeRequested,
        this,
        [this,row]()
        {
            auto idx=indexOfRow(row);
            if (idx>=0)
            {
                removeItem(idx);
            }
        }
    );
}

//--------------------------------------------------------------------------

int FileUploadWidget::indexOfRow(FileUploadListItem* row) const
{
    auto it=std::find(pimpl->listItems.begin(),pimpl->listItems.end(),row);
    if (it==pimpl->listItems.end())
    {
        return -1;
    }
    return static_cast<int>(std::distance(pimpl->listItems.begin(),it));
}

//--------------------------------------------------------------------------

void FileUploadWidget::applyViewToRow(FileUploadListItem* row, int index)
{
    auto view=(row->item().presentAsImage() && !pimpl->sendAsDocuments)
                ? FileUploadListItem::View::Image
                : FileUploadListItem::View::Row;
    if (view==FileUploadListItem::View::Row &&
        index>=0 && static_cast<size_t>(index)<pimpl->items.size())
    {
        // Row view always displays a name; a pasted/generated buffer-sourced item has none
        // until named. ensureFileName() is a no-op if the item is already named (e.g. a real
        // Type::File item, or one renamed earlier).
        if (pimpl->items[static_cast<size_t>(index)].ensureFileName(index))
        {
            row->setItem(pimpl->items[static_cast<size_t>(index)]);
        }
    }
    row->setView(view);
}

//--------------------------------------------------------------------------

int FileUploadWidget::addItems(FileUploadItems newItems)
{
    auto wasEmpty=pimpl->items.empty();

    auto toInsert=filterDuplicates(pimpl->items,std::move(newItems));

    // maxFileCount()<=0 means "no limit". Otherwise the whole incoming bunch is rejected as one
    // unit when it would push the total over the limit -- no more silent partial-add truncation,
    // see maxFileCountExceeded()'s own doc comment.
    auto limit=maxFileCount();
    if (limit>0 && static_cast<int>(pimpl->items.size())+static_cast<int>(toInsert.size())>limit)
    {
        emit maxFileCountExceeded(static_cast<int>(toInsert.size()));
        updateItemsState(wasEmpty,true);
        return 0;
    }

    for (const auto& item : toInsert)
    {
        addRowFor(item);
    }

    updateItemsState(wasEmpty,true);
    return static_cast<int>(toInsert.size());
}

//--------------------------------------------------------------------------

void FileUploadWidget::setItems(FileUploadItems newItems)
{
    // NOT implemented as clearItemsInternal()+addItems(): that would emit emptied() for the
    // clear step even when newItems is non-empty, misreporting a plain replace as a
    // momentary empty-then-refill transition
    auto wasEmpty=pimpl->items.empty();

    for (auto* row : pimpl->listItems)
    {
        destroyWidget(row);
    }
    pimpl->listItems.clear();
    pimpl->items.clear();

    auto toInsert=filterDuplicates({},std::move(newItems));

    // Same "reject the whole bunch, no truncation" rule as addItems() -- pimpl->items was just
    // cleared above, so the check is against toInsert alone.
    auto limit=maxFileCount();
    if (limit>0 && static_cast<int>(toInsert.size())>limit)
    {
        emit maxFileCountExceeded(static_cast<int>(toInsert.size()));
        updateItemsState(wasEmpty,true);
        return;
    }

    for (const auto& item : toInsert)
    {
        addRowFor(item);
    }

    updateItemsState(wasEmpty,true);
}

//--------------------------------------------------------------------------

int FileUploadWidget::addFiles(const QStringList& paths)
{
    FileUploadItems newItems;
    newItems.reserve(static_cast<size_t>(paths.size()));
    for (const auto& p : paths)
    {
        newItems.push_back(FileUploadItem::fromFile(p));
    }
    return addItems(std::move(newItems));
}

//--------------------------------------------------------------------------

void FileUploadWidget::removeItem(int index)
{
    if (index<0 || static_cast<size_t>(index)>=pimpl->items.size())
    {
        return;
    }

    auto wasEmpty=pimpl->items.empty();

    auto* row=pimpl->listItems[static_cast<size_t>(index)];
    destroyWidget(row);

    pimpl->listItems.erase(pimpl->listItems.begin()+index);
    pimpl->items.erase(pimpl->items.begin()+index);

    updateItemsState(wasEmpty,true);
}

//--------------------------------------------------------------------------

void FileUploadWidget::clearItemsInternal(bool notifyEmptied)
{
    auto wasEmpty=pimpl->items.empty();

    for (auto* row : pimpl->listItems)
    {
        destroyWidget(row);
    }
    pimpl->listItems.clear();
    pimpl->items.clear();

    updateItemsState(wasEmpty,notifyEmptied);
}

//--------------------------------------------------------------------------

void FileUploadWidget::clearItems()
{
    clearItemsInternal(true);
}

//--------------------------------------------------------------------------

void FileUploadWidget::updateItemsState(bool wasEmpty, bool notifyEmptied)
{
    updateCaption();
    updateSendEnabled();
    updateAddEnabled();
    updateMenuVisibility();
    updateListAreaHeight();
    emit itemsChanged();
    if (notifyEmptied && !wasEmpty && pimpl->items.empty())
    {
        emit emptied();
    }
}

//--------------------------------------------------------------------------

void FileUploadWidget::updateCaption()
{
    auto n=static_cast<int>(pimpl->items.size());
    if (n==0)
    {
        setCaption(tr("Send files"));
        return;
    }

    // presentAsImage(), not isImage(): the caption should match what the rows actually show --
    // an extreme-aspect-ratio image stages as a document row, so it should read as a file here
    // too, even though it is still isImage()==true underneath (editable, has a thumbnail).
    auto imageCount=std::count_if(
        pimpl->items.begin(),
        pimpl->items.end(),
        [](const FileUploadItem& it){ return it.presentAsImage(); }
    );

    if (n==1)
    {
        setCaption(imageCount==1 ? tr("Send an image") : tr("Send as a file"));
        return;
    }

    if (imageCount==n)
    {
        setCaption(tr("%1 images selected").arg(n));
    }
    else
    {
        setCaption(tr("%1 files selected").arg(n));
    }
}

//--------------------------------------------------------------------------

void FileUploadWidget::updateSendEnabled()
{
    pimpl->sendButton->setEnabled(!pimpl->items.empty());
}

//--------------------------------------------------------------------------

bool FileUploadWidget::isAtMaxFileCount() const
{
    return static_cast<int>(pimpl->items.size())>=maxFileCount();
}

//--------------------------------------------------------------------------

void FileUploadWidget::updateAddEnabled()
{
    pimpl->addButton->setEnabled(!isAtMaxFileCount());
}

//--------------------------------------------------------------------------

void FileUploadWidget::updateMenuVisibility()
{
    // presentAsImage(), not isImage(): "High quality"/"Send as documents" are only meaningful
    // for an item that would otherwise actually be shown as an image tile -- an item already
    // forced to a document row by the aspect-ratio guard gets no visible effect from either.
    auto hasImage=std::any_of(
        pimpl->items.begin(),
        pimpl->items.end(),
        [](const FileUploadItem& it){ return it.presentAsImage(); }
    );
    auto hasMultipleItems=pimpl->items.size()>1;

    pimpl->headerMenuButton->setVisible(hasImage);
    pimpl->sendAsDocumentsCheck->setVisible(hasImage);
    pimpl->groupItemsCheck->setVisible(hasMultipleItems);
    // rememberChoiceCheck covers whichever of the other two toggles is actually offered --
    // with neither visible there is nothing left to remember
    pimpl->rememberChoiceCheck->setVisible(hasImage || hasMultipleItems);

    // the two header-menu items that have a checkbox counterpart follow the same rule as
    // their checkbox (see above); HighQuality has no checkbox and stays governed solely by
    // headerMenuButton's own visibility (hasImage), same as before
    pimpl->headerMenu->setItemVisible(static_cast<int>(FileUploadMenuAction::SendAsDocuments),hasImage);
    pimpl->headerMenu->setItemVisible(static_cast<int>(FileUploadMenuAction::GroupItems),hasMultipleItems);
}

//--------------------------------------------------------------------------

void FileUploadWidget::updateListAreaHeight()
{
    doUpdateListAreaHeight();

    // Belt and suspenders: doUpdateListAreaHeight() forces a synchronous repolish and
    // relayout of everything it can reach, but a widget added moments ago (mid-add, this
    // very call) has not necessarily been through a real QEvent::Show/Polish/LayoutRequest
    // cycle yet, and when this widget sits inside a QScrollArea (as in the demo), growing is
    // also gated by QScrollArea's OWN content-widget tracking, which is itself driven by an
    // event filter reacting to events Qt POSTS rather than sends. No amount of synchronous
    // forcing from inside this call is a hard guarantee those have settled by the time it
    // returns -- which is consistent with a checkbox click or theme toggle "fixing" a resize
    // that adding an item did not: both merely pump the event loop for unrelated reasons and
    // happen to let the backlog catch up. Schedule an explicit repeat of the FULL update (not
    // just layout activation) after a short real delay, rather than depending on a stray
    // event to do it by accident. doUpdateListAreaHeight() does not itself re-schedule
    // anything, so this cannot chain into a repeating timer. The timer is single-shot and
    // restarted (not a fresh QTimer::singleShot() per call), so a burst of calls (e.g.
    // addItems() with several files, followed immediately by setSendAsDocuments()) coalesces
    // into one pending re-run instead of N -- it still fires >=50ms after the LAST call, which
    // is a later and therefore strictly safer deadline than 50ms after the first.
    pimpl->heightUpdateTimer->start();
}

//--------------------------------------------------------------------------

void FileUploadWidget::updateCommentsAreaHeight()
{
    // EnhancedTextEdit::updateSize() (see messageeditor.cpp) reacts to typed content by
    // calling updateGeometry() alone, which -- exactly like the setMinimumHeight()/
    // setMaximumHeight() calls doUpdateListAreaHeight() makes above -- only POSTS a
    // QEvent::LayoutRequest for the next event loop turn rather than resizing anything
    // synchronously. Same two-part fix as updateListAreaHeight(): force every ancestor layout
    // to re-activate now (pimpl->messageEditor->qWidget() is the MessageEditor wrapper frame,
    // not the inner EnhancedTextEdit -- its own child layout is exactly what needs to
    // re-query the editor's new sizeHint()), then repeat after a short real delay in case
    // something in the chain (a QScrollArea ancestor's own posted-event-driven content
    // tracking, same as the list area) has not settled by the time this call returns. This
    // fires on every keystroke, so the re-run timer is coalesced the same way as
    // updateListAreaHeight()'s, rather than allocating a fresh QTimer::singleShot() per call.
    activateLayoutsUpward(pimpl->messageEditor->qWidget());
    pimpl->commentsUpdateTimer->start();
}

//--------------------------------------------------------------------------

void FileUploadWidget::doUpdateListAreaHeight()
{
    // repolish/ensurePolished the list content, NOT `this`: Style::repolishRecursive() does
    // style->unpolish(w)+style->polish(w), which re-applies every "qproperty-*" QSS rule --
    // including uise--FileUploadWidget's own qproperty-minListAreaHeight/maxListAreaHeight,
    // whose setters land right back here (see deferListAreaHeightUpdate()). Repolishing
    // `this` would re-enter Qt's style engine on a widget already mid-polish when called from
    // a property setter (a real crash, seen in practice), and even outside that call path it
    // would turn every resize into an unbounded polish->setter->repolish loop. What actually
    // needs a fresh polish pass is the list content, not the header/checkboxes/editor, since
    // those are already-shown widgets whose geometry isn't what's stale here -- only a
    // just-inserted row is.
    Style::repolishRecursive(pimpl->listContent);
    pimpl->listContent->ensurePolished();
    const auto rows=pimpl->listContent->findChildren<QWidget*>();
    for (auto* w : rows)
    {
        w->ensurePolished();
    }

    // two-pass measurement, same as DropdownFrame::measureContentSize(): a row added (or
    // reparented out) just now propagates its own layout's invalidation upwards via a
    // posted, asynchronous QEvent::LayoutRequest, so a single synchronous invalidate()+
    // activate() can still answer sizeHint() from a stale cache -- pass 1 primes geometry
    // with the initial hint, pass 2 re-measures after the subtree has gone through a real
    // layout cycle. This gets the CONTENT HEIGHT NUMBER right; activateLayoutsUpward() below
    // is the other half -- getting that number actually reflected on screen.
    auto invalidateAll=[this,&rows]()
    {
        pimpl->listLayout->invalidate();
        for (auto* w : rows)
        {
            if (w->layout()!=nullptr)
            {
                w->layout()->invalidate();
            }
        }
    };

    int contentHeight=0;
    for (int pass=0;pass<2;++pass)
    {
        invalidateAll();
        pimpl->listLayout->activate();

        auto hint=pimpl->listContent->sizeHint().height();
        if (pass>0 && hint==contentHeight)
        {
            break;
        }
        contentHeight=hint;
    }

    // minListAreaHeight exists to keep a multi-row, scrollable list from collapsing to a
    // sliver -- headroom a lone row doesn't need. With exactly one item, contentHeight already
    // IS that row's own natural height (the two-pass measurement above), so drop the floor to
    // it instead of the general minimum, letting the list area (and, via
    // ModalFileUploadDialog's setPopupAutoHeight(true), the whole popup) shrink to fit a
    // single-file send.
    auto minHeight=pimpl->minListAreaHeight;
    if (pimpl->listItems.size()==1)
    {
        minHeight=qMin(minHeight,contentHeight);
    }

    auto h=qBound(minHeight,contentHeight,pimpl->maxListAreaHeight);

    pimpl->listArea->setMinimumHeight(h);
    pimpl->listArea->setMaximumHeight(h);

    // see the comment above: the two lines above only give listArea the right constraints,
    // they do not resize anything on screen by themselves.
    //
    // Deliberately no explicit resize()/adjustSize() call here on top of this: an earlier
    // version called resize(width(), sizeHint().height()) as an extra nudge, but the very
    // first call to this function happens from the constructor, before this widget has ever
    // been through a real layout pass -- inside FileUploadDialog specifically, width() at
    // that point can still be 0 (or some other not-yet-laid-out placeholder), and locking
    // that in as the widget's actual geometry, right before the modal popup sizes itself
    // from it, is what shrank the whole dialog to a tiny top-left rectangle. activate() calls
    // (here and in the deferred re-run below) already call setGeometry() on any child whose
    // size actually changed, which does send a real, synchronous QResizeEvent -- there is
    // nothing this extra call was doing that activateLayoutsUpward() does not already cover,
    // without the risk of freezing a bad size before the real layout has had a chance to run.
    activateLayoutsUpward(this);
}

//--------------------------------------------------------------------------

void FileUploadWidget::setMaxListAreaHeight(int height)
{
    pimpl->maxListAreaHeight=height;
    deferListAreaHeightUpdate();
}

int FileUploadWidget::maxListAreaHeight() const noexcept
{
    return pimpl->maxListAreaHeight;
}

//--------------------------------------------------------------------------

void FileUploadWidget::setMinListAreaHeight(int height)
{
    pimpl->minListAreaHeight=height;
    deferListAreaHeightUpdate();
}

int FileUploadWidget::minListAreaHeight() const noexcept
{
    return pimpl->minListAreaHeight;
}

//--------------------------------------------------------------------------

void FileUploadWidget::deferListAreaHeightUpdate()
{
    // setMinListAreaHeight()/setMaxListAreaHeight() are Q_PROPERTY writers, and Qt's style
    // engine calls them DURING polish, to apply "qproperty-minListAreaHeight"/"qproperty-
    // maxListAreaHeight" from fileupload.qss (see the qt_static_metacall frame in a crash
    // report if this guard is ever removed). Calling updateListAreaHeight() -- which
    // repolishes this same widget via doUpdateListAreaHeight() -- synchronously from inside
    // that call would re-enter Qt's style sheet engine on a widget it is still in the middle
    // of polishing, which crashes. Defer to the next event loop turn instead, by which point
    // the in-progress polish has fully unwound.
    QPointer<FileUploadWidget> guard(this);
    QMetaObject::invokeMethod(
        this,
        [guard]()
        {
            if (!guard.isNull())
            {
                guard->updateListAreaHeight();
            }
        },
        Qt::QueuedConnection
    );
}

//--------------------------------------------------------------------------

void FileUploadWidget::setMaxCommentsHeight(int height)
{
    pimpl->maxCommentsHeight=height;
    pimpl->messageEditor->qWidget()->setMaximumHeight(height);
}

int FileUploadWidget::maxCommentsHeight() const noexcept
{
    return pimpl->maxCommentsHeight;
}

//--------------------------------------------------------------------------

void FileUploadWidget::setMaxCommentLength(int length)
{
    pimpl->maxCommentLength=length;
    enforceMaxCommentLength();
}

int FileUploadWidget::maxCommentLength() const noexcept
{
    return pimpl->maxCommentLength;
}

//--------------------------------------------------------------------------

void FileUploadWidget::enforceMaxCommentLength()
{
    if (pimpl->maxCommentLength<=0)
    {
        return;
    }

    auto plain=pimpl->messageEditor->text(TextFormat::Plain);
    if (plain.length()<=pimpl->maxCommentLength)
    {
        return;
    }

    auto truncated=plain.left(pimpl->maxCommentLength);

    // block textChanged() around the rewrite -- loadText() below fires it again via the
    // editor's own setPlainText(), which would otherwise re-enter this method
    QSignalBlocker b(pimpl->messageEditor);
    pimpl->messageEditor->loadText(truncated,TextFormat::Plain);
}

//--------------------------------------------------------------------------

QImage FileUploadWidget::itemImage(int index) const
{
    if (index<0 || static_cast<size_t>(index)>=pimpl->items.size())
    {
        return QImage();
    }
    return pimpl->items[static_cast<size_t>(index)].image();
}

//--------------------------------------------------------------------------

void FileUploadWidget::setItemImage(int index, QImage image)
{
    if (index<0 || static_cast<size_t>(index)>=pimpl->items.size())
    {
        return;
    }
    pimpl->items[static_cast<size_t>(index)].setImage(std::move(image));

    auto* row=pimpl->listItems[static_cast<size_t>(index)];
    row->setItem(pimpl->items[static_cast<size_t>(index)]);
    applyViewToRow(row,index);

    updateCaption();
    updateMenuVisibility();
    updateListAreaHeight();
    emit itemsChanged();
}

//--------------------------------------------------------------------------

void FileUploadWidget::setItemFileName(int index, const QString& name)
{
    if (index<0 || static_cast<size_t>(index)>=pimpl->items.size())
    {
        return;
    }
    pimpl->items[static_cast<size_t>(index)].setFileName(name);
    pimpl->listItems[static_cast<size_t>(index)]->refresh();
}

//--------------------------------------------------------------------------

bool FileUploadWidget::isHighQuality() const
{
    return pimpl->highQuality;
}

void FileUploadWidget::setHighQuality(bool enable)
{
    if (pimpl->highQuality==enable)
    {
        return;
    }
    pimpl->highQuality=enable;
    pimpl->headerMenu->setItemChecked(static_cast<int>(FileUploadMenuAction::HighQuality),enable);
}

//--------------------------------------------------------------------------

bool FileUploadWidget::isSendAsDocuments() const
{
    return pimpl->sendAsDocuments;
}

void FileUploadWidget::setSendAsDocuments(bool enable)
{
    if (pimpl->sendAsDocuments==enable)
    {
        return;
    }
    pimpl->sendAsDocuments=enable;

    pimpl->headerMenu->setItemChecked(static_cast<int>(FileUploadMenuAction::SendAsDocuments),enable);
    {
        QSignalBlocker b(pimpl->sendAsDocumentsCheck);
        pimpl->sendAsDocumentsCheck->setChecked(enable);
    }

    for (size_t i=0;i<pimpl->listItems.size();++i)
    {
        applyViewToRow(pimpl->listItems[i],static_cast<int>(i));
    }
    updateListAreaHeight();
}

//--------------------------------------------------------------------------

uint32_t FileUploadWidget::maxImageAspectRatio() const noexcept
{
    return pimpl->maxImageAspectRatio;
}

void FileUploadWidget::setMaxImageAspectRatio(uint32_t ratio)
{
    if (pimpl->maxImageAspectRatio==ratio)
    {
        return;
    }
    pimpl->maxImageAspectRatio=ratio;

    // Re-stamp every item already staged, same mutate-then-resync pattern as
    // setItemImage() above: pimpl->items is the source of truth, each row's
    // own copy is refreshed via setItem() afterward.
    for (size_t i=0;i<pimpl->items.size();++i)
    {
        pimpl->items[i].setMaxImageAspectRatio(ratio);
        auto* row=pimpl->listItems[i];
        row->setItem(pimpl->items[i]);
        applyViewToRow(row,static_cast<int>(i));
    }

    updateCaption();
    updateMenuVisibility();
    updateListAreaHeight();
}

//--------------------------------------------------------------------------

bool FileUploadWidget::isGroupItems() const
{
    return pimpl->groupItems;
}

void FileUploadWidget::setGroupItems(bool enable)
{
    if (pimpl->groupItems==enable)
    {
        return;
    }
    pimpl->groupItems=enable;

    pimpl->headerMenu->setItemChecked(static_cast<int>(FileUploadMenuAction::GroupItems),enable);
    QSignalBlocker b(pimpl->groupItemsCheck);
    pimpl->groupItemsCheck->setChecked(enable);
}

//--------------------------------------------------------------------------

bool FileUploadWidget::isRememberChoice() const
{
    return pimpl->rememberChoice;
}

void FileUploadWidget::setRememberChoice(bool enable)
{
    if (pimpl->rememberChoice==enable)
    {
        return;
    }
    pimpl->rememberChoice=enable;

    QSignalBlocker b(pimpl->rememberChoiceCheck);
    pimpl->rememberChoiceCheck->setChecked(enable);
}

//--------------------------------------------------------------------------

FileUploadOptions FileUploadWidget::options() const
{
    FileUploadOptions opts;
    opts.highQuality=pimpl->highQuality;
    opts.sendAsDocuments=pimpl->sendAsDocuments;
    opts.groupItems=pimpl->groupItems;
    opts.rememberChoice=pimpl->rememberChoice;
    opts.comment=pimpl->messageEditor->text();
    opts.commentFormat=TextFormat::Markdown;
    return opts;
}

//--------------------------------------------------------------------------

AbstractMessageEditor* FileUploadWidget::messageEditor() const
{
    return pimpl->messageEditor;
}

//--------------------------------------------------------------------------

IconTextButton* FileUploadWidget::menuButton() const
{
    return pimpl->headerMenuButton;
}

//--------------------------------------------------------------------------

void FileUploadWidget::setHeaderVisible(bool visible)
{
    pimpl->headerVisible=visible;
    pimpl->headerFrame->setVisible(visible);
}

//--------------------------------------------------------------------------

void FileUploadWidget::setButtonsVisible(bool visible)
{
    pimpl->buttonsVisible=visible;
    pimpl->buttonsFrame->setVisible(visible);
}

//--------------------------------------------------------------------------

void FileUploadWidget::setCommentsVisible(bool visible)
{
    pimpl->commentsVisible=visible;
    pimpl->commentsTitle->setVisible(visible);
    pimpl->messageEditor->qWidget()->setVisible(visible);
}

//--------------------------------------------------------------------------

void FileUploadWidget::settleLayout()
{
    // qproperty-minListAreaHeight/maxListAreaHeight (fileupload.qss) are applied by Qt's style
    // engine during polish; measuring before that would lock in the C++ defaults and let the
    // QSS values arrive later (via deferListAreaHeightUpdate()), i.e. after the popup hosting
    // this widget is already on screen. ensurePolished() is idempotent -- unlike
    // Style::repolishRecursive() (unpolish+polish), it does NOT re-fire those setters on a
    // widget that has already been polished once, so it cannot manufacture that same
    // post-show refit here.
    ensurePolished();
    doUpdateListAreaHeight();

    // Keep the deferred safety re-run ARMED rather than cancelling it: this is exactly the
    // path (first open, freshly built rows inside a QScrollArea) updateListAreaHeight()'s own
    // timer exists for. A re-run that finds the height unchanged is inert -- setMinimumHeight()/
    // setMaximumHeight() early-return when the value does not actually change -- so leaving it
    // armed cannot cause a visible refit.
    pimpl->heightUpdateTimer->start();
}

//--------------------------------------------------------------------------

void FileUploadWidget::reset()
{
    clearItemsInternal(false);

    pimpl->messageEditor->clear();

    // set every toggle back to its construction default directly and sync ALL of their UI
    // unconditionally -- not via setHighQuality()/setSendAsDocuments()/etc, whose early
    // "already at this value" guards are the right call for external API users but are
    // exactly what must NOT gate a reset: if this ran right after construction (nothing
    // ever touched), or after a PRIOR reset, every one of those guards would trip and skip
    // its sync, so nothing here may assume the widget isn't already in the target state
    pimpl->highQuality=false;
    pimpl->sendAsDocuments=false;
    pimpl->groupItems=true;
    pimpl->rememberChoice=false;

    pimpl->headerMenu->setItemChecked(static_cast<int>(FileUploadMenuAction::HighQuality),false);
    pimpl->headerMenu->setItemChecked(static_cast<int>(FileUploadMenuAction::SendAsDocuments),false);
    pimpl->headerMenu->setItemChecked(static_cast<int>(FileUploadMenuAction::GroupItems),true);
    pimpl->headerMenu->closeDropdown(true);

    {
        QSignalBlocker b(pimpl->sendAsDocumentsCheck);
        pimpl->sendAsDocumentsCheck->setChecked(false);
    }
    {
        QSignalBlocker b(pimpl->groupItemsCheck);
        pimpl->groupItemsCheck->setChecked(true);
    }
    {
        QSignalBlocker b(pimpl->rememberChoiceCheck);
        pimpl->rememberChoiceCheck->setChecked(false);
    }

    pimpl->listArea->verticalScrollBar()->setValue(0);

    // force a fresh layout/paint pass rather than trusting whatever is already scheduled --
    // reset() is exactly the kind of bulk, multi-property change where relying on each
    // individual setter's own update scheduling is the least reliable option
    updateListAreaHeight();
    Style::repolishRecursive(this);
    updateGeometry();
    update();
}

//--------------------------------------------------------------------------

void FileUploadWidget::pasteFromClipboard()
{
    auto* clipboard=QApplication::clipboard();
    if (clipboard==nullptr || clipboard->mimeData()==nullptr)
    {
        return;
    }
    addFromMimeData(clipboard->mimeData());
}

//--------------------------------------------------------------------------

void FileUploadWidget::addFromMimeData(const QMimeData* mimeData)
{
    if (mimeData==nullptr)
    {
        return;
    }

    if (mimeData->hasUrls())
    {
        QStringList paths;
        const auto urls=mimeData->urls();
        for (const auto& url : urls)
        {
            if (url.isLocalFile())
            {
                QFileInfo fi(url.toLocalFile());
                if (fi.isFile())
                {
                    paths.push_back(fi.filePath());
                }
            }
        }
        if (!paths.isEmpty())
        {
            addFiles(paths);
            return;
        }
    }

    if (mimeData->hasImage())
    {
        auto image=qvariant_cast<QImage>(mimeData->imageData());
        if (!image.isNull())
        {
            addItems({FileUploadItem::fromImage(image)});
            return;
        }
    }

    for (const auto& fmt : acceptedImageMimeFormats())
    {
        if (mimeData->hasFormat(fmt))
        {
            auto bytes=mimeData->data(fmt);
            if (!bytes.isEmpty())
            {
                auto subtype=fmt.section(QLatin1Char('/'),1).toUpper();
                addItems({FileUploadItem::fromEncodedImage(bytes,{},subtype.toUtf8())});
                return;
            }
        }
    }
}

//--------------------------------------------------------------------------

void FileUploadWidget::requestAddFiles()
{
    QFileDialog::Options options;
    if (!isNativeFileDialog())
    {
        options=QFileDialog::DontUseNativeDialog;
    }
    auto paths=QFileDialog::getOpenFileNames(this,tr("Add files"),QString{},QString{},nullptr,options);
    if (!paths.isEmpty())
    {
        addFiles(paths);
    }
}

//--------------------------------------------------------------------------

void FileUploadWidget::keyPressEvent(QKeyEvent* event)
{
    if (event->matches(QKeySequence::Paste))
    {
        pasteFromClipboard();
        event->accept();
        return;
    }
    AbstractFileUploadWidget::keyPressEvent(event);
}

//--------------------------------------------------------------------------

void FileUploadWidget::showEvent(QShowEvent* event)
{
    AbstractFileUploadWidget::showEvent(event);

    // only seed focus if nothing inside this widget already has it -- e.g. FileUploadDialog
    // may have already routed focus to something specific via setDialogFocus(), and a rename
    // or the comments editor mid-edit must never be yanked away from by a later, unrelated
    // show event (hide/show round trips can happen without the widget actually closing).
    // QWidget::focusWidget() only answers for focus SCOPES/windows (this widget is neither),
    // so it would always read back nullptr here regardless of where focus actually is --
    // QApplication::focusWidget() is the one that reflects the true application-wide focus.
    auto* current=QApplication::focusWidget();
    if (current==nullptr || !isAncestorOf(current))
    {
        setFocus();
    }
}

//--------------------------------------------------------------------------

bool FileUploadWidget::acceptsMimeData(const QMimeData* mimeData) const
{
    if (mimeData==nullptr)
    {
        return false;
    }
    if (mimeData->hasUrls())
    {
        const auto urls=mimeData->urls();
        for (const auto& url : urls)
        {
            if (url.isLocalFile())
            {
                return true;
            }
        }
    }
    if (mimeData->hasImage())
    {
        return true;
    }
    for (const auto& fmt : acceptedImageMimeFormats())
    {
        if (mimeData->hasFormat(fmt))
        {
            return true;
        }
    }
    return false;
}

//--------------------------------------------------------------------------

void FileUploadWidget::dragEnterEvent(QDragEnterEvent* event)
{
    if (!isAtMaxFileCount() && acceptsMimeData(event->mimeData()))
    {
        event->acceptProposedAction();
        setProperty("dropActive",true);
        Style::updateWidgetStyle(this);
    }
}

//--------------------------------------------------------------------------

void FileUploadWidget::dragMoveEvent(QDragMoveEvent* event)
{
    if (!isAtMaxFileCount() && acceptsMimeData(event->mimeData()))
    {
        event->acceptProposedAction();
    }
}

//--------------------------------------------------------------------------

void FileUploadWidget::dragLeaveEvent(QDragLeaveEvent* event)
{
    setProperty("dropActive",false);
    Style::updateWidgetStyle(this);
    AbstractFileUploadWidget::dragLeaveEvent(event);
}

//--------------------------------------------------------------------------

void FileUploadWidget::dropEvent(QDropEvent* event)
{
    setProperty("dropActive",false);
    Style::updateWidgetStyle(this);

    addFromMimeData(event->mimeData());
    event->acceptProposedAction();
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END

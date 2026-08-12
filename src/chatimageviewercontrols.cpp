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

/** @file uise/desktop/chatimageviewercontrols.cpp
*
*  Defines ChatImageViewerControls.
*
*/

/****************************************************************************/

#include <QFrame>
#include <QPointer>
#include <QLocale>

#include <uise/desktop/style.hpp>
#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/pushbutton.hpp>
#include <uise/desktop/icontextbutton.hpp>
#include <uise/desktop/elidedlabel.hpp>
#include <uise/desktop/label.hpp>
#include <uise/desktop/dropdownmenu.hpp>
#include <uise/desktop/chatimageviewercontrols.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

namespace {

std::shared_ptr<SvgIcon> ctrlIcon(const QString& alias, QWidget* context)
{
    return Style::instance().svgIconLocator().icon(QString("ChatImageViewer::%1").arg(alias),context);
}

} // anonymous namespace

//--------------------------------------------------------------------------

class ChatImageViewerControls_p
{
    public:

        QFrame* textBlock=nullptr;
        Label* counter=nullptr;
        QFrame* senderRow=nullptr;
        ElidedLabel* sender=nullptr;
        QFrame* separator=nullptr;
        Label* dateTimeLabel=nullptr;

        ImagePreviewStrip* previewStrip=nullptr;

        QFrame* toolbar=nullptr;
        PushButton* saveAs=nullptr;
        PushButton* rotate=nullptr;
        PushButton* zoomIn=nullptr;
        PushButton* zoomOut=nullptr;
        IconTextButton* menuButton=nullptr;
        QPointer<DropdownMenu> menu;

        QDateTime dateTime;
};

//--------------------------------------------------------------------------

ChatImageViewerControls::ChatImageViewerControls(QWidget* parent)
    : Frame(parent),
      pimpl(std::make_unique<ChatImageViewerControls_p>())
{
    setObjectName("chatImageViewerControls");

    auto l=Layout::horizontal(this);

    // --- text block: counter on top, elided sender + separator + datetime below ---

    pimpl->textBlock=new QFrame(this);
    pimpl->textBlock->setObjectName("textBlock");
    // Never grow beyond its own natural content width -- without this, whenever previewStrip's
    // own stretch factor has nothing to claim leftover space with (e.g. transiently, before its
    // sizeHint reflects the current album), QHBoxLayout hands that leftover width to the nearest
    // Preferred-policy sibling instead, which stretched senderRow along with it and made
    // dateTimeLabel drift away from sender/separator instead of hugging them.
    pimpl->textBlock->setSizePolicy(QSizePolicy::Maximum,QSizePolicy::Preferred);
    l->addWidget(pimpl->textBlock);
    auto tl=Layout::vertical(pimpl->textBlock);

    pimpl->counter=new Label(pimpl->textBlock);
    pimpl->counter->setObjectName("counter");
    tl->addWidget(pimpl->counter);

    pimpl->senderRow=new QFrame(pimpl->textBlock);
    pimpl->senderRow->setObjectName("senderRow");
    tl->addWidget(pimpl->senderRow);
    auto sl=Layout::horizontal(pimpl->senderRow);

    pimpl->sender=new ElidedLabel(pimpl->senderRow);
    pimpl->sender->setObjectName("sender");
    sl->addWidget(pimpl->sender,1);

    pimpl->separator=new QFrame(pimpl->senderRow);
    pimpl->separator->setObjectName("separator");
    sl->addWidget(pimpl->separator);

    pimpl->dateTimeLabel=new Label(pimpl->senderRow);
    pimpl->dateTimeLabel->setObjectName("dateTime");
    sl->addWidget(pimpl->dateTimeLabel);

    // --- centred album preview strip ---

    pimpl->previewStrip=new ImagePreviewStrip(this);
    pimpl->previewStrip->setObjectName("previewStrip");
    l->addWidget(pimpl->previewStrip,1);
    connect(
        pimpl->previewStrip,
        &ImagePreviewStrip::previewClicked,
        this,
        &ChatImageViewerControls::previewClicked
    );
    connect(
        pimpl->previewStrip,
        &ImagePreviewStrip::previewClickedKey,
        this,
        &ChatImageViewerControls::previewClickedKey
    );

    // --- right-hand toolbar ---

    pimpl->toolbar=new QFrame(this);
    pimpl->toolbar->setObjectName("toolbar");
    // Same rationale as textBlock's own setSizePolicy() above -- keep the button row compact
    // instead of it absorbing leftover width whenever previewStrip has none to claim.
    pimpl->toolbar->setSizePolicy(QSizePolicy::Maximum,QSizePolicy::Preferred);
    l->addWidget(pimpl->toolbar);
    auto tbl=Layout::horizontal(pimpl->toolbar);

    pimpl->saveAs=new PushButton(pimpl->toolbar);
    pimpl->saveAs->setObjectName("saveAs");
    pimpl->saveAs->setToolTip(tr("Save as"));
    pimpl->saveAs->setSvgIcon(ctrlIcon("saveAs",this));
    tbl->addWidget(pimpl->saveAs);
    connect(pimpl->saveAs,&PushButton::clicked,this,&ChatImageViewerControls::saveAsRequested);

    pimpl->rotate=new PushButton(pimpl->toolbar);
    pimpl->rotate->setObjectName("rotate");
    pimpl->rotate->setToolTip(tr("Rotate counterclockwise"));
    pimpl->rotate->setSvgIcon(ctrlIcon("rotate",this));
    tbl->addWidget(pimpl->rotate);
    connect(pimpl->rotate,&PushButton::clicked,this,&ChatImageViewerControls::rotateRequested);

    pimpl->zoomIn=new PushButton(pimpl->toolbar);
    pimpl->zoomIn->setObjectName("zoomIn");
    pimpl->zoomIn->setToolTip(tr("Zoom in"));
    pimpl->zoomIn->setSvgIcon(ctrlIcon("zoom-in",this));
    tbl->addWidget(pimpl->zoomIn);
    connect(pimpl->zoomIn,&PushButton::clicked,this,&ChatImageViewerControls::zoomInRequested);

    pimpl->zoomOut=new PushButton(pimpl->toolbar);
    pimpl->zoomOut->setObjectName("zoomOut");
    pimpl->zoomOut->setToolTip(tr("Zoom out"));
    pimpl->zoomOut->setSvgIcon(ctrlIcon("zoom-out",this));
    tbl->addWidget(pimpl->zoomOut);
    connect(pimpl->zoomOut,&PushButton::clicked,this,&ChatImageViewerControls::zoomOutRequested);

    pimpl->menuButton=new IconTextButton(
        ctrlIcon("menu",this),pimpl->toolbar,IconTextButton::IconPosition::BeforeText
    );
    pimpl->menuButton->setObjectName("menuButton");
    pimpl->menuButton->setText(QString());
    pimpl->menuButton->setCursor(Qt::PointingHandCursor);
    tbl->addWidget(pimpl->menuButton);

    // DropdownMenu is constructed parentless -- DropdownFrame reparents itself lazily to the
    // trigger's window() on first opening, same recipe as ChatMessageImageItem's own menu button.
    pimpl->menu=new DropdownMenu();
    pimpl->menu->setItems(
        {
            MenuItem(static_cast<int>(MenuAction::GoToMessage),tr("Go to message"),ctrlIcon("goToMessage",this)),
            MenuItem(static_cast<int>(MenuAction::Copy),tr("Copy"),ctrlIcon("copy",this)),
            MenuItem(static_cast<int>(MenuAction::Forward),tr("Forward"),ctrlIcon("forward",this)),
            MenuItem(static_cast<int>(MenuAction::DeleteMessage),tr("Delete message"),ctrlIcon("delete",this)),
            MenuItem::separator(),
            MenuItem(static_cast<int>(MenuAction::SaveAs),tr("Save as"),ctrlIcon("saveAs",this))
        }
    );
    pimpl->menu->attachTo(pimpl->menuButton);
    connect(pimpl->menu,&DropdownMenu::itemTriggered,this,&ChatImageViewerControls::onMenuItemTriggered);

    setCounter(0,0);
}

//--------------------------------------------------------------------------

ChatImageViewerControls::~ChatImageViewerControls()
{}

//--------------------------------------------------------------------------

void ChatImageViewerControls::setCounter(size_t number, size_t total)
{
    pimpl->counter->setText(tr("%1 of %2").arg(number).arg(total));
}

//--------------------------------------------------------------------------

void ChatImageViewerControls::setSender(QString sender)
{
    pimpl->sender->setText(sender);
}

//--------------------------------------------------------------------------

QString ChatImageViewerControls::sender() const
{
    return pimpl->sender->text();
}

//--------------------------------------------------------------------------

void ChatImageViewerControls::setDateTime(QDateTime dateTime)
{
    pimpl->dateTime=dateTime;
    auto str=dateTime.isValid() ? QLocale().toString(dateTime,QLocale::ShortFormat) : QString();
    pimpl->dateTimeLabel->setText(str);
}

//--------------------------------------------------------------------------

QDateTime ChatImageViewerControls::dateTime() const
{
    return pimpl->dateTime;
}

//--------------------------------------------------------------------------

void ChatImageViewerControls::setPreviews(std::vector<ImagePreviewStrip::Preview> previews, int currentIndex)
{
    pimpl->previewStrip->setPreviews(std::move(previews),currentIndex);
}

//--------------------------------------------------------------------------

void ChatImageViewerControls::setCurrentPreview(int index)
{
    pimpl->previewStrip->setCurrentIndex(index);
}

//--------------------------------------------------------------------------

void ChatImageViewerControls::setPreviewSource(std::shared_ptr<PixmapSource> source)
{
    pimpl->previewStrip->setImageSource(std::move(source));
}

//--------------------------------------------------------------------------

ImagePreviewStrip* ChatImageViewerControls::previewStrip() const
{
    return pimpl->previewStrip;
}

//--------------------------------------------------------------------------

DropdownMenu* ChatImageViewerControls::menu() const
{
    return pimpl->menu;
}

//--------------------------------------------------------------------------

void ChatImageViewerControls::setMenuActionVisible(MenuAction action, bool visible)
{
    pimpl->menu->setItemVisible(static_cast<int>(action),visible);
}

//--------------------------------------------------------------------------

void ChatImageViewerControls::addMenuItem(MenuItem item)
{
    pimpl->menu->addItem(std::move(item));
}

//--------------------------------------------------------------------------

void ChatImageViewerControls::addMenuSeparator()
{
    pimpl->menu->addSeparator();
}

//--------------------------------------------------------------------------

PushButton* ChatImageViewerControls::saveAsButton() const
{
    return pimpl->saveAs;
}

//--------------------------------------------------------------------------

PushButton* ChatImageViewerControls::rotateButton() const
{
    return pimpl->rotate;
}

//--------------------------------------------------------------------------

PushButton* ChatImageViewerControls::zoomInButton() const
{
    return pimpl->zoomIn;
}

//--------------------------------------------------------------------------

PushButton* ChatImageViewerControls::zoomOutButton() const
{
    return pimpl->zoomOut;
}

//--------------------------------------------------------------------------

IconTextButton* ChatImageViewerControls::menuButton() const
{
    return pimpl->menuButton;
}

//--------------------------------------------------------------------------

void ChatImageViewerControls::onMenuItemTriggered(int id)
{
    switch (static_cast<MenuAction>(id))
    {
        case (MenuAction::GoToMessage):
            emit goToMessageRequested();
            break;

        case (MenuAction::Copy):
            emit copyRequested();
            break;

        case (MenuAction::Forward):
            emit forwardRequested();
            break;

        case (MenuAction::DeleteMessage):
            emit deleteMessageRequested();
            break;

        case (MenuAction::SaveAs):
            emit saveAsRequested();
            break;

        default:
            // Not one of the five built-in ids -- a custom row added via addMenuItem() (or
            // directly via menu()->addItem()). static_cast<MenuAction>(id) is well-defined even
            // for an id outside MenuAction's own enumerators, since MenuAction has a fixed (int)
            // underlying type.
            emit customMenuItemTriggered(id);
            break;
    }
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END

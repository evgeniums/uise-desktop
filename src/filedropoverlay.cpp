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

/** @file uise/desktop/filedropoverlay.cpp
*
*  Defines FileDropOverlay.
*
*/

/****************************************************************************/

#include <QMimeData>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDragLeaveEvent>
#include <QDropEvent>
#include <QShortcut>
#include <QTimer>
#include <QCursor>
#include <QEvent>

#include <uise/desktop/style.hpp>
#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/utils/mimedatautils.hpp>
#include <uise/desktop/roundedimage.hpp>
#include <uise/desktop/label.hpp>
#include <uise/desktop/filedropoverlay.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

namespace {

std::shared_ptr<SvgIcon> fileDropOverlayIcon(const QString& alias, QWidget* context)
{
    return Style::instance().svgIconLocator().icon(QString("FileDropOverlay::%1").arg(alias),context);
}

}

//==========================================================================

class FileDropOverlay_p
{
    public:

        QWidget* host=nullptr;

        bool autoShow=FileDropOverlay::DefaultAutoShow;
        bool imagesPanelAllowed=FileDropOverlay::DefaultImagesPanelAllowed;
        bool active=false;
        bool hasImages=false;
        FileDropOverlay::Panel hoveredPanel=FileDropOverlay::Panel::None;
        int leaveWatchdogIntervalMs=FileDropOverlay::DefaultLeaveWatchdogIntervalMs;
        Qt::Orientation panelOrientation=FileDropOverlay::DefaultPanelOrientation;

        QFrame* documentsPanel=nullptr;
        RoundedImage* documentsIcon=nullptr;
        Label* documentsCaptionLabel=nullptr;
        Label* documentsSubtitleLabel=nullptr;

        QFrame* imagesPanel=nullptr;
        RoundedImage* imagesIcon=nullptr;
        Label* imagesCaptionLabel=nullptr;
        Label* imagesSubtitleLabel=nullptr;

        QFrame* photosPanel=nullptr;
        RoundedImage* photosIcon=nullptr;
        Label* photosCaptionLabel=nullptr;
        Label* photosSubtitleLabel=nullptr;

        QString singleCaption;
        QString singleSubtitle;
        QString imagesCaption;
        QString imagesSubtitle;
        QString photosCaption;
        QString photosSubtitle;

        std::shared_ptr<SvgIcon> svgUploadIcon;
        std::shared_ptr<SvgIcon> svgImagesIcon;
        std::shared_ptr<SvgIcon> svgPhotosIcon;

        QShortcut* escShortcut=nullptr;
        QTimer* leaveWatchdog=nullptr;
};

//==========================================================================

namespace {

// Same idiom as IconTextButton::setHovered() -- a dynamic property set on a container does not
// invalidate the cached style evaluation of its own children, so each widget that needs to
// react to the hover state carries and is repolished with its own copy of the property, rather
// than being matched through an ancestor attribute selector.
void applyPanelHovered(QFrame* panel, RoundedImage* icon, Label* caption, Label* subtitle, bool hovered)
{
    if (panel==nullptr)
    {
        return;
    }

    panel->setProperty("hovered",hovered);
    caption->setProperty("hovered",hovered);
    subtitle->setProperty("hovered",hovered);
    Style::updateWidgetStyle(panel);
    Style::updateWidgetStyle(caption);
    Style::updateWidgetStyle(subtitle);
    icon->setParentHovered(hovered);
}

}

//==========================================================================

FileDropOverlay::FileDropOverlay(QWidget* host)
    : QFrame(host),
      pimpl(std::make_unique<FileDropOverlay_p>())
{
    Q_ASSERT(host!=nullptr);

    pimpl->host=host;

    setObjectName("fileDropOverlay");

    // Deliberately NOT WA_TransparentForMouseEvents, unlike RippleOverlay -- see the class docs:
    // QWidgetPrivate::childAtRecursiveHelper() skips transparent-for-mouse children when Qt
    // resolves a drag target, so a transparent overlay could never become that target and
    // per-panel hover would be impossible.
    setAcceptDrops(true);
    setFocusPolicy(Qt::NoFocus);

    pimpl->singleCaption=tr("Drop files here to send as documents");
    pimpl->imagesCaption=tr("Send as images");
    pimpl->imagesSubtitle=tr("Full quality, larger size");
    pimpl->photosCaption=tr("Send as photos");
    pimpl->photosSubtitle=tr("Adaptive quality, faster delivery");

    // Panel widgets are built here as plain children of `this`, deliberately NOT added to any
    // layout yet -- rebuildPanelsLayout() (called just below, and again from
    // setPanelOrientation()) is the single place a QBoxLayout is created and both panels are
    // added to it, so switching orientation later is just "delete the old layout, build a new
    // one the other way, re-add the same two widgets" rather than duplicating this construction.
    auto buildPanel=[this](const QString& objName) -> QFrame*
    {
        auto* panel=new QFrame(this);
        panel->setObjectName(objName);
        auto* panelLayout=Layout::vertical(panel);
        panelLayout->setAlignment(Qt::AlignCenter);

        auto* icon=new RoundedImage(panel);
        icon->setObjectName("panelIcon");
        panelLayout->addWidget(icon,0,Qt::AlignHCenter);

        auto* caption=new Label(panel);
        caption->setObjectName("panelCaption");
        caption->setAlignment(Qt::AlignCenter);
        caption->setWordWrap(true);
        panelLayout->addWidget(caption);

        auto* subtitle=new Label(panel);
        subtitle->setObjectName("panelSubtitle");
        subtitle->setAlignment(Qt::AlignCenter);
        subtitle->setWordWrap(true);
        panelLayout->addWidget(subtitle);

        return panel;
    };

    pimpl->documentsPanel=buildPanel(QStringLiteral("documentsPanel"));
    pimpl->documentsIcon=pimpl->documentsPanel->findChild<RoundedImage*>(QStringLiteral("panelIcon"));
    pimpl->documentsCaptionLabel=pimpl->documentsPanel->findChild<Label*>(QStringLiteral("panelCaption"));
    pimpl->documentsSubtitleLabel=pimpl->documentsPanel->findChild<Label*>(QStringLiteral("panelSubtitle"));

    pimpl->imagesPanel=buildPanel(QStringLiteral("imagesPanel"));
    pimpl->imagesIcon=pimpl->imagesPanel->findChild<RoundedImage*>(QStringLiteral("panelIcon"));
    pimpl->imagesCaptionLabel=pimpl->imagesPanel->findChild<Label*>(QStringLiteral("panelCaption"));
    pimpl->imagesSubtitleLabel=pimpl->imagesPanel->findChild<Label*>(QStringLiteral("panelSubtitle"));
    pimpl->imagesPanel->setVisible(false);

    pimpl->photosPanel=buildPanel(QStringLiteral("photosPanel"));
    pimpl->photosIcon=pimpl->photosPanel->findChild<RoundedImage*>(QStringLiteral("panelIcon"));
    pimpl->photosCaptionLabel=pimpl->photosPanel->findChild<Label*>(QStringLiteral("panelCaption"));
    pimpl->photosSubtitleLabel=pimpl->photosPanel->findChild<Label*>(QStringLiteral("panelSubtitle"));
    pimpl->photosPanel->setVisible(false);

    rebuildPanelsLayout();
    updateIcons();
    updatePanels();

    pimpl->escShortcut=new QShortcut(Qt::Key_Escape,this);
    pimpl->escShortcut->setContext(Qt::WindowShortcut);
    pimpl->escShortcut->setEnabled(false);
    connect(pimpl->escShortcut,&QShortcut::activated,this,&FileDropOverlay::dismiss);

    pimpl->leaveWatchdog=new QTimer(this);
    connect(pimpl->leaveWatchdog,&QTimer::timeout,this,&FileDropOverlay::checkPointerLeft);

    updateGeometryFromHost();
    host->installEventFilter(this);

    hide();
}

//--------------------------------------------------------------------------

FileDropOverlay::~FileDropOverlay()
{
}

//--------------------------------------------------------------------------

FileDropOverlay* FileDropOverlay::install(QWidget* host)
{
    if (host==nullptr)
    {
        return nullptr;
    }

    auto* overlay=find(host);
    if (overlay!=nullptr)
    {
        return overlay;
    }

    overlay=new FileDropOverlay(host);

    // Push QSS-supplied values (qproperty-leaveWatchdogIntervalMs, panel geometry/colours) into
    // effect now, same idiom as RippleOverlay::install()/LoadControl's hidden #sample frame:
    // a widget that is never shown until the first drag never goes through Qt's normal
    // show-triggered polish on its own.
    Style::updateWidgetStyle(overlay);
    Style::repolishRecursive(overlay);
    overlay->ensurePolished();

    return overlay;
}

//--------------------------------------------------------------------------

FileDropOverlay* FileDropOverlay::find(QWidget* host)
{
    if (host==nullptr)
    {
        return nullptr;
    }
    return host->findChild<FileDropOverlay*>(QString{},Qt::FindDirectChildrenOnly);
}

//--------------------------------------------------------------------------

QWidget* FileDropOverlay::host() const noexcept
{
    return pimpl->host;
}

//--------------------------------------------------------------------------

void FileDropOverlay::setAutoShow(bool enable) noexcept
{
    pimpl->autoShow=enable;
}

//--------------------------------------------------------------------------

bool FileDropOverlay::isAutoShow() const noexcept
{
    return pimpl->autoShow;
}

//--------------------------------------------------------------------------

void FileDropOverlay::setImagesPanelAllowed(bool enable)
{
    pimpl->imagesPanelAllowed=enable;
    if (pimpl->active)
    {
        pimpl->hasImages=pimpl->hasImages && enable;
        updatePanels();
    }
}

//--------------------------------------------------------------------------

bool FileDropOverlay::isImagesPanelAllowed() const noexcept
{
    return pimpl->imagesPanelAllowed;
}

//--------------------------------------------------------------------------

void FileDropOverlay::setPanelOrientation(Qt::Orientation orientation)
{
    if (pimpl->panelOrientation==orientation)
    {
        return;
    }
    pimpl->panelOrientation=orientation;
    rebuildPanelsLayout();
}

Qt::Orientation FileDropOverlay::panelOrientation() const noexcept
{
    return pimpl->panelOrientation;
}

void FileDropOverlay::setPanelOrientationName(const QString& name)
{
    setPanelOrientation(
        name.compare(QStringLiteral("vertical"),Qt::CaseInsensitive)==0 ? Qt::Vertical : Qt::Horizontal
    );
}

QString FileDropOverlay::panelOrientationName() const
{
    return pimpl->panelOrientation==Qt::Vertical ? QStringLiteral("vertical") : QStringLiteral("horizontal");
}

//--------------------------------------------------------------------------

bool FileDropOverlay::isActive() const noexcept
{
    return pimpl->active;
}

//--------------------------------------------------------------------------

bool FileDropOverlay::hasImages() const noexcept
{
    return pimpl->hasImages;
}

//--------------------------------------------------------------------------

FileDropOverlay::Panel FileDropOverlay::hoveredPanel() const noexcept
{
    return pimpl->hoveredPanel;
}

//--------------------------------------------------------------------------

QFrame* FileDropOverlay::panelFrame(Panel panel) const
{
    switch (panel)
    {
        case (Panel::Documents): return pimpl->documentsPanel;
        case (Panel::Images): return pimpl->imagesPanel;
        case (Panel::Photos): return pimpl->photosPanel;
        case (Panel::None): break;
    }
    return nullptr;
}

//--------------------------------------------------------------------------

void FileDropOverlay::setSingleCaption(const QString& text)
{
    pimpl->singleCaption=text;
    if (pimpl->active && !pimpl->hasImages)
    {
        updatePanels();
    }
}

QString FileDropOverlay::singleCaption() const
{
    return pimpl->singleCaption;
}

void FileDropOverlay::setSingleSubtitle(const QString& text)
{
    pimpl->singleSubtitle=text;
    if (pimpl->active && !pimpl->hasImages)
    {
        updatePanels();
    }
}

QString FileDropOverlay::singleSubtitle() const
{
    return pimpl->singleSubtitle;
}

//--------------------------------------------------------------------------

void FileDropOverlay::setImagesCaption(const QString& text)
{
    pimpl->imagesCaption=text;
    if (pimpl->active && pimpl->hasImages)
    {
        updatePanels();
    }
}

QString FileDropOverlay::imagesCaption() const
{
    return pimpl->imagesCaption;
}

void FileDropOverlay::setImagesSubtitle(const QString& text)
{
    pimpl->imagesSubtitle=text;
    if (pimpl->active && pimpl->hasImages)
    {
        updatePanels();
    }
}

QString FileDropOverlay::imagesSubtitle() const
{
    return pimpl->imagesSubtitle;
}

//--------------------------------------------------------------------------

void FileDropOverlay::setPhotosCaption(const QString& text)
{
    pimpl->photosCaption=text;
    if (pimpl->active && pimpl->hasImages)
    {
        updatePanels();
    }
}

QString FileDropOverlay::photosCaption() const
{
    return pimpl->photosCaption;
}

void FileDropOverlay::setPhotosSubtitle(const QString& text)
{
    pimpl->photosSubtitle=text;
    if (pimpl->active && pimpl->hasImages)
    {
        updatePanels();
    }
}

QString FileDropOverlay::photosSubtitle() const
{
    return pimpl->photosSubtitle;
}

//--------------------------------------------------------------------------

void FileDropOverlay::setLeaveWatchdogIntervalMs(int ms)
{
    pimpl->leaveWatchdogIntervalMs=ms;
    if (pimpl->active)
    {
        restartLeaveWatchdog();
    }
}

int FileDropOverlay::leaveWatchdogIntervalMs() const noexcept
{
    return pimpl->leaveWatchdogIntervalMs;
}

//--------------------------------------------------------------------------

bool FileDropOverlay::acceptsMimeData(const QMimeData* mimeData)
{
    if (mimeData==nullptr)
    {
        return false;
    }
    if (mimeData->hasUrls())
    {
        return true;
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

void FileDropOverlay::showForMimeData(const QMimeData* mimeData)
{
    pimpl->hasImages=pimpl->imagesPanelAllowed && mimeDataHasImages(mimeData);
    setHoveredPanel(Panel::None);
    updatePanels();

    auto wasActive=pimpl->active;
    pimpl->active=true;

    updateGeometryFromHost();
    show();
    raise();

    pimpl->escShortcut->setEnabled(true);
    restartLeaveWatchdog();

    if (!wasActive)
    {
        emit activeChanged(true);
    }
}

//--------------------------------------------------------------------------

void FileDropOverlay::dismiss()
{
    if (!pimpl->active)
    {
        return;
    }

    pimpl->active=false;
    setHoveredPanel(Panel::None);
    stopLeaveWatchdog();
    pimpl->escShortcut->setEnabled(false);
    hide();

    emit activeChanged(false);
}

//--------------------------------------------------------------------------

void FileDropOverlay::updateGeometryFromHost()
{
    if (pimpl->host!=nullptr)
    {
        setGeometry(pimpl->host->rect());
    }
}

//--------------------------------------------------------------------------

void FileDropOverlay::rebuildPanelsLayout()
{
    // Layout::box() deletes this widget's current layout (if any) before creating the new one --
    // that only detaches the panels from it, it does not destroy them (a QLayout does not own
    // the widgets placed into it), so all three survive to be re-added below. See the
    // constructor's own comment on why panel construction and layout assembly are kept separate.
    // Only two of the three are ever visible at once (updatePanels()), but all three stay in the
    // layout permanently -- a hidden widget is skipped by the layout, so there is no need to
    // add/remove on every hasImages change.
    auto* layout=Layout::box(this,pimpl->panelOrientation);
    layout->addWidget(pimpl->imagesPanel,1);
    layout->addWidget(pimpl->photosPanel,1);
    layout->addWidget(pimpl->documentsPanel,1);
}

//--------------------------------------------------------------------------

void FileDropOverlay::updatePanels()
{
    pimpl->imagesPanel->setVisible(pimpl->hasImages);
    pimpl->photosPanel->setVisible(pimpl->hasImages);
    pimpl->documentsPanel->setVisible(!pimpl->hasImages);

    if (pimpl->hasImages)
    {
        pimpl->imagesIcon->setSvgIcon(pimpl->svgImagesIcon);
        pimpl->imagesCaptionLabel->setText(pimpl->imagesCaption);
        pimpl->imagesSubtitleLabel->setText(pimpl->imagesSubtitle);
        pimpl->imagesSubtitleLabel->setVisible(!pimpl->imagesSubtitle.isEmpty());

        pimpl->photosIcon->setSvgIcon(pimpl->svgPhotosIcon);
        pimpl->photosCaptionLabel->setText(pimpl->photosCaption);
        pimpl->photosSubtitleLabel->setText(pimpl->photosSubtitle);
        pimpl->photosSubtitleLabel->setVisible(!pimpl->photosSubtitle.isEmpty());
    }
    else
    {
        pimpl->documentsIcon->setSvgIcon(pimpl->svgUploadIcon);
        pimpl->documentsCaptionLabel->setText(pimpl->singleCaption);
        pimpl->documentsSubtitleLabel->setText(pimpl->singleSubtitle);
        pimpl->documentsSubtitleLabel->setVisible(!pimpl->singleSubtitle.isEmpty());
    }
}

//--------------------------------------------------------------------------

void FileDropOverlay::updateIcons()
{
    pimpl->svgUploadIcon=fileDropOverlayIcon(QStringLiteral("upload"),this);
    pimpl->svgImagesIcon=fileDropOverlayIcon(QStringLiteral("images"),this);
    pimpl->svgPhotosIcon=fileDropOverlayIcon(QStringLiteral("photos"),this);
}

//--------------------------------------------------------------------------

void FileDropOverlay::setHoveredPanel(Panel panel)
{
    if (pimpl->hoveredPanel==panel)
    {
        return;
    }

    applyPanelHovered(pimpl->documentsPanel,pimpl->documentsIcon,pimpl->documentsCaptionLabel,pimpl->documentsSubtitleLabel,panel==Panel::Documents);
    applyPanelHovered(pimpl->imagesPanel,pimpl->imagesIcon,pimpl->imagesCaptionLabel,pimpl->imagesSubtitleLabel,panel==Panel::Images);
    applyPanelHovered(pimpl->photosPanel,pimpl->photosIcon,pimpl->photosCaptionLabel,pimpl->photosSubtitleLabel,panel==Panel::Photos);

    pimpl->hoveredPanel=panel;

    if (pimpl->active)
    {
        emit panelHovered(panel);
    }
}

//--------------------------------------------------------------------------

FileDropOverlay::Panel FileDropOverlay::panelAt(const QPoint& pos) const
{
    if (!pimpl->hasImages)
    {
        return Panel::Documents;
    }

    // Midpoint split, not strict rect containment, so the padding band and inter-panel gap have
    // no dead zone -- panelAt() always resolves to one panel or the other. imagesPanel is
    // always added to the layout first (see rebuildPanelsLayout()), so it is the left panel in
    // Qt::Horizontal and the top panel in Qt::Vertical; split on the axis the layout actually
    // arranges the panels along.
    if (pimpl->panelOrientation==Qt::Vertical)
    {
        auto split=(pimpl->imagesPanel->geometry().bottom()+pimpl->photosPanel->geometry().top())/2;
        return pos.y()<split ? Panel::Images : Panel::Photos;
    }

    auto split=(pimpl->imagesPanel->geometry().right()+pimpl->photosPanel->geometry().left())/2;
    return pos.x()<split ? Panel::Images : Panel::Photos;
}

//--------------------------------------------------------------------------

void FileDropOverlay::handleDrop(const QMimeData* mimeData, const QPoint& pos)
{
    auto panel=panelAt(pos);
    dismiss();
    emit dropped(panel,mimeData);
}

//--------------------------------------------------------------------------

void FileDropOverlay::restartLeaveWatchdog()
{
    if (pimpl->leaveWatchdogIntervalMs<=0)
    {
        stopLeaveWatchdog();
        return;
    }
    pimpl->leaveWatchdog->start(pimpl->leaveWatchdogIntervalMs);
}

//--------------------------------------------------------------------------

void FileDropOverlay::stopLeaveWatchdog()
{
    pimpl->leaveWatchdog->stop();
}

//--------------------------------------------------------------------------

void FileDropOverlay::checkPointerLeft()
{
    if (!pimpl->active)
    {
        stopLeaveWatchdog();
        return;
    }

    QRect globalRect(mapToGlobal(QPoint(0,0)),size());
    if (!globalRect.contains(QCursor::pos()))
    {
        dismiss();
    }
}

//--------------------------------------------------------------------------

bool FileDropOverlay::eventFilter(QObject* watched, QEvent* event)
{
    if (watched!=pimpl->host)
    {
        return QFrame::eventFilter(watched,event);
    }

    switch (event->type())
    {
        case (QEvent::Resize):
        {
            updateGeometryFromHost();
        }
        break;

        case (QEvent::ChildAdded):
        case (QEvent::Show):
        {
            // Keep the overlay last in the host's child list -- and therefore on top -- even if
            // the host adds children (or is re-shown) after install() was called.
            raise();
        }
        break;

        case (QEvent::Hide):
        case (QEvent::WindowDeactivate):
        {
            // Cheap insurance: while visible the overlay blocks all mouse input to the host, so
            // a host that stops being usable (hidden, or the window loses activation mid-drag)
            // must never leave the overlay stuck up.
            dismiss();
        }
        break;

        case (QEvent::DragEnter):
        {
            if (pimpl->autoShow)
            {
                auto* dragEvent=static_cast<QDragEnterEvent*>(event);
                if (acceptsMimeData(dragEvent->mimeData()))
                {
                    dragEvent->acceptProposedAction();
                    showForMimeData(dragEvent->mimeData());
                    return true;
                }
            }
        }
        break;

        case (QEvent::DragMove):
        {
            if (pimpl->autoShow && pimpl->active)
            {
                auto* dragEvent=static_cast<QDragMoveEvent*>(event);
                dragEvent->acceptProposedAction();
                return true;
            }
        }
        break;

        case (QEvent::DragLeave):
        {
            if (pimpl->active)
            {
                // Swallowed on purpose -- see the class docs: this fires the instant Qt hands
                // the drag target from the host over to this overlay, and does not mean the
                // pointer actually left. Acting on it here would immediately hide the overlay,
                // handing the target straight back to the host, which would show it again on
                // the very next drag-move -- an infinite show/hide loop.
                return true;
            }
        }
        break;

        case (QEvent::Drop):
        {
            if (pimpl->autoShow && pimpl->active)
            {
                // A real, reachable path: on macOS a drag held still after entering (no further
                // pointer movement) never hands the target off from the host to this overlay --
                // see handleDrop()'s caller here vs. dropEvent() below -- so the drop still
                // lands on the host and must be handled from here or it would be lost.
                auto* dropEv=static_cast<QDropEvent*>(event);
                handleDrop(dropEv->mimeData(),dropEv->position().toPoint());
                dropEv->acceptProposedAction();
                return true;
            }
        }
        break;

        default:
            break;
    }

    return false;
}

//--------------------------------------------------------------------------

void FileDropOverlay::dragEnterEvent(QDragEnterEvent* event)
{
    if (acceptsMimeData(event->mimeData()))
    {
        event->acceptProposedAction();
        if (!pimpl->active)
        {
            showForMimeData(event->mimeData());
        }
    }
}

//--------------------------------------------------------------------------

void FileDropOverlay::dragMoveEvent(QDragMoveEvent* event)
{
    event->acceptProposedAction();
    setHoveredPanel(panelAt(event->position().toPoint()));
    restartLeaveWatchdog();
}

//--------------------------------------------------------------------------

void FileDropOverlay::dragLeaveEvent(QDragLeaveEvent* /* event */)
{
    // Trustworthy here specifically because nothing inside this overlay accepts drops (the
    // panels don't -- see the class docs), so a leave delivered directly to the overlay itself
    // can only mean the pointer really left it.
    dismiss();
}

//--------------------------------------------------------------------------

void FileDropOverlay::dropEvent(QDropEvent* event)
{
    handleDrop(event->mimeData(),event->position().toPoint());
    event->acceptProposedAction();
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END

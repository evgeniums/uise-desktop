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

/** @file uise/desktop/src/dialog.ipp
*
*  Defines Dialog.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_DIALOG_IPP
#define UISE_DESKTOP_DIALOG_IPP

#include <QPointer>
#include <QLabel>
#include <QSignalMapper>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/utils/destroywidget.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/pushbutton.hpp>
#include <uise/desktop/dialog.hpp>

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

/**************************** Dialog ***********************************/

//--------------------------------------------------------------------------

template <typename BaseT>
class Dialog_p
{
    public:

        Dialog<BaseT>* widget;

        QBoxLayout* layout;

        QFrame* titleFrame;
        QBoxLayout* titleLayout;
        QLabel* title;
        PushButton* titleClose;
        QLabel* placeHolder=nullptr;
        QPointer<QWidget> titleControl;

        QFrame* contentFrame;
        QBoxLayout* contentLayout;

        QFrame* buttonsFrame=nullptr;
        QBoxLayout* buttonLayout=nullptr;

        QSignalMapper* buttonGroup;

        std::map<int,PushButton*> buttons;
        //! Insertion order -- std::map above is ordered by button id, not insertion order,
        //! so a relayout driven off it would silently reorder the row.
        std::vector<PushButton*> orderedButtons;

        QFrame* dialogFrame;
        PushButton* icon;
        QBoxLayout* dialogLayout;
};

//--------------------------------------------------------------------------

template <typename BaseT>
Dialog<BaseT>::Dialog(QWidget* parent)
    : BaseT(parent),
      pimpl(std::make_unique<Dialog_p<BaseT>>())
{
    pimpl->widget=this;

    pimpl->layout=Layout::vertical(this);

    pimpl->titleFrame=new QFrame(this);
    pimpl->titleFrame->setObjectName("titleFrame");
    auto tl=Layout::horizontal(pimpl->titleFrame);
    pimpl->titleLayout=tl;
    pimpl->layout->addWidget(pimpl->titleFrame);
    pimpl->titleClose=new PushButton(Style::instance().svgIconLocator().icon("DialogTitle::close",this),pimpl->titleFrame);
    pimpl->titleClose->setToolTip(AbstractDialog::tr("Close","dialog"));
    pimpl->title=new QLabel(pimpl->titleFrame);
    pimpl->title->setAlignment(Qt::AlignCenter);
    pimpl->titleFrame->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
#ifdef Q_OS_MACOS
    tl->addWidget(pimpl->titleClose,0);
    tl->addWidget(pimpl->title,1);
    pimpl->placeHolder=new QLabel();
    pimpl->placeHolder->setObjectName("placeHolder");
    tl->addWidget(pimpl->placeHolder);
#else
    tl->addWidget(pimpl->title,1);
    tl->addWidget(pimpl->titleClose);
#endif

    pimpl->contentFrame=new QFrame(this);
    pimpl->contentFrame->setObjectName("contentFrame");
    pimpl->contentLayout=Layout::horizontal(pimpl->contentFrame);
    pimpl->layout->addWidget(pimpl->contentFrame);

    pimpl->icon=new PushButton(this);
    pimpl->contentLayout->addWidget(pimpl->icon);
    pimpl->icon->setObjectName("dialogIcon");
    pimpl->icon->setVisible(false);

    pimpl->dialogFrame=new QFrame(this);
    pimpl->dialogFrame->setObjectName("dialogFrame");
    pimpl->dialogLayout=Layout::vertical(pimpl->dialogFrame);
    pimpl->contentLayout->addWidget(pimpl->dialogFrame,1);

    pimpl->buttonGroup=new QSignalMapper(this);
    QObject::connect(
        pimpl->buttonGroup,
        &QSignalMapper::mappedInt,
        this,
        [this](int id)
        {
            emit AbstractDialog::buttonClicked(id);
            if (AbstractDialog::isButton(id,AbstractDialog::StandardButton::Close) || AbstractDialog::isButton(id,AbstractDialog::StandardButton::Cancel))
            {
                this->closeDialog();
            }
        }
    );

    QObject::connect(
        pimpl->titleClose,
        &PushButton::clicked,
        this,
        [this]()
        {
            this->closeDialog();
        }
    );

    doSetButtons({AbstractDialog::standardButton(AbstractDialog::StandardButton::Close,this)});

    this->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Fixed);
}

//--------------------------------------------------------------------------

template <typename BaseT>
Dialog<BaseT>::~Dialog()
{
}

//--------------------------------------------------------------------------

template <typename BaseT>
void Dialog<BaseT>::setWidget(QWidget* widget)
{
    pimpl->dialogLayout->addWidget(widget);
}

//--------------------------------------------------------------------------

template <typename BaseT>
void Dialog<BaseT>::setButtons(std::vector<AbstractDialog::ButtonConfig> buttons)
{
    doSetButtons(std::move(buttons));
}

//--------------------------------------------------------------------------

template <typename BaseT>
void Dialog<BaseT>::doSetButtons(std::vector<AbstractDialog::ButtonConfig> buttons)
{
    for (auto* bt: pimpl->orderedButtons)
    {
        destroyWidget(bt);
    }
    pimpl->orderedButtons.clear();
    pimpl->buttons.clear();

    destroyWidget(pimpl->buttonsFrame);

    pimpl->buttonsFrame=new QFrame(this);
    pimpl->buttonsFrame->setObjectName("dialogButtonsFrame");
    pimpl->buttonLayout=nullptr; // created by applyButtonsLayout() below
    pimpl->layout->addWidget(pimpl->buttonsFrame); // alignment applied by applyButtonsLayout()

    for (const auto& button: buttons)
    {
        auto bt=new PushButton(button.text,button.icon,pimpl->buttonsFrame);
        bt->setObjectName(button.name);
        pimpl->buttonGroup->setMapping(bt,button.id);
        QObject::connect(
            bt,
            SIGNAL(clicked()),
            pimpl->buttonGroup,
            SLOT(map())
        );
        pimpl->buttons.emplace(button.id,bt);
        pimpl->orderedButtons.push_back(bt);
    }

    // the frame is brand new and has never been polished, so the dynamic "vertical"
    // property set below will be picked up by its first polish -- no repolish needed, and
    // repolishing a widget from inside the Dialog constructor is best avoided
    applyButtonsLayout(false);
}

//--------------------------------------------------------------------------

template <typename BaseT>
void Dialog<BaseT>::updateButtonsLayout()
{
    applyButtonsLayout(true);
}

//--------------------------------------------------------------------------

template <typename BaseT>
void Dialog<BaseT>::applyButtonsLayout(bool repolish)
{
    if (pimpl->buttonsFrame==nullptr)
    {
        return;
    }

    const auto style=this->effectiveButtonsStyle();
    const bool vertical=style.orientation==Qt::Vertical;
    const auto hAlign=style.alignment & Qt::AlignHorizontal_Mask;
    const auto vAlign=style.alignment & Qt::AlignVertical_Mask;

    // Layout::box() deletes the previous layout first; deleting a QLayout does not delete
    // the widgets it managed, they just stay children of the frame until re-added below.
    pimpl->buttonLayout=Layout::box(pimpl->buttonsFrame,style.orientation);

    // Horizontal: the legacy rule was `alignment | Qt::AlignBottom`, which produced a
    // contradictory AlignVCenter|AlignBottom for callers passing Qt::AlignCenter.
    // Qt::AlignBottom is now only the DEFAULT vertical flag, used when the style names none,
    // so every horizontal-only alignment (AlignRight, AlignLeft, AlignHCenter) behaves
    // exactly as before.
    // Vertical: only the horizontal flag is meaningful per item, and its absence makes
    // QBoxLayout stretch every button to the column width.
    const Qt::Alignment itemAlignment = vertical
                                             ? hAlign
                                             : (hAlign | (vAlign ? vAlign : Qt::AlignBottom));

    const bool stretchButtons = vertical && hAlign==Qt::Alignment{};
    for (auto* bt: pimpl->orderedButtons)
    {
        pimpl->buttonLayout->addWidget(bt,0,itemAlignment);
        // PushButton pins its inner QPushButton to sizeHint and centers it, so stretching
        // the frame alone would not widen the visible button -- let it fill the frame too.
        bt->setContentAlignment(stretchButtons ? Qt::AlignVCenter : Qt::AlignCenter);
    }

    // Horizontal mode keeps the historical Fixed width. Vertical mode needs Preferred: with
    // a Fixed policy and no horizontal alignment flag, QWidgetItem::setGeometry() clamps the
    // frame to its sizeHint and centers it, making a full-width column impossible. Preferred
    // still hugs sizeHint whenever a horizontal flag is present, because aligned items are
    // clamped to their sizeHint anyway.
    pimpl->buttonsFrame->setSizePolicy(vertical?QSizePolicy::Preferred:QSizePolicy::Fixed,
                                       QSizePolicy::Fixed);

    // setAlignment() rather than remove+addWidget: keeps the frame's position in the dialog
    // layout and avoids a spurious hide/show.
    pimpl->layout->setAlignment(pimpl->buttonsFrame,style.alignment);

    // dynamic property so QSS can style the two forms differently:
    //   #dialogButtonsFrame[vertical="true"] { ... }
    const auto prev=pimpl->buttonsFrame->property("vertical");
    const bool changed=!prev.isValid() || prev.toBool()!=vertical;
    pimpl->buttonsFrame->setProperty("vertical",vertical);
    if (repolish && changed)
    {
        // recursive: a rule like #dialogButtonsFrame[vertical="true"] uise--PushButton is
        // only re-evaluated when the CHILD is repolished, not the ancestor
        Style::repolishRecursive(pimpl->buttonsFrame);
    }
}

//--------------------------------------------------------------------------

template <typename BaseT>
void Dialog<BaseT>::doActivateButton(int id)
{
    auto it=pimpl->buttons.find(id);
    if (it!=pimpl->buttons.end())
    {
        it->second->click();
    }
}

//--------------------------------------------------------------------------

template <typename BaseT>
void Dialog<BaseT>::doSetButtonEnabled(int id, bool enable)
{
    auto it=pimpl->buttons.find(id);
    if (it!=pimpl->buttons.end())
    {
        it->second->setEnabled(enable);
    }
}

//--------------------------------------------------------------------------

template <typename BaseT>
void Dialog<BaseT>::doSetButtonVisible(int id, bool enable)
{
    auto it=pimpl->buttons.find(id);
    if (it!=pimpl->buttons.end())
    {
        it->second->setVisible(enable);
    }
}

//--------------------------------------------------------------------------

template <typename BaseT>
void Dialog<BaseT>::doSetButtonText(int id, const QString& text)
{
    auto it=pimpl->buttons.find(id);
    if (it!=pimpl->buttons.end())
    {
        it->second->setText(text);
    }
}

//--------------------------------------------------------------------------

template <typename BaseT>
void Dialog<BaseT>::setTitle(const QString& title)
{
    pimpl->title->setText(title);
}

//--------------------------------------------------------------------------

template <typename BaseT>
void Dialog<BaseT>::setClosable(bool enable)
{
    pimpl->titleClose->setVisible(enable);
}

//--------------------------------------------------------------------------

template <typename BaseT>
QWidget* Dialog<BaseT>::titleBar() const
{
    return pimpl->titleFrame;
}

//--------------------------------------------------------------------------

template <typename BaseT>
void Dialog<BaseT>::setSvgIcon(std::shared_ptr<SvgIcon> icon)
{
    pimpl->icon->setVisible(static_cast<bool>(icon));
    pimpl->icon->setSvgIcon(std::move(icon));
}

//--------------------------------------------------------------------------

template <typename BaseT>
std::shared_ptr<SvgIcon> Dialog<BaseT>::svgIcon() const
{
    return pimpl->icon->svgIcon();
}

//--------------------------------------------------------------------------

template <typename BaseT>
void Dialog<BaseT>::setTitleControl(QWidget* widget)
{
    if (!pimpl->titleControl.isNull())
    {
        destroyWidget(pimpl->titleControl);
    }
    pimpl->titleControl=widget;

#ifdef Q_OS_MACOS
    // placeHolder exists solely to balance titleClose's width on the other side so the title
    // stays visually centered (see the ctor); a real title control sits at the same trailing
    // position and is close enough in size to serve that same balancing role on its own, so
    // the placeholder would otherwise just be dead space stacked after it, pushing it further
    // from the actual right edge than necessary. QBoxLayout gives a hidden widget zero space
    // (see doUpdateListAreaHeight()'s header-height comment for the same rule elsewhere in
    // this library), so hiding it -- rather than removing it from the layout -- is enough.
    pimpl->placeHolder->setVisible(widget==nullptr);
#endif

    if (widget==nullptr)
    {
        return;
    }

    // inserted just before the layout's last item -- titleClose on most platforms, the
    // (now hidden) placeHolder on macOS -- so it always lands right after the title text,
    // flush against the trailing edge on either layout
    widget->setParent(pimpl->titleFrame);
    pimpl->titleLayout->insertWidget(pimpl->titleLayout->count()-1,widget);
}

//--------------------------------------------------------------------------

}

#endif // UISE_DESKTOP_DIALOG_IPP

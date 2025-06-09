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

/** @file uise/desktop/src/editablepanel.cpp
*
*  Defines EditablePanel.
*
*/

/****************************************************************************/

#include <QLabel>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/utils/destroywidget.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/pushbutton.hpp>
#include <uise/desktop/editablepanel.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class EditablePanel_p
{
    public:

        bool editable=true;
        bool editingMode=false;
        bool hovered=false;

        QBoxLayout* layout=nullptr;

        QFrame* topButtonsFrame=nullptr;
        QBoxLayout* topButtonsLayout=nullptr;
        PushButton* topButtonEdit=nullptr;
        PushButton* topButtonApply=nullptr;
        PushButton* topButtonCancel=nullptr;
        EditablePanel::ButtonsMode buttonsMode=EditablePanel::ButtonsMode::TopAlwaysVisible;

        QFrame* titleFrame=nullptr;
        QBoxLayout* titleLayout=nullptr;
        QLabel* title=nullptr;
        bool titleVisible=false;

        QFrame* contentFrame=nullptr;
        QBoxLayout* contentLayout=nullptr;
        QWidget* contenWidget=nullptr;

        QFrame* bottomButtonsFrame=nullptr;
        QBoxLayout* bottomButtonsLayout=nullptr;
        PushButton* buttomButtonApply=nullptr;
        bool bottomButtonsVisible=true;
};

//--------------------------------------------------------------------------

EditablePanel::EditablePanel(
        QWidget* parent
    ) : QFrame(parent),
        pimpl(std::make_unique<EditablePanel_p>())
{
    pimpl->layout=Layout::vertical(this);

    pimpl->topButtonsFrame=new QFrame(this);
    pimpl->topButtonsFrame->setObjectName("topButtonsFrame");
    pimpl->topButtonsLayout=Layout::horizontal(pimpl->topButtonsFrame);
    pimpl->layout->addWidget(pimpl->topButtonsFrame);
    pimpl->topButtonEdit=new PushButton(tr("Edit"),pimpl->topButtonsFrame);
    pimpl->topButtonEdit->setObjectName("topButtonEdit");
    pimpl->topButtonsLayout->addWidget(pimpl->topButtonEdit,0,Qt::AlignRight);
    pimpl->topButtonApply=new PushButton(tr("Apply"),pimpl->topButtonsFrame);
    pimpl->topButtonApply->setObjectName("topButtonApply");
    pimpl->topButtonsLayout->addWidget(pimpl->topButtonApply,0,Qt::AlignLeft);
    pimpl->topButtonCancel=new PushButton(tr("Cancel"),pimpl->topButtonsFrame);
    pimpl->topButtonCancel->setObjectName("topButtonCancel");
    pimpl->topButtonsLayout->addWidget(pimpl->topButtonCancel,0,Qt::AlignRight);

    pimpl->titleFrame=new QFrame(this);
    pimpl->titleFrame->setObjectName("titleFrame");
    pimpl->titleLayout=Layout::horizontal(pimpl->titleFrame);
    pimpl->layout->addWidget(pimpl->titleFrame,0,Qt::AlignHCenter);
    pimpl->title=new QLabel(pimpl->titleFrame);
    pimpl->title->setObjectName("panelTitle");
    pimpl->titleLayout->addWidget(pimpl->title,0,Qt::AlignHCenter);

    pimpl->contentFrame=new QFrame(this);
    pimpl->contentFrame->setObjectName("contentFrame");
    pimpl->contentLayout=Layout::horizontal(pimpl->contentFrame);
    pimpl->layout->addWidget(pimpl->contentFrame,1);
    pimpl->contentFrame->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);

    pimpl->bottomButtonsFrame=new QFrame(this);
    pimpl->bottomButtonsFrame->setObjectName("bottomButtonsFrame");
    pimpl->bottomButtonsLayout=Layout::horizontal(pimpl->bottomButtonsFrame);
    pimpl->layout->addWidget(pimpl->bottomButtonsFrame,0,Qt::AlignCenter);
    pimpl->buttomButtonApply=new PushButton(tr("Apply"),pimpl->bottomButtonsFrame);
    pimpl->buttomButtonApply->setObjectName("bottomButtonsApply");
    pimpl->bottomButtonsLayout->addWidget(pimpl->buttomButtonApply);

    connect(
        pimpl->topButtonEdit,
        &PushButton::clicked,
        this,
        [this]()
        {
            edit();
        }
    );
    connect(
        pimpl->topButtonApply,
        &PushButton::clicked,
        this,
        [this]()
        {
            apply();
        }
    );
    connect(
        pimpl->topButtonCancel,
        &PushButton::clicked,
        this,
        [this]()
        {
            cancel();
        }
    );
    connect(
        pimpl->buttomButtonApply,
        &PushButton::clicked,
        this,
        [this]()
        {
            apply();
        }
    );

    updateState();
}

//--------------------------------------------------------------------------

EditablePanel::~EditablePanel()
{}

//--------------------------------------------------------------------------

void EditablePanel::setEditable(bool enable)
{
    pimpl->editable=enable;
    updateState();
}

//--------------------------------------------------------------------------

bool EditablePanel::isEditable() const noexcept
{
    return pimpl->editable;
}

//--------------------------------------------------------------------------

void EditablePanel::setEditingMode(bool enable)
{
    pimpl->editingMode=enable;
    updateState();
}

//--------------------------------------------------------------------------

bool EditablePanel::isEditingMode() const noexcept
{
    return pimpl->editingMode;
}

//--------------------------------------------------------------------------

void EditablePanel::setTitle(const QString& title)
{
    pimpl->title->setText(title);
    setTitleVisible(!title.isEmpty());
}

//--------------------------------------------------------------------------

QString EditablePanel::title() const
{
    return pimpl->title->text();
}

//--------------------------------------------------------------------------

void EditablePanel::setTitleVisible(bool enable)
{
    pimpl->titleVisible=enable;
    updateState();
}

//--------------------------------------------------------------------------

bool EditablePanel::isTitleVisible() const noexcept
{
    return pimpl->titleVisible;
}

//--------------------------------------------------------------------------

void EditablePanel::setButtonsMode(ButtonsMode mode)
{
    pimpl->buttonsMode=mode;
    updateState();
}

//--------------------------------------------------------------------------

EditablePanel::ButtonsMode EditablePanel::buttonsMode() const noexcept
{
    return pimpl->buttonsMode;
}

//--------------------------------------------------------------------------

void EditablePanel::setBottomApplyText(const QString& text)
{
    pimpl->buttomButtonApply->setText(text);
}

//--------------------------------------------------------------------------

QString EditablePanel::bottomApplyText() const
{
    return pimpl->buttomButtonApply->text();
}

//--------------------------------------------------------------------------

void EditablePanel::setBottomApplyIcon(std::shared_ptr<SvgIcon> icon)
{
    pimpl->buttomButtonApply->setSvgIcon(std::move(icon));
}

//--------------------------------------------------------------------------

std::shared_ptr<SvgIcon> EditablePanel::bottomApplyIcon() const
{
    return pimpl->buttomButtonApply->svgIcon();
}

//--------------------------------------------------------------------------

void EditablePanel::setWidget(QWidget* widget)
{
    destroyWidget(pimpl->contenWidget);
    pimpl->contenWidget=widget;
    pimpl->contentLayout->addWidget(widget,1);
}

//--------------------------------------------------------------------------

void EditablePanel::edit()
{
    if (isEditingMode())
    {
        return;
    }

    setEditingMode(true);
    emit editRequested();
}

//--------------------------------------------------------------------------

void EditablePanel::apply()
{
    if (!isEditingMode())
    {
        return;
    }

    setEditingMode(false);
    emit applyRequested();
}

//--------------------------------------------------------------------------

void EditablePanel::cancel()
{
    if (!isEditingMode())
    {
        return;
    }

    setEditingMode(false);
    emit cancelRequested();
}

//--------------------------------------------------------------------------

void EditablePanel::updateState()
{
    pimpl->titleFrame->setVisible(pimpl->titleVisible);
    pimpl->bottomButtonsFrame->setVisible(isEditingMode() && buttonsMode()==ButtonsMode::BottomAlwaysVisible);

    auto topButtonsVisible=
                          pimpl->editable
                          &&
                          (
                             buttonsMode()==ButtonsMode::TopAlwaysVisible ||
                             (buttonsMode()==ButtonsMode::TopOnHoverVisible && (pimpl->hovered || isEditingMode()))
                          );
    pimpl->topButtonsFrame->setVisible(topButtonsVisible);
    pimpl->topButtonEdit->setVisible(!isEditingMode());
    pimpl->topButtonApply->setVisible(isEditingMode());
    pimpl->topButtonCancel->setVisible(isEditingMode());
}

//--------------------------------------------------------------------------

void EditablePanel::enterEvent(QEnterEvent* event)
{
    pimpl->hovered=true;
    QFrame::enterEvent(event);
    if (buttonsMode()==ButtonsMode::TopOnHoverVisible)
    {
        updateState();
    }
}

//--------------------------------------------------------------------------

void EditablePanel::leaveEvent(QEvent* event)
{
    pimpl->hovered=false;
    QFrame::leaveEvent(event);
    if (buttonsMode()==ButtonsMode::TopOnHoverVisible)
    {
        updateState();
    }
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END

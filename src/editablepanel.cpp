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
#include <uise/desktop/loadingframe.hpp>

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
        PushButton* bottomButtonApply=nullptr;
        bool bottomButtonsVisible=true;

        QFrame* statusFrame=nullptr;
        QBoxLayout* statusLayout=nullptr;
        QLabel* statusText;

        LoadingFrame* loadingFrame=nullptr;
};

//--------------------------------------------------------------------------

EditablePanel::EditablePanel(
        QWidget* parent
    ) : AbstractEditablePanel(parent),
        pimpl(std::make_unique<EditablePanel_p>())
{
    auto l=Layout::vertical(this);
    pimpl->loadingFrame=new LoadingFrame(this);
    pimpl->loadingFrame->construct();
    l->addWidget(pimpl->loadingFrame,1);

    pimpl->layout=Layout::vertical(pimpl->loadingFrame);

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

    pimpl->statusFrame=new QFrame(this);
    pimpl->statusFrame->setObjectName("statusFrame");
    pimpl->statusLayout=Layout::horizontal(pimpl->statusFrame);
    pimpl->layout->addWidget(pimpl->statusFrame);
    pimpl->statusText=new QLabel(pimpl->statusFrame);
    pimpl->statusText->setObjectName("statusText");
    pimpl->statusText->setWordWrap(true);
    pimpl->statusText->setTextInteractionFlags(Qt::TextBrowserInteraction);
    pimpl->statusLayout->addWidget(pimpl->statusText,0,Qt::AlignLeft);

    pimpl->bottomButtonsFrame=new QFrame(this);
    pimpl->bottomButtonsFrame->setObjectName("bottomButtonsFrame");
    pimpl->bottomButtonsLayout=Layout::horizontal(pimpl->bottomButtonsFrame);
    pimpl->layout->addWidget(pimpl->bottomButtonsFrame,0,Qt::AlignCenter);
    pimpl->bottomButtonApply=new PushButton(tr("Apply"),pimpl->bottomButtonsFrame);
    pimpl->bottomButtonApply->setObjectName("bottomButtonsApply");
    pimpl->bottomButtonsLayout->addWidget(pimpl->bottomButtonApply);

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
        pimpl->bottomButtonApply,
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
    pimpl->bottomButtonApply->setText(text);
}

//--------------------------------------------------------------------------

QString EditablePanel::bottomApplyText() const
{
    return pimpl->bottomButtonApply->text();
}

//--------------------------------------------------------------------------

void EditablePanel::setBottomApplyIcon(std::shared_ptr<SvgIcon> icon)
{
    pimpl->bottomButtonApply->setSvgIcon(std::move(icon));
}

//--------------------------------------------------------------------------

std::shared_ptr<SvgIcon> EditablePanel::bottomApplyIcon() const
{
    return pimpl->bottomButtonApply->svgIcon();
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

    setBusyWaiting(false);
    setEditingMode(true);
    emit editRequested();
}

//--------------------------------------------------------------------------

void EditablePanel::cancel()
{
    if (!isEditingMode())
    {
        return;
    }

    setBusyWaiting(false);
    setEditingMode(false);
    emit cancelRequested();
}

//--------------------------------------------------------------------------

void EditablePanel::doCommitApply()
{
    setEditingMode(false);
}

//--------------------------------------------------------------------------

void EditablePanel::updateState()
{
    bool editing=isEditingMode();
    pimpl->titleFrame->setVisible(pimpl->titleVisible);
    pimpl->bottomButtonsFrame->setVisible(isEditingMode() && buttonsMode()==ButtonsMode::BottomAlwaysVisible);

    auto topButtonsVisible=
                          pimpl->editable
                          &&
                          (
                             buttonsMode()==ButtonsMode::TopAlwaysVisible ||
                             (buttonsMode()==ButtonsMode::TopOnHoverVisible && (pimpl->hovered || editing))
                          );

    pimpl->topButtonsFrame->setVisible(buttonsMode()==ButtonsMode::TopOnHoverVisible || buttonsMode()==ButtonsMode::TopAlwaysVisible);
    pimpl->topButtonEdit->setEnabled(topButtonsVisible);
    pimpl->topButtonEdit->setVisible(!editing);
    pimpl->topButtonApply->setVisible(topButtonsVisible && editing);
    pimpl->topButtonCancel->setVisible(topButtonsVisible && editing);

    if (!editing)
    {
        resetStatus();
    }
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

void EditablePanel::setBusyWaiting(bool enable)
{
    pimpl->loadingFrame->setBusyWaiting(enable);
    if (enable)
    {
        resetStatus();
    }
}

//--------------------------------------------------------------------------

void EditablePanel::resetStatus()
{
    pimpl->statusText->setText(" ");
    pimpl->statusText->setProperty("status",QVariant{});
}

//--------------------------------------------------------------------------

void EditablePanel::showStatus(const QString& message, const QString& status)
{
    setBusyWaiting(false);
    pimpl->statusText->setText(message);
    pimpl->statusText->setProperty("status",status);
    Style::updateWidgetStyle(pimpl->statusText);
}

//--------------------------------------------------------------------------

void EditablePanel::doBeginApply()
{
    resetStatus();
}

//--------------------------------------------------------------------------

void EditablePanel::contentEdited()
{
    resetStatus();
}

//--------------------------------------------------------------------------

int AbstractEditablePanel::addValueWidget(AbstractValueWidget* widget)
{
    widget->setEditablePanel(this);
    widget->setGroupEditingRequestEnabled(m_requestGroupEditingEnabled);
    if (m_requestGroupEditingEnabled)
    {
        connect(
            widget,
            &AbstractValueWidget::groupEditingRequested,
            this,
            &AbstractEditablePanel::edit
            );
        connect(
            widget,
            &AbstractValueWidget::groupCancelRequested,
            this,
            &AbstractEditablePanel::cancel
        );
    }
    const auto& config=widget->config();
    return addRow(
        config.property(ValueWidgetProperty::Label).toString(),
        widget,
        config.property(ValueWidgetProperty::ColumnSpan,1).toInt(),
        static_cast<Qt::Alignment>(config.property(ValueWidgetProperty::ColumnSpan,static_cast<int>(Qt::Alignment{})).toInt()),
        config.property(ValueWidgetProperty::Comment).toString(),
        config.property(ValueWidgetProperty::RowSpan,1).toInt()
    );
}

//--------------------------------------------------------------------------

void AbstractEditablePanel::commitApply()
{
    doCommitApply();
    setBusyWaiting(false);
    emit applyCommited();
}

//--------------------------------------------------------------------------

void AbstractEditablePanel::apply()
{
    if (!isEditingMode())
    {
        return;
    }

    doBeginApply();
    emit applyRequested();
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END

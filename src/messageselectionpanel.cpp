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
/*

*/
/** @file uise/desktop/messageselectionpanel.сpp
  *
  */

#include <QFrame>

#include <uise/desktop/style.hpp>
#include <uise/desktop/pushbutton.hpp>
#include <uise/desktop/messageselectionpanel.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/************************** MessageSelectionPanelUi *****************************/

MessageSelectionPanelUi::MessageSelectionPanelUi(QWidget* parent) : FrameWithDrawer(parent)
{
    setAutocloseDisabled(true);
    setDrawerAlpha(0);
    setSlideDurationMs(110);

    auto l=Layout::vertical(this);

    m_drawer=new QFrame(this);
    m_drawer->setObjectName("drawer");
    l->addWidget(m_drawer);

    auto dl=Layout::horizontal(m_drawer);

    auto addButton=[this,dl](const QString& name)
    {
        auto button=new PushButton(name,this);
        button->setCursor(Qt::PointingHandCursor);
        button->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Fixed);
        dl->addWidget(button,0,Qt::AlignVCenter);
        return button;
    };

    m_copyTmpl=tr("Copy");
    m_forwardTmpl=tr("Forward");
    m_deleteTmpl=tr("Delete");

    m_copyButton=addButton(m_copyTmpl);
    m_forwardButton=addButton(m_forwardTmpl);
    m_deleteButton=addButton(m_deleteTmpl);

    dl->addStretch(1);

    m_cancelButton=addButton(tr("Cancel"));
    m_cancelButton->setObjectName("cancel");

    setDrawerEdge(Qt::TopEdge);
    setDrawerWidget(m_drawer);
    setSizePercent(100);
}

//--------------------------------------------------------------------------

void MessageSelectionPanelUi::setSelectionController(AbstractMessageSelectionPanel* ctrl)
{
    m_ctrl=ctrl;

    connect(
        m_copyButton,
        &PushButton::clicked,
        selectionController(),
        &AbstractMessageSelectionPanel::copyRequested
    );
    connect(
        m_forwardButton,
        &PushButton::clicked,
        selectionController(),
        &AbstractMessageSelectionPanel::forwardRequested
    );
    connect(
        m_deleteButton,
        &PushButton::clicked,
        selectionController(),
        &AbstractMessageSelectionPanel::deleteRequested
    );
    connect(
        m_cancelButton,
        &PushButton::clicked,
        this,
        [this]()
        {
            closeDrawer();
            emit selectionController()->cancelRequested();
        }
    );
}

//--------------------------------------------------------------------------

void MessageSelectionPanelUi::setSelectedCount(const QString& value)
{
    if (value.isEmpty()||value=="0")
    {
        m_copyButton->setText(m_copyTmpl);
        m_forwardButton->setText(m_forwardTmpl);
        m_deleteButton->setText(m_deleteTmpl);
    }
    else
    {
        QString count{" (%1)"};
        m_copyButton->setText(QString{m_copyTmpl+count}.arg(value));
        m_forwardButton->setText(QString{m_forwardTmpl+count}.arg(value));
        m_deleteButton->setText(QString{m_deleteTmpl+count}.arg(value));
    }
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END

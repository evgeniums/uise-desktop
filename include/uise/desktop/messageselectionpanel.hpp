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

/** @file uise/desktop/messageselectionpanel.hpp
*
*  Declares frame with drawer control.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_MESSAGESELECTIONPANEL_HPP
#define UISE_DESKTOP_MESSAGESELECTIONPANEL_HPP

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/drawer.hpp>
#include <uise/desktop/widget.hpp>
#include <uise/desktop/pushbutton.hpp>

class QFrame;

UISE_DESKTOP_NAMESPACE_BEGIN

class UISE_DESKTOP_EXPORT AbstractMessageSelectionPanel : public UISE_DESKTOP_NAMESPACE::WidgetController
{
    Q_OBJECT

    public:

        using WidgetController::WidgetController;

        virtual void setSelectedCount(const QString& count) =0;

        virtual void setContent(QWidget* widget) =0;

    public slots:

        virtual void setVisible(bool enable) =0;

    signals:

        void copyRequested();
        void forwardRequested();
        void deleteRequested();
        void cancelRequested();
};

class UISE_DESKTOP_EXPORT MessageSelectionPanelUi : public FrameWithDrawer,
                                                  public Widget
{
    Q_OBJECT

    public:

        MessageSelectionPanelUi(QWidget* parent=nullptr);

        void setSelectionController(AbstractMessageSelectionPanel* value);

        AbstractMessageSelectionPanel* selectionController() const
        {
            return m_ctrl;
        }

        QWidget* qWidget()
        {
            return this;
        }

        void setSelectedCount(const QString& count);

    private:

        QFrame* m_drawer;
        UISE_DESKTOP_NAMESPACE::PushButton* m_copyButton;
        UISE_DESKTOP_NAMESPACE::PushButton* m_forwardButton;
        UISE_DESKTOP_NAMESPACE::PushButton* m_deleteButton;

        UISE_DESKTOP_NAMESPACE::PushButton* m_cancelButton;

        QString m_copyTmpl;
        QString m_forwardTmpl;
        QString m_deleteTmpl;

        AbstractMessageSelectionPanel* m_ctrl=nullptr;
};

class UISE_DESKTOP_EXPORT MessageSelectionPanel : public AbstractMessageSelectionPanel
{
    Q_OBJECT

    public:

        using AbstractMessageSelectionPanel::AbstractMessageSelectionPanel;

        void setSelectedCount(const QString& value) override
        {
            m_ui->setSelectedCount(value);
        }

        void setVisible(bool enable) override
        {
            m_ui->setDrawerVisible(enable);
        }

        void setContent(QWidget* widget) override
        {
            m_ui->setContentWidget(widget);
        }

    protected:

        Widget* doCreateActualWidget(QWidget* parent) override
        {
            m_ui=new MessageSelectionPanelUi(parent);
            m_ui->setSelectionController(this);
            return m_ui;
        }

    private:

        MessageSelectionPanelUi* m_ui=nullptr;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_MESSAGESELECTIONPANEL_HPP

/**
@copyright Evgeny Sidorov 2021

This software is dual-licensed. Choose the appropriate license for your project.

1. The GNU GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-GPLv3.md](LICENSE-GPLv3.md) or copy at https://www.gnu.org/licenses/gpl-3.0.txt)
    
2. The GNU LESSER GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-LGPLv3.md](LICENSE-LGPLv3.md) or copy at https://www.gnu.org/licenses/lgpl-3.0.txt).

You may select, at your option, one of the above-listed licenses.

*/

/****************************************************************************/

/** @file uise/desktop/editablepanel.hpp
*
*  Declares EditablePanel.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_EDITABLEPANEL_HPP
#define UISE_DESKTOP_EDITABLEPANEL_HPP

#include <memory>

#include <QFrame>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/widget.hpp>
#include <uise/desktop/valuewidget.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class SvgIcon;

class UISE_DESKTOP_EXPORT AbstractEditablePanel : public WidgetQFrame
{
    Q_OBJECT

    public:

        struct Item
        {
            QWidget* widget=nullptr;
            Qt::Alignment alignment;
            int columnSpan=1;
            int rowSpan=1;

            Item(
                    QWidget* widget,
                    Qt::Alignment alignment=Qt::Alignment{},
                    int columnSpan=1,
                    int rowSpan=1
                ) : widget(widget),
                    alignment(alignment),
                    columnSpan(columnSpan),
                    rowSpan(rowSpan)
            {}
        };

        enum class ButtonsMode : int
        {
            TopAlwaysVisible,
            TopOnHoverVisible,
            BottomAlwaysVisible,
            ButtonsInvisible
        };

        using WidgetQFrame::WidgetQFrame;

        virtual void setEditable(bool enable)=0;
        virtual bool isEditable() const noexcept=0;

        virtual bool isEditingMode() const noexcept=0;

        virtual void setTitle(const QString& title)=0;
        virtual QString title() const=0;

        virtual void setTitleVisible(bool enable)=0;
        virtual bool isTitleVisible() const noexcept=0;

        virtual void setButtonsMode(ButtonsMode mode)=0;
        virtual ButtonsMode buttonsMode() const noexcept=0;

        virtual void setBottomApplyText(const QString& text)=0;
        virtual QString bottomApplyText() const=0;

        virtual void setBottomApplyIcon(std::shared_ptr<SvgIcon> icon)=0;
        virtual std::shared_ptr<SvgIcon> bottomApplyIcon() const=0;

        virtual void setWidget(QWidget* widget)=0;

        int addRow(const QString& label, QWidget* widget, int columnSpan=1, Qt::Alignment alignment=Qt::Alignment{}, const QString& comment={}, int rowSpan=1)
        {
            return addRow(label,{Item{widget,alignment,columnSpan,1}},comment);
        }

        int addRow(std::vector<Item> items, const QString& comment={})
        {
            return addRow("",std::move(items),comment);
        }

        virtual int addRow(const QString& label, Item item, const QString& comment={})
        {
            return addRow(label,std::vector<Item>{std::move(item)},comment);
        }

        virtual int addRow(const QString& label, std::vector<Item> items, const QString& comment={})=0;

        virtual int addValueWidget(AbstractValueWidget* widget);

        virtual void setRowVisible(int index, bool enable)=0;
        virtual bool isRowVisible(int index) const =0;

        virtual void setComment(int index, const QString& comment)=0;
        virtual void setLabel(int index, const QString& label)=0;

    signals:

        void editRequested();
        void cancelRequested();
        void applyRequested();

    public slots:

        virtual void edit()=0;
        virtual void apply()=0;
        virtual void cancel()=0;
};

class EditablePanel_p;

class UISE_DESKTOP_EXPORT EditablePanel : public AbstractEditablePanel
{
    Q_OBJECT

    public:

        EditablePanel(QWidget* parent=nullptr);

        ~EditablePanel();

        EditablePanel(const EditablePanel&)=delete;
        EditablePanel(EditablePanel&&)=delete;
        EditablePanel& operator=(const EditablePanel&)=delete;
        EditablePanel& operator=(EditablePanel&&)=delete;

        void setEditable(bool enable) override final;
        bool isEditable() const noexcept override final;

        bool isEditingMode() const noexcept override final;

        void setTitle(const QString& title) override final;
        QString title() const override final;

        void setTitleVisible(bool enable) override final;
        bool isTitleVisible() const noexcept override final;

        void setButtonsMode(ButtonsMode mode) override final;
        ButtonsMode buttonsMode() const noexcept override final;

        void setBottomApplyText(const QString& text) override final;
        QString bottomApplyText() const override final;

        void setBottomApplyIcon(std::shared_ptr<SvgIcon> icon) override final;
        std::shared_ptr<SvgIcon> bottomApplyIcon() const override final;

        void setWidget(QWidget* widget) override final;

    public slots:

        void edit() override final;
        void apply() override final;
        void cancel() override final;

    protected:

        void enterEvent(QEnterEvent* event) override;
        void leaveEvent(QEvent* event) override;

    private:

        void setEditingMode(bool enable);
        void updateState();

        std::unique_ptr<EditablePanel_p> pimpl;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_EDITABLEPANEL_HPP

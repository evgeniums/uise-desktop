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

#include <boost/hana.hpp>

#include <QFrame>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/modalpopup.hpp>
#include <uise/desktop/editablelabel.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class EditablePanel_p;

class UISE_DESKTOP_EXPORT EditablePanel : public FrameWithModalPopup
{
    Q_OBJECT

    public:

        enum class Buttons : int
        {
            Edit,
            Apply,
            Cancel,
            Save,
            Connect,
            Send,
            Complete,

            Custom=0x100
        };

        struct Row
        {
            QString name;
            std::vector<EditableLabelConfig> data;
        };

        EditablePanel(QWidget* parent=nullptr);

        ~EditablePanel();

        EditablePanel(const EditablePanel&)=delete;
        EditablePanel(EditablePanel&&)=delete;
        EditablePanel& operator=(const EditablePanel&)=delete;
        EditablePanel& operator=(EditablePanel&&)=delete;

        bool isEditable() const noexcept;

        bool isEditingMode() const noexcept;

        void loadRows(const std::vector<Row>& rows);

        QVariantMap data() const;
        void loadData(const QVariantMap& data) const;

        template <typename ...Buttons>
        void setButtons(Buttons&& ... buttons)
        {
            auto ids=boost::hana::make_tuple(buttons...);
            auto intIds=boost::hana::fold(
                ids,
                std::vector<int>{},
                [](auto&& state, auto&& id)
                {
                    state.push_back(static_cast<int>(id));
                    return state;
                }
            );
            setButtons(std::move(intIds));
        }

        void setButtons(std::vector<int> buttons);

        template <typename T>
        PushButton* button(T buttonId)
        {
            return button(static_cast<int>(buttonId));
        }

        PushButton* button(int buttonId) const;

        template <typename T>
        void emitButtonActivated(T id)
        {
            emit buttonActivated(static_cast<int>(id));
        }

    signals:

        void buttonActivated(int);

    public slots:

        void setOperationStatus(
            const QString& error={}
        );

        void setEditable(bool enable);

        void setEditingMode(bool enable);

        /**
         * @brief Cancel editing.
         */
        void cancel()
        {
            setEditingMode(false);
        }

        /**
         * @brief Enable editing.
         */
        void edit()
        {
            setEditingMode(true);
        }

    private:

        std::unique_ptr<EditablePanel_p> pimpl;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_EDITABLEPANEL_HPP

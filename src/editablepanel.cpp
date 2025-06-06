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

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/editablepanel.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class EditablePanel_p
{
    public:

        bool editable=true;
        bool editingMode=false;

        QGridLayout* layout=nullptr;

        QFrame buttonsFrame;
        std::map<int,PushButton*> buttons;

        std::map<QString,EditableLabel*> labels;
};

//--------------------------------------------------------------------------

EditablePanel::EditablePanel(
        QWidget* parent
    ) : FrameWithModalPopup(parent),
        pimpl(std::make_unique<EditablePanel_p>())
{
}

//--------------------------------------------------------------------------

EditablePanel::~EditablePanel()
{}

//--------------------------------------------------------------------------

void EditablePanel::setEditable(bool enable)
{
    pimpl->editable=enable;
    if (!enable && pimpl->editingMode)
    {
        setEditingMode(false);
    }
}

//--------------------------------------------------------------------------

void EditablePanel::setEditingMode(bool enable)
{
    pimpl->editingMode=enable;
    //! @todo Update editing mode of editable labels
    //! @todo update buttons
    //!
    if (!enable)
    {
        emitButtonActivated(Buttons::Cancel);
    }
}

//--------------------------------------------------------------------------

bool EditablePanel::isEditable() const noexcept
{
    return pimpl->editable;
}

//--------------------------------------------------------------------------

bool EditablePanel::isEditingMode() const noexcept
{
    return pimpl->editingMode;
}

//--------------------------------------------------------------------------

void EditablePanel::loadRows(const std::vector<Row>& rows)
{

}

//--------------------------------------------------------------------------

void EditablePanel::setButtons(std::vector<int> buttons)
{

}

//--------------------------------------------------------------------------

PushButton* EditablePanel::button(int buttonId) const
{
    //! @todo Implement button()
    return nullptr;
}

//--------------------------------------------------------------------------

void EditablePanel::setOperationStatus(const QString& error)
{
    std::ignore=error;
    //! @todo Implement setOperationStatus
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END

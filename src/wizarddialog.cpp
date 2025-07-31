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

/** @file uise/desktop/src/wizarddialog.cpp
*
*  Defines WizardDialog.
*
*/

/****************************************************************************/

#include <QStackedWidget>

#include <uise/desktop/wizarddialog.hpp>

#include <uise/desktop/ipp/dialog.ipp>

UISE_DESKTOP_NAMESPACE_BEGIN

/**************************** WizardDialog ***********************************/

//--------------------------------------------------------------------------

class WizardDialog_p
{
    public:

        QStackedWidget* stack;

        std::vector<std::shared_ptr<SvgIcon>> icons;
};

//--------------------------------------------------------------------------

WizardDialog::WizardDialog(QWidget* parent)
    : Base(parent),
      pimpl(std::make_unique<WizardDialog_p>())
{
    pimpl->stack=new QStackedWidget();
    setWidget(pimpl->stack);

    setButtons(
        {
            StandardButton::Back,
            StandardButton::Next,
            StandardButton::Complete,
            StandardButton::Cancel
        }
    );
}

//--------------------------------------------------------------------------

WizardDialog::~WizardDialog()
{
}

//--------------------------------------------------------------------------

int WizardDialog::currentIndex() const
{
    return pimpl->stack->currentIndex();
}

//--------------------------------------------------------------------------

void WizardDialog::addPage(QWidget* page, std::shared_ptr<SvgIcon> icon)
{
    pimpl->stack->addWidget(page);
    pimpl->icons.emplace_back(std::move(icon));
}

//--------------------------------------------------------------------------

void WizardDialog::setCurrentIndex(int index)
{
    pimpl->stack->setCurrentIndex(index);

    auto icon=pimpl->icons.at(index);
    if (!icon)
    {
        icon=defaultSvgIcon();
    }
    setSvgIcon(std::move(icon));

    setButtonVisible(StandardButton::Back,index!=0 && pimpl->icons.size()>1);
    setButtonVisible(StandardButton::Next,index!=(pimpl->icons.size()-1));
    setButtonVisible(StandardButton::Complete,index==pimpl->icons.size()-1);
}

//--------------------------------------------------------------------------

void WizardDialog::updateDefaultSvgIcon()
{
    if (pimpl->icons.empty())
    {
        return;
    }

    auto icon=pimpl->icons.at(currentIndex());
    if (!icon)
    {
        icon=defaultSvgIcon();
        setSvgIcon(std::move(icon));
    }
}

//--------------------------------------------------------------------------

void WizardDialog::setPageSvgIcon(int index, std::shared_ptr<SvgIcon> icon)
{
    pimpl->icons[index]=icon;
    if (index==currentIndex())
    {
        setSvgIcon(std::move(icon));
    }
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END

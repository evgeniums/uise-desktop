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

/** @file uise/desktop/defaultwidgetfactory.cpp
*
*  Defines defaultWidgetFactory().
*
*/

/****************************************************************************/

#include <uise/desktop/statusdialog.hpp>
#include <uise/desktop/editablepanelgrid.hpp>
#include <uise/desktop/editablepanels.hpp>
#include <uise/desktop/loadingframe.hpp>

#include <uise/desktop/defaultwidgetfactory.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

//--------------------------------------------------------------------------

std::shared_ptr<WidgetFactory> defaultWidgetFactory()
{
    auto factory=std::make_shared<WidgetFactory>();

    factory->registerBuilder<AbstractStatusDialog>([](QWidget* parent){return new StatusDialog(parent);});
    factory->registerBuilder<AbstractEditablePanel>([](QWidget* parent){return new EditablePanelGrid(parent);});
    factory->registerBuilder<AbstractEditablePanels>([](QWidget* parent){return new EditablePanels(parent);});

    factory->registerBuilder<LoadingFrame>([](QWidget* parent){return new LoadingFrame(parent);});

    return factory;
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END

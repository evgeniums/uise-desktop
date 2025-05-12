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

/** @file uise/desktop/htreesidebar.cpp
*
*  Defines HTreeSideBar.
*
*/

/****************************************************************************/

#include <QSplitter>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/utils/destroywidget.hpp>
#include <uise/desktop/scrollarea.hpp>

#include <uise/desktop/htreesidebar.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

//--------------------------------------------------------------------------

class HTreeSideBar_p
{
    public:

        HTree* tree=nullptr;
};

//--------------------------------------------------------------------------

HTreeSideBar::HTreeSideBar(HTree* tree, QWidget* parent)
    : QFrame(parent),
      pimpl(std::make_unique<HTreeSideBar_p>())
{
    pimpl->tree=tree;
}

//--------------------------------------------------------------------------

HTreeSideBar::~HTreeSideBar()
{}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END

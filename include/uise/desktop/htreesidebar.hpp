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

/** @file uise/desktop/htreesidebar.hpp
*
*  Declares horizontal tree side bar.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_HTREE_SIDEBAR_HPP
#define UISE_DESKTOP_HTREE_SIDEBAR_HPP

#include <memory>

#include <QFrame>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/framewithrefresh.hpp>

#include <uise/desktop/htreepath.hpp>

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

class HTree;

class HTreeSideBar_p;

class UISE_DESKTOP_EXPORT HTreeSideBar : public QFrame
{
        Q_OBJECT

    public:

        /**
         * @brief Constructor.
         * @param parent Parent widget.
         */
        HTreeSideBar(HTree* tree, QWidget* parent=nullptr);

        /**
         * @brief Destructor.
         */
        ~HTreeSideBar();

        HTreeSideBar(const HTreeSideBar&)=delete;
        HTreeSideBar(HTreeSideBar&&)=delete;
        HTreeSideBar& operator=(const HTreeSideBar&)=delete;
        HTreeSideBar& operator=(HTreeSideBar&&)=delete;

    private:

        std::unique_ptr<HTreeSideBar_p> pimpl;
};

}

#endif // UISE_DESKTOP_HTREE_SIDEBAR_HPP

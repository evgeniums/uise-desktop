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

/** @file uise/desktop/htreebranch.hpp
*
*  Declares branch node of horizontal tree.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_HTREE_BRANCH_HPP
#define UISE_DESKTOP_HTREE_BRANCH_HPP

#include <memory>

#include <QFrame>

#include <uise/desktop/uisedesktop.hpp>

#include <uise/desktop/htreepath.hpp>
#include <uise/desktop/htreenode.hpp>

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

class HTreeBranch_p;

class UISE_DESKTOP_EXPORT HTreeBranch : public HTreeNode
{
    Q_OBJECT

    public:

        /**
         * @brief Constructor.
         * @param tree The tree this node belongs to.
         * @param parent Parent widget.
         */
        HTreeBranch(HTreeTab* treeTab, QWidget* parent=nullptr);

        /**
         * @brief Constructor.
         * @param parent Parent widget.
         */
        HTreeBranch(QWidget* parent=nullptr);

        /**
         * @brief Destructor.
         */
        ~HTreeBranch();

        HTreeBranch(const HTreeBranch&)=delete;
        HTreeBranch(HTreeBranch&&)=delete;
        HTreeBranch& operator=(const HTreeBranch&)=delete;
        HTreeBranch& operator=(HTreeBranch&&)=delete;

        HTreeNode* loadNextNode(const HTreePathElement& pathElement, bool last=true);
        void closeNextNode();

    public slots:

        void openNextNode(const UISE_DESKTOP_NAMESPACE::HTreePathElement& pathElement, const UISE_DESKTOP_NAMESPACE::HTreePath& residentPath={}, bool exclusive=false);
        void openNextNodeInNewTab(const UISE_DESKTOP_NAMESPACE::HTreePathElement& pathElement, const UISE_DESKTOP_NAMESPACE::HTreePath& residentPath={});
        void openNextNodeInNewTree(const UISE_DESKTOP_NAMESPACE::HTreePathElement& pathElement, const UISE_DESKTOP_NAMESPACE::HTreePath& residentPath={});

        void openNextNodes(const UISE_DESKTOP_NAMESPACE::HTreePath& subPath, const UISE_DESKTOP_NAMESPACE::HTreePath& residentPath={});
        void openNextNodesInNewTab(const UISE_DESKTOP_NAMESPACE::HTreePath& subPath, const UISE_DESKTOP_NAMESPACE::HTreePath& residentPath={});
        void openNextNodesInNewTree(const UISE_DESKTOP_NAMESPACE::HTreePath& subPath, const UISE_DESKTOP_NAMESPACE::HTreePath& residentPath={});

    private:

        std::unique_ptr<HTreeBranch_p> pimpl;
};

}

#endif // UISE_DESKTOP_HTREE_BRANCH_HPP

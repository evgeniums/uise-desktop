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

UISE_DESKTOP_NAMESPACE_BEGIN

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

        HTreeNode* loadNextNode(const HTreePathElement& pathElement);
        HTreeNode* nextNode() const;
        void closeNextNode();

        bool isExpanded() const;

    public slots:

        void setExpanded(bool enable);

        void openNextNode(const UISE_DESKTOP_NAMESPACE::HTreePathElement& pathElement);
        void openNextNodeInNewTab(const UISE_DESKTOP_NAMESPACE::HTreePathElement& pathElement);

    private:

        std::unique_ptr<HTreeBranch_p> pimpl;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_HTREE_BRANCH_HPP

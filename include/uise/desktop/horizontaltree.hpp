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

/** @file uise/desktop/horizontaltree.hpp
*
*  Declares horizontal tree control with loadable nodes, navigation history and bookmarks of tree paths.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_HORIZONTAL_TREE_HPP
#define UISE_DESKTOP_HORIZONTAL_TREE_HPP

#include <memory>

#include <QFrame>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/framewithrefresh.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class HorizontalTreeePathElement
{
    public:

        HorizontalTreeePathElement(
            std::string type,
            std::string id,
            std::string name={}
        ) : m_type(std::move(type)),
            m_id(std::move(id)),
            m_name(std::move(name))
        {}

        const std::string& type() const noexcept
        {
            return m_type;
        }

        const std::string& id() const noexcept
        {
            return m_id;
        }

        const std::string& name() const noexcept
        {
            return m_name;
        }

    private:

        std::string m_type;
        std::string m_id;
        std::string m_name;
};

class HorizontalTreeePath
{
    public:

        HorizontalTreeePath(std::vector<HorizontalTreeePathElement> elements={}) : m_elements(std::move(elements))
        {}

        const std::vector<HorizontalTreeePathElement>& elements() const noexcept
        {
            return m_elements;
        }

        std::vector<HorizontalTreeePathElement>& elements() noexcept
        {
            return m_elements;
        }

        void setElements(std::vector<HorizontalTreeePathElement> elements)
        {
            m_elements=std::move(elements);
        }

    private:

        std::vector<HorizontalTreeePathElement> m_elements;
};

class HorizontalTree;

class HorizontalTreeSideBar_p;

class UISE_DESKTOP_EXPORT HorizontalTreeSideBar : public QFrame
{
        Q_OBJECT

    public:

        /**
         * @brief Constructor.
         * @param parent Parent widget.
         */
        HorizontalTreeSideBar(HorizontalTree* tree, QWidget* parent=nullptr);

        /**
         * @brief Destructor.
         */
        ~HorizontalTreeSideBar();

        HorizontalTreeSideBar(const HorizontalTreeSideBar&)=delete;
        HorizontalTreeSideBar(HorizontalTreeSideBar&&)=delete;
        HorizontalTreeSideBar& operator=(const HorizontalTreeSideBar&)=delete;
        HorizontalTreeSideBar& operator=(HorizontalTreeSideBar&&)=delete;

    private:

        std::unique_ptr<HorizontalTreeSideBar_p> pimpl;
};

class HorizontalTreeNode_p;

class UISE_DESKTOP_EXPORT HorizontalTreeNode : public FrameWithRefresh
{
    Q_OBJECT

    public:

        /**
         * @brief Constructor.
         * @param tree The tree this node belongs to.
         * @param parent Parent widget.
         */
        HorizontalTreeNode(HorizontalTree* tree, QWidget* parent=nullptr);

        /**
         * @brief Constructor.
         * @param parent Parent widget.
         */
        HorizontalTreeNode(QWidget* parent=nullptr);

        /**
         * @brief Destructor.
         */
        ~HorizontalTreeNode();

        HorizontalTreeNode(const HorizontalTreeNode&)=delete;
        HorizontalTreeNode(HorizontalTreeNode&&)=delete;
        HorizontalTreeNode& operator=(const HorizontalTreeNode&)=delete;
        HorizontalTreeNode& operator=(HorizontalTreeNode&&)=delete;

        void setTree(HorizontalTree* tree);
        HorizontalTree* tree() const;

        void setPath(HorizontalTreeePath path);
        const HorizontalTreeePath& path() const;

        void setParentNode(HorizontalTreeNode* node);
        HorizontalTreeNode* parentNode() const;

        void loadNextNode(const HorizontalTreeePathElement& pathElement);
        HorizontalTreeNode* nextNode() const;

        void resetNextNode();

    private:

        std::unique_ptr<HorizontalTreeNode_p> pimpl;
};

class HorizontalTreeBranch_p;

class UISE_DESKTOP_EXPORT HorizontalTreeBranch : public HorizontalTreeNode
{
    Q_OBJECT

    public:

        /**
         * @brief Constructor.
         * @param tree The tree this node belongs to.
         * @param parent Parent widget.
         */
        HorizontalTreeBranch(HorizontalTree* tree, QWidget* parent=nullptr);

        /**
         * @brief Constructor.
         * @param parent Parent widget.
         */
        HorizontalTreeBranch(QWidget* parent=nullptr);

        /**
         * @brief Destructor.
         */
        ~HorizontalTreeBranch();

        HorizontalTreeBranch(const HorizontalTreeBranch&)=delete;
        HorizontalTreeBranch(HorizontalTreeBranch&&)=delete;
        HorizontalTreeBranch& operator=(const HorizontalTreeBranch&)=delete;
        HorizontalTreeBranch& operator=(HorizontalTreeBranch&&)=delete;

    private:

        std::unique_ptr<HorizontalTreeBranch_p> pimpl;
};

class HorizontalTreeNodeFactory;

class HorizontalTree_p;

class UISE_DESKTOP_EXPORT HorizontalTree : public QFrame
{
    Q_OBJECT

    public:

        /**
         * @brief Constructor.
         * @param parent Parent widget.
         */
        HorizontalTree(QWidget* parent=nullptr);

        /**
         * @brief Destructor.
         */
        ~HorizontalTree();

        HorizontalTree(const HorizontalTree&)=delete;
        HorizontalTree(HorizontalTree&&)=delete;
        HorizontalTree& operator=(const HorizontalTree&)=delete;
        HorizontalTree& operator=(HorizontalTree&&)=delete;

        void setNodeFactory(const HorizontalTreeNodeFactory* factory);
        const HorizontalTreeNodeFactory* nodeFactory() const;

        void openPath(const HorizontalTreeePath& path, int tabIndex=-1);
        void refreshPath(const HorizontalTreeePath& path, int tabIndex=-1);

        HorizontalTreeNode* rootNode(int tabIndex) const;

        int tabCount() const;
        void setCurrentTab(int tabIndex);
        void closeTab(int tabIndex);

    private:

        std::unique_ptr<HorizontalTree_p> pimpl;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_HORIZONTAL_TREE_HPP

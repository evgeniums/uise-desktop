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

class HorizontalTreePathElementConfig
{
    public:

        HorizontalTreePathElementConfig(bool expanded=true, int width=-1)
            : m_expanded(expanded),
              m_width(width)
        {}

        bool expanded() const noexcept
        {
            return m_expanded;
        }

        int width() const noexcept
        {
            return m_width;
        }

        void setExpanded(bool m_expanded) noexcept
        {
            m_expanded=m_expanded;
        }

        void setWidth(int width) noexcept
        {
            m_width=width;
        }

    private:

        bool m_expanded;
        int m_width;
};

class HorizontalTreePathElement
{
    public:

        HorizontalTreePathElement(
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

        const HorizontalTreePathElementConfig& config() const noexcept
        {
            return m_config;
        }

        void setConfig(HorizontalTreePathElementConfig config) noexcept
        {
            m_config=std::move(config);
        }

    private:

        std::string m_type;
        std::string m_id;
        std::string m_name;

        HorizontalTreePathElementConfig m_config;
};

class HorizontalTreePathConfig
{
    public:

        HorizontalTreePathConfig(int tabIndex=-1)
            : m_tabIndex(tabIndex)
        {}


        int tabIndex() const noexcept
        {
            return m_tabIndex;
        }

        void setTabIndex(int tabIndex) noexcept
        {
            m_tabIndex=tabIndex;
        }

    private:

        int m_tabIndex;
};

class HorizontalTreePath
{
    public:

        HorizontalTreePath(std::vector<HorizontalTreePathElement> elements={}) : m_elements(std::move(elements))
        {}

        const std::vector<HorizontalTreePathElement>& elements() const noexcept
        {
            return m_elements;
        }

        std::vector<HorizontalTreePathElement>& elements() noexcept
        {
            return m_elements;
        }

        void setElements(std::vector<HorizontalTreePathElement> elements)
        {
            m_elements=std::move(elements);
        }

    private:

        std::vector<HorizontalTreePathElement> m_elements;
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

        void setPath(HorizontalTreePath path);
        const HorizontalTreePath& path() const;

        void setParentNode(HorizontalTreeNode* node);
        HorizontalTreeNode* parentNode() const;

        bool isExpanded() const noexcept;

        QString id() const;
        QString name() const;
        QIcon icon() const;
        QString tooltip() const;

    public slots:

        void setExpanded(bool enable);

    signals:

        void nameUpdated(const QString&);
        void tooltipUpdated(const QString&);
        void iconUpdated(const QIcon&);

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

        void loadNextNode(const HorizontalTreePathElement& pathElement);
        HorizontalTreeNode* nextNode() const;
        void closeNextNode();
        void refreshNextNode();

    private:

        std::unique_ptr<HorizontalTreeBranch_p> pimpl;
};

class UISE_DESKTOP_EXPORT HorizontalTreeNodeFactory
{
    public:

        HorizontalTreeNode* makeNode(HorizontalTreePath path) const;
};

class HorizontalTreeTab_p;

class UISE_DESKTOP_EXPORT HorizontalTreeTab : public QFrame
{
    Q_OBJECT

    public:

        /**
         * @brief Constructor.
         * @param parent Parent widget.
         */
        HorizontalTreeTab(HorizontalTree* tree=nullptr, QWidget* parent=nullptr);

        /**
         * @brief Destructor.
         */
        ~HorizontalTreeTab();

        HorizontalTreeTab(const HorizontalTreeTab&)=delete;
        HorizontalTreeTab(HorizontalTreeTab&&)=delete;
        HorizontalTreeTab& operator=(const HorizontalTreeTab&)=delete;
        HorizontalTreeTab& operator=(HorizontalTreeTab&&)=delete;

        void openPath(HorizontalTreePath path);

        HorizontalTreeNode* node() const;

    signals:

        void nameUpdated(const QString&);
        void tooltipUpdated(const QString&);
        void iconUpdated(const QIcon&);

    private:

        friend class HorizontalTree;

        std::unique_ptr<HorizontalTreeTab_p> pimpl;
};

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

        void setNodeFactory(const HorizontalTreeNodeFactory* factory) noexcept;
        const HorizontalTreeNodeFactory* nodeFactory() const noexcept;

        void openPath(HorizontalTreePath path, int tabIndex=0);

        void loadPaths(const std::vector<HorizontalTreePath>& paths);
        std::vector<HorizontalTreePath> paths() const;

        int tabCount() const;
        int currentTabIndex() const;
        void setCurrentTab(int tabIndex);
        void closeTab(int tabIndex);

        HorizontalTreeTab* tab(int tabIndex=0) const;

        void closeAllTabs();

    private:

        std::unique_ptr<HorizontalTree_p> pimpl;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_HORIZONTAL_TREE_HPP

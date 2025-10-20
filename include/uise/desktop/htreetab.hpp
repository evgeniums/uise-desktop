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

/** @file uise/desktop/htreetab.hpp
*
*  Declares horizontal tree tab widget.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_HTREE_TAB_HPP
#define UISE_DESKTOP_HTREE_TAB_HPP

#include <memory>

#include <QFrame>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/framewithrefresh.hpp>

#include <uise/desktop/htreepath.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class NavigationBar;

class HTree;
class HTreeNode;

class HTreeTab_p;

class UISE_DESKTOP_EXPORT HTreeTab : public QFrame
{
    Q_OBJECT

    public:

        /**
         * @brief Constructor.
         * @param parent Parent widget.
         */
        HTreeTab(HTree* tree=nullptr, QWidget* parent=nullptr);

        /**
         * @brief Destructor.
         */
        ~HTreeTab();

        HTreeTab(const HTreeTab&)=delete;
        HTreeTab(HTreeTab&&)=delete;
        HTreeTab& operator=(const HTreeTab&)=delete;
        HTreeTab& operator=(HTreeTab&&)=delete;

        bool openPath(HTreePath path);

        HTreeNode* node() const;
        HTreeNode* node(const HTreePath& path, bool exact=true) const;

        HTreePath path() const;

        void appendNode(HTreeNode* node);
        void closeNode(HTreeNode* node);

        void setTree(HTree* tree);
        HTree* tree() const;

        NavigationBar* navbar() const;

        void truncate(int index);

        void scrollToNode(HTreeNode* node);

        bool isSingleCollapsePlaceholder() const noexcept;

    public slots:

        void activate();

    signals:

        void nameUpdated(const QString&);
        void tooltipUpdated(const QString&);
        void iconUpdated(const QIcon&);

        void nodeUpdated(const UISE_DESKTOP_NAMESPACE::HTreePath&);

    private slots:

        void nodeCloseHovered(UISE_DESKTOP_NAMESPACE::HTreeNode*, bool);

    private:

        void adjustWidthsAndPositions();

        friend class HTree;
        friend class HTreeNode;
        friend class HTreeTab_p;

        std::unique_ptr<HTreeTab_p> pimpl;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_HTREE_TAB_HPP

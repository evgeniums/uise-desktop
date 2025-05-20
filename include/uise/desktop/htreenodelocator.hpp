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

/** @file uise/desktop/htreenodelocator.hpp
*
*  Declares horizontal tree nodes locator.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_HTREE_NODE_LOCATOR_HPP
#define UISE_DESKTOP_HTREE_NODE_LOCATOR_HPP

#include <map>

#include <QPointer>
#include <QSignalMapper>

#include <uise/desktop/uisedesktop.hpp>

#include <uise/desktop/htreepath.hpp>
#include <uise/desktop/htreenodefactory.hpp>
#include <uise/desktop/htreenode.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class UISE_DESKTOP_EXPORT HTreeNodeLocator : public QObject
{
    Q_OBJECT

    public:

        HTreeNodeLocator(std::shared_ptr<HTreeNodeFactory> factory, QObject* parent=nullptr);
        ~HTreeNodeLocator();

        HTreeNodeLocator(HTreeNodeLocator&)=delete;
        HTreeNodeLocator(HTreeNodeLocator&&)=delete;
        HTreeNodeLocator& operator= (HTreeNodeLocator&)=delete;
        HTreeNodeLocator& operator= (HTreeNodeLocator&&)=delete;

        void setFactory(std::shared_ptr<HTreeNodeFactory> factory)
        {
            m_factory=std::move(factory);
        }

        std::shared_ptr<HTreeNodeFactory> factory() const noexcept
        {
            return m_factory;
        }

        HTreeNode* findUniqueNode(const std::string& id) const;

        std::pair<HTreeNode*,bool> findOrCreateNode(
            const HTreePathElement& pathElement,
            HTreeNode* parentNode=nullptr,
            HTreeTab* treeTab=nullptr
        );

    private:

        std::map<std::string,QPointer<HTreeNode>> m_uniqueNodes;
        std::shared_ptr<HTreeNodeFactory> m_factory;
        QSignalMapper m_nodeDestroyedMapper;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_HTREE_NODE_LOCATOR_HPP

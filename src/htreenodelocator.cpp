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

/** @file uise/desktop/htreenodelocator.cpp
*
*  Defines HTreeNodeLocator.
*
*/

/****************************************************************************/

#include <uise/desktop/htreenode.hpp>
#include <uise/desktop/htreenodelocator.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

HTreeNodeLocator::HTreeNodeLocator(std::shared_ptr<HTreeNodeFactory> factory, QObject* parent)
    : QObject(parent),
      m_factory(std::move(factory))
{
    connect(&m_nodeDestroyedMapper,&QSignalMapper::mappedString,this,
        [this](const QString& str)
        {
            m_uniqueNodes.erase(str.toStdString());
        }
    );
}

//--------------------------------------------------------------------------

HTreeNodeLocator::~HTreeNodeLocator()
{
    m_nodeDestroyedMapper.blockSignals(true);
}

//--------------------------------------------------------------------------

HTreeNode* HTreeNodeLocator::findUniqueNode(const std::string& id) const
{
    auto it=m_uniqueNodes.find(id);
    if (it!=m_uniqueNodes.end())
    {
        return it->second;
    }
    return nullptr;
}

//--------------------------------------------------------------------------

std::pair<HTreeNode*,bool> HTreeNodeLocator::findOrCreateNode(
        const HTreePathElement& pathElement,
        HTreeNode* parentNode,
        HTreeTab* treeTab
    )
{
    auto node=findUniqueNode(pathElement.id());
    if (node!=nullptr)
    {
        return std::make_pair(node,false);
    }

    node=m_factory->makeNode(pathElement,parentNode,treeTab);
    if (node!=nullptr)
    {
        if (node->isUnique())
        {
            m_uniqueNodes.emplace(pathElement.id(),node);
        }
        return std::make_pair(node,true);
    }
    return std::make_pair(nullptr,false);
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END

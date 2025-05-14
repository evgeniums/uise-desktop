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

/** @file demo/htree/dirlist.hpp
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_TEST_HTREE_DIRLIST_HPP
#define UISE_DESKTOP_TEST_HTREE_DIRLIST_HPP

#include <boost/algorithm/string.hpp>

#include <filesystem>

#include <QApplication>
#include <QMainWindow>
#include <QLabel>
#include <QTimer>
#include <QPushButton>
#include <QTextBrowser>
#include <QSpinBox>
#include <QComboBox>
#include <QLineEdit>
#include <QFile>

#include <uise/desktop/flyweightlistview.hpp>
#include <uise/desktop/flyweightlistview.ipp>
#include <uise/desktop/flyweightlistitem.hpp>

#include <uise/desktop/htreelistitem.hpp>
#include <uise/desktop/htreelist.hpp>
#include <uise/desktop/htreetab.hpp>
#include <uise/desktop/htreenodefactory.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

//--------------------------------------------------------------------------

class DirListItem : public HTreeStansardListItem
{
    Q_OBJECT

    public:

        constexpr static const char* Folder="folder";
        constexpr static const char* File="file";

        DirListItem(
                const std::filesystem::directory_entry& entry,
                QWidget* parent=nullptr
            ) : HTreeStansardListItem(entry.is_directory()?Folder:File, parent),
                entry(entry)
        {
            auto drawIcon=[this](bool selected)
            {
                const auto& entry=this->entry;
                QString iconName;

                // qDebug() << "name=" << name().c_str() << ", id=" << id().c_str();

                if (!entry.is_directory())
                {
                    QString extIconsRoot{":/uise/icons/ext"};
                    std::string ext=entry.path().extension().string();
                    if (!ext.empty())
                    {
                        ext=ext.substr(1);
                    }
                    auto iconNameTemplate=QString("%1/%2/%3.svg");
                    iconName=iconNameTemplate.arg(extIconsRoot,QString("classic"),QString(ext.c_str()));
                    if (!QFile::exists(iconName))
                    {
                        // qDebug() << "Icon does not exists ext="<<ext.c_str() << ", iconName=" << iconName;

                        iconName=iconNameTemplate.arg(extIconsRoot,QString("fallback"),QString(ext.c_str()));
                        if (!QFile::exists(iconName))
                        {
                            // qDebug() << "Fallback not exists, using blank: ext="<<ext.c_str() << ", iconName=" << iconName;

                            ext="blank";
                            iconName=iconNameTemplate.arg(extIconsRoot,QString("classic"),QString(ext.c_str()));
                        }
                    }
                }
                else
                {
                    QString folderIconsRoot{":/uise/icons/folder"};
                    if (entry.is_symlink())
                    {
                        if (selected)
                        {
                            iconName=QString("%1/folder-plain-open.svg").arg(folderIconsRoot);
                        }
                        else
                        {
                            iconName=QString("%1/folder-plain.svg").arg(folderIconsRoot);
                        }
                    }
                    else
                    {
                        if (selected)
                        {
                            iconName=QString("%1/folder-open.svg").arg(folderIconsRoot);
                        }
                        else
                        {
                            iconName=QString("%1/folder.svg").arg(folderIconsRoot);
                        }
                    }

                    // qDebug() << "folder, iconName=" << iconName;
                }

                QPixmap pix=QPixmap(iconName).scaledToHeight(24);
                setPixmap(pix);
            };
            drawIcon(false);

            setText(QString::fromStdString(name()));
            setToolTip(QString::fromStdString(id()));
            setObjectName("DirListItem");

            connect(
                this,
                &DirListItem::selectionChanged,
                [this,drawIcon](bool selected)
                {
                    if (!this->entry.is_directory())
                    {
                        return;
                    }
                    drawIcon(selected);
                }
            );
        }

        std::string name() const noexcept
        {
            return entry.path().filename().string();
        }

        std::string sortValue() const noexcept
        {
            return name();
        }

        std::string id() const
        {
            return std::filesystem::absolute(entry.path()).string();
        }

    private:

        std::filesystem::directory_entry entry;
};

struct DirListItemTraits : public FlyweightListItemTraits<DirListItem*,HTreeListItem,std::string,std::string>
{
    static auto sortValue(const DirListItem* item) noexcept
    {
        return item->sortValue();
    }

    static HTreeListItem* widget(DirListItem* item) noexcept
    {
        return item;
    }

    static auto id(const DirListItem* item)
    {
        return item->id();
    }
};

using DirItemWrapper=FlyweightListItem<DirListItemTraits>;

class DirListView : public FlyweightListView<DirItemWrapper>
{
    Q_OBJECT

    public:

        DirListView(QWidget* parent=nullptr) : FlyweightListView<DirItemWrapper>(parent)
        {
            setObjectName("DirListView");
        }
};

class DirList : public HTreeListView<DirItemWrapper>
{
    Q_OBJECT

    public:

        DirList(QWidget* parent=nullptr) : HTreeListView<DirItemWrapper>(parent)
        {
            auto l=Layout::vertical(this);
            auto listView=new DirListView(this);
            l->addWidget(listView);

            listView->setFlyweightEnabled(false);
            listView->setStickMode(Direction::HOME);
            listView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
            listView->setWheelHorizontalScrollEnabled(false);
            setListView(listView);

            setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);
            setObjectName("DirList");
        }

    protected:

        void doReload() override
        {
            try {
                std::vector<DirItemWrapper> items;

                std::filesystem::path path;
                for (const auto& element: hTreeList()->path().elements())
                {
                    path.append(element.name());
                }
                if (path.empty())
                {
                    path=std::filesystem::current_path().root_path();
                }

                std::vector<std::filesystem::directory_entry> entries;
                for (const auto& entry : std::filesystem::directory_iterator(path))
                {
                    entries.push_back(entry);
                }

                std::sort(entries.begin(),entries.end(),[](const auto& l, const auto& r)
                {
                    if (!l.is_directory() && r.is_directory())
                    {
                        return false;
                    }
                    if (l.is_directory() && !r.is_directory())
                    {
                        return true;
                    }
                    return l<r;
                });

                for (const auto& entry : entries)
                {
                    auto item=DirItemWrapper(new DirListItem(entry));

                    std::string type=DirListItem::File;
                    if (entry.is_directory())
                    {
                        type=DirListItem::Folder;
                    }
                    HTreePathElement pathEl{type,std::filesystem::absolute(entry.path()).string(),entry.path().filename().string()};
                    item.item()->setPathElement(std::move(pathEl));

                    if (entry.is_symlink())
                    {
                        std::error_code ec;
                        auto residentPath=std::filesystem::read_symlink(entry.path(),ec);
                        if (!ec)
                        {
                            residentPath=std::filesystem::absolute(residentPath);
                            HTreePath p;
                            for (auto it=residentPath.begin();it!=residentPath.end();++it)
                            {
                                auto name=it->string();
                                std::string id=p.id();
                                if (!id.empty() && !boost::algorithm::ends_with(id,"/"))
                                {
                                    id+="/";
                                }
                                id+=name;
                                HTreePathElement pathEl{entry.is_directory()?DirListItem::Folder:DirListItem::File,id,name};
                                p.append(std::move(pathEl));
                            }
                            item.item()->setResidentPath(std::move(p));
                            item.item()->setToolTip(QString::fromStdString(item.item()->residentPath().id()));
                        }
                    }

                    items.push_back(item);
                }
                listView()->loadItems(items);
            }
            catch (...)
            {
            }
        }
};

class FolderNodeBuilder : public HTreeNodeBuilder
{
    public:

        HTreeNode* makeNode(const HTreePathElement& pathElement, HTreeNode* parentNode=nullptr, HTreeTab* treeTab=nullptr) const override
        {
            HTreePath path;
            if (parentNode!=nullptr)
            {
                path=HTreePath{parentNode->path(),pathElement};
            }
            auto dirList=new DirList();

            auto node=new HTreeList(treeTab);
            node->setNodeTooltip(QString::fromStdString(pathElement.id()));
            node->setView(dirList);
            node->setMinimumWidth(200);
            return node;
        }
};

UISE_DESKTOP_NAMESPACE_END

//--------------------------------------------------------------------------
#endif // UISE_DESKTOP_TEST_HTREE_DIRLIST_HPP

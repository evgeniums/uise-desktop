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

/** @file uise/desktop/htreestandardlistitem.hpp
*
*  Declares standard list item of horizontal tree branch.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_HTREE_STANDARD_LIST_ITEM_HPP
#define UISE_DESKTOP_HTREE_STANDARD_LIST_ITEM_HPP

#include <memory>
#include <QFrame>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/style.hpp>
#include <uise/desktop/flyweightlistview.hpp>
#include <uise/desktop/flyweightlistview.ipp>
#include <uise/desktop/flyweightlistitem.hpp>

#include <uise/desktop/htreelist.hpp>
#include <uise/desktop/htreelistitem.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class HTreeStandardListItem_p;

class UISE_DESKTOP_EXPORT HTreeStandardListItem : public HTreeListItem
{
    Q_OBJECT

    public:

        HTreeStandardListItem(const QString& text, std::shared_ptr<SvgIcon> icon={}, QWidget* parent=nullptr) :
            HTreeStandardListItem(HTreePathElement{},text,std::move(icon),parent)
        {}

        HTreeStandardListItem(HTreePathElement el, const QString& text, std::shared_ptr<SvgIcon> icon={}, QWidget* parent=nullptr);

        HTreeStandardListItem(QWidget* parent=nullptr) :
            HTreeStandardListItem({},{},{},parent)
        {}

        /**
         * @brief Destructor.
         */
        ~HTreeStandardListItem();

        HTreeStandardListItem(const HTreeStandardListItem&)=delete;
        HTreeStandardListItem(HTreeStandardListItem&&)=delete;
        HTreeStandardListItem& operator=(const HTreeStandardListItem&)=delete;
        HTreeStandardListItem& operator=(HTreeStandardListItem&&)=delete;

        QString text() const;
        QPixmap pixmap() const;

        void setIcon(std::shared_ptr<SvgIcon> icon);
        std::shared_ptr<SvgIcon> icon() const;

        void setTextElideMode(Qt::TextElideMode mode);
        Qt::TextElideMode textElideMode() const;

        void setPropagateIconClick(bool enable);
        bool isPropagateIconClick() const;

        std::string name() const
        {
            return pathElement().name();
        }

        std::string sortValue() const noexcept
        {
            return name();
        }

        std::string type() const
        {
            return pathElement().type();
        }

        std::string id() const
        {
            return pathElement().id();
        }

        static QString iconName(const QString& name)
        {
            return QString("HTreeStandardListItem::%1").arg(name);
        }

    signals:

        void iconClicked();

    public slots:

        void setText(const QString& text);
        void setPixmap(const QPixmap& pixmap);

    protected:

        void doSetHovered(bool enable) override;
        void doSetSelected(bool enable) override;

    private:

        std::unique_ptr<HTreeStandardListItem_p> pimpl;
};

struct HTreeStandardListItemTraits : public FlyweightListItemTraits<HTreeStandardListItem*,HTreeListItem,std::string,std::string>
{
    static auto sortValue(const HTreeStandardListItem* item) noexcept
    {
        return item->sortValue();
    }

    static HTreeListItem* widget(HTreeStandardListItem* item) noexcept
    {
        return item;
    }

    static auto id(const HTreeStandardListItem* item)
    {
        return item->id();
    }
};

using HTreeStansardListIemWrapper=FlyweightListItem<HTreeStandardListItemTraits>;

class UISE_DESKTOP_EXPORT HTreeStandardListItemView : public FlyweightListView<HTreeStansardListIemWrapper>
{
    Q_OBJECT

    public:

        HTreeStandardListItemView(QWidget* parent=nullptr);
};

inline void createHTreeStandardListItem(std::vector<HTreeStansardListIemWrapper>& items,
                                    HTreePathElement el,
                                    const QString& icon,
                                    QWidget* parent=nullptr)
{
    auto item=new HTreeStandardListItem(el,QString::fromStdString(el.name()),Style::instance().svgIconLocator().icon(icon,parent));
    items.emplace_back(item);
}

class UISE_DESKTOP_EXPORT HTreeStandardListView : public HTreeListFlyweightView<HTreeStansardListIemWrapper>
{
    Q_OBJECT

    public:

        constexpr static const int DefaultMinimumWidth=300;

        HTreeStandardListView(QWidget* parent=nullptr, int minimumWidth=DefaultMinimumWidth);

        HTreeStandardListView(int minimumWidth) : HTreeStandardListView(nullptr,minimumWidth)
        {}
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_HTREE_STANDARD_LIST_ITEM_HPP

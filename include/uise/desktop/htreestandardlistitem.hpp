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

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/htreeflyweightlistitem.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class PushButton;
class ElidedLabel;

class HTreeStandardListItem_p;

class UISE_DESKTOP_EXPORT HTreeStandardListItem : public HTreeFlyweightListItem<>
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

        QString text() const;
        QPixmap pixmap() const;

        void setIcon(std::shared_ptr<SvgIcon> icon);
        std::shared_ptr<SvgIcon> icon() const;

        void setTextElideMode(Qt::TextElideMode mode);
        Qt::TextElideMode textElideMode() const;

        void setPropagateIconClick(bool enable);
        bool isPropagateIconClick() const;

        static QString iconName(const QString& name)
        {
            return QString("HTreeStandardListItem::%1").arg(name);
        }

        void setExpandVisible(bool enable);
        bool isExpandVisible() const;

    signals:

        void iconClicked();

    public slots:

        void setText(const QString& text);
        void setPixmap(const QPixmap& pixmap);

    protected:

        void doSetHovered(bool enable) override;
        void doSetSelected(bool enable) override;

    private:

        PushButton* m_icon=nullptr;
        ElidedLabel* m_text=nullptr;
        PushButton* m_expand=nullptr;
        bool m_propagateIconClick=true;
};

using HTreeStandardListItemTraits=HTreeFlyweightListItemTraits<HTreeStandardListItem>;
using HTreeStansardListIemWrapper=HTreeFlyweightListItemWrapper<HTreeStandardListItem>;

class UISE_DESKTOP_EXPORT HTreeStandardListItemView : public HTreeFlyweightListItemView<HTreeStandardListItem>
{
    Q_OBJECT

    public:

        using HTreeFlyweightListItemView<HTreeStandardListItem>::HTreeFlyweightListItemView;
};

inline void createHTreeStandardListItem(std::vector<HTreeStansardListIemWrapper>& items,
                                    HTreePathElement el,
                                    const QString& icon,
                                    bool expandVisible=false,
                                    QWidget* parent=nullptr)
{
    auto item=new HTreeStandardListItem(el,QString::fromStdString(el.name()),Style::instance().svgIconLocator().icon(icon,parent));
    item->setExpandVisible(expandVisible);
    items.emplace_back(item);
}

class UISE_DESKTOP_EXPORT HTreeStandardListView : public HTreeFlyweightListView<HTreeStandardListItem>
{
    Q_OBJECT

    public:

        using HTreeFlyweightListView<HTreeStandardListItem>::HTreeFlyweightListView;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_HTREE_STANDARD_LIST_ITEM_HPP

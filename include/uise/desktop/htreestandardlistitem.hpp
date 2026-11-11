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

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

class PushButton;
class ElidedLabel;
class CountBadge;

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

        /**
         * @brief Make the icon its own button (with ripple and an action on release) instead of part of the row.
         *
         * Off by default: the icon ignores the mouse, so a press on it is a press on the row
         * (immediate click, no ripple). setPropagateIconClick() and iconClicked() only matter when this is on.
         */
        void setIconClickable(bool enable);
        bool isIconClickable() const;

        /** Only used when the icon is clickable: true makes an icon click click the whole item, false emits iconClicked(). */
        void setPropagateIconClick(bool enable);
        bool isPropagateIconClick() const;

        static QString iconName(const QString& name)
        {
            return QString("HTreeStandardListItem::%1").arg(name);
        }

        void setExpandVisible(bool enable);
        bool isExpandVisible() const;

        /**
         * @brief Show a counter badge (e.g. an unread count) right after the item's text.
         *
         * Empty \p text hides the badge. The badge is created lazily on the first non-empty text, so
         * items that never use it keep their layout unchanged. \p muted and \p mentioned select the
         * badge's QSS colour variants, see CountBadge::setMuted()/setMentioned(). Mirrors
         * HTreeNode::setTitleBadge().
         */
        void setBadge(const QString& text, bool muted=false, bool mentioned=false);
        QString badgeText() const;

    signals:

        void iconClicked();
        void clicked();

    public slots:

        void setText(const QString& text);
        void setPixmap(const QPixmap& pixmap);

    protected:

        void doSetHovered(bool enable) override;
        void doSetSelected(bool enable) override;

        void doClick() override;

    private:

        PushButton* m_icon=nullptr;
        ElidedLabel* m_text=nullptr;
        PushButton* m_expand=nullptr;
        CountBadge* m_badge=nullptr;
        bool m_propagateIconClick=true;
        bool m_iconClickable=false;
};

using HTreeStandardListItemTraits=HTreeFlyweightListItemTraits<HTreeStandardListItem>;
using HTreeStansardListIemWrapper=HTreeFlyweightListItemWrapper<HTreeStandardListItem>;

class UISE_DESKTOP_EXPORT HTreeStandardListItemView : public HTreeFlyweightListItemView<HTreeStandardListItem>
{
    Q_OBJECT

    public:

        using HTreeFlyweightListItemView<HTreeStandardListItem>::HTreeFlyweightListItemView;
};

inline auto* createHTreeStandardListItem(std::vector<HTreeStansardListIemWrapper>& items,
                                    HTreePathElement el,
                                    const QString& icon,
                                    bool expandVisible=false,
                                    QWidget* parent=nullptr)
{
    auto item=new HTreeStandardListItem(el,QString::fromStdString(el.name()),Style::instance().svgIconLocator().icon(icon,parent));
    item->setExpandVisible(expandVisible);
    items.push_back(item);
    return item;
}

class UISE_DESKTOP_EXPORT HTreeStandardListView : public HTreeFlyweightListView<HTreeStandardListItem>
{
    Q_OBJECT

    public:

        using HTreeFlyweightListView<HTreeStandardListItem>::HTreeFlyweightListView;
};

}

#endif // UISE_DESKTOP_HTREE_STANDARD_LIST_ITEM_HPP

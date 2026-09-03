/**
@copyright Evgeny Sidorov 2026

This software is dual-licensed. Choose the appropriate license for your project.

1. The GNU GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-GPLv3.md](LICENSE-GPLv3.md) or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. The GNU LESSER GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-LGPLv3.md](LICENSE-LGPLv3.md) or copy at https://www.gnu.org/licenses/lgpl-3.0.txt).

You may select, at your option, one of the above-listed licenses.

*/

/****************************************************************************/

/** @file uise/desktop/dropdownmenu.hpp
*
*  Declares MenuItem and DropdownMenu.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_DROPDOWNMENU_HPP
#define UISE_DESKTOP_DROPDOWNMENU_HPP

#include <memory>
#include <vector>

#include <QString>
#include <QVariant>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/svgicon.hpp>
#include <uise/desktop/dropdownframe.hpp>

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

class IconTextButton;

/**
 * @brief Plain descriptor for one row of a DropdownMenu.
 *
 * Not a widget: DropdownMenu rebuilds actual IconTextButton rows from a list of these on every
 * opening (see DropdownMenu::fillContent()), so mutating a menu while it is closed never has to
 * touch any live widget.
 */
class UISE_DESKTOP_EXPORT MenuItem
{
    public:

        /**
         * @brief Construct a separator. Use separator() instead for readability.
         */
        MenuItem()=default;

        /**
         * @brief Construct a plain clickable item.
         */
        MenuItem(int id, QString text, std::shared_ptr<SvgIcon> icon={})
            : id(id),
              text(std::move(text)),
              icon(std::move(icon))
        {}

        static MenuItem separator()
        {
            MenuItem item;
            item.isSeparator=true;
            return item;
        }

        static MenuItem checkable(int id, QString text, bool checked=false, std::shared_ptr<SvgIcon> icon={})
        {
            MenuItem item(id,std::move(text),std::move(icon));
            item.isCheckable=true;
            item.isChecked=checked;
            return item;
        }

        /**
         * @brief Construct a section-header item.
         *
         * Purely presentational: a section is a normal clickable (or, with isEnabled=false,
         * inert) row that carries the dynamic property section="true" for QSS. Rows that
         * follow a section are automatically marked subsection="true" by
         * DropdownMenu::fillContent(), until the next section/separator.
         */
        static MenuItem section(int id, QString text, std::shared_ptr<SvgIcon> icon={})
        {
            MenuItem item(id,std::move(text),std::move(icon));
            item.isSection=true;
            return item;
        }

        /**
         * @brief Construct an item that opens a nested submenu instead of activating.
         *
         * A submenu item is never checkable: its trailing chevron occupies the same slot the
         * checkmark uses (see IconTextButton::setTrailingSvgIcon()), and clicking it opens its
         * submenu rather than emitting DropdownMenu::itemTriggered(). Ids must be unique across
         * the WHOLE menu tree, not just within one level -- DropdownMenu's per-id mutators
         * (setItemChecked(), setItemEnabled(), etc.) search it recursively.
         */
        static MenuItem submenu(int id, QString text, std::vector<MenuItem> children, std::shared_ptr<SvgIcon> icon={})
        {
            MenuItem item(id,std::move(text),std::move(icon));
            item.children=std::move(children);
            return item;
        }

        bool hasSubmenu() const noexcept
        {
            return !children.empty();
        }

        int id=-1;
        QString text;
        std::shared_ptr<SvgIcon> icon;
        bool isCheckable=false;
        bool isChecked=false;
        bool isEnabled=true;
        bool isSeparator=false;
        bool isVisible=true;
        bool isSection=false;

        /**
         * @brief Exclusive (radio-like) group this item belongs to, or -1 for none.
         *
         * Checking an item with group>=0 unchecks every other currently-checked item that
         * shares the same group value, within this menu level -- a submenu's items are their
         * own separate group namespace from their parent's.
         */
        int group=-1;

        /**
         * @brief objectName applied to the item's row widget, for QSS targeting.
         */
        QString name;

        /**
         * @brief Free-form payload the owner can use to stash extra context per item.
         */
        QVariant data;

        /**
         * @brief Child items shown in a nested submenu, or empty for an ordinary item.
         *
         * Prefer the submenu() factory over setting this directly.
         */
        std::vector<MenuItem> children;
};

class DropdownMenu_p;

/**
 * @brief Generic anchored, animated drop-down menu built on DropdownFrame.
 *
 * Rows are IconTextButton instances, rebuilt from items() every time the menu opens (see
 * DropdownFrame::fillContent()), so changing items() while the menu is closed just works.
 * Per-row mutators (setItemChecked()/setItemEnabled()/setItemText()/setItemIcon()) additionally
 * update the live row widget if the menu happens to be open when called.
 */
class UISE_DESKTOP_EXPORT DropdownMenu : public DropdownFrame
{
    Q_OBJECT

    Q_PROPERTY(int submenuHoverDelayMs READ submenuHoverDelayMs WRITE setSubmenuHoverDelayMs)
    Q_PROPERTY(int submenuCloseDelayMs READ submenuCloseDelayMs WRITE setSubmenuCloseDelayMs)

    public:

        constexpr static const int DefaultSubmenuHoverDelayMs=200;
        constexpr static const int DefaultSubmenuCloseDelayMs=250;

        DropdownMenu(QWidget* parent=nullptr);

        ~DropdownMenu();

        DropdownMenu(const DropdownMenu&)=delete;
        DropdownMenu(DropdownMenu&&)=delete;
        DropdownMenu& operator=(const DropdownMenu&)=delete;
        DropdownMenu& operator=(DropdownMenu&&)=delete;

        void setItems(std::vector<MenuItem> items);
        void addItem(MenuItem item);
        void addSeparator();
        void clear();
        const std::vector<MenuItem>& items() const;

        /**
         * @brief Set/get whether a checkable item is checked.
         *
         * Searches the WHOLE item tree, not just this level, and forwards to the corresponding
         * live submenu (see submenuFor()) if one has been created -- ids must be unique across
         * the whole tree.
         */
        void setItemChecked(int id, bool checked);
        bool isItemChecked(int id) const;

        void setItemEnabled(int id, bool enable);

        /**
         * @brief Show/hide an item.
         * @param id Item id.
         * @param visible New visibility.
         *
         * If the item's row is currently rendered (the menu is open and the item was visible
         * at the last opening), this simply shows/hides that row widget. An item that starts
         * invisible and is made visible while the menu is already open only gains a row on the
         * NEXT opening -- inserting a brand-new row into an already-measured, open menu is not
         * supported. Searches the whole item tree; see setItemChecked().
         */
        void setItemVisible(int id, bool visible);

        void setItemText(int id, const QString& text);
        void setItemIcon(int id, std::shared_ptr<SvgIcon> icon);

        /**
         * @brief Get the live row widget for an item.
         * @return Operation result, valid only while the item's OWN menu level is open (and the
         *  item was visible at the last opening); nullptr otherwise. Searches the whole item
         *  tree; see setItemChecked().
         */
        IconTextButton* itemButton(int id) const;

        /**
         * @brief Close the menu when a checkable item is toggled, same as a clickable item.
         * @param enable Default false: checkable items stay open so multiple can be toggled.
         *
         * Propagated to every submenu created from this point on (see ensureSubmenu()).
         */
        void setCloseOnCheckableActivation(bool enable) noexcept;

        bool isCloseOnCheckableActivation() const noexcept;

        /**
         * @brief Wire a trigger widget to open/close this menu.
         * @param trigger Widget whose click toggles the menu.
         *
         * Sets triggerWidget(trigger). If trigger is an IconTextButton, it is made checkable
         * and kept checked for as long as the menu is open (mirroring FastSwitchButton's own
         * main-button handling). Any other widget exposing a clicked() signal also toggles the
         * menu, but without checked-state syncing.
         */
        void attachTo(QWidget* trigger);

        /**
         * @brief Delay before hovering a submenu row opens its submenu.
         */
        void setSubmenuHoverDelayMs(int ms) noexcept;
        int submenuHoverDelayMs() const noexcept;

        /**
         * @brief Grace delay before hovering an ordinary row closes a currently-open submenu.
         *
         * Kept nonzero so a pointer travelling diagonally from the row that opened a submenu
         * into that submenu, which very often clips the row below on the way, does not close
         * the submenu it is heading towards.
         */
        void setSubmenuCloseDelayMs(int ms) noexcept;
        int submenuCloseDelayMs() const noexcept;

        /**
         * @brief Get the live child menu for a submenu item.
         * @return Operation result, or nullptr if that item has no submenu or has never been
         *  opened -- child menus are created lazily on first hover/click (see ensureSubmenu()).
         */
        DropdownMenu* submenuFor(int id) const;

    signals:

        void itemTriggered(int id);
        void itemToggled(int id, bool checked);

        /**
         * @brief A submenu is about to be filled and shown.
         * @param id Id of the submenu item being opened.
         *
         * Last chance to edit that item's children (via items()/setItems() reaching into the
         * descriptor tree) before the submenu measures itself.
         */
        void submenuAboutToShow(int id);

    protected:

        void fillContent() override;
        void clearContent() override;
        void enterEvent(QEnterEvent* event) override;

    private:

        void onItemToggled(int id, bool checked);
        void onRowHovered(int id, bool hovered);
        void onSubmenuRowClicked(int id);
        void openSubmenu(int id);
        void closeSubmenu(bool immediate=false);
        void onSubmenuClosed(int id);
        DropdownMenu* ensureSubmenu(int id);

    private slots:

        void onTriggerClicked();

    private:

        std::unique_ptr<DropdownMenu_p> pimpl;
};

}

#endif // UISE_DESKTOP_DROPDOWNMENU_HPP

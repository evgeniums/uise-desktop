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

        /**
         * @brief Construct an item that opens a host-provided DropdownFrame instead of a child menu.
         *
         * For content a menu row cannot express (a list with avatars, a form). The row looks and
         * behaves like any submenu row -- chevron, hover-to-open, click-to-open -- but what opens
         * is the frame the host registered with DropdownMenu::setSubmenuFrame(), which is asked
         * for in DropdownMenu::submenuAboutToShow(id). A frame submenu with no frame registered
         * by then simply does not open. The row's text, enabled and visible state are ordinary
         * (setItemText() etc.), so a host can update it while the menu is open.
         */
        static MenuItem frameSubmenu(int id, QString text, std::shared_ptr<SvgIcon> icon={})
        {
            MenuItem item(id,std::move(text),std::move(icon));
            item.isFrameSubmenu=true;
            return item;
        }

        bool hasSubmenu() const noexcept
        {
            return isFrameSubmenu || !children.empty();
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
         * @brief The row opens a host-provided frame (see frameSubmenu()); children stays empty.
         */
        bool isFrameSubmenu=false;

        /**
         * @brief The row is informational: it looks like any enabled row, but does nothing.
         *
         * No hover highlight, no press ripple, no pointing cursor, and a click on it neither emits
         * DropdownMenu::itemTriggered() nor closes the menu -- the row is transparent to the mouse, so the
         * events fall through to the menu frame. Unlike isEnabled=false it is not greyed out, and its text
         * can still be changed with DropdownMenu::setItemText(). (A disabled row is for something that is
         * not available; this is for something that is only to be read.) Keyboard activation does not exist
         * for menu rows, so there is nothing else to switch off.
         */
        bool isInert=false;

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

        /**
         * @brief Secondary label shown at the right edge of the row, e.g. the remaining time of a mute.
         *
         * Empty for no label. It sits after the trailing-icon slot, so on a checkable row it comes
         * to the right of the checkmark. Not meant for submenu rows (the chevron already ends them).
         * Styled by `QLabel#hint` inside the row (dropdownmenu.qss); the row carries the dynamic
         * property hint="true" while it has one. Change it on an open menu with DropdownMenu::setItemHint().
         */
        QString hint;
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
         * @brief Set the right-aligned secondary label of an item (see MenuItem::hint).
         *
         * Updates the live row too if the menu is open; empty text hides the label. Searches the
         * whole item tree; see setItemChecked().
         */
        void setItemHint(int id, const QString& hint);

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
         * main-button handling), and gets a rounded-rect press ripple to match its checked
         * background (IconTextButton::setRectRipple()). Any other widget exposing a clicked() signal also toggles the
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

        /**
         * @brief Register the frame a MenuItem::frameSubmenu() row opens.
         * @param id Id of the frame-submenu item.
         * @param frame The frame to open beside the row, or nullptr to unregister.
         *
         * Normally called from a slot of submenuAboutToShow(id), which fires on every opening just
         * before the frame is shown, so the host can fill it with fresh content first. The frame is
         * chained to this menu (DropdownFrame::setChainParent()), so a click inside it does not
         * dismiss the menu, and an outside click or Escape closes both. The menu does NOT take
         * ownership: the host keeps the frame alive for as long as it is registered, and the menu
         * unchains it when it is unregistered, replaced, or the menu is destroyed or cleared.
         * Re-registering the same frame is a cheap no-op.
         *
         * A plain DropdownFrame draws no background, so give the frame a style: either the
         * ready-made one, setObjectName("dropdownPanel") (dropdownmenu.qss), or your own.
         */
        void setSubmenuFrame(int id, DropdownFrame* frame);

        /**
         * @brief The frame registered for a frame-submenu item, or nullptr.
         */
        DropdownFrame* submenuFrame(int id) const;

    signals:

        void itemTriggered(int id);
        void itemToggled(int id, bool checked);

        /**
         * @brief A submenu is about to be filled and shown.
         * @param id Id of the submenu item being opened.
         *
         * Last chance to edit that item's children (via items()/setItems() reaching into the
         * descriptor tree) before the submenu measures itself. For a MenuItem::frameSubmenu()
         * row this is where the host calls setSubmenuFrame() and fills the frame.
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

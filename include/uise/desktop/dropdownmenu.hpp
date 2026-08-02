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

UISE_DESKTOP_NAMESPACE_BEGIN

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

        int id=-1;
        QString text;
        std::shared_ptr<SvgIcon> icon;
        bool isCheckable=false;
        bool isChecked=false;
        bool isEnabled=true;
        bool isSeparator=false;
        bool isVisible=true;

        /**
         * @brief Exclusive (radio-like) group this item belongs to, or -1 for none.
         *
         * Checking an item with group>=0 unchecks every other currently-checked item that
         * shares the same group value, within this menu.
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

    public:

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
         * supported.
         */
        void setItemVisible(int id, bool visible);

        void setItemText(int id, const QString& text);
        void setItemIcon(int id, std::shared_ptr<SvgIcon> icon);

        /**
         * @brief Get the live row widget for an item.
         * @return Operation result, valid only while the menu is open (and the item was
         *  visible at the last opening); nullptr otherwise.
         */
        IconTextButton* itemButton(int id) const;

        /**
         * @brief Close the menu when a checkable item is toggled, same as a clickable item.
         * @param enable Default false: checkable items stay open so multiple can be toggled.
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

    signals:

        void itemTriggered(int id);
        void itemToggled(int id, bool checked);

    protected:

        void fillContent() override;
        void clearContent() override;

    private:

        void onItemToggled(int id, bool checked);

    private slots:

        void onTriggerClicked();

    private:

        std::unique_ptr<DropdownMenu_p> pimpl;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_DROPDOWNMENU_HPP

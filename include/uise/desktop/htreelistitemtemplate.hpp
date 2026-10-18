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

/** @file uise/desktop/htreelistitemtemplate.hpp
*
*  Declares template for list item of horizontal tree branch.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_HTREE_LIST_ITEM_TEMPLATE_HPP
#define UISE_DESKTOP_HTREE_LIST_ITEM_TEMPLATE_HPP

#include <functional>
#include <utility>
#include <memory>
#include <vector>
#include <QFrame>
#include <QPointer>

#include <uise/desktop/uisedesktop.hpp>

#include <uise/desktop/htreepath.hpp>
#include <uise/desktop/htreenode.hpp>
#include <uise/desktop/svgicon.hpp>
#include <uise/desktop/dropdownmenu.hpp>

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

class UISE_DESKTOP_EXPORT HTreeListItemTQ : public QObject
{
    Q_OBJECT

    public:

        using QObject::QObject;

        HTreeListItemTQ(const HTreeListItemTQ&)=delete;
        HTreeListItemTQ(HTreeListItemTQ&&)=delete;
        HTreeListItemTQ& operator=(const HTreeListItemTQ&)=delete;
        HTreeListItemTQ& operator=(HTreeListItemTQ&&)=delete;

        ~HTreeListItemTQ()
        {
            if (m_onDestroy)
            {
                m_onDestroy();
            }
        }

        void setOnDestroy(std::function<void()> onDestroy)
        {
            m_onDestroy=onDestroy;
        }

        void resetOnDestroy()
        {
            m_onDestroy=std::function<void()>{};
        }

        void setPathElement(HTreePathElement pathElement)
        {
            m_pathElement=std::move(pathElement);
        }

        const HTreePathElement& pathElement() const noexcept
        {
            return m_pathElement;
        }

        void setResidentPath(HTreePath residentPath)
        {
            m_residentPath=std::move(residentPath);
        }

        const HTreePath& residentPath() const noexcept
        {
            return m_residentPath;
        }

        void setTargetSubpath(HTreePath targetPath)
        {
            m_targetSubpath=std::move(targetPath);
        }

        const HTreePath& targetSubpath() const noexcept
        {
            return m_targetSubpath;
        }

        std::string uniqueId() const
        {
            return pathElement().uniqueId();
        }

    signals:

        void selectionChanged(bool);

        void openRequested(const UISE_DESKTOP_NAMESPACE::HTreePathElement&, const UISE_DESKTOP_NAMESPACE::HTreePath& residentPath={}, bool exclusive=false);
        void openInNewTabRequested(const UISE_DESKTOP_NAMESPACE::HTreePathElement&, const UISE_DESKTOP_NAMESPACE::HTreePath& residentPath);
        void openInNewTreeRequested(const UISE_DESKTOP_NAMESPACE::HTreePathElement&, const UISE_DESKTOP_NAMESPACE::HTreePath& residentPath);

        void openSubpathRequested(const UISE_DESKTOP_NAMESPACE::HTreePath&, const UISE_DESKTOP_NAMESPACE::HTreePath& residentPath);
        void openSubpathInNewTabRequested(const UISE_DESKTOP_NAMESPACE::HTreePath&, const UISE_DESKTOP_NAMESPACE::HTreePath& residentPath);
        void openSubpathInNewTreeRequested(const UISE_DESKTOP_NAMESPACE::HTreePath&, const UISE_DESKTOP_NAMESPACE::HTreePath& residentPath);

        void activateRequested(const UISE_DESKTOP_NAMESPACE::HTreePathElement&);

    private:

        std::function<void()> m_onDestroy;

        HTreePathElement m_pathElement;
        HTreePath m_residentPath;
        HTreePath m_targetSubpath;
};

/**
 * @brief Entries a host adds to the context menu of an HTreeListItemT row.
 *
 * The row's standard entries (Open, Open in new tab, Open in new window) are added by the row
 * itself; a host appends its own through HTreeListItemT::setContextMenuExtender() or
 * HTreeListItemT::fillContextMenu(). Each entry carries the function to run when it is chosen,
 * so the host does not deal with menu ids.
 *
 * addSubmenu() returns a child builder for a nested menu. Children share the handler list with
 * the root builder, so ids stay unique across the whole tree (DropdownMenu requires that) and
 * handler() on the root resolves any of them. A builder is move-only because it owns its children.
 */
class HTreeListItemMenu
{
    public:

        /** Ids below this are reserved for the row's standard entries. */
        constexpr static const int FirstCustomId=3;

        HTreeListItemMenu()
            : m_handlers(std::make_shared<std::vector<std::function<void ()>>>())
        {}

        HTreeListItemMenu(HTreeListItemMenu&&)=default;
        HTreeListItemMenu& operator=(HTreeListItemMenu&&)=default;
        HTreeListItemMenu(const HTreeListItemMenu&)=delete;
        HTreeListItemMenu& operator=(const HTreeListItemMenu&)=delete;

        /**
         * @brief Append an entry.
         * @param text Text of the entry.
         * @param icon Optional icon of the entry.
         * @param handler Called when the user chooses the entry.
         * @param hint Optional secondary label at the right edge of the entry (see MenuItem::hint).
         */
        void addItem(QString text, std::shared_ptr<SvgIcon> icon, std::function<void ()> handler, QString hint={})
        {
            MenuItem item(nextId(),std::move(text),std::move(icon));
            item.hint=std::move(hint);
            m_items.emplace_back(std::move(item));
            m_handlers->emplace_back(std::move(handler));
        }

        void addItem(QString text, std::function<void ()> handler, QString hint={})
        {
            addItem(std::move(text),std::shared_ptr<SvgIcon>{},std::move(handler),std::move(hint));
        }

        /**
         * @brief Append an entry that opens a nested menu.
         * @return The builder of the nested menu, valid until this builder is cleared or destroyed.
         *
         * A submenu left empty is shown disabled, since a submenu without children cannot open.
         */
        HTreeListItemMenu& addSubmenu(QString text, std::shared_ptr<SvgIcon> icon={})
        {
            // the submenu row takes an id of its own; it has no handler, so the slot stays empty
            m_items.emplace_back(nextId(),std::move(text),std::move(icon));
            m_handlers->emplace_back(std::function<void ()>{});

            m_submenus.emplace_back(m_items.size()-1,std::unique_ptr<HTreeListItemMenu>(new HTreeListItemMenu(m_handlers)));
            return *m_submenus.back().second;
        }

        void addSeparator()
        {
            m_items.emplace_back(MenuItem::separator());
        }

        /** @brief True if no entry was added at any level (separators do not count). */
        bool isEmpty() const noexcept
        {
            return m_handlers->empty();
        }

        /**
         * @brief The entries as DropdownMenu items, submenus included.
         * @return By value: the submenu children are assembled here.
         */
        std::vector<MenuItem> items() const
        {
            auto result=m_items;
            for (const auto& sub : m_submenus)
            {
                auto& item=result[sub.first];
                item.children=sub.second->items();
                if (item.children.empty())
                {
                    item.isEnabled=false;
                }
            }
            return result;
        }

        /**
         * @brief Handler of the entry with the given id.
         * @return A copy, so the caller can run it even if running it destroys the menu's owner
         *  (a handler may well rebuild the list the row belongs to). Empty for an unknown id.
         */
        std::function<void ()> handler(int id) const
        {
            const auto index=id-FirstCustomId;
            if (index<0 || static_cast<size_t>(index)>=m_handlers->size())
            {
                return std::function<void ()>{};
            }
            return (*m_handlers)[static_cast<size_t>(index)];
        }

        void clear()
        {
            m_items.clear();
            m_submenus.clear();
            // the handler list is shared with the root: only the root may reset it, or the ids it
            // handed out would shift under it
            if (!m_isChild)
            {
                m_handlers->clear();
            }
        }

    private:

        explicit HTreeListItemMenu(std::shared_ptr<std::vector<std::function<void ()>>> handlers)
            : m_handlers(std::move(handlers)),
              m_isChild(true)
        {}

        int nextId() const noexcept
        {
            return FirstCustomId+static_cast<int>(m_handlers->size());
        }

        std::vector<MenuItem> m_items;
        std::shared_ptr<std::vector<std::function<void ()>>> m_handlers;
        std::vector<std::pair<size_t,std::unique_ptr<HTreeListItemMenu>>> m_submenus;
        bool m_isChild=false;
};

template <typename BaseT>
class HTreeListItemT_p;

template <typename BaseT>
class HTreeListItemT : public BaseT
{
    public:

        using Base=BaseT;

        /**
         * @brief Constructor.
         * @param parent Parent widget.
         */
        HTreeListItemT(HTreePathElement el, QWidget* parent=nullptr);

        HTreeListItemT(QWidget* parent=nullptr) : HTreeListItemT(HTreePathElement{},parent)
        {}

        /**
         * @brief Destructor.
         */
        ~HTreeListItemT();

        HTreeListItemT(const HTreeListItemT&)=delete;
        HTreeListItemT(HTreeListItemT&&)=delete;
        HTreeListItemT& operator=(const HTreeListItemT&)=delete;
        HTreeListItemT& operator=(HTreeListItemT&&)=delete;

        void setWidget(QWidget* widget);

        QWidget* widget() const;

        template <typename T>
        T* widget() const
        {
            return qobject_cast<T*>(widget());
        }

        void setPathElement(HTreePathElement pathElement)
        {
            m_qobject->setPathElement(std::move(pathElement));
        }

        const HTreePathElement& pathElement() const noexcept
        {
            return m_qobject->pathElement();
        }

        void setResidentPath(HTreePath residentPath)
        {
            m_qobject->setResidentPath(std::move(residentPath));
        }

        const HTreePath& residentPath() const noexcept
        {
            return m_qobject->residentPath();
        }

        void setTargetSubpath(HTreePath targetPath)
        {
            m_qobject->setTargetSubpath(std::move(targetPath));
        }

        const HTreePath& targetSubpath() const noexcept
        {
            return m_qobject->targetSubpath();
        }

        void setSelected(bool enable);
        bool isSelected() const;

        void setOpenInTabEnabled(bool val);
        bool isOpenInTabEnabled() const noexcept;

        void setOpenInWindowEnabled(bool val);
        bool isOpenInWindowEnabled() const noexcept;

        void setNavigationHandler(
            QObject* obj,
            std::function<void(const HTreePathElement&,const HTreePath&)> openFn,
            std::function<void(const HTreePath&,const HTreePath&)> openSubpathFn
        );

        std::string uniqueId() const
        {
            return m_qobject->uniqueId();
        }

        HTreeListItemTQ* qobject() const
        {
            return m_qobject;
        }

        void click();

        /**
         * @brief Open the row's node the way a plain click does.
         *
         * Selects the row, then goes through the navigation handler if one is set, or emits
         * openRequested()/openSubpathRequested() otherwise. Navigation is deferred by one short
         * cycle so the selection highlight paints first.
         */
        void requestOpen();

        /**
         * @brief Open the row's node in a new tab, as Ctrl/Cmd-click does.
         *
         * Emits openSubpathInNewTabRequested() if the row has a target subpath, openInNewTabRequested() otherwise.
         */
        void requestOpenInNewTab();

        /**
         * @brief Open the row's node in a new window, as Shift-click does.
         *
         * Emits openSubpathInNewTreeRequested() if the row has a target subpath, openInNewTreeRequested() otherwise.
         */
        void requestOpenInNewWindow();

        void setButtonMode(bool enable)
        {
            m_buttonMode=enable;
        }

        bool isButtonMode() const
        {
            return m_buttonMode;
        }

        /**
         * @brief Let the user drag the row to a new place in its list.
         * @param enable Enable or disable dragging.
         *
         * The list must have a ListReorderDrop target for the drag to go anywhere, see
         * HTreeFlyweightListView::enableDragReorder(). While enabled the row opens its node on
         * mouse release instead of press, because the same press may turn into a drag; moving
         * the mouse past the drag distance starts the drag and the row is not opened. A row
         * with dragging disabled opens on press, as before.
         */
        void setDragReorderEnabled(bool enable);
        bool isDragReorderEnabled() const noexcept;

        /**
         * @brief Add entries to the row's context menu.
         * @param extender Called with the menu entries each time the menu is shown, after the
         *  standard entries and fillContextMenu() have been added. Entries it adds go below a
         *  separator. Pass an empty function to remove it.
         */
        void setContextMenuExtender(std::function<void (HTreeListItemMenu&)> extender);

        /**
         * @brief Show the row's context menu yourself instead of the standard one.
         * @param handler Called with the global position the menu was requested at, each time the
         *  user asks for the menu. When set, the standard menu, fillContextMenu() and the
         *  extender are all skipped. The handler emits qobject()'s openRequested() and friends
         *  itself for the entries it offers. Pass an empty function to restore the standard menu.
         *
         * For hosts that want a menu of their own, for example with entries that depend on state
         * of the node behind the row.
         */
        void setContextMenuHandler(std::function<void (const QPoint& globalPos)> handler);

    protected:

        void enterEvent(QEnterEvent *event) override;
        void leaveEvent(QEvent *event) override;
        void mousePressEvent(QMouseEvent *event) override;
        void mouseMoveEvent(QMouseEvent *event) override;
        void mouseReleaseEvent(QMouseEvent *event) override;
        void mouseDoubleClickEvent(QMouseEvent *event) override;

        virtual void fillContextMenu(HTreeListItemMenu& menu);

        void showMenu(const QPoint& pos);

        virtual void doSetHovered(bool enable);
        virtual void doSetSelected(bool enable);

        virtual void doClick()
        {}

    private:

        template <typename BaseT1>
        friend class HTreeListItemT_p;

        std::unique_ptr<HTreeListItemT_p<BaseT>> pimpl;

        HTreeListItemTQ* m_qobject;
        bool m_buttonMode=false;
};

#if defined(_MSC_VER) && !defined(UISE_DESKTOP_BUILD)
// The QFrame specialization is built into and exported from the uisedesktop
// DLL (htreelistitemexport.hpp). Suppress implicit instantiation in consumer
// TUs: clang-cl otherwise emits local copies of the inline members that
// collide with the DLL-exported ones at link time (LNK2005).
extern template class UISE_DESKTOP_EXPORT HTreeListItemT<QFrame>;
#endif

}

#endif // UISE_DESKTOP_HTREE_LIST_ITEM_TEMPLATE_HPP

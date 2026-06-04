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

/** @file uise/desktop/htreecontrollerlistitem.hpp
*
*  Declares HTreeControllerListItem — a generic list item that can act as
*  either a standard icon+text row (HTreeStandardListItem) or host an
*  arbitrary WidgetController-derived controller widget.
*
*  Usage:
*   - Default / text mode:  use the inherited setText/setIcon API; behaves
*     identically to HTreeStandardListItem.
*
*   - Controller mode:  call setController(ctrl) after init()ing the
*     controller.  The item takes QObject ownership of the controller.
*     Selection is forwarded to ctrl->setSelected(bool) if that method
*     exists on ControllerT (detected at compile time).
*
*  Requirements on ControllerT:
*   - Must derive from WidgetController (provides qWidget() / QObject
*     ancestry needed for QPointer).
*   - Should expose void setSelected(bool) for selection forwarding;
*     if absent, selection is silently ignored in controller mode.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_HTREE_CONTROLLER_LIST_ITEM_HPP
#define UISE_DESKTOP_HTREE_CONTROLLER_LIST_ITEM_HPP

#include <type_traits>

#include <QPointer>

#include <uise/desktop/widget.hpp>
#include <uise/desktop/htreestandardlistitem.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

namespace detail {

template <typename T, typename = void>
struct HasSetSelected : std::false_type {};

template <typename T>
struct HasSetSelected<T, std::void_t<decltype(std::declval<T*>()->setSelected(bool{}))>>
    : std::true_type {};

} // namespace detail

/**
 * @brief A list item compatible with HTreeStandardListView that can hold
 *        either a plain icon+text row or an embedded WidgetController.
 *
 * Because it inherits HTreeStandardListItem it slots into any
 * HTreeStansardListIemWrapper / HTreeStandardListView without changes to
 * the wrapper, view, or template instantiations.
 *
 * @tparam ControllerT  A WidgetController-derived class whose qWidget()
 *                      surface is used as the item's content in controller
 *                      mode.  Should provide void setSelected(bool).
 */
template <typename ControllerT>
class HTreeControllerListItem : public HTreeStandardListItem
{
    static_assert(std::is_base_of_v<WidgetController, ControllerT>,
        "ControllerT must derive from WidgetController");

    public:

        using HTreeStandardListItem::HTreeStandardListItem;

        /**
         * @brief Switch to controller mode.
         *
         * Hosts the controller's widget surface (ctrl->qWidget()) inside
         * this item's content area, replacing the standard icon+text widgets.
         * Transfers QObject ownership of @p ctrl to this item.
         *
         * @pre  ctrl->qWidget() must be non-null.  This is guaranteed when
         *       ctrl was created via makeWidget<ControllerT>() because
         *       WidgetControllerTraits::preConstruct() eagerly calls
         *       createActualWidget() during construction.
         * @pre  ctrl->init() (or equivalent setup) should be done before
         *       this call.
         */
        void setController(ControllerT* ctrl)
        {
            m_controller = ctrl;
            if (ctrl != nullptr)
            {
                // Transfer QObject ownership so the controller is destroyed
                // with the item.
                ctrl->setParent(this);

                // setItemWidgets() destroys the base m_icon / m_text and
                // replaces them with the controller's widget surface.
                // addWidget() inside setItemWidgets() reparents ctrl->qWidget()
                // to the internal content wrapper.
                setItemWidgets(nullptr, ctrl->qWidget(), 1);
            }
        }

        ControllerT* controller() const noexcept
        {
            return m_controller;
        }

        bool hasController() const noexcept
        {
            return m_controller != nullptr;
        }

    protected:

        /**
         * @brief Forward selection to the controller in controller mode;
         *        fall back to the base implementation in standard mode.
         *
         * The base HTreeStandardListItem::doSetSelected touches m_icon which
         * is destroyed after setController(), so it must not be called in
         * controller mode.
         */
        void doSetSelected(bool enable) override
        {
            if (m_controller != nullptr)
            {
                if constexpr (detail::HasSetSelected<ControllerT>::value)
                {
                    m_controller->setSelected(enable);
                }
            }
            else
            {
                HTreeStandardListItem::doSetSelected(enable);
            }
        }

        /**
         * @brief Propagate hover to the base only in standard mode.
         *
         * In controller mode the embedded controller's widget manages its own
         * hover visuals; no external propagation is needed.
         */
        void doSetHovered(bool enable) override
        {
            if (m_controller == nullptr)
            {
                HTreeStandardListItem::doSetHovered(enable);
            }
        }

    private:

        QPointer<ControllerT> m_controller;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_HTREE_CONTROLLER_LIST_ITEM_HPP

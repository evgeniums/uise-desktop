/**
@copyright Evgeny Sidorov 2021

This software is dual-licensed. Choose the appropriate license for your project.

1. For open source projects this software is distrubuted under the GNU GENERAL PUBLIC LICENSE, Version 3.0.
(See accompanying file LICENSE-GPLv3.md or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. For proprietary license contact evgeniums@dracosha.com.

*/

/****************************************************************************/

/** @file uise/desktop/utils/orientationinvariant.hpp
*
*  Contains definition of OrientationInvariant.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_ORIENTATIONINVARIANT_HPP
#define UISE_DESKTOP_ORIENTATIONINVARIANT_HPP

#include <cstdint>
#include <type_traits>

#include <QPoint>
#include <QWidget>

#include <uise/desktop/uisedesktop.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

//--------------------------------------------------------------------------

/**
 * @brief Properties that can be get/set for orientation invariant objects.
 */
enum class OProp : uint8_t
{
    size,
    pos,
    edge,
    min_size,
    max_size
};

/**
 * @brief Base class for implementing orientation invariant classes.
 *
 * Orientation invariant class is a class that has a orientation property and must be processed in the same way
 * regardless of the current value of the orientation property. Special methods must be used to work with sizes and coordintaes.
 * Instead of width/height/x/y methods use oprop/setOprop methods as getters/setters of corresponding fields.
 *
 */
class OrientationInvariant
{
    public:

        //! Constructor.
        OrientationInvariant()=default;

        //! Destructor.
        virtual ~OrientationInvariant()=default;

        OrientationInvariant(const OrientationInvariant&)=default;
        OrientationInvariant(OrientationInvariant&&)=default;
        OrientationInvariant& operator=(const OrientationInvariant&)=default;
        OrientationInvariant& operator=(OrientationInvariant&&)=default;

        /**
         * @brief Get object property for specified orientation.
         * @param horizontal True if the orientation is horizontal, false if vertical.
         * @param obj Object whose property to get.
         * @param prop Property ID.
         * @return Requested property.
         */
        template <typename T>
        static int opropRef(bool horizontal, const T& obj, OProp prop) noexcept
        {
            if constexpr (std::is_same_v<QPoint,std::decay_t<T>>)
            {
                switch (prop)
                {
                    case(OProp::pos):
                        return horizontal?obj.x():obj.y();
                        break;

                    case(OProp::size): [[fallthrough]];
                    case(OProp::edge): [[fallthrough]];
                    case(OProp::min_size): [[fallthrough]];
                    case(OProp::max_size):
                        return 0;
                }
                return 0;
            }
            else if constexpr (std::is_base_of_v<QWidget,std::decay_t<T>>)
            {
                switch (prop)
                {
                    case(OProp::size):
                        return horizontal?obj.width():obj.height();
                        break;

                    case(OProp::pos):
                        return horizontal?obj.x():obj.y();
                        break;

                    case(OProp::edge):
                        return horizontal?obj.geometry().right():obj.geometry().bottom();
                        break;

                    case(OProp::min_size):
                        return horizontal?obj.minimumWidth():obj.minimumHeight();
                        break;

                    case(OProp::max_size):
                        return horizontal?obj.maximumWidth():obj.maximumHeight();
                        break;
                }
                return 0;
            }
            else if constexpr (std::is_same_v<QSize,std::decay_t<T>>)
            {
                return horizontal?obj.width():obj.height();
            }
            else if constexpr (
                        std::is_base_of_v<QRect,std::decay_t<T>>
                    )
            {
                switch (prop)
                {
                    case(OProp::size): [[fallthrough]];
                    case(OProp::min_size): [[fallthrough]];
                    case(OProp::max_size):
                        return horizontal?obj.width():obj.height();
                        break;

                    case(OProp::pos):
                        return horizontal?obj.x():obj.y();
                        break;

                    case(OProp::edge):
                        return horizontal?obj.right():obj.bottom();
                        break;
                }
                return 0;
            }

            return 0;
        }

        /**
         * @brief Get object property for specified orientation.
         * @param horizontal True if the orientation is horizontal, false if vertical.
         * @param obj Object whose property to get.
         * @param prop Property ID.
         * @return Requested property.
         */
        template <typename T>
        static int oprop(bool horizontal, const T& obj, OProp prop) noexcept
        {
            if constexpr (std::is_pointer_v<std::remove_reference_t<T>>)
            {
                return opropRef(horizontal,*obj,prop);
            }
            else
            {
                return opropRef(horizontal,obj,prop);
            }
        }

        /**
         * @brief Set object property for specified orientation.
         * @param horizontal True if the orientation is horizontal, false if vertical.
         * @param obj Object whose property to set.
         * @param prop Property ID.
         * @param value Value to set to the property.
         */
        template <typename T>
        static void setOProp(bool horizontal, T& obj, OProp prop, int value) noexcept
        {
            if constexpr (std::is_same_v<QPoint,std::decay_t<T>>)
            {
                switch (prop)
                {
                    case(OProp::pos):
                        if (horizontal)
                        {
                            obj.setX(value);
                        }
                        else
                        {
                            obj.setY(value);
                        }
                    break;

                    case(OProp::min_size): [[fallthrough]];
                    case(OProp::max_size): [[fallthrough]];
                    case(OProp::size): [[fallthrough]];
                    case(OProp::edge):
                    break;
                }
            }
            else if constexpr (std::is_base_of_v<QWidget,std::decay_t<T>>)
            {
                switch (prop)
                {
                    case(OProp::min_size):
                        if (horizontal)
                        {
                            obj.setMinimumWidth(value);
                        }
                        else
                        {
                            obj.setMinimumHeight(value);
                        }
                    break;

                    case(OProp::max_size):
                        if (horizontal)
                        {
                            obj.setMaximumWidth(value);
                        }
                        else
                        {
                            obj.setMaximumHeight(value);
                        }
                    break;

                    case(OProp::size): [[fallthrough]];
                    case(OProp::pos): [[fallthrough]];
                    case(OProp::edge):
                    break;
                }
            }
            else if constexpr (std::is_same_v<QSize,std::decay_t<T>>)
            {
                if (horizontal)
                {
                    obj.setWidth(value);
                }
                else
                {
                    obj.setHeight(value);
                }
            }
            else if constexpr (std::is_base_of_v<QRect,std::decay_t<T>>)
            {
                switch (prop)
                {
                    case(OProp::size): [[fallthrough]];
                    case(OProp::min_size): [[fallthrough]];
                    case(OProp::max_size):
                        if (horizontal)
                        {
                            obj.setWidth(value);
                        }
                        else
                        {
                            obj.setHeight(value);
                        }
                    break;

                    case(OProp::pos):
                        if (horizontal)
                        {
                            obj.setX(value);
                        }
                        else
                        {
                            obj.setY(value);
                        }
                    break;

                    case(OProp::edge):
                    break;
                }
            }
        }

        /**
         * @brief Get object's property depending on the orientation of this object.
         * @param obj Object whose property to get.
         * @param prop Property ID.
         * @param other If set then use orientation that is orthogonal to the orientation of this object (horizontal<->vertical).
         */
        template <typename T>
        int oprop(const T& obj, OProp prop, bool other = false) const noexcept
        {
            bool horizontal=isHorizontal();
            if (other)
            {
                horizontal=!horizontal;
            }
            return oprop(horizontal,obj,prop);
        }

        /**
         * @brief Set object's property depending on the orientation of this object.
         * @param obj Object whose property to set.
         * @param prop Property ID.
         * @param value Value to set to the property.
         * @param other If set then use orientation that is orthogonal to the orientation of this object (horizontal<->vertical).
         */
        template <typename T>
        void setOProp(T& obj, OProp prop, int value, bool other = false) noexcept
        {
            bool horizontal=isHorizontal();
            if (other)
            {
                horizontal=!horizontal;
            }

            if constexpr (std::is_pointer_v<std::remove_reference_t<T>>)
            {
                setOProp(horizontal,*obj,prop,value);
            }
            else
            {
                setOProp(horizontal,obj,prop,value);
            }
        }

        /**
         * @brief Set object property depending on the orientation of this object.
         * @param obj Object whose property to set.
         * @param prop Property ID.
         * @param value Value to set to the property.
         * @param other If set then use orientation that is orthogonal to the orientation of this object (horizontal<->vertical).
         *
         * Overloaded method that can be called from constant methods.
         */
        template <typename T>
        void setOProp(T& obj, OProp prop, int value, bool other = false) const noexcept
        {
            auto self=const_cast<OrientationInvariant*>(this);
            self->setOProp(obj,prop,value,other);
        }

        /**
         * @brief Check if orientetion of this object is horizontal.
         * @return True if orientation is horizontal, false if is vertical.
         */
        virtual bool isHorizontal() const noexcept=0;

        /**
         * @brief Check if orientetion of this object is vertical.
         * @return True if orientation is vertical, false if is horizontal.
         */
        inline bool isVertical() const noexcept
        {
            return !isHorizontal();
        }
};

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_ORIENTATIONINVARIANT_HPP

/**
@copyright Evgeny Sidorov 2021

This software is dual-licensed. Choose the appropriate license for your project.

1. For open source projects this software is distrubuted under the GNU GENERAL PUBLIC LICENSE, Version 3.0.
(See accompanying file LICENSE-GPLv3.md or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. For proprietary license contact evgeniums@dracosha.com.

*/

/****************************************************************************/

/** @file uise/desktop/utils/pointerholder.hpp
*
*  Defines PointerHolder class.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_POINTERHOLDER_HPP
#define UISE_DESKTOP_POINTERHOLDER_HPP

#include <QMetaType>
#include <QVariant>
#include <QObject>

#include <uise/desktop/uisedesktop.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/**
 * @brief Helper for holding pointer of any type in QVariant.
 */
class PointerHolder
{
    public:

        /**
         * @brief Default constructor.
         */
        PointerHolder():m_value(0)
        {}

        /**
         * @brief Constructor.
         * @param ptr Pointer to hold.
         */
        template <typename T>
        PointerHolder(T ptr) noexcept : m_value(reinterpret_cast<uintptr_t>(ptr))
        {}

        /**
         * @brief Get held pointer;
         * @return Held pointer.
         */
        template <typename T>
        T value() const noexcept
        {
            return reinterpret_cast<T>(m_value);
        }

        /**
         * @brief Keep holder in property of QObject.
         * @param ptr Pointer to keep.
         * @param obj Object where to keep.
         * @param propertyName Name of the property.
         */
        template <typename T>
        static void keepProperty(T ptr, QObject* obj, const char* propertyName)
        {
            QVariant prop;
            prop.setValue(PointerHolder(ptr));
            obj->setProperty(propertyName,prop);
        }

        /**
         * @brief Get pointer from property of QObject.
         * @param obj Object where to get pointer from.
         * @param propertyName Name of the property.
         */
        template <typename T>
        static T getProperty(const QObject* obj, const char* propertyName)
        {
            if (!obj)
            {
                return nullptr;
            }
            return obj->property(propertyName).value<PointerHolder>().value<T>();
        }

        /**
         * @brief Remove pointer from property of QObject.
         * @param obj Object where to remove pointer from.
         * @param propertyName Name of the property.
         */
        static void clearProperty(QObject* obj, const char* propertyName)
        {
            if (obj)
            {
                obj->setProperty(propertyName,QVariant());
            }
        }

    private:

        uintptr_t m_value;
};

UISE_DESKTOP_NAMESPACE_END

Q_DECLARE_METATYPE(UISE_DESKTOP_NAMESPACE::PointerHolder)

#endif // UISE_DESKTOP_POINTERHOLDER_HPP

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

/** @file uise/desktop/stylecontext.hpp
*
*  Declares StyleCOntext.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_STYLE_CONTEXT_HPP
#define UISE_DESKTOP_STYLE_CONTEXT_HPP

#include <QObject>
#include <QSet>

#include <uise/desktop/uisedesktop.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

using ContextSelector=QStringList;

class StyleContext
{
    public:

        StyleContext(const QObject* obj=nullptr) : m_object(obj)
        {}

        const QObject* object() const
        {
            return m_object;
        }

        std::pair<size_t,QString> pathHash(const QString& name=QString{}) const
        {
            QString acc{name};
            auto obj=m_object;
            while (obj!=nullptr)
            {
                acc+=typeAndName(obj);
                obj=obj->parent();
            }

            auto hash=qHash(acc);
            return std::make_pair(hash,acc);
        }

        static std::pair<size_t,QString> contextPathHash(const ContextSelector& context, const QString& name=QString{})
        {
            QString acc{name};
            for (auto&& item:context)
            {
                acc+=item;
            }
            auto hash=qHash(acc);
            return std::make_pair(hash,acc);
        }

        constexpr static const uint64_t Mask=0x8000000000;

        uint64_t matches(const ContextSelector& selector) const
        {
            if (m_object==nullptr || selector.empty())
            {
                return 0;
            }

            std::array<const QObject*,64> chain;

            size_t index=0;
            size_t depth=0;
            uint64_t mask=0;

            // find matching object for the first selector
            const auto* obj=m_object;
            while (obj!=nullptr)
            {
                chain[depth]=obj;
                mask=matches(obj,selector.at(0),depth);
                if (mask!=0)
                {
                    break;
                }
                ++depth;
                obj=obj->parent();
            }

            if (mask==0)
            {
                // matching objects not found
                return 0;
            }

            // calculate matching mask for the rest selectors
            for (size_t i=1;i<selector.size();i++)
            {
                if (depth==0)
                {
                    return 0;
                }

                const auto nextSelector=selector.at(i);

                // check matching starting from found object
                for (size_t j=depth-1;j>=0;)
                {
                    auto nextMask=matches(chain[j],nextSelector,depth);
                    if (nextMask!=0)
                    {
                        mask|=nextMask;
                        depth=j;
                        break;
                    }
                }
            }

            // done
            return mask;
        }

        uint64_t matches(const QObject* obj, const QString& selector, size_t depth=0) const
        {
            if (m_object==nullptr || selector.isEmpty())
            {
                return 0;
            }

            auto typeAndName=StyleContext::typeAndName(obj);
            if (!typeAndName.isEmpty())
            {
                if (selector==typeAndName)
                {
                    return Mask>>depth;
                }
                if (selector==m_object->objectName())
                {
                    return Mask>>(depth+1);
                }
            }

            if (selector==m_object->metaObject()->className())
            {
                return Mask>>(depth+2);
            }

            return 0;
        }

        static QString typeAndName(const QObject* obj, bool orJustType=false)
        {
            if (obj!=nullptr)
            {
                auto name=obj->objectName();
                if (!name.isEmpty())
                {
                    return QString("%1#%2").arg(obj->metaObject()->className(),name);
                }
                if (orJustType)
                {
                    return obj->metaObject()->className();
                }
            }
            return QString{};
        }

        static QString joinSelector(const ContextSelector& selector)
        {
            return selector.join("");
        }

    private:

        const QObject* m_object;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_STYLE_CONTEXT_HPP

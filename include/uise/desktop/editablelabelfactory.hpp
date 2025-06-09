/**
@copyright Evgeny Sidorov 2021

This software is dual-licensed. Choose the appropriate license for your project.

1. The GNU GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-GPLv3.md](LICENSE-GPLv3.md) or copy at https://www.gnu.org/licenses/gpl-3.0.txt)
    
2. The GNU LESSER GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-LGPLv3.md](LICENSE-LGPLv3.md) or copy at https://www.gnu.org/licenses/lgpl-3.0.txt).

You may select, at your option, one of the above-comboed licenses.

*/

/****************************************************************************/

/** @file uise/desktop/EditableLabelfactory.hpp
*
*  Declares EditableLabelFactory.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_EditableLabelFACTORY_HPP
#define UISE_DESKTOP_EditableLabelFACTORY_HPP

#include <map>

#include <QFrame>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/editablelabel.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

using EditableLabelBuilder=std::function<EditableLabel* (QWidget*)>;

class UISE_DESKTOP_EXPORT EditableLabelFactory
{
    public:

        EditableLabelFactory()
        {
            addBuilder(EditableLabelType::Text,[](QWidget* parent=nullptr){return new EditableLabelText(parent);});
            addBuilder(EditableLabelType::Int,[](QWidget* parent=nullptr){return new EditableLabelInt(parent);});
            addBuilder(EditableLabelType::Double,[](QWidget* parent=nullptr){return new EditableLabelDouble(parent);});
            addBuilder(EditableLabelType::Combo,[](QWidget* parent=nullptr){return new EditableLabelCombo(parent);});
            addBuilder(EditableLabelType::Date,[](QWidget* parent=nullptr){return new EditableLabelDate(parent);});
            addBuilder(EditableLabelType::Time,[](QWidget* parent=nullptr){return new EditableLabelTime(parent);});
            addBuilder(EditableLabelType::DateTime,[](QWidget* parent=nullptr){return new EditableLabelDateTime(parent);});
        }

        template <typename Type>
        EditableLabel* make(Type type, QWidget* parent=nullptr, QString id={}, bool inGroup=false) const
        {
            auto b=builder(type);
            if (!b)
            {
                return nullptr;
            }

            auto l=b(parent);
            if (l!=nullptr)
            {
                l->setId(std::move(id));
                l->setInGroup(inGroup);
            }
            return l;
        }

        EditableLabel* make(const EditableLabelConfig& config, QWidget* parent=nullptr, bool inGroup=false) const
        {
            auto l=make(config.type,parent,config.id,inGroup);
            if (l==nullptr)
            {
                return nullptr;
            }
            l->setVariantValue(config.value);
            return l;
        }

        template <typename Type>
        void addBuilder(Type type, EditableLabelBuilder builder)
        {
            m_builders.emplace(static_cast<int>(type),std::move(builder));
        }

        template <typename Type>
        EditableLabelBuilder builder(Type type) const
        {
            auto it=m_builders.find(static_cast<int>(type));
            if (it!=m_builders.end())
            {
                return it->second;
            }
            return EditableLabelBuilder{};
        }

        static EditableLabelFactory& instance();

    private:

        std::map<int,EditableLabelBuilder> m_builders;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_EditableLabelFACTORY_HPP

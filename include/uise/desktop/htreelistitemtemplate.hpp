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

#include <memory>
#include <QFrame>

#include <uise/desktop/uisedesktop.hpp>

#include <uise/desktop/htreepath.hpp>
#include <uise/desktop/htreenode.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

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

    signals:

        void selectionChanged(bool);
        void openRequested(const UISE_DESKTOP_NAMESPACE::HTreePathElement&, bool exclusive=false);
        void openInNewTabRequested(const UISE_DESKTOP_NAMESPACE::HTreePathElement&, const UISE_DESKTOP_NAMESPACE::HTreePath& residentPath);
        void openInNewTreeRequested(const UISE_DESKTOP_NAMESPACE::HTreePathElement&, const UISE_DESKTOP_NAMESPACE::HTreePath& residentPath);

        void activateRequested(const UISE_DESKTOP_NAMESPACE::HTreePathElement&);

    private:

        std::function<void()> m_onDestroy;
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

        void setPathElement(HTreePathElement pathElement);
        const HTreePathElement& pathElement() const noexcept;

        void setResidentPath(HTreePath setResidentPath);
        const HTreePath& residentPath() const noexcept;

        void setSelected(bool enable);
        bool isSelected() const;

        void setOpenInTabEnabled(bool val);
        bool isOpenInTabEnabled() const noexcept;

        void setOpenInWindowEnabled(bool val);
        bool isOpenInWindowEnabled() const noexcept;

        std::string uniqueId() const;

        HTreeListItemTQ* qobject() const
        {
            return m_qobject;
        }

        void click();

    protected:

        void enterEvent(QEnterEvent *event) override;
        void leaveEvent(QEvent *event) override;
        void mousePressEvent(QMouseEvent *event) override;
        void mouseDoubleClickEvent(QMouseEvent *event) override;

        virtual void fillContextMenu(QMenu* menu);

        void showMenu(const QPoint& pos);

        virtual void doSetHovered(bool enable);
        virtual void doSetSelected(bool enable);

    private:

        template <typename BaseT1>
        friend class HTreeListItemT_p;

        std::unique_ptr<HTreeListItemT_p<BaseT>> pimpl;

        HTreeListItemTQ* m_qobject;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_HTREE_LIST_ITEM_TEMPLATE_HPP

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

        void openRequested(const UISE_DESKTOP_NAMESPACE::HTreePathElement&, const UISE_DESKTOP_NAMESPACE::HTreePath& residentPath, bool exclusive=false);
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

        std::string uniqueId() const
        {
            return m_qobject->uniqueId();
        }

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

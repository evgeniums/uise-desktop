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

/** @file uise/desktop/htreelistitem.hpp
*
*  Declares list item of horizontal tree branch.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_HTREE_LIST_ITEM_HPP
#define UISE_DESKTOP_HTREE_LIST_ITEM_HPP

#include <memory>
#include <QFrame>

#include <uise/desktop/uisedesktop.hpp>

#include <uise/desktop/htreepath.hpp>
#include <uise/desktop/htreenode.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class SvgIcon;
class HTreeListItem_p;

class UISE_DESKTOP_EXPORT HTreeListItem : public QFrame
{
    Q_OBJECT

    public:

        /**
         * @brief Constructor.
         * @param parent Parent widget.
         */
        HTreeListItem(QWidget* parent=nullptr);

        /**
         * @brief Destructor.
         */
        ~HTreeListItem();

        HTreeListItem(const HTreeListItem&)=delete;
        HTreeListItem(HTreeListItem&&)=delete;
        HTreeListItem& operator=(const HTreeListItem&)=delete;
        HTreeListItem& operator=(HTreeListItem&&)=delete;

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

    public slots:

        void click();

    signals:

        void selectionChanged(bool);
        void openRequested(const UISE_DESKTOP_NAMESPACE::HTreePathElement&);
        void openInNewTabRequested(const UISE_DESKTOP_NAMESPACE::HTreePathElement&, const UISE_DESKTOP_NAMESPACE::HTreePath& residentPath);
        void openInNewTreeRequested(const UISE_DESKTOP_NAMESPACE::HTreePathElement&, const UISE_DESKTOP_NAMESPACE::HTreePath& residentPath);

        void activateRequested(const UISE_DESKTOP_NAMESPACE::HTreePathElement&);

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

        friend class HTreeListItem_p;

        std::unique_ptr<HTreeListItem_p> pimpl;
};

class HTreeStansardListItem_p;

class UISE_DESKTOP_EXPORT HTreeStansardListItem : public HTreeListItem
{
    Q_OBJECT

    public:

        /**
         * @brief Constructor.
         * @param parent Parent widget.
         */
        HTreeStansardListItem(const QString& type, const QString& text, std::shared_ptr<SvgIcon> icon={}, QWidget* parent=nullptr);

        /**
         * @brief Destructor.
         */
        ~HTreeStansardListItem();

        HTreeStansardListItem(const HTreeStansardListItem&)=delete;
        HTreeStansardListItem(HTreeStansardListItem&&)=delete;
        HTreeStansardListItem& operator=(const HTreeStansardListItem&)=delete;
        HTreeStansardListItem& operator=(HTreeStansardListItem&&)=delete;

        QString type() const;

        QString text() const;
        QPixmap pixmap() const;

        void setIcon(std::shared_ptr<SvgIcon> icon);
        std::shared_ptr<SvgIcon> icon() const;

        void setTextElideMode(Qt::TextElideMode mode);
        Qt::TextElideMode textElideMode() const;

        void setPropagateIconClick(bool enable);
        bool isPropagateIconClick() const;

        std::string name() const
        {
            return pathElement().name();
        }

        std::string sortValue() const noexcept
        {
            return name();
        }

        std::string id() const
        {
            return pathElement().id();
        }

    signals:

        void iconClicked();

    public slots:

        void setText(const QString& text);
        void setPixmap(const QPixmap& pixmap);

    protected:

        void doSetHovered(bool enable) override;
        void doSetSelected(bool enable) override;

    private:

        std::unique_ptr<HTreeStansardListItem_p> pimpl;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_HTREE_LIST_ITEM_HPP

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
        const HTreePathElement& pathElement() const;

        void setSelected(bool enable);
        bool isSelected() const;

    signals:

        void selectionChanged(bool);
        void openRequested(const UISE_DESKTOP_NAMESPACE::HTreePathElement&);
        void openInNewTabRequested(const UISE_DESKTOP_NAMESPACE::HTreePathElement&);

    protected:

        void enterEvent(QEnterEvent *event) override;
        void leaveEvent(QEvent *event) override;
        void mousePressEvent(QMouseEvent *event) override;

        virtual void fillContextMenu(QMenu* menu);

        void showMenu(const QPoint& pos);

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
        HTreeStansardListItem(const QString& type, QWidget* parent=nullptr);

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

        void setTextElideMode(Qt::TextElideMode mode);
        Qt::TextElideMode textElideMode() const;

    public slots:

        void setText(const QString& text);
        void setPixmap(const QPixmap& pixmap);

    private:

        std::unique_ptr<HTreeStansardListItem_p> pimpl;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_HTREE_LIST_ITEM_HPP

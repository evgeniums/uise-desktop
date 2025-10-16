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

/** @file uise/desktop/htreelistwidget.hpp
*
*  Declares list widget for htree list branch node.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_HTREE_LIST_WIDGET_HPP
#define UISE_DESKTOP_HTREE_LIST_WIDGET_HPP

#include <memory>

#include <QFrame>
#include <QPointer>
#include <QVBoxLayout>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/widget.hpp>
#include <uise/desktop/statusdialog.hpp>

#include <uise/desktop/htreelistitem.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class HTreeList;

class HTreeListWidget_p;
class UISE_DESKTOP_EXPORT HTreeListWidget : public WidgetQFrame
{
    Q_OBJECT

    public:

        constexpr static const int DefaultMaxItemWidth=170;
        constexpr static const int ItemExtraWidth=30;

        HTreeListWidget(QWidget* parent=nullptr);

        ~HTreeListWidget();

        HTreeListWidget(const HTreeListWidget&)=delete;
        HTreeListWidget(HTreeListWidget&&)=delete;
        HTreeListWidget& operator=(const HTreeListWidget&)=delete;
        HTreeListWidget& operator=(HTreeListWidget&&)=delete;

        QSize sizeHint() const override;

        void setDefaultMaxItemWidth(int val);

        int defaultMaxItemWidth() const noexcept;

        void setLayoutFrame(QWidget* frame, QVBoxLayout* layout=nullptr);
        void setContentWidgets(QWidget* listView, QWidget* topWidget=nullptr, QWidget* bottomWidget=nullptr);

        void showError(const QString& message, const QString& title={});
        void popupStatus(const QString& message, StatusDialog::Type type, const QString& title={});
        void setBusyWaiting(bool enable);

    public slots:

        void setNextNodeId(const std::string& id);
        void onItemInsert(UISE_DESKTOP_NAMESPACE::HTreeListItem* item);
        void onItemRemove(UISE_DESKTOP_NAMESPACE::HTreeListItem* item);

    private:

        std::unique_ptr<HTreeListWidget_p> pimpl;

        friend class HTreeListUiHelper;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_HTREE_LIST_WIDGET_HPP

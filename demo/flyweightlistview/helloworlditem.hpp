/**
@copyright Evgeny Sidorov 2021

This software is dual-licensed. Choose the appropriate license for your project.

1. For open source projects this software is distrubuted under the GNU GENERAL PUBLIC LICENSE, Version 3.0.
(See accompanying file LICENSE-GPLv3.md or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. For proprietary license contact evgeniums@dracosha.com.

*/

/****************************************************************************/

/** \file demo/flyweightlistview/main.cpp
*
*  Demo application of FlyweightListView.
*
*/

/****************************************************************************/

#include <QApplication>
#include <QMainWindow>
#include <QLabel>
#include <QTimer>
#include <QPushButton>
#include <QTextBrowser>
#include <QSpinBox>
#include <QComboBox>
#include <QLineEdit>

#include <uise/desktop/flyweightlistview.hpp>
#include <uise/desktop/flyweightlistview.ipp>

#include <uise/desktop/flyweightlistitem.hpp>

using namespace UISE_DESKTOP_NAMESPACE;

//--------------------------------------------------------------------------

static size_t MaxHelloWorldItemId=1000000;
static size_t HelloWorldItemId=MaxHelloWorldItemId;

class HelloWorldItem : public QFrame
{
    Q_OBJECT

    public:

        HelloWorldItem(size_t seqNum, size_t id, QWidget* parent=nullptr)
            : QFrame(parent),
              m_id(id),
              m_seqNum(seqNum),
              m_browser(new QTextBrowser(this))
        {
            auto l=Layout::horizontal(this);
            l->addWidget(m_browser);

            m_browser->setText(QString("Hello world %1, %2").arg(seqNum).arg(m_id));
            setObjectName(QString("Item %1, %2").arg(seqNum).arg(m_id));
            setFixedHeight(64);
        }

        HelloWorldItem(size_t seqNum, QWidget* parent=nullptr)
            : HelloWorldItem(seqNum,0,parent)
        {
        }

        size_t seqNum() const noexcept
        {
            return m_seqNum;
        }

        size_t id() const noexcept
        {
            return m_id;
        }

        void setSeqNum(size_t value)
        {
            m_seqNum=value;
        }

    private:

        void keyPressEvent(QKeyEvent *ev)
        {
            ev->ignore();
        }

#if 0
        void resizeEvent(QResizeEvent* ev)
        {
            if (ev->size().width()<=20)
            {
                return;
            }
            auto newWidth=ev->size().width()-20;
            if (newWidth<450)
            {
                 newWidth=450;
            }
            else if (newWidth>750)
            {
                newWidth=750;
            }
            m_browser->setFixedWidth(newWidth);
        }
#endif
        size_t m_id;
        size_t m_seqNum;
        QTextEdit* m_browser;
};

struct HelloWorldItemTraits : public FlyweightListItemTraits<HelloWorldItem*,QFrame,size_t,size_t>
{
    static size_t sortValue(const HelloWorldItem* item) noexcept
    {
        return item->seqNum();
    }

    static QFrame* widget(HelloWorldItem* item) noexcept
    {
        return item;
    }

    static size_t id(const HelloWorldItem* item)
    {
        return item->id();
    }
};

using HelloWorldItemWrapper=FlyweightListItem<HelloWorldItemTraits>;

//--------------------------------------------------------------------------

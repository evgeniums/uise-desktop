/**
@copyright Evgeny Sidorov 2021

This software is dual-licensed. Choose the appropriate license for your project.

1. For open source projects this software is distrubuted under the GNU GENERAL PUBLIC LICENSE, Version 3.0.
(See accompanying file LICENSE-GPLv3.md or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. For proprietary license contact evgeniums@dracosha.com.

*/

/****************************************************************************/

/** @file uise/test/utils/testorientationinvariant.cpp
*
*  Test of OrientationInvariant class.
*
*/

/****************************************************************************/

#include <boost/test/unit_test.hpp>

#include <uise/test/uise-testthread.hpp>
#include <uise/desktop/utils/orientationinvariant.hpp>

using namespace UISE_DESKTOP_NAMESPACE;
using namespace UISE_TEST_NAMESPACE;

namespace {

template <Qt::Orientation orientation>
class SampleOrientationInvariant : public OrientationInvariant
{
    public:

        bool isHorizontal() const noexcept override
        {
            return orientation==Qt::Horizontal;
        }
};

}

BOOST_AUTO_TEST_SUITE(TestOrientationInvariant)

BOOST_AUTO_TEST_CASE(TestPoint)
{
    SampleOrientationInvariant<Qt::Horizontal> h;
    SampleOrientationInvariant<Qt::Vertical> v;

    QPoint point(10,20);

    UISE_TEST_CHECK_EQUAL(point.x(),h.oprop(point,OProp::pos));
    UISE_TEST_CHECK_EQUAL(point.y(),v.oprop(point,OProp::pos));
    UISE_TEST_CHECK_EQUAL(point.x(),v.oprop(point,OProp::pos,true));
    UISE_TEST_CHECK_EQUAL(point.y(),h.oprop(point,OProp::pos,true));
    UISE_TEST_CHECK_EQUAL(point.x(),h.oprop(&point,OProp::pos));
    UISE_TEST_CHECK_EQUAL(point.y(),v.oprop(&point,OProp::pos));
    UISE_TEST_CHECK_EQUAL(point.x(),v.oprop(&point,OProp::pos,true));
    UISE_TEST_CHECK_EQUAL(point.y(),h.oprop(&point,OProp::pos,true));

    std::vector<OProp> zeros{OProp::size,OProp::edge,OProp::min_size,OProp::max_size};
    for (auto&& prop : zeros)
    {
        UISE_TEST_CHECK_EQUAL(0,h.oprop(point,prop));
        UISE_TEST_CHECK_EQUAL(0,v.oprop(point,prop));
        UISE_TEST_CHECK_EQUAL(0,v.oprop(point,prop,true));
        UISE_TEST_CHECK_EQUAL(0,h.oprop(point,prop,true));
        UISE_TEST_CHECK_EQUAL(0,h.oprop(&point,prop));
        UISE_TEST_CHECK_EQUAL(0,v.oprop(&point,prop));
        UISE_TEST_CHECK_EQUAL(0,v.oprop(&point,prop,true));
        UISE_TEST_CHECK_EQUAL(0,h.oprop(&point,prop,true));
    }

    h.setOProp(point,OProp::pos,100);
    h.setOProp(point,OProp::pos,200,true);
    UISE_TEST_CHECK_EQUAL(point.x(),100);
    UISE_TEST_CHECK_EQUAL(point.y(),200);
    v.setOProp(point,OProp::pos,1000,true);
    v.setOProp(point,OProp::pos,2000);
    UISE_TEST_CHECK_EQUAL(point.x(),1000);
    UISE_TEST_CHECK_EQUAL(point.y(),2000);

    point.setX(0);
    point.setY(0);
    auto ptr=&point;
    h.setOProp(ptr,OProp::pos,100);
    h.setOProp(ptr,OProp::pos,200,true);
    UISE_TEST_CHECK_EQUAL(point.x(),100);
    UISE_TEST_CHECK_EQUAL(point.y(),200);
    v.setOProp(ptr,OProp::pos,1000,true);
    v.setOProp(ptr,OProp::pos,2000);
    UISE_TEST_CHECK_EQUAL(point.x(),1000);
    UISE_TEST_CHECK_EQUAL(point.y(),2000);

    point.setX(0);
    point.setY(0);
    for (auto&& prop : zeros)
    {
        h.setOProp(point,prop,10000);
        h.setOProp(point,prop,20000,true);
        UISE_TEST_CHECK_EQUAL(point.x(),0);
        UISE_TEST_CHECK_EQUAL(point.y(),0);
        v.setOProp(point,prop,10000,true);
        v.setOProp(point,prop,20000);
        UISE_TEST_CHECK_EQUAL(point.x(),0);
        UISE_TEST_CHECK_EQUAL(point.y(),0);

        auto ptr=&point;
        h.setOProp(ptr,prop,10000);
        h.setOProp(ptr,prop,20000,true);
        UISE_TEST_CHECK_EQUAL(point.x(),0);
        UISE_TEST_CHECK_EQUAL(point.y(),0);
        v.setOProp(ptr,prop,10000,true);
        v.setOProp(ptr,prop,20000);
        UISE_TEST_CHECK_EQUAL(point.x(),0);
        UISE_TEST_CHECK_EQUAL(point.y(),0);
    }
}

BOOST_AUTO_TEST_CASE(TestSize)
{
    SampleOrientationInvariant<Qt::Horizontal> h;
    SampleOrientationInvariant<Qt::Vertical> v;

    QSize size(10,20);

    std::vector<OProp> props{OProp::size,OProp::pos,OProp::edge,OProp::min_size,OProp::max_size};
    for (auto&& prop : props)
    {
        UISE_TEST_CHECK_EQUAL(size.width(),h.oprop(size,prop));
        UISE_TEST_CHECK_EQUAL(size.height(),v.oprop(size,prop));
        UISE_TEST_CHECK_EQUAL(size.width(),v.oprop(size,prop,true));
        UISE_TEST_CHECK_EQUAL(size.height(),h.oprop(size,prop,true));
        UISE_TEST_CHECK_EQUAL(size.width(),h.oprop(&size,prop));
        UISE_TEST_CHECK_EQUAL(size.height(),v.oprop(&size,prop));
        UISE_TEST_CHECK_EQUAL(size.width(),v.oprop(&size,prop,true));
        UISE_TEST_CHECK_EQUAL(size.height(),h.oprop(&size,prop,true));
    }

    for (auto&& prop : props)
    {
        size.setHeight(0);
        size.setWidth(0);

        h.setOProp(size,prop,100);
        h.setOProp(size,prop,200,true);
        UISE_TEST_CHECK_EQUAL(size.width(),100);
        UISE_TEST_CHECK_EQUAL(size.height(),200);
        v.setOProp(size,prop,1000,true);
        v.setOProp(size,prop,2000);
        UISE_TEST_CHECK_EQUAL(size.width(),1000);
        UISE_TEST_CHECK_EQUAL(size.height(),2000);

        auto ptr=&size;

        size.setHeight(0);
        size.setWidth(0);

        h.setOProp(ptr,prop,100);
        h.setOProp(ptr,prop,200,true);
        UISE_TEST_CHECK_EQUAL(size.width(),100);
        UISE_TEST_CHECK_EQUAL(size.height(),200);
        v.setOProp(ptr,prop,1000,true);
        v.setOProp(ptr,prop,2000);
        UISE_TEST_CHECK_EQUAL(size.width(),1000);
        UISE_TEST_CHECK_EQUAL(size.height(),2000);
    }
}

BOOST_AUTO_TEST_CASE(TestRect)
{
    SampleOrientationInvariant<Qt::Horizontal> h;
    SampleOrientationInvariant<Qt::Vertical> v;

    QRect rect(10,20,100,200);

    UISE_TEST_CHECK_EQUAL(rect.x(),h.oprop(rect,OProp::pos));
    UISE_TEST_CHECK_EQUAL(rect.y(),v.oprop(rect,OProp::pos));
    UISE_TEST_CHECK_EQUAL(rect.x(),v.oprop(rect,OProp::pos,true));
    UISE_TEST_CHECK_EQUAL(rect.y(),h.oprop(rect,OProp::pos,true));
    UISE_TEST_CHECK_EQUAL(rect.x(),h.oprop(&rect,OProp::pos));
    UISE_TEST_CHECK_EQUAL(rect.y(),v.oprop(&rect,OProp::pos));
    UISE_TEST_CHECK_EQUAL(rect.x(),v.oprop(&rect,OProp::pos,true));
    UISE_TEST_CHECK_EQUAL(rect.y(),h.oprop(&rect,OProp::pos,true));

    UISE_TEST_CHECK_EQUAL(rect.right(),h.oprop(rect,OProp::edge));
    UISE_TEST_CHECK_EQUAL(rect.bottom(),v.oprop(rect,OProp::edge));
    UISE_TEST_CHECK_EQUAL(rect.right(),v.oprop(rect,OProp::edge,true));
    UISE_TEST_CHECK_EQUAL(rect.bottom(),h.oprop(rect,OProp::edge,true));
    UISE_TEST_CHECK_EQUAL(rect.right(),h.oprop(&rect,OProp::edge));
    UISE_TEST_CHECK_EQUAL(rect.bottom(),v.oprop(&rect,OProp::edge));
    UISE_TEST_CHECK_EQUAL(rect.right(),v.oprop(&rect,OProp::edge,true));
    UISE_TEST_CHECK_EQUAL(rect.bottom(),h.oprop(&rect,OProp::edge,true));

    std::vector<OProp> sizeProps{OProp::size,OProp::min_size,OProp::max_size};
    for (auto&& prop : sizeProps)
    {
        UISE_TEST_CHECK_EQUAL(rect.width(),h.oprop(rect,prop));
        UISE_TEST_CHECK_EQUAL(rect.height(),v.oprop(rect,prop));
        UISE_TEST_CHECK_EQUAL(rect.width(),v.oprop(rect,prop,true));
        UISE_TEST_CHECK_EQUAL(rect.height(),h.oprop(rect,prop,true));
        UISE_TEST_CHECK_EQUAL(rect.width(),h.oprop(&rect,prop));
        UISE_TEST_CHECK_EQUAL(rect.height(),v.oprop(&rect,prop));
        UISE_TEST_CHECK_EQUAL(rect.width(),v.oprop(&rect,prop,true));
        UISE_TEST_CHECK_EQUAL(rect.height(),h.oprop(&rect,prop,true));
    }

    rect.setX(0);
    rect.setY(0);
    rect.setWidth(0);
    rect.setHeight(0);
    std::vector<OProp> zeros{OProp::edge};
    for (auto&& prop : zeros)
    {
        h.setOProp(rect,prop,10000);
        h.setOProp(rect,prop,20000,true);
        UISE_TEST_CHECK_EQUAL(rect.x(),0);
        UISE_TEST_CHECK_EQUAL(rect.y(),0);
        UISE_TEST_CHECK_EQUAL(rect.width(),0);
        UISE_TEST_CHECK_EQUAL(rect.height(),0);
        v.setOProp(rect,prop,10000,true);
        v.setOProp(rect,prop,20000);
        UISE_TEST_CHECK_EQUAL(rect.x(),0);
        UISE_TEST_CHECK_EQUAL(rect.y(),0);
        UISE_TEST_CHECK_EQUAL(rect.width(),0);
        UISE_TEST_CHECK_EQUAL(rect.height(),0);

        auto ptr=&rect;
        h.setOProp(ptr,prop,10000);
        h.setOProp(ptr,prop,20000,true);
        UISE_TEST_CHECK_EQUAL(rect.x(),0);
        UISE_TEST_CHECK_EQUAL(rect.y(),0);
        UISE_TEST_CHECK_EQUAL(rect.width(),0);
        UISE_TEST_CHECK_EQUAL(rect.height(),0);
        v.setOProp(ptr,prop,10000,true);
        v.setOProp(ptr,prop,20000);
        UISE_TEST_CHECK_EQUAL(rect.x(),0);
        UISE_TEST_CHECK_EQUAL(rect.y(),0);
        UISE_TEST_CHECK_EQUAL(rect.width(),0);
        UISE_TEST_CHECK_EQUAL(rect.height(),0);
    }

    h.setOProp(rect,OProp::pos,100);
    h.setOProp(rect,OProp::pos,200,true);
    UISE_TEST_CHECK_EQUAL(rect.x(),100);
    UISE_TEST_CHECK_EQUAL(rect.y(),200);
    v.setOProp(rect,OProp::pos,1000,true);
    v.setOProp(rect,OProp::pos,2000);
    UISE_TEST_CHECK_EQUAL(rect.x(),1000);
    UISE_TEST_CHECK_EQUAL(rect.y(),2000);

    rect.setX(0);
    rect.setY(0);
    auto ptr=&rect;
    h.setOProp(ptr,OProp::pos,100);
    h.setOProp(ptr,OProp::pos,200,true);
    UISE_TEST_CHECK_EQUAL(rect.x(),100);
    UISE_TEST_CHECK_EQUAL(rect.y(),200);
    v.setOProp(ptr,OProp::pos,1000,true);
    v.setOProp(ptr,OProp::pos,2000);
    UISE_TEST_CHECK_EQUAL(rect.x(),1000);
    UISE_TEST_CHECK_EQUAL(rect.y(),2000);

    for (auto&& prop : sizeProps)
    {
        rect.setX(0);
        rect.setY(0);
        rect.setWidth(0);
        rect.setHeight(0);

        h.setOProp(rect,prop,100);
        h.setOProp(rect,prop,200,true);
        UISE_TEST_CHECK_EQUAL(rect.width(),100);
        UISE_TEST_CHECK_EQUAL(rect.height(),200);
        v.setOProp(rect,prop,1000,true);
        v.setOProp(rect,prop,2000);
        UISE_TEST_CHECK_EQUAL(rect.width(),1000);
        UISE_TEST_CHECK_EQUAL(rect.height(),2000);

        auto ptr=&rect;

        rect.setHeight(0);
        rect.setWidth(0);

        h.setOProp(ptr,prop,100);
        h.setOProp(ptr,prop,200,true);
        UISE_TEST_CHECK_EQUAL(rect.width(),100);
        UISE_TEST_CHECK_EQUAL(rect.height(),200);
        v.setOProp(ptr,prop,1000,true);
        v.setOProp(ptr,prop,2000);
        UISE_TEST_CHECK_EQUAL(rect.width(),1000);
        UISE_TEST_CHECK_EQUAL(rect.height(),2000);
    }
}

BOOST_AUTO_TEST_CASE(TestWidget)
{
    auto handler=[]()
    {
        SampleOrientationInvariant<Qt::Horizontal> h;
        SampleOrientationInvariant<Qt::Vertical> v;

        auto widget=new QWidget();

        widget->resize(100,200);

        widget->setMinimumWidth(80);
        widget->setMinimumHeight(180);
        widget->setMaximumWidth(1000);
        widget->setMaximumHeight(3000);

        widget->move(10,20);

        UISE_TEST_CHECK_EQUAL(widget->width(),100);
        UISE_TEST_CHECK_EQUAL(widget->height(),200);
        UISE_TEST_CHECK_EQUAL(widget->x(),10);
        UISE_TEST_CHECK_EQUAL(widget->y(),20);

        UISE_TEST_CHECK_EQUAL(widget->x(),h.oprop(widget,OProp::pos));
        UISE_TEST_CHECK_EQUAL(widget->y(),v.oprop(widget,OProp::pos));
        UISE_TEST_CHECK_EQUAL(widget->x(),v.oprop(widget,OProp::pos,true));
        UISE_TEST_CHECK_EQUAL(widget->y(),h.oprop(widget,OProp::pos,true));

        UISE_TEST_CHECK_EQUAL(widget->geometry().right(),h.oprop(widget,OProp::edge));
        UISE_TEST_CHECK_EQUAL(widget->geometry().bottom(),v.oprop(widget,OProp::edge));
        UISE_TEST_CHECK_EQUAL(widget->geometry().right(),v.oprop(widget,OProp::edge,true));
        UISE_TEST_CHECK_EQUAL(widget->geometry().bottom(),h.oprop(widget,OProp::edge,true));

        UISE_TEST_CHECK_EQUAL(widget->width(),h.oprop(widget,OProp::size));
        UISE_TEST_CHECK_EQUAL(widget->height(),v.oprop(widget,OProp::size));
        UISE_TEST_CHECK_EQUAL(widget->width(),v.oprop(widget,OProp::size,true));
        UISE_TEST_CHECK_EQUAL(widget->height(),h.oprop(widget,OProp::size,true));

        UISE_TEST_CHECK_EQUAL(widget->minimumWidth(),h.oprop(widget,OProp::min_size));
        UISE_TEST_CHECK_EQUAL(widget->minimumHeight(),v.oprop(widget,OProp::min_size));
        UISE_TEST_CHECK_EQUAL(widget->minimumWidth(),v.oprop(widget,OProp::min_size,true));
        UISE_TEST_CHECK_EQUAL(widget->minimumHeight(),h.oprop(widget,OProp::min_size,true));

        UISE_TEST_CHECK_EQUAL(widget->maximumWidth(),h.oprop(widget,OProp::max_size));
        UISE_TEST_CHECK_EQUAL(widget->maximumHeight(),v.oprop(widget,OProp::max_size));
        UISE_TEST_CHECK_EQUAL(widget->maximumWidth(),v.oprop(widget,OProp::max_size,true));
        UISE_TEST_CHECK_EQUAL(widget->maximumHeight(),h.oprop(widget,OProp::max_size,true));

        h.setOProp(widget,OProp::min_size,50);
        h.setOProp(widget,OProp::min_size,60,true);
        UISE_TEST_CHECK_EQUAL(widget->minimumWidth(),50);
        UISE_TEST_CHECK_EQUAL(widget->minimumHeight(),60);
        v.setOProp(widget,OProp::min_size,70,true);
        v.setOProp(widget,OProp::min_size,80);
        UISE_TEST_CHECK_EQUAL(widget->minimumWidth(),70);
        UISE_TEST_CHECK_EQUAL(widget->minimumHeight(),80);

        h.setOProp(widget,OProp::max_size,500);
        h.setOProp(widget,OProp::max_size,600,true);
        UISE_TEST_CHECK_EQUAL(widget->maximumWidth(),500);
        UISE_TEST_CHECK_EQUAL(widget->maximumHeight(),600);
        v.setOProp(widget,OProp::max_size,700,true);
        v.setOProp(widget,OProp::max_size,800);
        UISE_TEST_CHECK_EQUAL(widget->maximumWidth(),700);
        UISE_TEST_CHECK_EQUAL(widget->maximumHeight(),800);

        std::vector<OProp> zeros{OProp::pos,OProp::size,OProp::edge};
        for (auto&& prop : zeros)
        {
            h.setOProp(widget,prop,300);
            h.setOProp(widget,prop,400,true);
            UISE_TEST_CHECK_EQUAL(widget->x(),10);
            UISE_TEST_CHECK_EQUAL(widget->y(),20);
            UISE_TEST_CHECK_EQUAL(widget->width(),100);
            UISE_TEST_CHECK_EQUAL(widget->height(),200);

            v.setOProp(widget,prop,300,true);
            v.setOProp(widget,prop,400);
            UISE_TEST_CHECK_EQUAL(widget->x(),10);
            UISE_TEST_CHECK_EQUAL(widget->y(),20);
            UISE_TEST_CHECK_EQUAL(widget->width(),100);
            UISE_TEST_CHECK_EQUAL(widget->height(),200);
        }

        delete widget;
        TestThread::instance()->continueTest();
    };

    TestThread::instance()->postGuiThread(handler);
    TestThread::instance()->execTest();
}

BOOST_AUTO_TEST_SUITE_END()

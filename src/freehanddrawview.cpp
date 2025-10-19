/**
@copyright Evgeny Sidorov 2022

This software is dual-licensed. Choose the appropriate license for your project.

1. The GNU GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-GPLv3.md](LICENSE-GPLv3.md) or copy at https://www.gnu.org/licenses/gpl-3.0.txt)
    
2. The GNU LESSER GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-LGPLv3.md](LICENSE-LGPLv3.md) or copy at https://www.gnu.org/licenses/lgpl-3.0.txt).

You may select, at your option, one of the above-listed licenses.

*/

/****************************************************************************/

/** @file uise/desktop/freehanddrawview.cpp
*
*  Defines FreeHanDrawView.
*
*/

/****************************************************************************/

#include <QMouseEvent>

#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPathItem>

#include <uise/desktop/freehanddrawview.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

/********************* SimpleImageEditorWidget *****************************/

//--------------------------------------------------------------------------

FreeHandDrawView::FreeHandDrawView(QWidget *parent)
    : QGraphicsView(parent),
      m_currentPathItem(nullptr),
      m_freeHandDrawEnabled(false),
      m_penColor(Qt::black),
      m_penWidth(2)
{
    setMouseTracking(true);
}

//--------------------------------------------------------------------------

void FreeHandDrawView::mousePressEvent(QMouseEvent *event)
{
    if (!m_freeHandDrawEnabled)
    {
        QGraphicsView::mousePressEvent(event);
        return;
    }

    if (event->button() == Qt::LeftButton)
    {
        // create a new path item when drawing starts
        QPen pen(m_penColor, m_penWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        m_currentPathItem = new QGraphicsPathItem();
        m_currentPathItem->setPen(pen);
        scene()->addItem(m_currentPathItem);

        // start the painter path
        m_currentPath.moveTo(mapToScene(event->pos()));
    }
    QGraphicsView::mousePressEvent(event);
}

//--------------------------------------------------------------------------

void FreeHandDrawView::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_freeHandDrawEnabled)
    {
        QGraphicsView::mouseMoveEvent(event);
        return;
    }

    if (event->buttons() & Qt::LeftButton && m_currentPathItem)
    {
        // add the current mouse position to the path
        m_currentPath.lineTo(mapToScene(event->pos()));
        m_currentPathItem->setPath(m_currentPath);
    }
    QGraphicsView::mouseMoveEvent(event);
}

//--------------------------------------------------------------------------

void FreeHandDrawView::mouseReleaseEvent(QMouseEvent *event)
{
    if (!m_freeHandDrawEnabled)
    {
        QGraphicsView::mouseReleaseEvent(event);
        return;
    }

    if (event->button() == Qt::LeftButton && m_currentPathItem)
    {
        m_undoStack.push(m_currentPathItem);

        m_currentPathItem = nullptr;
        m_currentPath = QPainterPath();
    }
    QGraphicsView::mouseReleaseEvent(event);
}

//--------------------------------------------------------------------------

void FreeHandDrawView::acceptHandDraw()
{
    m_undoStack.clear();
    m_redoStack.clear();
}

//--------------------------------------------------------------------------

void FreeHandDrawView::cancelHandDraw()
{
    while (!m_undoStack.isEmpty())
    {
        auto item=m_undoStack.pop();
        scene()->removeItem(item);
        delete item;
    }
    m_redoStack.clear();
}

//--------------------------------------------------------------------------

void FreeHandDrawView::undoHandDraw()
{
    if (!m_undoStack.isEmpty())
    {
        auto item=m_undoStack.pop();
        scene()->removeItem(item);
        m_redoStack.push(item);
    }
}

//--------------------------------------------------------------------------

void FreeHandDrawView::redoHandDraw()
{
    if (!m_redoStack.isEmpty())
    {
        auto item=m_redoStack.pop();
        scene()->addItem(item);
        m_undoStack.push(item);
    }
}

//--------------------------------------------------------------------------

void FreeHandDrawView::setFreeHandDrawEnabled(bool value)
{
    m_freeHandDrawEnabled=value;
    if (!value)
    {
        cancelHandDraw();
    }
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END

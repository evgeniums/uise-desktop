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

/** @file uise/desktop/src/autoresizingtextedit.cpp
*
*  Defines AutoResizingTextEdit.
*
*/

/****************************************************************************/

#include <QKeyEvent>
#include <QAbstractTextDocumentLayout>
#include <QtMath>

#include <uise/desktop/autoresizingtextedit.hpp>

namespace uise {

//--------------------------------------------------------------------------

AutoResizingTextEdit::AutoResizingTextEdit(QWidget* parent)
    : QTextEdit(parent),
      m_maxLines(DefaultMaxLines),
      m_returnInsertsNewLine(true)
{
    // The value held here is plain text (see EditableLabelTraits<TextEdit>), never rich text.
    setAcceptRichText(false);

    // Tab moves to the next field rather than indenting -- this is a compact form field, not a
    // composer with its own list/indent handling (contrast EnhancedTextEdit, which reclaims Tab
    // for that). Same choice VoiceRecorderDialog's comment editor makes (voicerecorderdialog.cpp).
    setTabChangesFocus(true);

    // Below the line cap the widget is sized exactly to its content, so a scrollbar would have
    // nothing to scroll and would only steal width. Restored to AsNeeded once the cap starts
    // biting, in updateSize().
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    // QAbstractScrollArea's constructor sets Expanding in both directions, which would let a
    // layout with leftover vertical space stretch this widget past sizeHint() rather than
    // stopping at it (verified against Qt source -- see EnhancedTextEdit::setExpandedEnabled()'s
    // doc comment in messageeditor.cpp for the same reasoning). Fixed makes sizeHint() the only
    // acceptable height.
    auto policy=sizePolicy();
    policy.setVerticalPolicy(QSizePolicy::Fixed);
    setSizePolicy(policy);

    // documentSizeChanged rather than textChanged: it also fires when the WIDTH changes (first
    // show, host resize), which is exactly when re-wrapping changes the document height without
    // any edit to provoke a textChanged. textChanged alone would miss that case.
    connect(
        document()->documentLayout(),
        &QAbstractTextDocumentLayout::documentSizeChanged,
        this,
        [this](const QSizeF&)
        {
            updateSize();
        }
    );

    updateSize();
}

//--------------------------------------------------------------------------

int AutoResizingTextEdit::verticalChrome() const
{
    // The real top+bottom insets around the viewport, NOT frameWidth()*2: under a stylesheet
    // QFrame::frameWidth() is the MAX of the four per-side border+padding insets
    // (QFramePrivate::updateStyledFrameWidths()), so an asymmetric box -- e.g. a bottom-only
    // border, as whitemdesktop's panels use -- is over-counted, and the editor ends up taller
    // than the EditableLabel text it replaces. QFrame stores the per-side insets as its contents
    // margins, which is exactly what QAbstractScrollArea lays the viewport out inside.
    const auto margins=contentsMargins();
    const auto viewport=viewportMargins();
    return margins.top()+margins.bottom()+viewport.top()+viewport.bottom();
}

//--------------------------------------------------------------------------

int AutoResizingTextEdit::contentHeight() const
{
    return static_cast<int>(document()->size().height())+verticalChrome();
}

//--------------------------------------------------------------------------

int AutoResizingTextEdit::maxContentHeight() const
{
    auto height=static_cast<int>(
        qCeil(fontMetrics().lineSpacing()*m_maxLines + 2*document()->documentMargin())
    ) + verticalChrome();

    // A QSS "max-height" rule is already a real setMaximumHeight() on this widget
    // (QStyleSheetStyle::setGeometry()) -- honour it rather than fighting it, the same way
    // EnhancedTextEdit::effectiveMaxHeight() does for the message composer.
    const auto qssMax=maximumHeight();
    if (qssMax<QWIDGETSIZE_MAX)
    {
        height=qMin(height,qssMax);
    }

    return height;
}

//--------------------------------------------------------------------------

void AutoResizingTextEdit::setMaxLines(int lines)
{
    m_maxLines=qMax(lines,1);
    updateGeometry();
    updateSize();
}

//--------------------------------------------------------------------------

void AutoResizingTextEdit::updateSize()
{
    updateGeometry();

    // Cannot oscillate: showing the bar narrows the text area and makes the document taller, so
    // it stays over the cap; hiding it widens the text area and makes the document shorter, so it
    // stays under. Same pattern as EnhancedTextEdit::updateSize().
    setVerticalScrollBarPolicy(
        contentHeight()>maxContentHeight() ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff
    );
}

//--------------------------------------------------------------------------

QSize AutoResizingTextEdit::sizeHint() const
{
    return QSize(QTextEdit::sizeHint().width(),qMin(contentHeight(),maxContentHeight()));
}

//--------------------------------------------------------------------------

QSize AutoResizingTextEdit::minimumSizeHint() const
{
    auto hint=QTextEdit::minimumSizeHint();
    hint.setHeight(sizeHint().height());
    return hint;
}

//--------------------------------------------------------------------------

void AutoResizingTextEdit::keyPressEvent(QKeyEvent* event)
{
    if (!m_returnInsertsNewLine
        && (event->key()==Qt::Key_Return || event->key()==Qt::Key_Enter)
        && !(event->modifiers() & Qt::ShiftModifier))
    {
        emit returnPressed();
        event->accept();
        return;
    }

    QTextEdit::keyPressEvent(event);
}

//--------------------------------------------------------------------------

void AutoResizingTextEdit::changeEvent(QEvent* event)
{
    QTextEdit::changeEvent(event);

    if (event->type()==QEvent::FontChange || event->type()==QEvent::StyleChange)
    {
        updateSize();
    }
}

//--------------------------------------------------------------------------

}

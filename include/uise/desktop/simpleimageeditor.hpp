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

/** @file uise/desktop/simpleimageeditor.hpp
*
*  Declares SimpleImageEditor.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_SIMPLE_IMAGE_EDITOR_HPP
#define UISE_DESKTOP_SIMPLE_IMAGE_EDITOR_HPP

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/abstractimageeditor.hpp>

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

class SimpleImageEditorWidget;

class UISE_DESKTOP_EXPORT SimpleImageEditor : public AbstractImageEditor
{
    Q_OBJECT

    public:

        using AbstractImageEditor::AbstractImageEditor;

        QPixmap editedImage() override;

    public slots:

        void zoomIn() override;
        void zoomOut() override;
        void flipVertical() override;
        void flipHorizontal() override;
        void rotate() override;
        void rotateClockwise() override;

        virtual void setFreeHandDrawMode(bool enable) override;

        void acceptFreeHandDraw();
        void cancelFreeHandDraw();

    protected:

        void updateCropShape() override;

        void updateCropEnabled() override;

        void updateCropButtonState() override;

        void updateImageSizeLimits() override;

        void updateAspectRatio() override;

        void updateCropFrameMode() override;

        void doLoadImage() override;

        void updateFilenameState() override;

        Widget* doCreateActualWidget(QWidget* parent) override;

    private:

        void doUpdateFilenameState();
        void resetCropper();
        void destroyCropper();

        //! Called after rotate()/rotateClockwise()/flipHorizontal()/flipVertical(). In fixed-on-
        //! screen mode the frame must stay put through a rotate/flip (per this mode's own contract),
        //! so this re-derives the crop rect's scene-space projection and zoom/pan limits in place
        //! instead of calling resetCropper(), which would rebuild (and recentre) the frame. Legacy
        //! mode is unchanged -- it still calls resetCropper(), exactly as before this feature.
        void resetCropperOrKeepFrame();

        //! Repaints the crop rect (if any) in place after a view-level zoom/pan change -- see the
        //! definition's own doc for why this replaces resetCropper() on those paths.
        void refreshCropperForViewChange();

        //! Reacts to CropRectItem::frameChanged() (fixed-on-screen mode) -- pushes the frame's new
        //! viewport rectangle into GraphicsViewZoom::setCoverRect(), reclamps the current zoom
        //! against it, and re-derives the pan bounds. Also called directly wherever the frame or
        //! image can change without frameChanged() firing (crop enable/disable, mode switch).
        void updateZoomLimitsForCropper();

        //! Sets/clears the view's own sceneRect() override that bounds panning -- in fixed-on-screen
        //! mode with an active cropper, expands the image's scene rect outward by the (scene-unit)
        //! margin between the crop frame and the viewport edges, so QGraphicsView's native
        //! scrollbar clamping keeps the image covering the frame; otherwise resets the override so
        //! the view falls back to tracking the scene's own sceneRect(), as before this feature.
        void updateViewBounds();

        SimpleImageEditorWidget* m_widget;
};

class SimpleImageEditorWidget_p;
class UISE_DESKTOP_EXPORT SimpleImageEditorWidget : public WidgetQFrame
{
    Q_OBJECT

    public:

        SimpleImageEditorWidget(SimpleImageEditor* ctrl, QWidget* parent=nullptr);

        ~SimpleImageEditorWidget();
        SimpleImageEditorWidget(const SimpleImageEditorWidget&)=delete;
        SimpleImageEditorWidget(SimpleImageEditorWidget&&)=delete;
        SimpleImageEditorWidget& operator=(const SimpleImageEditorWidget&)=delete;
        SimpleImageEditorWidget& operator=(SimpleImageEditorWidget&&)=delete;

    private:

        std::unique_ptr<SimpleImageEditorWidget_p> pimpl;

        friend class SimpleImageEditor;
};

}

#endif // UISE_DESKTOP_SIMPLE_IMAGE_EDITOR_HPP

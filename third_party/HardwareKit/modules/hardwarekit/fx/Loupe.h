#pragma once

/*  HardwareKit loupe: a fisheye magnifier that re-renders the scene zoomed in around a point, instead of
    stretching pixels. Usage per frame while it is shown:

        target.ensureSize (2 * radiusPx, 2 * radiusPx);
        target.bind();                          // render the scene with the zoomed view-projection
        drawScene (Loupe::zoomedViewProj (camera.viewProj, anchorNdcX, anchorNdcY, zoom, radiusPx, vw, vh));
        RenderTarget::unbind(); glViewport (...);
        target.bindColour (0);                  // then draw a screen quad with shaders::library::magnifierLens
*/
namespace hwk::fx
{
    struct Loupe
    {
        /** View-projection for a square render target that covers a lens of `radiusPx` centred on the anchor,
            showing the scene `zoom` times larger than on screen. */
        static gfx::Mat4 zoomedViewProj (const gfx::Mat4& viewProj, float anchorNdcX, float anchorNdcY, float zoom,
                                         float radiusPx, int viewportW, int viewportH) noexcept
        {
            // Clip-space pre-transform: move the anchor to the centre, then scale so that
            // radiusPx / zoom screen pixels fill the target's half-width.
            const float sx = zoom * (float) viewportW / (2.0f * radiusPx);
            const float sy = zoom * (float) viewportH / (2.0f * radiusPx);
            auto m = gfx::Mat4::identity();
            m.at (0, 0) = sx; m.at (0, 3) = -sx * anchorNdcX;
            m.at (1, 1) = sy; m.at (1, 3) = -sy * anchorNdcY;
            return m * viewProj;
        }
    };
}

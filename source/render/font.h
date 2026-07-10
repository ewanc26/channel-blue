/*
 * Font rendering via FreeType → GX texture pipeline.
 *
 * Renders TrueType glyphs into GX IA8 textures and draws them as
 * textured quads. Glyphs are cached after first render to avoid
 * repeated FreeType calls.
 *
 * Call font_init() once after GX is configured. Then use font_draw_text()
 * during the render loop. Call font_shutdown() before exit.
 */

#ifndef CB_FONT_H
#define CB_FONT_H

#include <gccore.h>

/* font sizes available */
typedef enum {
    FONT_SIZE_TAB_BAR = 20,
    FONT_SIZE_HEADER  = 18,
    FONT_SIZE_HINTS   = 14,
    FONT_SIZE_COUNT
} font_size_id_t;

/*
 * font_init — load the embedded font and set up FreeType + glyph cache.
 *
 * Must be called after gx_init() but before any font_draw_text() calls.
 * Returns 0 on success, -1 on failure.
 */
int font_init(void);

/*
 * font_shutdown — release FreeType resources and glyph cache.
 */
void font_shutdown(void);

/*
 * font_draw_text — draw a null-terminated ASCII string at (x, y).
 *
 * Draws using the specified font size and GXColor.
 * Returns the total pixel width of the rendered text.
 * Only supports ASCII printable characters (32-126); others are skipped.
 */
int font_draw_text(f32 x, f32 y, const char *text, font_size_id_t size,
                   GXColor color);

/*
 * font_text_width — measure the pixel width of a string without drawing.
 *
 * Useful for centre-aligning text.
 */
int font_text_width(const char *text, font_size_id_t size);

#endif /* CB_FONT_H */

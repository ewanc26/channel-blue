/*
 * Image decoding and GX texture upload.
 *
 * Decodes JPEG and PNG image data from memory into GX-ready textures.
 * Supports runtime decoding of network-received images (avatars, embeds).
 *
 * Usage:
 *   1. Call image_init() once after GX is configured.
 *   2. Call image_decode() with raw JPEG/PNG data to get a decoded_image_t.
 *   3. Call image_draw() to render the decoded image as a textured quad.
 *   4. Call image_free() to release the decoded image resources.
 */

#ifndef CB_IMAGE_H
#define CB_IMAGE_H

#include <gccore.h>
#include <stdint.h>

/* maximum image dimensions to prevent excessive memory use on Wii */
#define IMAGE_MAX_WIDTH  640
#define IMAGE_MAX_HEIGHT 480
/* Keep compressed-image expansion bounded before any downsampling pass. */
#define IMAGE_MAX_SOURCE_WIDTH  (IMAGE_MAX_WIDTH * 8)
#define IMAGE_MAX_SOURCE_HEIGHT (IMAGE_MAX_HEIGHT * 8)
#define IMAGE_MAX_SOURCE_PIXELS (4u * 1024u * 1024u)

/* decoded image — holds GX-ready texture data */
typedef struct {
    u32 *texture_data;     /* GX_TF_RGBA8 texture data, memalign(32,...) */
    u16  texture_width;    /* tile-aligned texture width */
    u16  texture_height;   /* tile-aligned texture height */
    u16  image_width;      /* original image width */
    u16  image_height;     /* original image height */
    u32  data_size;        /* size of texture_data allocation in bytes */
} decoded_image_t;

/*
 * image_init — initialize the image decoding subsystem.
 *
 * Must be called after GX is configured but before any decode/draw calls.
 * Returns 0 on success, -1 on failure.
 */
int image_init(void);

/*
 * image_shutdown — release all resources used by the image subsystem.
 */
void image_shutdown(void);

/*
 * image_decode — decode raw JPEG or PNG data into a GX texture.
 *
 * Detects format from magic bytes (JPEG: 0xFFD8, PNG: 0x89504E47).
 * Output is GX_TF_RGBA8 format, tile-aligned, ready for GX_LoadTexObj().
 *
 * @param data       Raw image data (JPEG or PNG encoded)
 * @param data_size  Size of data in bytes
 * @param max_width  Maximum width to decode to (downscale if larger)
 * @param max_height Maximum height to decode to (downscale if larger)
 * @param out        Output decoded image (caller must image_free() when done)
 * @return 0 on success, -1 on failure
 */
int image_decode(const u8 *data, u32 data_size,
                 u16 max_width, u16 max_height,
                 decoded_image_t *out);

/*
 * image_draw — render a decoded image as a textured quad at (x, y).
 *
 * Uses GX_VTXFMT1 (position + color + texcoord).
 * The image is drawn at its original decoded size.
 * Restores position-only vertex format after drawing.
 */
void image_draw(f32 x, f32 y, const decoded_image_t *img, GXColor color);

/*
 * image_draw_scaled — render a decoded image scaled to (draw_w, draw_h).
 *
 * Like image_draw but scales the image to the specified dimensions.
 */
void image_draw_scaled(f32 x, f32 y, f32 draw_w, f32 draw_h,
                       const decoded_image_t *img, GXColor color);

/*
 * image_free — release the texture data allocated by image_decode().
 */
void image_free(decoded_image_t *img);

#endif /* CB_IMAGE_H */

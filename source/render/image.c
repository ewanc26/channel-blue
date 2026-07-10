/*
 * Image decoding implementation.
 *
 * Decodes JPEG (via libjpeg-turbo) and PNG (via libpng) image data
 * from memory into GX_TF_RGBA8 textures with proper tile alignment.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <setjmp.h>
#include <gccore.h>

#include <jpeglib.h>
#include <png.h>

#include "image.h"

/* ---- JPEG error handler ---- */

struct cb_jpeg_error_mgr {
    struct jpeg_error_mgr pub;
    jmp_buf setjmp_buffer;
};

static void jpeg_error_exit(j_common_ptr cinfo) {
    struct cb_jpeg_error_mgr *err = (struct cb_jpeg_error_mgr *)cinfo->err;
    longjmp(err->setjmp_buffer, 1);
}

/* ---- PNG memory reader ---- */

typedef struct {
    const u8 *data;
    u32       size;
    u32       offset;
} png_mem_reader_t;

static void png_mem_read(png_structp png_ptr, png_bytep out_bytes,
                          png_size_t count) {
    png_mem_reader_t *r = (png_mem_reader_t *)png_get_io_ptr(png_ptr);
    if (r->offset + (u32)count > r->size) {
        png_error(png_ptr, "Read past end of PNG data");
        return;
    }
    memcpy(out_bytes, r->data + r->offset, count);
    r->offset += count;
}

/* ---- GX tile conversion ---- */

static u16 align_to_4(u16 dim) {
    return (dim + 3) & ~3;
}

static u32 *rgba_to_gx_rgba8(const u8 *rgba, u16 width, u16 height,
                              u16 *out_w, u16 *out_h) {
    u16 tw = align_to_4(width);
    u16 th = align_to_4(height);

    u32 tile_row_bytes = (tw / 4) * 128;
    u32 total = tile_row_bytes * (th / 4);

    u32 *gx_data = (u32 *)memalign(32, total);
    if (!gx_data) return NULL;
    memset(gx_data, 0, total);

    u8 *dst = (u8 *)gx_data;

    for (u16 ty = 0; ty < th; ty += 4) {
        for (u16 tx = 0; tx < tw; tx += 4) {
            u16 hi_block[16];
            u16 lo_block[16];

            for (u16 row = 0; row < 4; row++) {
                for (u16 col = 0; col < 4; col++) {
                    u16 px = tx + col;
                    u16 py = ty + row;
                    u16 idx = row * 4 + col;

                    if (px < width && py < height) {
                        u32 off = (py * width + px) * 4;
                        u8 r = rgba[off + 0];
                        u8 g = rgba[off + 1];
                        u8 b = rgba[off + 2];
                        u8 a = rgba[off + 3];
                        hi_block[idx] = (u16)((a << 8) | r);
                        lo_block[idx] = (u16)((g << 8) | b);
                    } else {
                        hi_block[idx] = 0;
                        lo_block[idx] = 0;
                    }
                }
            }

            memcpy(dst, hi_block, 32);
            dst += 32;
            memcpy(dst, lo_block, 32);
            dst += 32;
        }
    }

    DCFlushRange(gx_data, total);

    *out_w = tw;
    *out_h = th;
    return gx_data;
}

/* ---- JPEG decoding ---- */

static int decode_jpeg(const u8 *data, u32 data_size,
                       u16 max_w, u16 max_h,
                       decoded_image_t *out) {
    struct jpeg_decompress_struct cinfo;
    struct cb_jpeg_error_mgr jerr;

    cinfo.err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = jpeg_error_exit;

    if (setjmp(jerr.setjmp_buffer)) {
        jpeg_destroy_decompress(&cinfo);
        return -1;
    }

    jpeg_create_decompress(&cinfo);
    jpeg_mem_src(&cinfo, data, data_size);
    jpeg_read_header(&cinfo, TRUE);

    if (cinfo.image_width > max_w * 8 || cinfo.image_height > max_h * 8)
        cinfo.scale_denom = 8;
    else if (cinfo.image_width > max_w * 4 || cinfo.image_height > max_h * 4)
        cinfo.scale_denom = 4;
    else if (cinfo.image_width > max_w * 2 || cinfo.image_height > max_h * 2)
        cinfo.scale_denom = 2;
    else
        cinfo.scale_denom = 1;

    cinfo.out_color_space = JCS_RGB;
    jpeg_start_decompress(&cinfo);

    u16 w = cinfo.output_width;
    u16 h = cinfo.output_height;

    /* decode to RGB first, then expand to RGBA */
    u8 *rgb = (u8 *)memalign(32, (u32)w * h * 3);
    if (!rgb) {
        jpeg_destroy_decompress(&cinfo);
        return -1;
    }

    while (cinfo.output_scanline < h) {
        u8 *row = rgb + cinfo.output_scanline * w * 3;
        jpeg_read_scanlines(&cinfo, &row, 1);
    }

    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);

    /* expand RGB → RGBA */
    u8 *rgba = (u8 *)memalign(32, (u32)w * h * 4);
    if (!rgba) {
        free(rgb);
        return -1;
    }

    for (u32 i = 0; i < (u32)w * h; i++) {
        rgba[i * 4 + 0] = rgb[i * 3 + 0];
        rgba[i * 4 + 1] = rgb[i * 3 + 1];
        rgba[i * 4 + 2] = rgb[i * 3 + 2];
        rgba[i * 4 + 3] = 0xFF;
    }
    free(rgb);

    u16 tex_w, tex_h;
    u32 *gx = rgba_to_gx_rgba8(rgba, w, h, &tex_w, &tex_h);
    free(rgba);

    if (!gx) return -1;

    out->texture_data   = gx;
    out->texture_width  = tex_w;
    out->texture_height = tex_h;
    out->image_width    = w;
    out->image_height   = h;
    out->data_size      = (tex_w / 4) * (tex_h / 4) * 128;

    return 0;
}

/* ---- PNG decoding ---- */

static int decode_png(const u8 *data, u32 data_size,
                      u16 max_w, u16 max_h,
                      decoded_image_t *out) {
    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING,
                                              NULL, NULL, NULL);
    if (!png) return -1;

    png_infop info = png_create_info_struct(png);
    if (!info) {
        png_destroy_read_struct(&png, NULL, NULL);
        return -1;
    }

    if (setjmp(png_jmpbuf(png))) {
        png_destroy_read_struct(&png, &info, NULL);
        return -1;
    }

    png_mem_reader_t reader;
    reader.data   = data;
    reader.size   = data_size;
    reader.offset = 0;
    png_set_read_fn(png, &reader, png_mem_read);

    png_read_info(png, info);

    u32 orig_w = png_get_image_width(png, info);
    u32 orig_h = png_get_image_height(png, info);
    png_byte color_type = png_get_color_type(png, info);
    png_byte bit_depth = png_get_bit_depth(png, info);

    if (bit_depth == 16)
        png_set_strip_16(png);
    if (color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_palette_to_rgb(png);
    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
        png_set_expand_gray_1_2_4_to_8(png);
    if (png_get_valid(png, info, PNG_INFO_tRNS))
        png_set_tRNS_to_alpha(png);
    if (color_type == PNG_COLOR_TYPE_RGB ||
        color_type == PNG_COLOR_TYPE_GRAY ||
        color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_filler(png, 0xFF, PNG_FILLER_AFTER);
    if (color_type == PNG_COLOR_TYPE_GRAY ||
        color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_gray_to_rgb(png);

    png_read_update_info(png, info);

    u8 *full_rgba = (u8 *)memalign(32, orig_w * orig_h * 4);
    if (!full_rgba) {
        png_destroy_read_struct(&png, &info, NULL);
        return -1;
    }

    png_bytep *row_pointers = (png_bytep *)malloc(orig_h * sizeof(png_bytep));
    if (!row_pointers) {
        free(full_rgba);
        png_destroy_read_struct(&png, &info, NULL);
        return -1;
    }

    for (u32 y = 0; y < orig_h; y++)
        row_pointers[y] = full_rgba + y * orig_w * 4;

    png_read_image(png, row_pointers);
    free(row_pointers);
    png_destroy_read_struct(&png, &info, NULL);

    u16 w = orig_w;
    u16 h = orig_h;

    while (w > max_w || h > max_h) {
        u16 nw = w / 2;
        u16 nh = h / 2;
        if (nw < 1) nw = 1;
        if (nh < 1) nh = 1;

        u8 *small = (u8 *)memalign(32, (u32)nw * nh * 4);
        if (!small) { free(full_rgba); return -1; }

        for (u16 dy = 0; dy < nh; dy++) {
            for (u16 dx = 0; dx < nw; dx++) {
                u32 src_x = dx * w / nw;
                u32 src_y = dy * h / nh;
                u32 si = (src_y * w + src_x) * 4;
                u32 di = (dy * nw + dx) * 4;
                small[di + 0] = full_rgba[si + 0];
                small[di + 1] = full_rgba[si + 1];
                small[di + 2] = full_rgba[si + 2];
                small[di + 3] = full_rgba[si + 3];
            }
        }

        free(full_rgba);
        full_rgba = small;
        w = nw;
        h = nh;
    }

    u16 tex_w, tex_h;
    u32 *gx = rgba_to_gx_rgba8(full_rgba, w, h, &tex_w, &tex_h);
    free(full_rgba);

    if (!gx) return -1;

    out->texture_data   = gx;
    out->texture_width  = tex_w;
    out->texture_height = tex_h;
    out->image_width    = w;
    out->image_height   = h;
    out->data_size      = (tex_w / 4) * (tex_h / 4) * 128;

    return 0;
}

/* ---- GX texture drawing helpers ---- */

static void setup_textured_vtx_fmt(void) {
    GX_SetVtxDesc(GX_VA_POS,  GX_DIRECT);
    GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
    GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);

    GX_SetVtxAttrFmt(GX_VTXFMT1, GX_VA_POS,  GX_POS_XY,  GX_S16,   0);
    GX_SetVtxAttrFmt(GX_VTXFMT1, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
    GX_SetVtxAttrFmt(GX_VTXFMT1, GX_VA_TEX0, GX_TEX_ST,  GX_F32,   0);
}

static void restore_position_only_vtx_fmt(void) {
    GX_ClearVtxDesc();
    GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XY, GX_F32, 0);
    GX_SetNumTexGens(0);
    GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORDNULL, GX_TEXMAP_NULL, GX_COLORNULL);
    GX_SetTevOp(GX_TEVSTAGE0, GX_PASSCLR);
}

/* ---- public API ---- */

int image_init(void) {
    return 0;
}

void image_shutdown(void) {
}

int image_decode(const u8 *data, u32 data_size,
                 u16 max_width, u16 max_height,
                 decoded_image_t *out) {
    if (!data || data_size < 4 || !out) return -1;
    memset(out, 0, sizeof(*out));

    if (data[0] == 0xFF && data[1] == 0xD8 && data[2] == 0xFF)
        return decode_jpeg(data, data_size, max_width, max_height, out);

    if (data[0] == 0x89 && data[1] == 0x50 && data[2] == 0x4E && data[3] == 0x47)
        return decode_png(data, data_size, max_width, max_height, out);

    return -1;
}

void image_draw(f32 x, f32 y, const decoded_image_t *img, GXColor color) {
    if (!img || !img->texture_data) return;
    image_draw_scaled(x, y, (f32)img->image_width, (f32)img->image_height,
                      img, color);
}

void image_draw_scaled(f32 x, f32 y, f32 draw_w, f32 draw_h,
                       const decoded_image_t *img, GXColor color) {
    if (!img || !img->texture_data) return;

    setup_textured_vtx_fmt();

    GX_SetNumTexGens(1);
    GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
    GX_SetTevOp(GX_TEVSTAGE0, GX_MODULATE);
    GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
    GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_PNMTX0);

    GXTexObj tex_obj;
    GX_InitTexObj(&tex_obj, img->texture_data,
                  img->texture_width, img->texture_height,
                  GX_TF_RGBA8, GX_CLAMP, GX_CLAMP, GX_FALSE);
    GX_LoadTexObj(&tex_obj, GX_TEXMAP0);
    GX_InvalidateTexAll();

    s16 sx = (s16)x;
    s16 sy = (s16)y;
    s16 sw = (s16)draw_w;
    s16 sh = (s16)draw_h;

    GX_Begin(GX_QUADS, GX_VTXFMT1, 4);

    GX_Position2s16(sx,      sy);
    GX_Color4u8(color.r, color.g, color.b, color.a);
    GX_TexCoord2f32(0.0f, 0.0f);

    GX_Position2s16(sx + sw, sy);
    GX_Color4u8(color.r, color.g, color.b, color.a);
    GX_TexCoord2f32(1.0f, 0.0f);

    GX_Position2s16(sx + sw, sy + sh);
    GX_Color4u8(color.r, color.g, color.b, color.a);
    GX_TexCoord2f32(1.0f, 1.0f);

    GX_Position2s16(sx,      sy + sh);
    GX_Color4u8(color.r, color.g, color.b, color.a);
    GX_TexCoord2f32(0.0f, 1.0f);

    GX_End();

    restore_position_only_vtx_fmt();
}

void image_free(decoded_image_t *img) {
    if (!img) return;
    if (img->texture_data) {
        free(img->texture_data);
        img->texture_data = NULL;
    }
    img->texture_width  = 0;
    img->texture_height = 0;
    img->data_size      = 0;
}

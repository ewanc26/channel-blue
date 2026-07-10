/*
 * Channel Blue — Bluesky client for the Nintendo Wii
 *
 * Bootstrap: GX rendering pipeline, double-buffered display, Wiimote input.
 * Draws a solid Bluesky-blue rectangle to prove the pipeline works.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <gccore.h>
#include <wiiuse/wpad.h>

/* 256 KB GX command FIFO — generous for a 2D text-heavy UI */
#define GX_FIFO_SIZE (256 * 1024)

/* Bluesky brand colour: #1d9bf0 */
#define BSKY_BLUE_R 0x1d
#define BSKY_BLUE_G 0x9b
#define BSKY_BLUE_B 0xf0

static void *frameBuffer[2] = {NULL, NULL};
static GXRModeObj *rmode = NULL;

/*
 * GX vertex positions for a full-screen quad.
 * orthographic projection maps screen pixel coords directly,
 * so these match the 640x480 framebuffer dimensions.
 */
static f32 quadVertices[] ATTRIBUTE_ALIGN(32) = {
    /* top-left     */  0.0f,   0.0f,
    /* top-right    */ 640.0f,   0.0f,
    /* bottom-right */ 640.0f, 480.0f,
    /* bottom-left  */   0.0f, 480.0f,
};

/* index buffer for a single quad (two triangles) */
static u16 quadIndices[] ATTRIBUTE_ALIGN(32) = {
    0, 1, 2,
    0, 2, 3,
};

/*
 * gx_init — set up the GX rendering pipeline for 2D drawing.
 *
 * This configures the embedded framebuffer, viewport, orthographic projection,
 * and a vertex descriptor that accepts direct f32 positions with no tex coords.
 */
static void gx_init(void) {
    void *fifoBuffer = MEM_K0_TO_K1(memalign(32, GX_FIFO_SIZE));
    memset(fifoBuffer, 0, GX_FIFO_SIZE);
    GX_Init(fifoBuffer, GX_FIFO_SIZE);

    /* clear colour: black */
    GXColor background = {0, 0, 0, 255};
    GX_SetCopyClear(background, 0x00ffffff);

    /* viewport + display copy setup */
    GX_SetViewport(0, 0, rmode->fbWidth, rmode->efbHeight, 0, 1);
    f32 yscale = GX_GetYScaleFactor(rmode->efbHeight, rmode->xfbHeight);
    u32 xfbHeight = GX_SetDispCopyYScale(yscale);
    GX_SetScissor(0, 0, rmode->fbWidth, rmode->efbHeight);
    GX_SetDispCopySrc(0, 0, rmode->fbWidth, rmode->efbHeight);
    GX_SetDispCopyDst(rmode->fbWidth, xfbHeight);
    GX_SetCopyFilter(rmode->aa, rmode->sample_pattern, GX_TRUE, rmode->vfilter);
    GX_SetFieldMode(rmode->field_rendering,
                    (rmode->viHeight == 2 * rmode->xfbHeight) ? GX_ENABLE : GX_DISABLE);

    if (rmode->aa)
        GX_SetPixelFmt(GX_PF_RGB565_Z16, GX_ZC_LINEAR);
    else
        GX_SetPixelFmt(GX_PF_RGB8_Z24, GX_ZC_LINEAR);

    GX_SetCullMode(GX_CULL_NONE);
    GX_CopyDisp(frameBuffer[0], GX_TRUE);
    GX_SetDispCopyGamma(GX_GM_1_0);

    /* 2D orthographic projection — pixel coordinates map 1:1 */
    Mtx44 ortho;
    guOrtho(ortho, 0, rmode->efbHeight, 0, rmode->fbWidth, 0, 300);
    GX_LoadProjectionMtx(ortho, GX_ORTHOGRAPHIC);

    /* vertex descriptor: direct f32 XY positions, no tex coords */
    GX_ClearVtxDesc();
    GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XY, GX_F32, 0);
    GX_SetNumChans(1);
    GX_SetNumTexGens(0);
    GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORDNULL, GX_TEXMAP_NULL, GX_COLORNULL);
    GX_SetTevOp(GX_TEVSTAGE0, GX_PASSCLR);
}

/*
 * draw_blue_quad — draw a Bluesky-blue quad covering the full screen.
 *
 * The model-view matrix is set to identity with a small Z translation,
 * then 4 vertices are emitted as a triangle list.
 */
static void draw_blue_quad(void) {
    Mtx modelView;
    guMtxIdentity(modelView);
    guMtxTransApply(modelView, modelView, 0.0f, 0.0f, -5.0f);
    GX_LoadPosMtxImm(modelView, GX_PNMTX0);

    /*
     * TODO(fx/gx): replace with a real UI — post cards, header bar, scroll cursor.
     * For now this just proves the pipeline produces visible output.
     */
    GX_Begin(GX_TRIANGLES, GX_VTXFMT0, 6);
        GX_Position3f32(quadVertices[0],  quadVertices[1],  0.0f);
        GX_Position3f32(quadVertices[2],  quadVertices[3],  0.0f);
        GX_Position3f32(quadVertices[4],  quadVertices[5],  0.0f);
        GX_Position3f32(quadVertices[0],  quadVertices[1],  0.0f);
        GX_Position3f32(quadVertices[4],  quadVertices[5],  0.0f);
        GX_Position3f32(quadVertices[6],  quadVertices[7],  0.0f);
    GX_End();
}

int main(int argc, char **argv) {
    /* --- video init --- */
    VIDEO_Init();
    WPAD_Init();

    rmode = VIDEO_GetPreferredMode(NULL);

    frameBuffer[0] = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
    frameBuffer[1] = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));

    VIDEO_Configure(rmode);
    VIDEO_SetNextFramebuffer(frameBuffer[0]);
    VIDEO_SetBlack(false);
    VIDEO_Flush();
    VIDEO_WaitVSync();
    if (rmode->viTVMode & VI_NON_INTERLACE) VIDEO_WaitVSync();

    /* --- gx init --- */
    gx_init();

    u32 fb = 0; /* current framebuffer index */

    /* --- main loop --- */
    while (SYS_MainLoop()) {
        WPAD_ScanPads();

        u32 pressed = WPAD_ButtonsDown(0);
        if (pressed & WPAD_BUTTON_HOME) exit(0);

        /* draw this frame */
        GX_SetViewport(0, 0, rmode->fbWidth, rmode->efbHeight, 0, 1);
        GX_InvVtxCache();
        GX_InvalidateTexAll();
        draw_blue_quad();

        /* present */
        GX_DrawDone();
        fb ^= 1;
        GX_SetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
        GX_SetColorUpdate(GX_TRUE);
        GX_CopyDisp(frameBuffer[fb], GX_TRUE);
        VIDEO_SetNextFramebuffer(frameBuffer[fb]);
        VIDEO_Flush();
        VIDEO_WaitVSync();
    }

    return 0;
}

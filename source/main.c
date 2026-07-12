/*
 * Channel Blue — Bluesky client for the Nintendo Wii
 *
 * Bootstrap: GX rendering pipeline, double-buffered display, Wiimote input,
 * navigation framework with tab bar, header, and screen stack.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <gccore.h>
#include <wiiuse/wpad.h>
#include <wiikeyboard/keyboard.h>
#include <fat.h>
#include <sdcard/wiisd_io.h>
#include <ogc/lwp.h>
#include <ogc/system.h>
#include <wolfram/platform.h>
#include <wolfram/wii.h>

#include "navigation/nav.h"
#include "render/palette.h"
#include "app/auth.h"
#include "app/timeline.h"
#include "app/compose.h"
#include "app/login.h"
#include "app/input.h"
#include "app/entropy_seed.h"
#include "integration/wolfram_backend.h"
#include "render/font.h"
#include "render/image.h"
#include "render/texcache.h"

/* 256 KB GX command FIFO — generous for a 2D text-heavy UI */
#define GX_FIFO_SIZE (256 * 1024)
#define CB_ENTROPY_SEED_PATH "sd:/apps/channel-blue/entropy.bin"

static void *frameBuffer[2] = {NULL, NULL};
static GXRModeObj *rmode = NULL;
static unsigned char network_stack[64 * 1024] ATTRIBUTE_ALIGN(32);
static volatile int network_init_done;
static volatile wf_status network_init_status = WF_ERR_NETWORK;
static volatile int entropy_init_ready;
static int startup_sd_mounted;

static int provision_wolfram_entropy(void);

static void *network_init_thread(void *unused) {
    (void)unused;
    if (startup_sd_mounted)
        entropy_init_ready = provision_wolfram_entropy();
    network_init_status = wf_platform_init();
    __sync_synchronize();
    network_init_done = 1;
    return NULL;
}

static void keyboard_keypress(char symbol) {
    nav_handle_key((unsigned char)symbol);
}

static int provision_wolfram_entropy(void) {
    unsigned char seed[CB_ENTROPY_SEED_SIZE];
    wf_status status;

    if (!cb_entropy_seed_load(CB_ENTROPY_SEED_PATH, seed)) return 0;
    status = wf_wii_set_entropy_seed(seed, sizeof(seed));
    memset(seed, 0, sizeof(seed));
    if (status != WF_OK) return 0;
    status = wf_wii_rotate_entropy_seed(seed, sizeof(seed));
    if (status != WF_OK) return 0;
    if (!cb_entropy_seed_save(CB_ENTROPY_SEED_PATH, seed)) {
        memset(seed, 0, sizeof(seed));
        return 0;
    }
    memset(seed, 0, sizeof(seed));
    return wf_wii_commit_entropy_rotation() == WF_OK;
}

/*
 * gx_init — set up the GX rendering pipeline for 2D drawing.
 *
 * Configures the embedded framebuffer, viewport, orthographic projection,
 * and a vertex descriptor that accepts direct f32 positions with no tex coords.
 */
static void gx_init(void) {
    void *fifoBuffer = MEM_K0_TO_K1(memalign(32, GX_FIFO_SIZE));
    memset(fifoBuffer, 0, GX_FIFO_SIZE);
    GX_Init(fifoBuffer, GX_FIFO_SIZE);

    /* clear colour: black */
    GXColor background = CB_COLOR_SURFACE_DEEP;
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
    GX_SetDispCopyGamma(GX_GM_1_0);

    /* 2D orthographic projection — pixel coordinates map 1:1 */
    Mtx44 ortho;
    guOrtho(ortho, 0, rmode->efbHeight, 0, rmode->fbWidth, 0, 300);
    GX_LoadProjectionMtx(ortho, GX_ORTHOGRAPHIC);

    /* vertex descriptor: direct f32 XY positions, no tex coords */
    GX_ClearVtxDesc();
    GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
    GX_SetNumChans(1);
    GX_SetChanCtrl(GX_COLOR0A0, GX_DISABLE, GX_SRC_REG, GX_SRC_REG,
                   GX_LIGHTNULL, GX_DF_NONE, GX_AF_NONE);
    GX_SetNumTexGens(0);
    GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORDNULL, GX_TEXMAP_NULL, GX_COLOR0A0);
    GX_SetTevOp(GX_TEVSTAGE0, GX_PASSCLR);
}

/*
 * gx_begin_frame — prepare GX state for a new frame of drawing.
 */
static void gx_begin_frame(void) {
    GX_SetViewport(0, 0, rmode->fbWidth, rmode->efbHeight, 0, 1);
    GX_InvVtxCache();
    GX_InvalidateTexAll();

    /* set model-view to identity with slight Z offset */
    Mtx modelView;
    guMtxIdentity(modelView);
    guMtxTransApply(modelView, modelView, 0.0f, 0.0f, -5.0f);
    GX_LoadPosMtxImm(modelView, GX_PNMTX0);
}

int main(int argc, char **argv) {
    int sd_mounted;
    cb_wolfram_context wolfram;
    cb_auth auth;
    cb_auth_backend auth_backend;
    cb_timeline_backend backend;
    cb_login_form login;
    lwp_t network_thread = LWP_THREAD_NULL;
    int resume_attempted = 0;
    cb_input_repeat input_repeat;

    /* --- video init --- */
    VIDEO_Init();
    WPAD_Init();
    KEYBOARD_Init(keyboard_keypress);
    sd_mounted = fatMountSimple("sd", &__io_wiisd);
    startup_sd_mounted = sd_mounted;
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

    /* --- font init --- */
    if (font_init() < 0) {
        /* font init failed — continue with placeholder rendering */
    }

    /* --- image init --- */
    image_init();
    texcache_init();

    /* --- navigation init --- */
    nav_init();
    cb_timeline timeline;
    cb_compose compose;
    cb_notifications notifications;
    cb_search search;
    cb_profile profile;
    cb_thread thread;
    cb_wolfram_context_init(&wolfram);
    cb_auth_init(&auth);
    auth_backend = cb_wolfram_auth_backend();
    backend = cb_wolfram_timeline_backend();
    cb_notifications_backend notes_backend = cb_wolfram_notifications_backend();
    cb_search_backend search_backend = cb_wolfram_search_backend();
    cb_profile_backend profile_backend = cb_wolfram_profile_backend();
    cb_thread_backend threadb = cb_wolfram_thread_backend();
    cb_timeline_init(&timeline);
    cb_compose_init(&compose);
    cb_login_form_init(&login);
    cb_notifications_init(&notifications);
    cb_search_init(&search);
    cb_profile_init(&profile);
    cb_thread_init(&thread);
    cb_input_repeat_init(&input_repeat);
    nav_bind_timeline(&timeline, &compose, &backend, &wolfram);
    nav_bind_thread(&thread, &threadb, &wolfram);
    nav_bind_discovery(&notifications, &search, &profile, &notes_backend,
                       &search_backend, &profile_backend, &wolfram);
    nav_bind_auth(&auth, &login, &auth_backend,
                  "sd:/apps/channel-blue/session.dat");
    if (LWP_CreateThread(&network_thread, network_init_thread, NULL,
                         network_stack, sizeof(network_stack), 64) != LWP_SUCCESSFUL) {
        network_init_done = 1;
        network_init_status = WF_ERR_NETWORK;
    }

    u32 fb = 0; /* current framebuffer index */

    /* --- main loop --- */
	while (SYS_MainLoop() && !nav_exit_requested()) {
        if (network_init_done && !resume_attempted) {
            __sync_synchronize();
            resume_attempted = 1;
            cb_wolfram_context_set_network_ready(
                &wolfram, network_init_status == WF_OK && entropy_init_ready);
            if (!entropy_init_ready) {
                login.last_status = CB_APP_CONFIGURATION;
            } else if (network_init_status == WF_OK && sd_mounted) {
                if (cb_auth_resume(&auth, &auth_backend, &wolfram,
                                   "sd:/apps/channel-blue/session.dat") == CB_APP_OK)
                    nav_bind_auth(&auth, &login, &auth_backend,
                                  "sd:/apps/channel-blue/session.dat");
            } else if (network_init_status != WF_OK) {
                login.last_status = CB_APP_NETWORK;
            }
        }
        WPAD_ScanPads();

        WPADData *wpad = WPAD_Data(0);

        expansion_t expansion;
        u32 raw_pressed = WPAD_ButtonsDown(0);
        u32 pressed;
        memset(&expansion, 0, sizeof(expansion));
        WPAD_Expansion(0, &expansion);
        pressed = cb_input_translate(
            raw_pressed, expansion.type == WPAD_EXP_CLASSIC,
            expansion.type == WPAD_EXP_CLASSIC ? expansion.classic.ljs.mag : 0.0f,
            expansion.type == WPAD_EXP_CLASSIC ? expansion.classic.ljs.ang : 0.0f,
            &input_repeat);
        nav_pointer_update(wpad && wpad->ir.valid ? wpad->ir.x : 0.0f,
                           wpad && wpad->ir.valid ? wpad->ir.y : 0.0f,
                           wpad && wpad->ir.valid,
                           (raw_pressed & WPAD_BUTTON_A) != 0);
        /* Pointer A clicks are dispatched by nav_pointer_update so they do
         * not also trigger the button action for the current screen. */
        if (!(wpad && wpad->ir.valid && (raw_pressed & WPAD_BUTTON_A)))
            nav_handle_input(pressed);

        /* draw this frame */
        texcache_begin_frame();
        gx_begin_frame();
        nav_render();

        /* present */
        GX_DrawDone();
        fb ^= 1;
        GX_SetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
        GX_SetColorUpdate(GX_TRUE);
        GX_CopyDisp(frameBuffer[fb], GX_TRUE);
        GX_Flush();
        VIDEO_SetNextFramebuffer(frameBuffer[fb]);
        VIDEO_Flush();
        VIDEO_WaitVSync();
    }

	texcache_shutdown();
	image_shutdown();
	if (network_thread != LWP_THREAD_NULL && network_init_done)
		LWP_JoinThread(network_thread, NULL);
	cb_timeline_free(&timeline);
    cb_compose_free(&compose);
    cb_notifications_free(&notifications);
    cb_search_free(&search);
    cb_profile_free(&profile);
    cb_thread_free(&thread);
    cb_auth_free(&auth);
    cb_wolfram_context_free(&wolfram);
    if (network_init_done && network_init_status == WF_OK)
        wf_platform_shutdown();
    if (sd_mounted) fatUnmount("sd");
    KEYBOARD_Deinit();
	font_shutdown();
	if (nav_exit_requested())
		SYS_ResetSystem(SYS_RETURNTOMENU, 0, 0);

	return 0;
}

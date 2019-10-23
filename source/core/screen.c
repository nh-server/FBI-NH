#include <errno.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>

#include <3ds.h>
#include <citro3d.h>

#include "error.h"
#include "screen.h"
#include "../libs/stb_image/stb_image.h"

#include "default_shbin.h"

static bool c3d_initialized;

static bool shader_initialized;
static DVLB_s* dvlb;
static shaderProgram_s program;

static C3D_RenderTarget* target_top;
static C3D_RenderTarget* target_bottom;
static C3D_Mtx projection_top;
static C3D_Mtx projection_bottom;

static C3D_Tex* glyph_sheets;
static u32 glyph_count;
static float font_scale;

static u8 base_alpha = 0xFF;

static u32 color_config[MAX_COLORS] = {0xFF000000};

static struct {
    bool allocated;
    C3D_Tex tex;
    u32 width;
    u32 height;
} textures[MAX_TEXTURES];

static void screen_set_blend(u32 color, bool rgb, bool alpha) {
    C3D_TexEnv* env = C3D_GetTexEnv(0);
    if(env == NULL) {
        error_panic("Failed to retrieve combiner settings.");
        return;
    }

    C3D_TexEnvInit(env);

    if(rgb) {
        C3D_TexEnvSrc(env, C3D_RGB, GPU_CONSTANT, GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR);
        C3D_TexEnvFunc(env, C3D_RGB, GPU_REPLACE);
    } else {
        C3D_TexEnvSrc(env, C3D_RGB, GPU_TEXTURE0, GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR);
        C3D_TexEnvFunc(env, C3D_RGB, GPU_REPLACE);
    }

    if(alpha) {
        C3D_TexEnvSrc(env, C3D_Alpha, GPU_TEXTURE0, GPU_CONSTANT, GPU_PRIMARY_COLOR);
        C3D_TexEnvFunc(env, C3D_Alpha, GPU_MODULATE);
    } else {
        C3D_TexEnvSrc(env, C3D_Alpha, GPU_TEXTURE0, GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR);
        C3D_TexEnvFunc(env, C3D_Alpha, GPU_REPLACE);
    }

    C3D_TexEnvColor(env, color);
}

void screen_init() {
    if(!C3D_Init(C3D_DEFAULT_CMDBUF_SIZE * 4)) {
        error_panic("Failed to initialize the GPU.");
        return;
    }

    c3d_initialized = true;

    u32 displayFlags = GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) | GX_TRANSFER_RAW_COPY(0) | GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGB8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) | GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO);

    target_top = C3D_RenderTargetCreate(TOP_SCREEN_HEIGHT, TOP_SCREEN_WIDTH, GPU_RB_RGB8, 0);
    if(target_top == NULL) {
        error_panic("Failed to initialize the top screen target.");
        return;
    }

    C3D_RenderTargetSetOutput(target_top, GFX_TOP, GFX_LEFT, displayFlags);

    target_bottom = C3D_RenderTargetCreate(BOTTOM_SCREEN_HEIGHT, BOTTOM_SCREEN_WIDTH, GPU_RB_RGB8, 0);
    if(target_bottom == NULL) {
        error_panic("Failed to initialize the bottom screen target.");
        return;
    }

    C3D_RenderTargetSetOutput(target_bottom, GFX_BOTTOM, GFX_LEFT, displayFlags);

    Mtx_OrthoTilt(&projection_top, 0.0, TOP_SCREEN_WIDTH, TOP_SCREEN_HEIGHT, 0.0, 0.0, 1.0, true);
    Mtx_OrthoTilt(&projection_bottom, 0.0, BOTTOM_SCREEN_WIDTH, BOTTOM_SCREEN_HEIGHT, 0.0, 0.0, 1.0, true);

    dvlb = DVLB_ParseFile((u32*) default_shbin, default_shbin_len);
    if(dvlb == NULL) {
        error_panic("Failed to parse shader.");
        return;
    }

    Result progInitRes = shaderProgramInit(&program);
    if(R_FAILED(progInitRes)) {
        error_panic("Failed to initialize shader program: 0x%08lX", progInitRes);
        return;
    }

    shader_initialized = true;

    Result progSetVshRes = shaderProgramSetVsh(&program, &dvlb->DVLE[0]);
    if(R_FAILED(progSetVshRes)) {
        error_panic("Failed to set up vertex shader: 0x%08lX", progInitRes);
        return;
    }

    C3D_BindProgram(&program);

    C3D_AttrInfo* attrInfo = C3D_GetAttrInfo();
    if(attrInfo == NULL) {
        error_panic("Failed to retrieve attribute info.");
        return;
    }

    AttrInfo_Init(attrInfo);
    AttrInfo_AddLoader(attrInfo, 0, GPU_FLOAT, 3);
    AttrInfo_AddLoader(attrInfo, 1, GPU_FLOAT, 2);

    C3D_DepthTest(true, GPU_GEQUAL, GPU_WRITE_ALL);

    screen_set_blend(0, false, false);

    Result fontMapRes = fontEnsureMapped();
    if(R_FAILED(fontMapRes)) {
        error_panic("Failed to map system font: 0x%08lX", fontMapRes);
        return;
    }

    TGLP_s* glyphInfo = fontGetGlyphInfo(NULL);

    glyph_count = glyphInfo->nSheets;
    glyph_sheets = calloc(glyph_count, sizeof(C3D_Tex));
    if(glyph_sheets == NULL) {
        error_panic("Failed to allocate font glyph texture data.");
        return;
    }

    for(int i = 0; i < glyph_count; i++) {
        C3D_Tex* tex = &glyph_sheets[i];
        tex->data = fontGetGlyphSheetTex(NULL, i);
        tex->fmt = (GPU_TEXCOLOR) glyphInfo->sheetFmt;
        tex->size = glyphInfo->sheetSize;
        tex->width = glyphInfo->sheetWidth;
        tex->height = glyphInfo->sheetHeight;
        tex->param = GPU_TEXTURE_MAG_FILTER(GPU_LINEAR) | GPU_TEXTURE_MIN_FILTER(GPU_LINEAR) | GPU_TEXTURE_WRAP_S(GPU_CLAMP_TO_EDGE) | GPU_TEXTURE_WRAP_T(GPU_CLAMP_TO_EDGE);
    }

    font_scale = 30.0f / glyphInfo->cellHeight; // 30 is cellHeight in J machines
}

void screen_exit() {
    for(u32 id = 0; id < MAX_TEXTURES; id++) {
        screen_unload_texture(id);
    }

    if(glyph_sheets != NULL) {
        free(glyph_sheets);
        glyph_sheets = NULL;
    }

    if(shader_initialized) {
        shaderProgramFree(&program);
        shader_initialized = false;
    }

    if(dvlb != NULL) {
        DVLB_Free(dvlb);
        dvlb = NULL;
    }

    if(target_top != NULL) {
        C3D_RenderTargetDelete(target_top);
        target_top = NULL;
    }

    if(target_bottom != NULL) {
        C3D_RenderTargetDelete(target_bottom);
        target_bottom = NULL;
    }

    if(c3d_initialized) {
        C3D_Fini();
        c3d_initialized = false;
    }
}

void screen_set_base_alpha(u8 alpha) {
    base_alpha = alpha;
}

void screen_set_color(u32 id, u32 color) {
    if(id >= MAX_COLORS) {
        error_panic("Attempted to draw string with invalid color ID \"%lu\".", id);
        return;
    }

    color_config[id] = color;
}

static u32 screen_next_pow_2(u32 i) {
    i--;
    i |= i >> 1;
    i |= i >> 2;
    i |= i >> 4;
    i |= i >> 8;
    i |= i >> 16;
    i++;

    return i;
}

u32 screen_allocate_free_texture() {
    u32 id = 0;
    for(u32 i = 1; i < MAX_TEXTURES; i++) {
        if(!textures[i].allocated) {
            textures[i].allocated = true;

            id = i;
            break;
        }
    }

    if(id == 0) {
        error_panic("Out of free textures.");
        return 0;
    }

    return id;
}

static void screen_prepare_texture(u32* pow2WidthOut, u32* pow2HeightOut, u32 id, u32 width, u32 height, GPU_TEXCOLOR format, bool linearFilter) {
    if(id >= MAX_TEXTURES) {
        error_panic("Attempted to prepare invalid texture ID \"%lu\".", id);
        return;
    }

    u32 pow2Width = screen_next_pow_2(width);
    if(pow2Width < 64) {
        pow2Width = 64;
    }

    u32 pow2Height = screen_next_pow_2(height);
    if(pow2Height < 64) {
        pow2Height = 64;
    }

    if(textures[id].tex.data != NULL && (textures[id].tex.width != pow2Width || textures[id].tex.height != pow2Height || textures[id].tex.fmt != format)) {
        C3D_TexDelete(&textures[id].tex);
        textures[id].tex.data = NULL;
    }

    if(textures[id].tex.data == NULL && !C3D_TexInit(&textures[id].tex, (u16) pow2Width, (u16) pow2Height, format)) {
        error_panic("Failed to initialize texture with ID \"%lu\".", id);
        return;
    }

    C3D_TexSetFilter(&textures[id].tex, linearFilter ? GPU_LINEAR : GPU_NEAREST, GPU_NEAREST);

    textures[id].allocated = true;
    textures[id].width = width;
    textures[id].height = height;

    if(pow2WidthOut != NULL) {
        *pow2WidthOut = pow2Width;
    }

    if(pow2HeightOut != NULL) {
        *pow2HeightOut = pow2Height;
    }
}

void screen_load_texture_tiled(u32 id, void* data, u32 size, u32 width, u32 height, GPU_TEXCOLOR format, bool linearFilter) {
    u32 pow2Width = 0;
    u32 pow2Height = 0;
    screen_prepare_texture(&pow2Width, &pow2Height, id, width, height, format, linearFilter);

    if(width != pow2Width || height != pow2Height) {
        u32 pixelSize = size / width / height;

        memset(textures[id].tex.data, 0, textures[id].tex.size);
        for(u32 y = 0; y < height; y += 8) {
            u32 dstPos = y * pow2Width * pixelSize;
            u32 srcPos = y * width * pixelSize;

            memcpy(&((u8*) textures[id].tex.data)[dstPos], &((u8*) data)[srcPos], width * 8 * pixelSize);
        }
    } else {
        memcpy(textures[id].tex.data, data, textures[id].tex.size);
    }

    C3D_TexFlush(&textures[id].tex);
}

void screen_load_texture_untiled(u32 id, void* data, u32 size, u32 width, u32 height, GPU_TEXCOLOR format, bool linearFilter) {
    u32 pow2Width = 0;
    u32 pow2Height = 0;
    screen_prepare_texture(&pow2Width, &pow2Height, id, width, height, format, linearFilter);

    u32 pixelSize = size / width / height;

    memset(textures[id].tex.data, 0, textures[id].tex.size);
    for(u32 x = 0; x < width; x++) {
        for(u32 y = 0; y < height; y++) {
            u32 dstPos = ((((y >> 3) * (pow2Width >> 3) + (x >> 3)) << 6) + ((x & 1) | ((y & 1) << 1) | ((x & 2) << 1) | ((y & 2) << 2) | ((x & 4) << 2) | ((y & 4) << 3))) * pixelSize;
            u32 srcPos = (y * width + x) * pixelSize;

            memcpy(&((u8*) textures[id].tex.data)[dstPos], &((u8*) data)[srcPos], pixelSize);
        }
    }

    C3D_TexFlush(&textures[id].tex);
}

void screen_load_texture_path(u32 id, const char* path, bool linearFilter) {
    if(id >= MAX_TEXTURES) {
        error_panic("Attempted to load path \"%s\" to invalid texture ID \"%lu\".", path, id);
        return;
    }

    FILE* fd = fopen(path, "rb");
    if(fd == NULL) {
        error_panic("Failed to load PNG file \"%s\": %s", path, strerror(errno));
        return;
    }

    screen_load_texture_file(id, fd, linearFilter);

    fclose(fd);
}

void screen_load_texture_file(u32 id, FILE* fd, bool linearFilter) {
    if(id >= MAX_TEXTURES) {
        error_panic("Attempted to load file to invalid texture ID \"%lu\".", id);
        return;
    }

    int width;
    int height;
    int depth;
    u8* image = stbi_load_from_file(fd, &width, &height, &depth, STBI_rgb_alpha);

    if(image == NULL) {
        error_panic("Failed to load PNG file to texture ID \"%lu\".", id);
        return;
    }

    for(u32 x = 0; x < width; x++) {
        for(u32 y = 0; y < height; y++) {
            u32 pos = (y * width + x) * 4;

            u8 c1 = image[pos + 0];
            u8 c2 = image[pos + 1];
            u8 c3 = image[pos + 2];
            u8 c4 = image[pos + 3];

            image[pos + 0] = c4;
            image[pos + 1] = c3;
            image[pos + 2] = c2;
            image[pos + 3] = c1;
        }
    }

    screen_load_texture_untiled(id, image, (u32) (width * height * 4), (u32) width, (u32) height, GPU_RGBA8, linearFilter);

    free(image);
}

void screen_unload_texture(u32 id) {
    if(id >= MAX_TEXTURES) {
        error_panic("Attempted to unload invalid texture ID \"%lu\".", id);
        return;
    }

    C3D_TexDelete(&textures[id].tex);
    textures[id].tex.data = NULL;

    textures[id].allocated = false;
    textures[id].width = 0;
    textures[id].height = 0;
}

void screen_get_texture_size(u32* width, u32* height, u32 id) {
    if(id >= MAX_TEXTURES) {
        error_panic("Attempted to get size of invalid texture ID \"%lu\".", id);
        return;
    }

    if(width) {
        *width = textures[id].width;
    }

    if(height) {
        *height = textures[id].height;
    }
}

void screen_begin_frame() {
    if(!C3D_FrameBegin(C3D_FRAME_SYNCDRAW)) {
        error_panic("Failed to begin frame.");
        return;
    }
}

void screen_end_frame() {
    C3D_FrameEnd(0);
}

void screen_select(gfxScreen_t screen) {
    C3D_RenderTarget* target = screen == GFX_TOP ? target_top : target_bottom;

    C3D_RenderTargetClear(target, C3D_CLEAR_ALL, 0, 0);
    if(!C3D_FrameDrawOn(target)) {
        error_panic("Failed to select render target.");
        return;
    }

    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, shaderInstanceGetUniformLocation(program.vertexShader, "projection"), screen == GFX_TOP ? &projection_top : &projection_bottom);
}

static void screen_draw_quad(float x1, float y1, float x2, float y2, float left, float bottom, float right, float top) {
    C3D_ImmDrawBegin(GPU_TRIANGLE_STRIP);

    C3D_ImmSendAttrib(x1, y2, 0.5f, 0.0f);
    C3D_ImmSendAttrib(left, bottom, 0.0f, 0.0f);

    C3D_ImmSendAttrib(x2, y2, 0.5f, 0.0f);
    C3D_ImmSendAttrib(right, bottom, 0.0f, 0.0f);

    C3D_ImmSendAttrib(x1, y1, 0.5f, 0.0f);
    C3D_ImmSendAttrib(left, top, 0.0f, 0.0f);

    C3D_ImmSendAttrib(x2, y1, 0.5f, 0.0f);
    C3D_ImmSendAttrib(right, top, 0.0f, 0.0f);

    C3D_ImmDrawEnd();
}

void screen_draw_texture(u32 id, float x, float y, float width, float height) {
    if(id >= MAX_TEXTURES) {
        error_panic("Attempted to draw invalid texture ID \"%lu\".", id);
        return;
    }

    if(textures[id].tex.data == NULL) {
        return;
    }

    if(base_alpha != 0xFF) {
        screen_set_blend(base_alpha << 24, false, true);
    }

    C3D_TexBind(0, &textures[id].tex);
    screen_draw_quad(x, y, x + width, y + height, 0, (float) (textures[id].tex.height - textures[id].height) / (float) textures[id].tex.height, (float) textures[id].width / (float) textures[id].tex.width, 1.0f);

    if(base_alpha != 0xFF) {
        screen_set_blend(0, false, false);
    }
}

void screen_draw_texture_crop(u32 id, float x, float y, float width, float height) {
    if(id >= MAX_TEXTURES) {
        error_panic("Attempted to draw invalid texture ID \"%lu\".", id);
        return;
    }

    if(textures[id].tex.data == NULL) {
        return;
    }

    if(base_alpha != 0xFF) {
        screen_set_blend(base_alpha << 24, false, true);
    }

    C3D_TexBind(0, &textures[id].tex);
    screen_draw_quad(x, y, x + width, y + height, 0, (float) (textures[id].tex.height - textures[id].height) / (float) textures[id].tex.height, width / (float) textures[id].tex.width, (textures[id].tex.height - textures[id].height + height) / (float) textures[id].tex.height);

    if(base_alpha != 0xFF) {
        screen_set_blend(0, false, false);
    }
}

float screen_get_font_height(float scaleY) {
    return scaleY * fontGetInfo(NULL)->lineFeed;
}

#define MAX_LINES 64

inline static void screen_wrap_string_finish_line(float* w, float* h, float* lw, float* lh, u32* line, u32* linePos, u32* lastAlignPos,
                                                  u32* lines, float* lineWidths, float* lineHeights,
                                                  u32 maxLines) {
    if(*lw > *w) {
        *w = *lw;
    }

    *h += *lh;

    if(*line < maxLines)  {
        if(lines != NULL) {
            lines[*line] = *linePos;
        }

        if(lineWidths != NULL) {
            lineWidths[*line] = *lw;
        }

        if(lineHeights != NULL) {
            lineHeights[*line] = *lh;
        }

        (*line)++;
    }

    *lw = 0;
    *lh = 0;
    *linePos = 0;
    *lastAlignPos = 0;
}

static void screen_wrap_string(u32* lines, float* lineWidths, float* lineHeights, u32* numLines, float* totalWidth, float* totalHeight,
                               const char* text, u32 maxLines, float maxWidth, float scaleX, float scaleY, bool wordWrap) {
    scaleX *= font_scale;
    scaleY *= font_scale;

    float w = 0;
    float h = 0;

    u32 line = 0;
    float lw = 0;
    float lh = 0;
    u32 linePos = 0;
    u32 lastAlignPos = 0;
    int wordPos = -1;
    float ww = 0;

    const uint8_t* p = (const uint8_t*) text;
    u32 code = 0;
    ssize_t units = -1;

    while(*p && (units = decode_utf8(&code, p)) != -1 && code > 0) {
        p += units;

        float charWidth = 1;
        if(code == '\t') {
            code = ' ';
            charWidth = 4 - (linePos - lastAlignPos) % 4;

            lastAlignPos = linePos;
        }

        charWidth *= scaleX * fontGetCharWidthInfo(NULL, fontGlyphIndexFromCodePoint(NULL, code))->charWidth;

        if(code == '\n' || (wordWrap && lw + charWidth >= maxWidth)) {
            if(code == '\n') {
                linePos++;
                lh = scaleY * fontGetInfo(NULL)->lineFeed;
            }

            u32 oldLinePos = linePos;

            if(code != '\n' && wordPos != -1) {
                linePos = (u32) wordPos;
                lw -= ww;
            }

            screen_wrap_string_finish_line(&w, &h, &lw, &lh, &line, &linePos, &lastAlignPos,
                                           lines, lineWidths, lineHeights,
                                           maxLines);

            if(code != '\n' && wordPos != -1) {
                linePos = oldLinePos - wordPos;
                lw = ww;
            }

            wordPos = -1;
            ww = 0;
        }

        if(code == ' ') {
            wordPos = -1;
            ww = 0;
        } else if(wordPos == -1) {
            wordPos = (int) linePos;
            ww = 0;
        }

        if(code != '\n') {
            if(wordPos != -1) {
                ww += charWidth;
            }

            lw += charWidth;
            lh = scaleY * fontGetInfo(NULL)->lineFeed;

            linePos++;
        }
    }

    if(linePos > 0)  {
        screen_wrap_string_finish_line(&w, &h, &lw, &lh, &line, &linePos, &lastAlignPos,
                                       lines, lineWidths, lineHeights,
                                       maxLines);
    }

    if(numLines != NULL) {
        *numLines = line;
    }

    if(totalWidth != NULL) {
        *totalWidth = w;
    }

    if(totalHeight != NULL) {
        *totalHeight = h;
    }
}

void screen_get_string_size(float* width, float* height, const char* text, float scaleX, float scaleY) {
    screen_wrap_string(NULL, NULL, NULL, NULL, width, height, text, 0, 0, scaleX, scaleY, false);
}

void screen_get_string_size_wrap(float* width, float* height, const char* text, float scaleX, float scaleY, float wrapWidth) {
    screen_wrap_string(NULL, NULL, NULL, NULL, width, height, text, 0, wrapWidth, scaleX, scaleY, true);
}

static void screen_draw_string_internal(const char* text, float x, float y, float scaleX, float scaleY, u32 colorId, bool centerLines, bool wrap, float wrapX) {
    if(text == NULL) {
        return;
    }

    if(colorId >= MAX_COLORS) {
        error_panic("Attempted to draw string with invalid color ID \"%lu\".", colorId);
        return;
    }

    u32 blendColor = color_config[colorId];
    if(base_alpha != 0xFF) {
        float alpha1 = ((blendColor >> 24) & 0xFF) / 255.0f;
        float alpha2 = base_alpha / 255.0f;
        float blendedAlpha = alpha1 * alpha2;

        blendColor = (((u32) (blendedAlpha * 0xFF)) << 24) | (blendColor & 0x00FFFFFF);
    }

    screen_set_blend(blendColor, true, true);

    u32 lines[MAX_LINES];
    float lineWidths[MAX_LINES];
    float lineHeights[MAX_LINES];
    u32 numLines = 0;
    float totalWidth = 0;
    float totalHeight = 0;
    screen_wrap_string(lines, lineWidths, lineHeights, &numLines, &totalWidth, &totalHeight, text, MAX_LINES, wrapX - x, scaleX, scaleY, wrap);

    float currX = x;
    float currY = y;

    u32 linePos = 0;
    u32 lastAlignPos = 0;
    int lastSheet = -1;

    const uint8_t* p = (const uint8_t*) text;
    u32 code = 0;
    ssize_t units = -1;

    for(u32 i = 0; i < numLines; i++) {
        currX = x;
        if(centerLines) {
            currX += (totalWidth - lineWidths[i]) / 2;
        }

        while(linePos < lines[i] && *p && (units = decode_utf8(&code, p)) != -1 && code > 0) {
            p += units;

            if(code != '\n') {
                u32 num = 1;
                if(code == '\t') {
                    code = ' ';
                    num = 4 - (linePos - lastAlignPos) % 4;

                    lastAlignPos = linePos;
                }

                fontGlyphPos_s data;
                fontCalcGlyphPos(&data, NULL, fontGlyphIndexFromCodePoint(NULL, code), GLYPH_POS_CALC_VTXCOORD, scaleX * font_scale, scaleY * font_scale);

                if(data.sheetIndex >= glyph_count) {
                    fontCalcGlyphPos(&data, NULL, fontGlyphIndexFromCodePoint(NULL, 0xFFFD), GLYPH_POS_CALC_VTXCOORD, scaleX * font_scale, scaleY * font_scale);
                }

                if(data.sheetIndex < glyph_count && data.sheetIndex != lastSheet) {
                    lastSheet = data.sheetIndex;
                    C3D_TexBind(0, &glyph_sheets[lastSheet]);
                }

                for(u32 j = 0; j < num; j++) {
                    screen_draw_quad(currX + data.vtxcoord.left, currY + data.vtxcoord.top, currX + data.vtxcoord.right, currY + data.vtxcoord.bottom, data.texcoord.left, data.texcoord.bottom, data.texcoord.right, data.texcoord.top);

                    currX += data.xAdvance;
                }
            }

            linePos++;
        }

        currY += lineHeights[i];

        linePos = 0;
        lastAlignPos = 0;
    }

    screen_set_blend(0, false, false);
}

void screen_draw_string(const char* text, float x, float y, float scaleX, float scaleY, u32 colorId, bool centerLines) {
    screen_draw_string_internal(text, x, y, scaleX, scaleY, colorId, centerLines, false, 0);
}

void screen_draw_string_wrap(const char* text, float x, float y, float scaleX, float scaleY, u32 colorId, bool centerLines, float wrapX) {
    screen_draw_string_internal(text, x, y, scaleX, scaleY, colorId, centerLines, true, wrapX);
}

/* Native Terminus Bold 14 bitmap text. Integer positions and a one-pixel shadow.
 * Compatibility names retained for the existing UI call sites. */
#ifndef CAIRO_TEXT_H
#define CAIRO_TEXT_H
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "font_terminus.h"
#define CAIRO_LINE_H 18
#define CAIRO_GLYPH_H 14
#define CAIRO_BASE_ROW 10
typedef struct { int adv4; unsigned char code; } CairoGlyph;

#define CAIRO_NO_SHADOW (-1)

static inline uint32_t Cairo_NextChar(const char **text, const char *end)
{
    const unsigned char *start = (const unsigned char *)*text;
    uint32_t lead = *start;
    *text += 1;
    if (lead < 0x80) return lead;

    int bytes;
    uint32_t codepoint, minimum;
    if (lead >= 0xC2 && lead <= 0xDF) {
        bytes = 2; codepoint = lead & 0x1F; minimum = 0x80;
    } else if (lead >= 0xE0 && lead <= 0xEF) {
        bytes = 3; codepoint = lead & 0x0F; minimum = 0x800;
    } else if (lead >= 0xF0 && lead <= 0xF4) {
        bytes = 4; codepoint = lead & 0x07; minimum = 0x10000;
    } else {
        return '?';
    }

    for (int i = 1; i < bytes; i++) {
        if (end != NULL && end - (const char *)start <= i) return '?';
        if ((start[i] & 0xC0) != 0x80) return '?';
        codepoint = (codepoint << 6) | (start[i] & 0x3F);
    }
    if (codepoint < minimum || codepoint > 0x10FFFF ||
        (codepoint >= 0xD800 && codepoint <= 0xDFFF)) return '?';
    *text += bytes - 1;
    return codepoint;
}

static inline const CairoGlyph *Cairo_Glyph(uint32_t ch)
{
    static const uint16_t lt[] = {0x104,0x105,0x10C,0x10D,0x118,0x119,0x116,0x117,0x12E,0x12F,0x160,0x161,0x172,0x173,0x16A,0x16B,0x17D,0x17E};
    static CairoGlyph glyph;
    glyph.code = ch >= 32 && ch < 127 ? ch - 32 : '?' - 32;
    for (int i = 0; i < 18; i++) if (lt[i] == ch) glyph.code = 95 + i;
    glyph.adv4 = 32;
    return &glyph;
}

/* Width in 1/4 px of the text up to the first '\n' or the end of `len` bytes (len < 0: NUL). */
static inline int Cairo_Width4(const char *text, int len)
{
    int w = 0;
    const char *p = text, *end = len >= 0 ? text + len : NULL;
    while ((!end || p < end) && *p && *p != '\n') w += Cairo_Glyph(Cairo_NextChar(&p, end))->adv4;
    return w;
}

static inline int Cairo_TextWidth(const char *text) { return (Cairo_Width4(text, -1) + 2) / 4; }

static inline void Cairo_Stamp(uint8_t *buf, int stride, int x, int y, const CairoGlyph *g, uint8_t color, int clipTop, int clipBottom)
{
    for (int r = 0; r < CAIRO_GLYPH_H; r++) {
        int py = y - CAIRO_BASE_ROW + r;
        if (py < clipTop || py >= clipBottom) continue;
        unsigned char bits = s_terminusRows[g->code][r];
        for (int c = 0; c < 8; c++) {
            int px = x + c;
            if ((bits & (0x80u >> c)) && px >= 0 && px < stride)
                buf[py * stride + px] = color;
        }
    }
}

/* Draw one line (up to '\n' or `len` bytes) centred on cx with its baseline at baseY.
 * Shadow: one pixel down/right of every glyph (shadowColor < 0: none). */
static inline void Cairo_DrawTextClipped(uint8_t *buf, int stride, int cx, int baseY, const char *text, int len, uint8_t color, int shadowColor, int clipTop, int clipBottom)
{
    
    if (!buf || !text) return;
    int left4 = (cx - Cairo_Width4(text, len) / 8) * 4;
    const char *end = len >= 0 ? text + len : NULL;
    for (int pass = shadowColor >= 0 ? 0 : 1; pass < 2; pass++) {
        const char *p = text;
        int pen4 = left4;
        while ((!end || p < end) && *p && *p != '\n') {
            const CairoGlyph *g = Cairo_Glyph(Cairo_NextChar(&p, end));
            int x = (pen4 + 2) >> 2;
            if (pass == 0) { Cairo_Stamp(buf, stride, x + 1, baseY + 1, g, (uint8_t)shadowColor, clipTop, clipBottom); }
            else Cairo_Stamp(buf, stride, x, baseY, g, color, clipTop, clipBottom);
            pen4 += g->adv4;
        }
    }
}

static inline void Cairo_DrawText(uint8_t *buf, int stride, int cx, int baseY, const char *text, uint8_t color, int shadowColor)
{
    Cairo_DrawTextClipped(buf, stride, cx, baseY, text, -1, color, shadowColor, 0, 240);
}

/* Find the painted bounds of a block relative to its centre x and first baseline. */
typedef struct { int left, top, right, bottom; } CairoBounds;

static inline CairoBounds Cairo_BlockBounds(const char *const *lines, int count, int step, bool shadow)
{
    CairoBounds bounds = {0, 0, 0, 0};
    bool ink = false;
    for (int i = 0; i < count; i++) {
        const char *p = lines[i];
        int x = -Cairo_TextWidth(p) / 2;
        for (; *p; x += 8) {
            const CairoGlyph *glyph = Cairo_Glyph(Cairo_NextChar(&p, NULL));
            for (int row = 0; row < CAIRO_GLYPH_H; row++) {
                unsigned char bits = s_terminusRows[glyph->code][row];
                if (!bits) continue;
                int y = i * step + row - CAIRO_BASE_ROW;
                for (int col = 0; col < 8; col++) {
                    if (!(bits & (0x80u >> col))) continue;
                    int px = x + col;
                    if (!ink || px < bounds.left) bounds.left = px;
                    if (!ink || px > bounds.right) bounds.right = px;
                    if (!ink || y < bounds.top) bounds.top = y;
                    if (!ink || y > bounds.bottom) bounds.bottom = y;
                    ink = true;
                }
            }
        }
    }
    if (ink && shadow) {
        bounds.right++;
        bounds.bottom++;
    }
    return bounds;
}

/* Wrap at spaces or explicit line breaks. A long word is split at a glyph boundary.
 * Return the unconsumed text so the caller can display another page. */
static inline int Cairo_WrapText(const char *text, int maxW, char lines[][128], int maxLines, const char **nextPage)
{
    const char *p = text;
    int count = 0;
    while (*p && count < maxLines) {
        while (*p == ' ') p++;
        if (!*p) break;
        const char *start = p;
        const char *lastSpace = NULL;
        const char *end = p;
        int width = 0;
        while (*p && *p != '\n' && *p != '\r') {
            if (*p == ' ' && (p == start || p[-1] != ' ')) lastSpace = p;
            const char *next = p;
            int advance = Cairo_Glyph(Cairo_NextChar(&next, NULL))->adv4 / 4;
            if (next - start >= (int)sizeof(lines[0]) || (width + advance > maxW && p != start)) {
                if (lastSpace && lastSpace > start) p = end = lastSpace;
                break;
            }
            width += advance;
            p = next;
            if (next[-1] != ' ') end = next;
        }
        if (*p == '\r') {
            p++;
            if (*p == '\n') p++;
        } else if (*p == '\n') {
            p++;
        }
        size_t length = end - start;
        memcpy(lines[count], start, length);
        lines[count][length] = '\0';
        count++;
    }
    while (*p == ' ') p++;
    *nextPage = p;
    return count;
}
#endif

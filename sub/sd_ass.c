/*
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <math.h>
#include <limits.h>

#include <libavutil/common.h>
#include <ass/ass.h>

#include "talloc.h"

#include "options/options.h"
#include "common/common.h"
#include "common/msg.h"
#include "demux/stheader.h"
#include "video/csputils.h"
#include "video/mp_image.h"
#include "dec_sub.h"
#include "ass_mp.h"
#include "sd.h"

struct sd_ass_priv {
    struct ass_track *ass_track;
    struct ass_track *shadow_track; // for --sub-ass=no rendering
    bool is_converted;
    bool on_top;
    struct sub_bitmap *parts;
    bool flush_on_seek;
    int extend_event;
    char last_text[500];
    struct mp_image_params video_params;
    struct mp_image_params last_params;
    double sub_speed;
};

static void mangle_colors(struct sd *sd, struct sub_bitmaps *parts);
static void fill_plaintext(struct sd *sd, double pts);

// Add default styles, if the track does not have any styles yet.
// Apply style overrides if the user provides any.
static void mp_ass_add_default_styles(ASS_Track *track, struct MPOpts *opts)
{
    if (opts->ass_styles_file && opts->ass_style_override)
        ass_read_styles(track, opts->ass_styles_file, NULL);

    if (track->n_styles == 0) {
        if (!track->PlayResY) {
            track->PlayResY = MP_ASS_FONT_PLAYRESY;
            track->PlayResX = track->PlayResY * 4 / 3;
        }
        track->Kerning = true;
        int sid = ass_alloc_style(track);
        track->default_style = sid;
        ASS_Style *style = track->styles + sid;
        style->Name = strdup("Default");
        mp_ass_set_style(style, track->PlayResY, opts->sub_text_style);
    }

    if (opts->ass_style_override)
        ass_process_force_style(track);
}

static bool supports_format(const char *format)
{
    // ass-text is produced by converters and the subreader.c ssa parser; this
    // format has ASS tags, but doesn't start with any prelude, nor does it
    // have extradata.
    return format && (strcmp(format, "ass") == 0 ||
                      strcmp(format, "ssa") == 0 ||
                      strcmp(format, "ass-text") == 0);
}

static int init(struct sd *sd)
{
    struct MPOpts *opts = sd->opts;
    if (!sd->ass_library || !sd->ass_renderer || !sd->ass_lock || !sd->codec)
        return -1;

    struct sd_ass_priv *ctx = talloc_zero(NULL, struct sd_ass_priv);
    sd->priv = ctx;

    ctx->extend_event = -1;
    ctx->is_converted = sd->converted_from != NULL;

    pthread_mutex_lock(sd->ass_lock);

    ctx->ass_track = ass_new_track(sd->ass_library);
    if (!ctx->is_converted)
        ctx->ass_track->track_type = TRACK_TYPE_ASS;

    ctx->shadow_track = ass_new_track(sd->ass_library);
    ctx->shadow_track->PlayResX = 384;
    ctx->shadow_track->PlayResY = 288;
    mp_ass_add_default_styles(ctx->shadow_track, opts);

    if (sd->extradata) {
        ass_process_codec_private(ctx->ass_track, sd->extradata,
                                  sd->extradata_len);
    }

    mp_ass_add_default_styles(ctx->ass_track, opts);

    pthread_mutex_unlock(sd->ass_lock);

    ctx->sub_speed = 1.0;

    if (sd->video_fps && sd->sh && sd->sh->sub->frame_based > 0) {
        MP_VERBOSE(sd, "Frame based format, dummy FPS: %f, video FPS: %f\n",
                   sd->sh->sub->frame_based, sd->video_fps);
        ctx->sub_speed *= sd->sh->sub->frame_based / sd->video_fps;
    }

    if (opts->sub_fps && sd->video_fps)
        ctx->sub_speed *= opts->sub_fps / sd->video_fps;

    ctx->sub_speed *= opts->sub_speed;

    return 0;
}

static void decode(struct sd *sd, struct demux_packet *packet)
{
    struct sd_ass_priv *ctx = sd->priv;
    ASS_Track *track = ctx->ass_track;
    long long ipts = packet->pts * 1000 + 0.5;
    long long iduration = packet->duration * 1000 + 0.5;
    if (strcmp(sd->codec, "ass") == 0) {
        ass_process_chunk(track, packet->buffer, packet->len, ipts, iduration);
        return;
    } else if (strcmp(sd->codec, "ssa") == 0) {
        // broken ffmpeg ASS packet format
        ctx->flush_on_seek = true;
        ass_process_data(track, packet->buffer, packet->len);
        return;
    }

    // plaintext subs
    if (packet->pts == MP_NOPTS_VALUE) {
        MP_WARN(sd, "Subtitle without pts, ignored\n");
        return;
    }
    if (ctx->extend_event >= 0 && ctx->extend_event < track->n_events) {
        ASS_Event *event = &track->events[ctx->extend_event];
        if (event->Start <= ipts)
            event->Duration = ipts - event->Start;
        ctx->extend_event = -1;
    }

    unsigned char *text = packet->buffer;
    if (!sd->no_remove_duplicates) {
        for (int i = 0; i < track->n_events; i++) {
            if (track->events[i].Start == ipts
                && (track->events[i].Duration == iduration)
                && strcmp(track->events[i].Text, text) == 0)
                return;   // We've already added this subtitle
        }
    }
    int eid = ass_alloc_event(track);
    ASS_Event *event = track->events + eid;

    if (packet->duration == 0) {
        MP_WARN(sd, "Subtitle without duration or "
                "duration set to 0 at pts %f.\n", packet->pts);
    }
    if (packet->duration < 0) {
        // Assume unknown duration. The FFmpeg API is very unclear about this.
        MP_WARN(sd, "Assuming subtitle without duration at pts %f\n", packet->pts);
        // _If_ there's a next subtitle, the duration will be adjusted again.
        // If not, show it forever.
        iduration = INT_MAX;
        ctx->extend_event = eid;
    }

    event->Start = ipts;
    event->Duration = iduration;
    event->Style = track->default_style;
    event->Text = strdup(text);
}

static void configure_ass(struct sd *sd, struct mp_osd_res *dim,
                          bool converted, ASS_Track *track)
{
    struct MPOpts *opts = sd->opts;
    ASS_Renderer *priv = sd->ass_renderer;

    ass_set_frame_size(priv, dim->w, dim->h);
    ass_set_margins(priv, dim->mt, dim->mb, dim->ml, dim->mr);

    bool set_use_margins = false;
    int set_sub_pos = 0;
    float set_line_spacing = 0;
    float set_font_scale = 1;
    int set_hinting = 0;
    bool set_scale_with_window = false;
    bool set_scale_by_window = true;
    bool total_override = false;
    // With forced overrides, apply the --sub-* specific options
    if (converted || opts->ass_style_override == 3) {
        set_scale_with_window = opts->sub_scale_with_window;
        set_use_margins = opts->sub_use_margins;
        set_scale_by_window = opts->sub_scale_by_window;
        total_override = true;
    } else {
        set_scale_with_window = opts->ass_scale_with_window;
        set_use_margins = opts->ass_use_margins;
    }
    if (converted || opts->ass_style_override) {
        set_sub_pos = 100 - opts->sub_pos;
        set_line_spacing = opts->ass_line_spacing;
        set_hinting = opts->ass_hinting;
        set_font_scale = opts->sub_scale;
    }
    if (set_scale_with_window) {
        int vidh = dim->h - (dim->mt + dim->mb);
        set_font_scale *= dim->h / (float)MPMAX(vidh, 1);
    }
    if (!set_scale_by_window) {
        double factor = dim->h / 720.0;
        if (factor != 0.0)
            set_font_scale /= factor;
    }
    ass_set_use_margins(priv, set_use_margins);
    ass_set_line_position(priv, set_sub_pos);
    ass_set_shaper(priv, opts->ass_shaper);
    int set_force_flags = 0;
    if (total_override)
        set_force_flags |= ASS_OVERRIDE_BIT_STYLE | ASS_OVERRIDE_BIT_FONT_SIZE;
    if (opts->ass_style_override == 4)
        set_force_flags |= ASS_OVERRIDE_BIT_FONT_SIZE;
    ass_set_selective_style_override_enabled(priv, set_force_flags);
    ASS_Style style = {0};
    mp_ass_set_style(&style, 288, opts->sub_text_style);
    ass_set_selective_style_override(priv, &style);
    free(style.FontName);
    if (converted && track->default_style < track->n_styles) {
        mp_ass_set_style(track->styles + track->default_style,
                         track->PlayResY, opts->sub_text_style);
    }
    ass_set_font_scale(priv, set_font_scale);
    ass_set_hinting(priv, set_hinting);
    ass_set_line_spacing(priv, set_line_spacing);
}

static bool has_overrides(char *s)
{
    if (!s)
        return false;
    return strstr(s, "\\pos") || strstr(s, "\\move") || strstr(s, "\\clip") ||
           strstr(s, "\\iclip") || strstr(s, "\\org") || strstr(s, "\\p");
}

#define END(ev) ((ev)->Start + (ev)->Duration)

static long long find_timestamp(struct sd *sd, double pts)
{
    struct sd_ass_priv *priv = sd->priv;
    if (pts == MP_NOPTS_VALUE)
        return 0;

    pts /= priv->sub_speed;

    long long ts = llrint(pts * 1000);

    if (!sd->opts->sub_fix_timing)
        return ts;

    // Try to fix small gaps and overlaps.
    ASS_Track *track = priv->ass_track;
    int threshold = SUB_GAP_THRESHOLD * 1000;
    int keep = SUB_GAP_KEEP * 1000;

    // Find the "current" event.
    ASS_Event *ev[2] = {0};
    int n_ev = 0;
    for (int n = 0; n < track->n_events; n++) {
        ASS_Event *event = &track->events[n];
        if (ts >= event->Start - threshold && ts <= END(event) + threshold) {
            if (n_ev >= MP_ARRAY_SIZE(ev))
                return ts; // multiple overlaps - give up (probably complex subs)
            ev[n_ev++] = event;
        }
    }

    if (n_ev != 2)
        return ts;

    // Simple/minor heuristic against destroying typesetting.
    if (ev[0]->Style != ev[1]->Style || has_overrides(ev[0]->Text) ||
        has_overrides(ev[1]->Text))
        return ts;

    // Sort by start timestamps.
    if (ev[0]->Start > ev[1]->Start)
        MPSWAP(ASS_Event*, ev[0], ev[1]);

    // We want to fix partial overlaps only.
    if (END(ev[0]) >= END(ev[1]))
        return ts;

    if (ev[0]->Duration < keep || ev[1]->Duration < keep)
        return ts;

    // Gap between the events -> move ts to show the end of the first event.
    if (ts >= END(ev[0]) && ts < ev[1]->Start && END(ev[0]) < ev[1]->Start &&
        END(ev[0]) + threshold >= ev[1]->Start)
        return END(ev[0]) - 1;

    // Overlap -> move ts to the (exclusive) end of the first event.
    // Relies on the fact that the ASS_Renderer has no overlap registered, even
    // if there is one. This happens to work because we never render the
    // overlapped state, and libass never resolves a collision.
    if (ts >= ev[1]->Start && ts <= END(ev[0]) && END(ev[0]) > ev[1]->Start &&
        END(ev[0]) <= ev[1]->Start + threshold)
        return END(ev[0]);

    return ts;
}

#undef END

static void get_bitmaps(struct sd *sd, struct mp_osd_res dim, double pts,
                        struct sub_bitmaps *res)
{
    struct sd_ass_priv *ctx = sd->priv;
    struct MPOpts *opts = sd->opts;
    bool no_ass = !opts->ass_enabled || ctx->on_top;
    bool converted = ctx->is_converted || no_ass;
    ASS_Track *track = no_ass ? ctx->shadow_track : ctx->ass_track;

    if (pts == MP_NOPTS_VALUE || !sd->ass_renderer)
        return;

    pthread_mutex_lock(sd->ass_lock);

    ASS_Renderer *renderer = sd->ass_renderer;
    double scale = dim.display_par;
    if (!converted && (!opts->ass_style_override ||
                       opts->ass_vsfilter_aspect_compat))
    {
        // Let's use the original video PAR for vsfilter compatibility:
        double par = scale
            * (ctx->video_params.d_w / (double)ctx->video_params.d_h)
            / (ctx->video_params.w   / (double)ctx->video_params.h);
        if (isnormal(par))
            scale = par;
    }
    configure_ass(sd, &dim, converted, track);
    ass_set_pixel_aspect(renderer, scale);
    if (!converted && (!opts->ass_style_override ||
                       opts->ass_vsfilter_blur_compat))
    {
        ass_set_storage_size(renderer, ctx->video_params.w, ctx->video_params.h);
    } else {
        ass_set_storage_size(renderer, 0, 0);
    }
    if (no_ass)
        fill_plaintext(sd, pts);
    long long ts = find_timestamp(sd, pts);
    mp_ass_render_frame(renderer, track, ts, &ctx->parts, res);
    talloc_steal(ctx, ctx->parts);

    if (!converted)
        mangle_colors(sd, res);

    pthread_mutex_unlock(sd->ass_lock);
}

struct buf {
    char *start;
    int size;
    int len;
};

static void append(struct buf *b, char c)
{
    if (b->len < b->size) {
        b->start[b->len] = c;
        b->len++;
    }
}

static void ass_to_plaintext(struct buf *b, const char *in)
{
    bool in_tag = false;
    const char *open_tag_pos = NULL;
    bool in_drawing = false;
    while (*in) {
        if (in_tag) {
            if (in[0] == '}') {
                in += 1;
                in_tag = false;
            } else if (in[0] == '\\' && in[1] == 'p') {
                in += 2;
                // Skip text between \pN and \p0 tags. A \p without a number
                // is the same as \p0, and leading 0s are also allowed.
                in_drawing = false;
                while (in[0] >= '0' && in[0] <= '9') {
                    if (in[0] != '0')
                        in_drawing = true;
                    in += 1;
                }
            } else {
                in += 1;
            }
        } else {
            if (in[0] == '\\' && (in[1] == 'N' || in[1] == 'n')) {
                in += 2;
                append(b, '\n');
            } else if (in[0] == '\\' && in[1] == 'h') {
                in += 2;
                append(b, ' ');
            } else if (in[0] == '{') {
                open_tag_pos = in;
                in += 1;
                in_tag = true;
            } else {
                if (!in_drawing)
                    append(b, in[0]);
                in += 1;
            }
        }
    }
    // A '{' without a closing '}' is always visible.
    if (in_tag) {
        while (*open_tag_pos)
            append(b, *open_tag_pos++);
    }
}

// Empty string counts as whitespace. Reads s[len-1] even if there are \0s.
static bool is_whitespace_only(char *s, int len)
{
    for (int n = 0; n < len; n++) {
        if (s[n] != ' ' && s[n] != '\t')
            return false;
    }
    return true;
}

static char *get_text(struct sd *sd, double pts)
{
    struct sd_ass_priv *ctx = sd->priv;
    ASS_Track *track = ctx->ass_track;

    if (pts == MP_NOPTS_VALUE)
        return NULL;
    long long ipts = find_timestamp(sd, pts);

    struct buf b = {ctx->last_text, sizeof(ctx->last_text) - 1};

    for (int i = 0; i < track->n_events; ++i) {
        ASS_Event *event = track->events + i;
        if (ipts >= event->Start && ipts < event->Start + event->Duration) {
            if (event->Text) {
                int start = b.len;
                ass_to_plaintext(&b, event->Text);
                if (is_whitespace_only(&b.start[start], b.len - start)) {
                    b.len = start;
                } else {
                    append(&b, '\n');
                }
            }
        }
    }

    b.start[b.len] = '\0';

    if (b.len > 0 && b.start[b.len - 1] == '\n')
        b.start[b.len - 1] = '\0';

    return ctx->last_text;
}

static void fill_plaintext(struct sd *sd, double pts)
{
    struct sd_ass_priv *ctx = sd->priv;
    ASS_Track *track = ctx->shadow_track;

    ass_flush_events(track);

    char *text = get_text(sd, pts);
    if (!text)
        return;

    bstr dst = {0};
    while (*text) {
        if (*text == '{')
            bstr_xappend(NULL, &dst, bstr0("\\"));
        bstr_xappend(NULL, &dst, (bstr){text, 1});
        // Break ASS escapes with U+2060 WORD JOINER
        if (*text == '\\')
            mp_append_utf8_bstr(NULL, &dst, 0x2060);
        text++;
    }

    if (!dst.start || !dst.start[0])
        return;

    int n = ass_alloc_event(track);
    ASS_Event *event = track->events + n;
    event->Start = 0;
    event->Duration = INT_MAX;
    event->Style = track->default_style;
    event->Text = strdup(dst.start);

    if (track->default_style < track->n_styles)
        track->styles[track->default_style].Alignment = ctx->on_top ? 6 : 2;
}

static void fix_events(struct sd *sd)
{
    struct sd_ass_priv *ctx = sd->priv;
    ctx->flush_on_seek = false;
}

static void reset(struct sd *sd)
{
    struct sd_ass_priv *ctx = sd->priv;
    if (ctx->flush_on_seek || sd->opts->sub_clear_on_seek) {
        ass_flush_events(ctx->ass_track);
        ctx->extend_event = -1;
    }
    ctx->flush_on_seek = false;
}

static void uninit(struct sd *sd)
{
    struct sd_ass_priv *ctx = sd->priv;

    ass_free_track(ctx->ass_track);
    talloc_free(ctx);
}

static int control(struct sd *sd, enum sd_ctrl cmd, void *arg)
{
    struct sd_ass_priv *ctx = sd->priv;
    switch (cmd) {
    case SD_CTRL_SUB_STEP: {
        double *a = arg;
        long long ts = llrint(a[0] * (1000.0 / ctx->sub_speed));
        long long res = ass_step_sub(ctx->ass_track, ts, a[1]);
        if (!res)
            return false;
        a[0] = res / (1000.0 / ctx->sub_speed);
        return true;
    }
    case SD_CTRL_SET_VIDEO_PARAMS:
        ctx->video_params = *(struct mp_image_params *)arg;
        return CONTROL_OK;
    case SD_CTRL_SET_TOP:
        ctx->on_top = *(bool *)arg;
        return CONTROL_OK;
    default:
        return CONTROL_UNKNOWN;
    }
}

const struct sd_functions sd_ass = {
    .name = "ass",
    .accept_packets_in_advance = true,
    .supports_format = supports_format,
    .init = init,
    .decode = decode,
    .get_bitmaps = get_bitmaps,
    .get_text = get_text,
    .fix_events = fix_events,
    .control = control,
    .reset = reset,
    .uninit = uninit,
};

// Disgusting hack for (xy-)vsfilter color compatibility.
static void mangle_colors(struct sd *sd, struct sub_bitmaps *parts)
{
    struct MPOpts *opts = sd->opts;
    struct sd_ass_priv *ctx = sd->priv;
    enum mp_csp csp = 0;
    enum mp_csp_levels levels = 0;
    if (opts->ass_vsfilter_color_compat == 0) // "no"
        return;
    bool force_601 = opts->ass_vsfilter_color_compat == 3;
    ASS_Track *track = ctx->ass_track;
    static const int ass_csp[] = {
        [YCBCR_BT601_TV]        = MP_CSP_BT_601,
        [YCBCR_BT601_PC]        = MP_CSP_BT_601,
        [YCBCR_BT709_TV]        = MP_CSP_BT_709,
        [YCBCR_BT709_PC]        = MP_CSP_BT_709,
        [YCBCR_SMPTE240M_TV]    = MP_CSP_SMPTE_240M,
        [YCBCR_SMPTE240M_PC]    = MP_CSP_SMPTE_240M,
    };
    static const int ass_levels[] = {
        [YCBCR_BT601_TV]        = MP_CSP_LEVELS_TV,
        [YCBCR_BT601_PC]        = MP_CSP_LEVELS_PC,
        [YCBCR_BT709_TV]        = MP_CSP_LEVELS_TV,
        [YCBCR_BT709_PC]        = MP_CSP_LEVELS_PC,
        [YCBCR_SMPTE240M_TV]    = MP_CSP_LEVELS_TV,
        [YCBCR_SMPTE240M_PC]    = MP_CSP_LEVELS_PC,
    };
    int trackcsp = track->YCbCrMatrix;
    if (force_601)
        trackcsp = YCBCR_BT601_TV;
    // NONE is a bit random, but the intention is: don't modify colors.
    if (trackcsp == YCBCR_NONE)
        return;
    if (trackcsp < sizeof(ass_csp) / sizeof(ass_csp[0]))
        csp = ass_csp[trackcsp];
    if (trackcsp < sizeof(ass_levels) / sizeof(ass_levels[0]))
        levels = ass_levels[trackcsp];
    if (trackcsp == YCBCR_DEFAULT) {
        csp = MP_CSP_BT_601;
        levels = MP_CSP_LEVELS_TV;
    }
    // Unknown colorspace (either YCBCR_UNKNOWN, or a valid value unknown to us)
    if (!csp || !levels)
        return;

    struct mp_image_params params = ctx->video_params;

    if (force_601) {
        params.colorspace = MP_CSP_BT_709;
        params.colorlevels = MP_CSP_LEVELS_TV;
    }

    if (csp == params.colorspace && levels == params.colorlevels)
        return;

    bool basic_conv = params.colorspace == MP_CSP_BT_709 &&
                      params.colorlevels == MP_CSP_LEVELS_TV &&
                      csp == MP_CSP_BT_601 &&
                      levels == MP_CSP_LEVELS_TV;

    // With "basic", only do as much as needed for basic compatibility.
    if (opts->ass_vsfilter_color_compat == 1 && !basic_conv)
        return;

    if (params.colorspace != ctx->last_params.colorspace ||
        params.colorlevels != ctx->last_params.colorlevels)
    {
        int msgl = basic_conv ? MSGL_V : MSGL_WARN;
        ctx->last_params = params;
        MP_MSG(sd, msgl, "mangling colors like vsfilter: "
               "RGB -> %s %s -> %s %s -> RGB\n",
               m_opt_choice_str(mp_csp_names, csp),
               m_opt_choice_str(mp_csp_levels_names, levels),
               m_opt_choice_str(mp_csp_names, params.colorspace),
               m_opt_choice_str(mp_csp_names, params.colorlevels));
    }

    // Conversion that VSFilter would use
    struct mp_csp_params vs_params = MP_CSP_PARAMS_DEFAULTS;
    vs_params.colorspace = csp;
    vs_params.levels_in = levels;
    struct mp_cmat vs_yuv2rgb, vs_rgb2yuv;
    mp_get_csp_matrix(&vs_params, &vs_yuv2rgb);
    mp_invert_cmat(&vs_rgb2yuv, &vs_yuv2rgb);

    // Proper conversion to RGB
    struct mp_csp_params rgb_params = MP_CSP_PARAMS_DEFAULTS;
    rgb_params.colorspace = params.colorspace;
    rgb_params.levels_in = params.colorlevels;
    struct mp_cmat vs2rgb;
    mp_get_csp_matrix(&rgb_params, &vs2rgb);

    for (int n = 0; n < parts->num_parts; n++) {
        struct sub_bitmap *sb = &parts->parts[n];
        uint32_t color = sb->libass.color;
        int r = (color >> 24u) & 0xff;
        int g = (color >> 16u) & 0xff;
        int b = (color >>  8u) & 0xff;
        int a = 0xff - (color & 0xff);
        int rgb[3] = {r, g, b}, yuv[3];
        mp_map_fixp_color(&vs_rgb2yuv, 8, rgb, 8, yuv);
        mp_map_fixp_color(&vs2rgb, 8, yuv, 8, rgb);
        sb->libass.color = MP_ASS_RGBA(rgb[0], rgb[1], rgb[2], a);
    }
}

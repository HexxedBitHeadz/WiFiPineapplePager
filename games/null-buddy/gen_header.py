#!/usr/bin/env python3
"""Generate complete null_buddy_sprites.h with direct RGB565 pixel data from PNGs.

Sprite folder layout:
    sprites/default/   — 6 form-specific PNGs (t1.png, t2_sniffer.png, etc.)
    sprites/happy/     — N mood variant PNGs (any names, randomly picked at runtime)
    sprites/hungry/    — N mood variant PNGs
    sprites/sad/       — N mood variant PNGs
    sprites/sleepy/    — N mood variant PNGs
    sprites/blank/     — 1 PNG for prestige glitch pulse
"""

from PIL import Image
import os, sys, glob, re

TRANSPARENT = 0xF81F
ACCENT      = 0x07EC
DIM_ACCENT  = 0x0223
RESERVED    = {TRANSPARENT, ACCENT, DIM_ACCENT}
NUDGE = {TRANSPARENT: 0xF81E, ACCENT: 0x07ED, DIM_ACCENT: 0x0224}

BG_THRESH = 20

# All mood sprites get converted to this uniform size
MOOD_W, MOOD_H = 44, 44

def rgb888_to_rgb565(r, g, b):
    r5 = (r >> 3) & 0x1F
    g6 = (g >> 2) & 0x3F
    b5 = (b >> 3) & 0x1F
    val = (r5 << 11) | (g6 << 5) | b5
    if val in RESERVED:
        val = NUDGE[val]
    return val

def is_bg_white(r, g, b):
    return r > (255 - BG_THRESH) and g > (255 - BG_THRESH) and b > (255 - BG_THRESH)

def auto_crop_white(img, thresh=240):
    px = img.load()
    w, h = img.size
    minx, miny, maxx, maxy = w, h, 0, 0
    for y in range(h):
        for x in range(w):
            r, g, b = px[x, y][:3]
            if r < thresh or g < thresh or b < thresh:
                minx = min(minx, x)
                maxx = max(maxx, x)
                miny = min(miny, y)
                maxy = max(maxy, y)
    if maxx >= minx:
        return img.crop((minx, miny, maxx + 1, maxy + 1))
    return img

def convert(img_path, tw, th):
    img = Image.open(img_path)
    has_alpha = img.mode == "RGBA"
    if not has_alpha:
        img = img.convert("RGB")
        img = auto_crop_white(img)
    else:
        # Auto-crop using alpha channel (trim fully transparent edges)
        bbox = img.split()[3].getbbox()  # alpha channel bounding box
        if bbox:
            img = img.crop(bbox)
    # Two-step resize: LANCZOS to 2x target, then NEAREST for final snap
    img = img.resize((tw * 2, th * 2), Image.LANCZOS)
    img = img.resize((tw, th), Image.NEAREST)
    if has_alpha:
        img = img.convert("RGBA")
    else:
        img = img.convert("RGB")
    px = img.load()
    rows = []
    for y in range(th):
        row = []
        for x in range(tw):
            p = px[x, y]
            if has_alpha:
                r, g, b, a = p
                if a < 128:
                    row.append(TRANSPARENT)
                    continue
            else:
                r, g, b = p[:3]
                if is_bg_white(r, g, b):
                    row.append(TRANSPARENT)
                    continue
            row.append(rgb888_to_rgb565(r, g, b))
        rows.append(row)
    return rows

def sprite_c_array(name, tw, th, rows):
    lines = []
    lines.append(f"static const uint16_t nb_{name}_px[] = {{")
    for y, row in enumerate(rows):
        vals = ", ".join(f"0x{v:04X}" for v in row)
        lines.append(f"    {vals},  /* row {y:<2} */")
    lines.append("};")
    lines.append(f"static const Sprite NB_SPRITE_{name.upper()} = {{")
    lines.append(f"    {tw}, {th}, 0xF81F, nb_{name}_px")
    lines.append("};")
    return "\n".join(lines)

def get_pngs(folder):
    """Get sorted list of .png files in a folder, ignoring desktop.ini etc."""
    pngs = sorted(glob.glob(os.path.join(folder, "*.png")))
    return pngs


# === Main ===
script_dir = os.path.dirname(os.path.abspath(__file__))
sprites_dir = os.path.join(script_dir, "null-buddy-images", "sprites")

# --- Default forms (stage-specific) ---
default_dir = os.path.join(sprites_dir, "default")
forms = [
    ("t1.png",           24, 28, "spore",   "BINARY SPORE",   "Tier 1 — small screen-headed creature"),
    ("t2_sniffer.png",   28, 32, "sniffer", "PACKET SNIFFER", "Tier 2 — growing creature with sensors"),
    ("t2_cyborg.png",    32, 36, "cyborg",  "CYBORG",         "Tier 2 — mechanical hybrid"),
    ("t2_wraith.png",    36, 40, "wraith",  "WRAITH",         "Tier 2 — dark face, detailed body"),
    ("t3_titan.png",     40, 44, "titan",   "TITAN",          "Tier 3 — large creature with crest"),
    ("t3_prime.png",     44, 44, "prime",   "PRIME",          "Tier 3 — largest evolved form"),
]

# --- Mood pools (randomly picked at runtime) ---
moods = ["happy", "hungry", "sad", "sleepy"]
mood_sprites = {}  # mood_name -> list of png paths
for mood in moods:
    mood_dir = os.path.join(sprites_dir, mood)
    if os.path.isdir(mood_dir):
        mood_sprites[mood] = get_pngs(mood_dir)
    else:
        mood_sprites[mood] = []

# --- Blank (prestige glitch pulse) ---
blank_path = os.path.join(sprites_dir, "blank", "blank.png")
has_blank = os.path.isfile(blank_path)

# ===== Generate header =====
out = []

# Count totals for header comment
mood_counts = {m: len(mood_sprites[m]) for m in moods}
total_mood = sum(mood_counts.values())

out.append(f"""/*
 * null_buddy_sprites.h -- Sprite data for NULL-BUDDY evolutions
 *
 * Auto-generated by gen_header.py — do not edit by hand.
 *
 * Six default forms converted from PNG source art to RGB565.
 * Mood sprite pools: {', '.join(f'{m}={mood_counts[m]}' for m in moods)}
 * Blank sprite: {'yes' if has_blank else 'no'}
 *
 * Special values handled by nb_blit_recolor():
 *   0xF81F  -- transparent (skipped)
 *   0x07EC  -- accent color (replaced at runtime with stage color)
 *   0x0223  -- dim accent  (replaced at runtime with dimmed stage color)
 *
 * Default forms:
 *   SPORE   (S0-2)   24x28  -- small screen-headed creature
 *   SNIFFER (S3-5)   28x32  -- growing sensor creature
 *   CYBORG  (S6-8)   32x36  -- mechanical hybrid
 *   WRAITH  (S9-11)  36x40  -- dark detailed body
 *   TITAN   (S12-14) 40x44  -- large creature with crest
 *   PRIME   (S15-17) 44x44  -- largest evolved form
 *
 * Mood sprites are all {MOOD_W}x{MOOD_H} and randomly selected from pool.
 *
 * Included by null_buddy_render.c — NOT standalone.
 */

#ifndef NB_SPRITES_H
#define NB_SPRITES_H
""")

# --- Default form sprites ---
for i, (fname, tw, th, name, label, desc) in enumerate(forms):
    path = os.path.join(default_dir, fname)
    if not os.path.isfile(path):
        print(f"WARNING: missing {path}, skipping")
        continue
    rows = convert(path, tw, th)
    out.append(f"/* =================================================================== */")
    out.append(f"/*  FORM {i+1}: {label} -- {tw} x {th}  */")
    out.append(f"/*  {desc:<64s}*/")
    out.append(f"/* =================================================================== */")
    out.append(sprite_c_array(name, tw, th, rows))
    out.append("")

# --- Mood sprite pools ---
for mood in moods:
    pngs = mood_sprites[mood]
    if not pngs:
        continue
    out.append(f"/* =================================================================== */")
    out.append(f"/*  MOOD: {mood.upper()} -- {len(pngs)} variants, {MOOD_W}x{MOOD_H} each" + " " * (22 - len(mood)) + "*/")
    out.append(f"/* =================================================================== */")
    for idx, png_path in enumerate(pngs):
        cname = f"{mood}_{idx}"
        rows = convert(png_path, MOOD_W, MOOD_H)
        out.append(sprite_c_array(cname, MOOD_W, MOOD_H, rows))
        out.append("")

    # Array of pointers for this mood
    out.append(f"static const Sprite *NB_MOOD_{mood.upper()}[] = {{")
    refs = ", ".join(f"&NB_SPRITE_{mood.upper()}_{i}" for i in range(len(pngs)))
    out.append(f"    {refs}")
    out.append("};")
    out.append(f"#define NB_MOOD_{mood.upper()}_COUNT {len(pngs)}")
    out.append("")

# --- Blank sprite ---
if has_blank:
    out.append(f"/* =================================================================== */")
    out.append(f"/*  BLANK -- prestige glitch pulse sprite, {MOOD_W}x{MOOD_H}" + " " * 17 + "*/")
    out.append(f"/* =================================================================== */")
    rows = convert(blank_path, MOOD_W, MOOD_H)
    out.append(sprite_c_array("blank", MOOD_W, MOOD_H, rows))
    out.append("")

# --- Metadata and helper functions ---
out.append(f"""/* ═══════════════════════════════════════════════════════════════════ */
/*  SPRITE HELPERS                                                    */
/* ═══════════════════════════════════════════════════════════════════ */

/* Mood enum for sprite selection */
#define NB_MOOD_DEFAULT  0
#define NB_MOOD_HAPPY_ID 1
#define NB_MOOD_HUNGRY_ID 2
#define NB_MOOD_SAD_ID   3
#define NB_MOOD_SLEEPY_ID 4
#define NB_MOOD_BLANK_ID 5

/* Form index from stage */
static int nb_sprite_form(int stage) {{
    if (stage <= 2) return 0;
    if (stage <= 5) return 1;
    if (stage <= 8) return 2;
    if (stage <= 11) return 3;
    if (stage <= 14) return 4;
    return 5;
}}

/* Default sprite selection (by stage) */
static const Sprite* nb_get_sprite(int stage) {{
    static const Sprite *forms[] = {{
        &NB_SPRITE_SPORE, &NB_SPRITE_SNIFFER, &NB_SPRITE_CYBORG,
        &NB_SPRITE_WRAITH, &NB_SPRITE_TITAN, &NB_SPRITE_PRIME
    }};
    return forms[nb_sprite_form(stage)];
}}

/* Mood sprite selection — picks random variant from pool */
static const Sprite* nb_get_mood_sprite(int mood_id, int rand_val) {{
    switch (mood_id) {{""")

for mood in moods:
    if mood_sprites[mood]:
        out.append(f"    case NB_MOOD_{mood.upper()}_ID:")
        out.append(f"        return NB_MOOD_{mood.upper()}[rand_val % NB_MOOD_{mood.upper()}_COUNT];")

out.append(f"""    default:
        return (const Sprite*)0;
    }}
}}
""")

if has_blank:
    out.append("/* Blank sprite for prestige glitch pulse */")
    out.append("static const Sprite* nb_get_blank_sprite(void) { return &NB_SPRITE_BLANK; }")
    out.append("")

out.append(f"""/* Scale factors — sized so all forms fill a good portion of screen */
static int nb_get_sprite_scale(int stage) {{
    if (stage <= 2) return 5;   /* Spore: 120x140 */
    if (stage <= 5) return 4;   /* Sniffer: 112x128 */
    if (stage <= 8) return 4;   /* Cyborg: 128x144 */
    if (stage <= 11) return 3;  /* Wraith: 108x120 */
    if (stage <= 14) return 3;  /* Titan: 120x132 */
    return 3;                   /* Prime: 132x132 */
}}

/* Mood sprites use a fixed scale since they're all {MOOD_W}x{MOOD_H} */
#define NB_MOOD_SPRITE_SCALE 3  /* {MOOD_W*3}x{MOOD_H*3} on screen */

/* Dim a color by quartering its RGB components */
static uint16_t nb_dim_color(uint16_t c) {{
    int r = ((c >> 11) & 0x1F) >> 2;
    int g = ((c >> 5) & 0x3F) >> 2;
    int b = (c & 0x1F) >> 2;
    return (uint16_t)((r << 11) | (g << 5) | b);
}}

/* Blit sprite with accent recoloring */
static void nb_blit_recolor(GfxContext *gfx, const Sprite *s, int x, int y,
                            int scale, uint16_t accent, uint16_t dim_accent,
                            int flip) {{
    for (int py = 0; py < s->height; py++) {{
        for (int px = 0; px < s->width; px++) {{
            int src_px = flip ? (s->width - 1 - px) : px;
            uint16_t c = s->data[py * s->width + src_px];
            if (c == s->transparent) continue;
            if (c == 0x07EC) c = accent;
            else if (c == 0x0223) c = dim_accent;
            for (int dy = 0; dy < scale; dy++)
                for (int dx = 0; dx < scale; dx++)
                    gfx_pixel(gfx, x + px * scale + dx, y + py * scale + dy, c);
        }}
    }}
}}

/* Blit sprite dimmed — for ghost/afterimage trail */
static void nb_blit_ghost(GfxContext *gfx, const Sprite *s, int x, int y,
                          int scale, int dim_shifts, int flip) {{
    for (int py = 0; py < s->height; py++) {{
        for (int px = 0; px < s->width; px++) {{
            int src_px = flip ? (s->width - 1 - px) : px;
            uint16_t c = s->data[py * s->width + src_px];
            if (c == s->transparent) continue;
            int r = ((c >> 11) & 0x1F) >> dim_shifts;
            int g = ((c >> 5) & 0x3F) >> dim_shifts;
            int b = (c & 0x1F) >> dim_shifts;
            c = (uint16_t)((r << 11) | (g << 5) | b);
            if (c == 0) continue;
            for (int dy = 0; dy < scale; dy++)
                for (int dx = 0; dx < scale; dx++)
                    gfx_pixel(gfx, x + px * scale + dx, y + py * scale + dy, c);
        }}
    }}
}}

/* Blit with color channel mask — for chromatic aberration */
static void nb_blit_tint(GfxContext *gfx, const Sprite *s, int x, int y,
                         int scale, uint16_t channel_mask, int flip) {{
    for (int py = 0; py < s->height; py++) {{
        for (int px = 0; px < s->width; px++) {{
            int src_px = flip ? (s->width - 1 - px) : px;
            uint16_t c = s->data[py * s->width + src_px];
            if (c == s->transparent) continue;
            c = c & channel_mask;
            if (c == 0) continue;
            for (int dy = 0; dy < scale; dy++)
                for (int dx = 0; dx < scale; dx++)
                    gfx_pixel(gfx, x + px * scale + dx, y + py * scale + dy, c);
        }}
    }}
}}

/* Blit with random scanline displacement — glitch effect */
static void nb_blit_glitch(GfxContext *gfx, const Sprite *s, int x, int y,
                           int scale, uint16_t accent, uint16_t dim_accent,
                           int glitch_pct, int flip) {{
    for (int py = 0; py < s->height; py++) {{
        int row_shift = 0;
        if (glitch_pct > 0 && (rand() % 100) < glitch_pct)
            row_shift = (rand() % 12) - 6;
        for (int px = 0; px < s->width; px++) {{
            int src_px = flip ? (s->width - 1 - px) : px;
            uint16_t c = s->data[py * s->width + src_px];
            if (c == s->transparent) continue;
            if (c == 0x07EC) c = accent;
            else if (c == 0x0223) c = dim_accent;
            for (int dy = 0; dy < scale; dy++)
                for (int dx = 0; dx < scale; dx++)
                    gfx_pixel(gfx, x + px * scale + dx + row_shift,
                              y + py * scale + dy, c);
        }}
    }}
}}

#endif /* NB_SPRITES_H */
""")

# Write the header
output_path = os.path.join(script_dir, "null_buddy_sprites.h")
with open(output_path, "w", newline="\n", encoding="utf-8") as f:
    f.write("\n".join(out))

print(f"Written {output_path}")
print(f"  Default forms: {len(forms)}")
for mood in moods:
    print(f"  {mood}: {len(mood_sprites[mood])} variants")
print(f"  Blank: {'yes' if has_blank else 'no'}")

print(f"Written {output_path}")
print(f"Total lines: {len(out)}")

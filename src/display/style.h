// SPDX-License-Identifier: GPL-2.0
/*
 * xe_top - Color/style constants and theme support
 */
#ifndef STYLE_H
#define STYLE_H

/* ── ANSI 颜色代码 ── */
#define COLOR_RESET  "\033[0m"
#define COLOR_BOLD   "\033[1m"
#define COLOR_DIM    "\033[2m"
#define COLOR_CYAN   "\033[36m"
#define COLOR_GREEN  "\033[32m"
#define COLOR_YELLOW "\033[33m"
#define COLOR_RED    "\033[31m"
#define COLOR_BLUE   "\033[34m"
#define COLOR_MAGENTA "\033[35m"
#define COLOR_BG_BLACK "\033[40m"

/* ── 框线字符 (UTF-8) ── */
#define HORIZ "─"
#define VERT  "│"
#define TL    "┌"
#define TR    "┐"
#define BL    "└"
#define BR    "┘"

/* ── Sparkline characters (8 levels) ── */
#define SPARK_CHARS "▁▂▃▄▅▆▇█"

/* Color for a percentage value (green/yellow/red thresholds) */
static inline const char *color_for_pct(float pct)
{
    if (pct > 80.0f) return COLOR_RED;
    if (pct > 50.0f) return COLOR_YELLOW;
    return COLOR_GREEN;
}

#endif /* STYLE_H */
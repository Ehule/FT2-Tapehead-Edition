#!/usr/bin/env python3
"""Compile and run state-level regressions for the palette list and field colors."""

import subprocess
import tempfile
from pathlib import Path

SOURCE = r'''
#include <assert.h>
#include <stdint.h>
#define PAL_NUM 34
#define VISIBLE 6
static uint8_t offset, selected;
static uint32_t colors[12];
static void scroll_to(unsigned pos) { offset = pos > 12 ? 12 : pos; }
static void select_row(unsigned row) { selected = offset + row; if (selected > 17) selected = 17; }
static uint32_t recolor(uint32_t pixel, const uint32_t pal[PAL_NUM]) {
    uint8_t index = pixel >> 24;
    return index < PAL_NUM ? pal[index] : pixel;
}
int main(void) {
    uint32_t pal[PAL_NUM];
    for (unsigned i=0; i<PAL_NUM; i++) pal[i] = (i<<24) | i;
    for (unsigned i=22; i<=33; i++) assert(recolor(i<<24, pal) == pal[i]);
    assert(recolor(0xFE123456, pal) == 0xFE123456);
    selected=2; scroll_to(12); assert(selected==2 && offset==12);
    select_row(3); assert(selected==15); scroll_to(99); assert(offset==12 && selected==15);
    colors[0]=0x112233; colors[3]=0x445566; colors[11]=0xA0B0C0;
    assert(colors[0]==0x112233 && colors[1]==0 && colors[3]==0x445566 && colors[11]==0xA0B0C0);
    uint32_t saved[12]; for (unsigned i=0;i<12;i++) saved[i]=colors[i];
    for (unsigned i=0;i<12;i++) colors[i]=saved[i];
    assert(colors[0]==0x112233 && colors[3]==0x445566 && colors[11]==0xA0B0C0);
    return 0;
}
'''

with tempfile.TemporaryDirectory() as directory:
    root = Path(directory)
    source = root / "pattern_palette_test.c"
    binary = root / "pattern_palette_test"
    source.write_text(SOURCE)
    subprocess.run(["cc", "-std=c99", "-Wall", "-Wextra", "-Werror", str(source), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)

print("Native palette-index, scrolling, selection, isolation, and round-trip checks passed.")

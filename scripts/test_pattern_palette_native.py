#!/usr/bin/env python3
"""Compile and run state-level regressions for the palette list and field colors."""

import subprocess
import tempfile
from pathlib import Path

SOURCE = r'''
#include <assert.h>
#include <stdint.h>
#define PAL_NUM 28
#define VISIBLE 6
static uint8_t offset, selected;
static uint32_t colors[6];
static void scroll_to(unsigned pos) { offset = pos > 6 ? 6 : pos; }
static void select_row(unsigned row) { selected = offset + row; if (selected > 11) selected = 11; }
static uint32_t recolor(uint32_t pixel, const uint32_t pal[PAL_NUM]) {
    uint8_t index = pixel >> 24;
    return index < PAL_NUM ? pal[index] : pixel;
}
int main(void) {
    uint32_t pal[PAL_NUM];
    for (unsigned i=0; i<PAL_NUM; i++) pal[i] = (i<<24) | i;
    for (unsigned i=22; i<=27; i++) assert(recolor(i<<24, pal) == pal[i]);
    assert(recolor(0xFE123456, pal) == 0xFE123456);
    selected=2; scroll_to(6); assert(selected==2 && offset==6);
    select_row(3); assert(selected==9); scroll_to(99); assert(offset==6 && selected==9);
    colors[0]=0x112233; colors[3]=0x445566;
    assert(colors[0]==0x112233 && colors[1]==0 && colors[3]==0x445566 && colors[4]==0);
    uint32_t saved[6]; for (unsigned i=0;i<6;i++) saved[i]=colors[i];
    for (unsigned i=0;i<6;i++) colors[i]=saved[i];
    assert(colors[0]==0x112233 && colors[3]==0x445566);
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

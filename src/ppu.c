#include "ppu.h"

#include <string.h>

#include "gba.h"
#include "io.h"

const word sclayout[4][2][2] = {
    {{0, 0}, {0, 0}}, {{0, 0}, {1, 1}}, {{0, 1}, {0, 1}}, {{0, 1}, {2, 3}}};

void draw_bg_line_text(PPU* ppu, int bg) {
    word map_start = ppu->master->io.bgcnt[bg].tilemap_base * 0x800;
    word tile_start = ppu->master->io.bgcnt[bg].tile_base * 0x4000;
    hword sy = (ppu->ly + ppu->master->io.bgtext[bg].vofs) % 512;
    hword finey = sy & 0b111;
    hword tiley = (sy >> 3) & 0b11111;
    for (int x = 0; x < GBA_SCREEN_W; x++) {
        hword sx = (x + ppu->master->io.bgtext[bg].hofs) % 512;
        word map_addr =
            map_start +
            0x800 * sclayout[ppu->master->io.bgcnt[bg].size][sy >> 8][sx >> 8];
        hword finex = sx & 0b111;
        hword tilex = (sx >> 3) & 0b11111;
        hword tile_off = 32 * tiley + tilex;
        map_addr += tile_off << 1;
        map_addr %= 0x10000;
        Tile tile = {ppu->master->vram.h[map_addr >> 1]};
        if (tile.hflip) finex = 7 - finex;
        if (tile.vflip) finey = 7 - finey;
        hword pixel_off = 8 * finey + finex;
        byte col_ind;
        if (ppu->master->io.bgcnt[bg].palette) {
            hword tile_addr = tile_start + 64 * tile.num + pixel_off;
            col_ind = ppu->master->vram.b[tile_addr % 0x10000];
        } else {
            hword tile_addr = tile_start + 32 * tile.num + (pixel_off >> 1);
            col_ind = ppu->master->vram.b[tile_addr % 0x10000];
            if (pixel_off & 1) {
                col_ind >>= 4;
            } else {
                col_ind &= 0b1111;
            }
            if (col_ind) col_ind |= tile.palette << 4;
        }
        if (col_ind) ppu->screen[ppu->ly][x] = ppu->master->cram.h[col_ind];
    }
}

void draw_bg_line_rot(PPU* ppu, int bg) {}

void draw_bg_line_m0(PPU* ppu) {
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            if ((ppu->master->io.dispcnt.bg_enable & (1 << j)) &&
                ppu->master->io.bgcnt[j].priority == i) {
                draw_bg_line_text(ppu, j);
            }
        }
    }
}

void draw_bg_line_m1(PPU* ppu) {
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 3; j++) {
            if ((ppu->master->io.dispcnt.bg_enable & (1 << j)) &&
                ppu->master->io.bgcnt[j].priority == i) {
                if (j < 2) draw_bg_line_text(ppu, j);
                else draw_bg_line_rot(ppu, j);
            }
        }
    }
}

void draw_bg_line_m2(PPU* ppu) {
    for (int i = 0; i < 4; i++) {
        for (int j = 2; j < 4; j++) {
            if ((ppu->master->io.dispcnt.bg_enable & (1 << j)) &&
                ppu->master->io.bgcnt[j].priority == i) {
                draw_bg_line_rot(ppu, j);
            }
        }
    }
}

void draw_bg_line_m3(PPU* ppu) {
    if (ppu->master->io.dispcnt.bg_enable & (1 << 2)) {
        word start_addr = GBA_SCREEN_W * ppu->ly;
        for (int x = 0; x < GBA_SCREEN_W; x++) {
            ppu->screen[ppu->ly][x] = ppu->master->vram.h[start_addr + x];
        }
    }
}

void draw_bg_line_m4(PPU* ppu) {
    if (ppu->master->io.dispcnt.bg_enable & (1 << 2)) {
        word start_addr = (ppu->master->io.dispcnt.frame_sel) ? 0xa000 : 0x0000;
        start_addr += GBA_SCREEN_W * ppu->ly;
        for (int x = 0; x < GBA_SCREEN_W; x++) {
            byte col_ind = ppu->master->vram.b[start_addr + x];
            if (col_ind) ppu->screen[ppu->ly][x] = ppu->master->cram.h[col_ind];
        }
    }
}

void draw_bg_line_m5(PPU* ppu) {}

void draw_bg_line(PPU* ppu) {
    switch (ppu->master->io.dispcnt.bg_mode) {
        case 0:
            draw_bg_line_m0(ppu);
            break;
        case 1:
            draw_bg_line_m1(ppu);
            break;
        case 2:
            draw_bg_line_m2(ppu);
            break;
        case 3:
            draw_bg_line_m3(ppu);
            break;
        case 4:
            draw_bg_line_m4(ppu);
            break;
        case 5:
            draw_bg_line_m5(ppu);
            break;
    }
}

void tick_ppu(PPU* ppu) {
    if (ppu->lx == 0) {
        ppu->master->io.dispstat.hblank = 0;
        if (ppu->ly == ppu->master->io.dispstat.lyc) {
            ppu->master->io.dispstat.vcounteq = 1;
            if (ppu->master->io.dispstat.vcount_irq)
                ppu->master->io.ifl.vcounteq = 1;
        } else ppu->master->io.dispstat.vcounteq = 0;
        if (ppu->ly < GBA_SCREEN_H) {
            if (ppu->master->io.dispcnt.forced_blank) {
                memset(&ppu->screen[ppu->ly][0], 0xff, sizeof ppu->screen[0]);
            } else {
                for (int x = 0; x < GBA_SCREEN_W; x++) {
                    ppu->screen[ppu->ly][x] = ppu->master->cram.h[0];
                }
                draw_bg_line(ppu);
            }

        } else if (ppu->ly == GBA_SCREEN_H) {
            ppu->master->io.dispstat.vblank = 1;
            if (ppu->master->io.dispstat.vblank_irq)
                ppu->master->io.ifl.vblank = 1;
        } else if (ppu->ly == LINES_H - 1) {
            ppu->master->io.dispstat.vblank = 0;
            ppu->frame_complete = true;
        }
    } else if (ppu->lx == GBA_SCREEN_W) {
        ppu->master->io.dispstat.hblank = 1;
        if (ppu->master->io.dispstat.hblank_irq) ppu->master->io.ifl.hblank = 1;
    }

    ppu->lx++;
    if (ppu->lx == DOTS_W) {
        ppu->lx = 0;
        ppu->ly++;
        if (ppu->ly == LINES_H) {
            ppu->ly = 0;
        }
        ppu->master->io.vcount = ppu->ly;
    }
}
#include "ppu.h"

#include <stdio.h>
#include <string.h>

#include "gba.h"
#include "io.h"

const word sclayout[4][2][2] = {
    {{0, 0}, {0, 0}}, {{0, 1}, {0, 1}}, {{0, 0}, {1, 1}}, {{0, 1}, {2, 3}}};

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
        word tile_off = 32 * tiley + tilex;
        map_addr += tile_off << 1;
        map_addr %= 0x10000;
        Tile tile = {ppu->master->vram.h[map_addr >> 1]};
        if (tile.hflip) finex = 7 - finex;
        if (tile.vflip) finey = 7 - finey;
        word pixel_off = 8 * finey + finex;
        byte col_ind;
        if (ppu->master->io.bgcnt[bg].palette) {
            word tile_addr = tile_start + 64 * tile.num + pixel_off;
            col_ind = ppu->master->vram.b[tile_addr % 0x10000];
        } else {
            word tile_addr = tile_start + 32 * tile.num + (pixel_off >> 1);
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

void draw_bg_line_aff(PPU* ppu, int bg, int mode) {
    word map_start = ppu->master->io.bgcnt[bg].tilemap_base * 0x800;
    word tile_start = ppu->master->io.bgcnt[bg].tile_base * 0x4000;
    word bm_start = (ppu->master->io.dispcnt.frame_sel) ? 0xa000 : 0x0000;

    sword x0 = ppu->bgaffintr[bg - 2].x;
    sword y0 = ppu->bgaffintr[bg - 2].y;

    hword size = 1 << (7 + ppu->master->io.bgcnt[bg].size);

    for (int x = 0; x < GBA_SCREEN_W; x++,
             x0 += ppu->master->io.bgaff[bg - 2].pa,
             y0 += ppu->master->io.bgaff[bg - 2].pc) {
        word sx = x0 >> 8;
        word sy = y0 >> 8;
        if (((mode < 3) && (sx >= size || sy >= size) &&
             !ppu->master->io.bgcnt[bg].overflow) ||
            ((mode == 3 || mode == 4) &&
             (sx >= GBA_SCREEN_W || sy >= GBA_SCREEN_H)) ||
            ((mode == 5) && (sx >= 160 || sy >= 128)))
            continue;

        byte col_ind = 0;
        bool pal = true;
        switch (mode) {
            case 1:
            case 2:
                sx &= size - 1;
                sy &= size - 1;
                hword tilex = sx >> 3;
                hword tiley = sy >> 3;
                hword finex = sx & 0b111;
                hword finey = sy & 0b111;
                byte tile =
                    ppu->master->vram
                        .b[(map_start + tiley * (size >> 3) + tilex) % 0x10000];
                col_ind = ppu->master->vram
                              .b[(tile_start + 64 * tile + finey * 8 + finex) %
                                 0x10000];
                break;
            case 3:
                pal = false;
                ppu->screen[ppu->ly][x] =
                    ppu->master->vram.h[sy * GBA_SCREEN_W + sx];
                break;
            case 4:
                col_ind =
                    ppu->master->vram.b[bm_start + sy * GBA_SCREEN_W + sx];
                break;
            case 5:
                pal = false;
                ppu->screen[ppu->ly][x] =
                    ppu->master->vram.h[(bm_start >> 1) + sy * 160 + sx];
                break;
        }
        if (col_ind && pal)
            ppu->screen[ppu->ly][x] = ppu->master->cram.h[col_ind];
    }
}

void draw_bg_line_m0(PPU* ppu) {
    for (int i = 3; i >= 0; i--) {
        for (int j = 3; j >= 0; j--) {
            if ((ppu->master->io.dispcnt.bg_enable & (1 << j)) &&
                ppu->master->io.bgcnt[j].priority == i) {
                draw_bg_line_text(ppu, j);
            }
        }
    }
}

void draw_bg_line_m1(PPU* ppu) {
    for (int i = 3; i >= 0; i--) {
        for (int j = 3; j >= 0; j--) {
            if ((ppu->master->io.dispcnt.bg_enable & (1 << j)) &&
                ppu->master->io.bgcnt[j].priority == i) {
                if (j < 2) draw_bg_line_text(ppu, j);
                else draw_bg_line_aff(ppu, j, 1);
            }
        }
    }
}

void draw_bg_line_m2(PPU* ppu) {
    for (int i = 3; i >= 0; i--) {
        for (int j = 3; j >= 2; j--) {
            if ((ppu->master->io.dispcnt.bg_enable & (1 << j)) &&
                ppu->master->io.bgcnt[j].priority == i) {
                draw_bg_line_aff(ppu, j, 2);
            }
        }
    }
}

void draw_bg_line_m3(PPU* ppu) {
    if (ppu->master->io.dispcnt.bg_enable & (1 << 2)) {
        draw_bg_line_aff(ppu, 2, 3);
    }
}

void draw_bg_line_m4(PPU* ppu) {
    if (ppu->master->io.dispcnt.bg_enable & (1 << 2)) {
        draw_bg_line_aff(ppu, 2, 4);
    }
}

void draw_bg_line_m5(PPU* ppu) {
    if (ppu->master->io.dispcnt.bg_enable & (1 << 2)) {
        draw_bg_line_aff(ppu, 2, 5);
    }
}

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
        if (ppu->ly == GBA_SCREEN_H) {
            ppu->master->io.dispstat.vblank = 1;
            if (ppu->master->io.dispstat.vblank_irq)
                ppu->master->io.ifl.vblank = 1;

            ppu->bgaffintr[0].x = ppu->master->io.bgaff[0].x;
            ppu->bgaffintr[0].y = ppu->master->io.bgaff[0].y;
            ppu->bgaffintr[1].x = ppu->master->io.bgaff[1].x;
            ppu->bgaffintr[1].y = ppu->master->io.bgaff[1].y;
        } else if (ppu->ly == LINES_H - 1) {
            ppu->master->io.dispstat.vblank = 0;
            ppu->frame_complete = true;
        }
    } else if (ppu->lx == GBA_SCREEN_W) {
        if (ppu->ly < GBA_SCREEN_H) {
            if (ppu->master->io.dispcnt.forced_blank) {
                memset(&ppu->screen[ppu->ly][0], 0xff, sizeof ppu->screen[0]);
            } else {
                for (int x = 0; x < GBA_SCREEN_W; x++) {
                    ppu->screen[ppu->ly][x] = ppu->master->cram.h[0];
                }
                draw_bg_line(ppu);
            }
            ppu->bgaffintr[0].x += ppu->master->io.bgaff[0].pb;
            ppu->bgaffintr[0].y += ppu->master->io.bgaff[0].pd;
            ppu->bgaffintr[1].x += ppu->master->io.bgaff[1].pb;
            ppu->bgaffintr[1].y += ppu->master->io.bgaff[1].pd;
        }
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
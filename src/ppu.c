#include "ppu.h"

#include <stdio.h>
#include <string.h>

#include "dma.h"
#include "gba.h"
#include "io.h"
#include "scheduler.h"

const int SCLAYOUT[4][2][2] = {
    {{0, 0}, {0, 0}}, {{0, 1}, {0, 1}}, {{0, 0}, {1, 1}}, {{0, 1}, {2, 3}}};

// shape: sqr, short, long
const int OBJLAYOUT[4][3] = {
    {8, 8, 16}, {16, 8, 32}, {32, 16, 32}, {64, 32, 64}};

void render_bg_line_text(PPU* ppu, int bg) {
    if (!(ppu->master->io.dispcnt.bg_enable & (1 << bg))) return;
    ppu->draw_bg[bg] = true;

    word map_start = ppu->master->io.bgcnt[bg].tilemap_base * 0x800;
    word tile_start = ppu->master->io.bgcnt[bg].tile_base * 0x4000;
    hword sy = (ppu->ly + ppu->master->io.bgtext[bg].vofs) % 512;
    hword finey = sy & 0b111;
    hword tiley = (sy >> 3) & 0b11111;
    for (int x = 0; x < GBA_SCREEN_W; x++) {
        hword sx = (x + ppu->master->io.bgtext[bg].hofs) % 512;
        word map_addr =
            map_start +
            0x800 * SCLAYOUT[ppu->master->io.bgcnt[bg].size][sy >> 8][sx >> 8];
        hword finex = sx & 0b111;
        hword tilex = (sx >> 3) & 0b11111;
        word tile_off = 32 * tiley + tilex;
        map_addr += tile_off << 1;
        map_addr %= 0x10000;
        Tile tile = {ppu->master->vram.h[map_addr >> 1]};
        if (tile.hflip) finex = 7 - finex;
        hword tmpfiney = finey;
        if (tile.vflip) tmpfiney = 7 - finey;
        word pixel_off = 8 * tmpfiney + finex;
        byte col_ind;
        if (ppu->master->io.bgcnt[bg].palmode) {
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
        if (col_ind)
            ppu->bgline[bg][x] = ppu->master->pram.h[col_ind] & ~(1 << 15);
        else ppu->bgline[bg][x] = 1 << 15;
    }
}

void render_bg_line_aff(PPU* ppu, int bg, int mode) {
    if (!(ppu->master->io.dispcnt.bg_enable & (1 << bg))) return;
    ppu->draw_bg[bg] = true;

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
            ((mode == 5) && (sx >= 160 || sy >= 128))) {
            ppu->bgline[bg][x] = 1 << 15;
            continue;
        }

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
                ppu->bgline[bg][x] =
                    ppu->master->vram.h[sy * GBA_SCREEN_W + sx];
                break;
            case 4:
                col_ind =
                    ppu->master->vram.b[bm_start + sy * GBA_SCREEN_W + sx];
                break;
            case 5:
                pal = false;
                ppu->bgline[bg][x] =
                    ppu->master->vram.h[(bm_start >> 1) + sy * 160 + sx];
                break;
        }
        if (pal) {
            if (col_ind)
                ppu->bgline[bg][x] = ppu->master->pram.h[col_ind] & ~(1 << 15);
            else ppu->bgline[bg][x] = 1 << 15;
        }
    }
}

void render_bg_lines(PPU* ppu) {
    switch (ppu->master->io.dispcnt.bg_mode) {
        case 0:
            render_bg_line_text(ppu, 0);
            render_bg_line_text(ppu, 1);
            render_bg_line_text(ppu, 2);
            render_bg_line_text(ppu, 3);
            break;
        case 1:
            render_bg_line_text(ppu, 0);
            render_bg_line_text(ppu, 1);
            render_bg_line_aff(ppu, 2, 1);
            break;
        case 2:
            render_bg_line_aff(ppu, 2, 2);
            render_bg_line_aff(ppu, 3, 2);
            break;
        case 3:
            render_bg_line_aff(ppu, 2, 3);
            break;
        case 4:
            render_bg_line_aff(ppu, 2, 4);
            break;
        case 5:
            render_bg_line_aff(ppu, 2, 5);
            break;
    }
}

void render_obj_line(PPU* ppu, int i) {}

void render_obj_lines(PPU* ppu) {
    if (!ppu->master->io.dispcnt.obj_enable) return;

    ppu->obj_cycles = ppu->master->io.dispcnt.hblank_free ? 954 : 1210;

    for (int i = 0; i < GBA_SCREEN_W; i++) {
        ppu->objline[0][i] = 1 << 15;
        ppu->objline[1][i] = 1 << 15;
        ppu->objline[2][i] = 1 << 15;
        ppu->objline[3][i] = 1 << 15;
    }

    for (int i = 0; i < 128; i++) {
        render_obj_line(ppu, i);
        if (ppu->obj_cycles <= 0) break;
    }
}

void compose_line(PPU* ppu, hword* line) {
    for (int x = 0; x < GBA_SCREEN_W; x++) {
        if (line[x] & (1 << 15)) continue;

        ppu->screen[ppu->ly][x] = line[x];
    }
}

void draw_line(PPU* ppu) {
    for (int x = 0; x < GBA_SCREEN_W; x++) {
        ppu->screen[ppu->ly][x] = ppu->master->pram.h[0];
    }

    ppu->draw_bg[0] = false;
    ppu->draw_bg[1] = false;
    ppu->draw_bg[2] = false;
    ppu->draw_bg[3] = false;
    ppu->draw_obj[0] = false;
    ppu->draw_obj[1] = false;
    ppu->draw_obj[2] = false;
    ppu->draw_obj[3] = false;

    render_bg_lines(ppu);
    render_obj_lines(ppu);

    for (int pri = 3; pri >= 0; pri--) {
        for (int bg = 3; bg >= 0; bg--) {
            if (ppu->draw_bg[bg] && ppu->master->io.bgcnt[bg].priority == pri) {
                compose_line(ppu, ppu->bgline[bg]);
            }
            if (ppu->draw_obj[pri]) compose_line(ppu, ppu->objline[pri]);
        }
    }
}

void on_hdraw(PPU* ppu) {
    ppu->ly++;
    if (ppu->ly == LINES_H) {
        ppu->ly = 0;
    }
    ppu->master->io.vcount = ppu->ly;

    ppu->master->io.dispstat.hblank = 0;

    if (ppu->ly == ppu->master->io.dispstat.lyc) {
        ppu->master->io.dispstat.vcounteq = 1;
        if (ppu->master->io.dispstat.vcount_irq)
            ppu->master->io.ifl.vcounteq = 1;
    } else ppu->master->io.dispstat.vcounteq = 0;

    if (ppu->ly == GBA_SCREEN_H) {
        ppu->master->io.dispstat.vblank = 1;
        on_vblank(ppu);
    } else if (ppu->ly == LINES_H - 1) {
        ppu->master->io.dispstat.vblank = 0;
        ppu->frame_complete = true;
    }

    ppu->master->sched.ppu_next.time += 4 * GBA_SCREEN_W;
    ppu->master->sched.ppu_next.callback = PPU_CLBK_HBLANK;
}

void on_vblank(PPU* ppu) {
    if (ppu->master->io.dispstat.vblank_irq) ppu->master->io.ifl.vblank = 1;

    ppu->bgaffintr[0].x = ppu->master->io.bgaff[0].x;
    ppu->bgaffintr[0].y = ppu->master->io.bgaff[0].y;
    ppu->bgaffintr[1].x = ppu->master->io.bgaff[1].x;
    ppu->bgaffintr[1].y = ppu->master->io.bgaff[1].y;

    for (int i = 0; i < 4; i++) {
        if (ppu->master->io.dma[i].cnt.start == DMA_ST_VBLANK)
            dma_activate(&ppu->master->dmac, i);
    }
}

void on_hblank(PPU* ppu) {
    if (ppu->ly < GBA_SCREEN_H) {
        if (ppu->master->io.dispcnt.forced_blank) {
            memset(&ppu->screen[ppu->ly][0], 0xff, sizeof ppu->screen[0]);
        } else {
            draw_line(ppu);
        }
        ppu->bgaffintr[0].x += ppu->master->io.bgaff[0].pb;
        ppu->bgaffintr[0].y += ppu->master->io.bgaff[0].pd;
        ppu->bgaffintr[1].x += ppu->master->io.bgaff[1].pb;
        ppu->bgaffintr[1].y += ppu->master->io.bgaff[1].pd;
    }
    ppu->master->io.dispstat.hblank = 1;

    if (ppu->master->io.dispstat.hblank_irq) ppu->master->io.ifl.hblank = 1;

    for (int i = 0; i < 4; i++) {
        if (ppu->master->io.dma[i].cnt.start == DMA_ST_HBLANK)
            dma_activate(&ppu->master->dmac, i);
    }

    ppu->master->sched.ppu_next.time += 4 * (DOTS_W - GBA_SCREEN_W);
    ppu->master->sched.ppu_next.callback = PPU_CLBK_HDRAW;
}
#include "ppu.h"

#include <stdio.h>
#include <string.h>

#include "dma.h"
#include "gba.h"
#include "io.h"
#include "scheduler.h"

extern bool filter;

const int SCLAYOUT[4][2][2] = {
    {{0, 0}, {0, 0}}, {{0, 1}, {0, 1}}, {{0, 0}, {1, 1}}, {{0, 1}, {2, 3}}};

// size: sqr, short, long
const int OBJLAYOUT[4][3] = {{8, 8, 16}, {16, 8, 32}, {32, 16, 32}, {64, 32, 64}};

void render_bg_line_text(PPU* ppu, int bg) {
    if (!(ppu->master->io.dispcnt.bg_enable & (1 << bg))) return;
    ppu->draw_bg[bg] = true;

    word map_start = ppu->master->io.bgcnt[bg].tilemap_base * 0x800;
    word tile_start = ppu->master->io.bgcnt[bg].tile_base * 0x4000;

    hword sy;
    if (ppu->master->io.bgcnt[bg].mosaic) {
        sy = (ppu->bgmos_y + ppu->master->io.bgtext[bg].vofs) % 512;
    } else {
        sy = (ppu->ly + ppu->master->io.bgtext[bg].vofs) % 512;
    }
    hword scy = sy >> 8;
    hword ty = (sy >> 3) & 0b11111;
    hword fy = sy & 0b111;
    hword sx = ppu->master->io.bgtext[bg].hofs;
    hword scx = sx >> 8;
    hword tx = (sx >> 3) & 0b11111;
    hword fx = sx & 0b111;
    byte scs[2] = {SCLAYOUT[ppu->master->io.bgcnt[bg].size][scy & 1][0],
                   SCLAYOUT[ppu->master->io.bgcnt[bg].size][scy & 1][1]};
    word map_addr = map_start + 0x800 * scs[scx & 1] + 32 * 2 * ty + 2 * tx;
    BgTile tile = {ppu->master->vram.h[(map_addr % 0x10000) >> 1]};
    if (ppu->master->io.bgcnt[bg].palmode) {
        word tile_addr = tile_start + 64 * tile.num;
        hword tmpfy = fy;
        if (tile.vflip) tmpfy = 7 - fy;
        dword row = ppu->master->vram.w[((tile_addr + 8 * tmpfy) % 0x10000) >> 2];
        row |= (dword) ppu->master->vram.w[((tile_addr + 8 * tmpfy + 4) % 0x10000) >> 2] << 32;
        if (tile.hflip) {
            row = (row & 0xffffffff00000000) >> 32 | (row & 0x00000000ffffffff) << 32;
            row = (row & 0xffff0000ffff0000) >> 16 | (row & 0x0000ffff0000ffff) << 16;
            row = (row & 0xff00ff00ff00ff00) >> 8 | (row & 0x00ff00ff00ff00ff) << 8;
        }
        row >>= 8 * fx;
        for (int x = 0; x < GBA_SCREEN_W; x++) {
            byte col_ind = row & 0xff;
            if (col_ind) ppu->layerlines[bg][x] = ppu->master->pram.h[col_ind] & ~(1 << 15);
            else ppu->layerlines[bg][x] = 1 << 15;

            row >>= 8;
            fx++;
            if (fx == 8) {
                fx = 0;
                tx++;
                if (tx == 32) {
                    tx = 0;
                    scx++;
                    map_addr = map_start + 0x800 * scs[scx & 1] + 32 * 2 * ty;
                } else {
                    map_addr += 2;
                }
                tile.h = ppu->master->vram.h[(map_addr % 0x10000) >> 1];
                tile_addr = tile_start + 64 * tile.num;
                tmpfy = fy;
                if (tile.vflip) tmpfy = 7 - fy;
                row = ppu->master->vram.w[((tile_addr + 8 * tmpfy) % 0x10000) >> 2];
                row |= (dword) ppu->master->vram.w[((tile_addr + 8 * tmpfy + 4) % 0x10000) >> 2]
                       << 32;
                if (tile.hflip) {
                    row = (row & 0xffffffff00000000) >> 32 | (row & 0x00000000ffffffff) << 32;
                    row = (row & 0xffff0000ffff0000) >> 16 | (row & 0x0000ffff0000ffff) << 16;
                    row = (row & 0xff00ff00ff00ff00) >> 8 | (row & 0x00ff00ff00ff00ff) << 8;
                }
            }
        }
    } else {
        word tile_addr = tile_start + 32 * tile.num;
        hword tmpfy = fy;
        if (tile.vflip) tmpfy = 7 - fy;
        word row = ppu->master->vram.w[((tile_addr + 4 * tmpfy) % 0x10000) >> 2];
        if (tile.hflip) {
            row = (row & 0xffff0000) >> 16 | (row & 0x0000ffff) << 16;
            row = (row & 0xff00ff00) >> 8 | (row & 0x00ff00ff) << 8;
            row = (row & 0xf0f0f0f0) >> 4 | (row & 0x0f0f0f0f) << 4;
        }
        row >>= 4 * fx;
        for (int x = 0; x < GBA_SCREEN_W; x++) {
            byte col_ind = row & 0xf;
            if (col_ind) {
                col_ind |= tile.palette << 4;
                ppu->layerlines[bg][x] = ppu->master->pram.h[col_ind] & ~(1 << 15);
            } else ppu->layerlines[bg][x] = 1 << 15;

            row >>= 4;
            fx++;
            if (fx == 8) {
                fx = 0;
                tx++;
                if (tx == 32) {
                    tx = 0;
                    scx++;
                    map_addr = map_start + 0x800 * scs[scx & 1] + 32 * 2 * ty;
                } else {
                    map_addr += 2;
                }
                tile.h = ppu->master->vram.h[(map_addr % 0x10000) >> 1];
                tile_addr = tile_start + 32 * tile.num;
                tmpfy = fy;
                if (tile.vflip) tmpfy = 7 - fy;
                row = ppu->master->vram.w[((tile_addr + 4 * tmpfy) % 0x10000) >> 2];
                if (tile.hflip) {
                    row = (row & 0xffff0000) >> 16 | (row & 0x0000ffff) << 16;
                    row = (row & 0xff00ff00) >> 8 | (row & 0x00ff00ff) << 8;
                    row = (row & 0xf0f0f0f0) >> 4 | (row & 0x0f0f0f0f) << 4;
                }
            }
        }
    }
}

void render_bg_line_aff(PPU* ppu, int bg, int mode) {
    if (!(ppu->master->io.dispcnt.bg_enable & (1 << bg))) return;
    ppu->draw_bg[bg] = true;

    word map_start = ppu->master->io.bgcnt[bg].tilemap_base * 0x800;
    word tile_start = ppu->master->io.bgcnt[bg].tile_base * 0x4000;
    word bm_start = (ppu->master->io.dispcnt.frame_sel) ? 0xa000 : 0x0000;

    sword x0, y0;
    if (ppu->master->io.bgcnt[bg].mosaic) {
        x0 = ppu->bgaffintr[bg - 2].mosx;
        y0 = ppu->bgaffintr[bg - 2].mosy;
    } else {
        x0 = ppu->bgaffintr[bg - 2].x;
        y0 = ppu->bgaffintr[bg - 2].y;
    }

    hword size = 1 << (7 + ppu->master->io.bgcnt[bg].size);

    for (int x = 0; x < GBA_SCREEN_W;
         x++, x0 += ppu->master->io.bgaff[bg - 2].pa, y0 += ppu->master->io.bgaff[bg - 2].pc) {
        word sx = x0 >> 8;
        word sy = y0 >> 8;
        if (((mode < 3) && (sx >= size || sy >= size) && !ppu->master->io.bgcnt[bg].overflow) ||
            ((mode == 3 || mode == 4) && (sx >= GBA_SCREEN_W || sy >= GBA_SCREEN_H)) ||
            ((mode == 5) && (sx >= 160 || sy >= 128))) {
            ppu->layerlines[bg][x] = 1 << 15;
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
                    ppu->master->vram.b[(map_start + tiley * (size >> 3) + tilex) % 0x10000];
                col_ind =
                    ppu->master->vram.b[(tile_start + 64 * tile + finey * 8 + finex) % 0x10000];
                break;
            case 3:
                pal = false;
                ppu->layerlines[bg][x] = ppu->master->vram.h[sy * GBA_SCREEN_W + sx] & ~(1 << 15);
                break;
            case 4:
                col_ind = ppu->master->vram.b[bm_start + sy * GBA_SCREEN_W + sx];
                break;
            case 5:
                pal = false;
                ppu->layerlines[bg][x] =
                    ppu->master->vram.h[(bm_start >> 1) + sy * 160 + sx] & ~(1 << 15);
                break;
        }
        if (pal) {
            if (col_ind) ppu->layerlines[bg][x] = ppu->master->pram.h[col_ind] & ~(1 << 15);
            else ppu->layerlines[bg][x] = 1 << 15;
        }
    }
}

void render_bgs(PPU* ppu) {
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

void render_obj_line(PPU* ppu, int i) {
    ObjAttr o = ppu->master->oam.objs[i];
    byte w, h;
    switch (o.shape) {
        case OBJ_SHAPE_SQR:
            w = h = OBJLAYOUT[o.size][0];
            break;
        case OBJ_SHAPE_HORZ:
            w = OBJLAYOUT[o.size][2];
            h = OBJLAYOUT[o.size][1];
            break;
        case OBJ_SHAPE_VERT:
            w = OBJLAYOUT[o.size][1];
            h = OBJLAYOUT[o.size][2];
            break;
        default:
            return;
    }
    if (o.disable_double) {
        if (o.aff) {
            w *= 2;
            h *= 2;
        } else return;
    }

    byte yofs = ppu->ly - (byte) o.y;
    if (yofs >= h) return;

    if (o.mosaic) {
        yofs = ppu->objmos_y - (byte) o.y;
        ppu->obj_mos = true;
    }
    if (o.mode == OBJ_MODE_SEMITRANS) ppu->obj_semitrans = true;

    word tile_start = o.tilenum * 32;

    if (ppu->master->io.dispcnt.bg_mode > 2 && tile_start < 0x4000) return;

    if (o.aff) {
        ppu->obj_cycles -= 10 + 2 * w;

        byte ow = w;
        byte oh = h;
        if (o.disable_double) {
            ow /= 2;
            oh /= 2;
        }

        shword pa = ppu->master->oam.objs[4 * o.affparamind + 0].affparam;
        shword pb = ppu->master->oam.objs[4 * o.affparamind + 1].affparam;
        shword pc = ppu->master->oam.objs[4 * o.affparamind + 2].affparam;
        shword pd = ppu->master->oam.objs[4 * o.affparamind + 3].affparam;

        sword x0 = pa * (-w / 2) + pb * (yofs - h / 2) + ((ow / 2) << 8);
        sword y0 = pc * (-w / 2) + pd * (yofs - h / 2) + ((oh / 2) << 8);

        for (int x = 0; x < w; x++, x0 += pa, y0 += pc) {
            hword sx = (o.x + x) % 512;
            if (sx >= GBA_SCREEN_W) continue;

            hword ty = y0 >> 11;
            hword fy = (y0 >> 8) & 0b111;
            hword tx = x0 >> 11;
            hword fx = (x0 >> 8) & 0b111;

            hword col;

            if (ty >= oh / 8 || tx >= ow / 8) {
                col = 1 << 15;
            } else {
                if (o.palmode) {
                    word tile_addr =
                        tile_start + ty * 64 * (ppu->master->io.dispcnt.obj_mapmode ? ow / 8 : 16);
                    tile_addr += 64 * tx + 8 * fy + fx;
                    byte col_ind = ppu->master->vram.b[0x10000 + tile_addr % 0x8000];
                    if (col_ind) {
                        col = ppu->master->pram.h[0x100 + col_ind] & ~(1 << 15);
                    } else col = 1 << 15;
                } else {
                    word tile_addr =
                        tile_start + ty * 32 * (ppu->master->io.dispcnt.obj_mapmode ? ow / 8 : 32);
                    tile_addr += 32 * tx + 4 * fy + fx / 2;
                    byte col_ind = ppu->master->vram.b[0x10000 + tile_addr % 0x8000];
                    if (fx & 1) col_ind >>= 4;
                    else col_ind &= 0b1111;
                    if (col_ind) {
                        col_ind |= o.palette << 4;
                        col = ppu->master->pram.h[0x100 + col_ind] & ~(1 << 15);
                    } else col = 1 << 15;
                }
            }
            if (o.mode == OBJ_MODE_OBJWIN) {
                if (ppu->master->io.dispcnt.winobj_enable && !(col & (1 << 15))) {
                    ppu->window[sx] = WOBJ;
                }
            } else if (o.priority < ppu->objdotattrs[sx].priority ||
                       (ppu->layerlines[LOBJ][sx] & (1 << 15))) {
                if (!(col & (1 << 15))) {
                    ppu->draw_obj = true;
                    ppu->layerlines[LOBJ][sx] = col;
                    ppu->objdotattrs[sx].semitrans = (o.mode == OBJ_MODE_SEMITRANS) ? 1 : 0;
                }
                ppu->objdotattrs[sx].mosaic = o.mosaic;
                ppu->objdotattrs[sx].priority = o.priority;
            }
        }
    } else {
        ppu->obj_cycles -= w;

        word tile_addr = tile_start;

        if (o.vflip) yofs = h - 1 - yofs;
        byte ty = yofs >> 3;
        byte fy = yofs & 0b111;

        byte fx = 0;
        if (o.palmode) {
            tile_addr += ty * 64 * (ppu->master->io.dispcnt.obj_mapmode ? w / 8 : 16);
            dword row;
            if (o.hflip) {
                tile_addr += 64 * (w / 8 - 1);
                row = ppu->master->vram.w[(0x10000 + (tile_addr + 8 * fy) % 0x8000) >> 2];
                row |=
                    (dword) ppu->master->vram.w[(0x10000 + (tile_addr + 8 * fy + 4) % 0x8000) >> 2]
                    << 32;
                row = (row & 0xffffffff00000000) >> 32 | (row & 0x00000000ffffffff) << 32;
                row = (row & 0xffff0000ffff0000) >> 16 | (row & 0x0000ffff0000ffff) << 16;
                row = (row & 0xff00ff00ff00ff00) >> 8 | (row & 0x00ff00ff00ff00ff) << 8;
            } else {
                row = ppu->master->vram.w[(0x10000 + (tile_addr + 8 * fy) % 0x8000) >> 2];
                row |=
                    (dword) ppu->master->vram.w[(0x10000 + (tile_addr + 8 * fy + 4) % 0x8000) >> 2]
                    << 32;
            }
            for (int x = 0; x < w; x++) {
                int sx = (o.x + x) % 512;
                if (sx < GBA_SCREEN_W) {
                    byte col_ind = row & 0xff;
                    if (o.mode == OBJ_MODE_OBJWIN) {
                        if (ppu->master->io.dispcnt.winobj_enable && col_ind) {
                            ppu->window[sx] = WOBJ;
                        }
                    } else if (o.priority < ppu->objdotattrs[sx].priority ||
                               (ppu->layerlines[LOBJ][sx] & (1 << 15))) {
                        if (col_ind) {
                            hword col = ppu->master->pram.h[0x100 + col_ind];
                            ppu->draw_obj = true;
                            ppu->layerlines[LOBJ][sx] = col & ~(1 << 15);
                            ppu->objdotattrs[sx].semitrans = (o.mode == OBJ_MODE_SEMITRANS) ? 1 : 0;
                        }
                        ppu->objdotattrs[sx].mosaic = o.mosaic;
                        ppu->objdotattrs[sx].priority = o.priority;
                    }
                }

                row >>= 8;
                fx++;
                if (fx == 8) {
                    fx = 0;
                    if (o.hflip) {
                        tile_addr -= 64;
                        row = ppu->master->vram.w[(0x10000 + (tile_addr + 8 * fy) % 0x8000) >> 2];
                        row |= (dword) ppu->master->vram
                                   .w[(0x10000 + (tile_addr + 8 * fy + 4) % 0x8000) >> 2]
                               << 32;
                        row = (row & 0xffffffff00000000) >> 32 | (row & 0x00000000ffffffff) << 32;
                        row = (row & 0xffff0000ffff0000) >> 16 | (row & 0x0000ffff0000ffff) << 16;
                        row = (row & 0xff00ff00ff00ff00) >> 8 | (row & 0x00ff00ff00ff00ff) << 8;
                    } else {
                        tile_addr += 64;
                        row = ppu->master->vram.w[(0x10000 + (tile_addr + 8 * fy) % 0x8000) >> 2];
                        row |= (dword) ppu->master->vram
                                   .w[(0x10000 + (tile_addr + 8 * fy + 4) % 0x8000) >> 2]
                               << 32;
                    }
                }
            }
        } else {
            tile_addr += ty * 32 * (ppu->master->io.dispcnt.obj_mapmode ? w / 8 : 32);
            word row;
            if (o.hflip) {
                tile_addr += 32 * (w / 8 - 1);
                row = ppu->master->vram.w[(0x10000 + (tile_addr + 4 * fy) % 0x8000) >> 2];
                row = (row & 0xffff0000) >> 16 | (row & 0x0000ffff) << 16;
                row = (row & 0xff00ff00) >> 8 | (row & 0x00ff00ff) << 8;
                row = (row & 0xf0f0f0f0) >> 4 | (row & 0x0f0f0f0f) << 4;
            } else {
                row = ppu->master->vram.w[(0x10000 + (tile_addr + 4 * fy) % 0x8000) >> 2];
            }
            for (int x = 0; x < w; x++) {
                int sx = (o.x + x) % 512;

                if (sx < GBA_SCREEN_W) {
                    byte col_ind = row & 0xf;
                    if (o.mode == OBJ_MODE_OBJWIN) {
                        if (ppu->master->io.dispcnt.winobj_enable && col_ind) {
                            ppu->window[sx] = WOBJ;
                        }
                    } else if (o.priority < ppu->objdotattrs[sx].priority ||
                               (ppu->layerlines[LOBJ][sx] & (1 << 15))) {
                        if (col_ind) {
                            col_ind |= o.palette << 4;
                            hword col = ppu->master->pram.h[0x100 + col_ind];
                            ppu->draw_obj = true;
                            ppu->layerlines[LOBJ][sx] = col & ~(1 << 15);
                            ppu->objdotattrs[sx].semitrans = (o.mode == OBJ_MODE_SEMITRANS) ? 1 : 0;
                        }
                        ppu->objdotattrs[sx].mosaic = o.mosaic;
                        ppu->objdotattrs[sx].priority = o.priority;
                    }
                }
                row >>= 4;
                fx++;
                if (fx == 8) {
                    fx = 0;
                    if (o.hflip) {
                        tile_addr -= 32;
                        row = ppu->master->vram.w[(0x10000 + (tile_addr + 4 * fy) % 0x8000) >> 2];
                        row = (row & 0xffff0000) >> 16 | (row & 0x0000ffff) << 16;
                        row = (row & 0xff00ff00) >> 8 | (row & 0x00ff00ff) << 8;
                        row = (row & 0xf0f0f0f0) >> 4 | (row & 0x0f0f0f0f) << 4;
                    } else {
                        tile_addr += 32;
                        row = ppu->master->vram.w[(0x10000 + (tile_addr + 4 * fy) % 0x8000) >> 2];
                    }
                }
            }
        }
    }
}

void render_objs(PPU* ppu) {
    if (!ppu->master->io.dispcnt.obj_enable) return;

    for (int x = 0; x < GBA_SCREEN_W; x++) {
        ppu->layerlines[LOBJ][x] = 1 << 15;
    }

    ppu->obj_cycles = (ppu->master->io.dispcnt.hblank_free ? GBA_SCREEN_W : DOTS_W) * 4 - 6;

    for (int i = 0; i < 128; i++) {
        render_obj_line(ppu, i);
        if (ppu->obj_cycles <= 0) break;
    }
}

void render_windows(PPU* ppu) {
    if (!ppu->master->io.dispcnt.win_enable) return;

    for (int i = 1; i >= 0; i--) {
        if (!(ppu->master->io.dispcnt.win_enable & (1 << i)) || !ppu->in_win[i]) continue;

        byte x1 = ppu->master->io.winh[i].x1;
        byte x2 = ppu->master->io.winh[i].x2;
        for (byte x = x1; x != x2; x++) {
            if (x < GBA_SCREEN_W) ppu->window[x] = i;
        }
    }
}

void hmosaic_bg(PPU* ppu, int bg) {
    byte mos_ct = -1;
    byte mos_x = 0;
    for (int x = 0; x < GBA_SCREEN_W; x++) {
        ppu->layerlines[bg][x] = ppu->layerlines[bg][mos_x];
        if (++mos_ct == ppu->master->io.mosaic.bg_h) {
            mos_ct = -1;
            mos_x = x + 1;
        }
    }
}

void hmosaic_obj(PPU* ppu) {
    byte mos_ct = -1;
    byte mos_x = 0;
    bool prev_mos = false;
    for (int x = 0; x < GBA_SCREEN_W; x++) {
        if (++mos_ct == ppu->master->io.mosaic.obj_h || !ppu->objdotattrs[x].mosaic || !prev_mos) {
            mos_ct = -1;
            mos_x = x;
            prev_mos = ppu->objdotattrs[x].mosaic;
        }
        ppu->layerlines[LOBJ][x] = ppu->layerlines[LOBJ][mos_x];
    }
}

void compose_lines(PPU* ppu) {
    byte sorted_bgs[4];
    byte bg_prios[4];
    byte bgs = 0;
    for (int i = 0; i < 4; i++) {
        if (!ppu->draw_bg[i]) continue;
        sorted_bgs[bgs] = i;
        bg_prios[bgs] = ppu->master->io.bgcnt[i].priority;
        int j = bgs;
        bgs++;
        while (j > 0 && bg_prios[j] < bg_prios[j - 1]) {
            byte tmp = sorted_bgs[j];
            sorted_bgs[j] = sorted_bgs[j - 1];
            sorted_bgs[j - 1] = tmp;
            tmp = bg_prios[j];
            bg_prios[j] = bg_prios[j - 1];
            bg_prios[j - 1] = tmp;
            j--;
        }
    }

    byte effect = ppu->master->io.bldcnt.effect;
    byte eva = ppu->master->io.bldalpha.eva;
    byte evb = ppu->master->io.bldalpha.evb;
    byte evy = ppu->master->io.bldy.evy;
    if (eva > 16) eva = 16;
    if (evb > 16) evb = 16;
    if (evy > 16) evy = 16;

    if (effect || ppu->obj_semitrans) {
        for (int x = 0; x < GBA_SCREEN_W; x++) {
            byte layers[6];
            bool win_ena =
                ppu->master->io.dispcnt.win_enable || ppu->master->io.dispcnt.winobj_enable;
            byte win = ppu->window[x];
            int l = 0;
            bool put_obj = ppu->draw_obj && !(ppu->layerlines[LOBJ][x] & (1 << 15)) &&
                           (!win_ena || (ppu->master->io.wincnt[win].obj_enable));
            for (int i = 0; i < bgs && l < 2; i++) {
                if (put_obj && ppu->objdotattrs[x].priority <= bg_prios[i]) {
                    put_obj = false;
                    layers[l++] = LOBJ;
                }
                if (!(ppu->layerlines[sorted_bgs[i]][x] & (1 << 15)) &&
                    (!win_ena || (ppu->master->io.wincnt[win].bg_enable & (1 << sorted_bgs[i])))) {
                    layers[l++] = sorted_bgs[i];
                }
            }
            if (put_obj) {
                layers[l++] = LOBJ;
            }
            layers[l++] = LBD;

            hword color1 = ppu->layerlines[layers[0]][x];

            if (layers[0] == LOBJ && ppu->objdotattrs[x].semitrans && l > 1 &&
                (ppu->master->io.bldcnt.target2 & (1 << layers[1]))) {
                byte r1 = color1 & 0x1f;
                byte g1 = (color1 >> 5) & 0x1f;
                byte b1 = (color1 >> 10) & 0x1f;
                hword color2 = ppu->layerlines[layers[1]][x];
                byte r2 = color2 & 0x1f;
                byte g2 = (color2 >> 5) & 0x1f;
                byte b2 = (color2 >> 10) & 0x1f;
                r1 = (eva * r1 + evb * r2) / 16;
                if (r1 > 31) r1 = 31;
                g1 = (eva * g1 + evb * g2) / 16;
                if (g1 > 31) g1 = 31;
                b1 = (eva * b1 + evb * b2) / 16;
                if (b1 > 31) b1 = 31;
                ppu->screen[ppu->ly][x] = (b1 << 10) | (g1 << 5) | r1;
            } else if ((ppu->master->io.bldcnt.target1 & (1 << layers[0])) &&
                       (!win_ena || ppu->master->io.wincnt[win].effects_enable)) {
                byte r1 = color1 & 0x1f;
                byte g1 = (color1 >> 5) & 0x1f;
                byte b1 = (color1 >> 10) & 0x1f;
                switch (effect) {
                    case EFF_ALPHA: {
                        if (l == 1 || !(ppu->master->io.bldcnt.target2 & (1 << layers[1]))) break;
                        hword color2 = ppu->layerlines[layers[1]][x];
                        byte r2 = color2 & 0x1f;
                        byte g2 = (color2 >> 5) & 0x1f;
                        byte b2 = (color2 >> 10) & 0x1f;
                        r1 = (eva * r1 + evb * r2) / 16;
                        if (r1 > 31) r1 = 31;
                        g1 = (eva * g1 + evb * g2) / 16;
                        if (g1 > 31) g1 = 31;
                        b1 = (eva * b1 + evb * b2) / 16;
                        if (b1 > 31) b1 = 31;
                        break;
                    }
                    case EFF_BINC: {
                        r1 += (31 - r1) * evy / 16;
                        g1 += (31 - g1) * evy / 16;
                        b1 += (31 - b1) * evy / 16;
                        break;
                    }
                    case EFF_BDEC: {
                        r1 -= r1 * evy / 16;
                        g1 -= g1 * evy / 16;
                        b1 -= b1 * evy / 16;
                        break;
                    }
                }
                ppu->screen[ppu->ly][x] = (b1 << 10) | (g1 << 5) | r1;
            } else {
                ppu->screen[ppu->ly][x] = color1;
            }
        }
    } else {
        for (int x = 0; x < GBA_SCREEN_W; x++) {
            byte layers[6];
            bool win_ena =
                ppu->master->io.dispcnt.win_enable || ppu->master->io.dispcnt.winobj_enable;
            byte win = ppu->window[x];
            int l = 0;
            bool put_obj = ppu->draw_obj && !(ppu->layerlines[LOBJ][x] & (1 << 15)) &&
                           (!win_ena || (ppu->master->io.wincnt[win].obj_enable));

            for (int i = 0; i < bgs; i++) {
                if (put_obj && ppu->objdotattrs[x].priority <= bg_prios[i]) {
                    put_obj = false;
                    layers[l++] = LOBJ;
                    break;
                }
                if (!(ppu->layerlines[sorted_bgs[i]][x] & (1 << 15)) &&
                    (!win_ena || (ppu->master->io.wincnt[win].bg_enable & (1 << sorted_bgs[i])))) {
                    layers[l++] = sorted_bgs[i];
                    break;
                }
            }
            if (put_obj) {
                layers[l++] = LOBJ;
            }
            layers[l++] = LBD;

            ppu->screen[ppu->ly][x] = ppu->layerlines[layers[0]][x];
        }
    }
}

void draw_scanline(PPU* ppu) {
    for (int x = 0; x < GBA_SCREEN_W; x++) {
        ppu->layerlines[LBD][x] = ppu->master->pram.h[0];
    }

    ppu->draw_bg[0] = false;
    ppu->draw_bg[1] = false;
    ppu->draw_bg[2] = false;
    ppu->draw_bg[3] = false;
    ppu->draw_obj = false;
    ppu->obj_mos = false;
    ppu->obj_semitrans = false;

    if (ppu->master->io.dispcnt.win_enable || ppu->master->io.dispcnt.winobj_enable)
        memset(ppu->window, WOUT, GBA_SCREEN_W);
    render_bgs(ppu);
    render_objs(ppu);
    render_windows(ppu);

    if (ppu->master->io.bgcnt[0].mosaic) hmosaic_bg(ppu, 0);
    if (ppu->master->io.bgcnt[1].mosaic) hmosaic_bg(ppu, 1);
    if (ppu->master->io.bgcnt[2].mosaic) hmosaic_bg(ppu, 2);
    if (ppu->master->io.bgcnt[3].mosaic) hmosaic_bg(ppu, 3);
    if (ppu->obj_mos) hmosaic_obj(ppu);

    compose_lines(ppu);
}

void ppu_hdraw(PPU* ppu) {
    ppu->ly++;
    if (ppu->ly == LINES_H) {
        ppu->ly = 0;
    }
    ppu->master->io.vcount = ppu->ly;

    ppu->master->io.dispstat.hblank = 0;

    if (ppu->ly == ppu->master->io.dispstat.lyc) {
        ppu->master->io.dispstat.vcounteq = 1;
        if (ppu->master->io.dispstat.vcount_irq) ppu->master->io.ifl.vcounteq = 1;
    } else ppu->master->io.dispstat.vcounteq = 0;

    if (ppu->ly == GBA_SCREEN_H) {
        ppu->master->io.dispstat.vblank = 1;
        ppu_vblank(ppu);
    } else if (ppu->ly == LINES_H - 1) {
        ppu->master->io.dispstat.vblank = 0;
        ppu->frame_complete = true;
    }

    for (int i = 0; i < 2; i++) {
        if (ppu->ly == ppu->master->io.winv[i].y1) ppu->in_win[i] = true;
        if (ppu->ly == ppu->master->io.winv[i].y2) ppu->in_win[i] = false;
    }

    if (ppu->master->io.dma[3].cnt.start == DMA_ST_SPEC) {
        if (2 <= ppu->ly && ppu->ly < 162) {
            dma_activate(&ppu->master->dmac, 3);
        } else if (ppu->ly == 162) {
            ppu->master->io.dma[3].cnt.enable = 0;
        }
    }

    if (ppu->ly < GBA_SCREEN_H) {
        if (ppu->master->io.dispcnt.forced_blank) {
            memset(&ppu->screen[ppu->ly][0], 0xff, sizeof ppu->screen[0]);
        } else {
            draw_scanline(ppu);
        }
    }

    add_event(&ppu->master->sched, EVENT_PPU_HBLANK,
              ppu->master->sched.now + 4 * GBA_SCREEN_W + 46);

    add_event(&ppu->master->sched, EVENT_PPU_HDRAW, ppu->master->sched.now + 4 * DOTS_W);
}

void ppu_vblank(PPU* ppu) {
    if (ppu->master->io.dispstat.vblank_irq) ppu->master->io.ifl.vblank = 1;

    ppu->bgaffintr[0].x = ppu->master->io.bgaff[0].x;
    ppu->bgaffintr[0].y = ppu->master->io.bgaff[0].y;
    ppu->bgaffintr[1].x = ppu->master->io.bgaff[1].x;
    ppu->bgaffintr[1].y = ppu->master->io.bgaff[1].y;
    ppu->bgaffintr[0].mosx = ppu->bgaffintr[0].x;
    ppu->bgaffintr[0].mosy = ppu->bgaffintr[0].y;
    ppu->bgaffintr[1].mosx = ppu->bgaffintr[1].x;
    ppu->bgaffintr[1].mosy = ppu->bgaffintr[1].y;

    ppu->bgmos_y = 0;
    ppu->bgmos_ct = -1;
    ppu->objmos_y = 0;
    ppu->objmos_ct = -1;

    for (int i = 0; i < 4; i++) {
        if (ppu->master->io.dma[i].cnt.start == DMA_ST_VBLANK) dma_activate(&ppu->master->dmac, i);
    }
}

void ppu_hblank(PPU* ppu) {

    if (ppu->ly < GBA_SCREEN_H) {
        ppu->bgaffintr[0].x += ppu->master->io.bgaff[0].pb;
        ppu->bgaffintr[0].y += ppu->master->io.bgaff[0].pd;
        ppu->bgaffintr[1].x += ppu->master->io.bgaff[1].pb;
        ppu->bgaffintr[1].y += ppu->master->io.bgaff[1].pd;

        if (++ppu->bgmos_ct == ppu->master->io.mosaic.bg_v) {
            ppu->bgmos_ct = -1;
            ppu->bgmos_y = ppu->ly + 1;
            ppu->bgaffintr[0].mosx = ppu->bgaffintr[0].x;
            ppu->bgaffintr[0].mosy = ppu->bgaffintr[0].y;
            ppu->bgaffintr[1].mosx = ppu->bgaffintr[1].x;
            ppu->bgaffintr[1].mosy = ppu->bgaffintr[1].y;
        }
        if (++ppu->objmos_ct == ppu->master->io.mosaic.obj_v) {
            ppu->objmos_ct = -1;
            ppu->objmos_y = ppu->ly + 1;
        }
        for (int i = 0; i < 4; i++) {
            if (ppu->master->io.dma[i].cnt.start == DMA_ST_HBLANK)
                dma_activate(&ppu->master->dmac, i);
        }
    }

    ppu->master->io.dispstat.hblank = 1;
    if (ppu->master->io.dispstat.hblank_irq) ppu->master->io.ifl.hblank = 1;
}
#include "ppu.h"

#include "gba.h"
#include "io.h"

void draw_bg_line_m0(PPU* ppu) {}

void draw_bg_line_m1(PPU* ppu) {}

void draw_bg_line_m2(PPU* ppu) {}

void draw_bg_line_m3(PPU* ppu) {
    word start_addr = GBA_SCREEN_W * ppu->ly;
    for (int x = 0; x < GBA_SCREEN_W; x++) {
        ppu->screen[ppu->ly][x] = ppu->master->vram.h[start_addr + x];
    }
}

void draw_bg_line_m4(PPU* ppu) {
    word start_addr = (ppu->master->io.dispcnt.frame_sel) ? 0xa000 : 0x0000;
    start_addr += GBA_SCREEN_W * ppu->ly;
    for (int x = 0; x < GBA_SCREEN_W; x++) {
        byte col_ind = ppu->master->vram.b[start_addr + x];
        hword col = ppu->master->cram.h[col_ind];
        ppu->screen[ppu->ly][x] = col;
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

            draw_bg_line(ppu);

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
        ppu->master->io.vcount.ly = ppu->ly;
    }
}
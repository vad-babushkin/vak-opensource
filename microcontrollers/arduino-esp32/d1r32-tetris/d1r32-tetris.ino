/*
 * Tetris (C) Copyright 1995, Vadim Antonov
 * Port to Arduino (C) 2018, Serge Vakulenko
 *
 * Permission to use, copy, modify, and distribute this software
 * and its documentation for any purpose and without fee is hereby
 * granted, provided that the above copyright notice appear in all
 * copies and that both that the copyright notice and this
 * permission notice and warranty disclaimer appear in supporting
 * documentation, and that the name of the author not be used in
 * advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.
 *
 * The author disclaim all warranties with regard to this
 * software, including all implied warranties of merchantability
 * and fitness.  In no event shall the author be liable for any
 * special, indirect or consequential damages or any damages
 * whatsoever resulting from loss of use, data or profits, whether
 * in an action of contract, negligence or other tortious action,
 * arising out of or in connection with the use or performance of
 * this software.
 */
#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>

#define PITWIDTH    12
#define PITDEPTH    21

#define NSHAPES     7
#define NBLOCKS     5

#define FIN         999

typedef struct {
    int x, y;
} coord_t;

typedef struct {
    int dx, dy;
    coord_t p [NBLOCKS];
} shape_t;

const shape_t shape [NSHAPES] = {
/* OOOO */  { 0, 3, { {0,0}, {0,1}, {0,2}, {0,3}, {FIN,FIN} } },

/* O   */   { 1, 2, { {0,0}, {1,0}, {1,1}, {1,2}, {FIN,FIN} } },
/* OOO */

/*  O  */   { 1, 2, { {0,1}, {1,0}, {1,1}, {1,2}, {FIN,FIN} } },
/* OOO */

/*   O */   { 1, 2, { {0,2}, {1,0}, {1,1}, {1,2}, {FIN,FIN} } },
/* OOO */

/*  OO */   { 1, 2, { {0,0}, {0,1}, {1,1}, {1,2}, {FIN,FIN} } },
/* OO  */

/* OO  */   { 1, 2, { {0,1}, {0,2}, {1,0}, {1,1}, {FIN,FIN} } },
/*  OO */

/* OO */    { 1, 1, { {0,0}, {0,1}, {1,0}, {1,1}, {FIN,FIN} } },
/* OO */
};

int pit [PITDEPTH+1] [PITWIDTH];
int pitcnt [PITDEPTH];
coord_t old [NBLOCKS], nnew [NBLOCKS], chk [NBLOCKS];

/*
 * LCD screen.
 */
Adafruit_PCD8544 lcd = Adafruit_PCD8544(
    26,     // D2 - SCLK
    25,     // D3 - DIN
    17,     // D4 - DC
    16,     // D5 - CS
    27);    // D6 - RST

/*
 * Output piece coordinates given its center and angle
 */
void translate(const shape_t *t, coord_t c, int a, coord_t *res)
{
    coord_t org, tmp;
    int yw, xw, i;

    if (a & 1) {        /* 90 deg */
        xw = t->dy;
        yw = t->dx;
    } else {
        xw = t->dx;
        yw = t->dy;
    }
    org = c;
    org.x -= (xw + 1) / 2;
    org.y -= yw / 2;
    if (org.y < 0)
        org.y = 0;
    if (org.y + yw >= PITWIDTH && c.y <= PITWIDTH)
        org.y = PITWIDTH-1 - yw;
    for (i=0; t->p[i].x!=FIN; i++) {
        switch (a) {
        case 0:
            res[i].x = t->p[i].x;
            res[i].y = t->p[i].y;
            break;
        case 3:
            res[i].x = xw - t->p[i].y;
            res[i].y = t->p[i].x;
            break;
        case 2:
            res[i].x = xw - t->p[i].x;
            res[i].y = yw - t->p[i].y;
            break;
        case 1:
            res[i].x = t->p[i].y;
            res[i].y = yw - t->p[i].x;
        }
        res[i].x += org.x;
        res[i].y += org.y;
    }
    res[i].x = res[i].y = FIN;

    do {
        xw = 0;
        for (i=0; res[i+1].x!=FIN; i++) {
            if (res[i].x < res[i+1].x)
                continue;
            if (res[i].x == res[i+1].x && res[i].y <= res[i+1].y)
                continue;
            xw++;
            tmp = res[i];
            res[i] = res[i+1];
            res[i+1] = tmp;
        }
    } while (xw);
}

/*
 * Draw a filled rectangle in the specified color from (x1,y1) to (x2,y2).
 *
 * The best way to fill a rectangle is to take advantage of the "wrap-around" featute
 * built into the Philips PCF8833 controller. By defining a drawing box, the memory can
 * be simply filled by successive memory writes until all pixels have been illuminated.
 */
void fill_rect(int x0, int y0, int x1, int y1, int color)
{
    /* Temporary solution */
    int xmin, xmax, ymin, ymax, x, y;

    /* calculate the min and max for x and y directions */
    if (x0 <= x1) {
        xmin = x0;
        xmax = x1;
    } else {
        xmin = x1;
        xmax = x0;
    }
    if (y0 <= y1) {
        ymin = y0;
        ymax = y1;
    } else {
        ymin = y1;
        ymax = y0;
    }
    for (y=ymin; y<=ymax; y++)
        for (x=xmin; x<=xmax; x++)
            lcd.drawPixel(x, y, color);
}

/*
 * Draw the block
 */
void draw_block(int h, int w, int visible)
{
    h *= 4;
    w *= 4;
    if (visible) {
        fill_rect(LCDWIDTH-1 - h, w, LCDWIDTH-1 - (h + 3), w + 3, 1);
    } else {
        fill_rect(LCDWIDTH-1 - h, w, LCDWIDTH-1 - (h + 3), w + 3, 0);

        if (h == (PITDEPTH-1)*5)
            lcd.drawPixel(LCDWIDTH-1 - (h + 3), w + 2, 1);

        if (w == 0)
            lcd.drawPixel(LCDWIDTH-1 - (h + 2), w, 1);
        else if (w % 16 == 12)
            lcd.drawPixel(LCDWIDTH-1 - (h + 2), w + 3, 1);
    }
}

/*
 * Move the piece
 */
void move(coord_t *old, coord_t *nnew)
{
    for (;;) {
        if (old->x == FIN) {
draw:       if (nnew->x == FIN)
                break;
            if (nnew->x >= 0)
                draw_block(nnew->x, nnew->y, 1);
            nnew++;
            continue;
        }
        if (nnew->x == FIN) {
clear:      if (old->x >= 0)
                draw_block(old->x, old->y, 0);
            old++;
            continue;
        }
        if (old->x > nnew->x)
            goto draw;
        if (old->x < nnew->x)
            goto clear;
        if (old->y > nnew->y)
            goto draw;
        if (old->y != nnew->y)
            goto clear;
        /* old & new at the same place */
        old++;
        nnew++;
    }
}

/*
 * Draw the pit
 */
void clear()
{
    int h, w;

    for (h=0; h<PITDEPTH; h++) {
        for (w=0; w<PITWIDTH; w++) {
            draw_block(h, w, 0);
            pit[h][w] = 0;
        }
        pitcnt[h] = 0;
    }
    for (w=0; w<PITWIDTH; w++)
        pit[PITDEPTH][w] = 1;
}

/*
 * The piece reached the bottom
 */
void stopped(coord_t *c)
{
    int h, nfull, w, k;

    /* Count the full lines */
    nfull = 0;
    for (; c->x!=FIN; c++) {
        if (c->x <= 0) {
            /* Game over. */
            clear();
            return;
        }
        pit[c->x][c->y] = 1;
        ++pitcnt[c->x];
        if (pitcnt[c->x] == PITWIDTH)
            nfull++;
    }
    if (! nfull)
        return;

    /* Clear upper nfull lines */
    for (h=0; h<nfull; h++) {
        for (w=0; w<PITWIDTH; w++) {
            if (pit[h][w]) {
                draw_block(h, w, 0);
            }
        }
    }

    /* Move lines down */
    k = nfull;
    for (h=nfull; h<PITDEPTH && k>0; h++) {
        if (pitcnt[h-k] == PITWIDTH) {
            k--;
            h--;
            continue;
        }
        for (w=0; w<PITWIDTH; w++) {
            if (pit[h][w] != pit[h-k][w]) {
                draw_block(h, w, pit[h-k][w]);
            }
        }
    }

    /* Now fix the pit contents */
    for (h=PITDEPTH-1; h>0; h--) {
        if (pitcnt[h] != PITWIDTH)
            continue;
        memmove(pit[0]+PITWIDTH, pit[0], h * sizeof(pit[0]));
        memset(pit[0], 0, sizeof(pit[0]));
        memmove(pitcnt+1, pitcnt, h * sizeof(pitcnt[0]));
        pitcnt[0] = 0;
        h++;
    }
}

//
// Constants for joystick input.
//
enum {
    JOYSTICK_NONE,
    JOYSTICK_RIGHT,
    JOYSTICK_UP,
    JOYSTICK_DOWN,
    JOYSTICK_LEFT,
    JOYSTICK_SELECT,
};

//
// Read the joystick.
//
int joystick_get()
{
    static int last_button = JOYSTICK_NONE;
    int button;

    // Read pin IO2 (A0) on D1 R32 board.
    int input = analogRead(2);

    // Convert analog value into button.
    if      (input > 1153) button = JOYSTICK_NONE;
    else if (input > 990)  button = JOYSTICK_UP;
    else if (input > 788)  button = JOYSTICK_RIGHT;
    else if (input > 514)  button = JOYSTICK_DOWN;
    else if (input > 140)  button = JOYSTICK_SELECT;
    else                   button = JOYSTICK_LEFT;

    // Wait until state is stable.
    if (button != last_button) {
        last_button = button;
        return JOYSTICK_NONE;
    }
    return button;
}

void setup()
{
    lcd.begin();
    lcd.setContrast(50);
    lcd.clearDisplay();
    lcd.display();
}

void loop()
{
    int ptype;              /* Piece type */
    int angle, anew;        /* Angle */
    int msec;               /* Timeout */
    int key = 0;
    coord_t c, cnew, *cp;
    unsigned up_pressed = 0, left_pressed = 0;
    unsigned right_pressed = 0, down_pressed = 0;

    /* Draw the pit */
    clear();

newpiece:
    ptype = random(NSHAPES);
    angle = random(3);

    c.y = PITWIDTH/2 - 1;
    for (c.x= -2; ; c.x++) {
        translate(&shape[ptype], c, angle, nnew);
        for (cp=nnew; cp->x!=FIN; cp++) {
            if (cp->x >= 0)
                goto ok;
        }
    }
ok:
    /* Draw new piece */
    for (cp=nnew; cp->x!=FIN; cp++) {
        if (cp->x >= 0) {
            draw_block(cp->x, cp->y, 1);
        }
    }
    memcpy(old, nnew, sizeof old);

    msec = 500;
    for (;;) {
        cnew = c;
        anew = angle;

        if (msec <= 0) {
            /* Timeout: move down */
            msec = 500;
            cnew.x++;
            translate(&shape[ptype], cnew, anew, chk);
            for (cp=chk; cp->x!=FIN; cp++) {
                if (cp->x >= 0 && pit[cp->x][cp->y]) {
                    stopped(nnew);
                    goto newpiece;
                }
            }
            goto check;
        }

        lcd.display();
        key = joystick_get();
        if (key != JOYSTICK_RIGHT)
            right_pressed = 0;
        else if (! right_pressed) {
            right_pressed = 1;

            /* Right: rotate */
            if (--anew < 0)
                anew = 3;
            translate(&shape[ptype], cnew, anew, chk);
            goto check;
        }

        if (key != JOYSTICK_UP)
            up_pressed = 0;
        else if (! up_pressed) {
            up_pressed = 1;

            /* Up: move left. */
            if (cnew.y <= 0)
                continue;
            cnew.y--;
            translate(&shape[ptype], cnew, anew, chk);
            goto check;
        }

        if (key != JOYSTICK_DOWN)
            down_pressed = 0;
        else if (! down_pressed) {
            down_pressed = 1;

            /* Down: move right */
            if (cnew.y >= PITWIDTH-1)
                continue;
            cnew.y++;
            translate(&shape[ptype], cnew, anew, chk);
            goto check;
        }

        if (key != JOYSTICK_LEFT)
            left_pressed = 0;
        else if (! left_pressed) {
            left_pressed = 1;

            /* Right: drop */
            for (;;) {
                cnew.x++;
                translate(&shape[ptype], cnew, anew, chk);
                for (cp=chk; cp->x!=FIN; cp++) {
                    if (cp->x >= 0 && pit[cp->x][cp->y]) {
                        cnew.x--;
                        translate(&shape[ptype], cnew, anew, chk);
                        move(nnew, chk);
                        stopped(chk);
                        goto newpiece;
                    }
                }
            }
        }

        delay(10);
        msec -= 10;
        continue;

check:
        for (cp=chk; cp->x!=FIN; cp++) {
            if (cp->y < 0 || cp->y >= PITWIDTH)
                goto done;
        }
        for (cp=chk; cp->x!=FIN; cp++) {
            if (cp->x >= 0 && pit[cp->x][cp->y])
                goto done;
        }
        c = cnew;
        angle = anew;
        memcpy(old, nnew, sizeof old);
        memcpy(nnew, chk, sizeof nnew);
        move(old, nnew);
done:   ;
    }
}

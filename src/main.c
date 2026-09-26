#include <ti/screen.h>
#include <graphx.h>
#include <ti/getkey.h>
#include <fileioc.h>
#include "pieces.h"
#include "piece_renders.h"

#define EMPTY_ROW {0,0,0,0,0,0,0,0}
// typedef union {
//     uint8_t flat[64];
//     uint8_t grid[8][8];
// } Board;
// static Board tiles = { .grid={
//     {BROOK, BKNIGHT, BBISHOP, BQUEEN, BKING, BBISHOP, BKNIGHT, BROOK},
//     {BPAWN, BPAWN, BPAWN, BPAWN, BPAWN, BPAWN, BPAWN, BPAWN}, 
//     EMPTY_ROW, EMPTY_ROW, EMPTY_ROW, EMPTY_ROW,
//     {WPAWN, WPAWN, WPAWN, WPAWN, WPAWN, WPAWN, WPAWN, WPAWN}, 
//     {WROOK, WKNIGHT, WBISHOP, WQUEEN, WKING, WBISHOP, WKNIGHT, WROOK},
// }};
uint8_t tiles[8][8] = {
    {BROOK, BKNIGHT, BBISHOP, BQUEEN, BKING, BBISHOP, BKNIGHT, BROOK},
    {BPAWN, BPAWN, BPAWN, BPAWN, BPAWN, BPAWN, BPAWN, BPAWN}, 
    EMPTY_ROW, EMPTY_ROW, EMPTY_ROW, EMPTY_ROW,
    {WPAWN, WPAWN, WPAWN, WPAWN, WPAWN, WPAWN, WPAWN, WPAWN}, 
    {WROOK, WKNIGHT, WBISHOP, WQUEEN, WKING, WBISHOP, WKNIGHT, WROOK},
};

#define DARK_SQUARE 0xF9           // dark and light square colors are overwritten in main;
#define LIGHT_SQUARE DARK_SQUARE+1 // others are default: https://ce-programming.github.io/toolchain/_images/graphx_palette.png
#define WHITE_PLAYER_CURSOR 0x57
#define BLACK_PLAYER_CURSOR 0xE8

#define PIECE_COLOR_BLACK_PIXEL 0x00
#define PIECE_COLOR_WHITE_PIXEL 0xFF

#define SQUARE_SIDE GFX_LCD_HEIGHT/8                  // 30
#define BOARD_OFFSET (GFX_LCD_WIDTH-GFX_LCD_HEIGHT)/2 // 40

#define SPRITE_WIDTH 24

#define getX(N) (N>>3&0b111)
#define getY(N) (N   &0b111)
#define getFlatCoords(N) (N&0b111111)
#define squareColor(x,y) (DARK_SQUARE+((x+y+1)&1))

// coordinates are represented as 0b00xxxyyy; three bits are enough for the standard board
static uint8_t cursors[2] = {0b100110,0b100001};
static uint8_t cursor_colors[2] = {WHITE_PLAYER_CURSOR,BLACK_PLAYER_CURSOR};
static uint8_t cur = 0;
static uint8_t selected = 0; // bit 7 - is anything selected; bit 6 - any, full structure - i0xxxyyy

static bool flip_black_pieces = true;
static bool flip_all_pieces = false;

// SPRITE_TYPE - unsigned long
void drawBitmapSprite(const SPRITE_TYPE sprite[SPRITE_ELEMS], int x, int y, uint8_t invert)
{
    /* the array is read as a contiguous array of bits. 2 bits per pixel */
    uint32_t mask_alpha = 0x80000000u;
    uint32_t mask_col   = 0x40000000u;
    uint8_t  sprite_index = 0;

    int8_t invert_m = (invert^flip_all_pieces&&flip_black_pieces)?-1:1;
    int8_t invert_h = (invert^flip_all_pieces&&flip_black_pieces)?SPRITE_WIDTH:0;

    for (uint8_t row = 0; row < SPRITE_WIDTH; row++) {
        int8_t  color   = -1;
        uint8_t run_len = 0;

        for (uint8_t col = 0; col < SPRITE_WIDTH; col++) {
            if (mask_alpha == 0) {
                mask_alpha = 0x80000000u;
                mask_col   = 0x40000000u;
                sprite_index++;
            }

            SPRITE_TYPE word = sprite[sprite_index];
            int8_t pix_color = (word & mask_col) ? 1 : 0;

            if (word & mask_alpha) {
                if (pix_color == color) {
                    run_len++;
                } else {
                    if (run_len) {
                        gfx_SetColor(color^invert?PIECE_COLOR_WHITE_PIXEL:PIECE_COLOR_BLACK_PIXEL);
                        gfx_FillRectangle(
                            x + (int)(col - run_len),
                            y + (int)row * invert_m + invert_h,
                            (int)run_len,
                            1);
                    }
                    color   = pix_color;
                    run_len = 1;
                }
            } else {
                if (run_len) {
                    gfx_SetColor(color^invert?PIECE_COLOR_WHITE_PIXEL:PIECE_COLOR_BLACK_PIXEL);
                    gfx_FillRectangle(
                        x + (int)(col - run_len),
                        y + (int)row * invert_m + invert_h,
                        (int)run_len,
                        1);
                }
                color   = -1;
                run_len = 0;
            }

            mask_alpha >>= 2;
            mask_col   >>= 2;
        }

        if (run_len) {
            gfx_SetColor(color^invert?PIECE_COLOR_WHITE_PIXEL:PIECE_COLOR_BLACK_PIXEL);
            gfx_FillRectangle(
                x + (int)(SPRITE_WIDTH - run_len),
                y + (int)row * invert_m + invert_h,
                (int)run_len,
                1);
        }
    }
}

void drawSelection(uint8_t x, uint8_t y){
    gfx_SetColor(cursor_colors[cur]);
    gfx_Rectangle(BOARD_OFFSET+SQUARE_SIDE*x,SQUARE_SIDE*y,SQUARE_SIDE,SQUARE_SIDE);
    gfx_Rectangle(BOARD_OFFSET+SQUARE_SIDE*x+1,SQUARE_SIDE*y+1,SQUARE_SIDE-2,SQUARE_SIDE-2);
}
void undrawSelection(uint8_t x, uint8_t y){
    gfx_SetColor(squareColor(x,y));
    gfx_Rectangle(BOARD_OFFSET+SQUARE_SIDE*x,SQUARE_SIDE*y,SQUARE_SIDE,SQUARE_SIDE);
    gfx_Rectangle(BOARD_OFFSET+SQUARE_SIDE*x+1,SQUARE_SIDE*y+1,SQUARE_SIDE-2,SQUARE_SIDE-2);
}
void fillSquare(uint8_t x, uint8_t y){
    gfx_SetColor(squareColor(x,y));
    gfx_FillRectangle(BOARD_OFFSET+SQUARE_SIDE*x,SQUARE_SIDE*y,SQUARE_SIDE,SQUARE_SIDE);
    const SPRITE_TYPE el = tiles[y][x];
    if(el&PIECE_MASK) drawBitmapSprite(pieces[(el&PIECE_MASK)-1],
            BOARD_OFFSET+(SQUARE_SIDE-SPRITE_WIDTH)/2+x*SQUARE_SIDE,
            (SQUARE_SIDE-SPRITE_WIDTH)/2+y*SQUARE_SIDE, 
            (el&COLOR_MASK)>>PIECE_BITS);
}
void renderBoard(){
    for(uint8_t x=0;x<8;x++){
        for(uint8_t y=0;y<8;y++){
            fillSquare(x,y);
        }
    }
}
uint8_t processKey(){
    /* event handling
    buttons:
    arrows - move cursor

    enter - select a tile, then move piece

    mode - flip black pieces
    1-6 - create a white piece
    9 - toggle piece color
    0 - remove piece

    del - save and exit
    */
    const uint8_t cx = getX(cursors[cur]);
    const uint8_t cy = getY(cursors[cur]);
    uint16_t key = os_GetKey();
    undrawSelection(cx,cy);
    switch (key)
    {
    case k_Clear:
        return 1;
    case k_Up:
        cursors[cur]-=1;
        break;
    case k_Down:
        cursors[cur]+=1;
        break;
    case k_Left:
        // integer overflow is not considered as it will still be *somewhere* on the board
        cursors[cur]-=0b1000; 
        break;
    case k_Right:
        cursors[cur]+=0b1000;
        break;
    case k_Enter:
        if(selected&0b10000000){
            const uint8_t p = tiles[getY(selected)][getX(selected)];
            tiles[getY(selected)][getX(selected)] = 0;
            tiles[cy][cx] = p;

            fillSquare(getX(selected),getY(selected));
            fillSquare(cx,cy);

            cur^=1;
            selected=0;
        } else {
            selected = cursors[cur] | 0b10000000;
        }
        break;
    case k_0:
        tiles[cy][cx] = 0;
        fillSquare(cx,cy);
        break;
    case k_9:
        tiles[cy][cx] ^= COLOR_MASK;
        fillSquare(cx,cy);
        return 0;
    case k_Graph:{
        for(uint8_t y=0;y<8;y++){
            for(uint8_t x=0;x<4;x++){
                // if(tiles[y][x]) tiles[y][x]^=COLOR_MASK;
                // if(tiles[y][7-x]) tiles[y][7-x]^=COLOR_MASK;
                
                const uint8_t t = tiles[y][x];
                tiles[y][x]=tiles[7-y][7-x];
                tiles[7-y][7-x]=t;
            }
            flip_all_pieces=!flip_all_pieces;
        }; // fallthrough
    case k_Trace:
        flip_black_pieces=!flip_black_pieces;
        a:
        renderBoard();
        return 0;
    case k_Zoom:
        flip_all_pieces=!flip_all_pieces;
        goto a;
    }
    case k_Del:{
        const uint8_t handle = ti_Open("CHESSBRD", "w");
        ti_Write(tiles, 1, sizeof(tiles), handle);
        ti_Write(&cur, 1, 1, handle);
        ti_Close(handle);
        return 1;}
    }
    if((unsigned)(key-k_1)<6){
        tiles[getY(cursors[cur])][getX(cursors[cur])]=key-k_1+1;
        fillSquare(getX(cursors[cur]),getY(cursors[cur]));
    }
    if(selected&0b10000000)
        drawSelection(getX(selected),getY(selected));
    drawSelection(getX(cursors[cur]),getY(cursors[cur]));
    return 0;
}

int main(void) {
    os_ClrLCDFull();
    gfx_Begin();
    gfx_SetDrawBuffer();

    gfx_palette[DARK_SQUARE] = gfx_RGBTo1555(0xB5,0x88,0x63);
    gfx_palette[LIGHT_SQUARE] = gfx_RGBTo1555(0xF0,0xD9,0xB5);

    const uint8_t handle = ti_Open("CHESSBRD", "r");
    if(handle){
        ti_Read(tiles, 1, sizeof(tiles), handle);
        ti_Read(&cur,1,1,handle);
    }
    ti_Close(handle);
    ti_Delete("CHESSBRD");

    renderBoard();
    drawSelection(getX(cursors[cur]),getY(cursors[cur]));
    
    while(1){
        gfx_BlitBuffer();
        if(processKey()==1)break;
    }
    
    gfx_End();
    return 0;
}
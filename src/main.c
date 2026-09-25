#include <ti/screen.h>
#include <graphx.h>
#include <ti/getkey.h>
#include <fileioc.h>
#include "pieces.h"
#include "piece_renders.h"

// с комментариями для Коли, который ничего не знает о Си

// #define - директива создания макросов. они подставляются в код при компиляции и после нее в коде нигде не упоминаются
#define EMPTY_ROW {0,0,0,0,0,0,0,0}
// uint8_t - 8-битное число без знака (расшифровывается: unsigned integer 8 bit - type)
// [8][8] - двумерный массив 8x8
uint8_t tiles[8][8] = {
    {BROOK, BKNIGHT, BBISHOP, BQUEEN, BKING, BBISHOP, BKNIGHT, BROOK},
    {BPAWN, BPAWN, BPAWN, BPAWN, BPAWN, BPAWN, BPAWN, BPAWN}, 
    EMPTY_ROW, EMPTY_ROW, EMPTY_ROW, EMPTY_ROW,
    {WPAWN, WPAWN, WPAWN, WPAWN, WPAWN, WPAWN, WPAWN, WPAWN}, 
    {WROOK, WKNIGHT, WBISHOP, WQUEEN, WKING, WBISHOP, WKNIGHT, WROOK},
};

// калькулятор хранит цвета как индексы в массив цветов (длиной 256).
// эти константы - индексы в этом массиве
#define DARK_SQUARE 0xF9           // этот и следующий цвет перезаписаны в main(), остальные 
#define LIGHT_SQUARE DARK_SQUARE+1 // взяты из https://ce-programming.github.io/toolchain/_images/graphx_palette.png
#define WHITE_PLAYER_CURSOR 0x57   // макросы используются для констант, даже если они нужны только один раз
#define BLACK_PLAYER_CURSOR 0xE8

#define PIECE_COLOR_BLACK_PIXEL 0x00
#define PIECE_COLOR_WHITE_PIXEL 0xFF

#define SQUARE_SIDE GFX_LCD_HEIGHT/8                  // 30
#define BOARD_OFFSET (GFX_LCD_WIDTH-GFX_LCD_HEIGHT)/2 // 40

#define SPRITE_WIDTH 24

#define getX(N) (N>>3&0b111) // макросы-функции (getX(123) компилятором распаковывается в 123>>3&0b111)
#define getY(N) (N   &0b111)
#define squareColor(x,y) (DARK_SQUARE+(x+y+1)%2)

// три бита на X, три бита на Y
// чтобы лучше понять, попробуй почитать https://www.cs.cornell.edu/courses/cs3410/2024fa/notes/bitpack.html
// я проглянул, вроде статья ровная, и объясняют хорошо
// (только хз, я без переводчика же, а переводчик наверняка фигню какую-то напишет)
uint8_t cursors[2] = {0b100110,0b100001};
uint8_t cursor_colors[2] = {WHITE_PLAYER_CURSOR,BLACK_PLAYER_CURSOR};
uint8_t cur = 0; // индекс
uint8_t selected = 0; // последний бит - выбрали ли мы хоть что-то? остальные биты - как и координаты

// SPRITE_TYPE - 32-битное беззнаковое
void drawBitmapSprite(const SPRITE_TYPE sprite[SPRITE_ELEMS], int x, int y, uint8_t invert)
{
    /* принцип работы:
    массив - строка из бит. читаем биты по очереди, будто они все написаны в строчку.
    для этого читаем два бита за пиксель, для этого есть две маски.
    первая - 1000..0000
    вторая - 0100..0000
    после каждого пикселя обе маски сдвигаются:
    0010..0000
    0001..0000
    когда доходит до нуля, маски перезагружаются и мы переходим на следующий элемент массива.
    */
    uint32_t mask_alpha = 0x80000000u;
    uint32_t mask_col   = 0x40000000u;
    uint8_t  sprite_index = 0;

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
                            y + (int)row,
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
                        y + (int)row,
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
                y + (int)row,
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
// намного быстрее просто закрасить сверху, чем перерисовывать всю доску
void undrawSelection(uint8_t x, uint8_t y){
    gfx_SetColor(squareColor(x,y));
    gfx_Rectangle(BOARD_OFFSET+SQUARE_SIDE*x,SQUARE_SIDE*y,SQUARE_SIDE,SQUARE_SIDE);
    gfx_Rectangle(BOARD_OFFSET+SQUARE_SIDE*x+1,SQUARE_SIDE*y+1,SQUARE_SIDE-2,SQUARE_SIDE-2);
}
void drawPiece(uint8_t x, uint8_t y){
    const SPRITE_TYPE el = tiles[y][x];
    if(el) drawBitmapSprite(pieces[(el&PIECE_MASK)-1],
            BOARD_OFFSET+(SQUARE_SIDE-SPRITE_WIDTH)/2+x*SQUARE_SIDE,
            (SQUARE_SIDE-SPRITE_WIDTH)/2+y*SQUARE_SIDE, 
            (el&COLOR_MASK)>>PIECE_BITS);
}
void fillSquare(uint8_t x, uint8_t y){
    gfx_SetColor(squareColor(x,y));
    gfx_FillRectangle(BOARD_OFFSET+SQUARE_SIDE*x,SQUARE_SIDE*y,SQUARE_SIDE,SQUARE_SIDE);
}
int processKey(){
    /* обработка событий
    кнопки:
    стрелки - двигать курсор

    enter - выбрать клетку/переставить фигуру

    1-6 - создать белую фигуру (потому что нельзя провести пешку в ферзи, это пока что исправлять не буду)
    9 - поменять цвет фигуры
    0 - убрать фигуру

    del - сохранить доску и выйти
    */
    uint16_t key = os_GetKey();
    undrawSelection(getX(cursors[cur]),getY(cursors[cur]));
    switch (key)
    {
    case k_Clear:
        return 1; // выход
    case k_Up:
        cursors[cur]-=1;
        break;
    case k_Down:
        cursors[cur]+=1;
        break;
    case k_Left:
        // даже если переменная перезаполнится, курсор будет где-то на доске.
        // починить, если требуется, можно будет потом.
        cursors[cur]-=0b1000; 
        break;
    case k_Right:
        cursors[cur]+=0b1000;
        break;
    case k_Enter:
        if(selected&0b10000000){
            const uint8_t p = tiles[getY(selected)][getX(selected)];
            tiles[getY(selected)][getX(selected)] = 0;
            tiles[getY(cursors[cur])][getX(cursors[cur])] = p;

            fillSquare(getX(selected),getY(selected));
            drawPiece(getX(cursors[cur]),getY(cursors[cur]));

            cur^=1;
            selected=0;
        } else {
            selected = cursors[cur] | 0b10000000;
        }
        break;
    case k_0:
        tiles[getY(cursors[cur])][getX(cursors[cur])] = 0;
        fillSquare(getX(cursors[cur]),getY(cursors[cur]));
        break;
    case k_9:
        tiles[getY(cursors[cur])][getX(cursors[cur])] ^= COLOR_MASK;
        drawPiece(getX(cursors[cur]),getY(cursors[cur]));
    case k_Del:{
        const int handle = ti_Open("CHESSBRD", "w");
        ti_Write(tiles, 1, sizeof(tiles), handle);
        ti_Write(&cur, 1, 1, handle);
        ti_Close(handle);
        return 1;}
    }
    if(key>=k_1 && key-k_1<6){
        tiles[getY(cursors[cur])][getX(cursors[cur])]=key-k_1+1;
        fillSquare(getX(cursors[cur]),getY(cursors[cur]));
        drawPiece(getX(cursors[cur]),getY(cursors[cur]));
    }
    if(selected&0b10000000)
        drawSelection(getX(selected),getY(selected));
    drawSelection(getX(cursors[cur]),getY(cursors[cur]));
    return 0;
}
void renderBoard(){
    for(uint8_t x=0;x<8;x++){ // узнаешь, джаваскриптер фигов?)
        for(uint8_t y=0;y<8;y++){
            fillSquare(x,y);
            drawPiece(x,y);
        }
    }
}

int main(void) {
    os_ClrLCDFull();
    gfx_Begin();
    gfx_SetDrawBuffer();

    gfx_palette[DARK_SQUARE] = gfx_RGBTo1555(0xB5,0x88,0x63);
    gfx_palette[LIGHT_SQUARE] = gfx_RGBTo1555(0xF0,0xD9,0xB5);

    const int handle = ti_Open("CHESSBRD", "r");
    if(handle){
        ti_Read(tiles, 1, sizeof(tiles), handle);
        ti_Read(&cur,1,1,handle);
    }
    ti_Close(handle);
    ti_Delete("CHESSBRD");

    // сначала нужно отрисовать 64 клетки и 32 фигуры. это долго - поэтому мы это делаем только единожды
    renderBoard();
    drawSelection(getX(cursors[cur]),getY(cursors[cur]));
    
    while(1){
        gfx_BlitBuffer();
        if(processKey()==1)break;
    }
    
    gfx_End();
    return 0;
}
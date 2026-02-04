/**
 * Snake Arena - Genesis Homebrew with PIZZAZZ!
 */

#include <genesis.h>

#define TILE_EMPTY   0
#define TILE_SNAKE1  1
#define TILE_SNAKE2  2
#define TILE_FOOD    3
#define TILE_WALL    4
#define TILE_HEAD1   5
#define TILE_HEAD2   6

#define ARENA_W      38
#define ARENA_H      24
#define MAX_LEN      200
#define OFFSET_X     1
#define OFFSET_Y     3

static s8 snake1X[MAX_LEN], snake1Y[MAX_LEN];
static s8 snake2X[MAX_LEN], snake2Y[MAX_LEN];
static u8 len1, len2, dir1, dir2, ndir1, ndir2;
static s8 foodX, foodY;
static u8 alive1, alive2;
static u16 score;
static u8 gameState, gameMode;
static u16 frameCount;
static u16 seed = 54321;
static u8 speed = 8;
static u8 combo = 0;
static u8 comboTimer = 0;
static s8 shakeX, shakeY;
static u8 shakeTimer;
static u8 soundEnabled = TRUE;
static u8 titleAnim;
static u8 foodAnim;
static u16 highScore = 0;

// Forward declaration
static u16 rnd(void);

// ============= SOUND =============

static void sfxEat(void) {
    if (!soundEnabled) return;
    PSG_setEnvelope(0, PSG_ENVELOPE_MAX);
    PSG_setFrequency(0, 800 + combo * 100);
}

static void sfxDeath(void) {
    if (!soundEnabled) return;
    // Descending death sound
    for (u8 i = 0; i < 4; i++) {
        PSG_setEnvelope(0, PSG_ENVELOPE_MAX - i);
        PSG_setFrequency(0, 400 - i * 80);
        for (u8 j = 0; j < 5; j++) SYS_doVBlankProcess();
    }
    // Noise crash
    PSG_setEnvelope(3, PSG_ENVELOPE_MAX);
    PSG_setNoise(PSG_NOISE_TYPE_WHITE, PSG_NOISE_FREQ_CLOCK4);
}

static void sfxCombo(void) {
    if (!soundEnabled) return;
    // Quick ascending arpeggio
    u16 notes[] = {523, 659, 784};
    for (u8 i = 0; i < 3; i++) {
        PSG_setEnvelope(0, PSG_ENVELOPE_MAX);
        PSG_setFrequency(0, notes[i]);
        for (u8 j = 0; j < 3; j++) SYS_doVBlankProcess();
    }
    PSG_setEnvelope(0, PSG_ENVELOPE_MIN);
}

static void sfxSilence(void) {
    PSG_setEnvelope(0, PSG_ENVELOPE_MIN);
    PSG_setEnvelope(1, PSG_ENVELOPE_MIN);
    PSG_setEnvelope(3, PSG_ENVELOPE_MIN);
}

// ============= EFFECTS =============

static void startShake(u8 intensity) {
    shakeTimer = intensity;
}

static void updateShake(void) {
    if (shakeTimer > 0) {
        shakeX = (rnd() % 9) - 4;
        shakeY = (rnd() % 9) - 4;
        shakeTimer--;
        VDP_setHorizontalScroll(BG_A, shakeX);
        VDP_setVerticalScroll(BG_A, shakeY);
    } else if (shakeX || shakeY) {
        shakeX = shakeY = 0;
        VDP_setHorizontalScroll(BG_A, 0);
        VDP_setVerticalScroll(BG_A, 0);
    }
}

// ============= GAME =============

static u16 rnd(void) { 
    seed = seed * 1103515245 + 12345; 
    return (seed >> 16) & 0x7FFF; 
}

static void createTiles(void) {
    u32 td[8];
    
    memset(td, 0, 32); 
    VDP_loadTileData(td, TILE_EMPTY, 1, CPU);
    
    // Snake body with pattern
    td[0] = 0x01111110;
    td[1] = 0x11111111;
    td[2] = 0x11111111;
    td[3] = 0x11111111;
    td[4] = 0x11111111;
    td[5] = 0x11111111;
    td[6] = 0x11111111;
    td[7] = 0x01111110;
    VDP_loadTileData(td, TILE_SNAKE1, 1, CPU);
    
    td[0] = 0x02222220;
    td[1] = 0x22222222;
    td[2] = 0x22222222;
    td[3] = 0x22222222;
    td[4] = 0x22222222;
    td[5] = 0x22222222;
    td[6] = 0x22222222;
    td[7] = 0x02222220;
    VDP_loadTileData(td, TILE_SNAKE2, 1, CPU);
    
    // Food - apple shape
    td[0] = 0x00033000;
    td[1] = 0x00033000;
    td[2] = 0x03333330;
    td[3] = 0x33433333;
    td[4] = 0x33333333;
    td[5] = 0x33333333;
    td[6] = 0x03333330;
    td[7] = 0x00333300;
    VDP_loadTileData(td, TILE_FOOD, 1, CPU);
    
    // Wall - brick pattern
    td[0] = 0x44444444;
    td[1] = 0x45454545;
    td[2] = 0x44444444;
    td[3] = 0x54545454;
    td[4] = 0x44444444;
    td[5] = 0x45454545;
    td[6] = 0x44444444;
    td[7] = 0x54545454;
    VDP_loadTileData(td, TILE_WALL, 1, CPU);
    
    // Snake heads with eyes
    td[0] = 0x01111110;
    td[1] = 0x11011011;
    td[2] = 0x11011011;
    td[3] = 0x11111111;
    td[4] = 0x11111111;
    td[5] = 0x11111111;
    td[6] = 0x11111111;
    td[7] = 0x01111110;
    VDP_loadTileData(td, TILE_HEAD1, 1, CPU);
    
    td[0] = 0x02222220;
    td[1] = 0x22022022;
    td[2] = 0x22022022;
    td[3] = 0x22222222;
    td[4] = 0x22222222;
    td[5] = 0x22222222;
    td[6] = 0x22222222;
    td[7] = 0x02222220;
    VDP_loadTileData(td, TILE_HEAD2, 1, CPU);
}

static void spawnFood(void) {
    u8 valid;
    do { 
        foodX = 1 + rnd() % (ARENA_W-2); 
        foodY = 1 + rnd() % (ARENA_H-2);
        valid = TRUE;
        // Don't spawn on snake
        for (u8 i = 0; i < len1 && valid; i++) {
            if (snake1X[i] == foodX && snake1Y[i] == foodY) valid = FALSE;
        }
        for (u8 i = 0; i < len2 && alive2 && valid; i++) {
            if (snake2X[i] == foodX && snake2Y[i] == foodY) valid = FALSE;
        }
    } while(!valid);
    foodAnim = 0;
}

static void initGame(u8 mode) {
    gameMode = mode;
    score = 0;
    speed = 8;
    combo = 0;
    comboTimer = 0;
    
    len1 = 4; dir1 = 1; ndir1 = 1; alive1 = 1;
    for (u8 i = 0; i < len1; i++) { snake1X[i] = 5-i; snake1Y[i] = ARENA_H/2; }
    
    if (mode == 1) {
        len2 = 4; dir2 = 3; ndir2 = 3; alive2 = 1;
        for (u8 i = 0; i < len2; i++) { snake2X[i] = ARENA_W-6+i; snake2Y[i] = ARENA_H/2; }
    } else { alive2 = 0; }
    
    spawnFood();
    gameState = 1;
    sfxSilence();
}

static void moveSnake(s8* sx, s8* sy, u8* len, u8 dir, u8* alive, u8 ate) {
    if (!*alive) return;
    
    s8 nx = sx[0], ny = sy[0];
    if (dir == 0) ny--; else if (dir == 1) nx++; else if (dir == 2) ny++; else nx--;
    
    // Wall check
    if (nx < 1 || nx >= ARENA_W-1 || ny < 1 || ny >= ARENA_H-1) { 
        *alive = 0; 
        sfxDeath();
        startShake(15);
        return; 
    }
    
    // Self check
    for (u8 i = 0; i < *len - 1; i++) {
        if (sx[i] == nx && sy[i] == ny) { 
            *alive = 0;
            sfxDeath();
            startShake(15);
            return; 
        }
    }
    
    // Other snake check (2P mode)
    if (gameMode == 1) {
        s8* ox = (sx == snake1X) ? snake2X : snake1X;
        s8* oy = (sy == snake1Y) ? snake2Y : snake1Y;
        u8 olen = (sx == snake1X) ? len2 : len1;
        for (u8 i = 0; i < olen; i++) {
            if (ox[i] == nx && oy[i] == ny) {
                *alive = 0;
                sfxDeath();
                startShake(15);
                return;
            }
        }
    }
    
    if (ate) {
        for (u8 i = *len; i > 0; i--) { sx[i] = sx[i-1]; sy[i] = sy[i-1]; }
        (*len)++;
    } else {
        for (u8 i = *len - 1; i > 0; i--) { sx[i] = sx[i-1]; sy[i] = sy[i-1]; }
    }
    sx[0] = nx; sy[0] = ny;
}

static void update(void) {
    u16 joy1 = JOY_readJoypad(JOY_1);
    if ((joy1 & BUTTON_UP) && dir1 != 2) ndir1 = 0;
    if ((joy1 & BUTTON_RIGHT) && dir1 != 3) ndir1 = 1;
    if ((joy1 & BUTTON_DOWN) && dir1 != 0) ndir1 = 2;
    if ((joy1 & BUTTON_LEFT) && dir1 != 1) ndir1 = 3;
    
    if (gameMode == 1 && alive2) {
        u16 joy2 = JOY_readJoypad(JOY_2);
        if ((joy2 & BUTTON_UP) && dir2 != 2) ndir2 = 0;
        if ((joy2 & BUTTON_RIGHT) && dir2 != 3) ndir2 = 1;
        if ((joy2 & BUTTON_DOWN) && dir2 != 0) ndir2 = 2;
        if ((joy2 & BUTTON_LEFT) && dir2 != 1) ndir2 = 3;
    }
    
    dir1 = ndir1; dir2 = ndir2;
    
    u8 ate1 = (snake1X[0] + (dir1==1?1:dir1==3?-1:0) == foodX && 
               snake1Y[0] + (dir1==0?-1:dir1==2?1:0) == foodY);
    u8 ate2 = alive2 && (snake2X[0] + (dir2==1?1:dir2==3?-1:0) == foodX && 
               snake2Y[0] + (dir2==0?-1:dir2==2?1:0) == foodY);
    
    moveSnake(snake1X, snake1Y, &len1, dir1, &alive1, ate1);
    if (alive2) moveSnake(snake2X, snake2Y, &len2, dir2, &alive2, ate2);
    
    if (ate1 || ate2) { 
        // Combo system
        if (comboTimer > 0) {
            combo++;
            if (combo >= 3) {
                sfxCombo();
                score += combo * 5;
            }
        } else {
            combo = 1;
        }
        comboTimer = 60;  // Reset combo timer
        
        score += 10 * combo;
        sfxEat();
        spawnFood();
        
        // Speed up every 5 food
        if ((len1 + (alive2 ? len2 : 0)) % 5 == 0 && speed > 3) {
            speed--;
        }
    }
    
    if (comboTimer > 0) comboTimer--;
    else combo = 0;
    
    if (!alive1 && (gameMode == 0 || !alive2)) {
        if (score > highScore) highScore = score;
        gameState = 2;
    }
    if (gameMode == 1 && (!alive1 || !alive2)) {
        gameState = 2;
    }
}

static void draw(void) {
    VDP_clearPlane(BG_A, TRUE);
    
    // Border
    for (u8 x = 0; x < ARENA_W; x++) {
        VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL1, 0, 0, 0, TILE_WALL), OFFSET_X+x, OFFSET_Y);
        VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL1, 0, 0, 0, TILE_WALL), OFFSET_X+x, OFFSET_Y+ARENA_H-1);
    }
    for (u8 y = 0; y < ARENA_H; y++) {
        VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL1, 0, 0, 0, TILE_WALL), OFFSET_X, OFFSET_Y+y);
        VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL1, 0, 0, 0, TILE_WALL), OFFSET_X+ARENA_W-1, OFFSET_Y+y);
    }
    
    // Snake 1
    if (alive1) {
        // Head
        VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL2, 0, 0, 0, TILE_HEAD1), 
                        OFFSET_X+snake1X[0], OFFSET_Y+snake1Y[0]);
        // Body
        for (u8 i = 1; i < len1; i++) {
            VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL2, 0, 0, 0, TILE_SNAKE1), 
                            OFFSET_X+snake1X[i], OFFSET_Y+snake1Y[i]);
        }
    }
    
    // Snake 2
    if (alive2) {
        VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL3, 0, 0, 0, TILE_HEAD2), 
                        OFFSET_X+snake2X[0], OFFSET_Y+snake2Y[0]);
        for (u8 i = 1; i < len2; i++) {
            VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL3, 0, 0, 0, TILE_SNAKE2), 
                            OFFSET_X+snake2X[i], OFFSET_Y+snake2Y[i]);
        }
    }
    
    // Food with pulsing animation
    foodAnim++;
    u8 foodPal = ((foodAnim / 8) % 2) ? PAL0 : PAL1;
    VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(foodPal, 0, 0, 0, TILE_FOOD), OFFSET_X+foodX, OFFSET_Y+foodY);
    
    // HUD
    char buf[20];
    sprintf(buf, "SCORE:%d", score);
    VDP_drawText(buf, 1, 1);
    
    sprintf(buf, "HI:%d", highScore);
    VDP_drawText(buf, 30, 1);
    
    sprintf(buf, "LEN:%d", len1);
    VDP_drawText(buf, 15, 1);
    
    // Combo display
    if (combo >= 2 && comboTimer > 0) {
        sprintf(buf, "x%d COMBO!", combo);
        VDP_drawText(buf, 15, OFFSET_Y + ARENA_H);
    }
    
    // Speed indicator
    if (speed <= 4) {
        VDP_drawText("TURBO!", 34, 1);
    }
}

static void drawTitle(void) {
    VDP_clearPlane(BG_A, TRUE);
    
    // Pulsing title color
    if (titleAnim % 30 < 15) {
        PAL_setColor(1, RGB24_TO_VDPCOLOR(0x00FF00));
    } else {
        PAL_setColor(1, RGB24_TO_VDPCOLOR(0x88FF88));
    }
    
    // Standard title block
    VDP_drawText("====================", 10, 4);
    VDP_drawText("    SNAKE ARENA     ", 10, 5);
    VDP_drawText("====================", 10, 6);
    VDP_drawText("Free Retro Games", 12, 8);
    VDP_drawText("v1.0.0", 17, 9);
    
    // Animated snake
    u8 snakeY = 11 + ((frameCount / 12) % 2);
    for (u8 i = 0; i < 6; i++) {
        VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL2, 0, 0, 0, i == 0 ? TILE_HEAD1 : TILE_SNAKE1), 
                        14 + i, snakeY);
    }
    
    // Player select
    VDP_drawText("--------------------", 10, 13);
    VDP_drawText("START - 1 Player", 12, 15);
    VDP_drawText("    A - 2 Players", 12, 17);
    VDP_drawText("--------------------", 10, 19);
    
    // High score
    char buf[24];
    sprintf(buf, "High Score: %d", highScore);
    VDP_drawText(buf, 13, 21);
    
    // Sound toggle
    VDP_drawText("C: Sound", 16, 23);
    VDP_drawText(soundEnabled ? "[ON] " : "[OFF]", 17, 24);
    
    // Footer
    VDP_drawText("(C) 2026 monteslu", 11, 27);
}

static void drawGameOver(void) {
    // Draw over the game
    VDP_drawText("================", 12, 10);
    
    if (gameMode == 0) {
        VDP_drawText("   GAME OVER   ", 12, 12);
        char buf[20];
        sprintf(buf, "Score: %d", score);
        VDP_drawText(buf, 15, 14);
        if (score >= highScore) {
            VDP_drawText("NEW HIGH SCORE!", 12, 16);
        }
    } else {
        if (alive1) {
            VDP_drawText(" PLAYER 1 WINS! ", 12, 12);
        } else if (alive2) {
            VDP_drawText(" PLAYER 2 WINS! ", 12, 12);
        } else {
            VDP_drawText("   IT'S A TIE!  ", 12, 12);
        }
    }
    
    VDP_drawText("================", 12, 18);
    VDP_drawText("Press START", 14, 22);
}

int main(void) {
    VDP_setScreenWidth320();
    PAL_setColor(0, RGB24_TO_VDPCOLOR(0x000011));  // Dark blue BG
    PAL_setColor(3, RGB24_TO_VDPCOLOR(0xFF0000));  // Food red
    PAL_setColor(4, RGB24_TO_VDPCOLOR(0xFFFFFF));  // Food highlight
    
    PAL_setColor(17, RGB24_TO_VDPCOLOR(0x555555)); // Wall gray
    PAL_setColor(21, RGB24_TO_VDPCOLOR(0x333333)); // Wall dark
    
    PAL_setColor(33, RGB24_TO_VDPCOLOR(0x00FF00)); // P1 green
    PAL_setColor(49, RGB24_TO_VDPCOLOR(0x00AAFF)); // P2 blue
    
    createTiles();
    gameState = 0;
    titleAnim = 0;
    shakeX = shakeY = 0;
    shakeTimer = 0;
    
    u16 lastJoy = 0;
    while(TRUE) {
        u16 joy = JOY_readJoypad(JOY_1);
        u16 pressed = joy & ~lastJoy;
        lastJoy = joy; 
        seed += frameCount;
        
        updateShake();
        if (frameCount % 8 == 0) sfxSilence();
        
        if (gameState == 0) {
            if (titleAnim < 80) titleAnim++;
            if (pressed & BUTTON_START) initGame(0);
            if (pressed & BUTTON_A) initGame(1);
            drawTitle();
        } else if (gameState == 1) {
            if (frameCount % speed == 0) update();
            draw();
        } else {
            drawGameOver();
            if (pressed & BUTTON_START) {
                gameState = 0;
                titleAnim = 0;
            }
        }
        frameCount++;
        SYS_doVBlankProcess();
    }
    return 0;
}

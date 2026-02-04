/**
 * Breakout - Genesis Homebrew
 * With ALL THE PIZZAZZ!
 */

#include <genesis.h>

#define TILE_EMPTY   0
#define TILE_PADDLE  1
#define TILE_BALL    2
#define TILE_BRICK   3
#define TILE_SPARK   4

#define ARENA_W      40
#define ARENA_H      28
#define BRICKS_W     20
#define BRICKS_H     6
#define BRICK_X      10

static s16 paddleX, ballX, ballY;
static s8 ballDX, ballDY;
static u8 bricks[BRICKS_H][BRICKS_W];
static u8 lives, bricksLeft;
static u16 score;
static u8 gameState;
static u16 frameCount;

// PIZZAZZ!
static u8 soundEnabled = 1;
static s16 shakeX = 0, shakeY = 0;
static u8 shakeTimer = 0;
static u16 highScore = 0;
static u8 titleFrame = 0;
static u8 combo = 0;
static u8 comboTimer = 0;
static u8 level = 1;
static u8 ballSpeed = 1;
static u8 paddleFlash = 0;
static u8 sparkX, sparkY, sparkTimer = 0;
static u8 brickFlashX, brickFlashY, brickFlashTimer = 0;
static s16 ball2X, ball2Y;
static s8 ball2DX, ball2DY;
static u8 ball2Active = 0;

static u16 seed = 12345;
static u16 rnd() { seed = seed * 1103515245 + 12345; return (seed >> 16) & 0x7FFF; }

// ============ SOUND EFFECTS ============
static void playBounce() {
    if (!soundEnabled) return;
    PSG_setEnvelope(0, PSG_ENVELOPE_MAX);
    PSG_setFrequency(0, 600 + combo * 50);
    PSG_setEnvelope(0, PSG_ENVELOPE_MIN + 5);
}

static void playBrickHit() {
    if (!soundEnabled) return;
    PSG_setEnvelope(1, PSG_ENVELOPE_MAX);
    PSG_setFrequency(1, 800 + (rnd() % 200));
    PSG_setEnvelope(1, PSG_ENVELOPE_MIN + 6);
}

static void playPaddleHit() {
    if (!soundEnabled) return;
    PSG_setEnvelope(0, PSG_ENVELOPE_MAX);
    PSG_setFrequency(0, 400);
    PSG_setEnvelope(0, PSG_ENVELOPE_MIN + 4);
}

static void playLoseLife() {
    if (!soundEnabled) return;
    for (u8 i = 0; i < 4; i++) {
        PSG_setEnvelope(0, PSG_ENVELOPE_MAX);
        PSG_setFrequency(0, 300 - i * 50);
        for (u16 j = 0; j < 4000; j++) {}
    }
    PSG_setEnvelope(0, PSG_ENVELOPE_MIN);
}

static void playLevelUp() {
    if (!soundEnabled) return;
    for (u8 i = 0; i < 4; i++) {
        PSG_setEnvelope(0, PSG_ENVELOPE_MAX);
        PSG_setFrequency(0, 600 + i * 150);
        for (u16 j = 0; j < 2500; j++) {}
    }
}

static void playCombo() {
    if (!soundEnabled) return;
    PSG_setEnvelope(0, PSG_ENVELOPE_MAX);
    PSG_setFrequency(0, 1200 + combo * 100);
    PSG_setEnvelope(0, PSG_ENVELOPE_MIN + 3);
    PSG_setEnvelope(1, PSG_ENVELOPE_MAX);
    PSG_setFrequency(1, 1500 + combo * 100);
    PSG_setEnvelope(1, PSG_ENVELOPE_MIN + 3);
}

static void playMenuBlip() {
    if (!soundEnabled) return;
    PSG_setEnvelope(0, PSG_ENVELOPE_MAX);
    PSG_setFrequency(0, 1000);
    PSG_setEnvelope(0, PSG_ENVELOPE_MIN + 2);
}

static void updateSound() {
    static u8 fadeTimer = 0;
    fadeTimer++;
    if (fadeTimer > 3) {
        fadeTimer = 0;
        for (u8 ch = 0; ch < 4; ch++) {
            PSG_setEnvelope(ch, PSG_ENVELOPE_MIN);
        }
    }
}

// ============ SCREEN SHAKE ============
static void startShake(u8 intensity) {
    shakeTimer = intensity;
}

static void updateShake() {
    if (shakeTimer > 0) {
        shakeX = (rnd() % (shakeTimer * 2 + 1)) - shakeTimer;
        shakeY = (rnd() % (shakeTimer * 2 + 1)) - shakeTimer;
        shakeTimer--;
    } else {
        shakeX = shakeY = 0;
    }
    VDP_setHorizontalScroll(BG_A, shakeX);
    VDP_setVerticalScroll(BG_A, shakeY);
}

static void spawnSpark(u8 x, u8 y) {
    sparkX = x;
    sparkY = y;
    sparkTimer = 8;
}

static void createTiles() {
    u32 td[8];
    memset(td, 0, 32); VDP_loadTileData(td, TILE_EMPTY, 1, CPU);
    memset(td, 0x11, 32); VDP_loadTileData(td, TILE_PADDLE, 1, CPU);
    memset(td, 0x22, 32); VDP_loadTileData(td, TILE_BALL, 1, CPU);
    memset(td, 0x33, 32); VDP_loadTileData(td, TILE_BRICK, 1, CPU);
    
    // Spark
    memset(td, 0, 32);
    td[1] = 0x00200200;
    td[2] = 0x02020020;
    td[3] = 0x00222200;
    td[4] = 0x00222200;
    td[5] = 0x02020020;
    td[6] = 0x00200200;
    VDP_loadTileData(td, TILE_SPARK, 1, CPU);
}

static void resetBall() {
    ballX = paddleX + 2;
    ballY = ARENA_H - 4;
    ballDX = (rnd() % 2) ? 1 : -1;
    ballDY = -1;
    ballSpeed = 1;
    combo = 0;
}

static void setupLevel() {
    bricksLeft = 0;
    for (u8 y = 0; y < BRICKS_H; y++) {
        for (u8 x = 0; x < BRICKS_W; x++) {
            // Different patterns per level
            u8 hits = BRICKS_H - y;
            if (level > 1 && (x + y) % 3 == 0) hits++;  // Some tougher bricks
            if (level > 3 && y < 2) hits++;  // Top rows extra tough
            
            bricks[y][x] = hits;
            bricksLeft++;
        }
    }
}

static void initGame() {
    lives = 3; 
    score = 0;
    level = 1;
    combo = 0;
    comboTimer = 0;
    ball2Active = 0;
    paddleX = ARENA_W / 2 - 2;
    
    setupLevel();
    resetBall();
    gameState = 1;
}

static void processBallPhysics(s16* bx, s16* by, s8* bdx, s8* bdy) {
    *bx += *bdx;
    *by += *bdy;
    
    // Wall bounces
    if (*bx <= 0) { 
        *bdx = 1; 
        playBounce();
        spawnSpark(*bx, *by);
    }
    if (*bx >= ARENA_W - 1) { 
        *bdx = -1; 
        playBounce();
        spawnSpark(*bx, *by);
    }
    if (*by <= 2) { 
        *bdy = 1; 
        playBounce();
        spawnSpark(*bx, *by);
    }
    
    // Paddle collision
    if (*by == ARENA_H - 3 && *bx >= paddleX && *bx < paddleX + 5) {
        *bdy = -1;
        
        // Angle based on hit position
        s8 hitPos = *bx - paddleX - 2;
        *bdx = hitPos;
        if (*bdx == 0) *bdx = (rnd() % 2) ? 1 : -1;
        
        playPaddleHit();
        paddleFlash = 6;
        spawnSpark(*bx, *by);
        
        // Reset combo on paddle hit
        combo = 0;
    }
    
    // Brick collision
    if (*by >= 3 && *by < 3 + BRICKS_H) {
        s16 brickX = *bx - BRICK_X;
        s16 brickY = *by - 3;
        if (brickX >= 0 && brickX < BRICKS_W && bricks[brickY][brickX] > 0) {
            bricks[brickY][brickX]--;
            
            if (bricks[brickY][brickX] == 0) {
                bricksLeft--;
                
                // Combo!
                if (comboTimer > 0) {
                    combo++;
                    if (combo >= 3) {
                        playCombo();
                        startShake(combo > 5 ? 4 : 2);
                    }
                } else {
                    combo = 1;
                }
                comboTimer = 30;
                
                // Score with combo multiplier
                u16 points = (BRICKS_H - brickY) * 10 * (1 + combo / 2);
                score += points;
            }
            
            brickFlashX = BRICK_X + brickX;
            brickFlashY = 3 + brickY;
            brickFlashTimer = 6;
            
            *bdy = -*bdy;
            playBrickHit();
            startShake(1);
            
            // Easter egg: 10+ combo spawns second ball!
            if (combo == 10 && !ball2Active) {
                ball2Active = 1;
                ball2X = *bx;
                ball2Y = *by;
                ball2DX = -*bdx;
                ball2DY = -*bdy;
                startShake(6);
            }
        }
    }
}

static void update() {
    u16 joy = JOY_readJoypad(JOY_1);
    
    // Faster paddle
    if (joy & BUTTON_LEFT) paddleX -= 3;
    if (joy & BUTTON_RIGHT) paddleX += 3;
    if (paddleX < 1) paddleX = 1;
    if (paddleX > ARENA_W - 5) paddleX = ARENA_W - 5;
    
    // Speed ramping
    u8 speed = ballSpeed;
    if (score > 500) speed = 2;
    if (score > 1500) speed = 3;
    
    for (u8 s = 0; s < speed; s++) {
        processBallPhysics(&ballX, &ballY, &ballDX, &ballDY);
        
        if (ball2Active) {
            processBallPhysics(&ball2X, &ball2Y, &ball2DX, &ball2DY);
            
            // Ball 2 out of bounds
            if (ball2Y >= ARENA_H - 1) {
                ball2Active = 0;
            }
        }
    }
    
    // Ball out of bounds
    if (ballY >= ARENA_H - 1) {
        if (ball2Active) {
            // Switch to ball 2
            ballX = ball2X;
            ballY = ball2Y;
            ballDX = ball2DX;
            ballDY = ball2DY;
            ball2Active = 0;
        } else {
            lives--;
            playLoseLife();
            startShake(8);
            
            if (lives == 0) {
                gameState = 2;
            } else {
                resetBall();
            }
        }
        return;
    }
    
    // Combo timer
    if (comboTimer > 0) comboTimer--;
    
    // Level complete!
    if (bricksLeft == 0) {
        level++;
        playLevelUp();
        startShake(10);
        
        // Bonus points for level clear
        score += level * 100;
        if (lives < 5) lives++;  // Bonus life (max 5)
        
        setupLevel();
        resetBall();
        ball2Active = 0;
    }
}

static void draw() {
    VDP_clearPlane(BG_A, TRUE);
    
    // Bricks with flash effects
    for (u8 y = 0; y < BRICKS_H; y++) {
        for (u8 x = 0; x < BRICKS_W; x++) {
            if (bricks[y][x] > 0) {
                u8 pal = (bricks[y][x] % 3) + 1;
                
                // Flash effect on hit brick
                if (brickFlashTimer > 0 && 
                    brickFlashX == BRICK_X + x && 
                    brickFlashY == 3 + y &&
                    frameCount % 2) {
                    pal = PAL0;
                }
                
                VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(pal, 0, 0, 0, TILE_BRICK), BRICK_X + x, 3 + y);
            }
        }
    }
    if (brickFlashTimer > 0) brickFlashTimer--;
    
    // Paddle with flash
    u8 paddlePal = (paddleFlash > 0 && frameCount % 2) ? PAL0 : PAL2;
    for (u8 i = 0; i < 5; i++)
        VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(paddlePal, 0, 0, 0, TILE_PADDLE), paddleX + i, ARENA_H - 2);
    if (paddleFlash > 0) paddleFlash--;
    
    // Ball(s)
    VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL0, 0, 0, 0, TILE_BALL), ballX, ballY);
    if (ball2Active) {
        VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL3, 0, 0, 0, TILE_BALL), ball2X, ball2Y);
    }
    
    // Spark
    if (sparkTimer > 0) {
        VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL0, 0, 0, 0, TILE_SPARK), sparkX, sparkY);
        sparkTimer--;
    }
    
    // HUD
    char buf[24];
    sprintf(buf, "SCORE:%d", score);
    VDP_drawText(buf, 1, 0);
    
    sprintf(buf, "LV:%d", level);
    VDP_drawText(buf, 16, 0);
    
    sprintf(buf, "LIVES:%d", lives);
    VDP_drawText(buf, 24, 0);
    
    // Combo display
    if (combo > 1) {
        sprintf(buf, "x%d!", combo);
        VDP_drawText(buf, 35, 0);
    }
    
    // Multiball indicator
    if (ball2Active) {
        VDP_drawText("MULTI!", 1, 1);
    }
    
    // Sound indicator
    VDP_drawText(soundEnabled ? "SND" : "---", 1, ARENA_H - 1);
}

static void drawTitle(void) {
    VDP_clearPlane(BG_A, TRUE);
    titleFrame++;
    
    // Pulsing title color
    if (titleFrame % 20 < 10) {
        PAL_setColor(1, RGB24_TO_VDPCOLOR(0xFFFFFF));
    } else {
        PAL_setColor(1, RGB24_TO_VDPCOLOR(0xFF8888));
    }
    
    // Standard title block
    VDP_drawText("====================", 10, 4);
    VDP_drawText("     BREAKOUT       ", 10, 5);
    VDP_drawText("====================", 10, 6);
    VDP_drawText("Free Retro Games", 12, 8);
    VDP_drawText("v1.0.0", 17, 9);
    
    // Demo bricks and ball
    for (u8 x = 12; x < 28; x++) {
        VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL1 + (x % 3), 0, 0, 0, TILE_BRICK), x, 11);
    }
    u8 ballAnimX = 15 + (titleFrame / 3) % 10;
    VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL0, 0, 0, 0, TILE_BALL), ballAnimX, 13);
    
    // Player select (1P only for Breakout)
    VDP_drawText("--------------------", 10, 15);
    VDP_drawText("START - Play Game", 12, 17);
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

static void drawGameOver() {
    draw();
    
    if (frameCount % 10 < 5) {
        VDP_drawText("=== GAME OVER ===", 11, 10);
    }
    
    char buf[24];
    sprintf(buf, "Final Score: %d", score);
    VDP_drawText(buf, 12, 13);
    
    sprintf(buf, "Level Reached: %d", level);
    VDP_drawText(buf, 12, 15);
    
    if (score > highScore) {
        highScore = score;
        VDP_drawText("*** NEW HIGH SCORE! ***", 8, 18);
    }
    
    VDP_drawText("Press START", 14, 22);
}

int main() {
    VDP_setScreenWidth320();
    PAL_setColor(0, RGB24_TO_VDPCOLOR(0x000000));
    PAL_setColor(1, RGB24_TO_VDPCOLOR(0xFFFFFF));
    PAL_setColor(2, RGB24_TO_VDPCOLOR(0xFFFFFF));  // Ball
    
    PAL_setColor(17, RGB24_TO_VDPCOLOR(0xFF0000)); // Red bricks
    PAL_setColor(33, RGB24_TO_VDPCOLOR(0x00FF00)); // Green paddle + bricks
    PAL_setColor(49, RGB24_TO_VDPCOLOR(0x0088FF)); // Blue bricks + ball2
    
    createTiles();
    gameState = 0;
    drawTitle();
    
    u16 lastJoy = 0;
    while(TRUE) {
        u16 joy = JOY_readJoypad(JOY_1);
        u16 pressed = joy & ~lastJoy;
        lastJoy = joy;
        seed += frameCount;
        
        if (gameState == 0) {
            if (pressed & BUTTON_START) {
                playMenuBlip();
                initGame();
            }
            if (pressed & BUTTON_C) {
                soundEnabled = !soundEnabled;
                playMenuBlip();
            }
            drawTitle();
        } else if (gameState == 1) {
            if (frameCount % 2 == 0) update();  // Smoother gameplay
            updateShake();
            updateSound();
            draw();
        } else {
            drawGameOver();
            updateSound();
            if (pressed & BUTTON_START) {
                playMenuBlip();
                gameState = 0;
                VDP_clearPlane(BG_A, TRUE);
                drawTitle();
            }
        }
        frameCount++;
        SYS_doVBlankProcess();
    }
    return 0;
}

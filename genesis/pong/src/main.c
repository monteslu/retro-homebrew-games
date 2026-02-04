/**
 * Pong - Genesis Homebrew
 * With ALL the PIZZAZZ!
 */

#include <genesis.h>

#define TILE_EMPTY      0
#define TILE_PADDLE     1
#define TILE_BALL       2
#define TILE_BORDER     3
#define TILE_SPARK      4

#define ARENA_W         40
#define ARENA_H         26
#define PADDLE_H        5
#define WIN_SCORE       11

static s16 paddle1Y, paddle2Y;
static s16 ballX, ballY;
static s8 ballDX, ballDY;
static u8 score1, score2;
static u8 gameState;
static u8 gameMode;
static u16 frameCount;
static u16 seed = 12345;

// PIZZAZZ variables
static s16 shakeX = 0, shakeY = 0;
static u8 shakeTimer = 0;
static u8 soundEnabled = 1;
static u8 ballSpeed = 1;
static u8 rallyCount = 0;
static u16 highScore1 = 0, highScore2 = 0;
static u8 titleFrame = 0;
static u8 flashPaddle = 0;  // 1 or 2 for which paddle flashes
static u8 flashTimer = 0;
static u8 sparkX, sparkY, sparkTimer = 0;
static u8 comboMultiplier = 1;

static u16 rnd() {
    seed = seed * 1103515245 + 12345;
    return (seed >> 16) & 0x7FFF;
}

// ============ SOUND EFFECTS ============
static void playBounce(u8 intensity) {
    if (!soundEnabled) return;
    PSG_setEnvelope(0, PSG_ENVELOPE_MAX);
    PSG_setFrequency(0, 600 + intensity * 100);
    PSG_setEnvelope(0, PSG_ENVELOPE_MIN + 6);
}

static void playScore() {
    if (!soundEnabled) return;
    PSG_setEnvelope(1, PSG_ENVELOPE_MAX);
    PSG_setFrequency(1, 200);
    PSG_setEnvelope(1, PSG_ENVELOPE_MIN + 10);
    // Noise
    PSG_setEnvelope(3, PSG_ENVELOPE_MAX);
    PSG_setNoise(PSG_NOISE_TYPE_WHITE, PSG_NOISE_FREQ_TONE3);
}

static void playWallBounce() {
    if (!soundEnabled) return;
    PSG_setEnvelope(2, PSG_ENVELOPE_MAX - 6);
    PSG_setFrequency(2, 300);
    PSG_setEnvelope(2, PSG_ENVELOPE_MIN + 3);
}

static void playVictory() {
    if (!soundEnabled) return;
    for (u8 i = 0; i < 4; i++) {
        PSG_setEnvelope(0, PSG_ENVELOPE_MAX);
        PSG_setFrequency(0, 500 + i * 150);
        for (u16 j = 0; j < 4000; j++) {}
    }
    PSG_setEnvelope(0, PSG_ENVELOPE_MAX);
    PSG_setFrequency(0, 1200);
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
    if (fadeTimer > 4) {
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

static void createTiles() {
    u32 tileData[8];
    
    memset(tileData, 0, 32);
    VDP_loadTileData(tileData, TILE_EMPTY, 1, CPU);
    
    memset(tileData, 0x11, 32);
    VDP_loadTileData(tileData, TILE_PADDLE, 1, CPU);
    
    memset(tileData, 0x22, 32);
    VDP_loadTileData(tileData, TILE_BALL, 1, CPU);
    
    memset(tileData, 0x33, 32);
    VDP_loadTileData(tileData, TILE_BORDER, 1, CPU);
    
    // Spark/hit effect
    memset(tileData, 0, 32);
    tileData[1] = 0x00200200;
    tileData[2] = 0x02020020;
    tileData[3] = 0x00222200;
    tileData[4] = 0x00222200;
    tileData[5] = 0x02020020;
    tileData[6] = 0x00200200;
    VDP_loadTileData(tileData, TILE_SPARK, 1, CPU);
}

static void spawnSpark(u8 x, u8 y) {
    sparkX = x;
    sparkY = y;
    sparkTimer = 8;
}

static void resetBall() {
    ballX = ARENA_W / 2;
    ballY = ARENA_H / 2;
    ballDX = (rnd() % 2) ? 1 : -1;
    ballDY = (rnd() % 2) ? 1 : -1;
    ballSpeed = 1;
    rallyCount = 0;
    comboMultiplier = 1;
}

static void initGame(u8 mode) {
    gameMode = mode;
    score1 = score2 = 0;
    paddle1Y = paddle2Y = ARENA_H / 2 - PADDLE_H / 2;
    resetBall();
    gameState = 1;
}

static void updatePaddles() {
    u16 joy1 = JOY_readJoypad(JOY_1);
    if (joy1 & BUTTON_UP) paddle1Y -= 2;  // Faster paddle
    if (joy1 & BUTTON_DOWN) paddle1Y += 2;
    
    if (paddle1Y < 1) paddle1Y = 1;
    if (paddle1Y > ARENA_H - PADDLE_H - 1) paddle1Y = ARENA_H - PADDLE_H - 1;
    
    if (gameMode == 1) {
        u16 joy2 = JOY_readJoypad(JOY_2);
        if (joy2 & BUTTON_UP) paddle2Y -= 2;
        if (joy2 & BUTTON_DOWN) paddle2Y += 2;
    } else {
        // Smarter AI - tracks ball with some delay
        s16 targetY = ballY - PADDLE_H/2;
        if (paddle2Y < targetY) paddle2Y++;
        if (paddle2Y > targetY) paddle2Y--;
    }
    
    if (paddle2Y < 1) paddle2Y = 1;
    if (paddle2Y > ARENA_H - PADDLE_H - 1) paddle2Y = ARENA_H - PADDLE_H - 1;
}

static void updateBall() {
    // Speed ramp based on rally
    u8 speed = 1;
    if (rallyCount > 5) speed = 2;
    if (rallyCount > 10) {
        speed = 2;
        if (frameCount % 2 == 0) speed = 3;  // Even faster!
    }
    
    for (u8 s = 0; s < speed; s++) {
        ballX += ballDX;
        ballY += ballDY;
        
        // Top/bottom bounce
        if (ballY <= 1) {
            ballDY = 1;
            playWallBounce();
            spawnSpark(ballX, ballY);
        }
        if (ballY >= ARENA_H - 2) {
            ballDY = -1;
            playWallBounce();
            spawnSpark(ballX, ballY);
        }
        
        // Paddle 1 hit
        if (ballX == 2 && ballY >= paddle1Y && ballY < paddle1Y + PADDLE_H) {
            ballDX = 1;
            rallyCount++;
            
            // Angle based on where ball hits paddle
            s8 hitPos = ballY - paddle1Y - PADDLE_H/2;
            ballDY = (hitPos > 0) ? 1 : ((hitPos < 0) ? -1 : ballDY);
            
            playBounce(rallyCount);
            spawnSpark(ballX, ballY);
            flashPaddle = 1;
            flashTimer = 6;
            startShake(1);
            
            // Easter egg: 20+ rally gets screen shake
            if (rallyCount == 20) {
                startShake(4);
            }
        }
        
        // Paddle 2 hit
        if (ballX == ARENA_W - 3 && ballY >= paddle2Y && ballY < paddle2Y + PADDLE_H) {
            ballDX = -1;
            rallyCount++;
            
            s8 hitPos = ballY - paddle2Y - PADDLE_H/2;
            ballDY = (hitPos > 0) ? 1 : ((hitPos < 0) ? -1 : ballDY);
            
            playBounce(rallyCount);
            spawnSpark(ballX, ballY);
            flashPaddle = 2;
            flashTimer = 6;
            startShake(1);
        }
        
        // Score
        if (ballX <= 0) {
            score2++;
            playScore();
            startShake(8);
            resetBall();
            return;
        }
        if (ballX >= ARENA_W - 1) {
            score1++;
            playScore();
            startShake(8);
            resetBall();
            return;
        }
    }
    
    if (score1 >= WIN_SCORE || score2 >= WIN_SCORE) {
        gameState = 2;
        if (score1 > highScore1) highScore1 = score1;
        if (score2 > highScore2) highScore2 = score2;
        playVictory();
    }
}

static void draw() {
    VDP_clearPlane(BG_A, TRUE);
    
    // Border with varying color based on rally
    u8 borderPal = PAL1;
    if (rallyCount > 10) borderPal = PAL2;
    if (rallyCount > 15) borderPal = PAL3;
    
    for (u8 x = 0; x < ARENA_W; x++) {
        VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(borderPal, 0, 0, 0, TILE_BORDER), x, 0);
        VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(borderPal, 0, 0, 0, TILE_BORDER), x, ARENA_H-1);
    }
    
    // Center line (dashed)
    for (u8 y = 1; y < ARENA_H - 1; y += 2) {
        VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL1, 0, 0, 0, TILE_BORDER), ARENA_W/2, y);
    }
    
    // Paddles with flash effect
    u8 pal1 = (flashPaddle == 1 && flashTimer > 0 && frameCount % 2) ? PAL0 : PAL2;
    u8 pal2 = (flashPaddle == 2 && flashTimer > 0 && frameCount % 2) ? PAL0 : PAL3;
    
    for (u8 i = 0; i < PADDLE_H; i++) {
        VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(pal1, 0, 0, 0, TILE_PADDLE), 1, paddle1Y + i);
        VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(pal2, 0, 0, 0, TILE_PADDLE), ARENA_W-2, paddle2Y + i);
    }
    
    // Ball with trail effect
    if (rallyCount > 5) {
        // Ghost ball trail
        VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL1, 0, 0, 0, TILE_BALL), ballX - ballDX, ballY - ballDY);
    }
    VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL0, 0, 0, 0, TILE_BALL), ballX, ballY);
    
    // Spark effect
    if (sparkTimer > 0) {
        VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL0, 0, 0, 0, TILE_SPARK), sparkX, sparkY);
        sparkTimer--;
    }
    
    // Score display - big and centered
    char buf[8];
    sprintf(buf, "%d", score1);
    VDP_drawText(buf, 15, 1);
    sprintf(buf, "%d", score2);
    VDP_drawText(buf, 24, 1);
    
    // Rally counter
    if (rallyCount > 3) {
        sprintf(buf, "RALLY:%d", rallyCount);
        VDP_drawText(buf, 16, ARENA_H);
        
        if (rallyCount >= 10) {
            VDP_drawText("HOT!", 17, ARENA_H + 1);
        }
        if (rallyCount >= 20) {
            VDP_drawText("ON FIRE!", 16, ARENA_H + 1);
        }
    }
    
    // Sound indicator
    VDP_drawText(soundEnabled ? "SND" : "---", 1, ARENA_H);
}

static void drawTitle(void) {
    VDP_clearPlane(BG_A, TRUE);
    titleFrame++;
    
    // Pulsing title
    if (titleFrame % 20 < 10) {
        PAL_setColor(1, RGB24_TO_VDPCOLOR(0xFFFFFF));
    } else {
        PAL_setColor(1, RGB24_TO_VDPCOLOR(0x88FFFF));
    }
    
    // Standard title block
    VDP_drawText("====================", 10, 4);
    VDP_drawText("       PONG         ", 10, 5);
    VDP_drawText("====================", 10, 6);
    VDP_drawText("Free Retro Games", 12, 8);
    VDP_drawText("v1.0.0", 17, 9);
    
    // Bouncing ball animation
    u8 animBallX = 10 + (titleFrame / 3) % 20;
    u8 animBallY = 11 + ((titleFrame / 5) % 3);
    VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL0, 0, 0, 0, TILE_BALL), animBallX, animBallY);
    
    // Player select
    VDP_drawText("--------------------", 10, 13);
    VDP_drawText("START - 1 Player", 12, 15);
    VDP_drawText("    A - 2 Players", 12, 17);
    VDP_drawText("--------------------", 10, 19);
    
    VDP_drawText("First to 11 wins!", 11, 21);
    
    // Sound toggle
    VDP_drawText("C: Sound", 16, 23);
    VDP_drawText(soundEnabled ? "[ON] " : "[OFF]", 17, 24);
    
    // Footer
    VDP_drawText("(C) 2026 monteslu", 11, 27);
}

static void drawGameOver() {
    VDP_clearPlane(BG_A, TRUE);
    
    // Flashing winner
    if (frameCount % 8 < 4) {
        if (score1 >= WIN_SCORE) {
            VDP_drawText("*** PLAYER 1 WINS! ***", 9, 10);
        } else {
            VDP_drawText("*** PLAYER 2 WINS! ***", 9, 10);
        }
    }
    
    char buf[24];
    sprintf(buf, "Final Score: %d - %d", score1, score2);
    VDP_drawText(buf, 10, 14);
    
    VDP_drawText("Press START to play again", 7, 20);
}

int main() {
    VDP_setScreenWidth320();
    PAL_setColor(0, RGB24_TO_VDPCOLOR(0x000000));
    PAL_setColor(1, RGB24_TO_VDPCOLOR(0xFFFFFF));  // White for text
    PAL_setColor(2, RGB24_TO_VDPCOLOR(0xFFFFFF));  // White ball
    
    PAL_setColor(17, RGB24_TO_VDPCOLOR(0x444444)); // PAL1: gray border
    PAL_setColor(33, RGB24_TO_VDPCOLOR(0x00FF00)); // PAL2: green P1
    PAL_setColor(49, RGB24_TO_VDPCOLOR(0x0088FF)); // PAL3: blue P2
    
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
                initGame(0);
            }
            if (pressed & BUTTON_A) {
                playMenuBlip();
                initGame(1);
            }
            if (pressed & BUTTON_C) {
                soundEnabled = !soundEnabled;
                playMenuBlip();
            }
            drawTitle();
        } else if (gameState == 1) {
            if (frameCount % 4 == 0) {
                updatePaddles();
                updateBall();
            }
            
            if (flashTimer > 0) flashTimer--;
            updateShake();
            updateSound();
            draw();
        } else {
            drawGameOver();
            if (pressed & BUTTON_START) {
                playMenuBlip();
                gameState = 0;
                drawTitle();
            }
        }
        
        frameCount++;
        SYS_doVBlankProcess();
    }
    return 0;
}

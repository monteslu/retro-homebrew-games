/**
 * Space Shooter - Genesis Homebrew
 * NOW WITH MAXIMUM PIZZAZZ!
 */

#include <genesis.h>

#define TILE_EMPTY   0
#define TILE_PLAYER  1
#define TILE_BULLET  2
#define TILE_ENEMY   3
#define TILE_EXPLODE1 4
#define TILE_EXPLODE2 5
#define TILE_EXPLODE3 6
#define TILE_STAR    7
#define TILE_POWERUP 8

#define ARENA_W      40
#define ARENA_H      28
#define MAX_BULLETS  12
#define MAX_ENEMIES  15
#define MAX_EXPLOSIONS 10
#define MAX_STARS    30

typedef struct {
    s16 x, y;
    u8 frame;
    u8 timer;
} Explosion;

typedef struct {
    s16 x, y;
    u8 speed;
} Star;

static s16 playerX, playerY;
static s16 bulletX[MAX_BULLETS], bulletY[MAX_BULLETS];
static u8 bulletActive[MAX_BULLETS];
static s16 enemyX[MAX_ENEMIES], enemyY[MAX_ENEMIES];
static u8 enemyActive[MAX_ENEMIES];
static u8 enemyHP[MAX_ENEMIES];
static Explosion explosions[MAX_EXPLOSIONS];
static Star stars[MAX_STARS];
static u8 fireCooldown;
static u16 score;
static u8 gameState;
static u16 frameCount;
static u16 seed = 34463;

// PIZZAZZ!
static u8 soundEnabled = 1;
static s16 shakeX = 0, shakeY = 0;
static u8 shakeTimer = 0;
static u16 highScore = 0;
static u8 titleFrame = 0;
static u8 playerFlash = 0;
static u8 combo = 0;
static u8 comboTimer = 0;
static u8 level = 1;
static u16 enemiesKilled = 0;
static u8 powerUpX, powerUpY;
static u8 powerUpActive = 0;
static u8 rapidFire = 0;
static u8 lives = 3;
static u8 invincible = 0;

static u16 rnd() { seed = seed * 1103515245 + 12345; return (seed >> 16) & 0x7FFF; }

// ============ SOUND EFFECTS ============
static void playShoot() {
    if (!soundEnabled) return;
    PSG_setEnvelope(0, PSG_ENVELOPE_MAX);
    PSG_setFrequency(0, 1200);
    PSG_setEnvelope(0, PSG_ENVELOPE_MIN + 4);
}

static void playExplosion() {
    if (!soundEnabled) return;
    PSG_setEnvelope(1, PSG_ENVELOPE_MAX);
    PSG_setFrequency(1, 80 + (rnd() % 40));
    PSG_setEnvelope(1, PSG_ENVELOPE_MIN + 10);
    PSG_setEnvelope(3, PSG_ENVELOPE_MAX);
    PSG_setNoise(PSG_NOISE_TYPE_WHITE, PSG_NOISE_FREQ_CLOCK4);
}

static void playHit() {
    if (!soundEnabled) return;
    PSG_setEnvelope(2, PSG_ENVELOPE_MAX);
    PSG_setFrequency(2, 400);
    PSG_setEnvelope(2, PSG_ENVELOPE_MIN + 5);
}

static void playPowerUp() {
    if (!soundEnabled) return;
    for (u8 i = 0; i < 3; i++) {
        PSG_setEnvelope(0, PSG_ENVELOPE_MAX);
        PSG_setFrequency(0, 800 + i * 300);
        for (u16 j = 0; j < 2000; j++) {}
    }
}

static void playDeath() {
    if (!soundEnabled) return;
    for (u8 i = 0; i < 4; i++) {
        PSG_setEnvelope(0, PSG_ENVELOPE_MAX);
        PSG_setFrequency(0, 300 - i * 50);
        PSG_setEnvelope(3, PSG_ENVELOPE_MAX);
        PSG_setNoise(PSG_NOISE_TYPE_WHITE, PSG_NOISE_FREQ_CLOCK4);
        for (u16 j = 0; j < 3000; j++) {}
    }
}

static void playMenuBlip() {
    if (!soundEnabled) return;
    PSG_setEnvelope(0, PSG_ENVELOPE_MAX);
    PSG_setFrequency(0, 1000);
    PSG_setEnvelope(0, PSG_ENVELOPE_MIN + 2);
}

static void playCombo() {
    if (!soundEnabled) return;
    PSG_setEnvelope(0, PSG_ENVELOPE_MAX);
    PSG_setFrequency(0, 1400 + combo * 100);
    PSG_setEnvelope(0, PSG_ENVELOPE_MIN + 3);
}

static void updateSound() {
    static u8 fadeTimer = 0;
    fadeTimer++;
    if (fadeTimer > 2) {
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

// ============ EXPLOSIONS ============
static void spawnExplosion(s16 x, s16 y) {
    for (u8 i = 0; i < MAX_EXPLOSIONS; i++) {
        if (explosions[i].timer == 0) {
            explosions[i].x = x;
            explosions[i].y = y;
            explosions[i].frame = 0;
            explosions[i].timer = 15;
            return;
        }
    }
}

static void updateExplosions() {
    for (u8 i = 0; i < MAX_EXPLOSIONS; i++) {
        if (explosions[i].timer > 0) {
            explosions[i].timer--;
            explosions[i].frame = (15 - explosions[i].timer) / 5;
        }
    }
}

// ============ STARS (parallax) ============
static void initStars() {
    for (u8 i = 0; i < MAX_STARS; i++) {
        stars[i].x = rnd() % ARENA_W;
        stars[i].y = rnd() % ARENA_H;
        stars[i].speed = 1 + (rnd() % 2);
    }
}

static void updateStars() {
    for (u8 i = 0; i < MAX_STARS; i++) {
        stars[i].y += stars[i].speed;
        if (stars[i].y >= ARENA_H) {
            stars[i].y = 0;
            stars[i].x = rnd() % ARENA_W;
        }
    }
}

static void createTiles() {
    u32 td[8];
    memset(td, 0, 32); VDP_loadTileData(td, TILE_EMPTY, 1, CPU);
    
    // Player ship (triangle pointing up)
    memset(td, 0, 32);
    td[0] = 0x00011000;
    td[1] = 0x00111100;
    td[2] = 0x01111110;
    td[3] = 0x11111111;
    td[4] = 0x11111111;
    td[5] = 0x01100110;
    td[6] = 0x01000010;
    VDP_loadTileData(td, TILE_PLAYER, 1, CPU);
    
    // Bullet
    memset(td, 0, 32);
    td[2] = 0x00011000;
    td[3] = 0x00011000;
    td[4] = 0x00011000;
    td[5] = 0x00011000;
    VDP_loadTileData(td, TILE_BULLET, 1, CPU);
    
    // Enemy (invader style)
    memset(td, 0, 32);
    td[1] = 0x01000010;
    td[2] = 0x00111100;
    td[3] = 0x01111110;
    td[4] = 0x11011011;
    td[5] = 0x11111111;
    td[6] = 0x01011010;
    VDP_loadTileData(td, TILE_ENEMY, 1, CPU);
    
    // Explosion frames
    memset(td, 0, 32);
    td[3] = 0x00033000;
    td[4] = 0x00033000;
    VDP_loadTileData(td, TILE_EXPLODE1, 1, CPU);
    
    memset(td, 0, 32);
    td[2] = 0x00333300;
    td[3] = 0x03333330;
    td[4] = 0x03333330;
    td[5] = 0x00333300;
    VDP_loadTileData(td, TILE_EXPLODE2, 1, CPU);
    
    memset(td, 0x33, 32);
    VDP_loadTileData(td, TILE_EXPLODE3, 1, CPU);
    
    // Star
    memset(td, 0, 32);
    td[3] = 0x00001000;
    VDP_loadTileData(td, TILE_STAR, 1, CPU);
    
    // Power-up
    memset(td, 0, 32);
    td[1] = 0x00222200;
    td[2] = 0x02222220;
    td[3] = 0x22222222;
    td[4] = 0x22222222;
    td[5] = 0x02222220;
    td[6] = 0x00222200;
    VDP_loadTileData(td, TILE_POWERUP, 1, CPU);
}

static void initGame() {
    playerX = ARENA_W / 2;
    playerY = ARENA_H - 3;
    score = 0;
    fireCooldown = 0;
    level = 1;
    enemiesKilled = 0;
    lives = 3;
    invincible = 60;  // Brief invincibility at start
    rapidFire = 0;
    combo = 0;
    comboTimer = 0;
    powerUpActive = 0;
    
    for (u8 i = 0; i < MAX_BULLETS; i++) bulletActive[i] = 0;
    for (u8 i = 0; i < MAX_ENEMIES; i++) enemyActive[i] = 0;
    for (u8 i = 0; i < MAX_EXPLOSIONS; i++) explosions[i].timer = 0;
    
    initStars();
    gameState = 1;
}

static void spawnEnemy() {
    for (u8 i = 0; i < MAX_ENEMIES; i++) {
        if (!enemyActive[i]) {
            enemyX[i] = 2 + rnd() % (ARENA_W - 4);
            enemyY[i] = 0;
            enemyActive[i] = 1;
            enemyHP[i] = 1 + (level / 3);  // Tougher enemies in later levels
            return;
        }
    }
}

static void spawnPowerUp(s16 x, s16 y) {
    if (!powerUpActive && (rnd() % 5) == 0) {  // 20% chance
        powerUpX = x;
        powerUpY = y;
        powerUpActive = 1;
    }
}

static void fireBullet() {
    u8 cooldown = rapidFire ? 3 : 8;
    if (fireCooldown > 0) return;
    
    for (u8 i = 0; i < MAX_BULLETS; i++) {
        if (!bulletActive[i]) {
            bulletX[i] = playerX;
            bulletY[i] = playerY - 1;
            bulletActive[i] = 1;
            fireCooldown = cooldown;
            playShoot();
            return;
        }
    }
}

static void update() {
    u16 joy = JOY_readJoypad(JOY_1);
    
    // Player movement (faster!)
    if (joy & BUTTON_LEFT) playerX -= 2;
    if (joy & BUTTON_RIGHT) playerX += 2;
    if (joy & BUTTON_UP) playerY--;
    if (joy & BUTTON_DOWN) playerY++;
    
    if (playerX < 1) playerX = 1;
    if (playerX > ARENA_W - 2) playerX = ARENA_W - 2;
    if (playerY < 2) playerY = 2;
    if (playerY > ARENA_H - 2) playerY = ARENA_H - 2;
    
    if (joy & (BUTTON_A | BUTTON_B | BUTTON_C)) fireBullet();
    if (fireCooldown > 0) fireCooldown--;
    if (invincible > 0) invincible--;
    if (rapidFire > 0) rapidFire--;
    
    // Combo timer
    if (comboTimer > 0) comboTimer--;
    else combo = 0;
    
    // Update bullets
    for (u8 i = 0; i < MAX_BULLETS; i++) {
        if (bulletActive[i]) {
            bulletY[i] -= 2;  // Faster bullets
            if (bulletY[i] < 0) bulletActive[i] = 0;
        }
    }
    
    // Update power-up
    if (powerUpActive) {
        powerUpY++;
        if (powerUpY >= ARENA_H) powerUpActive = 0;
        
        // Collect power-up
        if (abs(powerUpX - playerX) < 2 && abs(powerUpY - playerY) < 2) {
            powerUpActive = 0;
            rapidFire = 200;  // 5 seconds of rapid fire
            playPowerUp();
            startShake(3);
        }
    }
    
    // Update enemies
    u8 enemySpeed = 4 + level;  // Faster in later levels
    for (u8 i = 0; i < MAX_ENEMIES; i++) {
        if (enemyActive[i]) {
            if (frameCount % enemySpeed == 0) enemyY[i]++;
            
            if (enemyY[i] >= ARENA_H - 1) {
                enemyActive[i] = 0;
            }
            
            // Hit player (if not invincible)
            if (!invincible && abs(enemyX[i] - playerX) < 2 && abs(enemyY[i] - playerY) < 2) {
                lives--;
                invincible = 90;  // 1.5 sec invincibility
                playerFlash = 30;
                startShake(8);
                playDeath();
                spawnExplosion(playerX, playerY);
                
                if (lives <= 0) {
                    gameState = 2;
                    return;
                }
            }
            
            // Hit by bullet
            for (u8 j = 0; j < MAX_BULLETS; j++) {
                if (bulletActive[j] && 
                    abs(bulletX[j] - enemyX[i]) < 2 && 
                    abs(bulletY[j] - enemyY[i]) < 2) {
                    
                    bulletActive[j] = 0;
                    enemyHP[i]--;
                    
                    if (enemyHP[i] <= 0) {
                        enemyActive[i] = 0;
                        
                        // Combo system
                        if (comboTimer > 0) {
                            combo++;
                            if (combo >= 3) {
                                playCombo();
                            }
                        } else {
                            combo = 1;
                        }
                        comboTimer = 45;  // ~0.75 sec window
                        
                        // Score with combo bonus
                        u16 points = 10 * level * (1 + combo / 2);
                        score += points;
                        
                        enemiesKilled++;
                        
                        spawnExplosion(enemyX[i], enemyY[i]);
                        spawnPowerUp(enemyX[i], enemyY[i]);
                        playExplosion();
                        startShake(2);
                        
                        // Level up every 10 kills
                        if (enemiesKilled % 10 == 0) {
                            level++;
                            startShake(5);
                        }
                    } else {
                        playHit();
                    }
                    break;
                }
            }
        }
    }
    
    // Spawn enemies (more frequent in later levels)
    u8 spawnRate = 30 - (level * 2);
    if (spawnRate < 10) spawnRate = 10;
    if (frameCount % spawnRate == 0) spawnEnemy();
    
    updateExplosions();
    updateStars();
}

static void draw() {
    VDP_clearPlane(BG_A, TRUE);
    
    // Stars (background)
    for (u8 i = 0; i < MAX_STARS; i++) {
        u8 pal = stars[i].speed == 1 ? PAL1 : PAL0;
        VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(pal, 0, 0, 0, TILE_STAR), stars[i].x, stars[i].y);
    }
    
    // Player (flash when invincible)
    if (!invincible || frameCount % 4 < 2) {
        u8 pal = (playerFlash > 0 && frameCount % 2) ? PAL0 : PAL2;
        VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(pal, 0, 0, 0, TILE_PLAYER), playerX, playerY);
    }
    if (playerFlash > 0) playerFlash--;
    
    // Bullets
    for (u8 i = 0; i < MAX_BULLETS; i++) {
        if (bulletActive[i])
            VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL0, 0, 0, 0, TILE_BULLET), bulletX[i], bulletY[i]);
    }
    
    // Enemies
    for (u8 i = 0; i < MAX_ENEMIES; i++) {
        if (enemyActive[i])
            VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL1, 0, 0, 0, TILE_ENEMY), enemyX[i], enemyY[i]);
    }
    
    // Power-up (flashing)
    if (powerUpActive && frameCount % 8 < 6) {
        VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL3, 0, 0, 0, TILE_POWERUP), powerUpX, powerUpY);
    }
    
    // Explosions
    for (u8 i = 0; i < MAX_EXPLOSIONS; i++) {
        if (explosions[i].timer > 0) {
            u8 tile = TILE_EXPLODE1 + explosions[i].frame;
            VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL0, 0, 0, 0, tile), explosions[i].x, explosions[i].y);
        }
    }
    
    // HUD
    char buf[20];
    sprintf(buf, "SCORE:%d", score);
    VDP_drawText(buf, 1, 0);
    
    sprintf(buf, "LV:%d", level);
    VDP_drawText(buf, 16, 0);
    
    // Lives display
    sprintf(buf, "LIVES:%d", lives);
    VDP_drawText(buf, 26, 0);
    
    // Combo display
    if (combo > 1) {
        sprintf(buf, "x%d!", combo);
        VDP_drawText(buf, 36, 0);
    }
    
    // Rapid fire indicator
    if (rapidFire > 0) {
        VDP_drawText("RAPID!", 1, 1);
    }
    
    // Sound indicator
    VDP_drawText(soundEnabled ? "SND" : "---", 1, ARENA_H - 1);
}

static void drawTitle(void) {
    VDP_clearPlane(BG_A, TRUE);
    titleFrame++;
    
    // Animated stars background
    updateStars();
    for (u8 i = 0; i < MAX_STARS; i++) {
        VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL1, 0, 0, 0, TILE_STAR), stars[i].x, stars[i].y);
    }
    
    // Pulsing title color
    if (titleFrame % 20 < 10) {
        PAL_setColor(1, RGB24_TO_VDPCOLOR(0xFFFFFF));
    } else {
        PAL_setColor(1, RGB24_TO_VDPCOLOR(0xFFFF00));
    }
    
    // Standard title block
    VDP_drawText("====================", 10, 4);
    VDP_drawText("   SPACE SHOOTER    ", 10, 5);
    VDP_drawText("====================", 10, 6);
    VDP_drawText("Free Retro Games", 12, 8);
    VDP_drawText("v1.0.0", 17, 9);
    
    // Animated ship
    u8 shipY = 12 + (titleFrame / 10) % 3;
    VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL2, 0, 0, 0, TILE_PLAYER), 20, shipY);
    
    // Player select (1P only)
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
    // Keep drawing game state
    draw();
    
    // Overlay
    if (frameCount % 10 < 5) {
        VDP_drawText("=== GAME OVER ===", 11, 10);
    }
    
    char buf[24];
    sprintf(buf, "Final Score: %d", score);
    VDP_drawText(buf, 12, 13);
    
    sprintf(buf, "Level Reached: %d", level);
    VDP_drawText(buf, 12, 15);
    
    sprintf(buf, "Enemies Destroyed: %d", enemiesKilled);
    VDP_drawText(buf, 10, 17);
    
    if (score > highScore) {
        highScore = score;
        VDP_drawText("*** NEW HIGH SCORE! ***", 8, 20);
    }
    
    VDP_drawText("Press START", 14, 24);
}

int main() {
    VDP_setScreenWidth320();
    PAL_setColor(0, RGB24_TO_VDPCOLOR(0x000022));  // Dark blue space
    PAL_setColor(1, RGB24_TO_VDPCOLOR(0xFFFFFF));  // White text
    PAL_setColor(2, RGB24_TO_VDPCOLOR(0xFFFF00));  // Yellow bullets
    PAL_setColor(3, RGB24_TO_VDPCOLOR(0xFF8800));  // Orange explosions
    
    PAL_setColor(17, RGB24_TO_VDPCOLOR(0xFF0000)); // Red enemies
    PAL_setColor(33, RGB24_TO_VDPCOLOR(0x00FF00)); // Green player
    PAL_setColor(49, RGB24_TO_VDPCOLOR(0x00FFFF)); // Cyan power-ups
    
    createTiles();
    initStars();
    gameState = 0;
    drawTitle();
    
    u16 lastJoy = 0;
    while(TRUE) {
        u16 joy = JOY_readJoypad(JOY_1);
        u16 pressed = joy & ~lastJoy;
        lastJoy = joy; seed += frameCount;
        
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
            update();
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

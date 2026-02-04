/**
 * Tank Battle - Genesis Homebrew
 * Classic top-down tank combat with PIZZAZZ!
 * 
 * 1P: Fight AI tank
 * 2P: Deathmatch - first to 5 kills wins
 */

#include <genesis.h>

// Tile indices
#define TILE_EMPTY      0
#define TILE_TANK1      1
#define TILE_TANK2      2
#define TILE_BULLET     3
#define TILE_WALL       4
#define TILE_EXPLODE1   5
#define TILE_EXPLODE2   6
#define TILE_EXPLODE3   7

#define ARENA_W         36
#define ARENA_H         24
#define ARENA_OFFSET_X  2
#define ARENA_OFFSET_Y  3

#define TANK_SPEED      1
#define BULLET_SPEED    2
#define FIRE_COOLDOWN   20
#define MAX_BULLETS     8

#define DIR_UP          0
#define DIR_RIGHT       1
#define DIR_DOWN        2
#define DIR_LEFT        3

#define STATE_TITLE     0
#define STATE_PLAYING   1
#define STATE_ROUNDOVER 2
#define STATE_GAMEOVER  3

typedef struct {
    s16 x, y;
    u8 dir;
    u8 alive;
    u8 fireCooldown;
    u8 score;
    u8 isAI;
    u8 aiTimer;
    u8 flashTimer;
} Tank;

typedef struct {
    s16 x, y;
    s8 dx, dy;
    u8 active;
    u8 owner;
} Bullet;

typedef struct {
    s16 x, y;
    u8 frame;
    u8 timer;
} Explosion;

static Tank tanks[2];
static Bullet bullets[MAX_BULLETS];
static Explosion explosions[8];
static u8 arena[ARENA_H][ARENA_W];
static u8 gameState;
static u8 gameMode;
static u8 winScore;
static u16 frameCount;
static u16 seed = 31337;

// Screen shake
static s16 shakeX = 0, shakeY = 0;
static u8 shakeTimer = 0;

// Sound toggle
static u8 soundEnabled = 1;

// High score
static u16 highScore = 0;

// Title animation
static u8 titleFrame = 0;

static u16 rnd() {
    seed = seed * 1103515245 + 12345;
    return (seed >> 16) & 0x7FFF;
}

// ============ SOUND EFFECTS ============
static void playShoot() {
    if (!soundEnabled) return;
    PSG_setEnvelope(0, PSG_ENVELOPE_MAX);
    PSG_setFrequency(0, 800);
    PSG_setEnvelope(0, PSG_ENVELOPE_MIN + 8);
}

static void playExplosion() {
    if (!soundEnabled) return;
    PSG_setEnvelope(1, PSG_ENVELOPE_MAX);
    PSG_setFrequency(1, 100 + (rnd() % 50));
    PSG_setEnvelope(1, PSG_ENVELOPE_MIN + 12);
    // Noise for explosion
    PSG_setEnvelope(3, PSG_ENVELOPE_MAX);
    PSG_setNoise(PSG_NOISE_TYPE_WHITE, PSG_NOISE_FREQ_TONE3);
}

static void playBounce() {
    if (!soundEnabled) return;
    PSG_setEnvelope(2, PSG_ENVELOPE_MAX - 4);
    PSG_setFrequency(2, 400);
    PSG_setEnvelope(2, PSG_ENVELOPE_MIN + 4);
}

static void playMenuBlip() {
    if (!soundEnabled) return;
    PSG_setEnvelope(0, PSG_ENVELOPE_MAX);
    PSG_setFrequency(0, 1200);
    PSG_setEnvelope(0, PSG_ENVELOPE_MIN + 3);
}

static void playVictoryJingle() {
    if (!soundEnabled) return;
    // Rising arpeggio
    for (u8 i = 0; i < 3; i++) {
        PSG_setEnvelope(0, PSG_ENVELOPE_MAX);
        PSG_setFrequency(0, 600 + i * 200);
        for (u16 j = 0; j < 3000; j++) {}
        PSG_setEnvelope(0, PSG_ENVELOPE_MIN);
    }
    PSG_setEnvelope(0, PSG_ENVELOPE_MAX);
    PSG_setFrequency(0, 1400);
}

static void playGameOverSound() {
    if (!soundEnabled) return;
    // Descending
    for (u8 i = 0; i < 4; i++) {
        PSG_setEnvelope(0, PSG_ENVELOPE_MAX);
        PSG_setFrequency(0, 400 - i * 80);
        for (u16 j = 0; j < 5000; j++) {}
    }
    PSG_setEnvelope(0, PSG_ENVELOPE_MIN);
}

static void updateSound() {
    // Fade out sounds naturally
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

// ============ EXPLOSIONS ============
static void spawnExplosion(s16 x, s16 y) {
    for (u8 i = 0; i < 8; i++) {
        if (explosions[i].timer == 0) {
            explosions[i].x = x;
            explosions[i].y = y;
            explosions[i].frame = 0;
            explosions[i].timer = 20;
            return;
        }
    }
}

static void updateExplosions() {
    for (u8 i = 0; i < 8; i++) {
        if (explosions[i].timer > 0) {
            explosions[i].timer--;
            explosions[i].frame = (20 - explosions[i].timer) / 7;
        }
    }
}

static void generateArena() {
    // Clear arena
    memset(arena, 0, sizeof(arena));
    
    // Border
    for (u8 x = 0; x < ARENA_W; x++) {
        arena[0][x] = 1;
        arena[ARENA_H-1][x] = 1;
    }
    for (u8 y = 0; y < ARENA_H; y++) {
        arena[y][0] = 1;
        arena[y][ARENA_W-1] = 1;
    }
    
    // Random obstacles
    u8 numWalls = 4 + (rnd() % 5);
    for (u8 i = 0; i < numWalls; i++) {
        u8 x = 4 + (rnd() % (ARENA_W - 8));
        u8 y = 4 + (rnd() % (ARENA_H - 8));
        u8 w = 1 + (rnd() % 3);
        u8 h = 1 + (rnd() % 3);
        
        for (u8 dy = 0; dy < h && y+dy < ARENA_H-1; dy++) {
            for (u8 dx = 0; dx < w && x+dx < ARENA_W-1; dx++) {
                arena[y+dy][x+dx] = 1;
            }
        }
    }
    
    // Center obstacle
    arena[ARENA_H/2][ARENA_W/2] = 1;
    arena[ARENA_H/2][ARENA_W/2+1] = 1;
    arena[ARENA_H/2+1][ARENA_W/2] = 1;
    arena[ARENA_H/2+1][ARENA_W/2+1] = 1;
    
    // Clear bullets & explosions
    for (u8 i = 0; i < MAX_BULLETS; i++) {
        bullets[i].active = FALSE;
    }
    for (u8 i = 0; i < 8; i++) {
        explosions[i].timer = 0;
    }
}

static void spawnTank(Tank* t, u8 idx) {
    if (idx == 0) {
        t->x = 3;
        t->y = ARENA_H / 2;
        t->dir = DIR_RIGHT;
    } else {
        t->x = ARENA_W - 4;
        t->y = ARENA_H / 2;
        t->dir = DIR_LEFT;
    }
    t->alive = TRUE;
    t->fireCooldown = 0;
    t->aiTimer = 0;
    t->flashTimer = 0;
}

static bool checkCollision(s16 x, s16 y) {
    if (x < 1 || x >= ARENA_W-1 || y < 1 || y >= ARENA_H-1) return TRUE;
    return arena[y][x] != 0;
}

static void fireBullet(Tank* t, u8 owner) {
    if (t->fireCooldown > 0) return;
    
    for (u8 i = 0; i < MAX_BULLETS; i++) {
        if (!bullets[i].active) {
            Bullet* b = &bullets[i];
            b->x = t->x;
            b->y = t->y;
            b->dx = 0;
            b->dy = 0;
            
            switch (t->dir) {
                case DIR_UP:    b->dy = -BULLET_SPEED; b->y--; break;
                case DIR_DOWN:  b->dy = BULLET_SPEED;  b->y++; break;
                case DIR_LEFT:  b->dx = -BULLET_SPEED; b->x--; break;
                case DIR_RIGHT: b->dx = BULLET_SPEED;  b->x++; break;
            }
            
            b->active = TRUE;
            b->owner = owner;
            t->fireCooldown = FIRE_COOLDOWN;
            playShoot();
            return;
        }
    }
}

static void updateAI(Tank* t, Tank* target, u8 owner) {
    t->aiTimer++;
    
    s16 dx = target->x - t->x;
    s16 dy = target->y - t->y;
    
    // Change direction periodically
    if (t->aiTimer > 20 + (rnd() % 40)) {
        t->aiTimer = 0;
        
        if (rnd() % 3 != 0) {
            if (abs(dx) > abs(dy)) {
                t->dir = (dx > 0) ? DIR_RIGHT : DIR_LEFT;
            } else {
                t->dir = (dy > 0) ? DIR_DOWN : DIR_UP;
            }
        } else {
            t->dir = rnd() % 4;
        }
    }
    
    // Try to move
    s16 nx = t->x, ny = t->y;
    switch (t->dir) {
        case DIR_UP:    ny--; break;
        case DIR_DOWN:  ny++; break;
        case DIR_LEFT:  nx--; break;
        case DIR_RIGHT: nx++; break;
    }
    
    if (!checkCollision(nx, ny) && !(target->alive && nx == target->x && ny == target->y)) {
        t->x = nx;
        t->y = ny;
    } else {
        t->aiTimer = 100; // Change direction sooner
    }
    
    // Fire if aligned
    bool aligned = FALSE;
    if (t->dir == DIR_UP && abs(dx) < 2 && dy < 0) aligned = TRUE;
    if (t->dir == DIR_DOWN && abs(dx) < 2 && dy > 0) aligned = TRUE;
    if (t->dir == DIR_LEFT && abs(dy) < 2 && dx < 0) aligned = TRUE;
    if (t->dir == DIR_RIGHT && abs(dy) < 2 && dx > 0) aligned = TRUE;
    
    if (aligned && (rnd() % 8) == 0) {
        fireBullet(t, owner);
    }
}

static void updateTank(Tank* t, u8 idx) {
    if (!t->alive) return;
    if (t->fireCooldown > 0) t->fireCooldown--;
    if (t->flashTimer > 0) t->flashTimer--;
    
    if (t->isAI) {
        updateAI(t, &tanks[idx == 0 ? 1 : 0], idx);
        return;
    }
    
    u16 joy = JOY_readJoypad(idx == 0 ? JOY_1 : JOY_2);
    s16 nx = t->x, ny = t->y;
    
    if (joy & BUTTON_UP)    { t->dir = DIR_UP;    ny--; }
    if (joy & BUTTON_DOWN)  { t->dir = DIR_DOWN;  ny++; }
    if (joy & BUTTON_LEFT)  { t->dir = DIR_LEFT;  nx--; }
    if (joy & BUTTON_RIGHT) { t->dir = DIR_RIGHT; nx++; }
    
    Tank* other = &tanks[idx == 0 ? 1 : 0];
    if (!checkCollision(nx, ny) && !(other->alive && nx == other->x && ny == other->y)) {
        t->x = nx;
        t->y = ny;
    }
    
    if (joy & (BUTTON_A | BUTTON_B | BUTTON_C)) {
        fireBullet(t, idx);
    }
}

static void updateBullets() {
    for (u8 i = 0; i < MAX_BULLETS; i++) {
        Bullet* b = &bullets[i];
        if (!b->active) continue;
        
        b->x += b->dx;
        b->y += b->dy;
        
        // Wall collision
        if (checkCollision(b->x, b->y)) {
            b->active = FALSE;
            playBounce();
            continue;
        }
        
        // Tank collision
        for (u8 t = 0; t < 2; t++) {
            if (!tanks[t].alive || t == b->owner) continue;
            
            if (b->x == tanks[t].x && b->y == tanks[t].y) {
                tanks[t].alive = FALSE;
                tanks[b->owner].score++;
                b->active = FALSE;
                
                // JUICE!
                spawnExplosion(tanks[t].x, tanks[t].y);
                playExplosion();
                startShake(6);
                
                // Flash winner's tank
                tanks[b->owner].flashTimer = 30;
                break;
            }
        }
    }
}

static void drawArena() {
    VDP_clearPlane(BG_A, TRUE);
    
    // Draw arena
    for (u8 y = 0; y < ARENA_H; y++) {
        for (u8 x = 0; x < ARENA_W; x++) {
            u16 tile = arena[y][x] ? TILE_ATTR_FULL(PAL1, 0, 0, 0, TILE_WALL) : 0;
            VDP_setTileMapXY(BG_A, tile, ARENA_OFFSET_X + x, ARENA_OFFSET_Y + y);
        }
    }
}

static void drawGame() {
    // Clear previous positions
    drawArena();
    
    // Draw tanks with flash effect
    for (u8 i = 0; i < 2; i++) {
        if (!tanks[i].alive) continue;
        
        // Flash white when just scored
        u8 pal = (tanks[i].flashTimer > 0 && (frameCount % 4 < 2)) ? PAL0 : (i == 0 ? PAL2 : PAL3);
        u16 tile = TILE_ATTR_FULL(pal, 0, 0, 0, TILE_TANK1 + i);
        VDP_setTileMapXY(BG_A, tile, ARENA_OFFSET_X + tanks[i].x, ARENA_OFFSET_Y + tanks[i].y);
    }
    
    // Draw bullets
    for (u8 i = 0; i < MAX_BULLETS; i++) {
        if (!bullets[i].active) continue;
        u16 tile = TILE_ATTR_FULL(PAL0, 0, 0, 0, TILE_BULLET);
        VDP_setTileMapXY(BG_A, tile, ARENA_OFFSET_X + bullets[i].x, ARENA_OFFSET_Y + bullets[i].y);
    }
    
    // Draw explosions
    for (u8 i = 0; i < 8; i++) {
        if (explosions[i].timer > 0) {
            u16 tile = TILE_ATTR_FULL(PAL0, 0, 0, 0, TILE_EXPLODE1 + explosions[i].frame);
            VDP_setTileMapXY(BG_A, tile, ARENA_OFFSET_X + explosions[i].x, ARENA_OFFSET_Y + explosions[i].y);
        }
    }
    
    // HUD
    char buf[20];
    sprintf(buf, "P1:%d", tanks[0].score);
    VDP_drawText(buf, 1, 1);
    sprintf(buf, "P2:%d", tanks[1].score);
    VDP_drawText(buf, 34, 1);
    sprintf(buf, "FIRST TO %d", winScore);
    VDP_drawText(buf, 14, 1);
    
    // Sound indicator
    VDP_drawText(soundEnabled ? "SND" : "---", 19, 27);
}

static void drawTitle(void) {
    VDP_clearPlane(BG_A, TRUE);
    
    // Pulsing title color
    if (titleFrame % 30 < 15) {
        PAL_setColor(1, RGB24_TO_VDPCOLOR(0xFFFFFF));
    } else {
        PAL_setColor(1, RGB24_TO_VDPCOLOR(0xFFFF00));
    }
    
    // Standard title block
    VDP_drawText("====================", 10, 4);
    VDP_drawText("    TANK BATTLE     ", 10, 5);
    VDP_drawText("====================", 10, 6);
    VDP_drawText("Free Retro Games", 12, 8);
    VDP_drawText("v1.0.0", 17, 9);
    
    // Animated tanks
    u8 tankPos = 10 + (titleFrame / 8) % 20;
    VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL2, 0, 0, 0, TILE_TANK1), tankPos, 12);
    VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL3, 0, 0, 0, TILE_TANK2), 30 - (titleFrame / 8) % 20, 12);
    
    // Player select
    VDP_drawText("--------------------", 10, 14);
    VDP_drawText("START - 1 Player", 12, 16);
    VDP_drawText("    A - 2 Players", 12, 18);
    VDP_drawText("--------------------", 10, 20);
    
    VDP_drawText("First to 5 wins!", 12, 22);
    
    // Sound toggle
    VDP_drawText("C: Sound", 16, 24);
    VDP_drawText(soundEnabled ? "[ON] " : "[OFF]", 17, 25);
    
    // Footer
    VDP_drawText("(C) 2026 monteslu", 11, 27);
    
    titleFrame++;
}

static void drawRoundOver() {
    // Flash the score
    if (frameCount % 8 < 4) {
        if (!tanks[0].alive) {
            VDP_drawText("** Player 2 scores! **", 9, 12);
        } else {
            VDP_drawText("** Player 1 scores! **", 9, 12);
        }
    }
}

static void drawGameOver() {
    VDP_clearPlane(BG_A, TRUE);
    
    // Flashing GAME OVER
    if (frameCount % 10 < 5) {
        VDP_drawText("===== GAME OVER =====", 9, 8);
    } else {
        VDP_drawText("      GAME OVER      ", 9, 8);
    }
    
    char buf[32];
    sprintf(buf, "Final Score: %d - %d", tanks[0].score, tanks[1].score);
    VDP_drawText(buf, 10, 12);
    
    if (tanks[0].score >= winScore) {
        VDP_drawText("*** PLAYER 1 WINS! ***", 9, 15);
        VDP_drawText("CONGRATULATIONS!", 12, 17);
    } else {
        VDP_drawText("*** PLAYER 2 WINS! ***", 9, 15);
        VDP_drawText("CONGRATULATIONS!", 12, 17);
    }
    
    VDP_drawText("Press START to continue", 8, 22);
}

static void startRound() {
    generateArena();
    spawnTank(&tanks[0], 0);
    spawnTank(&tanks[1], 1);
    gameState = STATE_PLAYING;
}

static void startGame(u8 mode) {
    gameMode = mode;
    winScore = 5;
    tanks[0].score = 0;
    tanks[0].isAI = FALSE;
    tanks[1].score = 0;
    tanks[1].isAI = (mode == 0);
    playMenuBlip();
    startRound();
}

static void createTiles() {
    // Simple solid color tiles
    u32 tileData[8];
    
    // Tile 0: empty (transparent)
    memset(tileData, 0, 32);
    VDP_loadTileData(tileData, TILE_EMPTY, 1, CPU);
    
    // Tile 1: Tank 1 (solid green)
    memset(tileData, 0x11, 32);
    VDP_loadTileData(tileData, TILE_TANK1, 1, CPU);
    
    // Tile 2: Tank 2 (solid blue)
    memset(tileData, 0x22, 32);
    VDP_loadTileData(tileData, TILE_TANK2, 1, CPU);
    
    // Tile 3: Bullet (solid yellow)
    memset(tileData, 0x33, 32);
    VDP_loadTileData(tileData, TILE_BULLET, 1, CPU);
    
    // Tile 4: Wall (solid gray)
    memset(tileData, 0x44, 32);
    VDP_loadTileData(tileData, TILE_WALL, 1, CPU);
    
    // Explosion tiles - expanding pattern
    // Explosion 1 - small
    memset(tileData, 0, 32);
    tileData[3] = 0x00033000;
    tileData[4] = 0x00033000;
    VDP_loadTileData(tileData, TILE_EXPLODE1, 1, CPU);
    
    // Explosion 2 - medium
    memset(tileData, 0, 32);
    tileData[2] = 0x00333300;
    tileData[3] = 0x03333330;
    tileData[4] = 0x03333330;
    tileData[5] = 0x00333300;
    VDP_loadTileData(tileData, TILE_EXPLODE2, 1, CPU);
    
    // Explosion 3 - large
    memset(tileData, 0x33, 32);
    VDP_loadTileData(tileData, TILE_EXPLODE3, 1, CPU);
}

int main() {
    VDP_setScreenWidth320();
    VDP_setBackgroundColor(0);
    
    // Palettes
    PAL_setColor(0, RGB24_TO_VDPCOLOR(0x000000));   // BG black
    PAL_setColor(1, RGB24_TO_VDPCOLOR(0xFFFFFF));   // White text
    PAL_setColor(3, RGB24_TO_VDPCOLOR(0xFFFF00));   // PAL0: yellow (bullets/explosion)
    
    PAL_setColor(17, RGB24_TO_VDPCOLOR(0x888888));  // PAL1: gray (walls)
    PAL_setColor(33, RGB24_TO_VDPCOLOR(0x00FF00));  // PAL2: green (P1)
    PAL_setColor(49, RGB24_TO_VDPCOLOR(0x0088FF));  // PAL3: blue (P2)
    
    createTiles();
    
    gameState = STATE_TITLE;
    drawTitle();
    
    u16 lastJoy = 0;
    u8 roundTimer = 0;
    
    while(TRUE) {
        u16 joy = JOY_readJoypad(JOY_1);
        u16 pressed = joy & ~lastJoy;
        lastJoy = joy;
        
        seed += frameCount;
        
        switch (gameState) {
            case STATE_TITLE:
                // Toggle sound with Start on title
                if (pressed & BUTTON_START) {
                    startGame(0);
                }
                if (pressed & BUTTON_A) {
                    startGame(1);
                }
                if (pressed & BUTTON_C) {
                    soundEnabled = !soundEnabled;
                    playMenuBlip();
                }
                drawTitle();
                break;
                
            case STATE_PLAYING:
                if (frameCount % 4 == 0) {  // Slow down movement
                    updateTank(&tanks[0], 0);
                    updateTank(&tanks[1], 1);
                }
                if (frameCount % 2 == 0) {  // Bullets move faster
                    updateBullets();
                }
                
                updateExplosions();
                updateShake();
                updateSound();
                
                if (!tanks[0].alive || !tanks[1].alive) {
                    gameState = STATE_ROUNDOVER;
                    roundTimer = 0;
                    drawRoundOver();
                }
                
                if (pressed & BUTTON_START) {
                    VDP_drawText("** PAUSED **", 14, 14);
                    while(!(JOY_readJoypad(JOY_1) & BUTTON_START)) {
                        SYS_doVBlankProcess();
                    }
                }
                
                drawGame();
                break;
                
            case STATE_ROUNDOVER:
                roundTimer++;
                updateExplosions();
                updateShake();
                updateSound();
                drawGame();
                drawRoundOver();
                
                if (tanks[0].score >= winScore || tanks[1].score >= winScore) {
                    if (roundTimer > 90) {
                        gameState = STATE_GAMEOVER;
                        
                        // Update high score
                        u8 maxScore = tanks[0].score > tanks[1].score ? tanks[0].score : tanks[1].score;
                        if (maxScore > highScore) highScore = maxScore;
                        
                        playVictoryJingle();
                        drawGameOver();
                    }
                } else if (roundTimer > 60) {
                    playMenuBlip();
                    startRound();
                }
                break;
                
            case STATE_GAMEOVER:
                if (pressed & BUTTON_START) {
                    gameState = STATE_TITLE;
                    playMenuBlip();
                    titleFrame = 0;
                    drawTitle();
                }
                drawGameOver();
                break;
        }
        
        frameCount++;
        SYS_doVBlankProcess();
    }
    
    return 0;
}

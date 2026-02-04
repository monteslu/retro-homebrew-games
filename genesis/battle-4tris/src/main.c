/**
 * Battle 4Tris - Genesis Homebrew
 * With MAXIMUM PIZZAZZ!
 */

#include <genesis.h>

#define FIELD_W     10
#define FIELD_H     20
#define TILE_EMPTY  0
#define TILE_BLOCK  1
#define TILE_GHOST  2
#define TILE_SPARK  3

static u8 field1[FIELD_H][FIELD_W];
static u8 field2[FIELD_H][FIELD_W];
static s8 pieceX, pieceY, pieceType, pieceRot;
static u8 nextPiece;
static u16 score1, score2, lines1, lines2;
static u8 dropTimer, dropSpeed;
static u8 gameState, gameMode;
static u8 currentPlayer;
static u16 frameCount;
static u16 seed = 12345;

// PIZZAZZ!
static u8 soundEnabled = 1;
static s16 shakeX = 0, shakeY = 0;
static u8 shakeTimer = 0;
static u16 highScore = 0;
static u8 titleFrame = 0;
static u8 lineClearFlash = 0;
static u8 flashY[4] = {0, 0, 0, 0};
static u8 flashCount = 0;
static u8 level = 1;
static u8 combo = 0;
static u8 lastWasLineClear = 0;
static u8 lockDelay = 0;
static u8 hardDropping = 0;

static const u16 PIECES[7][4] = {
    {0x0F00, 0x2222, 0x00F0, 0x4444}, // I - cyan
    {0x6600, 0x6600, 0x6600, 0x6600}, // O - yellow
    {0x0E40, 0x4C40, 0x4E00, 0x4640}, // T - purple
    {0x06C0, 0x8C40, 0x6C00, 0x4620}, // S - green
    {0x0C60, 0x4C80, 0xC600, 0x2640}, // Z - red
    {0x0E20, 0x44C0, 0x8E00, 0x6440}, // J - blue
    {0x0E80, 0xC440, 0x2E00, 0x4460}  // L - orange
};

// Piece colors (palette index)
static const u8 PIECE_COLORS[7] = {1, 2, 3, 1, 3, 2, 2};

static u16 rnd() { seed = seed * 1103515245 + 12345; return (seed >> 16) & 0x7FFF; }

// ============ SOUND EFFECTS ============
static void playMove() {
    if (!soundEnabled) return;
    PSG_setEnvelope(0, PSG_ENVELOPE_MAX - 6);
    PSG_setFrequency(0, 400);
    PSG_setEnvelope(0, PSG_ENVELOPE_MIN + 2);
}

static void playRotate() {
    if (!soundEnabled) return;
    PSG_setEnvelope(0, PSG_ENVELOPE_MAX);
    PSG_setFrequency(0, 800);
    PSG_setEnvelope(0, PSG_ENVELOPE_MIN + 3);
}

static void playLock() {
    if (!soundEnabled) return;
    PSG_setEnvelope(1, PSG_ENVELOPE_MAX);
    PSG_setFrequency(1, 200);
    PSG_setEnvelope(1, PSG_ENVELOPE_MIN + 6);
}

static void playLineClear(u8 lines) {
    if (!soundEnabled) return;
    u16 freq = 600 + lines * 200;
    PSG_setEnvelope(0, PSG_ENVELOPE_MAX);
    PSG_setFrequency(0, freq);
    PSG_setEnvelope(0, PSG_ENVELOPE_MIN + 8);
    
    if (lines == 4) {
        // 4-LINE!
        PSG_setEnvelope(1, PSG_ENVELOPE_MAX);
        PSG_setFrequency(1, 1200);
        PSG_setEnvelope(1, PSG_ENVELOPE_MIN + 8);
        PSG_setEnvelope(2, PSG_ENVELOPE_MAX);
        PSG_setFrequency(2, 1600);
        PSG_setEnvelope(2, PSG_ENVELOPE_MIN + 8);
    }
}

static void playHardDrop() {
    if (!soundEnabled) return;
    PSG_setEnvelope(0, PSG_ENVELOPE_MAX);
    PSG_setFrequency(0, 300);
    PSG_setEnvelope(0, PSG_ENVELOPE_MIN + 4);
    PSG_setEnvelope(3, PSG_ENVELOPE_MAX - 8);
    PSG_setNoise(PSG_NOISE_TYPE_WHITE, PSG_NOISE_FREQ_TONE3);
}

static void playGameOver() {
    if (!soundEnabled) return;
    for (u8 i = 0; i < 5; i++) {
        PSG_setEnvelope(0, PSG_ENVELOPE_MAX);
        PSG_setFrequency(0, 400 - i * 60);
        for (u16 j = 0; j < 5000; j++) {}
    }
    PSG_setEnvelope(0, PSG_ENVELOPE_MIN);
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
    PSG_setFrequency(0, 1000 + combo * 150);
    PSG_setEnvelope(0, PSG_ENVELOPE_MIN + 4);
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

static void createTiles() {
    u32 td[8];
    memset(td, 0, 32); VDP_loadTileData(td, TILE_EMPTY, 1, CPU);
    memset(td, 0x11, 32); VDP_loadTileData(td, TILE_BLOCK, 1, CPU);
    
    // Ghost piece (hollow)
    td[0] = 0x11111111;
    td[1] = 0x10000001;
    td[2] = 0x10000001;
    td[3] = 0x10000001;
    td[4] = 0x10000001;
    td[5] = 0x10000001;
    td[6] = 0x10000001;
    td[7] = 0x11111111;
    VDP_loadTileData(td, TILE_GHOST, 1, CPU);
    
    // Spark
    memset(td, 0, 32);
    td[2] = 0x00110000;
    td[3] = 0x01111100;
    td[4] = 0x01111100;
    td[5] = 0x00110000;
    VDP_loadTileData(td, TILE_SPARK, 1, CPU);
}

static bool checkCollision(u8 field[FIELD_H][FIELD_W], s8 px, s8 py, u8 type, u8 rot) {
    u16 shape = PIECES[type][rot];
    for (s8 y = 0; y < 4; y++) {
        for (s8 x = 0; x < 4; x++) {
            if (shape & (0x8000 >> (y * 4 + x))) {
                s8 fx = px + x, fy = py + y;
                if (fx < 0 || fx >= FIELD_W || fy >= FIELD_H) return TRUE;
                if (fy >= 0 && field[fy][fx]) return TRUE;
            }
        }
    }
    return FALSE;
}

// Calculate ghost piece position
static s8 getGhostY(u8 field[FIELD_H][FIELD_W]) {
    s8 ghostY = pieceY;
    while (!checkCollision(field, pieceX, ghostY + 1, pieceType, pieceRot)) {
        ghostY++;
    }
    return ghostY;
}

static void lockPiece(u8 field[FIELD_H][FIELD_W]) {
    u16 shape = PIECES[pieceType][pieceRot];
    for (s8 y = 0; y < 4; y++) {
        for (s8 x = 0; x < 4; x++) {
            if (shape & (0x8000 >> (y * 4 + x))) {
                s8 fx = pieceX + x, fy = pieceY + y;
                if (fy >= 0 && fy < FIELD_H && fx >= 0 && fx < FIELD_W)
                    field[fy][fx] = pieceType + 1;
            }
        }
    }
    playLock();
}

static u8 clearLines(u8 field[FIELD_H][FIELD_W], u16* score, u16* lines) {
    u8 cleared = 0;
    flashCount = 0;
    
    for (s8 y = FIELD_H - 1; y >= 0; y--) {
        bool full = TRUE;
        for (u8 x = 0; x < FIELD_W; x++) if (!field[y][x]) { full = FALSE; break; }
        if (full) {
            flashY[flashCount++] = y;
            cleared++;
            for (s8 yy = y; yy > 0; yy--)
                for (u8 x = 0; x < FIELD_W; x++) field[yy][x] = field[yy-1][x];
            for (u8 x = 0; x < FIELD_W; x++) field[0][x] = 0;
            y++;
        }
    }
    
    if (cleared) {
        *lines += cleared;
        
        // Scoring with combos and line bonuses
        u16 baseScore = 0;
        switch(cleared) {
            case 1: baseScore = 100; break;
            case 2: baseScore = 300; break;
            case 3: baseScore = 500; break;
            case 4: baseScore = 800; break;  // 4-LINE!
        }
        
        // Combo bonus
        if (lastWasLineClear) {
            combo++;
            baseScore += combo * 50;
            playCombo();
        } else {
            combo = 0;
        }
        lastWasLineClear = TRUE;
        
        // Level bonus
        baseScore *= level;
        *score += baseScore;
        
        playLineClear(cleared);
        lineClearFlash = 20;
        
        // Screen shake based on lines
        startShake(cleared * 2);
        
        // Level up every 10 lines
        if (*lines / 10 >= level) {
            level++;
            dropSpeed = 30 - level * 2;
            if (dropSpeed < 5) dropSpeed = 5;
        }
    } else {
        lastWasLineClear = FALSE;
    }
    
    return cleared;
}

static void spawnPiece() {
    pieceType = nextPiece;
    nextPiece = rnd() % 7;
    pieceX = 3; pieceY = -2; pieceRot = 0;
    lockDelay = 0;
    hardDropping = 0;
}

static void initGame(u8 mode) {
    gameMode = mode;
    memset(field1, 0, sizeof(field1));
    memset(field2, 0, sizeof(field2));
    score1 = score2 = lines1 = lines2 = 0;
    level = 1;
    dropSpeed = 30; dropTimer = 0;
    combo = 0;
    lastWasLineClear = 0;
    nextPiece = rnd() % 7;
    spawnPiece();
    currentPlayer = 0;
    gameState = 1;
}

static void update() {
    u16* score = currentPlayer == 0 ? &score1 : &score2;
    u16* lines = currentPlayer == 0 ? &lines1 : &lines2;
    u8 (*field)[FIELD_W] = currentPlayer == 0 ? field1 : field2;
    
    u16 joy = JOY_readJoypad(currentPlayer == 0 ? JOY_1 : JOY_2);
    static u16 lastJoy = 0;
    u16 pressed = joy & ~lastJoy;
    lastJoy = joy;
    
    // Movement with DAS (Delayed Auto Shift feel)
    if (pressed & BUTTON_LEFT) {
        if (!checkCollision(field, pieceX-1, pieceY, pieceType, pieceRot)) {
            pieceX--;
            playMove();
            lockDelay = 0;
        }
    }
    if (pressed & BUTTON_RIGHT) {
        if (!checkCollision(field, pieceX+1, pieceY, pieceType, pieceRot)) {
            pieceX++;
            playMove();
            lockDelay = 0;
        }
    }
    
    // Rotation
    if (pressed & BUTTON_A) {
        u8 newRot = (pieceRot + 1) % 4;
        if (!checkCollision(field, pieceX, pieceY, pieceType, newRot)) {
            pieceRot = newRot;
            playRotate();
            lockDelay = 0;
        } else {
            // Wall kick attempt
            if (!checkCollision(field, pieceX - 1, pieceY, pieceType, newRot)) {
                pieceX--;
                pieceRot = newRot;
                playRotate();
            } else if (!checkCollision(field, pieceX + 1, pieceY, pieceType, newRot)) {
                pieceX++;
                pieceRot = newRot;
                playRotate();
            }
        }
    }
    
    // Counter-clockwise rotation
    if (pressed & BUTTON_B) {
        u8 newRot = (pieceRot + 3) % 4;
        if (!checkCollision(field, pieceX, pieceY, pieceType, newRot)) {
            pieceRot = newRot;
            playRotate();
            lockDelay = 0;
        }
    }
    
    // Soft drop
    if (joy & BUTTON_DOWN) dropTimer += 5;
    
    // Hard drop
    if (pressed & BUTTON_C) {
        s8 ghostY = getGhostY(field);
        *score += (ghostY - pieceY) * 2;  // Bonus points for hard drop
        pieceY = ghostY;
        hardDropping = 1;
        playHardDrop();
        startShake(3);
    }
    
    dropTimer++;
    if (dropTimer >= dropSpeed || hardDropping) {
        dropTimer = 0;
        if (!checkCollision(field, pieceX, pieceY+1, pieceType, pieceRot)) {
            pieceY++;
        } else {
            // Lock delay
            lockDelay++;
            if (lockDelay > 15 || hardDropping) {
                lockPiece(field);
                clearLines(field, score, lines);
                spawnPiece();
                if (checkCollision(field, pieceX, pieceY, pieceType, pieceRot)) {
                    gameState = 2;
                    if (*score > highScore) highScore = *score;
                    playGameOver();
                }
            }
        }
    }
    
    // Line clear flash countdown
    if (lineClearFlash > 0) lineClearFlash--;
}

static void drawField(u8 field[FIELD_H][FIELD_W], u8 ox) {
    // Border
    for (u8 y = 0; y < FIELD_H + 1; y++) {
        VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL1, 0, 0, 0, TILE_BLOCK), ox - 1, 3 + y);
        VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL1, 0, 0, 0, TILE_BLOCK), ox + FIELD_W, 3 + y);
    }
    for (u8 x = 0; x < FIELD_W + 2; x++) {
        VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL1, 0, 0, 0, TILE_BLOCK), ox - 1 + x, 3 + FIELD_H);
    }
    
    // Field
    for (u8 y = 0; y < FIELD_H; y++) {
        // Check if this line should flash
        bool flash = FALSE;
        if (lineClearFlash > 0) {
            for (u8 i = 0; i < flashCount; i++) {
                if (flashY[i] == y && frameCount % 4 < 2) {
                    flash = TRUE;
                    break;
                }
            }
        }
        
        for (u8 x = 0; x < FIELD_W; x++) {
            u16 tile = 0;
            if (flash) {
                tile = TILE_ATTR_FULL(PAL0, 0, 0, 0, TILE_BLOCK);
            } else if (field[y][x]) {
                u8 pal = PIECE_COLORS[(field[y][x] - 1) % 7];
                tile = TILE_ATTR_FULL(pal, 0, 0, 0, TILE_BLOCK);
            }
            VDP_setTileMapXY(BG_A, tile, ox + x, 3 + y);
        }
    }
    
    // Ghost piece
    s8 ghostY = getGhostY(field);
    if (ghostY > pieceY) {
        u16 shape = PIECES[pieceType][pieceRot];
        for (s8 y = 0; y < 4; y++) {
            for (s8 x = 0; x < 4; x++) {
                if (shape & (0x8000 >> (y * 4 + x))) {
                    if (ghostY + y >= 0)
                        VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL1, 0, 0, 0, TILE_GHOST), 
                                        ox + pieceX + x, 3 + ghostY + y);
                }
            }
        }
    }
    
    // Current piece
    u16 shape = PIECES[pieceType][pieceRot];
    u8 pal = PIECE_COLORS[pieceType];
    for (s8 y = 0; y < 4; y++) {
        for (s8 x = 0; x < 4; x++) {
            if (shape & (0x8000 >> (y * 4 + x))) {
                if (pieceY + y >= 0)
                    VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(pal, 0, 0, 0, TILE_BLOCK), 
                                    ox + pieceX + x, 3 + pieceY + y);
            }
        }
    }
}

static void drawNextPiece(u8 ox) {
    VDP_drawText("NEXT", ox, 1);
    
    u16 shape = PIECES[nextPiece][0];
    u8 pal = PIECE_COLORS[nextPiece];
    for (s8 y = 0; y < 4; y++) {
        for (s8 x = 0; x < 4; x++) {
            u16 tile = 0;
            if (shape & (0x8000 >> (y * 4 + x))) {
                tile = TILE_ATTR_FULL(pal, 0, 0, 0, TILE_BLOCK);
            }
            VDP_setTileMapXY(BG_A, tile, ox + x, 2 + y);
        }
    }
}

static void draw() {
    VDP_clearPlane(BG_A, TRUE);
    drawField(field1, 2);
    drawNextPiece(14);
    
    if (gameMode == 1) {
        drawField(field2, 24);
    }
    
    // HUD
    char buf[20];
    sprintf(buf, "SCORE:%d", score1);
    VDP_drawText(buf, 14, 8);
    
    sprintf(buf, "LINES:%d", lines1);
    VDP_drawText(buf, 14, 10);
    
    sprintf(buf, "LV:%d", level);
    VDP_drawText(buf, 14, 12);
    
    // Combo display
    if (combo > 0) {
        sprintf(buf, "COMBO x%d!", combo);
        VDP_drawText(buf, 14, 14);
    }
    
    // Sound indicator
    VDP_drawText(soundEnabled ? "SND" : "---", 14, 24);
    
    // Controls hint
    VDP_drawText("A/B:ROT C:DROP", 13, 26);
}

static void drawTitle(void) {
    VDP_clearPlane(BG_A, TRUE);
    titleFrame++;
    
    // Pulsing title color
    if (titleFrame % 20 < 10) {
        PAL_setColor(1, RGB24_TO_VDPCOLOR(0xFFFFFF));
    } else {
        PAL_setColor(1, RGB24_TO_VDPCOLOR(0xFF00FF));
    }
    
    // Standard title block
    VDP_drawText("====================", 10, 3);
    VDP_drawText("   BATTLE 4TRIS    ", 10, 4);
    VDP_drawText("====================", 10, 5);
    VDP_drawText("Free Retro Games", 12, 7);
    VDP_drawText("v1.0.0", 17, 8);
    
    // Animated falling piece
    u8 animPieceY = (titleFrame / 6) % 8;
    u8 animPieceType = (titleFrame / 30) % 7;
    u16 shape = PIECES[animPieceType][0];
    for (s8 y = 0; y < 4; y++) {
        for (s8 x = 0; x < 4; x++) {
            if (shape & (0x8000 >> (y * 4 + x))) {
                VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PIECE_COLORS[animPieceType], 0, 0, 0, TILE_BLOCK), 
                                18 + x, 10 + animPieceY + y);
            }
        }
    }
    
    // Player select
    VDP_drawText("--------------------", 10, 18);
    VDP_drawText("START - 1 Player", 12, 20);
    VDP_drawText("    A - 2 Players", 12, 22);
    VDP_drawText("--------------------", 10, 24);
    
    // Sound toggle and high score
    char buf[24];
    sprintf(buf, "High Score: %d", highScore);
    VDP_drawText(buf, 13, 26);
    
    VDP_drawText("C:Sound", 2, 27);
    VDP_drawText(soundEnabled ? "[ON]" : "[OFF]", 2, 28);
    
    // Footer
    VDP_drawText("(C) 2026 monteslu", 22, 27);
}

static void drawGameOver() {
    draw();
    
    if (frameCount % 10 < 5) {
        VDP_drawText("=== GAME OVER ===", 11, 12);
    }
    
    char buf[24];
    sprintf(buf, "Final Score: %d", score1);
    VDP_drawText(buf, 12, 14);
    
    sprintf(buf, "Lines: %d  Level: %d", lines1, level);
    VDP_drawText(buf, 10, 16);
    
    if (score1 >= highScore && score1 > 0) {
        VDP_drawText("*** NEW HIGH SCORE! ***", 8, 18);
    }
    
    VDP_drawText("Press START", 14, 22);
}

int main() {
    VDP_setScreenWidth320();
    PAL_setColor(0, RGB24_TO_VDPCOLOR(0x000000));
    PAL_setColor(1, RGB24_TO_VDPCOLOR(0xFFFFFF));
    
    // Block colors
    PAL_setColor(17, RGB24_TO_VDPCOLOR(0x00FFFF)); // Cyan (I)
    PAL_setColor(33, RGB24_TO_VDPCOLOR(0xFFFF00)); // Yellow (O, L)
    PAL_setColor(49, RGB24_TO_VDPCOLOR(0xFF00FF)); // Magenta (T, Z)
    
    createTiles();
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

#include "headers.h"
#define BRICK_HEIGHT 100
#define BRICK_Y 280
Snowflake snow[MAX_SNOW];
SDL_Texture* snowTex = NULL;
SDL_Texture* upTex = NULL;
void InitUpSprite(void) {
    upTex = IMG_LoadTexture(renderer, "assets/up.png");
    if (!upTex) {
        SDL_Log("Failed to load up.png: %s", IMG_GetError());
        exit(1);
    }
}

void RenderUpSprite(void) {
    if (!upTex) return;

    int origW, origH;
    SDL_QueryTexture(upTex, NULL, NULL, &origW, &origH);

    float scale = 0.13f; // 25% of original size, adjust as needed

    SDL_Rect dst = {
        (SCREEN_WIDTH - (int)(origW * scale)) / 2,   // center horizontally
        SCREEN_HEIGHT - (int)(origH * scale) - 16,   // 16 px from bottom
        (int)(origW * scale),
        (int)(origH * scale)
    };

    SDL_RenderCopy(renderer, upTex, NULL, &dst);
}

void CleanupUpSprite(void) {
    if (upTex) { SDL_DestroyTexture(upTex); upTex = NULL; }
}

   //snowwwwwwwwwwwwwwwwwwwwww

void InitSnow(void) {
    snowTex = IMG_LoadTexture(renderer, "assets/snow.png");
    for (int i = 0; i < MAX_SNOW; i++) {
        snow[i].x = rand() % SCREEN_WIDTH;
        snow[i].y = rand() % SCREEN_HEIGHT;
        snow[i].speed = 50 + rand() % 100;
    }
}

void UpdateSnow(float delta) {
    for (int i = 0; i < MAX_SNOW; i++) {
        snow[i].y += snow[i].speed * delta;
        if (snow[i].y > SCREEN_HEIGHT) {
            snow[i].y = -16;
            snow[i].x = rand() % SCREEN_WIDTH;
        }
    }
}

void RenderSnow(void) {
    if (!snowTex) return;
    SDL_Rect dst = {0,0,16,16};
    for (int i = 0; i < MAX_SNOW; i++) {
        dst.x = (int)snow[i].x;
        dst.y = (int)snow[i].y;
        SDL_RenderCopy(renderer, snowTex, NULL, &dst);
    }
}
void snow_quit(void){    if (snowTex) { SDL_DestroyTexture(snowTex); snowTex = NULL; }


    }

/* ---------- Font ---------- */
void InitFont(void) {
    font = TTF_OpenFont("assets/font.ttf", 28);
    if (!font) {
        SDL_Log("TTF_OpenFont error: %s", TTF_GetError());
        exit(1);
    }
}

void CleanupFont(void) {
    if (font) { TTF_CloseFont(font); font = NULL; }
}

/* ---------- Background ---------- */
void InitBackground(void) {
    bgTexture = IMG_LoadTexture(renderer, "assets/background.png");
    if (!bgTexture) {
        SDL_Log("Failed to load background.png: %s", IMG_GetError());
        exit(1);
    }

    barFullTex = IMG_LoadTexture(renderer, "assets/bar_full.png");
    barEmptyTex = IMG_LoadTexture(renderer, "assets/bar_empty.png");

    volIcons[0] = IMG_LoadTexture(renderer, "assets/volume1.png");
    volIcons[1] = IMG_LoadTexture(renderer, "assets/volume2.png");
    volIcons[2] = IMG_LoadTexture(renderer, "assets/volume3.png");
    volIcons[3] = IMG_LoadTexture(renderer, "assets/volume4.png");

    sunIcons[0] = IMG_LoadTexture(renderer, "assets/sun1.png");
    sunIcons[1] = IMG_LoadTexture(renderer, "assets/sun2.png");
    sunIcons[2] = IMG_LoadTexture(renderer, "assets/sun3.png");

    if (!barFullTex || !barEmptyTex) {
        SDL_Log("Failed to load bar textures: %s", IMG_GetError());
        exit(1);
    }
}

void CleanupBackground(void) {
    if (bgTexture) { SDL_DestroyTexture(bgTexture); bgTexture = NULL; }
    if (barFullTex) { SDL_DestroyTexture(barFullTex); barFullTex = NULL; }
    if (barEmptyTex) { SDL_DestroyTexture(barEmptyTex); barEmptyTex = NULL; }
    for (int i=0;i<4;i++) { if (volIcons[i]) { SDL_DestroyTexture(volIcons[i]); volIcons[i]=NULL; } }
    for (int i=0;i<3;i++) { if (sunIcons[i]) { SDL_DestroyTexture(sunIcons[i]); sunIcons[i]=NULL; } }
}

/* ---------- Settings ---------- */
void LoadSettings(void) {
    FILE* f = fopen("settings.dat","rb");
    if (!f) {
        settings.master = 10;
        settings.music = 10;
        settings.vfx = 10;
        settings.brightness = 10;
        settings.fullscreen = 0;
        return;
    }
    fread(&settings, sizeof(Settings), 1, f);
    fclose(f);
}

void SaveSettings(void) {
    FILE* f = fopen("settings.dat","wb");
    if (!f) return;
    fwrite(&settings, sizeof(Settings), 1, f);
    fclose(f);
}

void ApplySettings(void) {
    int finalMusic = (settings.music * settings.master) / 10;
    int finalVfx   = (settings.vfx   * settings.master) / 10;
    Mix_VolumeMusic(finalMusic * MIX_MAX_VOLUME / 10);
    Mix_Volume(-1, finalVfx * MIX_MAX_VOLUME / 10);

    if (settings.fullscreen) SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN);
    else SDL_SetWindowFullscreen(window, 0);
}

/* ---------- Audio ---------- */
static Mix_Chunk* click1 = NULL;
static Mix_Chunk* click2 = NULL;
static Mix_Chunk* shoot1 = NULL;
static Mix_Chunk* shoot2 = NULL;
static Mix_Music* music = NULL;

void InitAudio(void) {
    click1 = Mix_LoadWAV("assets/click.wav");
    click2 = Mix_LoadWAV("assets/click.wav");
    shoot1 = Mix_LoadWAV("assets/shoot.wav");
    shoot2 = Mix_LoadWAV("assets/shoot.wav");
    music = Mix_LoadMUS("assets/menu_music.wav");
    if (music) Mix_PlayMusic(music, -1);
}

void CleanupAudio(void) {
    if (click1) { Mix_FreeChunk(click1); click1=NULL; }
    if (click2) { Mix_FreeChunk(click2); click2=NULL; }
    if (shoot1) { Mix_FreeChunk(shoot1); shoot1=NULL; }
    if (shoot2) { Mix_FreeChunk(shoot2); shoot2=NULL; }
    if (music) { Mix_FreeMusic(music); music=NULL; }
}

void PlayClick(void) {
    if (click1) Mix_PlayChannel(-1, click1, 0);
    if (click2) Mix_PlayChannel(-1, click2, 0);
}

void PlayShootLeft(void)  { if (shoot1) Mix_PlayChannel(-1, shoot1, 0); }
void PlayShootRight(void) { if (shoot2) Mix_PlayChannel(-1, shoot2, 0); }

/* ---------- Shooters ---------- */
void InitShooters(void) {
    leftShooter.texture  = IMG_LoadTexture(renderer, "assets/thief_left.png");
    rightShooter.texture = IMG_LoadTexture(renderer, "assets/thief_right.png");
    bulletTex = IMG_LoadTexture(renderer, "assets/bullet.png");
    brickTex  = IMG_LoadTexture(renderer, "assets/brick.png");

    if (!leftShooter.texture || !rightShooter.texture || !bulletTex || !brickTex) {
        SDL_Log("Missing shooter assets: %s", IMG_GetError());
        exit(1);
    }

    int texW, texH;
    SDL_QueryTexture(leftShooter.texture, NULL, NULL, &texW, &texH);
    leftShooter.frameW = texW / 6;
    leftShooter.frameH = texH / 6;

    SDL_QueryTexture(rightShooter.texture, NULL, NULL, &texW, &texH);
    rightShooter.frameW = texW / 6;
    rightShooter.frameH = texH / 6;

    leftShooter.x = 8;
    leftShooter.y = (float)(BRICK_HEIGHT- leftShooter.frameH);
    rightShooter.x = (float)(SCREEN_WIDTH- rightShooter.frameW - 8);
    rightShooter.y = (float)(BRICK_HEIGHT - rightShooter.frameH);

    leftShooter.direction = 1;
    rightShooter.direction = -1;

    leftShooter.shootInterval  = 0.9f;
    rightShooter.shootInterval = 1.1f;

    leftShooter.frame = rightShooter.frame = 0;
    leftShooter.animTimer = rightShooter.animTimer = 0.0f;
    leftShooter.shootTimer = rightShooter.shootTimer = 0.0f;
    leftShooter.firedThisCycle = rightShooter.firedThisCycle = 0;

    for (int i=0;i<MAX_BULLETS;i++) bullets[i].active = 0;
}

void CleanupShooters(void) {
    if (leftShooter.texture) { SDL_DestroyTexture(leftShooter.texture); leftShooter.texture=NULL; }
    if (rightShooter.texture) { SDL_DestroyTexture(rightShooter.texture); rightShooter.texture=NULL; }
    if (bulletTex) { SDL_DestroyTexture(bulletTex); bulletTex=NULL; }
    if (brickTex) { SDL_DestroyTexture(brickTex); brickTex=NULL; }
}

void SpawnBullet(Shooter* s) {
    for (int i=0;i<MAX_BULLETS;i++) {
        if (!bullets[i].active) {
            bullets[i].active = 1;
            bullets[i].x = s->x + s->frameW * 0.5f;
            bullets[i].y = s->y + s->frameH * 0.5f;
            bullets[i].speed = 600.0f * (s->direction > 0 ? 1.0f : -1.0f);
            if (s == &leftShooter) PlayShootLeft();
            else PlayShootRight();
            break;
        }
    }
}

void UpdateShooter(Shooter* s, float delta) {
    s->animTimer += delta;
    s->shootTimer += delta;

    if (s->animTimer >= 0.05f) {
        s->frame++;
        if (s->frame >= 36) {
            s->frame = 0;
            s->firedThisCycle = 0;
        }
        s->animTimer = 0.0f;
    }

    if (s->frame == 17 && !s->firedThisCycle && s->shootTimer >= s->shootInterval) {
        SpawnBullet(s);
        s->firedThisCycle = 1;
        s->shootTimer = 0.0f;
    }
}

void UpdateShooters(float delta) {
    UpdateShooter(&leftShooter, delta);
    UpdateShooter(&rightShooter, delta);

    for (int i=0;i<MAX_BULLETS;i++){
        if (bullets[i].active) {
            bullets[i].x += bullets[i].speed * delta;
            if (bullets[i].x < -50.0f || bullets[i].x > SCREEN_WIDTH + 50.0f)
                bullets[i].active = 0;
        }
    }
}

void RenderShooter(Shooter* s) {
    int row = s->frame / 6;
    int col = s->frame % 6;
    SDL_Rect src = { col * s->frameW, row * s->frameH, s->frameW, s->frameH };
    SDL_Rect dst = { (int)s->x, (int)s->y, s->frameW, s->frameH };
    SDL_RenderCopy(renderer, s->texture, &src, &dst);
}

void RenderShooters(void) {
    SDL_Rect leftBrick  = { 0, 0, leftShooter.frameW + 24, BRICK_Y  };
    SDL_Rect rightBrick = { SCREEN_WIDTH - (rightShooter.frameW + 24), 0, rightShooter.frameW + 24,BRICK_Y  };

    SDL_RenderCopy(renderer, brickTex, NULL, &leftBrick);
    SDL_RenderCopy(renderer, brickTex, NULL, &rightBrick);

    RenderShooter(&leftShooter);
    RenderShooter(&rightShooter);

    for (int i=0;i<MAX_BULLETS;i++){
        if (bullets[i].active) {
            SDL_Rect dst = { (int)bullets[i].x - 10, (int)bullets[i].y - 5, 20, 10 };
            SDL_RenderCopy(renderer, bulletTex, NULL, &dst);
        }
    }
}

/* ---------- Credits ---------- */
static char* creditsText = NULL;
static int creditsLength = 0;
static int visibleChars = 0;
static float typeTimer = 0.0f;
static float scrollY = SCREEN_HEIGHT;
static int finishedTyping = 0;

void LoadCreditsFromFile(void) {
    FILE* f = fopen("assets/credits.txt","rb");
    if (!f) { creditsText = NULL; creditsLength = 0; return; }
    fseek(f, 0, SEEK_END);
    creditsLength = ftell(f);
    rewind(f);
    creditsText = malloc(creditsLength + 1);
    fread(creditsText, 1, creditsLength, f);
    creditsText[creditsLength] = '\0';
    fclose(f);
}

void InitCredits(void) {
    visibleChars = 0;
    typeTimer = 0.0f;
    scrollY = SCREEN_HEIGHT;
    finishedTyping = 0;
}

void UpdateCredits(float delta) {
    if (!creditsText) return;
    if (!finishedTyping) {
        typeTimer += delta;
        if (typeTimer >= 0.03f) {
            if (visibleChars < creditsLength) visibleChars++;
            typeTimer = 0.0f;
        }
        if (visibleChars >= creditsLength) finishedTyping = 1;
    } else {
        scrollY -= 30.0f * delta;
    }
}

void RenderCredits(void) {
    if (!creditsText) return;
    int len = visibleChars;
    if (len < 1) return;
    char* tmp = malloc(len+1);
    memcpy(tmp, creditsText, len);
    tmp[len] = '\0';

    SDL_Color white = {255,255,255,255};
    SDL_Surface* surf = TTF_RenderText_Blended_Wrapped(font, tmp, white, SCREEN_WIDTH - 200);
    if (surf) {
        SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
        SDL_Rect dst = { (SCREEN_WIDTH - surf->w)/2, (int)scrollY, surf->w, surf->h };
        SDL_RenderCopy(renderer, tex, NULL, &dst);
        SDL_DestroyTexture(tex);
        SDL_FreeSurface(surf);
    }
    free(tmp);
}

/* ---------- Options menu ---------- */
void RenderOptionsMenu(int selected) {
    SDL_Color white = {255,255,255,255};
    SDL_Color warm  = {255,200,100,255};
    const char* items[MENU_ITEMS] = {
        "Master Volume",
        "Music Volume",
        "VFX Volume",
        "Brightness",
        "Full Screen",
        "Credits"
    };

    for (int i=0;i<MENU_ITEMS;i++) {
        SDL_Color c = (i==selected) ? warm : white;
        SDL_Surface* surf = TTF_RenderText_Blended(font, items[i], c);
        if (!surf) continue;
        SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
        int x = (SCREEN_WIDTH - surf->w) / 2 -60;
        int y = 150 + i * 70;
        SDL_Rect dst = { x, y, surf->w, surf->h };
        SDL_RenderCopy(renderer, tex, NULL, &dst);

        if (i < 4) {
            int value = (i==0) ? settings.master :
                        (i==1) ? settings.music :
                        (i==2) ? settings.vfx :
                                 settings.brightness;

            int bx = x + surf->w + 24;
            int by = y + (surf->h - 20)/2 -13;
            for (int t=0;t<10;t++) {
                SDL_Rect bdst = { bx + t*22, by, 35, 50 };
                if (t < value) SDL_RenderCopy(renderer, barFullTex, NULL, &bdst);
                else SDL_RenderCopy(renderer, barEmptyTex, NULL, &bdst);
            }

            if (i == 3) {
                int idx = (value >= 6) ? 2 : (value >= 3) ? 1 : 0;
                if (sunIcons[idx]) {
                    SDL_Rect idst = { bx + 10 + 10*22 + 16, by - 6, 60, 60 };
                    SDL_RenderCopy(renderer, sunIcons[idx], NULL, &idst);
                }
            } else {
                int vidx = (value >= 8) ? 3 : (value >= 6) ? 2 : (value >= 3) ? 1 : 0;
                if (volIcons[vidx]) {
                    SDL_Rect idst = { bx + 10 + 10*22 + 16, by - 6, 60, 60 };
                    SDL_RenderCopy(renderer, volIcons[vidx], NULL, &idst);
                }
            }
        }

        SDL_DestroyTexture(tex);
        SDL_FreeSurface(surf);
    }
}

/* ---------- Brightness overlay ---------- */
void RenderBrightnessOverlay(void) {
    int alpha = (10 - clamp_int(settings.brightness,0,10)) * 18;
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0,0,0, alpha);
    SDL_Rect r = {0,0,SCREEN_WIDTH,SCREEN_HEIGHT};
    SDL_RenderFillRect(renderer, &r);
}

#include "headers.h"

/* ===== DEFINE GLOBALS ONCE ===== */
SDL_Window* window = NULL;
SDL_Renderer* renderer = NULL;
TTF_Font* font = NULL;
Mix_Music* music = NULL;

SDL_Texture* background = NULL;
Sprite character;
Button buttons[4];

EnigmaState state = STATE_FADE_IN;
int fadeAlpha = 255;
int correctAnswer = 1;  // <-- set manually: 0=A,1=B,2=C,3=D
int resultFinished = 0;
char question[256] = "What is your character's favorite color?";

/* ===== UTILITY TO RENDER QUESTION TEXT ===== */
void RenderQuestion(const char* questionText)
{
    SDL_Color white = {255,255,255,255};
    SDL_Surface* surf = TTF_RenderText_Blended_Wrapped(font, questionText, white, SCREEN_WIDTH - 100);
    if (!surf) return;
    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
    SDL_Rect dst = { (SCREEN_WIDTH - surf->w)/2, 20, surf->w, surf->h }; // near top
    SDL_RenderCopy(renderer, tex, NULL, &dst);
    SDL_DestroyTexture(tex);
    SDL_FreeSurface(surf);
}

int main(int argc,char* argv[])
{
    if (argc < 2)
    {
        printf("Usage: ./enigmes <character_id 1-4>\n");
        return 1;
    }

    int character_id = atoi(argv[1]);
    if (character_id < 1 || character_id > 4)
        return 1;

    if (!InitSDL()) return 1;

    char bg[128], idle[128], happy[128], sad[128];
    sprintf(bg,"assets/background/bg%d.png",character_id);
    sprintf(idle,"assets/characters/idle%d.png",character_id);
    sprintf(happy,"assets/characters/happy%d.png",character_id);
    sprintf(sad,"assets/characters/sad%d.png",character_id);

    background = IMG_LoadTexture(renderer, bg);
    if (!background)
    {
        printf("Background load error: %s\n", IMG_GetError());
        return 1;
    }
    InitSprite(&character,idle,530,570,150,150,80,1);

    // ===== BUTTONS NEAR TOP =====
    InitButton(&buttons[0],"assets/decoration/button.png","Answer A",60,100,300,200);
    InitButton(&buttons[1],"assets/decoration/button.png","Answer B",60,250,300,200);
    InitButton(&buttons[2],"assets/decoration/button.png","Answer C",930,100,300,200);
    InitButton(&buttons[3],"assets/decoration/button.png","Answer D",930,250,300,200);

    // ===== TINT BUTTONS TO BLUE =====
    for (int i=0;i<4;i++)
        SDL_SetTextureColorMod(buttons[i].texture, 50,150,255);

    int running = 1;
    SDL_Event e;

    while (running)
    {
        while (SDL_PollEvent(&e))
        {
            if (e.type == SDL_QUIT)
                running = 0;

            if (e.type == SDL_KEYDOWN)
            {
                if (e.key.keysym.sym == SDLK_RETURN)
                {
                    if (state == STATE_DIALOGUE)
                        state = STATE_CHOICES;
                    else if (state == STATE_RESULT_ANIM && resultFinished)
                        state = STATE_FADE_OUT;
                }
            }

            if (e.type == SDL_MOUSEBUTTONDOWN && state == STATE_CHOICES)
            {
                int mx = e.button.x;
                int my = e.button.y;

                for (int i=0;i<4;i++)
                {
                    if (ButtonClicked(&buttons[i],mx,my))
                    {
                        for (int j=0;j<4;j++)
                            buttons[j].visible = 0;

                        DestroySprite(&character);

                        if (i == correctAnswer)
                            InitSprite(&character,happy,530,570,150,150,80,0);
                        else
                            InitSprite(&character,sad,530,570,150,150,80,0);

                        state = STATE_RESULT_ANIM;
                    }
                }
            }
        }

        UpdateSprite(&character);

        SDL_SetRenderDrawColor(renderer,0,0,0,255);
        SDL_RenderClear(renderer);

        SDL_RenderCopy(renderer,background,NULL,NULL);
        RenderSprite(&character);

        if (state == STATE_FADE_IN)
        {
            fadeAlpha -= 5;
            if (fadeAlpha <= 0)
            {
                fadeAlpha = 0;
                state = STATE_DIALOGUE;
            }
            RenderFade(fadeAlpha);
        }
        else if (state == STATE_DIALOGUE)
        {
            RenderText("Press ENTER", (SCREEN_WIDTH - 200)/2, SCREEN_HEIGHT/2);
        }
        else if (state == STATE_CHOICES)
        {
            RenderQuestion(question); // <-- added question text
            for (int i=0;i<4;i++)
                RenderButton(&buttons[i]);
        }
        else if (state == STATE_RESULT_ANIM)
        {
            if (character.currentFrame == TOTAL_FRAMES-1)
                resultFinished = 1;

            if (resultFinished)
                RenderText("Press ENTER", (SCREEN_WIDTH - 200)/2, SCREEN_HEIGHT/2);
        }
        else if (state == STATE_FADE_OUT)
        {
            fadeAlpha += 5;
            if (fadeAlpha >= 255)
            {
                fadeAlpha = 255;
                state = STATE_EXIT;
            }
            RenderFade(fadeAlpha);
        }

        SDL_RenderPresent(renderer);

        if (state == STATE_EXIT)
            running = 0;

        SDL_Delay(16);
    }

    DestroySprite(&character);
    SDL_DestroyTexture(background);
    CleanupSDL();

    return 0;
}

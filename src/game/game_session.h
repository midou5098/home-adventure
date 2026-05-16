#ifndef GAME_SESSION_H
#define GAME_SESSION_H

typedef struct {
    int duo_mode;          /* 0 = solo, 1 = duo */
    int player_count;      /* 1 or 2 */
    int selected_skin[2];  /* 1-based skin ids from choose scene */
    int control_scheme[2]; /* 1=WASD, 2=ARROWS, 3=CONTROLLER fallback */
    int resume_from_save;  /* 0 = new game, 1 = continue last save */
    int save_enabled;      /* 0 = do not keep checkpoint save, 1 = keep last checkpoint save */
} GameSelection;

#endif /* GAME_SESSION_H */

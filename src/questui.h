#ifndef QUESTUI_H
#define QUESTUI_H

#include "game.h"

/* The questbook: a button in the corner, and the book it opens - a tab for each lot of quests (the game's own, and
 * each mod's that has any), the quests of the tab that is open, and the one chosen: what it asks, how far that has
 * got, what it gives, and claiming it. */
typedef struct {
    bool open;
    bool pressed;
    int tab;                       /* which of content.quest_tabs is open */
    int chosen[MAX_QUEST_TABS];    /* the quest chosen in each tab, plus one; 0 for none yet */
    float scroll[MAX_QUEST_TABS];  /* how far down each tab's list is, in pixels */
} QuestUi;

/* True if the press was on it. */
bool questui_handle_input(QuestUi *ui, Game *g);
bool questui_mouse_over(const QuestUi *ui);
void questui_draw(const QuestUi *ui, const Game *g);

#endif

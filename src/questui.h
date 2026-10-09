#ifndef QUESTUI_H
#define QUESTUI_H

#include "game.h"

/* The questbook: a button in the corner, and the book it opens - a tab for each lot of quests (the game's own, and
 * each mod's that has any); the open tab's quests set out as a tree, each joined to what it comes after, on a page
 * that is dragged about; and the one chosen: what it asks, how far that has got, what it gives, and claiming it. */
typedef struct {
    bool open;
    bool pressed;                  /* a press that began on the book */
    bool dragging, moved;          /* that press is moving the page; and has moved it far enough not to be a click */
    float last_x, last_y, went;    /* where the mouse was, and how far it has gone since the press */
    int tab;                       /* which of content.quest_tabs is open */
    int chosen[MAX_QUEST_TABS];    /* the quest chosen in each tab, plus one; 0 for none yet */
    int choice;                    /* which reward of the chosen quest is picked, where one is to be, plus one; 0 for none */
    float pan_x[MAX_QUEST_TABS], pan_y[MAX_QUEST_TABS]; /* how far each tab's page has been moved, in pixels */
} QuestUi;

/* True if the press was on it. */
bool questui_handle_input(QuestUi *ui, Game *g);
bool questui_mouse_over(const QuestUi *ui);
void questui_draw(const QuestUi *ui, const Game *g);

#endif

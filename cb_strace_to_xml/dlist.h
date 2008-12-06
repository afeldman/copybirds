//Header file for double concatenated list manager by Malte Marwedel
/* dlist was done by me in the semester 3 as part of the
 subject "Betriebssysteme".
*/


#ifndef DLISTS_H
  #define DLISTS_H


struct list_head {
  struct list_head *next, *prev;
};

typedef struct list_head list_head_t;

/* initialize "shortcut links" for empty list */
extern void list_init(struct list_head *head);

/* insert new entry after specified head
   new ist a keyword - I do not use keywords for naming variables
*/
extern void list_add(struct list_head *newe, struct list_head *head);

/* insert a new entry before the specified head
   new ist a keyword - I do not use keywords for naming variables */
extern void list_add_tail(struct list_head *newe, struct list_head *head);

/* deletes entry from list, reinitializes it (next = prev = 0),
and returns pointer to entry */
extern struct list_head* list_del(struct list_head *entry);

/* delete entry from one list and add as another's head */
extern void list_move(struct list_head *entry, struct list_head *head);

/* delete entry from one list and add as another's tail */
extern void list_move_tail(struct list_head *entry, struct list_head *head);

/* tests whether a list is empty */
extern int list_empty(struct list_head *head);

#endif

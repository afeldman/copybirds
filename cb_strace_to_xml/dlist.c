//double concatenated list manager by Malte Marwedel

#include "dlist.h"

void list_init(struct list_head *head) {
head->next = head;
head->prev = head;
}

void list_add(struct list_head *newe, struct list_head *head) {
//Insert AFTER head
newe->prev = head;
newe->next = head->next;
head->next = newe;
newe->next->prev = newe;
}

void list_add_tail(struct list_head *newe, struct list_head *head) {
//Insert BEFORE head
newe->prev = head->prev;
newe->next = head;
head->prev->next = newe;
head->prev = newe;
}

struct list_head* list_del(struct list_head *entry) {
entry->prev->next = entry->next;
entry->next->prev = entry->prev;
entry->next = 0;
entry->prev = 0;
return entry;
}

void list_move(struct list_head *entry, struct list_head *head) {
list_del(entry);
list_add(entry, head);
}

void list_move_tail(struct list_head *entry, struct list_head *head) {
list_del(entry);
list_add_tail(entry, head);
}

int list_empty(struct list_head *head) {
//Returns: 1 if empty; 0 if not empty
if ((head->next == head) && (head->prev == head)) {
  return 1;    //List is empty
}
return 0;
}

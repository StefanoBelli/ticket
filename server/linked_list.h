#ifndef LINKED_LIST_H
#define LINKED_LIST_H

typedef struct __linked_list {
	void *data;
	struct __linked_list *next;
} linked_list;

linked_list* new_ll(void *data);
linked_list* append_ll(void *data, linked_list *head);
int del_ll(linked_list **head, linked_list *elem);

#endif

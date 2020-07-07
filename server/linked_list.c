/*
 * linked_list.c -- linked list utility
 *   ITA: modulo per facilitare la gestione di linked lists
 */
#include <stdlib.h>
#include "linked_list.h"

// linked list creation and delete
linked_list* new_ll(void* data) {
	linked_list* newlist = (linked_list*) malloc(sizeof(linked_list));
	if(newlist == NULL)
		return NULL;

	newlist->data = data;
	newlist->next = NULL;

	return newlist;
}

// linked list append / delete
linked_list* append_ll(void *data, linked_list *head) {
	linked_list* current_last_elem = head;
	while((head = head->next))
		current_last_elem = head;

	return (current_last_elem->next = new_ll(data));
}

int del_ll(linked_list **head, linked_list *elem) {
	if(*head == elem) {
		linked_list* newhead = (*head)->next;
		free(*head);
		*head = newhead;
		return 1; //elem found, it is head
	}

	linked_list* initial = *head;
	linked_list* prev;

	while(*head != elem) {
		prev = *head;
		if((*head = (*head)->next) == NULL)
			return 0; //elem not found
	}

	//elem found

	prev->next = (*head)->next;
	free(*head);
 
	*head = initial;
	
	return 1;
}

/*
 * Jonathan Reed - 9051-66446
 * Matthew Kracht - 9053_25165
 * ECE - 5565
 * Project 2
 */

#ifndef _ORDEREDLIST_H
#define _ORDEREDLIST_H

#include <stdlib.h>
#include <sys/types.h>

struct Node {
	int size;
	unsigned int seqNum;
	unsigned int seqEnd;
	char *del;
	char *data;
	int index;
	Node *next;
};

class OrderedList {
	public: 
		OrderedList();
		~OrderedList();
		int insert(unsigned int seqnum, unsigned int seqend, char *buf, int len, char *del=NULL);
		Node* peekHead();
		Node* peekTail();
		Node* removeHead();
		Node* findNext(unsigned int seqnum);
		int containsEnd(unsigned int seqend);
		int getSize();
	private:
		Node *head;
		Node *tail;
		int size;
};

#endif

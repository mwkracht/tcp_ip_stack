#ifndef _ORDEREDLIST_H
#define _ORDEREDLIST_H

#include <stdlib.h>
#include <sys/types.h>

struct Node {
	int size;
	unsigned int seqNum;
	unsigned int seqEnd;
	char *data;
	Node *next;
};

class OrderedList {
	public: 
		OrderedList();
		~OrderedList();
		int insert(unsigned int seqnum, unsigned int seqend, char *buf, int len);
		unsigned int peekHead();
		Node* removeHead();
		int getSize();
	private:
		Node *head;
		Node *end;
		int size;
};

#endif

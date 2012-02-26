#ifndef _ORDEREDLIST_H
#define _ORDEREDLIST_H

#include <stdlib.h>
#include <sys/types.h>

struct Node {
	int size;
	unsigned int SeqNum;
	char *data;
	Node *next;
};

class OrderedList {
	public: 
		OrderedList();
		~OrderedList();
		int insert(int seqnum, char *buf, int len);
		unsigned int OrderedList::peekHead(){
		Node* removeHead();
	private:
		Node *head;
		Node *end;
};

#endif

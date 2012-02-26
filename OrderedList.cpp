#include "OrderedList.h"

OrderedList::OrderedList(){
	head = NULL;
	end = NULL;
}

OrderedList::~OrderedList(){
	Node *temp = removeHead();
	while(temp!=NULL){
		delete temp;
		temp = removeHead();
	}
}

int OrderedList::insert(unsigned int seqnum, char *buf, int len){
	Node *newNode = new Node;
	newNode->SeqNum = seqnum; 
	newNode->size = len;
	newNode->data = buf;

	if(head==NULL){
		head = newNode;
		end = newNode;
		newNode->next = NULL;
		return 0;
	}

	if (seqnum < head->SeqNum) {
		end = head;
		newNode->next = head;
		head = newNode;
		return 0;
	}

	if (end->SeqNum < seqnum) {
		end->next = newNode;
		newNode->next = NULL;
		end = newNode;
		return 0;
	}

	Node *comp = head;
	while(comp->next!=NULL){
		if (seqnum < comp->next->SeqNum) {
			newNode->next = comp->next;
			comp->next = newNode;
			return 0;
		}
		if (seqnum == comp->next->SeqNum) {
			delete newNode;
			return -1;
		}
		comp = comp->next;
	}

	newNode->next = NULL;
	comp->next = newNode;
	end = newNode;
	return 0;
}

unsigned int OrderedList::peekHead(){
	return head->SeqNum;
}

Node* OrderedList::removeHead(){
	Node *temp = head;

	if (head == end) {
		end = NULL;
	}
	if (temp != NULL) {
		head = temp->next;		
	}

	return temp;
}

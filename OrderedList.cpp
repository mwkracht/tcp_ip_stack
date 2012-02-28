#include "OrderedList.h"

OrderedList::OrderedList(){
	head = NULL;
	end = NULL;
	size = 0;
}

OrderedList::~OrderedList(){
	Node *temp = removeHead();
	while(temp!=NULL){
		delete temp->data;
		delete temp;
		temp = removeHead();
	}
}

int OrderedList::insert(unsigned int seqnum, unsigned int seqend, char *buf, int len){
	Node *newNode = new Node;
	newNode->seqNum = seqnum;
	newNode->seqEnd = seqend;
	newNode->size = len;
	newNode->data = buf;

	if(head==NULL){
		head = newNode;
		end = newNode;
		newNode->next = NULL;
		size++;
		return 0;
	}

	if (seqnum < head->seqNum) {
		if (head->seqNum < seqend) {
			delete newNode;
			return -1;
		}
		end = head;
		newNode->next = head;
		head = newNode;
		size++;
		return 0;
	}

	if (end->seqNum < seqnum) {
		if (seqend < end->seqEnd) {
			delete newNode;
			return -1;
		}
		end->next = newNode;
		newNode->next = NULL;
		end = newNode;
		size++;
		return 0;
	}

	Node *comp = head;
	while(comp->next!=NULL){
		if (seqnum < comp->seqEnd) {
			delete newNode;
			return -1;
		}
		if (seqnum < comp->next->seqNum) {
			if (comp->next->seqNum < seqend) {
				delete newNode;
				return -1;
			}
			newNode->next = comp->next;
			comp->next = newNode;
			size++;
			return 0;
		}
		if (seqnum == comp->next->seqNum) {
			delete newNode;
			return -1;
		}
		
		comp = comp->next;
	}

	newNode->next = NULL;
	comp->next = newNode;
	end = newNode;
	size++;
	return 0;
}

unsigned int OrderedList::peekHead(){
	if (head == NULL) {
		return 0;	
	} else {	
		return head->seqNum;
	}
}

Node* OrderedList::removeHead(){
	Node *temp = head;

	if (head == end) {
		end = NULL;
	}
	if (temp != NULL) {
		size--;
		head = temp->next;		
	}

	return temp;
}

int OrderedList::getSize() {
	return size;
}

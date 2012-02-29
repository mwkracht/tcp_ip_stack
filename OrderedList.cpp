#include "OrderedList.h"

OrderedList::OrderedList(){
	head = NULL;
	tail = NULL;
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
		tail = newNode;
		newNode->next = NULL;
		size++;
		return 0;
	}

	if (seqnum < head->seqNum) {
		if (head->seqNum < seqend) {
			delete newNode;
			return -1;
		}
		tail = head;
		newNode->next = head;
		head = newNode;
		size++;
		return 0;
	}

	if (tail->seqNum < seqnum) {
		if (seqend < tail->seqEnd) {
			delete newNode;
			return -1;
		}
		tail->next = newNode;
		newNode->next = NULL;
		tail = newNode;
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
	tail = newNode;
	size++;
	return 0;
}

Node* OrderedList::peekHead(){
	return head;
}

Node* OrderedList::removeHead(){
	Node *temp = head;

	if (head == tail) {
		tail = NULL;
	}
	if (temp != NULL) {
		size--;
		head = temp->next;		
	}

	return temp;
}

Node* OrderedList::findNext(unsigned int seqnum) {
	Node *temp = head;
	
	while (temp != NULL && temp->seqNum <= seqnum) {
		temp = temp->next;
	}

	return temp;
}

int OrderedList::getSize() {
	return size;
}

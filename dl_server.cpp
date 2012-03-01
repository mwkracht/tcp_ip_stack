//Matthew Kracht - mwkracht@vt.edu
//Prashant Kapoor - prash310@vt.edu
//Project 1
//2-15-2012

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <iostream>
#include "FTP.h"
#include "UTrans.h"

using namespace std;

int main(int argc, char *argv[]) {
	/*
	 char *ADDR,*FILE,*PORT_NUM;
	 FTP* file_trans;

	 if(argc==2){
	 PORT_NUM = argv[1];
	 } else {
	 cout << "Error in command line arguments. ./dl_server <port>\n";
	 exit(-1);
	 }

	 file_trans = new FTP(NULL,NULL,PORT_NUM);
	 if(file_trans->recvFile() == -1){
	 cout << "ERROR: Unable to receive file on port " << PORT_NUM << endl;
	 }
	 delete file_trans;
	 */

	TCP *tcp = new TCP();
	char *mybuf = new char[10000];
	tcp->listenTCP(argv[1]);
	int count = 0;
	int size;
	while (1) {
		size = tcp->read(mybuf, 10000, /*2*/000);

		//if (size) {
			count += size;

			printf("read size=%d, count=%d, str='", size, count);
			/*
			for (int i = 0; i < size; i++) {
				printf("%u", mybuf[i]);
			}*/
			printf("'\n");
		//}
	}
	delete mybuf;
	delete tcp;
}

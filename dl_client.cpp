#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <iostream>
#include "FTP.h"
#include "UTrans.h"
#include "TCP.h"

using namespace std;

int main(int argc, char *argv[]) {
	/*char *ADDR,*FILE,*PORT_NUM;
	 FTP* file_trans;

	 if(argc==4){
	 PORT_NUM = argv[2];
	 ADDR = argv[1];
	 FILE = argv[3];
	 } else {
	 cout << "Error in command line arguments. ./dl_client <hostname> <port> <filename>\n";
	 exit(-1);
	 }

	 file_trans = new FTP(FILE,ADDR,PORT_NUM);
	 if(file_trans->sendFile() == -1){
	 cout << "ERROR: Unable to send file to " << ADDR << " port=" << PORT_NUM << endl;
	 }
	 delete file_trans;*/
	TCP *tcp = new TCP();
	tcp->connectTCP(argv[1], argv[2]);
	int len = 20000;
	char *mybuf = new char[len];
	for (int i = 0; i < len; i++) {
		mybuf[i] = i%10;
	}
	tcp->write(mybuf, len);
	while (1);
	delete mybuf;
	delete tcp;
}

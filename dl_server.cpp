
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <iostream>
#include "FTP.h"
#include "UTrans.h"

using namespace std;

int main(int argc, char *argv[]) {
	 char *ADDR,*FILE,*PORT_NUM;
	 FTP* file_trans;

	 if(argc==2){
		 PORT_NUM = argv[1];
	 } else {
		 cout << "Error in command line arguments. ./dl_server <port>\n";
		 exit(-1);
	 }

	 file_trans = new FTP(NULL,NULL,PORT_NUM);
	 file_trans->openPort();
	 if(file_trans->recvFile() == -1){
		 cout << "ERROR: Unable to receive file on port " << PORT_NUM << endl;
	 }
	 delete file_trans;
}

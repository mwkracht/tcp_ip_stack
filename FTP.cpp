#include "FTP.h"

FTP::FTP(){

}

FTP::FTP(char *file, char *host_n, char *port_n){

	size_t s;
	if(file != NULL){
		s = strlen(file);
		file_name = new char[s];
		memcpy((void *)file_name, (void *)file, s);
	}
	if(host_n != NULL){
		s = strlen(host_n);
		host = new char[s];
		memcpy((void *)host, (void *)host_n, s);
	}
	if(port_n != NULL){
		s = strlen(port_n);
		port = new char[s];
		memcpy((void *)port, (void *)port_n, s);
	}
	if(host_n != NULL){proto = new UTrans(host, port, this);}
	else{proto = new UTrans(port, this);}
}

FTP::~FTP(){
	delete file_name;
	file_name = NULL;
	delete host;
	host = NULL;
	delete port; 
	port = NULL;
	delete proto;
	proto = NULL;
}

int FTP::sendFile(){

	int send_back = 0;

	if(proto->isEstablished()){
		ifstream file(file_name,ios::binary);
		char *file_buffer;

		if ( file.is_open()  ) {
			file.seekg(0, ios::end);
			file_length = file.tellg();
			file.seekg(0, ios::beg); //get the total file length

			file_buffer = new char[file_length];
			file.read(file_buffer, file_length);

			unsigned char size[4];
			int temp_size = file_length;
			for(int i=3;i>=0;i--){
				size[i] = temp_size % 256;
				temp_size = temp_size / 256;
			}

			if(proto->transmit((char *)size, 4) == -1){
				send_back = -1;
				cerr << "ERROR: Unable to transmit size of file to host.\n";
			}

			if(proto->transmit(file_buffer, file_length) == -1){
				send_back = -1;
				cerr << "ERROR: Unable to transmit due to error in UTrans protocol.\n";
			}
		
			file.close();

			delete []file_buffer;
		} else {
			cerr << "ERROR: Unable to open file.\n";
			return -1;
		}
	} else {
		return -1;
	}

	return send_back;
	
}

int FTP::recvFile(){
	if(proto->isEstablished()){
		timeval before,after;
		char *size = new char[4];
		int length = 4;
		if(proto->receive(size,length) == -1){ //must first receive file length
			printf("ERROR: Unable to receive file length in first message.\n");
			return -1;
		}
		file_length = 0;
		for(int i=0;i<4;i++){
			file_length = file_length + (unsigned char)size[i];
			if(i!=3){file_length = file_length << 8;}
		}

		printf("File Length to receive = %u\n", file_length);
		delete size;
		size = NULL;
		char *output = new char[file_length];

		//gettimeofday(&before,0);		
		if(proto->receive(output,file_length) == -1){//receive the actual file
			printf("ERROR: Unable to receive the file via socket receive.\n");
			return -1;
		}
		/*gettimeofday(&after,0);
		double elapsed = 0.0;
		if(before.tv_usec > after.tv_usec){
			double decimal = (1000000.0 - (before.tv_usec - after.tv_usec))/1000000.0;
			elapsed = after.tv_sec - before.tv_sec - 1.0;
			elapsed = elapsed + decimal;
		} else {
			double decimal = (after.tv_usec - before.tv_usec)/1000000.0;
			elapsed = after.tv_sec - before.tv_sec;
			elapsed = elapsed + decimal;
		}
		printf("Elapsed time %f sec\n", elapsed);
		double throughput = (file_length * 8.0)/(elapsed * 1000.0);
		printf("Total throughput for transmission was %f Kb/sec \n", throughput);*/

		ofstream out_file("output.dat", ios::binary);
		if(out_file.is_open()){
			out_file.write(output,file_length);
			out_file.close();
		} else {
			printf("ERROR: Unable to open output.dat\n");
			delete output;
			output = NULL;
			return -1;
		}
		delete output;
		output = NULL;

	} else {
		printf("ERROR: Unable to receive data from socket that is not established.\n");
		return -1;
	}

	return 0;
}

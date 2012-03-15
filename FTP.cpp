/*
 * Jonathan Reed - 9051-66446
 * Matthew Kracht - 9053_25165
 * ECE - 5565
 * Project 2
 */

#include "FTP.h"

#define MAX_FILE_SEG 4*65535

FTP::FTP() {

}

FTP::FTP(char *file, char *host_n, char *port_n) {

	size_t s;
	if (file != NULL) {
		s = strlen(file);
		fileName = new char[s];
		memcpy((void *) fileName, (void *) file, s);
	}
	if (host_n != NULL) {
		s = strlen(host_n);
		host = new char[s];
		memcpy((void *) host, (void *) host_n, s);
	}
	if (port_n != NULL) {
		s = strlen(port_n);
		port = new char[s];
		memcpy((void *) port, (void *) port_n, s);
	}
	tcp = new TCP();
}

FTP::~FTP() {
	delete fileName;
	fileName = NULL;
	delete host;
	host = NULL;
	delete port;
	port = NULL;
	delete tcp;
	tcp = NULL;
}

int FTP::sendFile() {

	if (tcp->connectTCP(host, port) != 0) {
		cerr << "Error: FTP Unable to connect using TCP\n";
	}

	ifstream file(fileName, ios::binary);
	char *file_buffer;

	if (file.is_open()) {
		file.seekg(0, ios::end);
		fileLength = file.tellg();
		file.seekg(0, ios::beg); //get the total file length

		unsigned char size[4];
		int temp_size = fileLength;
		for (int i = 3; i >= 0; i--) {
			size[i] = temp_size % 256;
			temp_size = temp_size / 256;
		}

		if (tcp->write((char *) size, 4) != 0) { //send length
			cerr << "ERROR: Unable to transmit size of file to host.\n";
			return -1;
		}

		file_buffer = new char[MAX_FILE_SEG];

		for (int i = fileLength; i > 0; i -= MAX_FILE_SEG) {
			printf("writing i=%d\n", i);
			if (i > MAX_FILE_SEG) {
				file.read(file_buffer, MAX_FILE_SEG);

				if (tcp->write(file_buffer, MAX_FILE_SEG) != 0) {
					cerr
							<< "ERROR: Unable to transmit due to error in UTrans protocol.\n";
					return -1;
				}
			} else {
				file.read(file_buffer, i);

				if (tcp->write(file_buffer, i) != 0) {
					cerr
							<< "ERROR: Unable to transmit due to error in UTrans protocol.\n";
					return -1;
				}
			}
		}

		tcp->closeTCP();

		file.close();

		delete[] file_buffer;
	} else {
		cerr << "ERROR: Unable to open file.\n";
		return -1;
	}

	return fileLength;

}

int FTP::recvFile() {
	timeval before, after;
	char *size, *output;
	int index;
	unsigned int retSize;

	size = new char[4];
	index = 4;
	retSize = tcp->read(size, index, 0);
	if (retSize != 4) { //must first receive file length
		cerr << "ERROR: Unable to receive file length in first message.\n";
		return -1;
	}
	fileLength = 0;
	for (int i = 0; i < 4; i++) {
		fileLength = fileLength + (unsigned char) size[i];
		if (i != 3) {
			fileLength = fileLength << 8;
		}
	}
	delete size;
	size = NULL;

	output = new char[fileLength];
	index = 0;

	gettimeofday(&before, 0);
	while (index < fileLength) {
		retSize = tcp->read(output + index, fileLength - index, 0);
		if (retSize == 0) {
			printf("ERROR: Received nothing from TCP ind:%d\n", index);
		}
		index += retSize;
	}
	//tcp->closeTCP();
	gettimeofday(&after, 0);

	double elapsed = 0.0;
	if (before.tv_usec > after.tv_usec) {
		double decimal = (1000000.0 - (before.tv_usec - after.tv_usec))
				/ 1000000.0;
		elapsed = after.tv_sec - before.tv_sec - 1.0;
		elapsed = elapsed + decimal;
	} else {
		double decimal = (after.tv_usec - before.tv_usec) / 1000000.0;
		elapsed = after.tv_sec - before.tv_sec;
		elapsed = elapsed + decimal;
	}

	double throughput = (index * 8.0) / (elapsed * 1000.0);
	printf("Throughput:%f Kb/sec \n", throughput);
	printf("Lost: %u\n", fileLength - index);
	ofstream out_file("output.dat", ios::binary);
	if (out_file.is_open()) {
		out_file.write(output, index);
		out_file.close();
	} else {
		printf("ERROR: Unable to open output.dat\n");
		delete output;
		output = NULL;
		return -1;
	}
	delete output;
	output = NULL;

	return index;
}

int FTP::openPort() {
	return tcp->listenTCP(port);
}

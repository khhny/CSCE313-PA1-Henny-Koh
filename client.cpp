/*
	Original author of the starter code
    Tanzir Ahmed
    Department of Computer Science & Engineering
    Texas A&M University
    Date: 2/8/20
	
	Please include your Name, UIN, and the date below
	Name: Henny Koh
	UIN: 333006531
*/
#include "common.h"
#include "FIFORequestChannel.h"
#include <fstream>
#include <string>
using namespace std;


int main (int argc, char *argv[]) {
	int opt;
	int p = -1;
	double t = -1;
	int e = -1;
	int m = MAX_MESSAGE;
	bool new_chan = false;
	vector<FIFORequestChannel*> channels;

	string filename = "";
	while ((opt = getopt(argc, argv, "p:t:e:f:m:c")) != -1) {
		switch (opt) {
			case 'p':
				p = atoi (optarg);
				break;
			case 't':
				t = atof (optarg);
				break;
			case 'e':
				e = atoi (optarg);
				break;
			case 'f':
				filename = optarg;
				break;
			case 'm':
				m = atoi (optarg);
				break;
			case 'c':
				new_chan = true;
				break;
		}
	}


	//give arguments for the server: ./server, -m, <val for -m arg>, NULL
	//fork
	//run execup using server args in child process
	
	if(fork() == 0) {
		std::string mstr = std::to_string(m);
        char* const args[] = {(char*)"./server", (char*)"-m", (char*)mstr.c_str(), NULL};
		execvp(args[0], args);
	}
	
    FIFORequestChannel cont_chan("control", FIFORequestChannel::CLIENT_SIDE);
	channels.push_back(&cont_chan);
	FIFORequestChannel* newchan = nullptr;
	if(new_chan){
		//send new channel req to server
		MESSAGE_TYPE nc = NEWCHANNEL_MSG;
		cont_chan.cwrite(&nc, sizeof(MESSAGE_TYPE));

		//create variable to hold the name
		char namebuf[MAX_MESSAGE] = {0};
		cont_chan.cread(namebuf, sizeof(namebuf));
		string newchanname(namebuf);
		//cread the response from server

		//call FIFORequestChannel constructor with name from server
		newchan = new FIFORequestChannel(newchanname, FIFORequestChannel::CLIENT_SIDE);
		//push new channel into vector
		channels.push_back(newchan);
	}
	
	FIFORequestChannel chan = *(channels.back());

	//Single datapoint request (if p, t, e != 1)
	if(p != -1 && t != -1 && e != -1) {
		char buf[MAX_MESSAGE]; // 256
		datamsg x(p, t, e);
		
		memcpy(buf, &x, sizeof(datamsg));
		chan.cwrite(buf, sizeof(datamsg)); // question
		double reply;
		chan.cread(&reply, sizeof(double)); //answer
		cout << "For person " << p << ", at time " << t << ", the value of ecg " << e << " is " << reply << endl;
	}
	else if (p != -1){
		//open file
		std::ofstream myfile("received/x1.csv");

		//request 1000 datapoints
		for(int i = 0; i < 1000; i++){
			//send request for ecg 1, 2
			double tempT = i * 0.004;
			
			datamsg x(p, tempT, 1);
			chan.cwrite(&x, sizeof(datamsg));
			double reply1;
			chan.cread(&reply1, sizeof(double));

			datamsg y(p, tempT, 2);
			chan.cwrite(&y, sizeof(datamsg));
			double reply2;
			chan.cread(&reply2, sizeof(double));

			//write line to folder recieved, filename: x1.csv
			myfile << tempT << "," << reply1 << "," << reply2 << "\n";
		}
		myfile.close();
	}


	if(!filename.empty()){
		filemsg fm(0, 0);
		string fname = filename;

		int len = sizeof(filemsg) + (fname.size() + 1);
		char* buf2 = new char[len]; //buff request (size of filemsg + filename)
		memcpy(buf2, &fm, sizeof(filemsg));
		strcpy(buf2 + sizeof(filemsg), fname.c_str());
		chan.cwrite(buf2, len);  // I want the file length;

		int64_t filesize = 0;
		chan.cread(&filesize, sizeof(int64_t)); // read the file length

		
		//create buffer of size buff cap (m)
		char* buf3 = new char[m];

		//loop over the segments in the file filesize / buffer cap(m)
		int bufflen = m - (int)sizeof(filemsg);
		int64_t loop = (filesize + bufflen - 1) / bufflen;
		//create filemsg instance
		filemsg* file_req = (filemsg*) buf2;
		int64_t rem = 0;

		// file_req->length = m - sizeof(filemsg);
		// file_req->offset = 0;
		// int64_t rem = filesize;
		std::ofstream copyfile("received/" + fname);

		for(int64_t i = 0; i < loop; i++){
			//set offset in the file
			file_req->offset = i * bufflen;
			//set length. be careful of last segment
			rem = filesize - file_req->offset;
			if(rem < 0){rem = 0;}

			file_req->length = min<int64_t>(bufflen, rem);

			chan.cwrite(buf2, len);
			chan.cread(buf3, file_req->length);

			//write buf3 into file: recieved/filename
			copyfile.write(buf3, file_req->length);
		}
		
		copyfile.close();


		//delete buffers
		delete[] buf2;
		delete[] buf3;
	}

	//if necessary, close and delete new channel
	if(new_chan){
		MESSAGE_TYPE quitnc = QUIT_MSG;
		newchan->cwrite(&quitnc, sizeof(MESSAGE_TYPE));

		delete newchan;
		newchan = nullptr;

	}
	
	// closing the channel    
    MESSAGE_TYPE quit = QUIT_MSG;
    cont_chan.cwrite(&quit, sizeof(MESSAGE_TYPE));
}

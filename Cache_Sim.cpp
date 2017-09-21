/**
* Title: L1 Data Cache Simulator
* @Author : Nishad Saraf, Unmeel Mokashi & Sachin Muradi
* @Course : ECE 586 - Winter 2017
* @University : Portland State University, Oregon.
* Description : Program simulates a L1 data cache. The main features of this cache are,
* 1. Data cache (DC) is a 4 way set-associative.
* 2. Designed for 32-bit, byte addressable computer architecture.
* 3. DC has (N) 1024 sets, (k) 4 lines per set and (L) a line length of 32 bytes.
* 4. Uses LRU replacement policy for victim cache, write-back for store and allocate on write.
* Input to the program is a file called trace.txt saved locally within the project directory.
*
* References:
* 1. Program structure based on Xiao Long cache in c explanation.
* 2. Dr. Mayer's lecture notes.
* 3. Stackoverflow: http://stackoverflow.com/questions/23092307/cache-simulator
*/

// header files
#include<cstdio>
#include<cstdlib>
#include<cstring>
#include<cmath>
#include<stack>
#include<string>
#include<iostream>

using namespace std;

// macros definitions
#define BYTES_LINE 32
#define SETS 1024
#define LINES 4
#define TAG 17
#define OVERHEAD 21
#define TOTAL_LINES 4096
#define MISS_COST 50
#define ACCESS_COST 1

#define Size_Per_Line BYTES_LINE

#define SIZE_SET (Size_Per_Line*LINES)
#define CACHE_SIZE (SIZE_SET*SETS)

// by default, debug, trace and version command are all disabled
bool _DEBUG1 = false;
bool _TRACE = false;
bool _VERSION = false;

/** cacheStatus is a struct to keep track of parameters of the cache*/
struct cacheStatus {
	int access;  //number of memory access
	int read; //number of reads
	int write; //number of writes
	int cyclesCache;//cycles required with cache
	int cyclesNoCache; //cycles required without cache
	int streamIn; //number of stream-ins
	int streamOut; //number of stream-outs
	int replacement;
	int miss;//number of misses
	int hit; //number of hits
	int readHit; //number of read hits
	int writeHit; //number of write hits
	int write_back_count; //number of stream out dirty lines
	double hitRate;
} status = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0.0 };

/** display the status of all parameters  */
void printStatus() {

	//internal calculations
	status.access = status.read + status.write;
	status.cyclesCache = status.access + (status.streamIn + status.streamOut) * 20;
	status.cyclesNoCache = status.access * MISS_COST;
	status.hit = status.readHit + status.writeHit;

	status.hitRate = (double)status.hit / status.access;
	//print
	printf("\n--------------STATISTICS---------------\n");
	printf("Access:%d (read:%d, write:%d)\n", status.access, status.read, status.write);
	printf("Cycles with Cache: %d \n", status.cyclesCache);
	printf("Cycles without Cache: %d\n", status.cyclesNoCache);
	printf("StreamIn: %d\n", status.streamIn);
	printf("StreamOut: %d\n", status.streamOut);
	printf("Replacement: %d\n", status.replacement);
	printf("Miss: %d\n", status.miss);
	printf("Hit: %d (readHit: %d, writeHit: %d)\n", status.hit, status.readHit, status.writeHit);
	printf("Write back count: %d\n", status.write_back_count);
	printf("HIT RATE : %f \n", status.hitRate);
	printf("-----------------END-----------------\n\n");

}


stack<string> debugInfo; //debug info

/** class for each cache line*/
class cacheline {
public:
	char tag[TAG - 3];
	bool valid;
	bool dirty;
	int lru;

	cacheline()
	{
		for (int i = 0; i<TAG; ++i)tag[i] = 'x';
		valid = false;
		dirty = false;
		lru = 0;
	}
};

/**compare tags */
bool charEqual(char* a, char* b, int N) {
	int count = 0;
	for (int i = 0; i<N; ++i) {
		if (a[i] == b[i])count++;
	}
	return count == N;
}

/** class for a set, LRU policy followed*/
class set {
public:
	cacheline* cachelines[LINES];

	set()
	{
		for (int i = 0; i<LINES; ++i) {
			cachelines[i] = new cacheline();
		}
	}

	//update LRU bits
	void updateLRU(int hit) {
		for (int i = 0; i<LINES; ++i)
		{
			if (cachelines[i]->valid)
			{
				cachelines[i]->lru++;
				cachelines[i]->lru = cachelines[i]->lru% LINES;

			}
		}
		cachelines[hit]->lru = 0;//clear the age of hit lines
	}

	/* stream a paragraph into this set, given the address */
	void streamIn(char* addrBin) {

		//find the first empty line to stream in paragraph
		int choice;//use variable to track the selected line
		for (int i = 0; i<LINES; ++i) {
			if (cachelines[i]->valid == false) {
				choice = i;
				break;
			}//end-if
		}//end-for
		cachelines[choice]->valid = true;
		cachelines[choice]->dirty = false;
		for (int j = 0; j<TAG; ++j) cachelines[choice]->tag[j] = addrBin[j];
		updateLRU(choice);
		status.streamIn++;
	}

	/* this function increases the streamOut,
	* Function only called by evictAndLoad() */
	void streamOut(int lineIndex) {
		status.streamOut++;
		if (_TRACE) fprintf(stdout, "Stream out...\n");
	}

	/*stream in a new paragraph without stream out */
	void replaceLine(int lineIndex, char* addrBin) {
		cachelines[lineIndex]->valid = true;
		cachelines[lineIndex]->dirty = false;
		for (int j = 0; j<TAG; ++j) cachelines[lineIndex]->tag[j] = addrBin[j];
		updateLRU(lineIndex);
		status.streamIn++;
		status.replacement++;//count the replacements
		if (_TRACE)fprintf(stdout, "Replace/stream in...\n");
	}

	void evictAndLoad(char* addrBin) {//problematic!!!!!!!!!!
		int choice;
		/*the loop finds the oldest line in a set*/
		for (int i = 0; i<LINES; ++i) {
			if (cachelines[i]->lru == 3) {
				choice = i;
				break;
			};
		}//end-for

		if (cachelines[choice]->dirty) {//if dirty
										//stream out then stream in
			streamOut(choice);
			status.write_back_count++;//count the write-backs
			replaceLine(choice, addrBin);
		}
		else {//if not dirty
			  //stream in to replace
			replaceLine(choice, addrBin);
		}
	}


	/* setDirty() sets a line's dirty bit, given the address to write.
	* This function is only used by write() in cache class */
	void setDirty(char* addrBin) {
		for (int i = 0; i<LINES; ++i) {
			if (charEqual(cachelines[i]->tag, addrBin, TAG)) {
				cachelines[i]->dirty = true;
			}
		}//end-for
	}

};

// this function is only used in debugging....
/*this function converts an Int to binary form in char[]*/
char* DecToBin(int addrDec) {
	char* addrBin = new char[32];//or 33??
	for (int i = 0; i<32; ++i) {
		if ((addrDec & (1 << i)) >> i) addrBin[31 - i] = '1';
		else addrBin[31 - i] = '0';
	}
	return addrBin;
}

/* hexToBin() converts hex style into binary style, both in string type
* this function is fat.. i hope i can come up with a cleaner one..*/
char* hexToBin(char* input) {
	int x = 4;
	int size;
	size = strlen(input);


	char* output = new char[size * 4 + 1];

	for (int i = 0; i < size; i++)
	{
		if (input[i] == '0') {
			output[i*x + 0] = '0';
			output[i*x + 1] = '0';
			output[i*x + 2] = '0';
			output[i*x + 3] = '0';
		}
		else if (input[i] == '1') {
			output[i*x + 0] = '0';
			output[i*x + 1] = '0';
			output[i*x + 2] = '0';
			output[i*x + 3] = '1';
		}
		else if (input[i] == '2') {
			output[i*x + 0] = '0';
			output[i*x + 1] = '0';
			output[i*x + 2] = '1';
			output[i*x + 3] = '0';
		}
		else if (input[i] == '3') {
			output[i*x + 0] = '0';
			output[i*x + 1] = '0';
			output[i*x + 2] = '1';
			output[i*x + 3] = '1';
		}
		else if (input[i] == '4') {
			output[i*x + 0] = '0';
			output[i*x + 1] = '1';
			output[i*x + 2] = '0';
			output[i*x + 3] = '0';
		}
		else if (input[i] == '5') {
			output[i*x + 0] = '0';
			output[i*x + 1] = '1';
			output[i*x + 2] = '0';
			output[i*x + 3] = '1';
		}
		else if (input[i] == '6') {
			output[i*x + 0] = '0';
			output[i*x + 1] = '1';
			output[i*x + 2] = '1';
			output[i*x + 3] = '0';
		}
		else if (input[i] == '7') {
			output[i*x + 0] = '0';
			output[i*x + 1] = '1';
			output[i*x + 2] = '1';
			output[i*x + 3] = '1';
		}
		else if (input[i] == '8') {
			output[i*x + 0] = '1';
			output[i*x + 1] = '0';
			output[i*x + 2] = '0';
			output[i*x + 3] = '0';
		}
		else if (input[i] == '9') {
			output[i*x + 0] = '1';
			output[i*x + 1] = '0';
			output[i*x + 2] = '0';
			output[i*x + 3] = '1';
		}
		else if (input[i] == 'a') {
			output[i*x + 0] = '1';
			output[i*x + 1] = '0';
			output[i*x + 2] = '1';
			output[i*x + 3] = '0';
		}
		else if (input[i] == 'b') {
			output[i*x + 0] = '1';
			output[i*x + 1] = '0';
			output[i*x + 2] = '1';
			output[i*x + 3] = '1';
		}
		else if (input[i] == 'c') {
			output[i*x + 0] = '1';
			output[i*x + 1] = '1';
			output[i*x + 2] = '0';
			output[i*x + 3] = '0';
		}
		else if (input[i] == 'd') {
			output[i*x + 0] = '1';
			output[i*x + 1] = '1';
			output[i*x + 2] = '0';
			output[i*x + 3] = '1';
		}
		else if (input[i] == 'e') {
			output[i*x + 0] = '1';
			output[i*x + 1] = '1';
			output[i*x + 2] = '1';
			output[i*x + 3] = '0';
		}
		else if (input[i] == 'f') {
			output[i*x + 0] = '1';
			output[i*x + 1] = '1';
			output[i*x + 2] = '1';
			output[i*x + 3] = '1';
		}
	}

	output[32] = '\0';
	return output;
}



/* this function returns the setIndex, given an requested address */
int getSetIndex(char* addrBin) {
	int tmp = 0;
	for (int i = 0; i<6; ++i) {
		if (addrBin[TAG + i] == '1') tmp += pow(2, 5 - i);
	}
	return tmp;
}

/** the cache class is the entry for cache operations **/
class cache {
public:
	set* sets[SETS];

	cache() {//constructor
		for (int i = 0; i<SETS; ++i) sets[i] = new set();
	}

	/*function isHit() matches address with cache directory(tags)*/
	bool isHit(char* addrBin, int setIndex) {
		for (int j = 0; j<LINES; ++j) {
			if (charEqual(sets[setIndex]->cachelines[j]->tag, addrBin, TAG) && sets[setIndex]->cachelines[j]->valid) {
				if (_TRACE)fprintf(stdout, "Hit on set %d line %d\n\n", setIndex, j);
				return true;
			}
		}//end-for
		return false;
	}

	/* this function checks if there is empty line(s) within a given set */
	bool emptyLineAvailable(int setIndex) {
		for (int i = 0; i<LINES; ++i) {
			if (sets[setIndex]->cachelines[i]->valid == false) return true;
		}
		return false;
	}

	/*this function implements the logic to handle read request */
	void read(char* addrBin) {
		int setIndex = getSetIndex(addrBin);
		if (_TRACE)fprintf(stdout, "Reading set %d...\n ", setIndex);
		status.read++;
		if (isHit(addrBin, setIndex)) {
			status.readHit++;
			//read here..
		}
		else { // what about miss?
			status.miss++;
			if (emptyLineAvailable(setIndex)) {// still got space?
				if (_TRACE)fprintf(stdout, "R missed, space available, stream in \n\n");
				sets[setIndex]->streamIn(addrBin);//stream in
			}
			else { // what about full?
				if (_TRACE)fprintf(stdout, "R missed and full, do eviction\n\n");
				sets[setIndex]->evictAndLoad(addrBin); //stream out and stream in
			}
		}

		if (_DEBUG1) {// print debug info if activated, ONLY valid lines will be print
			for (int i = 0; i<SETS; ++i) {
				for (int j = 0; j<LINES; ++j) {
					if (sets[i]->cachelines[j]->valid) {
						char buffer[80];
						sprintf(buffer, "Set:%d\tLine:%d\tDirty:%d\tValid:%d\tLRU:%d\tTag:%s\n", i, j, sets[i]->cachelines[j]->dirty,
							sets[i]->cachelines[j]->valid, sets[i]->cachelines[j]->lru, sets[i]->cachelines[j]->tag);
						debugInfo.push(buffer);
					}
				}
			}
		}
	}

	/*this function implements the logic to handle write requests*/
	void write(char* addrBin) {
		int setIndex = getSetIndex(addrBin);
		if (_TRACE)fprintf(stdout, "Writing set %d...\n", setIndex);
		status.write++;
		if (isHit(addrBin, setIndex)) {
			status.writeHit++;
			//write here..
			sets[setIndex]->setDirty(addrBin);//SET DIRTY
		}
		else {//what if miss?
			status.miss++;
			if (emptyLineAvailable(setIndex)) {// got space?
											   //stream in
				if (_TRACE)fprintf(stdout, "W missed but not full, stream in\n ");
				sets[setIndex]->streamIn(addrBin);
				sets[setIndex]->setDirty(addrBin);//SET DIRTY
			}
			else {//what if full?
				if (_TRACE)fprintf(stdout, "W missed and full, do eviction\n");
				sets[setIndex]->evictAndLoad(addrBin);
				//some store operation
				sets[setIndex]->setDirty(addrBin);//SET DIRTY

			}//end-if-else
		}//end-if-else

		if (_DEBUG1) {// print debug info if activated, ONLY valid lines will be print
			for (int i = 0; i<SETS; ++i) {
				for (int j = 0; j<LINES; ++j) {
					if (sets[i]->cachelines[j]->valid) {
						char buffer[80];
						sprintf(buffer, "Set:%d; Line:%d; Dirty:%d; Valid:%d; LRU:%d; Tag:%s\n", i, j, sets[i]->cachelines[j]->dirty,
							sets[i]->cachelines[j]->valid, sets[i]->cachelines[j]->lru, sets[i]->cachelines[j]->tag);
						debugInfo.push(buffer);
					}
				}
			}
		}

	}//end

};


/* The driver creates and drives the cache. It scans the input
 and extract the r/w instructions to drive the cache, create statistics
* It reports errors if input file is not valid or contains errors  */
class driver {
	char* fileString;//to store filename passed with command
public:
	driver(char* arg) {
		fileString = arg;
	}

	void process() {
		//reopen and redirect stdin to the file
		FILE* inFile = freopen("trace.txt", "r", stdin);
		if (inFile == NULL)printf("INVALID FILE, TRY AGAIN! \n \n");
		else {
			printf("INPUT FILE SUCCESSFULLY LOADED \n");
			cache* myCache = new cache();

			while (true) {
				char temp[2]; //to store read/write
				fscanf(stdin, "%s", temp);
				if (!strcmp(temp, "-v")) {
					_VERSION = true;
					temp[0] = ' '; //the temp need to be cleared, to prevent infinite loop!
					continue;
				}
				if (!strcmp(temp, "-d")) {
					_DEBUG1 = true;
					temp[0] = ' ';
					continue;
				}
				if (!strcmp(temp, "-t")) {
					_TRACE = true;
					temp[0] = ' ';
					continue;
				}
				char temp2[10];//to store address starting with 0x..
				fscanf(stdin, "%s", temp2);
				if (feof(stdin)) break;
				if (!strcmp(temp, "r") || !strcmp(temp, "R")) {
					if (_TRACE)printf("Reading from address: %s\n", temp2);
					myCache->read(hexToBin(&temp2[2]));
				}
				else if (!strcmp(temp, "w") || !strcmp(temp, "W")) {
					if (_TRACE)printf("Writing to address: %s\n", temp2);
					myCache->write(hexToBin(&temp2[2]));
				}
				//report strange character in input if found
				else {
					printf("STRANGE CHAR! CHECK YOUR INPUT\n");
					return;//exit prog
				}

			}//end-while-loop

			fclose(stdin);//close the stdin stream
			if (_VERSION)printf("[[- BUILD VERSION: 1.4 -]]\n");
			if (_DEBUG1)printf("[[- DEBUG ACTIVATED -]]\n");
			if (_TRACE)printf("[[- TRACE ACTIVATED -]]\n");
			printStatus();
		}//end-else

		 /* following block prints out debug information  */
		if (_DEBUG1)printf("BELOW IS DEBUG INFORMATION IN LIFO ORDER\n");
		while (!debugInfo.empty()) {
			if (_DEBUG1)cout << debugInfo.top();
			debugInfo.pop();
		}

	}//end-function

};

//main here
int main(int argc, char* argv[]) {
	//set up Version, Trace, Debug if entered from keyboard
	for (int n = 1; n<argc; ++n) {
		if (strcmp(argv[n], "-d") == 0) _DEBUG1 = true;
		if (strcmp(argv[n], "-v") == 0) _VERSION = true;
		if (strcmp(argv[n], "-t") == 0) _TRACE = true;
	}//end-for

	 //initialization by providing the input file
	driver* myDriver = new driver(argv[argc - 1]);
	myDriver->process();
}

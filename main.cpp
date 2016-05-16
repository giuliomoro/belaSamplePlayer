/*
 * default_main.cpp
 *
 *  Created on: Oct 24, 2014
 *      Author: parallels
 */
#include <unistd.h>
#include <iostream>
#include <cstdlib>
#include <libgen.h>
#include <signal.h>
#include <getopt.h>
#include <vector>
#include "../include/BeagleRT.h"

using namespace std;

// Handle Ctrl-C by requesting that the audio rendering stop
void interrupt_handler(int var)
{
	gShouldStop = true;
}

// Print usage information
void usage(const char * processName)
{
	cerr << "Usage: " << processName << " [options]" << endl;

	BeagleRT_usage();
	cerr << "   --filesAtOnce [-o] val        \n";
	cerr << "   --usedFiles [-u] val        \n";
	cerr << "   --alternateDrives [-a] val        \n";
	cerr << "   --blockSize [-k] val        \n";
	cerr << "   --bufferSize [-b] val        \n";
	cerr << "   --priorityManagement [-y] val        \n";
	cerr << "   --preloadSize [-e] val        \n";
	cerr << "   --emptyAlloc [-l] val       MB \n";
	cerr << "   --help [-h]:                Print this menu\n";
}

extern int filesAtOnce; // files to play at once
extern int usedFiles; //files in use
extern bool alternateDrives;
extern int blockSize; // size of the block that is read from disk.
extern int bufferSize;
extern int priorityManagement;
extern int preloadSize;


int main(int argc, char *argv[])
{
	BeagleRTInitSettings settings;	// Standard audio settings

	struct option customOptions[] =
	{
		{"help", 0, NULL, 'h'},
		{"filesAtOnce", 1, NULL, 'o'},
		{"usedFiles", 1, NULL, 'u'},
		{"alternateDrives", 1, NULL, 'a'},
		{"blockSize", 1, NULL, 'k'},
		{"bufferSize", 1, NULL, 'b'},
		{"priorityManagement", 1, NULL, 'y'},
		{"preloadSize", 1, NULL, 'e'},
		{"emptyAlloc", 1, NULL, 'l'},
		{NULL, 0, NULL, 0}
	};

	// Set default settings
	BeagleRT_defaultSettings(&settings);
	int uselessAlloc = false;
	// Parse command-line arguments
	while (1) {
		int c;
		if ((c = BeagleRT_getopt_long(argc, argv, "ho:u:a:k:b:y:e:l:", customOptions, &settings)) < 0)
				break;
		switch (c) {
		case 'o':
			filesAtOnce = atoi(optarg);
			break;
		case 'u':
			usedFiles = atoi(optarg);
			break;
		case 'a':
			alternateDrives = (bool)atoi(optarg);
			break;
		case 'k':
			blockSize = atoi(optarg);
			break;
		case 'b':
			bufferSize = atoi(optarg);
			break;
		case 'y':
			priorityManagement = atoi(optarg);
			break;
		case 'e':
			preloadSize = atoi(optarg);
			break;
		case 'l':
			uselessAlloc = atoi(optarg);
			break;
		case '?':
		case 'h':
			usage(basename(argv[0]));
			exit(0);
		default:
			usage(basename(argv[0]));
			exit(1);
		}
	}
	std::vector<float> useless;
	if(uselessAlloc > 0){
		int size = uselessAlloc*1024*(1024/sizeof(float));
		printf("%d reserving: %d\n", uselessAlloc, size*4/1024/1024);
		useless.reserve(size);
	}
	// Initialise the PRU audio device
	if(BeagleRT_initAudio(&settings, 0) != 0) {
		cout << "Error: unable to initialise audio" << endl;
		return -1;
	}

	// Start the audio device running
	if(BeagleRT_startAudio()) {
		cout << "Error: unable to start real-time audio" << endl;
		return -1;
	}

	// Set up interrupt handler to catch Control-C and SIGTERM
	signal(SIGINT, interrupt_handler);
	signal(SIGTERM, interrupt_handler);

	// Run until told to stop
	while(!gShouldStop) {
		usleep(100000);
	}

	// Stop the audio device
	BeagleRT_stopAudio();

	// Clean up any resources allocated for audio
	BeagleRT_cleanupAudio();

	// All done!
	return 0;
}

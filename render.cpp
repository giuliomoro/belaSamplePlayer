/*
 * render.cpp
 *
 *  Created on: Oct 24, 2014
 *      Author: parallels
 */


#include <BeagleRT.h>
#include <cmath>
#include <Utilities.h>
#include "ReadFileVoice.h"

AuxiliaryTask ReadFileVoice::readingFileTask_ = NULL;
bool ReadFileVoice::staticConstructed_ = false;
std::vector<ReadFileVoice*> ReadFileVoice::instances_;
std::vector<file_sample_t> ReadFileVoice::tempBuffer_;
unsigned int ReadFileVoice::blockSize_;

float gFrequency = 440.0;
float gPhase;
float gInverseSampleRate;

// setup() is called once before the audio rendering starts.
// Use it to perform any initialisation and allocation which is dependent
// on the period size or sample rate.
//
// userData holds an opaque pointer to a data structure that was passed
// in from the call to initAudio().
//
// Return true on success; returning false halts the program.

std::vector<PlayFile*> playFiles;
AuxiliaryTask statusTask;

void currentsStatus(){
	playFiles[0]->checkStatus();
	rt_printf("Underruns: %d %2.3f%%:\n\n", underrunCounter, 100*underrunCounter/(float)totalCalls);
}
void statusTaskLoop(){
	int n = 0;
	while(!gShouldStop){
		n++;
		if(n % 100 == 0){
//			playFiles[0]->checkStatus();
//			rt_printf("Underruns: %d %2.3f%%:\n", underrunCounter, 100*underrunCounter/(float)totalCalls);
		}
		usleep(10000);
	}
}

int availableFiles = 1000;

int filesAtOnce = 40; // files to play at once
int usedFiles = 1000; //files in use
bool alternateDrives = false;
int blockSize = 4096; // size of the block that is read from disk.
int bufferSize = 32768;
int priorityManagement = 0;
int preloadSize = 32768*2;

std::vector<int> audioFiles;
bool setup(BeagleRTContext *context, void *userData)
{
	int maxNumFiles = availableFiles; //determines how many preloadSize are all allocated
	int numVoices = filesAtOnce*9; // determines how many bufferSize are allocated
	int totalMemory = preloadSize*sizeof(sample_t) * maxNumFiles + numVoices * bufferSize * sizeof(sample_t);

	printf("filesAtOnce: %d, usedFiles: %d, alternateDrives: %d. blockSize: %d, bufferSize: %d, priorityManagement: %d, preloadSize: %d\n",
			filesAtOnce, usedFiles, alternateDrives, blockSize, bufferSize, priorityManagement, preloadSize);
	printf("Memory usage:\nbufferSize %d, numVoices: %d, preloadSize: %d, maxNumFiles: %d, total memory: %5.1fMB\n",
			bufferSize, numVoices, preloadSize, maxNumFiles, totalMemory/1024.f/1024.f);
//	if(totalMemory > 400*1024*1024){
//		printf("Too much memory requested\n");
//		return false;
//	}
	playFiles.push_back(new PlayFile(preloadSize, blockSize, bufferSize, maxNumFiles, numVoices));
	PlayFile* pf = playFiles[0];
	char filename[100];
	audioFiles.reserve(usedFiles);
	printf("Loading %d files\n", usedFiles);
	char path[2][100] = {"../BeagleRT/source/samples/sin%04d.bin", "/mnt/root/BeagleRT/source/samples/sin%04d.bin"};

	for(int n = 0; n < usedFiles; ++n){
		int pathNumber = alternateDrives ? n%2 : 0;
		snprintf(filename, 100, path[pathNumber], (int)(0.5+n*(availableFiles/(float)usedFiles)));
		int audioFile = pf->open((char*)filename);
		if(audioFile < 0){
			rt_printf("error: could not open file %s\n", filename);
			return false;
		}
//		printf("Loading audioFile: %d %s\n", audioFile, filename);
		printf(".");
		fflush(stdout);
		audioFiles.push_back(audioFile);
	}
	printf("\n");
	statusTask = BeagleRT_createAuxiliaryTask(statusTaskLoop, 50, "status");
	BeagleRT_scheduleAuxiliaryTask(statusTask);
	return true;
}

// render() is called regularly at the highest priority by the audio engine.
// Input and output are given from the audio hardware and the other
// ADCs and DACs (if available). If only audio is available, numMatrixFrames
// will be 0.

void render(BeagleRTContext *context, void *userData)
{
	PlayFile* pf = playFiles[0];
	for(unsigned int n = 0; n < context->audioFrames; ++n){
		if((context->audioSampleCount + n) % (int)(context->audioSampleRate/2) == 0){
			for(int k = 0; k < filesAtOnce; ++k){
				static unsigned int nextAudioFile = 0;
//				rt_printf("start_playing %d size:%d\n", audioFiles[nextAudioFile], audioFiles.size());
				pf->startPlaying(audioFiles[nextAudioFile++]);
				if(nextAudioFile == audioFiles.size()){
					nextAudioFile = 0;
				}
			}
		}
		audioWriteFrame(context, n, 0, 0);
	}
	pf->getSamplesAdd(context->audioOut, context->audioFrames, context->audioChannels);
	float coeff = 0.05/filesAtOnce;
	for(unsigned int n = 0; n < context->audioFrames; ++n){
		context->audioOut[n*context->audioChannels] = context->audioOut[n*context->audioChannels]*coeff;
		context->audioOut[n*context->audioChannels + 1] = context->audioOut[n*context->audioChannels];
	}
}

// cleanup() is called once at the end, after the audio has stopped.
// Release any resources that were allocated in setup().

void cleanup(BeagleRTContext *context, void *userData)
{
	for(unsigned int n = 0; n < playFiles.size(); ++n){
		delete playFiles[n];
	}
}

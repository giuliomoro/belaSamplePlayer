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

PlayFile* pf;
int audioFile;
AuxiliaryTask statusTask;

void statusTaskLoop(){
	int n = 0;
	while(!gShouldStop){
		n++;
		if(n % 100 == 0){
//			pf->checkStatus();
		}
		usleep(10000);
	}
}

bool setup(BeagleRTContext *context, void *userData)
{
	int preloadSize = 32768;
	int blockSize = 16384;
	int bufferSize = blockSize * 4;
	int maxNumFiles = 1;
	int numVoices = 10;
	pf = new PlayFile(preloadSize, blockSize, bufferSize, maxNumFiles, numVoices);
	audioFile = pf->open("../BeagleRT/source/sin0100.bin");
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
	for(unsigned int n = 0; n < context->audioFrames; ++n){
		if((context->audioSampleCount + n) % (int)(context->audioSampleRate) == 0){
			rt_printf("start_playing\n");
			pf->startPlaying(audioFile);
		}
		audioWriteFrame(context, n, 0, 0);
	}
	pf->getSamplesAdd(context->audioOut, context->audioFrames, context->audioChannels);
	for(unsigned int n = 0; n < context->audioFrames; ++n){
		context->audioOut[n*context->audioChannels] = context->audioOut[n*context->audioChannels]*0.1;
		context->audioOut[n*context->audioChannels + 1] = context->audioOut[n*context->audioChannels];
	}
}

// cleanup() is called once at the end, after the audio has stopped.
// Release any resources that were allocated in setup().

void cleanup(BeagleRTContext *context, void *userData)
{
	delete pf;
}

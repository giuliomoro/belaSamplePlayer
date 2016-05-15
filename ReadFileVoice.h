#include <stdio.h>
#include <BeagleRT.h>
#include <assert.h>
#include <vector>
#include <string.h>
#include <fcntl.h>

#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
  TypeName(const TypeName&);               \
  void operator=(const TypeName&)

typedef int16_t file_sample_t; // the sample type as read from the file
typedef float sample_t; // the external sample type, requested by the calling application
typedef int file_t;

namespace ReadSamplesFromDisk{
// utility functions to open and read files
// later to be implemented with any audio-specific library of choice
	sample_t fileToSample(file_sample_t in){
		return in / 32768.f;
	}
	file_t open(char* filename){
		return ::open(filename, O_RDONLY | O_NONBLOCK);
	}
	int read(void* destination, unsigned int requestedLength, file_t fid, unsigned int offset){
		int ret = ::pread(fid, destination, requestedLength*sizeof(file_sample_t), offset*sizeof(file_sample_t));
		if(ret>=0){
			return ret/sizeof(file_sample_t);
		} else {
			return ret;
		}
	}
	int readConvert(sample_t* destination, file_sample_t* temp, unsigned int requestedLength, file_t fid, unsigned int offset){
		int ret = ReadSamplesFromDisk::read(temp, requestedLength, fid, offset);
		for(int n = 0; n < ret; ++n){
			destination[n] = ReadSamplesFromDisk::fileToSample(temp[n]);
		}
		return ret;
	}
};

namespace  SampleBuffer{
	void add(sample_t* source, sample_t* destination, unsigned int size, int destinationStep){
		for(unsigned int n = 0; n < size; ++n){
			destination[n*destinationStep] += source[n];
		}
	}
	void copy(sample_t* source, sample_t* destination, unsigned int size, int destinationStep){
		for(unsigned int n = 0; n < size; ++n){
			destination[n*destinationStep] = source[n];
		}
	}
};

class ReadFileVoice {
public:
	ReadFileVoice(int bufferSize, int blockSize){
		init(bufferSize, blockSize);
	};
	~ReadFileVoice(){
		deallocate();
		for(unsigned int n = 0; n < instances_.size(); ++n){
			if(this == instances_[n]){
				instances_.erase(instances_.begin() + n);// TODO: remove element
				break;
			}
		}
	}
	void init(unsigned int bufferSize, unsigned int blockSize){
		//TODO: blockSize is currently assigned to a static variable, so
		// values of blockSize passed after the first call are ignored.

		readPointer_ = -1;
		writePointer_ = -1;
		isPlaying_ = false;
		loadCompleted_ = false;
		bufferSize_ = bufferSize;
		blockSize_ = blockSize;
		ReadFileVoice::staticConstructor();
		allocate();
		instances_.push_back(this);
	}
	void startPlaying(file_t fd, unsigned int offset){
        //adds the file descriptor to the files to be read from, with
        //the appropriate offset
//		printf("start playing ..........\n");
//		printf("scheduling.........\n");
		BeagleRT_scheduleAuxiliaryTask(ReadFileVoice::readingFileTask_);
		fd_ = fd;
		samplesOffsetInFile_ = offset;
		readPointer_ = 0;
		writePointer_ = 0;
		readSamples_ = 0;
		writtenSamples_ = 0;
		isPlaying_ = true;
		loadCompleted_ = false;
	}
	void stopPlaying(){
		isPlaying_ = false;
	}
	int getSamplesAdd(float *destination, unsigned int size, int step){ //REALTIME
		assert(readPointer_ < bufferSize_);
		assert(readPointer_ >= 0);
		if(getActualNumberOfBufferedSamples() < (int)size && loadCompleted_ == false){
			rt_printf("\nbuffer underrun\n");
		}
		size = std::min((int)size, writtenSamples_ - readSamples_);
		//TODO: add here to find end of file
		int readFromCurrentReadPointer = std::min((bufferSize_ - readPointer_), (int)size);
		int readFromBeginning = size - readFromCurrentReadPointer;
		if(readFromCurrentReadPointer > 0){
			SampleBuffer::add(&buffer_[readPointer_], destination, readFromCurrentReadPointer, step);
			readPointer_ += readFromCurrentReadPointer;
		}
		if(readFromBeginning > 0){
			if(readPointer_ == bufferSize_){
				readPointer_ -= bufferSize_;
			}
			SampleBuffer::add(&buffer_[readPointer_], destination, readFromBeginning, step);
			readPointer_ += readFromBeginning;
		}
		if(readPointer_ >= bufferSize_){
			readPointer_ -= bufferSize_;
		}
		readSamples_ += readFromCurrentReadPointer + readFromBeginning;
		return readFromCurrentReadPointer + readFromBeginning;
	}
	int getActualNumberOfBufferedSamples(){
		return (bufferSize_ + writePointer_ - readPointer_) % bufferSize_;
	}

protected:
	static AuxiliaryTask readingFileTask_;
	static bool staticConstructed_;
	static void readingFileTask(){
		printf("readingFileTask\n");
		while(!gShouldStop){
			for(unsigned int n = 0; n < instances_.size(); ++n){
				instances_[n]->loadNextBlock();
			}
			usleep(2000);
		}
	}
	void staticConstructor(){
		if(staticConstructed_ == false){
			staticConstructed_ = true;
			tempBuffer_.assign(blockSize_, 0);
			readingFileTask_ = BeagleRT_createAuxiliaryTask(ReadFileVoice::readingFileTask, 90, "readingFileTask");
		}
	}
	static std::vector<ReadFileVoice*> instances_;
	static std::vector<file_sample_t> tempBuffer_;
private:
	DISALLOW_COPY_AND_ASSIGN(ReadFileVoice);
	void loadNextBlock(){
		if (    isPlaying_ && !loadCompleted_ &&
				(writtenSamples_ + (int)blockSize_ < readSamples_ + (int)bufferSize_) // avoid buffer overrun
			)
			{
			int requestedLength = blockSize_;
//			printf("writePointer_: %5d, readPointer_: %5d, bufferedSamples: %5d\rn,
//					writePointer_, readPointer_, getActualNumberOfBufferedSamples());
			int length = ReadSamplesFromDisk::readConvert(
					&buffer_[writePointer_], tempBuffer_.data(), requestedLength, fd_, samplesOffsetInFile_
				);
			if(length < 0){
				printf("error while reading \n" );
				return;
			} else if (length < requestedLength) {
				loadCompleted_ = true;
			}
			writePointer_ += length;
			writtenSamples_ += length;
			samplesOffsetInFile_ += length;
			if(writePointer_ == bufferSize_){
				writePointer_ = 0;
			}
			assert(writePointer_ < bufferSize_);// we assume that blockSize_ is a sub-multiple of bufferSize_
		}
	}
	file_t fd_;
	bool isPlaying_;
	bool loadCompleted_;
	static unsigned int blockSize_;
	int bufferSize_;
	sample_t* buffer_;
	int writePointer_;
	int readPointer_;
	int writtenSamples_;
	int readSamples_;
	unsigned int samplesOffsetInFile_;
	void allocate(){
		buffer_ = new sample_t[bufferSize_];
	}
	void deallocate(){
		delete[] buffer_;
	}
};

class PlayFile{
	struct AudioFile{
		file_t fd;
		int readPointer;
		sample_t* head;
		int jobId;
		AudioFile(int preloadSize){
			fd = -1;
			readPointer = -1;
			head = new sample_t[preloadSize];
			jobId = -1;
		}
		~AudioFile(){
			delete[] head;
		}
	};
	struct Voice{
		int fileReadPointer;
		ReadFileVoice* readFileVoice;
		int fileNumber;
		Voice(int bufferSize, int blockSize):
			fileReadPointer(-1),
			fileNumber(-1)
		{
			readFileVoice = new ReadFileVoice(bufferSize, blockSize);
		};
		~Voice(){
			delete readFileVoice;
		};
	};
public:
	PlayFile(int preloadSize, int blockSize, int bufferSize, int maxNumFiles, int numVoices){
		init(preloadSize, blockSize, bufferSize, maxNumFiles, numVoices);
	}
	~PlayFile(){
		deallocate();
	}

	void init(int preloadSize, int blockSize, int bufferSize, int maxNumFiles, int numVoices){
		audioFilesPointer_ = 0;
		preloadSize_ = preloadSize;
		blockSize_ = blockSize;
		preloadBuffer_ = new file_sample_t[preloadSize_];
		// allocate the voices that will read from disk upon request.
		voices_.reserve(numVoices);
		for(int n = 0; n < numVoices; ++n){
			voices_.push_back(new Voice(bufferSize, blockSize_));
		}
		// allocates maxNumFiles AudioFiles
		// having a fixed length makes it easier to re-use a given fileId,
		// but you are not going to see any difference if you never close files.
		audioFiles_.reserve(maxNumFiles);
		for(int n = 0; n < maxNumFiles; ++n){
			audioFiles_.push_back(new AudioFile(preloadSize));
		}
	}

	/**
	 * This is not thread-safe. Make sure you call open() from only one thread at a time.
	 */
	int open(char* filename){
        //open file
		file_t fd = ReadSamplesFromDisk::open(filename);
        if(fd < 0){
        	return -1;
        }
		AudioFile* audioFile = audioFiles_[audioFilesPointer_];
        audioFile->fd = fd;
        //load offset samples into the head
        ReadSamplesFromDisk::readConvert(audioFile->head, preloadBuffer_, preloadSize_, fd, 0);
        //returns the associated file number, to be used to refer to this file in the future
		return audioFilesPointer_++;
	}

    int startPlaying(int fileNumber){ //REALTIME
    	if(fileNumber < 0)
    		return -1;
    	// finds a suitable available voice(TODO: steal as necessary)
    	// the number of voices is expected to be reasonably small, so
    	// traversing the array looking for a free one should not
    	// be a big performance hit
    	unsigned int n;
    	for(n = 0; n < voices_.size(); ++n){
    		if(voices_[n]->fileReadPointer == -1){
    			break;
    		}
    	}

    	// check that a voice was found
    	if(n == voices_.size()){
    		rt_printf("no voices available for fileNumber: %d\n", fileNumber);
    		return -1;
    	}
    	// n is the voice we will assign to this fileNumber

    	// tells the voice to start reading the file from disk
    	voices_[n]->readFileVoice->startPlaying(audioFiles_[fileNumber]->fd, preloadSize_);
    	voices_[n]->fileReadPointer = 0; //this marks the voice as active
    	voices_[n]->fileNumber = fileNumber;
    	return n;
    }

    int getSamplesAdd(float* destination, int size, int step){ //REALTIME
    	int max = 0;
    	for(unsigned int n = 0; n < voices_.size(); n++){
    		if(voices_[n]->fileReadPointer >= 0){
    			int ret = getSamplesAddVoice(n, destination, size, step);
    			if(ret > max){
    				max = ret;
    			}
    		}
    	}
    	return max;
    }

    int getSamplesAddVoice(int v, float* destination, int size, int step){ //REALTIME
// places up to size samples from jobId into destination with the given step(e.g.: for interleaved buffers)
// returns the number of samples added. If return < size then the jobId
// is not to be used again ( end of file )
// reads file from head or, when finished, from the PlayFileVoice associated with the current jobId
    	Voice* voice = voices_[v];
    	int fileReadPointer = voices_[v]->fileReadPointer;
    	int samplesLeftInHead = preloadSize_ - fileReadPointer;
    	int readFromHead = samplesLeftInHead > 0 ? std::min(samplesLeftInHead, size) : 0;
    	int readFromVoice = size - readFromHead;
    	int ret = 0;
    	if(readFromHead > 0){ // read from head buffer
    		SampleBuffer::add(&(audioFiles_[voice->fileNumber]->head[fileReadPointer]), destination, readFromHead, step);
    		ret += size;
//    		rt_printf(".");
    	}
    	if(readFromVoice > 0){ // read from readFileVoice
    		ret += voice->readFileVoice->getSamplesAdd(destination+(readFromHead*step), readFromVoice, step);
    	}
    	if(ret < size){ // finished reading file
    		voice->fileReadPointer = -1;
    		voice->fileNumber = -1;
    	} else {
    		voice->fileReadPointer += ret;
    	}
    	return ret;
    }
    void checkStatus(){
    	int min = 1000000000;
    	int max = 0;
    	for(unsigned int n = 0; n < voices_.size(); ++n){
    		int num = voices_[n]->readFileVoice->getActualNumberOfBufferedSamples();
    		min = std::min(min, num);
    		max = std::max(max, num);
    	}
    	rt_printf("Buffered samples: min %d, max %d\n", min, max);
    }
private:
    DISALLOW_COPY_AND_ASSIGN(PlayFile);
	void allocate(){
//		buffer_ = new file_sample_t[preloadSize_];
	}
	void deallocate(){
		delete[] preloadBuffer_;
		for(unsigned int n = 0; n < voices_.size(); ++n){
			delete voices_[n];
		}
		for(unsigned int n = 0; n < audioFiles_.size(); ++n){
			delete audioFiles_[n];
		}
	}
	unsigned int blockSize_;
	unsigned int preloadSize_;
	file_sample_t* preloadBuffer_;
	std::vector<Voice*> voices_;
	std::vector<AudioFile*> audioFiles_;
	unsigned int audioFilesPointer_;
};


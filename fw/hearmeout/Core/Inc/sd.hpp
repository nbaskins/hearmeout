/*
 * Class for handling SD data transfers and DMA with CCR
 */

#include "fatfs.h"
#include "sd_diskio_spi.h"
#include "sd_spi.h"
#include "ff.h"
#include "ffconf.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <vector>
#include <string>

#define BUFFER_SIZE 2048

class SD {
private:
	FIL file; // file being read from SD card
	FATFS fs; // FATFS filesystem object
	char sd_path[4]; // char array for storing sd path info
	std::string filename; // name of file being read from SD card
	uint16_t sd_buffer1[BUFFER_SIZE]; // buffer of SD data
	uint16_t sd_buffer2[BUFFER_SIZE]; // buffer of SD data
	uint16_t* consumer_buf = sd_buffer1; // pointer to buffer currently being consumed by CCR
	uint16_t* producer_buf = sd_buffer2; // pointer to buffer being filled from SD card
	TIM_HandleTypeDef* htim1_DIR; // pointer to timer handle for transducers
	TIM_HandleTypeDef* htim2_EN; // pointer to timer handle for transducers
	DMA_HandleTypeDef* hdma_ptr; // pointer to dma handle for tim up
	bool need_refill; // bool that represents if a refill is needed for producer_buf
	bool next_requested; //bool that represents if the next song is requested
	bool play_requested;
	bool pause_requested;
	bool continuous;     //skips to next song after song ends
	std::vector<std::string> wav_paths; // vector of wav file paths

	size_t current_wav; // stores the current wav file in wav_paths
	uint32_t cur_song_total_bytes; // bytes remaining in current wav file
	uint32_t cur_song_bytes_read;  // bytes read so far in current wav file

	//////////////////
	// SD FUNCTIONS //
	//////////////////
	// mounts sd card, saves it in sd_path member var
	FRESULT sd_mount() {
		FRESULT res;
		extern uint8_t sd_is_sdhc(void);

		if (FATFS_LinkDriver(&SD_Driver, sd_path) != 0)
			return FR_DISK_ERR;

		DSTATUS stat = disk_initialize(0);
		if (stat != 0)
			return FR_NOT_READY;

		res = f_mount(&fs, sd_path, 1);
		return res;
	}

	// recursively finds all wav files on sd card and adds paths to wav_paths
	void get_files(const char *path, int depth) {
		DIR dir;
		FILINFO fno;

		//clear previous paths
		wav_paths.clear();

		// try to open directory at path
		if (f_opendir(&dir, path) != FR_OK)
			return;

		while (true) {
			// try to read next direntry
			if (f_readdir(&dir, &fno) != FR_OK || fno.fname[0] == 0)
				break;

			const char *name = fno.fname;

			// check if entry is directory or file
			if (fno.fattrib & AM_DIR) {
				// check that directory is not a special entry and call function again
				if (strcmp(name, ".") != 0 && strcmp(name, "..") != 0) {
					std::string newpath = std::string(path) + "/" + name;
					get_files(newpath.c_str(), depth + 1);
				}
			} else {
				// check if file has an extension and is a .wav file
				const char *ext = strrchr(name, '.');
				if (ext && strcasecmp(ext, ".wav") == 0) {
					std::string fullpath = std::string(path) + "/" + name;
					wav_paths.emplace_back(fullpath);
				}
			}
		}
		f_closedir(&dir);
	}

	///////////////////
	// WAV FUNCTIONS //
	///////////////////

	// resamples the input to be a 16 bit unsigned int with value from 0 to ARR
	uint16_t resample_CCR (int16_t s16) {

		uint32_t u16 = s16 + 32768;

		uint32_t t = (uint32_t)u16 * (uint32_t)(htim2_EN->Instance->ARR);
		uint16_t resampled = (uint16_t)(t >> 16);
		return resampled;
	}

	// Minimal WAV header skip: find "data" chunk and its size.
	uint32_t wav_seek_to_data () {
		typedef struct { char id[4]; uint32_t size; } chunk_t;
		uint8_t hdr[44];
		UINT br;

		// Read header (44 bytes): "RIFF", ..., "WAVE", ... "data", Subchunk2Size
		FRESULT fr = f_read(&file, hdr, sizeof(hdr), &br);
		if (fr != FR_OK || br != sizeof(hdr))
			printf("f_read failed with code: %d, or sd didn't read properly \r\n", fr);
		// file pointer now at start of PCM data

		// Check header to make sure it's a correcrt WAV file
		if (memcmp(hdr, "RIFF", 4) || memcmp(hdr+8, "WAVE", 4) || memcmp(hdr+36, "data", 4))
			printf("File read is not a wav file...?\r\n");

		// Get data chunk size
		// bytes 40-44 are Subchunk2Size = NumSamples * NumChannels * BitsPerSample/8, returns number of bytes of PCM data
		uint32_t Subchunk2Size = hdr[40] |
								(hdr[41] << 8) |
								(hdr[42] << 16) |
								(hdr[43] << 24);
		return Subchunk2Size;
	}

	// Fill a CCR buffer from the WAV file
	void fill_from_wav (uint16_t *dst) {
		UINT received = 0;

		// Read directly into dst buffer
		FRESULT fr = f_read(&file, (uint8_t*)dst, BUFFER_SIZE * sizeof(int16_t), &received);
		if (fr != FR_OK)
			printf("f_read failed with code: %d\r\n", fr);

		//increment samples read by number of bytes read (each sample is 2 bytes)
		cur_song_bytes_read += received;

		// Rescale in place
		uint32_t samples_read = received / sizeof(int16_t); //same as received >> 1
		for (uint32_t i = 0; i < samples_read; i++) {
			int16_t s16 = ((int16_t*)dst)[i];
			dst[i] = resample_CCR(s16);
		}

		// Pad remainder with zeros (i.e if end of file reached)
		for (uint32_t i = samples_read; i < BUFFER_SIZE; i++) {
			dst[i] = 0;
			if (continuous)
				next_requested = true;
		}
	}

	////////////////////////
	// PLAYBACK FUNCTIONS //
	////////////////////////

	//htim1 --> 40kHz square wave PWM generation to toggle DIR that triggers DMA to put buff[s] --> htim2_CCR
	//htim2 --> 80kHz modulated PWM
	void start_song() {
		//Seek to WAV data, set member variables
		cur_song_total_bytes = wav_seek_to_data();
		cur_song_bytes_read = 0;

		//Fill buffers and restart playback
		fill_from_wav(consumer_buf);
		fill_from_wav(producer_buf);

		//set DIR to be a 40kHz sq wave (ARR = 2999, CCR = 1499)
		htim1_DIR->Instance->CCR1 = 1499;

		//start the direction PWM timer (40kHz square wave)
		HAL_TIM_PWM_Start(htim1_DIR, TIM_CHANNEL_1);
		//start the EN PWM timer (80kHz modulated wave)
		HAL_TIM_PWM_Start(htim2_EN, TIM_CHANNEL_1);

		__HAL_TIM_ENABLE_DMA(htim1_DIR, TIM_DMA_UPDATE);

		HAL_DMA_RegisterCallback(hdma_ptr, HAL_DMA_XFER_CPLT_CB_ID, HAL_DMA_XferCpltCallback);
		HAL_DMA_Start_IT(hdma_ptr,
						(uint32_t)consumer_buf,
						(uint32_t)(&htim2_EN->Instance->CCR1),
						BUFFER_SIZE
					);

	}

	void stop_all() {
		HAL_DMA_Abort_IT(hdma_ptr);
		__HAL_TIM_DISABLE_DMA(htim1_DIR, TIM_DMA_UPDATE);
		HAL_TIM_PWM_Stop(htim1_DIR, TIM_CHANNEL_1);
		HAL_TIM_PWM_Stop(htim2_EN, TIM_CHANNEL_1);
	}

	void pause() {
		__HAL_TIM_DISABLE_DMA(htim1_DIR, TIM_DMA_UPDATE);
	}

	void play() {
		__HAL_TIM_ENABLE_DMA(htim1_DIR, TIM_DMA_UPDATE);
	}

public:
	SD() = default;
	//htim1 --> 40kHz square wave PWM generation to toggle DIR that triggers DMA to put buff[s] --> htim2_CCR
	//htim2 --> 80kHz modulated PWM
	void init (TIM_HandleTypeDef* htim1_in, TIM_HandleTypeDef* htim2_in, DMA_HandleTypeDef* hdma_in) {
		printf("Initializing system... \r\n");
		// set member variable values
		htim1_DIR = htim1_in;
		htim2_EN = htim2_in;
		hdma_ptr = hdma_in;
		need_refill = false;
		continuous = true;
		current_wav = 0;
		FRESULT fr;

		// mount the SD card
		printf("Mounting SD Card.. \r\n");
		fr = sd_mount();
		if (fr != FR_OK)
			printf("SD Mount Failed with code: %d\r\n", fr);

		// get all wav file paths
		printf("Reading in files from SD card... \r\n");
		get_files(sd_path, 0);
		filename = wav_paths[current_wav];

		// open the file
		fr = f_open(&file, filename.c_str(), FA_READ);
		if (fr != FR_OK)
			printf("f_open failed with code: %d\r\n", fr);

		printf("Now playing: %s\r\n", filename.c_str());
		//skip to PCM, fill buffers, start timer, PWM, start DMA
		start_song();
	}

	// this function gets called by main driver when a buffer is emptied
	void handle_dma_cb() {
		//swap buffers
		uint16_t* temp_buf = consumer_buf;
		consumer_buf = producer_buf;
		producer_buf = temp_buf;

		// launch next DMA on the new consumer_buf
		HAL_DMA_Start_IT(
			hdma_ptr,
			(uint32_t)consumer_buf,
			(uint32_t)&htim2_EN->Instance->CCR1,
			BUFFER_SIZE
		);

		//signal to refil the (now empty) producerbuffer
		need_refill = true;
	}

	// checks if the producer buffer needs to be refilled, called by main driver
	void check_prod() {
		if (need_refill) {
			need_refill = false;
			fill_from_wav(producer_buf);
		}
	}

	//check what the bext action should be (skip, pause, play), called by main driver
	void check_next() {
		if (next_requested) {
			next_requested = false;
			skip();
		}

		if (pause_requested) {
			pause_requested = false;
			pause();
		}

		if (play_requested) {
			play_requested = false;
			play();
		}
	}

	//sets flag to request a new song to be played, called by main driver
	void request_next() {
		next_requested = true;
	}

	//sets flag to request a song to be paused, called by main driver
	void request_pause() {
		pause_requested = true;
	}

	//sets flag to request a song to be played, called by main driver
	void request_play() {
		play_requested = true;
	}

	// returns progress of current song from 0.0 to 1.0
	float get_progress() {
		if (cur_song_total_bytes == 0)
			return 0.0f;
		return (float)cur_song_bytes_read / (float)cur_song_total_bytes;
	}

	// changes the filename to the next filename in the vector
	void skip() {
	    if (wav_paths.empty()) {
	        printf("End of playlist!\r\n");
	        continuous = false;
	        stop_all();
	        return;
	    }

	    // prevent check_prod() from using old file
	    need_refill = false;

	    // Close the current file
	    FRESULT fr = f_close(&file);
	    if (fr != FR_OK)
	        printf("f_close failed with code: %d\r\n", fr);

	    // Advance playlist
	    current_wav = (current_wav + 1) % wav_paths.size();
	    filename = wav_paths[current_wav];

	    //open new file
	    fr = f_open(&file, filename.c_str(), FA_READ);
	    if (fr != FR_OK) {
	        printf("f_open failed with code: %d\r\n", fr);
	        return;
	    }

		cur_song_total_bytes = wav_seek_to_data();
		cur_song_bytes_read = 0;

	    printf("Now playing: %s\r\n", filename.c_str());
	}

};

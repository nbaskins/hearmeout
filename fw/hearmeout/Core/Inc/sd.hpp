/*
 * Class for handling SD data transfers and DMA with CCR
 */

#include "fatfs.h"
#include "sd_diskio_spi.h"
#include "sd_spi.h"
#include "ff.h"
#include "ffconf.h"
#include "screen.hpp"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <vector>
#include <string>

#define BUFFER_SIZE 1024

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} Pixel;

#define ALBUM_W  152
#define ALBUM_H  150
#define ALBUM_READ_WIDTH 1

// BMP defines
#define BMP_HEADER_SIZE 54
#define BMP_HEADER_WIDTH_INDEX 18
#define BMP_HEADER_HEIGHT_INDEX 22
#define BMP_HEADER_BITCOUNT_INDEX 28


class SD {
private:
	FIL audioFile; 	// file being read from SD card
	FIL albumArt; 	// respective album art file
	FATFS fs; // FATFS filesystem object
	char sd_path[4]; // char array for storing sd path info
	std::string songName; // name of file being read from SD card
	uint16_t sd_buffer1[BUFFER_SIZE]; // buffer of SD data
	uint16_t sd_buffer2[BUFFER_SIZE]; // buffer of SD data
	Pixel albumArtRGB[ALBUM_W];  // your final RGB buffer
	
	uint16_t* consumer_buf;
	uint16_t* producer_buf;
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
	void (*song_finished_callback)();
	void (*song_duration_callback)(uint32_t, uint32_t, uint32_t);
	uint32_t total_song_length_bytes;
	uint32_t curr_song_length_bytes;

	// mounts sd card
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

	// resamples the input to be a 16 bit unsigned int with value from 0 to ARR
	uint16_t resample_CCR (int16_t s16) {

		uint32_t u16 = s16 + 32768;

		uint32_t t = (uint32_t)u16 * (uint32_t)(htim2_EN->Instance->ARR);
		uint16_t resampled = (uint16_t)(t >> 16);
		return resampled;
	}

	// Minimal WAV header skip: find "data" chunk and its size.
	FRESULT wav_seek_to_data (uint32_t *data_bytes_out) {
		typedef struct { char id[4]; uint32_t size; } chunk_t;
		uint8_t hdr[12];
		UINT br;

		// Read RIFF header (12 bytes): "RIFF", size, "WAVE"
		FRESULT fr = f_read(&audioFile, hdr, sizeof(hdr), &br);
		if (fr != FR_OK || br != sizeof(hdr))
			return FR_DISK_ERR;

		if (memcmp(hdr, "RIFF", 4) || memcmp(hdr+8, "WAVE", 4))
			return FR_INT_ERR;



		// Iterate chunks until we find "data"
		while (true) {
			chunk_t ck;
			fr = f_read(&audioFile, &ck, sizeof(ck), &br);
			if (fr != FR_OK || br != sizeof(ck))
				return FR_DISK_ERR;

			if (!memcmp(ck.id, "data", 4)) {
				if (data_bytes_out)
					*data_bytes_out = ck.size;
				total_song_length_bytes = ck.size;
				curr_song_length_bytes = 0;

				return FR_OK; // file pointer now at start of PCM data
			}

			// Skip this chunk
			fr = f_lseek(&audioFile, f_tell(&audioFile) + ck.size);
			if (fr != FR_OK) return fr;
		}
	}

	// Fill a CCR buffer from the WAV file
	void fill_from_wav (uint16_t *dst) {
		UINT received = 0;

		// Read directly into dst buffer
		FRESULT fr = f_read(&audioFile, (uint8_t*)dst, BUFFER_SIZE * sizeof(int16_t), &received);
		if (fr != FR_OK)
			printf("f_read failed with code: %d\r\n", fr);

		// Rescale in place
		uint32_t samples_read = received / sizeof(int16_t); //same as received >> 1
		for (uint32_t i = 0; i < samples_read; i++) {
			int16_t s16 = ((int16_t*)dst)[i];
			//printf("s16 Val: %d\r\n", s16);
			uint16_t resampled = resample_CCR(s16);
			dst[i] = resampled;
		}

		// Pad remainder with zeros (i.e if end of file reached)
		for (uint32_t i = samples_read; i < BUFFER_SIZE; i++) {
			dst[i] = 0;
			if (continuous)
				next_requested = true;
		}
		uint32_t prev_song_length_bytes = curr_song_length_bytes;
		curr_song_length_bytes += BUFFER_SIZE * sizeof(int16_t);

		song_duration_callback(curr_song_length_bytes, prev_song_length_bytes, total_song_length_bytes);
	}




public:
	SD() = default;
	//htim1 --> 40kHz square wave PWM generation to toggle DIR that triggers DMA to put buff[s] --> htim2_CCR
	//htim2 --> 80kHz modulated PWM

	void init (TIM_HandleTypeDef* htim1_in, TIM_HandleTypeDef* htim2_in, DMA_HandleTypeDef* hdma_in, void (*song_finished_callback_in)(), void (*song_duration_callback_in)(uint32_t, uint32_t, uint32_t)) {
//		printf("Initializing system... \r\n");
		// set member variable values
		htim1_DIR = htim1_in;
		htim2_EN = htim2_in;
		hdma_ptr = hdma_in;
		song_finished_callback = song_finished_callback_in;
		song_duration_callback = song_duration_callback_in;
		need_refill = false;
		continuous = true;
		current_wav = 0;

		consumer_buf = sd_buffer1; // pointer to buffer currently being consumed by CCR
		producer_buf = sd_buffer2; // pointer to buffer being filled from SD card
		FRESULT fr;

		// mount the SD card
//		printf("Mounting SD Card.. \r\n");
		fr = sd_mount();
		if (fr != FR_OK)
			printf("SD Mount Failed with code: %d\r\n", fr);
//		else
//			printf("SD Mounted!\n");

		wav_paths.clear();
		// get all wav file paths
//		printf("Reading in files from SD card... \r\n");
		get_files(sd_path, 0);
		songName = wav_paths[current_wav];

		song_finished_callback();

		// open the file
		fr = f_open(&audioFile, songName.c_str(), FA_READ);
		if (fr != FR_OK)
			printf("f_open failed with code: %d\r\n", fr);

//		printf("Now playing: %s\r\n", songName.c_str());
		//skip to PCM, fill buffers, start timer, PWM, start DMA
		start_song();
	}

	// this function gets called when a buffer is emptied
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

	void start_song() {
		 //Seek to WAV data
		uint32_t data_bytes = 0;
		FRESULT fr = wav_seek_to_data(&data_bytes);
		if (fr != FR_OK) {
			printf("wav_seek_to_data failed with code: %d\r\n", fr);
		}

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

	// checks if the producer buffer needs to be refilled, called by main driver
	void check_prod() {
		if (need_refill) {
			need_refill = false;
			fill_from_wav(producer_buf);
		}
	}

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

	void request_next() {
		next_requested = true;
	}

	void request_pause() {
		pause_requested = true;
	}

	void request_play() {
		play_requested = true;
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

	    //NEW SKIPPING: STOP PLAYBACK SO NO GLITCH
	    stop_all();

	    // Close the current file
	    FRESULT fr = f_close(&audioFile);
	    if (fr != FR_OK)
	        printf("f_close failed with code: %d\r\n", fr);

	    // Advance playlist
	    current_wav = (current_wav + 1) % wav_paths.size();
	    songName = wav_paths[current_wav];

	    //open new file
	    fr = f_open(&audioFile, songName.c_str(), FA_READ);
	    if (fr != FR_OK) {
	        printf("f_open failed with code: %d\r\n", fr);
	        return;
	    }

	    start_song();

//	    printf("Now playing: %s\r\n", songName.c_str());

	    song_finished_callback();
	}

	std::string get_song_name(){
		return songName.substr(0, songName.find_last_of('.'));
	}

	//gets respective album art for current song
	void display_image(std::string art_path, uint16_t x, uint16_t y, uint16_t w, uint16_t h, Screen* screen) {
		FRESULT fr = f_open(&albumArt, art_path.c_str(), FA_READ);
		if (fr != FR_OK) printf("f_open failed with code: %d\r\n", fr);

		// 14 byte header + 40-byte DIB header = 54
		UINT br;
		uint8_t header[BMP_HEADER_SIZE];
		fr = f_read(&albumArt, header, sizeof(header), &br);
		if (fr != FR_OK) printf("f_read failed with code: %d\r\n", fr);

		// decode the header
		int album_cover_width = *reinterpret_cast<int*>(&header[BMP_HEADER_WIDTH_INDEX]);
		int album_cover_height = *reinterpret_cast<int*>(&header[BMP_HEADER_HEIGHT_INDEX]);
		int bitcount = *reinterpret_cast<int*>(&header[BMP_HEADER_BITCOUNT_INDEX]);

		if(album_cover_width != ALBUM_W || album_cover_height != ALBUM_H){
			printf("Album cover image is wrong size. Expected width=%d and height=%d Received width=%d and height=%d\r\n", ALBUM_W, ALBUM_H, album_cover_width, album_cover_height);
			return;
		}

		if(bitcount != 24){ // use pattern table
			/*
			TODO: Redo this logic
			//read BMP palette (maps 256 colors to RGB)
			uint8_t palette[256 * 4];
			fr = f_read(&albumArt, palette, sizeof(palette), &br);
			if (fr != FR_OK) printf("f_read failed with code: %d\r\n", fr);

			// Need to convert from [0-255] indexed color to RGB = [0-7][0-7][0-7]
			// Can read in row by row and convert
			uint8_t row[ALBUM_W];
			for (int y = 0; y < ALBUM_H; ++y) {
				fr = f_read(&albumArt, row, ALBUM_W, &br);
				if (fr != FR_OK) printf("f_read failed with code: %d\r\n", fr);

				// BMP rows are stored bottom-up. Flip vertically into buffer
				int destY = ALBUM_H - 1 - y;
				for (int x = 0; x < ALBUM_W; ++x) {
					uint8_t idx = row[x];
					// palette entry: [B][G][R][0x00]
					uint8_t b = palette[4 * idx + 0];
					uint8_t g = palette[4 * idx + 1];
					uint8_t r = palette[4 * idx + 2];

					albumArtRGB[destY][x].r = r;
					albumArtRGB[destY][x].g = g;
					albumArtRGB[destY][x].b = b;
				}
			}
			*/
		}else{
//			printf("Decoding 24 bit BMP file...\r\n");
			for(int row = album_cover_height - 1; row >= 0; row -= ALBUM_READ_WIDTH){
				screen->draw_image_init(x, y + row, ALBUM_W, ALBUM_READ_WIDTH);
				Pixel row_buffer_bgr[ALBUM_READ_WIDTH * ALBUM_W];
				Pixel row_buffer_rgb[ALBUM_READ_WIDTH * ALBUM_W];
				fr = f_read(&albumArt, reinterpret_cast<uint8_t*>(row_buffer_bgr), sizeof(row_buffer_bgr), &br);

				for(int col = 0; col < ALBUM_READ_WIDTH * album_cover_width; ++col){
					row_buffer_rgb[col].r = row_buffer_bgr[col].b;
					row_buffer_rgb[col].g = row_buffer_bgr[col].g;
					row_buffer_rgb[col].b = row_buffer_bgr[col].r;
				}
				screen->draw_image_row(reinterpret_cast<uint8_t*>(row_buffer_rgb), sizeof(row_buffer_rgb));
			}
		}
		f_close(&albumArt);
	}
};

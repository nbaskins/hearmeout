/*
 * Class for handling SD data transfers and DMA with CCR
 */

#include "sd_functions.h"

#define BUFFER_SIZE 2048

class SD {
private:
	FIL file; // file being read from SD card
	const char* filename; // name of file being read from SD card
	uint16_t sd_buffer1[BUFFER_SIZE]; // buffer of SD data
	uint16_t sd_buffer2[BUFFER_SIZE]; // buffer of SD data
	uint16_t* consumer_buf = sd_buffer1; // pointer to buffer currently being consumed by CCR
	uint16_t* producer_buf = sd_buffer2; // pointer to buffer being filled from SD card
	TIM_HandleTypeDef* htim_ptr; // pointer to timer handle for transducers
	DMA_HandleTypeDef* hdma_ptr; // pointer to dma handle for tim up
	bool need_refill; // bool that represents if a refill is needed for producer_buf

	// resamples the input to be a 16 bit unsigned int with value from 0 to ARR
	uint16_t resample_CCR (int16_t s16) {
		uint16_t u16 = s16 + 32768u;
		uint32_t t = (uint32_t)u16 * (uint32_t)(htim_ptr->Instance->ARR + 1) / 2;
		uint16_t resampled = (uint16_t)(t >> 16) + 200;
		return resampled;
	}

	// Minimal WAV header skip: find "data" chunk and its size.
	FRESULT wav_seek_to_data (uint32_t *data_bytes_out) {
		typedef struct { char id[4]; uint32_t size; } chunk_t;
		uint8_t hdr[12];
		UINT br;

		// Read RIFF header (12 bytes): "RIFF", size, "WAVE"
		FRESULT fr = f_read(&file, hdr, sizeof(hdr), &br);
		if (fr != FR_OK || br != sizeof(hdr))
			return FR_DISK_ERR;

		if (memcmp(hdr, "RIFF", 4) || memcmp(hdr+8, "WAVE", 4))
			return FR_INT_ERR;

		// Iterate chunks until we find "data"
		while (true) {
			chunk_t ck;
			fr = f_read(&file, &ck, sizeof(ck), &br);
			if (fr != FR_OK || br != sizeof(ck))
				return FR_DISK_ERR;

			if (!memcmp(ck.id, "data", 4)) {
				if (data_bytes_out)
					*data_bytes_out = ck.size;

				return FR_OK; // file pointer now at start of PCM data
			}

			// Skip this chunk
			fr = f_lseek(&file, f_tell(&file) + ck.size);
			if (fr != FR_OK) return fr;
		}
	}

	// Fill a CCR buffer from the WAV file
	void fill_from_wav (uint16_t *dst) {
		UINT received = 0;

		// Read directly into dst buffer
		FRESULT fr = f_read(&file, (uint8_t*)dst, BUFFER_SIZE * sizeof(int16_t), &received);
		if (fr != FR_OK)
			printf("f_read failed with code: %d\r\n", fr);

		// Rescale in place
		uint32_t samples_read = received / sizeof(int16_t); //same as received >> 1
		for (uint32_t i = 0; i < samples_read; i++) {
			int16_t s16 = ((int16_t*)dst)[i];
			dst[i] = resample_CCR(s16);
		}

		// Pad remainder with zeros (i.e if end of file reached)
		for (uint32_t i = samples_read; i < BUFFER_SIZE; i++)
			dst[i] = 0;
	}

public:
	SD() = default;

	void init(const char* filename_in, TIM_HandleTypeDef* htim_in, DMA_HandleTypeDef* hdma_in) {
		// set member variable values
		filename = filename_in;
		htim_ptr = htim_in;
		hdma_ptr = hdma_in;

		// mount the SD card
		sd_mount();

		// open the file
		FRESULT fr = f_open(&file, filename, FA_READ);
		if (fr != FR_OK)
			printf("f_open failed with code: %d\r\n", fr);
		else
			printf("f_open opened file %s\n", filename);

		uint32_t data_bytes = 0;
		fr = wav_seek_to_data(&data_bytes);
		if (fr != FR_OK)
			printf("wav_seek_to_data failed with code: %d\r\n", fr);

		// fill consumer and producer buffers
		fill_from_wav(consumer_buf);
		fill_from_wav(producer_buf);

		// start tim PWM
		HAL_TIM_PWM_Start(htim_ptr, TIM_CHANNEL_1);
		HAL_TIMEx_PWMN_Start(htim_ptr, TIM_CHANNEL_1);

		// initialize DMA
		__HAL_TIM_ENABLE_DMA(htim_ptr, TIM_DMA_UPDATE);
		HAL_DMA_RegisterCallback(hdma_ptr, HAL_DMA_XFER_CPLT_CB_ID, HAL_DMA_XferCpltCallback);
		HAL_DMA_Start_IT(hdma_ptr, (uint32_t)consumer_buf, (uint32_t)&htim_ptr->Instance->CCR1, BUFFER_SIZE);
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
			(uint32_t)&htim_ptr->Instance->CCR1,
			BUFFER_SIZE
		);

		//signal to refil the (now empty) producerbuffer
		need_refill = true;
	}

	// checks if the producer buffer needs to be refilled, called by main driver
	void check_prod() {
		if (need_refill) {
			fill_from_wav(producer_buf);
			need_refill = false;
		}
	}
};

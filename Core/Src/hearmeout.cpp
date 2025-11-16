#include <cstdint>
#include <string>
 #include <cstring>
#include "main.h"
#include "fatfs.h"
#include "sd_functions.h"
//#include "transducer.hpp"
//#include "sd.hpp"

extern TIM_HandleTypeDef htim1;

#define BUFFER_SIZE 2048 //at 40kHz: 2048 samples = 51.2ms per sd card transfer. Is this too fast??
#define CSPORT GPIOC
#define CSPIN GPIO_PIN_5
volatile bool need_refill;

extern TIM_HandleTypeDef* htim1;
extern SPI_HandleTypeDef* hspi1;
extern DMA_HandleTypeDef* hdma_tim1_up;

uint16_t sd_buffer1[BUFFER_SIZE];
uint16_t sd_buffer2[BUFFER_SIZE];
uint16_t* consumer_buf = sd_buffer1; // set sd_buffer1 to be the first buffer to be consumed
uint16_t* producer_buf = sd_buffer2; // set sd_buffer2 to be the first buffer to be populated
const char* filename = "433.wav"; // current file name

FIL file;
FRESULT fr;
UINT br;

//Transducer t; // transducer object, gets instantiated in init
//SD sd; // sd card object, is instantiated in init

//input: 16 bit signed --> 16 bit unsigned [0-65535] --> [0-ARR]
uint16_t resample_CCR(int16_t s16) {
    uint16_t u16 = s16 + 32768u;             
    uint32_t t = (uint32_t)u16 * (uint32_t)(htim1->Init->ARR);
    return (uint16_t)(t >> 16);
}

// Minimal WAV header skip: find "data" chunk and its size.
FRESULT wav_seek_to_data(uint32_t *data_bytes_out)
{
    typedef struct { char id[4]; uint32_t size; } chunk_t;
    uint8_t  hdr[12];
    UINT     br;

    // Read RIFF header (12 bytes): "RIFF", size, "WAVE"
    fr = f_read(&file, hdr, sizeof(hdr), &br);
    if (fr != FR_OK || br != sizeof(hdr)) return FR_DISK_ERR;
    if (memcmp(hdr, "RIFF", 4) || memcmp(hdr+8, "WAVE", 4)) return FR_INT_ERR;

    // Iterate chunks until we find "data"
    for (;;) {
        chunk_t ck;
        fr = f_read(&file, &ck, sizeof(ck), &br);
        if (fr != FR_OK || br != sizeof(ck)) return FR_DISK_ERR;
        if (!memcmp(ck.id, "data", 4)) {
            if (data_bytes_out) *data_bytes_out = ck.size;
            return FR_OK; // file pointer now at start of PCM data
        }
        // Skip this chunk
        fr = f_lseek(&file, f_tell(&file) + ck.size);
        if (fr != FR_OK) return fr;
    }
}

// Fill a CCR buffer from the WAV file
void fill_from_wav(uint16_t *dst) {
    UINT received = 0;
    // Read directly into dst buffer
    fr = f_read(&file, (uint8_t*)dst, BUFFER_SIZE * sizeof(int16_t), &received);
    if (fr != FR_OK) { printf("f_read failed with code: %d\r\n", fr); /*error handling?*/ }
    
    // Rescale in place
    int samples_read = received / sizeof(int16_t); //same as received >> 1
    for (int i = 0; i < samples_read; i++) {
        int16_t s16 = ((int16_t*)dst)[i];
        dst[i] = resample_CCR(s16);
    }
    // Pad remainder with zeros (i.e if end of file reached)
    for (int i = samples_read; i < BUFFER_SIZE; i++) { dst[i] = 0; }
}


// initialize program and start event_loop
void init() {
    need_refill = false;

	//mount the SD card
	sd_mount();

	//open the file
	fr = f_open(&file, filename, FA_READ);
	if (fr != FR_OK) { printf("f_open failed with code: %d\r\n", fr); /*some other error handling?*/}

	uint32_t data_bytes = 0;
	fr = wav_seek_to_data(&data_bytes);
	if (fr != FR_OK) { printf("wav_seek_to_data failed with code: %d\r\n", fr); /*some other error handling?*/}

	//fill consumer buffer for the first time
	//producer buffer already populated with new data when it's needed the first time
	fill_from_wav(consumer_buf);
	fill_from_wav(producer_buf);

	// initialize DMA
	HAL_DMA_Start_IT(
		hdma_tim1_up,
		(uint32_t)consumer_buf,
		(uint32_t)&htim1->Instance->CCR1,
		BUFFER_SIZE
	);
	__HAL_TIM_ENABLE_DMA(htim1, TIM_DMA_UPDATE);
	HAL_TIM_PWM_Start(htim1, TIM_CHANNEL_1);

	event_loop();
}

// main loop
void event_loop() {
	while (true) {
        if (need_refill) {
            fill_from_wav(producer_buf);
            need_refill = false;
        }
    }
}

// HAL C functions
extern "C" {

void HAL_DMA_XferCpltCallback(DMA_HandleTypeDef *hdma) {
	//if (hdma == htim1.hdma[TIM_DMA_ID_UPDATE]) {
	if(hdma == hdma_tim1_up) {
    	//swap buffers
        std::swap(consumer_buf, producer_buf);

        // launch next DMA on the new consumer_buf
        HAL_DMA_Start_IT(hdma_tim1_up,
                        (uint32_t)consumer_buf,
                        (uint32_t)&htim1->Instance->CCR1,
                        BUFFER_SIZE);

        //signal to refil the (now empty) producerbuffer 
        need_refill = true;
    }
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{

}

// this callback is hit when the transducer sampling timer ccr value is reached
void HAL_TIM_OC_DelayElapsedCallback(TIM_HandleTypeDef *htim) {

}

// called by main.h allows for C++ projects
void HAL_PostInit() {
    init();
}

}

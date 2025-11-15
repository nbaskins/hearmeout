#include <string>
#include "main.h"
#include "fatfs.h"
#include "sd_functions.h"
//#include "transducer.hpp"
//#include "sd.hpp"

extern TIM_HandleTypeDef htim1;

#define BUFFER_SIZE 128
#define CSPORT GPIOC
#define CSPIN GPIO_PIN_5
int ARR;

extern TIM_HandleTypeDef* htim1;
extern SPI_HandleTypeDef* hspi1;
extern DMA_HandleTypeDef* hdma_tim1_up;

uint16_t current_sample = 0; // current sample index
uint16_t sd_buffer1[BUFFER_SIZE]; // buffer1 for bytes read from sd card
uint16_t sd_buffer2[BUFFER_SIZE]; // buffer2 for bytes read from sd card
uint16_t* producer_buf = sd_buffer1; // set sd_buffer1 to be the first buffer to be populated
uint16_t* consumer_buf = sd_buffer2; // set sd_buffer1 to be the first buffer to be populated
std::string filename = "433.wav"; // current file name

FIL file;
FRESULT fr;
UINT br;

//Transducer t; // transducer object, gets instantiated in init
//SD sd; // sd card object, is instantiated in init

// main loop
void event_loop() {
	while (true) {}
}

//input: 16 b signed --> 16 bit unsigned (0-65535) --> (0-ARR)
int resample_CCR(int input) {
    // Multiply first in 32-bit, add 0x8000 for rounding, then shift
    // Using ARR+1 with >>1
    int16_t s16 = (int16_t)(chunk[i] | (chunk[i+1] << 8));  // signed PCM sample
    uint16_t u16 = (uint16_t)(s16 + 32768);                 // shift to unsigned 0–65535
    uint32_t t = (uint32_t)x * (uint32_t)(ARR + 1);
    return (uint16_t)((t + 0x8000u) >> 16); //[0, ARR]
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
        fr = f_lseek(fp, f_tell(fp) + ck.size);
        if (fr != FR_OK) return fr;
    }
}

// Fill a CCR buffer from the WAV file
void fill_from_wav(uint16_t *dst){
    uint16_t chunk[BUFFER_SIZE];
    UINT got = 0;

    fr = f_read(&file, chunk, sizeof(chunk), &got);

    if (fr != FR_OK || got == 0) {	// EOF or error: pad zeros
        int produced = 0;
		while (produced < BUFFER_SIZE) dst[produced++] = 0;
		break;
	}
    //fill destination buffer with rescaled values
	for (int i = 0; i < BUFFER_SIZE; i++) {
		dst[i] = resample_CCR(chunk[i]);
	}
}


// initialize program and start event_loop
void init() {
	ARR = __HAL_TIM_GET_AUTORELOAD(htim);

	//mount the SD card
	sd_mount();

	//open the file
	fr = f_open(&file, filename, FA_READ);
	if (fr != FR_OK) { printf("f_open failed with code: %d\r\n", fr); /*some other error handling?*/}

	uint32_t data_bytes = 0;
	fr = wav_seek_to_data(&data_bytes);
	if (fr != FR_OK) { printf("f_open failed with code: %d\r\n", fr); /*some other error handling?*/}

	//fill consumer buffer for the first time
	//producer buffer already populated with new data
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

// HAL C functions
extern "C" {

void HAL_DMA_XferCpltCallback(DMA_HandleTypeDef *hdma) {
	//if (hdma == htim1.hdma[TIM_DMA_ID_UPDATE]) {
	if(hdma == hdma_tim1_up) {
    	//swap buffers
        uint16_t *just_played = (uint16_t*)consumer_buf;
        consumer_buf = producer_buf;
        producer_buf = just_played;

        // launch next DMA on the new consumer_buf
        HAL_DMA_Start_IT(hdma_tim1_up,
                        (uint32_t)consumer_buf,
                        (uint32_t)&htim1->Instance->CCR1,
                        BUFFER_SIZE);

        //fill producer buffer
        fill_from_wav(producer_buf);
    }
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
//    if (htim->Instance == TIM1) {
//        // Called every 25 µs (40 kHz)
//        // Fetch next PCM sample, map, update CCR
//        uint16_t s = current_buf[current_sample++];
//        uint16_t ccr = resample_CCR(s);
//        htim->Instance->CCR1 = ccr;
//    }
}

// this callback is hit when the transducer sampling timer ccr value is reached
void HAL_TIM_OC_DelayElapsedCallback(TIM_HandleTypeDef *htim) {
//    if(htim == htim1) {
//    	t.play_sound(audio_samples[current_sample]);
//    	current_sample = (current_sample + 1) % NUM_SAMPLES;
//    }
}

// called by main.h allows for C++ projects
void HAL_PostInit() {
    init();
}

}

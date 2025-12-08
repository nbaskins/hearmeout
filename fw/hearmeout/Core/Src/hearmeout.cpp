#include <stdbool.h>
#include <string.h>
#include <cstdio>
#include <cstdlib>
#include "main.h"
#include "fatfs.h"
#include "sd.hpp"
#include "screen.hpp"
#include "palette.hpp"
#include "gimbal.hpp"
#include "audio_jack.hpp"

extern TIM_HandleTypeDef htim1;
extern TIM_HandleTypeDef htim2;
extern TIM_HandleTypeDef htim3;
extern TIM_HandleTypeDef htim4;
extern TIM_HandleTypeDef htim5;
extern UART_HandleTypeDef huart2;
extern DMA_HandleTypeDef hdma_tim1_up;
extern SPI_HandleTypeDef hspi2;
extern SPI_HandleTypeDef hspi3;
extern ADC_HandleTypeDef hadc1;
extern OPAMP_HandleTypeDef hopamp2;
extern I2C_HandleTypeDef hi2c1;

SD sd; // sd object used to handle updating CCR based on audio file
Screen screen;
AudioJack jack;
Gimbal gimbal; // need to update cam class to have default constuctor with init function
volatile bool send_req = false;

enum STATE : uint8_t{
	SD_CARD = 0,
	AUDIO_JACK = 1
};

STATE state;
STATE prev_state;

// BEGIN //

void pause_callback(){
	printf("Pause\r\n");
	if(state == STATE::SD_CARD){
		sd.request_pause();
	}else if(state == STATE::AUDIO_JACK){
		jack.request_pause();
	}
}

void play_callback(){
	printf("Play\r\n");
	if(state == STATE::SD_CARD){
		sd.request_play();
	}else if(state == STATE::AUDIO_JACK){
		jack.request_play();
	}
}

void skip_callback(){
	printf("Skip\r\n");
	sd.request_next();
}

void input_callback(){
	printf("Input\r\n");
	state = (state == STATE::SD_CARD) ? STATE::AUDIO_JACK : STATE::SD_CARD;
}

void render_sd_gui(){
	screen.clear();

	screen.draw_button(15, 15, 200, 90, &pause_callback, "PAUSE");

	screen.draw_button(15, 115, 200, 90, &play_callback, "PLAY");

	screen.draw_button(15, 215, 200, 90, &skip_callback, "SKIP");

	screen.draw_button(230, 245, 235, 60, &input_callback, "AUX");
}

void render_jack_gui(){
	screen.clear();
	screen.draw_button(15, 15, 200, 90, &pause_callback, "MUTE");

	screen.draw_button(15, 115, 200, 90, &play_callback, "UNMUTE");

	screen.draw_button(230, 245, 235, 60, &input_callback, "SD CARD");
}

// main loop
void event_loop() {
	while (true) {
		if (send_req) {
			send_req = false;
			gimbal.request_pos();
		}

		gimbal.poll_dma();

		// pause one of either the SD card or the Audio Jack in order to prevent them from both
		if(state == STATE::SD_CARD){
			jack.pause();
		}else if(state == STATE::AUDIO_JACK){
			sd.pause();
		}

		// change the UI to the corresponding input
		if(state != prev_state){
			// pausing both the audio jack and the sd card will prevent any unwanted noising while re-rendering the screen
			jack.pause();
			sd.pause();
			if(state == STATE::SD_CARD){
				render_sd_gui();
				sd.display_image(sd.get_song_name() + ".bmp", 272, 36, 152, 150, &screen);
				sd.request_play();
				// init the LED
				uint8_t start_sd_card_led[] = {0x21};
				uint8_t stop_aux_led[] = {0x30};
				HAL_I2C_Master_Transmit(&hi2c1, (69 << 1), start_sd_card_led, sizeof(start_sd_card_led), (uint32_t)HAL_Delay);
				HAL_I2C_Master_Transmit(&hi2c1, (69 << 1), stop_aux_led, sizeof(stop_aux_led), (uint32_t)HAL_Delay);
			}else if(state == STATE::AUDIO_JACK){
				render_jack_gui();
				sd.display_image("0://aux.bmp", 272, 66, 152, 150, &screen);
				jack.request_play();
				// init the LED
				uint8_t start_sd_card_led[] = {0x20};
				uint8_t stop_aux_led[] = {0x31};
				HAL_I2C_Master_Transmit(&hi2c1, (69 << 1), start_sd_card_led, sizeof(start_sd_card_led), (uint32_t)HAL_Delay);
				HAL_I2C_Master_Transmit(&hi2c1, (69 << 1), stop_aux_led, sizeof(stop_aux_led), (uint32_t)HAL_Delay);
			}
		}
		prev_state = state;

		// run the event loop for one of the the SD Card or the Audio Jack
		if(state == STATE::SD_CARD){
			sd.check_prod();
			sd.check_next();
		}else if(state == STATE::AUDIO_JACK){
			jack.check_next();
			// No Events for the audio jack
		}
    }
}

void song_finished_callback(){
	printf("Song Finished\r\n");
	sd.display_image(sd.get_song_name() + ".bmp", 272, 36, 152, 150, &screen);
}

void song_duration_callback(uint32_t current_song_duration, uint32_t prev_song_duration, uint32_t total_song_duration){
	int pixel_percent = (int)(((float)current_song_duration/(float)total_song_duration) * 152);
	screen.draw_box(272 + pixel_percent, 220, 152 - pixel_percent, 10, PROGRESS_BAR_BACKGROUND_R, PROGRESS_BAR_BACKGROUND_G, PROGRESS_BAR_BACKGROUND_B);
	screen.draw_box(272, 220, pixel_percent, 10, PROGRESS_BAR_FOREGROUND_R, PROGRESS_BAR_FOREGROUND_G, PROGRESS_BAR_FOREGROUND_B);
}

// initialize program and start event_loop
void init() {
	gimbal.init(&htim4, &htim5, &huart2);
	gimbal.request_pos();

	jack.init(&hadc1, &hopamp2);
	screen.init(&hspi3, &hspi2, &htim3);

	// we default to playing from the SD_Card
	state = STATE::SD_CARD;
	prev_state = STATE::SD_CARD;
	render_sd_gui();

	sd.init(&htim1, &htim2, &hdma_tim1_up, &song_finished_callback, &song_duration_callback);

	// init the LED
	uint8_t start_status_led[] = {0x11};
	uint8_t clear_sd_card_led[] = {0x21};
	uint8_t clear_aux_led[] = {0x30};
	HAL_I2C_Master_Transmit(&hi2c1, (69 << 1), start_status_led, sizeof(start_status_led), (uint32_t)HAL_Delay);
	HAL_I2C_Master_Transmit(&hi2c1, (69 << 1), clear_sd_card_led, sizeof(clear_sd_card_led), (uint32_t)HAL_Delay);
	HAL_I2C_Master_Transmit(&hi2c1, (69 << 1), clear_aux_led, sizeof(clear_aux_led), (uint32_t)HAL_Delay);

	event_loop();
}

// HAL C functions
extern "C" {

// DMA callback when buffer is emptied
void HAL_DMA_XferCpltCallback (DMA_HandleTypeDef *hdma) {
	if(hdma == &hdma_tim1_up)
		sd.handle_dma_cb();
}

// callback to determine when the ADC read completes
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc){
	if(hadc == &hadc1){
		uint32_t adc_val = HAL_ADC_GetValue(hadc);
		uint32_t ccr = (static_cast<float>(adc_val) / static_cast<float>(MAX_ADC_VAL)) * htim2.Instance->ARR;
		htim2.Instance->CCR1 = ccr;
	}
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
	if (htim == &htim3) {
		static float prev_z = 0;
		static constexpr float alpha = 0.3;

		// frame rate controls
		static constexpr unsigned long TICKS_PER_FRAME = 500;
		static unsigned long last_interrupt_time = 0;
		unsigned long interrupt_time = HAL_GetTick();

		uint16_t touch_x;
		uint16_t touch_y;
		uint16_t touch_z;

		screen.sample_x_y(&touch_x, &touch_y, &touch_z);
		prev_z = alpha * touch_z + (1 - alpha) * prev_z;

		printf("Sampled %u %u %u\r\n", touch_x, touch_y, touch_z);

		screen.check_buttons(touch_x, touch_y, prev_z);
		last_interrupt_time = interrupt_time;
	} else if (htim == &htim5) {
		send_req = true;
	}
}

// called by main.h allows for C++ projects
void HAL_PostInit() {
    init();
}

}

#include <stdbool.h>
#include <string.h>
#include <cstdio>
#include <cstdlib>
#include "main.h"
#include "fatfs.h"
#include "sd.hpp"
#include "screen.hpp"
#include "palette.hpp"

extern TIM_HandleTypeDef htim1;
extern TIM_HandleTypeDef htim2;
extern TIM_HandleTypeDef htim3;
extern DMA_HandleTypeDef hdma_tim1_up;
extern SPI_HandleTypeDef hspi2;
extern SPI_HandleTypeDef hspi3;

SD sd; // sd object used to handle updating CCR based on audio file
Screen screen;

// BEGIN //

// main loop
void event_loop() {
	while (true) {
        sd.check_prod();
        sd.check_next();
        //printf("CCR: %d\r\n", (int16_t)(htim2.Instance->CCR1));
    }
}

void pause_callback(){
	printf("Pause\r\n");
	sd.request_pause();
}

void play_callback(){
	printf("Play\r\n");
	sd.request_play();
}

void skip_callback(){
	printf("Skip\r\n");
	sd.request_next();
}

void song_finished_callback(){
	printf("Song Finished\r\n");
	sd.display_album_cover(272, 66, 152, 150, &screen);
}

void song_duration_callback(uint32_t current_song_duration, uint32_t prev_song_duration, uint32_t total_song_duration){
	int pixel_percent = (int)(((float)current_song_duration/(float)total_song_duration) * 152);
	screen.draw_box(272 + pixel_percent, 250, 152 - pixel_percent, 10, PROGRESS_BAR_BACKGROUND_R, PROGRESS_BAR_BACKGROUND_G, PROGRESS_BAR_BACKGROUND_B);
	screen.draw_box(272, 250, pixel_percent, 10, PROGRESS_BAR_FOREGROUND_R, PROGRESS_BAR_FOREGROUND_G, PROGRESS_BAR_FOREGROUND_B);
}

// initialize program and start event_loop
void init() {
	screen.init(&hspi3, &hspi2);
	sd.init(&htim1, &htim2, &hdma_tim1_up, &song_finished_callback, &song_duration_callback);
	screen.draw_button(15, 15, 200, 90, &pause_callback, "PAUSE");

	screen.draw_button(15, 115, 200, 90, &play_callback, "PLAY");

	screen.draw_button(15, 215, 200, 90, &skip_callback, "SKIP");

	HAL_TIM_Base_Start_IT(&htim3);

	event_loop();
}

// HAL C functions
extern "C" {

// DMA callback when buffer is emptied
void HAL_DMA_XferCpltCallback (DMA_HandleTypeDef *hdma) {
	if(hdma == &hdma_tim1_up)
		sd.handle_dma_cb();
}

// callback for button press
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
	if (GPIO_Pin == GPIO_PIN_7) {
		static unsigned long last_interrupt_time = 0;
	    static bool is_pause = true;
		unsigned long interrupt_time = HAL_GetTick();

		if (interrupt_time - last_interrupt_time > 500) { // debounce
//			uint16_t touch_x;
//			uint16_t touch_y;
//			uint16_t touch_z;
//			screen.sample_x_y(&touch_x, &touch_y, &touch_z);
//			printf("Sampled %u %u\r\n", touch_x, touch_y);
//			screen.check_buttons(touch_x, touch_y);

			last_interrupt_time = interrupt_time;
		}
	}
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (htim == &htim3) {
	static float prev_z = 0;
	static constexpr float alpha = 0.15;

	// frame rate controls
	static constexpr unsigned long TICKS_PER_FRAME = 500;
	static unsigned long last_interrupt_time = 0;
	unsigned long interrupt_time = HAL_GetTick();

	uint16_t touch_x;
	uint16_t touch_y;
	uint16_t touch_z;
	screen.sample_x_y(&touch_x, &touch_y, &touch_z);
	prev_z = alpha * touch_z + (1 - alpha) * prev_z;
	if(interrupt_time - last_interrupt_time > TICKS_PER_FRAME){
		printf("Sampled %u %u %u\r\n", touch_x, touch_y, static_cast<uint16_t>(prev_z));
		screen.check_buttons(touch_x, touch_y, prev_z);
		last_interrupt_time = interrupt_time;
	}
  }
}

// called by main.h allows for C++ projects
void HAL_PostInit() {
    init();
}

}

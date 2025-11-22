#include <stdbool.h>
#include <string.h>
#include <cstdio>
#include "main.h"
#include "fatfs.h"
#include "sd.hpp"

extern TIM_HandleTypeDef htim1;
extern TIM_HandleTypeDef htim2;
extern DMA_HandleTypeDef hdma_tim1_up;

SD sd; // sd object used to handle updating CCR based on audio file

// main loop
void event_loop() {
	while (true) {
        sd.check_prod();
        sd.check_next();
        //printf("CCR: %d\r\n", (int16_t)(htim2.Instance->CCR1));
    }
}

// initialize program and start event_loop
void init() {
	sd.init(&htim1, &htim2, &hdma_tim1_up);
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
    if (GPIO_Pin == GPIO_PIN_13) {
        static uint32_t last_press = 0;
        static constexpr uint32_t DEBOUNCE_MS = 500;
        static bool is_pause = true;
        uint32_t now = HAL_GetTick();

        // if 50 ms has passed since last press, set next song
        if (now - last_press >= DEBOUNCE_MS) {
            last_press = now;
//            sd.request_next();
            if(is_pause){
            	sd.request_pause();
            }else{
            	sd.request_play();
            }
            is_pause = !is_pause;
        }


    }
}

// called by main.h allows for C++ projects
void HAL_PostInit() {
    init();
}

}

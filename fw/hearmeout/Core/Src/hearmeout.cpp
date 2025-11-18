#include <stdbool.h>
#include <string.h>
#include <cstdio>
#include "main.h"
#include "fatfs.h"
#include "sd.hpp"

extern TIM_HandleTypeDef htim1;
extern DMA_HandleTypeDef hdma_tim1_up;

const char* filename = "433.wav"; // name of audio file to update CCR
SD sd; // sd object used to handle updating CCR based on audio file

// main loop
void event_loop() {
	while (true) {
        sd.check_prod();
    }
}

// initialize program and start event_loop
void init() {
	sd.init(filename, &htim1, &hdma_tim1_up);
	event_loop();
}

// HAL C functions
extern "C" {

void HAL_DMA_XferCpltCallback(DMA_HandleTypeDef *hdma) {
	if(hdma == &hdma_tim1_up)
		sd.handle_dma_cb();
}

// called by main.h allows for C++ projects
void HAL_PostInit() {
    init();
}

}

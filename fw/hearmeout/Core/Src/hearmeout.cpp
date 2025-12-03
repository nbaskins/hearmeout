#include <gimbal.hpp>
#include <stdbool.h>
#include <string.h>
#include <cstdio>
#include "main.h"
#include "fatfs.h"
#include "sd.hpp"

extern TIM_HandleTypeDef htim1;
extern TIM_HandleTypeDef htim4;
extern TIM_HandleTypeDef htim5;
extern DMA_HandleTypeDef hdma_tim1_up;
extern UART_HandleTypeDef huart2;
//
//const char* filename = "433.wav"; // name of audio file to update CCR
//SD sd; // sd object used to handle updating CCR based on audio file
Gimbal gimbal; // need to update cam class to have default constuctor with init function

// main loop
void event_loop() {
    while (true) {
    	gimbal.poll_dma();
    }
}

// initialize program and start event_loop
void init() {
//	sd.init(filename, &htim1, &hdma_tim1_up);
	gimbal.init(&htim4, &htim5, &huart2);
	gimbal.request_pos();
	event_loop();
}

// HAL C functions
extern "C" {

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
    if (htim->Instance == TIM5) {
    	gimbal.request_pos();
    }
}

void HAL_PostInit() {
    init();
}

}


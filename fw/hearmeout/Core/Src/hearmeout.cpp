#include "main.h"
#include "transducer.hpp"

#define NUM_SAMPLES 8

extern TIM_HandleTypeDef htim1;

uint8_t current_sample = 0;
float audio_samples[NUM_SAMPLES] = {1.0f, 0.5f, 0.0f, 0.5f, 1.0f, 0.5f, 0.0f, 0.5f};
Transducer t;

void event_loop() {
	while (true) {}
}

void init() {
	t = Transducer(&htim1, TIM_CHANNEL_1, TIM_CHANNEL_2, NUM_SAMPLES);
	event_loop();
}


extern "C" {

// this callback is hit when the transducer sampling timer ccr value is reached
void HAL_TIM_OC_DelayElapsedCallback(TIM_HandleTypeDef *htim) {
	// check that the timer and
    if(htim->Instance == TIM1 && htim->Channel == HAL_TIM_ACTIVE_CHANNEL_2) {
    	t.play_sound(audio_samples[current_sample]);
    	current_sample = (current_sample + 1) % NUM_SAMPLES;
    }
}

// called by main.h allows for C++ projects
void HAL_PostInit() {
    init();
}

}

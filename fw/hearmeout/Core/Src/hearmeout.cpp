#include "main.h"
#include "transducer.hpp"

#define NUM_SAMPLES 8

extern TIM_HandleTypeDef htim1;

uint8_t current_sample = 0; // current sample index
float audio_samples[NUM_SAMPLES] = {1.0f, 0.5f, 0.0f, 0.5f, 1.0f, 0.5f, 0.0f, 0.5f}; // NUM_SAMPLES different sample values
Transducer t; // transducer object, gets instantiated in init

// main loop
void event_loop() {
	while (true) {}
}

// create transducer object and call event_loop
void init() {
	t = Transducer(&htim1, TIM_CHANNEL_1, TIM_CHANNEL_2, NUM_SAMPLES);
	event_loop();
}


// HAL C functions
extern "C" {

// this callback is hit when the transducer sampling timer ccr value is reached
void HAL_TIM_OC_DelayElapsedCallback(TIM_HandleTypeDef *htim) {
    if(htim == htim1) {
    	t.play_sound(audio_samples[current_sample]);
    	current_sample = (current_sample + 1) % NUM_SAMPLES;
    }
}

// called by main.h allows for C++ projects
void HAL_PostInit() {
    init();
}

}

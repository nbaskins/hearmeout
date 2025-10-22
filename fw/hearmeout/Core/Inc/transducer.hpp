#include "main.h"

class Transducer {
private:
	TIM_HandleTypeDef* tim; // pointer to handle timer
	uint16_t pwm_channel; // pwm channel
	uint16_t sample_channel; // sample interrupt channel
	uint8_t samples_per_period; // the number of audio samples per timer period
	bool is_initialized = false; // represents if the timer has been properly initialized

public:
	Transducer() = default;

	// creates transducer object
	Transducer(TIM_HandleTypeDef* tim_in, uint16_t pwm_channel_in, uint16_t sample_channel_in, uint8_t spp_in)
		: tim(tim_in), pwm_channel(pwm_channel_in), sample_channel(sample_channel_in), samples_per_period(spp_in) {
		// start pwm generation on pwm_channel
		HAL_TIM_PWM_Start(tim, pwm_channel);

		// configure sample callback frequency
		__HAL_TIM_SET_COMPARE(tim, sample_channel, tim->Instance->ARR / spp_in);
		HAL_TIM_OC_Start_IT(tim, sample_channel);

		is_initialized = true;
	};

	// plays a sound given an audio sample
	void play_sound(float sample) {
		if (!is_initialized)
			return; // return if tim has not been initialized yet

		// ensure that sample <= 1.0 so that CCR is <= ARR
		if (sample <= 1.0) {
			uint16_t duty_cycle = static_cast<uint16_t>(sample * tim->Instance->ARR);
			__HAL_TIM_SET_COMPARE(tim, pwm_channel, duty_cycle);
		} else {
			stop(); // stop if invalid sample value
		}
	}

	// play no noise by setting duty cycle to zero
	void stop() {
		__HAL_TIM_SET_COMPARE(tim, pwm_channel, 0);
	}
};

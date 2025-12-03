#include "main.h"

// class for servo implementation
class Servo {
private:
	TIM_HandleTypeDef* htim_ptr; // pointer to timer handle for servo
	uint32_t channel; // channel of timer that this servo is on

public:
	Servo(TIM_HandleTypeDef* htim_in, uint32_t channel_in)
		: htim_ptr(htim_in), channel(channel_in) {}

	// starts PWM generation for this servo
	void Servo::start_servo() {
		HAL_StatusTypeDef status = HAL_TIM_PWM_Start(this->m_tim_handle, this->m_channel);
		while (status != HAL_OK); // catch error
	}

	// adjust CCR to move servo to desired angle (0 - 180)
	void Servo::set_servo_angle(uint8_t angle) {
		// ensure angle is valid
		if (angle > 180)
			angle = 180;

		// find CCR for angle and set based on htim_ptr ARR
		uint32_t min_ccr = (htim_ptr->Instance->ARR + 1) / 5;
		uint32_t ccr = min_ccr + (angle * (htim_ptr->Instance->ARR + 1 - min_ccr)) / 270;
		__HAL_TIM_SET_COMPARE(htim_ptr, channel, ccr);
	}
};

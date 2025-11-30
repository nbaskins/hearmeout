/*
 * Class for handling SD data transfers and DMA with CCR
 */

#define MAX_ADC_VAL 4096

class AudioJack {
private:
	// audio jack adc
	ADC_HandleTypeDef* audio_jack_adc;

	// audio jack opamp
	OPAMP_HandleTypeDef* audio_amp;

	// requests
	bool requested_play;
	bool requested_pause;

public:
	AudioJack() = default;

	void init (ADC_HandleTypeDef* audio_jack_adc_in, OPAMP_HandleTypeDef* audio_amp_in) {
		audio_jack_adc = audio_jack_adc_in;
		audio_amp = audio_amp_in;

		// initialize the audio jack amp
		HAL_OPAMP_Start(audio_amp);

		// dont start the ADC interrupts here because we will default to not reading from the audio jack
	}

	void request_play(){
		requested_play = true;
	}

	void request_pause(){
		requested_pause = true;
	}

	void play(){
		HAL_ADC_Start_IT(audio_jack_adc);
	}

	void pause(){
		HAL_ADC_Stop_IT(audio_jack_adc);
	}

	void check_next(){
		if(requested_play){
			requested_play = false;
			play();
		}else if(requested_pause){
			requested_pause = false;
			pause();
		}
	}
};

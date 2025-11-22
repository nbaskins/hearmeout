#include <stdbool.h>
#include <string.h>
#include <cstdio>
#include <cstdlib>
#include "main.h"
#include "fatfs.h"
#include "sd.hpp"
#include "lvgl.h"

extern TIM_HandleTypeDef htim1;
extern TIM_HandleTypeDef htim2;
extern DMA_HandleTypeDef hdma_tim1_up;
extern SPI_HandleTypeDef hspi2;
extern SPI_HandleTypeDef hspi3;

SD sd; // sd object used to handle updating CCR based on audio file


// BEGIN //

extern "C" {

void setAddrWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);

void ILI9488_SendData_Multi(uint8_t *buff, size_t buff_size);

#define ILI9488_TFTWIDTH  320
#define ILI9488_TFTHEIGHT 480

void setRotation(uint8_t r);

void ILI9488_Init();

}

void my_flush_cb(lv_display_t * display, const lv_area_t * area, uint8_t * px_map)
{
	uint16_t x1 = (uint16_t) area->x1;
	uint16_t y1 = (uint16_t) area->y1;
	uint16_t x2 = (uint16_t) area->x2;
	uint16_t y2 = (uint16_t) area->y2;
	uint16_t px_map_size = abs(x2 - x1 + 1) * abs(y2 - y1 + 1);

	setAddrWindow(x1, y1, x2, y2);

	uint16_t * buf16 = (uint16_t *) px_map;

	uint8_t *buf8 = (uint8_t *)malloc(px_map_size * 3); // malloc fixes visual crap because I think stack was overflowing previously

	for (int i = 0; i < px_map_size; ++i) {
		uint16_t color = buf16[i];

		uint8_t r = (color & 0xF800) >> 11;
		uint8_t g = (color & 0x07E0) >> 5;
		uint8_t b = color & 0x001F;

		r = (r * 255) / 31;
		g = (g * 255) / 63;
		b = (b * 255) / 31;

		buf8[i * 3] = r;
		buf8[i * 3 + 1] = g;
		buf8[i * 3 + 2] = b;
	}

	ILI9488_SendData_Multi(buf8, px_map_size * 3);

	free(buf8);

    lv_display_flush_ready(display);
}

typedef enum {
    TouchscreenAxis_X,
    TouchscreenAxis_Y
} TouchscreenAxis;

#define SampleXControl 0x94
#define SampleYControl 0xD4
#define NUM_SAMPLES 1

uint16_t sampleTouchscreen(TouchscreenAxis axis) {

	HAL_NVIC_DisableIRQ(EXTI9_5_IRQn);

	uint16_t samples[NUM_SAMPLES];
	uint16_t result = 0;

	// adc values from calibration on 20-11-2025
	static uint16_t xmin = 180;
	static uint16_t xmax = 1800;
	static uint16_t ymin = 110;
	static uint16_t ymax = 1850;

	uint8_t ctrl[1] = { (axis == TouchscreenAxis_X) ? SampleXControl : SampleYControl };
	uint8_t poll[2] = { 0, 0 };
	uint16_t min = (axis == TouchscreenAxis_X) ? xmin : ymin;
	uint16_t max = (axis == TouchscreenAxis_X) ? xmax : ymax;
	uint16_t axisLength = (axis == TouchscreenAxis_X) ? ILI9488_TFTHEIGHT : ILI9488_TFTWIDTH;

	HAL_GPIO_WritePin(GPIOD, GPIO_PIN_0, GPIO_PIN_RESET); // select the touchscreen chip

	for (int i = 0; i < NUM_SAMPLES; ++i) {
		// send command to sample the axis
		HAL_SPI_Transmit(&hspi2, ctrl, 1, HAL_MAX_DELAY);

		// receive axis ADC reading
		HAL_SPI_Receive(&hspi2, poll, 2, HAL_MAX_DELAY);

		uint16_t sampleRaw = (uint16_t) ((poll[0] << 8) + poll[1]) >> 4;
		uint16_t sampleScaled = (uint16_t) ((float) (sampleRaw - min) * (float) axisLength / (float) (max - min));


		printf("raw sample for %s-axis: %hu\n\r", axis == TouchscreenAxis_X ? "X" : "Y", sampleRaw);
		if (sampleScaled > axisLength) { sampleScaled = axisLength; }

		samples[i] = sampleScaled;
	}

	HAL_GPIO_WritePin(GPIOD, GPIO_PIN_0, GPIO_PIN_SET); // unselect the touchscreen chip

	__HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_7);
	HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);


	return samples[0];

}

typedef enum {
	PEN_UP,
	PEN_DOWN
} PenState;
static volatile uint16_t penX = 0;
static volatile uint16_t penY = 0;
static volatile PenState penState = PEN_UP;

/**
 * This function gets called periodically by LVGL to read the state of the input device.
 * In this case the input device is the pen/finger.
 */
void my_input_read(lv_indev_t *indev, lv_indev_data_t *data) {
	data->state = (penState == PEN_UP) ? LV_INDEV_STATE_RELEASED : LV_INDEV_STATE_PRESSED;
	data->point.x = penX;
	data->point.y = penY;
}

static void trackPlayPauseBtn_event_cb(lv_event_t * e) {
	lv_event_code_t code = lv_event_get_code(e);
	lv_obj_t * btn = lv_event_get_target_obj(e);

	if(code == LV_EVENT_CLICKED) {
		static int playing = 0;
		playing = !playing;

		lv_obj_t * label = lv_obj_get_child(btn, 0);
		lv_label_set_text_fmt(label, "%s", playing ? "Playing" : "Paused");
	}
}

static void calibrationBtn_event_cb(lv_event_t * e) {
	lv_event_code_t code = lv_event_get_code(e);
	lv_obj_t * btn = lv_event_get_target_obj(e);

	static int opacity = 255;

	if(code == LV_EVENT_CLICKED) {
		opacity -= 50;
		lv_obj_set_style_bg_opa(btn, opacity, 0);
	}
}

// END //

// main loop
void event_loop() {
	while (true) {
        sd.check_prod();
        sd.check_next();
        //printf("CCR: %d\r\n", (int16_t)(htim2.Instance->CCR1));
        lv_timer_handler();
		HAL_Delay(5);
    }
}

// initialize program and start event_loop
void init() {
	sd.init(&htim1, &htim2, &hdma_tim1_up);
	ILI9488_Init();
	setRotation(LV_DISPLAY_ROTATION_0);


	lv_init();
	lv_tick_set_cb(HAL_GetTick);
	lv_display_t * display1 = lv_display_create(ILI9488_TFTWIDTH, ILI9488_TFTHEIGHT);
	lv_display_set_rotation(display1, LV_DISPLAY_ROTATION_0);

	lv_indev_t * indev = lv_indev_create();
	lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
	lv_indev_set_read_cb(indev, my_input_read);

	#define BYTES_PER_PIXEL (LV_COLOR_FORMAT_GET_SIZE(LV_COLOR_FORMAT_RGB565))
	static uint8_t buf1[ILI9488_TFTWIDTH * ILI9488_TFTHEIGHT / 10 * BYTES_PER_PIXEL];
	lv_display_set_buffers(display1, buf1, NULL, sizeof(buf1), LV_DISPLAY_RENDER_MODE_PARTIAL);
	lv_display_set_flush_cb(display1, my_flush_cb);




	/*Change the active screen's background color*/
	lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0x003a57), LV_PART_MAIN);

	lv_obj_t * container = lv_obj_create(lv_screen_active());
	lv_obj_set_size(container, LV_PCT(100), LV_SIZE_CONTENT);

	lv_style_t style;
	lv_style_init(&style);
	lv_style_set_flex_flow(&style, LV_FLEX_FLOW_ROW_WRAP);
	lv_style_set_flex_main_place(&style, LV_FLEX_ALIGN_SPACE_EVENLY);
	lv_style_set_layout(&style, LV_LAYOUT_FLEX);

	lv_obj_add_style(container, &style, 0);


	lv_obj_t * trackPrevBtn = lv_button_create(container);
	lv_obj_set_size(trackPrevBtn, 120, 50);


	lv_obj_t * trackNextBtn = lv_button_create(container);
	lv_obj_set_size(trackNextBtn, 120, 50);

	lv_obj_t * trackPlayPauseBtn = lv_button_create(container);
	lv_obj_set_size(trackPlayPauseBtn, 120, 50);
	lv_obj_add_event_cb(trackPlayPauseBtn, trackPlayPauseBtn_event_cb, LV_EVENT_CLICKED, NULL);


	lv_obj_t * trackPrevLabel = lv_label_create(trackPrevBtn);
	lv_label_set_text(trackPrevLabel, "<");
	lv_obj_center(trackPrevLabel);

	lv_obj_t * trackNextLabel = lv_label_create(trackNextBtn);
	lv_label_set_text(trackNextLabel, ">");
	lv_obj_center(trackNextLabel);

	lv_obj_t * trackPlayPauseLabel = lv_label_create(trackPlayPauseBtn);
	lv_label_set_text(trackPlayPauseLabel, "Play/Pause");
	lv_obj_center(trackPlayPauseLabel);

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

		if (interrupt_time - last_interrupt_time > 5) { // debounce

			GPIO_PinState rf = HAL_GPIO_ReadPin(GPIOD, GPIO_Pin);

			printf("interrupt caused by pen %s\n\r", rf == GPIO_PIN_SET ? "UP" : "DOWN");

			PenState prevPenState = penState;
			penState = rf == GPIO_PIN_SET ? PEN_UP : PEN_DOWN;

			if (prevPenState != penState && penState == PEN_DOWN) {
				uint16_t x = sampleTouchscreen(TouchscreenAxis_X);
				uint16_t y = sampleTouchscreen(TouchscreenAxis_Y);
				penX = x;
				penY = y;

				printf("(penX, penY) -> (%hu, %hu)\n\r", penX, penY);

				if(is_pause){
					sd.request_pause();
				}else{
					sd.request_play();
				}
				is_pause = !is_pause;
			}

			last_interrupt_time = interrupt_time;
		}
	}
}

// called by main.h allows for C++ projects
void HAL_PostInit() {
    init();
}

}

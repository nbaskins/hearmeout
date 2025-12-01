/*
 * Class for handling Screen animations and rendering
 */
#pragma once

#include "font.hpp"
#include "palette.hpp"

/*
 * Pixel coordinates start from the bottom left
 */

enum SCREEN_CMD : uint8_t {
	CASET = 0x2A,
	PASET = 0x2B,
	RAMWR = 0x2C,
	MADCTL = 0x36,
	PGAMCTRL = 0xE0,
	NGAMCTRL = 0xE1,
	PWCTRL1 = 0xC0,
	PWCTRL2 = 0xC1,
	VMCTRL = 0xC5,
	COLMOD = 0x3A,
	IFMODE = 0xB0,
	FRMCTR1 = 0xB1,
	SLPOUT = 0x11,
	DISON = 0x29,
	INVTR = 0xB4,
	DISCTRL = 0xB6,
	SETIMAGE = 0xE9,
	ADJCTRL = 0xF7
};

class Screen {
private:

	// Communication Variables
	SPI_HandleTypeDef* display_spi;
	SPI_HandleTypeDef* touch_spi;
	TIM_HandleTypeDef* touch_timer_poll;
	static constexpr int TEMP_BUFFER_SIZE = 256;
	static constexpr int NUM_RGB = 3;
	uint8_t temp[TEMP_BUFFER_SIZE][NUM_RGB];

	// Screen Constants
	static constexpr int SCREEN_WIDTH = 480;
	static constexpr int SCREEN_HEIGHT = 320;
	static constexpr int SCREEN_PRESS_THRESHOLD = 100;
	static constexpr int RGB_SIZE = 3;

	// Button Variables
	enum BUTTON_STATE : uint8_t{
		PRESSED = 0x0,
		UNPRESSED = 0x1
	};
	struct button{
		bool valid;
		uint16_t x;
		uint16_t y;
		uint16_t w;
		uint16_t h;
		BUTTON_STATE button_state;
		char const* text;
		void (*callback)();
	};

	static constexpr int NUM_BUTTONS = 5;

	int num_buttons;
	button buttons[NUM_BUTTONS];

	// Private Member Functions

	void cs_low(){
		HAL_GPIO_WritePin(GPIOD, GPIO_PIN_2, GPIO_PIN_RESET);
	}

	void cs_high(){
		HAL_GPIO_WritePin(GPIOD, GPIO_PIN_2, GPIO_PIN_SET);
	}

	void dc_low(){
		HAL_GPIO_WritePin(GPIOF, GPIO_PIN_5, GPIO_PIN_RESET);
	}

	void dc_high(){
		HAL_GPIO_WritePin(GPIOF, GPIO_PIN_5, GPIO_PIN_SET);
	}

	void rst_low(){
		HAL_GPIO_WritePin(GPIOF, GPIO_PIN_3, GPIO_PIN_RESET);
	}

	void rst_high(){
		HAL_GPIO_WritePin(GPIOF, GPIO_PIN_3, GPIO_PIN_SET);
	}

	void send_cmd(SCREEN_CMD cmd){
		HAL_StatusTypeDef status{};
		 // turn D/C low to indicate a command
		dc_low();

		// select the touchscreen chip
		cs_low();

		status = HAL_SPI_Transmit(display_spi, reinterpret_cast<uint8_t const*>(&cmd), sizeof(SCREEN_CMD), HAL_MAX_DELAY); // send the CASET command

		// check the status
		if(status != HAL_OK){
			printf("Error Occured while sending SPI command\r\n");
		}

		// deselect the touchscreen chip
		cs_high();
	}

	void send_data(uint8_t* buf, int len){
		HAL_StatusTypeDef status{};

		// turn D/C low to indicate a command
		dc_high();

		// select the touchscreen chip
		cs_low();
		status = HAL_SPI_Transmit(display_spi, buf, len, HAL_MAX_DELAY); // send the data

		// deselect the touchscreen chip
		cs_high();

		// check the status
		if(status != HAL_OK){
			printf("Error Occured while sending SPI data\r\n");
		}


	}

	void render_button(uint16_t index){
		// draw the button on the screen
		uint8_t caset[] = {static_cast<uint8_t>((buttons[index].x >> 8) & 0xFF), static_cast<uint8_t>(buttons[index].x & 0xFF), static_cast<uint8_t>(((buttons[index].x + buttons[index].w - 1) >> 8) & 0xFF), static_cast<uint8_t>((buttons[index].x + buttons[index].w - 1) & 0xFF)};
		send_cmd(SCREEN_CMD::CASET);
		send_data(caset, sizeof(caset));

		uint8_t paset[] = {static_cast<uint8_t>((buttons[index].y >> 8) & 0xFF), static_cast<uint8_t>(buttons[index].y & 0xFF), static_cast<uint8_t>(((buttons[index].y + buttons[index].h - 1) >> 8) & 0xFF), static_cast<uint8_t>((buttons[index].y + buttons[index].h - 1) & 0xFF)};
		send_cmd(SCREEN_CMD::PASET);
		send_data(paset, sizeof(paset));

		unsigned int num_pixels = buttons[index].w * buttons[index].h;
		uint8_t button_rgb[NUM_RGB];

		if(buttons[index].button_state == BUTTON_STATE::UNPRESSED){
			for(int i = 0; i < TEMP_BUFFER_SIZE; ++i){
				temp[i][0] = UNPRESSED_BUTTON_R;
				temp[i][1] = UNPRESSED_BUTTON_G;
				temp[i][2] = UNPRESSED_BUTTON_B;
			}
		}else{
			for(int i = 0; i < TEMP_BUFFER_SIZE; ++i){
				temp[i][0] = PRESSED_BUTTON_R;
				temp[i][1] = PRESSED_BUTTON_G;
				temp[i][2] = PRESSED_BUTTON_B;
			}
		}

		send_cmd(SCREEN_CMD::RAMWR);

		for(unsigned int i = 0; i < (num_pixels + TEMP_BUFFER_SIZE - 1) / TEMP_BUFFER_SIZE; ++i){
			send_data(&temp[0][0], sizeof(temp));
		}

		// draw text
		if(buttons[index].text){
			int len = strlen(buttons[index].text);
			draw_string(buttons[index].x + (buttons[index].w / 2) - (len * (FONT_WIDTH + FONT_SPACING) * FONT_SIZE) / 2, buttons[index].y + (buttons[index].h / 2) - (FONT_HEIGHT * FONT_SIZE) / 2, buttons[index].text);
		}
	}

public:
	Screen() = default;

	void draw_box(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t r, uint8_t g, uint8_t b){
		// draw the button on the screen
		uint8_t caset[] = {static_cast<uint8_t>((x >> 8) & 0xFF), static_cast<uint8_t>(x & 0xFF), static_cast<uint8_t>(((x + w - 1) >> 8) & 0xFF), static_cast<uint8_t>((x + w - 1) & 0xFF)};
		send_cmd(SCREEN_CMD::CASET);
		send_data(caset, sizeof(caset));

		uint8_t paset[] = {static_cast<uint8_t>((y >> 8) & 0xFF), static_cast<uint8_t>(y & 0xFF), static_cast<uint8_t>(((y + h - 1) >> 8) & 0xFF), static_cast<uint8_t>((y + h - 1) & 0xFF)};
		send_cmd(SCREEN_CMD::PASET);
		send_data(paset, sizeof(paset));

		unsigned int num_pixels = w * h;
		uint8_t rgb[NUM_RGB] = {r, g, b};

		for(int i = 0; i < TEMP_BUFFER_SIZE; ++i){
			temp[i][0] = rgb[0];
			temp[i][1] = rgb[1];
			temp[i][2] = rgb[2];
		}

		send_cmd(SCREEN_CMD::RAMWR);

		for(unsigned int i = 0; i < (num_pixels + TEMP_BUFFER_SIZE - 1) / TEMP_BUFFER_SIZE; ++i){
			send_data(&temp[0][0], sizeof(temp));
		}
	}

	void clear(){
		// clear the screen to the background color
		draw_box(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, BACKGROUND_R, BACKGROUND_G, BACKGROUND_B);

		num_buttons = 0;
		for(int i = 0; i < NUM_BUTTONS; ++i){
			buttons[i].valid = false;
		}
	}

	void init (SPI_HandleTypeDef* display_spi_in, SPI_HandleTypeDef* touch_spi_in, TIM_HandleTypeDef* touch_timer_poll_in) {
		printf("Initializing Screen\r\n");

		// get the spi device we will talk over
		display_spi = display_spi_in;
		touch_spi = touch_spi_in;

		cs_high();
		rst_low();
		HAL_Delay(10);
		rst_high();

		// set the gamma config
		// using gamma configuration from https://github.com/squadracorsepolito/ILI9488/blob/master/ili9488.c
		uint8_t p_gamma_config[] = {
				0x00,
				0x13,
				0x18,
				0x04,
				0x0F,
				0x06,
				0x3A,
				0x56,
				0x4D,
				0x03,
				0x0A,
				0x06,
				0x30,
				0x3E,
				0x0F
		};
		send_cmd(SCREEN_CMD::PGAMCTRL);
		send_data(p_gamma_config, sizeof(p_gamma_config));

		uint8_t n_gamma_config[] = {
				0x00,
				0x13,
				0x18,
				0x01,
				0x11,
				0x06,
				0x38,
				0x34,
				0x4D,
				0x06,
				0x0D,
				0x0B,
				0x31,
				0x37,
				0x0F
		};
		send_cmd(SCREEN_CMD::NGAMCTRL);
		send_data(n_gamma_config, sizeof(n_gamma_config));

		// power settings
		uint8_t pwrctl1[] = {0x17, 0x15};
		send_cmd(SCREEN_CMD::PWCTRL1);
		send_data(pwrctl1, sizeof(pwrctl1));
		uint8_t pwrctl2[] = {0x41};
		send_cmd(SCREEN_CMD::PWCTRL2);
		send_data(pwrctl2, sizeof(pwrctl2));

		uint8_t vmctrl[] = {0x00, 0x12, 0x80};
		send_cmd(SCREEN_CMD::VMCTRL);
		send_data(vmctrl, sizeof(vmctrl));

		// memory access pattern
		uint8_t madctl[] = {0x28};
		send_cmd(SCREEN_CMD::MADCTL);
		send_data(madctl, sizeof(madctl));

		// choose pixel interface
		uint8_t colmod[] = {0x66};
		send_cmd(SCREEN_CMD::COLMOD);
		send_data(colmod, sizeof(colmod));

		// choose interface
		uint8_t ifmode[] = {0x80};
		send_cmd(SCREEN_CMD::IFMODE);
		send_data(ifmode, sizeof(ifmode));

		// choose framerate
		uint8_t frmctr1[] = {0xA0};
		send_cmd(SCREEN_CMD::FRMCTR1);
		send_data(frmctr1, sizeof(frmctr1));

		uint8_t invtr[] = {0x02};
		send_cmd(SCREEN_CMD::INVTR);
		send_data(invtr, sizeof(invtr));

		send_cmd(SCREEN_CMD::DISCTRL);
		send_data(invtr, sizeof(invtr));
		send_data(invtr, sizeof(invtr));

		// choose image set
		uint8_t setimage[] = {0x00};
		send_cmd(SCREEN_CMD::SETIMAGE);
		send_data(setimage, sizeof(setimage));

		// choose image set
		uint8_t adjctrl[] = {0xA9, 0x51, 0x2C, 0x82};
		send_cmd(SCREEN_CMD::ADJCTRL);
		send_data(adjctrl, sizeof(adjctrl));

		send_cmd(SCREEN_CMD::SLPOUT);

		HAL_Delay(150);

		send_cmd(SCREEN_CMD::DISON);

		// the timer we are going to use to poll the screen
		touch_timer_poll = touch_timer_poll_in;
		HAL_TIM_Base_Start_IT(touch_timer_poll);
	}

	void draw_button(uint16_t x, uint16_t y, uint16_t w, uint16_t h, void (*callback)(), char const* text){
		// check to see if we have another button spot
		if(num_buttons >= NUM_BUTTONS){
			printf("No more available buttons\r\n");
			return;
		}

		// update the data structures
		buttons[num_buttons].valid = true;
		buttons[num_buttons].x = x;
		buttons[num_buttons].y = y;
		buttons[num_buttons].w = w;
		buttons[num_buttons].h = h;
		buttons[num_buttons].button_state = BUTTON_STATE::UNPRESSED;
		buttons[num_buttons].text = text;
		buttons[num_buttons].callback = callback;

		render_button(num_buttons);

		++num_buttons;
	}

	void check_buttons(uint16_t press_x, uint16_t press_y, uint16_t press_z){
		for(int i = 0; i < NUM_BUTTONS; ++i){
			if(!buttons[i].valid){
				continue;
			}

			// detect rising edge
			if(	buttons[i].button_state == BUTTON_STATE::UNPRESSED && press_z >= SCREEN_PRESS_THRESHOLD &&
				buttons[i].x <= press_x && press_x <= buttons[i].x + buttons[i].w &&
				buttons[i].y <= press_y && press_y <= buttons[i].y + buttons[i].h){
				// update the button state
				buttons[i].button_state = BUTTON_STATE::PRESSED;

				// rerender the button
				render_button(i);

				// call the buttons callback
				(*buttons[i].callback)();
			}

			// detect falling edge
			if(buttons[i].button_state == BUTTON_STATE::PRESSED && !(press_z >= SCREEN_PRESS_THRESHOLD &&
					buttons[i].x <= press_x && press_x <= buttons[i].x + buttons[i].w &&
					buttons[i].y <= press_y && press_y <= buttons[i].y + buttons[i].h)){
				buttons[i].button_state = BUTTON_STATE::UNPRESSED;
				// rerender the button
				render_button(i);
			}
		}
	}

	void draw_string(uint16_t base_x, uint16_t base_y, char const* str){
		int str_len = strlen(str);
		// loop through all of the characters in the string
		for(int idx = 0; idx < str_len; ++idx){

			uint8_t* char_ptr = &font[(str[idx] - 'A') * FONT_HEIGHT];

			// loop through all of the rows in a character
			for(int row = 0; row < FONT_HEIGHT; ++row){
				uint8_t row_data = char_ptr[row];
				for(int col = 0; col < FONT_WIDTH; ++col){
					// if there is a pixel to draw, draw it
					if((row_data & 0x80) == 0x80){
						// draw the button on the screen
						uint16_t px = base_x + FONT_SIZE * col;
						uint8_t caset[] = {static_cast<uint8_t>((px >> 8) & 0xFF), static_cast<uint8_t>(px & 0xFF), static_cast<uint8_t>(((px + FONT_SIZE - 1) >> 8) & 0xFF), static_cast<uint8_t>((px + FONT_SIZE - 1) & 0xFF)};
						send_cmd(SCREEN_CMD::CASET);
						send_data(caset, sizeof(caset));

						uint16_t py = base_y + FONT_SIZE * row;
						uint8_t paset[] = {static_cast<uint8_t>((py >> 8) & 0xFF), static_cast<uint8_t>(py & 0xFF), static_cast<uint8_t>(((py + FONT_SIZE - 1) >> 8) & 0xFF), static_cast<uint8_t>((py + FONT_SIZE - 1) & 0xFF)};
						send_cmd(SCREEN_CMD::PASET);
						send_data(paset, sizeof(paset));

						unsigned int num_pixels = FONT_SIZE * FONT_SIZE;
						uint8_t font_rgb[] = {FONT_R, FONT_G, FONT_B};
						send_cmd(SCREEN_CMD::RAMWR);

						for(unsigned int i = 0; i < num_pixels; ++i){
							send_data(font_rgb, sizeof(font_rgb));
						}
					}

					// shift to get the next pixel in the font
					row_data <<= 1;
				}
			}

			// increment for the next
			base_x += FONT_SIZE * (FONT_WIDTH + FONT_SPACING);
		}
	}

	void sample_x_y(uint16_t* x_in, uint16_t* y_in, uint16_t* z_in){
		// commands to sample x and y
		static constexpr uint8_t sample_x = 0x94;
		static constexpr uint8_t sample_y = 0xD4;
		static constexpr uint8_t sample_z = 0xB4;

		// adc values from calibration on 20-11-2025
		static uint16_t x_min = 180;
		static uint16_t x_max = 1800;
		static uint16_t y_min = 1850;
		static uint16_t y_max = 110;

		HAL_GPIO_WritePin(GPIOD, GPIO_PIN_0, GPIO_PIN_RESET); // select the touchscreen chip

		// send the command to read the x touch location
		uint8_t buf[2];
		HAL_SPI_Transmit(touch_spi, &sample_x, sizeof(sample_x), HAL_MAX_DELAY);
		HAL_SPI_Receive(touch_spi, buf, sizeof(buf), HAL_MAX_DELAY);

		HAL_GPIO_WritePin(GPIOD, GPIO_PIN_0, GPIO_PIN_SET); // unselect the touchscreen chip


		*x_in = ((buf[0] << 8) + buf[1]) >> 4;
		*x_in = (uint16_t) ((float) (*x_in - x_min) * (float) SCREEN_WIDTH / (float) (x_max - x_min));

		HAL_GPIO_WritePin(GPIOD, GPIO_PIN_0, GPIO_PIN_RESET); // select the touchscreen chip

		// send the command to read the y touch location
		HAL_SPI_Transmit(touch_spi, &sample_y, sizeof(sample_y), HAL_MAX_DELAY);
		HAL_SPI_Receive(touch_spi, buf, sizeof(buf), HAL_MAX_DELAY);

		HAL_GPIO_WritePin(GPIOD, GPIO_PIN_0, GPIO_PIN_SET); // unselect the touchscreen chip

		*y_in = ((buf[0] << 8) + buf[1]) >> 4;
		*y_in = (uint16_t) ((float) (*y_in - y_min) * (float) SCREEN_HEIGHT / (float) (y_max - y_min));

		HAL_GPIO_WritePin(GPIOD, GPIO_PIN_0, GPIO_PIN_RESET); // select the touchscreen chip

		// send the command to read the z touch location
		HAL_SPI_Transmit(touch_spi, &sample_z, sizeof(sample_z), HAL_MAX_DELAY);
		HAL_SPI_Receive(touch_spi, buf, sizeof(buf), HAL_MAX_DELAY);
		*z_in = ((buf[0] << 8) + buf[1]) >> 4;

		HAL_GPIO_WritePin(GPIOD, GPIO_PIN_0, GPIO_PIN_SET); // unselect the touchscreen chip
	}

	void draw_image_init(uint16_t x, uint16_t y, uint16_t w, uint16_t h){
		// draw the button on the screen
		uint8_t caset[] = {static_cast<uint8_t>((x >> 8) & 0xFF), static_cast<uint8_t>(x & 0xFF), static_cast<uint8_t>(((x + w - 1) >> 8) & 0xFF), static_cast<uint8_t>((x + w - 1) & 0xFF)};
		send_cmd(SCREEN_CMD::CASET);
		send_data(caset, sizeof(caset));

		uint8_t paset[] = {static_cast<uint8_t>((y >> 8) & 0xFF), static_cast<uint8_t>(y & 0xFF), static_cast<uint8_t>(((y + h - 1) >> 8) & 0xFF), static_cast<uint8_t>((y + h - 1) & 0xFF)};
		send_cmd(SCREEN_CMD::PASET);
		send_data(paset, sizeof(paset));

		send_cmd(SCREEN_CMD::RAMWR);
	}

	void draw_image_row(uint8_t* buf, int len){
		send_data(buf, len);
	}
};

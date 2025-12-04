#include "main.h"
#include <string.h>
#include <cstdio>

// struct type for data received from the pixy cam
typedef struct {
	uint16_t sig;
	uint16_t x, y, w, h;
	int16_t angle;
	uint8_t idx, age;
} PixyBlock;

// class for camera/servo gimbal system
class Gimbal {
private:
    TIM_HandleTypeDef* servo_tim; // servo timer handle
    TIM_HandleTypeDef* cam_tim; // camera interrupt timer handle
    UART_HandleTypeDef* uart; // uart handle
    uint16_t max_step; // max CCR step size for the servos (300 seems to work well)
    float smooth_x; // smoothed value for x direction (value after alpha is applied)
    float smooth_y; // smoothed value for y direction (value after alpha is applied)
    float alpha; // smoothing factor to reduce jumping from camera jitter
    uint16_t A_v; // new CCR value for vertical servo
    uint16_t A_h; // new CCR value for horizontal servo
    static const uint16_t DMA_RX_BUFFER_SIZE = 256; // chat suggested a size of 256 for this
    uint8_t dma_rx_buf[DMA_RX_BUFFER_SIZE]; // buffer for incoming RX data
    uint16_t dma_last_pos = 0; // last position read in dma buffer
    uint8_t frame_buf[32]; // array of bytes for PixyBlock
    uint8_t frame_pos = 0; // tracks the number of bytes stored so far in frame_buf
    uint8_t tx_buff[6]; // buffer for sending over TX line

    // parses raw bytes of b and stores data in PixyBlock
    bool parse_one_block(const uint8_t *b, PixyBlock *o) {
        if (b[0] != 0xAF || b[1] != 0xC1 || b[2] != 0x21 || b[3] != 0x0E)
            return false;

        uint16_t chk = b[4] | (b[5] << 8);
        uint16_t sum = 0;
        for (int i = 6; i < 20; ++i) sum += b[i];
        if (sum != chk)
            return false;

        o->sig   = b[6]  | (b[7]  << 8);
        o->x     = b[8]  | (b[9]  << 8);
        o->y     = b[10] | (b[11] << 8);
        o->w     = b[12] | (b[13] << 8);
        o->h     = b[14] | (b[15] << 8);
        o->angle = b[16] | (b[17] << 8);
        o->idx   = b[18];
        o->age   = b[19];

        return true;
    }

    // updates the smoothed values based on blk, updates CCR after finding values
    void handle_block(const PixyBlock &blk) {
        if (blk.sig == 1) {
            smooth_x = alpha * blk.x + (1.0f - alpha) * smooth_x;
            smooth_y = alpha * blk.y + (1.0f - alpha) * smooth_y;

            A_v = calculate_A_v(smooth_y, A_v);
            A_h = calculate_A_h(smooth_x, A_h);
        }

        __HAL_TIM_SET_COMPARE(servo_tim, TIM_CHANNEL_1, A_v);
        __HAL_TIM_SET_COMPARE(servo_tim, TIM_CHANNEL_2, A_h);
    }

    // adds received bytes to frame_buf and converts frame_buf to PixyBlock when 20 have arrived
    void process_bytes(uint8_t *data, uint16_t len) {
        for (uint16_t i = 0; i < len; i++) {
            uint8_t b = data[i];
            frame_buf[frame_pos++] = b;

            if (frame_pos >= 4) {
                if (!(frame_buf[0] == 0xAF && frame_buf[1] == 0xC1 && frame_buf[2] == 0x21 && frame_buf[3] == 0x0E)) {
                    memmove(frame_buf, frame_buf + 1, --frame_pos);
                    continue;
                }
            }

            if (frame_pos == 20) {
                PixyBlock p;
                if (parse_one_block(frame_buf, &p))
                    handle_block(p);

                frame_pos = 0;
            }
        }
    }

public:
    // default constructor for gimbal, init will actually construct the gimbal
    Gimbal() = default;

    // initializes the gimbal, need this instead of constructor so initialization happens after main.c even with globals
    void init(TIM_HandleTypeDef* servo_tim_in, TIM_HandleTypeDef* cam_tim_in, UART_HandleTypeDef* uart_in) {
        servo_tim = servo_tim_in;
        cam_tim = cam_tim_in;
        uart = uart_in;

        max_step = 300;
        smooth_x = 157.5f;
        smooth_y = 103.5f;
        alpha = 1.0f;
        A_v = 1500;
        A_h = 1500;

        uint8_t cmd[6] = {0xAE, 0xC1, 0x20, 0x02, 0x01, 0xFF};
        memcpy(tx_buff, cmd, 6);

        HAL_TIM_PWM_Start(servo_tim, TIM_CHANNEL_1);
        HAL_TIM_PWM_Start(servo_tim, TIM_CHANNEL_2);
        HAL_TIM_Base_Start_IT(cam_tim);

        __HAL_TIM_SET_COMPARE(servo_tim, TIM_CHANNEL_1, A_v);
        __HAL_TIM_SET_COMPARE(servo_tim, TIM_CHANNEL_2, A_h);

        HAL_UARTEx_ReceiveToIdle_DMA(uart, dma_rx_buf, DMA_RX_BUFFER_SIZE);
    }

    void process_rx_bytes(uint16_t size) {
    	process_bytes(dma_rx_buf, size);
    }

    // requests a pixy block from the pixy cam
    void request_pos() {
        HAL_UART_Transmit_IT(uart, tx_buff, sizeof(tx_buff));
    }

    // polls the dma buffer for any new bytes that have arrived
    void poll_dma() {
        uint16_t pos = DMA_RX_BUFFER_SIZE - __HAL_DMA_GET_COUNTER(uart->hdmarx);

        if (pos != dma_last_pos) {
            if (pos > dma_last_pos) {
                process_bytes(&dma_rx_buf[dma_last_pos], pos - dma_last_pos);
            } else {
                process_bytes(&dma_rx_buf[dma_last_pos], DMA_RX_BUFFER_SIZE - dma_last_pos);
                process_bytes(&dma_rx_buf[0], pos);
            }

            dma_last_pos = pos;
        }
    }

    // calculates new vertical CCR
    uint16_t calculate_A_v(uint16_t y, uint16_t A_v) {
        if (y > 108.5f || y < 98.5f) {
            int16_t delta = (y - 103.5f) / 5.175f * (5.0f) / 2.0f;
            if (delta > max_step) delta = max_step;
            if (delta < -max_step) delta = -max_step;

            A_v += delta;
            if (A_v > 2300) A_v = 2300;
            if (A_v < 800) A_v = 800;
        }
        return A_v;
    }

    // calculates new horizontal CCR
    uint16_t calculate_A_h(uint16_t x, uint16_t A_h) {
        if (x > 165 || x < 150) {
            int16_t delta = (157.5f - x) / 5.25f * (7.0f) / 2.0f;
            if (delta > max_step) delta = max_step;
            if (delta < -max_step) delta = -max_step;

            A_h += delta;
            if (A_h > 2500) A_h = 2500;
            if (A_h < 500) A_h = 500;
        }
        return A_h;
    }
};

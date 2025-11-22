/*
 * ili9488.c
 *
 *  Created on: Dec 14, 2021
 *      Author: timagr615
 */
#include "ili9488.h"

extern SPI_HandleTypeDef hspi3;
extern UART_HandleTypeDef huart1;

static uint8_t rotationNum=1;
static bool _cp437    = false;
static SPI_HandleTypeDef lcdSPIhandle;
//Chip Select pin
static GPIO_TypeDef  *tftCS_GPIO;
static uint16_t tftCS_PIN;
//Data Command pin
static GPIO_TypeDef  *tftDC_GPIO;
static uint16_t tftDC_PIN;
//Reset pin
static GPIO_TypeDef  *tftRESET_GPIO;
static uint16_t tftRESET_PIN;
//uint8_t frm_buf[65536] = {0};
uint16_t width;
uint16_t height;

//***** Functions prototypes *****//

void SendByte(uint8_t data)
{
  //while((SPI1->SR & SPI_SR_TXE) == RESET);
  SPI1->DR = data;
}
//1. Write Command to LCD
void ILI9488_SendCommand(uint8_t com)
{
	//*(__IO uint8_t *)(0x60000000) = com;
	uint8_t tmpCmd = com;
	//Set DC HIGH for COMMAND mode
	//HAL_GPIO_WritePin(tftDC_GPIO, tftDC_PIN, GPIO_PIN_RESET);
	DC_COMMAND();
	//Put CS LOW
	//HAL_GPIO_WritePin(tftCS_GPIO, tftCS_PIN, GPIO_PIN_RESET);
	CS_A();
	//Write byte using SPI
	HAL_SPI_Transmit(&hspi3, &tmpCmd, 1, 1);
	//SendByte(tmpCmd);
	//WaitLastData();
	CS_D();
	//Bring CS HIGH
	//HAL_GPIO_WritePin(tftCS_GPIO, tftCS_PIN, GPIO_PIN_SET);
}

//2. Write data to LCD
void ILI9488_SendData(uint8_t data)
{
	//*(__IO uint8_t *)(0x60040000) = data;
	uint8_t tmpCmd = data;
	//Set DC LOW for DATA mode
	//HAL_GPIO_WritePin(tftDC_GPIO, tftDC_PIN, GPIO_PIN_SET);
	DC_DATA();
	//Put CS LOW
	//HAL_GPIO_WritePin(tftCS_GPIO, tftCS_PIN, GPIO_PIN_RESET);
	CS_A();
	//Write byte using SPI
	HAL_SPI_Transmit(&hspi3, &tmpCmd, 1, 1);
	//SendByte(tmpCmd);
	//WaitLastData();
	CS_D();

	//Bring CS HIGH
	//HAL_GPIO_WritePin(tftCS_GPIO, tftCS_PIN, GPIO_PIN_SET);
}
//2.2 Write multiple/DMA
void ILI9488_SendData_Multi(uint8_t *buff, size_t buff_size){
	DC_DATA();
	/*CS_A();
	for (uint32_t i = 0; i < buff_size; i++)
	  {
	    SendByte(*buff);
	    buff++;
	  }

	  WaitLastData();
	  CS_D();*/
	CS_A();
	while (buff_size > 0){
		uint16_t chunk_size = buff_size > 32768 ? 32768 : buff_size;
		HAL_SPI_Transmit(&hspi3, buff, chunk_size, HAL_MAX_DELAY);
		buff += chunk_size;
		buff_size -= chunk_size;
	}
	CS_D();
}

void WaitLastData()
{
	while(!(SPI1->SR & SPI_SR_TXE));
	while(SPI1->SR & SPI_SR_BSY);
}
/*void ILI9488_SendData_Multi(uint16_t Colordata, uint32_t size)
{
	uint8_t colorL,colorH;

	//Set DC LOW for DATA mode
	//HAL_GPIO_WritePin(tftDC_GPIO, tftDC_PIN, GPIO_PIN_SET);
	DC_DATA();
	//Put CS LOW
	//HAL_GPIO_WritePin(tftCS_GPIO, tftCS_PIN, GPIO_PIN_RESET);
	CS_A();
	//Write byte using SPI
	HAL_SPI_Transmit(&lcdSPIhandle, (uint8_t *)&Colordata, size, 1000);
	//Wait for end of DMA transfer
	//Bring CS HIGH
	CS_D();
	//HAL_GPIO_WritePin(tftCS_GPIO, tftCS_PIN, GPIO_PIN_SET);
}*/

//void ILI9488_Init(SPI_HandleTypeDef *spiLcdHandle, GPIO_TypeDef *csPORT, uint16_t csPIN, GPIO_TypeDef *dcPORT, uint16_t dcPIN, GPIO_TypeDef *resetPORT, uint16_t resetPIN)
void ILI9488_Init()
{

	 /*//Copy SPI settings
	 memcpy(&lcdSPIhandle, spiLcdHandle, sizeof(*spiLcdHandle));
	 //CS pin
	 tftCS_GPIO = csPORT;
	 tftCS_PIN = csPIN;
	 //DC pin
	 tftDC_GPIO = dcPORT;
	 tftDC_PIN = dcPIN;
	 //HAL_GPIO_WritePin(tftCS_GPIO, tftCS_PIN, GPIO_PIN_SET);
	 CS_D();
	 //RESET pin
	 tftRESET_GPIO = resetPORT;
	 tftRESET_PIN = resetPIN;
	 //HAL_GPIO_WritePin(tftRESET_GPIO, tftRESET_PIN, GPIO_PIN_RESET);  //Turn LCD ON
	 RST_A();
	 HAL_Delay(10);
	 RST_D();
	 //HAL_GPIO_WritePin(tftRESET_GPIO, tftRESET_PIN, GPIO_PIN_SET);  //Turn LCD ON*/
	//SPI1->CR1 |= SPI_CR1_SPE;
	CS_D();
	RST_A();
	HAL_Delay(10);
	RST_D();
	width=ILI9488_TFTWIDTH;
	height=ILI9488_TFTHEIGHT;
	//CS_A();
	ILI9488_SendCommand(0xE0);
	ILI9488_SendData(0x00);
	ILI9488_SendData(0x03);
	ILI9488_SendData(0x09);
	ILI9488_SendData(0x08);
	ILI9488_SendData(0x16);
	ILI9488_SendData(0x0A);
	ILI9488_SendData(0x3F);
	ILI9488_SendData(0x78);
	ILI9488_SendData(0x4C);
	ILI9488_SendData(0x09);
	ILI9488_SendData(0x0A);
	ILI9488_SendData(0x08);
	ILI9488_SendData(0x16);
	ILI9488_SendData(0x1A);
	ILI9488_SendData(0x0F);

	ILI9488_SendCommand(0XE1);
	ILI9488_SendData(0x00);
	ILI9488_SendData(0x16);
	ILI9488_SendData(0x19);
	ILI9488_SendData(0x03);
	ILI9488_SendData(0x0F);
	ILI9488_SendData(0x05);
	ILI9488_SendData(0x32);
	ILI9488_SendData(0x45);
	ILI9488_SendData(0x46);
	ILI9488_SendData(0x04);
	ILI9488_SendData(0x0E);
	ILI9488_SendData(0x0D);
	ILI9488_SendData(0x35);
	ILI9488_SendData(0x37);
	ILI9488_SendData(0x0F);

	ILI9488_SendCommand(0XC0);      //Power Control 1
	ILI9488_SendData(0x17);    		//Vreg1out
	ILI9488_SendData(0x15);    		//Verg2out

	ILI9488_SendCommand(0xC1);      //Power Control 2
	ILI9488_SendData(0x41);    		//VGH,VGL

	ILI9488_SendCommand(0xC5);      //Power Control 3
	ILI9488_SendData(0x00);
	ILI9488_SendData(0x12);    		//Vcom
	ILI9488_SendData(0x80);

	ILI9488_SendCommand(0x36);      //Memory Access
	ILI9488_SendData(0x48);

	ILI9488_SendCommand(0x3A);      // Interface Pixel Format
	ILI9488_SendData(0x66); 	  	//18 bit
//	ILI9488_SendData(0x55); 	  	//16 bit

	ILI9488_SendCommand(0XB0);      // Interface Mode Control
	ILI9488_SendData(0x80);     	//SDO NOT USE

	ILI9488_SendCommand(0xB1);      //Frame rate
	ILI9488_SendData(0xA0);    		//60Hz

	ILI9488_SendCommand(0xB4);      //Display Inversion Control
	ILI9488_SendData(0x02);    		//2-dot

	ILI9488_SendCommand(0XB6); 		//Display Function Control  RGB/MCU Interface Control

	ILI9488_SendData(0x02);    		//MCU
	ILI9488_SendData(0x02);    		//Source,Gate scan dieection

	ILI9488_SendCommand(0XE9);      // Set Image Functio
	ILI9488_SendData(0x00);    		// Disable 24 bit data

	ILI9488_SendCommand(0xF7);      // Adjust Control
	ILI9488_SendData(0xA9);
	ILI9488_SendData(0x51);
	ILI9488_SendData(0x2C);
	ILI9488_SendData(0x82);    		// D7 stream, loose

	ILI9488_SendCommand(ILI9488_SLPOUT);    //Exit Sleep

	HAL_Delay(120);

	ILI9488_SendCommand(ILI9488_DISPON);    //Display on

}

void setAddrWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
	/*ILI9488_SendCommand(ILI9488_CASET); // Column addr set
	ILI9488_SendData(x0 >> 8);
	ILI9488_SendData(x0 & 0xFF);     // XSTART
	ILI9488_SendData(x1 >> 8);
	ILI9488_SendData(x1 & 0xFF);     // XEND
	ILI9488_SendCommand(ILI9488_PASET); // Row addr set
	ILI9488_SendData(y0 >> 8);
	ILI9488_SendData(y0 & 0xff);     // YSTART
	ILI9488_SendData(y1 >> 8);
	ILI9488_SendData(y1 & 0xff);     // YEND
	ILI9488_SendCommand(ILI9488_RAMWR); // write to RAM*/
	ILI9488_SendCommand(ILI9488_CASET); // Column addr set
	{
		uint8_t data[] = {(x0 >> 8) & 0xFF, x0 & 0xFF, (x1 >> 8) & 0xFF, x1 & 0xFF};
		ILI9488_SendData_Multi(data, sizeof(data));
	}
	ILI9488_SendCommand(ILI9488_PASET);
	{
		uint8_t data[] = {(y0 >> 8) & 0xFF, y0 & 0xFF, (y1 >> 8) & 0xFF, y1 & 0xFF};
		ILI9488_SendData_Multi(data, sizeof(data));
	}
	ILI9488_SendCommand(ILI9488_RAMWR); // write to RAM*/
}

void setScrollArea(uint16_t topFixedArea, uint16_t bottomFixedArea)
{
	ILI9488_SendCommand(0x33); // Vertical scroll definition
	ILI9488_SendData(topFixedArea >> 8);
	ILI9488_SendData(topFixedArea);
	ILI9488_SendData((height - topFixedArea - bottomFixedArea) >> 8);
	ILI9488_SendData(height - topFixedArea - bottomFixedArea);
	ILI9488_SendData(bottomFixedArea >> 8);
	ILI9488_SendData(bottomFixedArea);
}

void scroll(uint16_t pixels)
{
	ILI9488_SendCommand(0x37); // Vertical scrolling start address
	ILI9488_SendData(pixels >> 8);
	ILI9488_SendData(pixels);
}
void pushColor(uint16_t color)
{
	HAL_GPIO_WritePin(tftDC_GPIO, tftDC_PIN, GPIO_PIN_SET);
	HAL_GPIO_WritePin(tftCS_GPIO, tftCS_PIN, GPIO_PIN_RESET);
	write16BitColor(color);
	HAL_GPIO_WritePin(tftCS_GPIO, tftCS_PIN, GPIO_PIN_SET);

}

void pushColors(uint16_t *data, uint8_t len, uint8_t first)
{
	uint16_t color;
	uint8_t buff[len * 3 + 1];
	uint16_t count = 0;
	uint8_t lencount = len;
	HAL_GPIO_WritePin(tftCS_GPIO, tftCS_PIN, GPIO_PIN_RESET);

	if (first == 1) {
		HAL_GPIO_WritePin(tftDC_GPIO, tftDC_PIN, GPIO_PIN_SET);
	}
	while (lencount--)
	{
		color = *data++;
		buff[count] = (((color & 0xF800) >> 11) * 255) / 31;
		count++;
		buff[count] = (((color & 0x07E0) >> 5) * 255) / 63;
		count++;
		buff[count] = ((color & 0x001F) * 255) / 31;
		count++;
	}
	HAL_SPI_Transmit(&lcdSPIhandle, buff, len * 3, 100);
	HAL_GPIO_WritePin(tftCS_GPIO, tftCS_PIN, GPIO_PIN_SET);
}

void drawImage(const uint8_t* img, uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{

	if ((x >= width) || (y >= height))
		return;
	if ((x + w - 1) >= width)
		w = width - x;
	if ((y + h - 1) >= height)
		h = height - y;
	setAddrWindow(x, y, x + w - 1, y + h - 1);
	HAL_GPIO_WritePin(tftDC_GPIO, tftDC_PIN, GPIO_PIN_SET);
	HAL_GPIO_WritePin(tftCS_GPIO, tftCS_PIN, GPIO_PIN_RESET);

	uint8_t linebuff[w * 3 + 1];
	uint32_t count = 0;
	for (uint16_t i = 0; i < h; i++) {
		uint16_t pixcount = 0;
		for (uint16_t o = 0; o < w; o++) {
			uint8_t b1 = img[count];
			count++;
			uint8_t b2 = img[count];
			count++;
			uint16_t color = b1 << 8 | b2;
			linebuff[pixcount] = (((color & 0xF800) >> 11) * 255)
					/ 31;
			pixcount++;
			linebuff[pixcount] = (((color & 0x07E0) >> 5) * 255)
					/ 63;
			pixcount++;
			linebuff[pixcount] = ((color & 0x001F) * 255) / 31;
			pixcount++;
		}
		HAL_SPI_Transmit(&lcdSPIhandle, linebuff, w * 3, 100);

	}

	HAL_GPIO_WritePin(tftCS_GPIO, tftCS_PIN, GPIO_PIN_SET);
}


void drawPixel(int16_t x, int16_t y, uint16_t color)
{
	if ((x < 0) || (x >= width) || (y < 0) || (y >= height))
		return;

	setAddrWindow(x, y, x + 1, y + 1);
	HAL_GPIO_WritePin(tftDC_GPIO, tftDC_PIN, GPIO_PIN_SET);
	HAL_GPIO_WritePin(tftCS_GPIO, tftCS_PIN, GPIO_PIN_RESET);

	write16BitColor(color);
	HAL_GPIO_WritePin(tftCS_GPIO, tftCS_PIN, GPIO_PIN_SET);

}

void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color)
{

	if ((x >= width) || (y >= height))
		return;

	if ((y + h - 1) >= height)
		h = height - y;

	setAddrWindow(x, y, x, y + h - 1);
	HAL_GPIO_WritePin(tftDC_GPIO, tftDC_PIN, GPIO_PIN_SET);
	HAL_GPIO_WritePin(tftCS_GPIO, tftCS_PIN, GPIO_PIN_RESET);

	while (h--) {
		write16BitColor(color);
	}
	HAL_GPIO_WritePin(tftCS_GPIO, tftCS_PIN, GPIO_PIN_SET);

}

void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color)
{

	if ((x >= width) || (y >= height))
		return;
	if ((x + w - 1) >= width)
		w = width - x;
	setAddrWindow(x, y, x + w - 1, y);
	HAL_GPIO_WritePin(tftDC_GPIO, tftDC_PIN, GPIO_PIN_SET);
	HAL_GPIO_WritePin(tftCS_GPIO, tftCS_PIN, GPIO_PIN_RESET);
	while (w--)
	{
		write16BitColor(color);
	}
	HAL_GPIO_WritePin(tftCS_GPIO, tftCS_PIN, GPIO_PIN_SET);
}

void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color)
{
	 if (x0 == x1) {
	    if (y0 > y1)
	      swap(y0, y1);
	    drawFastVLine(x0, y0, y1 - y0 + 1, color);
	  } else if (y0 == y1) {
	    if (x0 > x1)
	      swap(x0, x1);
	    drawFastHLine(x0, y0, x1 - x0 + 1, color);
	  } else {

	    writeLine(x0, y0, x1, y1, color);

	  }

}

void writeLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) {

  int16_t steep = abs(y1 - y0) > abs(x1 - x0);
  if (steep) {
    swap(x0, y0);
    swap(x1, y1);
  }

  if (x0 > x1) {
	  swap(x0, x1);
	  swap(y0, y1);
  }

  int16_t dx, dy;
  dx = x1 - x0;
  dy = abs(y1 - y0);

  int16_t err = dx / 2;
  int16_t ystep;

  if (y0 < y1) {
    ystep = 1;
  } else {
    ystep = -1;
  }

  for (; x0 <= x1; x0++) {
    if (steep) {
      drawPixel(y0, x0, color);
    } else {
      drawPixel(x0, y0, color);
    }
    err -= dy;
    if (err < 0) {
      y0 += ystep;
      err += dx;
    }
  }
}
//6. Fill the entire screen with a background color

void fillScreen(uint16_t color)
{
	fillRect(0, 0,  width, height, color);
}

void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color)
{

	uint32_t i, n, cnt, buf_size;
	if ((x >= width) || (y >= height))
		return;
	if ((x + w - 1) >= width)
		w = width - x;
	if ((y + h - 1) >= height)
		h = height - y;
	setAddrWindow(x, y, x + w - 1, y + h - 1);
	uint8_t r = (color & 0xF800) >> 11;
	uint8_t g = (color & 0x07E0) >> 5;
	uint8_t b = color & 0x001F;

	r = (r * 255) / 31;
	g = (g * 255) / 63;
	b = (b * 255) / 31;

	n = w*h*3;
	if (n <= 65535){
		cnt = 1;
		buf_size = n;
	}
	else {
		cnt = n/3;
		buf_size = 3;
		uint8_t min_cnt = n/65535+1;
		for (i=min_cnt; i < n/3; i++){
			if(n%i == 0){
				cnt = i;
				buf_size = n/i;
				break;
			}
		}
	}
	uint8_t frm_buf[buf_size];
	for (i=0; i < buf_size/3; i++)
	{
		frm_buf[i*3] = r;
		frm_buf[i*3+1] = g;
		frm_buf[i*3+2] = b;
	}
	DC_DATA();
	CS_A();
		while(cnt>0)
		{
			HAL_SPI_Transmit(&hspi3, frm_buf, buf_size, HAL_MAX_DELAY);

			cnt -= 1;
		}
		CS_D();

}


void setRotation(uint8_t r)
{

	ILI9488_SendCommand(ILI9488_MADCTL);
	uint8_t rotation = r % 4; // can't be higher than 3
	switch (rotation) {
	case 0:
		ILI9488_SendData(MADCTL_MX | MADCTL_BGR);
		width = ILI9488_TFTWIDTH;
		height = ILI9488_TFTHEIGHT;
		break;
	case 1:
		ILI9488_SendData(MADCTL_MV | MADCTL_BGR);
		width = ILI9488_TFTHEIGHT;
		height = ILI9488_TFTWIDTH;
		break;
	case 2:
		ILI9488_SendData(MADCTL_MY | MADCTL_BGR);
		width = ILI9488_TFTWIDTH;
		height = ILI9488_TFTHEIGHT;
		break;
	case 3:
		ILI9488_SendData(MADCTL_MX | MADCTL_MY | MADCTL_MV | MADCTL_BGR);
		width = ILI9488_TFTHEIGHT;
		height = ILI9488_TFTWIDTH;
		break;
	}

}

void invertDisplay(uint8_t i)
{

	ILI9488_SendCommand(i ? ILI9488_INVON : ILI9488_INVOFF);

}
uint16_t color565(uint8_t r, uint8_t g, uint8_t b)
{
	return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

//11. Text printing functions
//void drawChar(int16_t x, int16_t y, unsigned char c, uint16_t color, uint16_t bg, uint8_t size)
//{
//	if(rotationNum == 1 || rotationNum ==3)
//	{
//		if((x >= ILI9488_TFTWIDTH)            || // Clip right
//     (y >= ILI9488_TFTHEIGHT)           || // Clip bottom
//     ((x + 6 * size - 1) < 0) || // Clip left
//     ((y + 8 * size - 1) < 0))   // Clip top
//    return;
//	}
//	else
//	{
//		if((y >= ILI9488_TFTWIDTH)            || // Clip right
//     (x >= ILI9488_TFTHEIGHT)           || // Clip bottom
//     ((y + 6 * size - 1) < 0) || // Clip left
//     ((x + 8 * size - 1) < 0))   // Clip top
//    return;
//	}
//
//
//  if(!_cp437 && (c >= 176)) c++; // Handle 'classic' charset behavior
//
//  for (int8_t i=0; i<6; i++ ) {
//    uint8_t line;
//    if (i == 5)
//      line = 0x0;
//    else
//      line = pgm_read_byte(font1+(c*5)+i);
//    for (int8_t j = 0; j<8; j++) {
//      if (line & 0x1) {
//        if (size == 1) // default size
//        	drawPixel(x+i, y+j, color);
//        else {  // big size
//        	fillRect(x+(i*size), y+(j*size), size + x+(i*size), size+1 + y+(j*size), color);
//        }
//      } else if (bg != color) {
//        if (size == 1) // default size
//        	drawPixel(x+i, y+j, bg);
//        else {  // big size
//        	fillRect(x+i*size, y+j*size, size + x+i*size, size+1 + y+j*size, bg);
//        }
//      }
//      line >>= 1;
//    }
//  }
//}
void ILI9488_printText(char text[], int16_t x, int16_t y, uint16_t color, uint16_t bg, uint8_t size)
{
	int16_t offset;
	offset = size*6;

	for(uint16_t i=0; i<40 && text[i]!=NULL; i++)
	{
		drawChar(x+(offset*i), y, text[i],color,bg,size);
	}
}
void testLines(uint8_t color)
{

	unsigned long start, t;
	int x1, y1, x2, y2, w = width, h = height;
	fillScreen(ILI9488_BLACK);

	x1 = y1 = 0;
	y2 = h - 1;

	for (x2 = 0; x2 < w; x2 += 6)
		drawLine(x1, y1, x2, y2, color);
	x2 = w - 1;
	for (y2 = 0; y2 < h; y2 += 6)
		drawLine(x1, y1, x2, y2, color);
	fillScreen(ILI9488_BLACK);

	x1 = w - 1;
	y1 = 0;
	y2 = h - 1;

	for (x2 = 0; x2 < w; x2 += 6)
		drawLine(x1, y1, x2, y2, color);
	x2 = 0;
	for (y2 = 0; y2 < h; y2 += 6)
		drawLine(x1, y1, x2, y2, color);

	fillScreen(ILI9488_BLACK);

	x1 = 0;
	y1 = h - 1;
	y2 = 0;

	for (x2 = 0; x2 < w; x2 += 6)
		drawLine(x1, y1, x2, y2, color);
	x2 = w - 1;
	for (y2 = 0; y2 < h; y2 += 6)
		drawLine(x1, y1, x2, y2, color);

	fillScreen(ILI9488_BLACK);

	x1 = w - 1;
	y1 = h - 1;
	y2 = 0;

	for (x2 = 0; x2 < w; x2 += 6)
		drawLine(x1, y1, x2, y2, color);
	x2 = 0;
	for (y2 = 0; y2 < h; y2 += 6)
		drawLine(x1, y1, x2, y2, color);
}

void write16BitColor(uint16_t color)
{

	  uint8_t r = (color & 0xF800) >> 11;
	  uint8_t g = (color & 0x07E0) >> 5;
	  uint8_t b = color & 0x001F;

	  r = (r * 255) / 31;
	  g = (g * 255) / 63;
	  b = (b * 255) / 31;
	  uint8_t data[3] = {r, g, b};
	  ILI9488_SendData_Multi(data, 3);
	  //HAL_SPI_Transmit(&hspi3, (uint8_t *)&r, 1, 10);
	  //HAL_SPI_Transmit(&hspi3, (uint8_t *)&g, 1, 10);
	  //HAL_SPI_Transmit(&hspi3, (uint8_t *)&b, 1, 10);

}

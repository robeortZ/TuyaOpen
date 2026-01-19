/*****************************************************************************
* | File      	:	EPD_7in5.c
* | Author      :   Waveshare team
* | Function    :   Electronic paper driver
* | Info        :
*----------------
* |	This version:   V3.0
* | Date        :   2023-12-18
* | Info        :
*****************************************************************************
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documnetation files(the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to  whom the Software is
# furished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS OR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
#
******************************************************************************/
#include "__EPD_7in5_V2.h"
#include "tal_log.h"
#include "tdd_display_spi.h"
#include "tkl_gpio.h"
#include "tkl_system.h"

#define SPI_PORT spi1

/**
 * GPIO
**/
int EPD_RST_PIN;
int EPD_DC_PIN;
int EPD_CS_PIN;
int EPD_BUSY_PIN;
int EPD_CLK_PIN;
int EPD_MOSI_PIN;

DISP_SPI_BASE_CFG_T *disp_spi_cfg = NULL;
/**
 * GPIO read and write
**/
void DEV_Digital_Write(uint16_t Pin, uint8_t Value)
{
	tkl_gpio_write(Pin, Value);
}

uint8_t DEV_Digital_Read(uint16_t Pin)
{
    TUYA_GPIO_LEVEL_E level;
	
    tkl_gpio_read(Pin,&level);
    return (level == TUYA_GPIO_LEVEL_HIGH) ? 1 : 0;
}


/**
 * GPIO Mode
**/
void DEV_GPIO_Mode(uint16_t Pin, uint16_t Mode)
{
    OPERATE_RET rt = OPRT_OK;

    /*GPIO  init*/
    TUYA_GPIO_BASE_CFG_T pin_cfg = {
        .mode = TUYA_GPIO_PULLUP,
        .direct = TUYA_GPIO_INPUT,
    };
   
        if(Mode == 0) {
		pin_cfg.direct = TUYA_GPIO_INPUT;
	} else {
		pin_cfg.direct = TUYA_GPIO_OUTPUT;
	}
    TUYA_CALL_ERR_LOG(tkl_gpio_init(Pin, &pin_cfg));
}

/**
 * delay x ms
**/
void DEV_Delay_ms(uint32_t xms)
{
	tkl_system_sleep(xms);
}

void DEV_GPIO_Init(void)
{

	EPD_RST_PIN     = TUYA_GPIO_NUM_12;
	EPD_DC_PIN      = TUYA_GPIO_NUM_18;
	EPD_BUSY_PIN    = TUYA_GPIO_NUM_13;
	
	EPD_CS_PIN      = TUYA_GPIO_NUM_15;
	EPD_CLK_PIN		= TUYA_GPIO_NUM_14;
	EPD_MOSI_PIN	= TUYA_GPIO_NUM_16;

	DEV_GPIO_Mode(EPD_RST_PIN, 1);
	DEV_GPIO_Mode(EPD_DC_PIN, 1);
	DEV_GPIO_Mode(EPD_CS_PIN, 1);
	DEV_GPIO_Mode(EPD_BUSY_PIN, 0);

	DEV_Digital_Write(EPD_CS_PIN, 1);
}
/******************************************************************************
function:	Module Initialize, the library and initialize the pins, SPI protocol
parameter:
Info:
******************************************************************************/
uint8_t DEV_Module_Init(void)
{
    // stdio_init_all(); // Not needed for this platform

	// GPIO Config
	DEV_GPIO_Init();
	
    // spi_init(SPI_PORT, 4000 * 1000);
    // gpio_set_function(EPD_CLK_PIN, GPIO_OUT);
    // gpio_set_function(EPD_MOSI_PIN, GPIO_OUT);
	
    PR_DEBUG("DEV_Module_Init OK \r\n");
	return 0;
}



/******************************************************************************
function:	Module exits, closes SPI and BCM2835 library
parameter:
Info:
******************************************************************************/
void DEV_Module_Exit(void)
{

}

/******************************************************************************
function :	Software reset
parameter:
******************************************************************************/
static void EPD_Reset(void)
{
    DEV_Digital_Write(EPD_RST_PIN, 1);
    DEV_Delay_ms(20);
    DEV_Digital_Write(EPD_RST_PIN, 0);
    DEV_Delay_ms(2);
    DEV_Digital_Write(EPD_RST_PIN, 1);
    DEV_Delay_ms(20);
}

/******************************************************************************
function :	send command
parameter:
     Reg : Command register
******************************************************************************/
static void EPD_SendCommand(uint8_t Reg)
{
    tdd_disp_spi_send_cmd(disp_spi_cfg, Reg);   
}

/******************************************************************************
function :	send data
parameter:
    Data : Write data
******************************************************************************/
static void EPD_SendData(uint8_t Data)
{
    tdd_disp_spi_send_data(disp_spi_cfg, &Data, 1);

}

static void EPD_SendData2(uint8_t *pData, uint32_t len)
{
    tdd_disp_spi_send_data(disp_spi_cfg, pData, len);
}

/******************************************************************************
function :	Wait until the busy_pin goes LOW
parameter:
******************************************************************************/
static void EPD_WaitUntilIdle(void)
{
    PR_DEBUG("e-Paper busy\r\n");
	do{
		DEV_Delay_ms(5);  
	}while(!(DEV_Digital_Read(EPD_BUSY_PIN)));   
	DEV_Delay_ms(5);      
    PR_DEBUG("e-Paper busy release\r\n");
}
/******************************************************************************
function :	Turn On Display
parameter:
******************************************************************************/
static void EPD_7IN5_V2_TurnOnDisplay(void)
{	
    EPD_SendCommand(0x12);			//DISPLAY REFRESH
    DEV_Delay_ms(100);	        //!!!The delay here is necessary, 200uS at least!!!
    EPD_WaitUntilIdle();
}

/******************************************************************************
function :	Initialize the e-Paper register
parameter:
******************************************************************************/
uint8_t EPD_7IN5_V2_Init(DISP_SPI_BASE_CFG_T *p_cfg)
{
    EPD_Reset();
    EPD_SendCommand(0x01);			//POWER SETTING
	EPD_SendData(0x07);
	EPD_SendData(0x07);    //VGH=20V,VGL=-20V
	EPD_SendData(0x3f);		//VDH=15V
	EPD_SendData(0x3f);		//VDL=-15V

	//Enhanced display drive(Add 0x06 command)
	EPD_SendCommand(0x06);			//Booster Soft Start 
	EPD_SendData(0x17);
	EPD_SendData(0x17);   
	EPD_SendData(0x28);		
	EPD_SendData(0x17);	

	EPD_SendCommand(0x04); //POWER ON
	DEV_Delay_ms(100); 
	EPD_WaitUntilIdle();        //waiting for the electronic paper IC to release the idle signal

	EPD_SendCommand(0X00);			//PANNEL SETTING
	EPD_SendData(0x1F);   //KW-3f   KWR-2F	BWROTP 0f	BWOTP 1f

	EPD_SendCommand(0x61);        	//tres			
	EPD_SendData(0x03);		//source 800
	EPD_SendData(0x20);
	EPD_SendData(0x01);		//gate 480
	EPD_SendData(0xE0);  

	EPD_SendCommand(0X15);		
	EPD_SendData(0x00);		

	/*
        If the screen appears gray, use the annotated initialization command
    */
    EPD_SendCommand(0X50);			
	EPD_SendData(0x10);
	EPD_SendData(0x07);
	// EPD_SendCommand(0X50);			
	// EPD_SendData(0x10);
	// EPD_SendData(0x17);
    // EPD_SendCommand(0X52);			
	// EPD_SendData(0x03);
	EPD_SendCommand(0X60);			//TCON SETTING
	EPD_SendData(0x22);
	
    disp_spi_cfg = p_cfg;
    return 0;
}

uint8_t EPD_7IN5_V2_Init_Fast(void)
{
    EPD_Reset();
    EPD_SendCommand(0X00);			//PANNEL SETTING
    EPD_SendData(0x1F);   //KW-3f   KWR-2F	BWROTP 0f	BWOTP 1f

    /*
        If the screen appears gray, use the annotated initialization command
    */
    EPD_SendCommand(0X50);			
	EPD_SendData(0x10);
	EPD_SendData(0x07);
	// EPD_SendCommand(0X50);			
	// EPD_SendData(0x10);
	// EPD_SendData(0x17);
    // EPD_SendCommand(0X52);			
	// EPD_SendData(0x03);

    EPD_SendCommand(0x04); //POWER ON
    DEV_Delay_ms(100); 
	EPD_WaitUntilIdle();        //waiting for the electronic paper IC to release the idle signal

    //Enhanced display drive(Add 0x06 command)
    EPD_SendCommand(0x06);			//Booster Soft Start 
    EPD_SendData (0x27);
    EPD_SendData (0x27);   
    EPD_SendData (0x18);		
    EPD_SendData (0x17);		

    EPD_SendCommand(0xE0);
    EPD_SendData(0x02);
    EPD_SendCommand(0xE5);
    EPD_SendData(0x5A);
	
    return 0;
}

uint8_t EPD_7IN5_V2_Init_Part(void)
{
    EPD_Reset();

	EPD_SendCommand(0X00);			//PANNEL SETTING
	EPD_SendData(0x1F);   //KW-3f   KWR-2F	BWROTP 0f	BWOTP 1f
	
	EPD_SendCommand(0x04); //POWER ON
	DEV_Delay_ms(100); 
	EPD_WaitUntilIdle();        //waiting for the electronic paper IC to release the idle signal
	
	EPD_SendCommand(0xE0);
	EPD_SendData(0x02);
	EPD_SendCommand(0xE5);
	EPD_SendData(0x6E);
	
    return 0;
}

/*
    The feature will only be available on screens sold after 24/10/23
*/
uint8_t EPD_7IN5_V2_Init_4Gray(DISP_SPI_BASE_CFG_T *p_cfg)
{
    EPD_Reset();

	EPD_SendCommand(0X00);			//PANNEL SETTING
	EPD_SendData(0x1F);   //KW-3f   KWR-2F	BWROTP 0f	BWOTP 1f

    EPD_SendCommand(0X50);			
	EPD_SendData(0x10);
	EPD_SendData(0x07);
	
	EPD_SendCommand(0x04); //POWER ON
	DEV_Delay_ms(100); 
	EPD_WaitUntilIdle();        //waiting for the electronic paper IC to release the idle signal
	
    EPD_SendCommand(0x06);			//Booster Soft Start 
    EPD_SendData (0x27);
    EPD_SendData (0x27);   
    EPD_SendData (0x18);		
    EPD_SendData (0x17);		

	EPD_SendCommand(0xE0);
	EPD_SendData(0x02);
	EPD_SendCommand(0xE5);
	EPD_SendData(0x5F);
	disp_spi_cfg = p_cfg;
    return 0;
}

/******************************************************************************
function :	Clear screen
parameter:
******************************************************************************/
void EPD_7IN5_V2_Clear(void)
{
    uint16_t Width, Height;
    Width =(EPD_7IN5_V2_WIDTH % 8 == 0)?(EPD_7IN5_V2_WIDTH / 8 ):(EPD_7IN5_V2_WIDTH / 8 + 1);
    Height = EPD_7IN5_V2_HEIGHT;
    uint8_t image[EPD_7IN5_V2_WIDTH / 8] = {0x00};

    uint16_t i;
    EPD_SendCommand(0x10);
    for(i=0; i<Width; i++) {
        image[i] = 0xFF;
    }
    for(i=0; i<Height; i++)
    {
        EPD_SendData2(image, Width);
    }

    EPD_SendCommand(0x13);
    for(i=0; i<Width; i++) {
        image[i] = 0x00;
    }
    for(i=0; i<Height; i++)
    {
        EPD_SendData2(image, Width);
    }
    
    EPD_7IN5_V2_TurnOnDisplay();
}

void EPD_7IN5_V2_ClearBlack(void)
{
    uint16_t Width, Height;
    Width =(EPD_7IN5_V2_WIDTH % 8 == 0)?(EPD_7IN5_V2_WIDTH / 8 ):(EPD_7IN5_V2_WIDTH / 8 + 1);
    Height = EPD_7IN5_V2_HEIGHT;
    uint8_t image[EPD_7IN5_V2_WIDTH / 8] = {0x00};

    uint16_t i;
    EPD_SendCommand(0x10);
    for(i=0; i<Width; i++) {
        image[i] = 0x00;
    }
    for(i=0; i<Height; i++)
    {
        EPD_SendData2(image, Width);
    }

    EPD_SendCommand(0x13);
    for(i=0; i<Width; i++) {
        image[i] = 0xFF;
    }
    for(i=0; i<Height; i++)
    {
        EPD_SendData2(image, Width);
    }
    
    EPD_7IN5_V2_TurnOnDisplay();
}

/******************************************************************************
function :	Sends the image buffer in RAM to e-Paper and displays
parameter:
******************************************************************************/
void EPD_7IN5_V2_Display(uint8_t *blackimage)
{
    uint32_t Width, Height;
    Width =(EPD_7IN5_V2_WIDTH % 8 == 0)?(EPD_7IN5_V2_WIDTH / 8 ):(EPD_7IN5_V2_WIDTH / 8 + 1);
    Height = EPD_7IN5_V2_HEIGHT;
	
    EPD_SendCommand(0x10);
    for (uint32_t j = 0; j < Height; j++) {
        EPD_SendData2((uint8_t *)(blackimage+j*Width), Width);
    }

    EPD_SendCommand(0x13);
    for (uint32_t j = 0; j < Height; j++) {
        for (uint32_t i = 0; i < Width; i++) {
            blackimage[i + j * Width] = ~blackimage[i + j * Width];
        }
    }
    for (uint32_t j = 0; j < Height; j++) {
        EPD_SendData2((uint8_t *)(blackimage+j*Width), Width);
    }
    EPD_7IN5_V2_TurnOnDisplay();
}

void EPD_7IN5_V2_Display_Part(uint8_t *blackimage,uint32_t x_start, uint32_t y_start, uint32_t x_end, uint32_t y_end)
{
    uint32_t Width, Height;
    Width =((x_end - x_start) % 8 == 0)?((x_end - x_start) / 8 ):((x_end - x_start) / 8 + 1);
    Height = y_end - y_start;

    EPD_SendCommand(0x50);
	EPD_SendData(0xA9);
	EPD_SendData(0x07);

	EPD_SendCommand(0x91);		//This command makes the display enter partial mode
	EPD_SendCommand(0x90);		//resolution setting
	EPD_SendData (x_start/256);
	EPD_SendData (x_start%256);   //x-start    

	EPD_SendData (x_end/256);		
	EPD_SendData (x_end%256-1);  //x-end	

	EPD_SendData (y_start/256);  //
	EPD_SendData (y_start%256);   //y-start    

	EPD_SendData (y_end/256);		
	EPD_SendData (y_end%256-1);  //y-end
	EPD_SendData (0x01);
    
    EPD_SendCommand(0x13);
    for (uint32_t j = 0; j < Height; j++) {
        EPD_SendData2((uint8_t *)(blackimage+j*Width), Width);
    }
    EPD_7IN5_V2_TurnOnDisplay();
}

void EPD_7IN5_V2_Display_4Gray(const uint8_t *Image)
{
    uint32_t i,j,k;
    uint8_t temp1,temp2,temp3;

    // old  data
    EPD_SendCommand(0x10);
    for(i=0; i<48000; i++) {
        temp3=0;
        for(j=0; j<2; j++) {
            temp1 = Image[i*2+j];
            for(k=0; k<2; k++) {
                temp2 = temp1&0xC0;
                if(temp2 == 0xC0)
                    temp3 |= 0x00;
                else if(temp2 == 0x00)
                    temp3 |= 0x01; 
                else if(temp2 == 0x80)
                    temp3 |= 0x01; 
                else //0x40
                    temp3 |= 0x00; 
                temp3 <<= 1;

                temp1 <<= 2;
                temp2 = temp1&0xC0 ;
                if(temp2 == 0xC0) 
                    temp3 |= 0x00;
                else if(temp2 == 0x00) 
                    temp3 |= 0x01;
                else if(temp2 == 0x80)
                    temp3 |= 0x01; 
                else    //0x40
                    temp3 |= 0x00;	
                if(j!=1 || k!=1)
                    temp3 <<= 1;

                temp1 <<= 2;
            }

        }
        EPD_SendData(temp3);
        // PR_DEBUG("%x",temp3);
    }

    EPD_SendCommand(0x13);   //write RAM for black(0)/white (1)
    for(i=0; i<48000; i++) {             //5808*4  46464
        temp3=0;
        for(j=0; j<2; j++) {
            temp1 = Image[i*2+j];
            for(k=0; k<2; k++) {
                temp2 = temp1&0xC0 ;
                if(temp2 == 0xC0)
                    temp3 |= 0x00;//white
                else if(temp2 == 0x00)
                    temp3 |= 0x01;  //black
                else if(temp2 == 0x80)
                    temp3 |= 0x00;  //gray1
                else //0x40
                    temp3 |= 0x01; //gray2
                temp3 <<= 1;

                temp1 <<= 2;
                temp2 = temp1&0xC0 ;
                if(temp2 == 0xC0)  //white
                    temp3 |= 0x00;
                else if(temp2 == 0x00) //black
                    temp3 |= 0x01;
                else if(temp2 == 0x80)
                    temp3 |= 0x00; //gray1
                else    //0x40
                    temp3 |= 0x01;	//gray2
                if(j!=1 || k!=1)
                    temp3 <<= 1;

                temp1 <<= 2;
            }
        }
        EPD_SendData(temp3);
        // PR_DEBUG("%x",temp3);
    }

    EPD_7IN5_V2_TurnOnDisplay();
}

/******************************************************************************
function :	Enter sleep mode
parameter:
******************************************************************************/
void EPD_7IN5_V2_Sleep(void)
{
    EPD_SendCommand(0x50);  	
    EPD_SendData(0XF7);
    EPD_SendCommand(0X02);  	//power off
    EPD_WaitUntilIdle();
    EPD_SendCommand(0X07);  	//deep sleep
    EPD_SendData(0xA5);
}

/*****************************************************************************
 * @file     senxorTask.c
 * @version  2.01
 * @brief    FreeRTOS task for interfacing with SenXor
 * @date	 11 Jul 2022
 ******************************************************************************/
#include <esp_log.h>				//ESP logger
#include "Customer_Interface.h"
#include "DrvLED.h"
#include "MCU_Dependent.h"
#include "esp_err.h"
#include "esp_mac.h"
#include "esp_system.h"
#include "msg.h"					//Messages
#include "SenXorLib.h"				//SenXor library
#include "SenXor_Capturedata.h"		//Interrupt handler
#include "senxorTask.h"
#include "util.h"
#include "ledCtrlTask.h"			//LED control task

//public:
EXT_RAM_BSS_ATTR uint16_t CalibData_BufferData[CALIBDATA_FLASH_SIZE];			//Array to hold the calibration data
EXT_RAM_BSS_ATTR QueueHandle_t senxorFrameQueue = NULL;

TaskHandle_t senxorTaskHandle = NULL;
//private:
EXT_RAM_BSS_ATTR static senxorFrame mLatestFrameObj;
static SemaphoreHandle_t latestFrameMutex = NULL;
static uint32_t latestFrameNumber = 0;
static bool latestFrameValid = false;
static void senxorTask_Init(void);


/*
 * ***********************************************************************
 * @brief       senxorTask_GetLatestFrame - Thread-safe frame access for web/API
 * @param       frame[out] - buffer to copy frame into
 * @param       framePixelCount - number of pixels to copy
 * @param       frameNumber[out] - optional monotonic frame counter
 * @return      true if a valid frame was copied
 *****************************************************************************/
bool senxorTask_GetLatestFrame(uint16_t *frame, size_t framePixelCount, uint32_t *frameNumber)
{
    if (latestFrameMutex == NULL) return false;
    xSemaphoreTake(latestFrameMutex, portMAX_DELAY);
    const bool isValid = latestFrameValid;
    if (isValid) {
        size_t copyPixels = (framePixelCount < 5040) ? framePixelCount : 5040;
        memcpy(frame, mLatestFrameObj.mFrame, copyPixels * sizeof(uint16_t));
        if (frameNumber) *frameNumber = latestFrameNumber;
    }
    xSemaphoreGive(latestFrameMutex);
    return isValid;
}

bool senxorTask_GetLatestFrameNumber(uint32_t *frameNumber)
{
    if (latestFrameMutex == NULL || frameNumber == NULL) return false;
    xSemaphoreTake(latestFrameMutex, portMAX_DELAY);
    const bool isValid = latestFrameValid;
    if (isValid) *frameNumber = latestFrameNumber;
    xSemaphoreGive(latestFrameMutex);
    return isValid;
}


/*
 * ***********************************************************************
 * @brief       senxorInit
 * @param       None
 * @return      None
 * @details     Initialise SenXor
 **************************************************************************/
uint8_t senxorInit(void)
{

	Initialize_McuRegister();																//Initialise SenXor software registers

	Power_On_Senxor(1);																//Power up SenXor and determine its model

	if(Initialize_SenXor(1))													//Initialise SenXor peripherals.
	{
#ifdef CONFIG_MI_LCD_EN
		drawIcon(96,76,ICON_ERROR);
		drawText(1,76+48,"Failed to initialise \nSenXor. Program halted.");
#endif
		return 1; 																			//Exit program immediately if failed.
	}//End if

	Read_CalibrationData();																	//Load calibration data from flash
	ESP_LOGI(SXRTAG,SXR_PROCESS_CALI);
	Process_CalibrationData(1,(uint16_t*)CalibData_BufferData);		//Process calibration data
	ESP_LOGI(SXRTAG,SXR_FITLER_INIT);
	Initialize_Filter();																	//Initialise filters
	Read_AGC_LUT();																			//Read auto gain

	/*
	 * If TCP server is used, TCP server should be started BEFORE SenXor starting capture.
	 */
	Acces_Write_Reg(0xB1,0);													//Stop capturing

	ESP_LOGI(SXRTAG,SXR_INIT_DONE);

	return 0;
}

/*
 * ***********************************************************************
 * @brief       senxorTask
 * @param       pvParameters - Task arguments
 * @return      None
 * @details     SenXor task
 **************************************************************************/
void senxorTask(void * pvParameters)
{
	uint8_t tOpMode = MCU_getOpMode();
	ESP_LOGI(SXRTAG,SXR_TASK_INFO,xPortGetCoreID());
	ESP_LOGI(SXRTAG,MAIN_FREE_RAM " / " MAIN_TOTAL_RAM,heap_caps_get_free_size(MALLOC_CAP_INTERNAL), heap_caps_get_total_size(MALLOC_CAP_INTERNAL));				//Display the total amount of DRAM
	ESP_LOGI(SXRTAG,MAIN_FREE_SPIRAM " / " MAIN_TOTAL_SPIRAM,heap_caps_get_free_size(MALLOC_CAP_SPIRAM), heap_caps_get_total_size(MALLOC_CAP_SPIRAM));				//Display the total amount of PSRAM

	senxorTask_Init();																																					//Initialise queue

	/* Delay capture start to let WiFi stabilize and avoid brownout */
	vTaskDelay(pdMS_TO_TICKS(3000));

	/* Start continuous capture (original behavior) */
	Acces_Write_Reg(0xB1, B1_SINGLE_CONT);
	ESP_LOGI(SXRTAG, "Continuous capture started");

	for(;;)
	{
		uint8_t regB1 = Acces_Read_Reg(0xB1);  /* Read once per iteration */
		if ( (regB1 & B1_SINGLE_CONT) || (regB1 & B1_START_CAPTURE) )
		{
			DataFrameReceiveSenxor();													//Receive frame from SenXor
			const uint16_t* senxorData = DataFrameGetPointer();							//Get processed frame

			if (senxorData != 0)
			{
#ifdef CONFIG_MI_SENXOR_DBG
				printSenXorLog(senxorData);
#endif
				/* Update thread-safe snapshot for web/API consumers */
				if(latestFrameMutex != NULL)
				{
					xSemaphoreTake(latestFrameMutex, portMAX_DELAY);
					memcpy(mLatestFrameObj.mFrame, senxorData, sizeof(mLatestFrameObj.mFrame));
					++latestFrameNumber;
					latestFrameValid = true;
					xSemaphoreGive(latestFrameMutex);
				}

			}//End if
			DataFrameProcess();															//Thermal frame post-processing
		}
		else
		{
			/* Capture stopped externally - poll slowly */
			vTaskDelay(pdMS_TO_TICKS(200));
		}//End if

		taskYIELD();
	}//End for
}//End senxorTask


/*
 * ***********************************************************************
 * @brief       DataFrameReceiveError
 * @param       None
 * @return      None
 * @details     Frame receive error handler
 **************************************************************************/
void DataFrameReceiveError(void)
{
	if(SenXorError)
    {
	  ESP_LOGE(SXRTAG,SXR_ERR,SenXorError);
	  ESP_LOGW(SXRTAG,SXR_WARN_RECR);
      SenXorError = 0;                                                    //Clear error flag
      Acces_Write_Reg(0xB1,0);                                            //Stop capturing
      Acces_Write_Reg(0xB0,3);                                            //Reinitialise SenXor
    }//End if
}


/*
 * ***********************************************************************
 * @brief       senxorTask_Init
 * @param       None
 * @return      None
 * @details     Initialise SenXorTask
 **************************************************************************/
static void senxorTask_Init(void)
{
	latestFrameMutex = xSemaphoreCreateMutex();
	if (latestFrameMutex == NULL) {
		ESP_LOGE(SXRTAG, "Failed to create latestFrameMutex");
	}
}

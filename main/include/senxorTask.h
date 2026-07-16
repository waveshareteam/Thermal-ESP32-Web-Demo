/*****************************************************************************
 * @file     senxorTask.h
 * @version  2.01
 * @brief    Header file for senxorTask.c
 * @date	 21 Jul 2022
 ******************************************************************************/
#ifndef MAIN_INCLUDE_SENXORTASK_H_
#define MAIN_INCLUDE_SENXORTASK_H_
#include "esp_log.h"					//ESP logger
#include <stdlib.h>
#include <stdbool.h>

#include "MCU_Dependent.h"
#include "msg.h"						//Messages
#include "SenXorLib.h"					//Using SenXor library
#include "restServer.h"

#define SENXOR_TASK_STACK_SIZE	4096	//Task stack size


typedef struct senxorFrame{
	uint16_t mFrame[80*63];
}senxorFrame;


uint8_t senxorInit(void);

void senxorTask(void * pvParameters);

/**
 * @brief Thread-safe access to the latest thermal frame (for web/API)
 * @param frame[out] Buffer to receive the frame data (must be >= framePixelCount uint16_t)
 * @param framePixelCount Number of pixels to copy
 * @param frameNumber[out] Optional pointer to receive monotonic frame counter
 * @return true if a valid frame was copied
 */
bool senxorTask_GetLatestFrame(uint16_t *frame, size_t framePixelCount, uint32_t *frameNumber);

#endif /* MAIN_INCLUDE_SENXORTASK_H_ */

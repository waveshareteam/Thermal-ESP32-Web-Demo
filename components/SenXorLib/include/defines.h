/*****************************************************************************
 * @file     Defines.h
 * @version  V1.00
 * @brief    hold all Senxor define and include
 *
 * @copyright (C) 2018 Meridian Innovation
 ******************************************************************************/
#ifndef __DEFINES_H__
#define __DEFINES_H__
enum SenXor_Model {
   MODEL_BOTCAT45=0,
   MODEL_LYNX45,
   MODEL_BOTCAT58,
   MODEL_COUGAR, 		
   MODEL_PANTHER,
   MODEL_COUGAR25, 		
   MODEL_PANTHER25,
   MODEL_CHEETAH,
   MODEL_FUTURE
};


//#define CALIB_DATA_MCUFLASH						//MCU flash ROM
//#define POWERDOWN_FEATURE							//Power down feature
//#define MEDIANFILTER
//#define FIRMWARE_ROLING_AVERAGE					//enable rolling average
#define MEDIAN_STARK
//#define TEST_FRAME


#endif //__DEFINES_H__

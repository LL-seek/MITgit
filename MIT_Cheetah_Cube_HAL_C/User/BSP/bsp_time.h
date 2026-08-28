#ifndef BSP_TIME_H
#define BSP_TIME_H


#include <stdbool.h>
#include <stdint.h>




#ifdef __cplusplus
extern "C" {
#endif





bool BSP_Time_Init(void);

uint32_t BSP_Time_NowUs32(void);

uint32_t BSP_Time_ElapsedUs32(uint32_t start_us);

bool BSP_Time_HasElapsedUs32(uint32_t start_us, uint32_t interval_us);


uint64_t BSP_Time_NowUs64(void);

uint32_t BSP_Time_NowCycles32(void);

uint32_t BSP_Time_ElapsedCycles32(uint32_t start_cycles);





#ifdef __cplusplus
}
#endif



#endif

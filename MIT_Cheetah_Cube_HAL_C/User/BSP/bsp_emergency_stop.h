#ifndef BSP_EMERGENCY_STOP_H
#define BSP_EMERGENCY_STOP_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    BSP_ERROR_NONE = 0U,
    BSP_ERROR_HAL  = 1U
} BSP_ErrorCode;

void BSP_EmergencyStop(BSP_ErrorCode error_code);

bool BSP_EmergencyStop_IsActive(void);

BSP_ErrorCode BSP_EmergencyStop_GetErrorCode(void);

#ifdef __cplusplus
}
#endif

#endif
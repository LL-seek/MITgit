#include "motor_types.h"



typedef char FocCommandSizeCheck[
    (sizeof(FocCommand) == 20U) ? 1 : -1
];

typedef char MotorFeedbackSizeCheck[
    (sizeof(MotorFeedback) == 12U) ? 1 : -1
];

typedef char MotorParametersSizeCheck[
    (sizeof(MotorParameters) == 572U) ? 1 : -1
];























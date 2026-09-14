#include "bsp_can.h"
#include "can.h"


static CAN_RxFrame rxMsg;                            //保存中断取出的完整原始帧
static volatile uint8_t rx_pending = 0U;             //邮箱状态，1表示有待取帧，0表示没有待取帧


HAL_StatusTypeDef BSP_CAN_SetFilter(uint16_t can_id)    //配置或更新本机CAN接收过滤器
{
    CAN_FilterTypeDef filter = {0};                     //保存本次过滤器配置

    filter.FilterBank = 0U;                             //沿用原工程的0号过滤器
    filter.FilterMode = CAN_FILTERMODE_IDMASK;          //沿用原工程的标识符掩码模式
    filter.FilterScale = CAN_FILTERSCALE_32BIT;         //沿用原工程的32位过滤器

    filter.FilterIdHigh = (uint32_t)can_id << 5;        //将标准ID放到过滤器高16位中的对应位置
    filter.FilterIdLow = 0U;                            //沿用原驱动的低16位标识符配置
    filter.FilterMaskIdHigh = 0x7FFU << 5;              //比较标准ID的全部11位
    filter.FilterMaskIdLow = 0U;                        //沿用原驱动配置，低16位不参与比较

    filter.FilterFIFOAssignment = CAN_FILTER_FIFO0;     //匹配的报文进入接收FIFO0
    filter.FilterActivation = ENABLE;                   //启用该过滤器
    filter.SlaveStartFilterBank = 14U;                  //沿用原驱动的CAN1与CAN2过滤器分界

    return HAL_CAN_ConfigFilter(&hcan1, &filter);       //将过滤器配置写入CAN1并返回执行结果
}


void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan) //处理FIFO0收到报文的通知
{
    if (HAL_CAN_GetRxMessage(hcan,CAN_RX_FIFO0,&rxMsg.header,rxMsg.data) != HAL_OK)           //从硬件FIFO0取出一帧
    {
        return;                                                 //取帧失败时不发布邮箱数据
    }

    HAL_CAN_DeactivateNotification(hcan,CAN_IT_RX_FIFO0_MSG_PENDING); //暂停通知，等待主循环取走当前帧

    rx_pending = 1U;                                           //标记邮箱中已有一帧完整数据
}


uint8_t BSP_CAN_Read(CAN_RxFrame *msg)                         //由主循环取走邮箱中的一帧
{
    if (rx_pending == 0U)                                      //当前没有待取帧
    {
        return 0U;                                             //告知调用方没有收到新帧
    }

    *msg = rxMsg;                                              //复制完整帧，此时接收通知仍处于暂停状态
    rx_pending = 0U;                                           //标记当前邮箱数据已经取走

    HAL_CAN_ActivateNotification(&hcan1,CAN_IT_RX_FIFO0_MSG_PENDING);  //恢复通知，允许接收下一帧

    return 1U;                                                 //告知调用方已经取到一帧
}

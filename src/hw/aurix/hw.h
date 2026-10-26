#ifndef __AURIX_HW_H__
#define __AURIX_HW_H__

#include "xcp_config.h"

#ifdef TP_ETHER

#include "IfxStm.h"
#include "IfxCpu.h"
#include "IfxGeth_Eth.h"
#include "Configuration.h"
#include "ConfigurationIsr.h"
#include "Ifx_Lwip.h"
#include "xcp.h"

// TODO: Can we do this more clever by using XCP_ON_ETHERNET_CORE as param?
#if XCP_ON_ETHERNET_CORE == 0
#define XCP_STM_MODULE  MODULE_STM0
#define XCP_STM_INTERRUPT_TOS   IfxSrc_Tos_cpu0
#elif XCP_ON_ETHERNET_CORE == 1
#define XCP_STM_MODULE  MODULE_STM2
#define XCP_STM_INTERRUPT_TOS   IfxSrc_Tos_cpu1
#elif XCP_ON_ETHERNET_CORE == 2
#define XCP_STM_MODULE  MODULE_STM3
#define XCP_STM_INTERRUPT_TOS   IfxSrc_Tos_cpu2
#elif XCP_ON_ETHERNET_CORE == 3
#define XCP_STM_MODULE  MODULE_STM4
#define XCP_STM_INTERRUPT_TOS   IfxSrc_Tos_cpu3
#elif XCP_ON_ETHERNET_CORE == 4
#define XCP_STM_MODULE  MODULE_STM5
#define XCP_STM_INTERRUPT_TOS   IfxSrc_Tos_cpu4
#elif XCP_ON_ETHERNET_CORE == 5
#define XCP_STM_MODULE  MODULE_STM5
#define XCP_STM_INTERRUPT_TOS   IfxSrc_Tos_cpu5
#elif XCP_ON_ETHERNET_CORE == 6
#define XCP_STM_MODULE  MODULE_STM6
#define XCP_STM_INTERRUPT_TOS   IfxSrc_Tos_cpu6
#endif

void XcpHw_Init(void);
void XcpHw_Deinit(void);
uint32_t XcpHw_GetTimerCounter(void);
uint32_t XcpHw_GetTimerCounterMS(void);
void XcpHw_AcquireLock(uint8_t lockIdx);
void XcpHw_ReleaseLock(uint8_t lockIdx);

#endif
#endif
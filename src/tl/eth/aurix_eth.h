#ifndef __AURIX_ETH_H__
#define __AURIX_ETH_H__

#include "xcp_config.h"

#ifdef TP_ETHER

#ifndef XCP_TL_UDP_DEBUG
#define XCP_TL_UDP_DEBUG    LWIP_DBG_ON
#endif

#define STORAGE_SIZE_BYTES 256  

#include <string.h>
#include "lwip/opt.h"
#include "lwip/debug.h"
#include "lwip/stats.h"
#include "xcp.h"

#if XCP_ON_ETHERNET_PROTOCOL == XCP_ON_ETHERNET_USE_UDP
#include "lwip/udp.h"

typedef struct udp_pcb udpPcb; 
#else
#include "lwip/tcp.h"

typedef struct tcp_pcb tcpPcb; 
#endif

void XcpTl_Init(void);
void XcpTl_MainFunction(void);
void XcpTl_Send(uint8_t const *buf, uint16_t len);
void XcpTl_ReleaseConnection(void);
void XcpTl_SaveConnection(void);

#if XCP_ON_ETHERNET_PROTOCOL == XCP_ON_ETHERNET_USE_UDP
void XcpTl_Udp_Init(void);
void XcpTl_Udp_Send(uint8_t const *buf, uint16_t len);
//void XcpTl_Udp_ReceiveCallback(void *arg, struct udpPcb *pcb, struct pbuf *p, const ip_addr_t *addr, u16_t port);
#else
void XcpTl_Tcp_Init(void);
#endif

#endif

#endif
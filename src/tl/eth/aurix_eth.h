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
#include "lwip/igmp.h"
#include "lwip/netif.h"
#include "xcp.h"
#include "lwip/udp.h"

#if XCP_ON_ETHERNET_PROTOCOL == XCP_ON_ETHERNET_USE_TCP
#include "lwip/tcp.h"

typedef struct tcp_pcb tcpPcb; 
#endif

typedef struct udp_pcb udpPcb; 

typedef struct IFX_PACKED
{
    uint16_t len;
    uint16_t ctr;
} Xcp_Eth_Header_t;

typedef struct
{
    boolean isConnected;
    ip_addr_t remoteIpAddress;
    uint16 remotePort;

    udpPcb *tlUdpPcb;
} Xcp_Eth_ConnectionStatus_t;

extern Xcp_Eth_ConnectionStatus_t g_xcp_eth_connectionstatus;

void XcpTl_Init(void);
void XcpTl_MainFunction(void);
void XcpTl_Send(uint8_t const *buf, uint16_t len);
void XcpTl_ReleaseConnection(void);
void XcpTl_SaveConnection(void);
void XcpTl_HandleIncomingData(struct pbuf *rxBuf);

#if XCP_ENABLE_ETH_DISCOVERY == XCP_ON
void XcpTl_InitMulticast(void);
#endif

#if XCP_ON_ETHERNET_PROTOCOL == XCP_ON_ETHERNET_USE_UDP
void XcpTl_Udp_Init(void);
void XcpTl_Udp_Send(uint8_t const *buf, uint16_t len);
#else
void XcpTl_Tcp_Init(void);
#endif

#endif

#endif
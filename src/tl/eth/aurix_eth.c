#include "aurix_eth.h"

void XcpTl_MainFunction(void)
{
    // TODO

    // Accept -> Check on whats actually needed for UDP. We want to stay connected to a single Master
    // RxHandler -> Not Needed, triggered by callback
}

void XcpTl_Send(uint8_t const *buf, uint16_t len)
{
#if XCP_ON_ETHERNET_PROTOCOL == XCP_ON_ETHERNET_USE_UDP
    XcpTl_Udp_Send(buf, len);
#else
    XcpTl_Tcp_Send(buf, len);
#endif
}

void XcpTl_Init(void)
{
#if XCP_ON_ETHERNET_PROTOCOL == XCP_ON_ETHERNET_USE_UDP
    XcpTl_Udp_Init();
#else
    XcpTl_Tcp_Init();
#endif
}

void XcpTl_ReleaseConnection(void)
{
    // TODO: Reset connection status
    // Remove TCP bindings
}

void XcpTl_SaveConnection(void)
{
    // TODO: Set connection status
    // Add TCP bindings
}
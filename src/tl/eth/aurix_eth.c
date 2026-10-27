#include "aurix_eth.h"

Xcp_Eth_ConnectionStatus_t g_xcp_eth_connectionstatus;

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

void XcpTl_ResetConnectionStatus(void)
{
    g_xcp_eth_connectionstatus.isConnected = false;
    g_xcp_eth_connectionstatus.remoteIpAddress.addr = IPADDR_NONE;
    g_xcp_eth_connectionstatus.remotePort = 0;
    g_xcp_eth_connectionstatus.tlUdpPcb = NULL;
}

void XcpTl_Init(void)
{
    XcpTl_ResetConnectionStatus();

#if XCP_ENABLE_ETH_DISCOVERY == XCP_ON
    XcpTl_InitMulticast();
#endif

#if XCP_ON_ETHERNET_PROTOCOL == XCP_ON_ETHERNET_USE_UDP
    XcpTl_Udp_Init();
#else
    XcpTl_Tcp_Init();
#endif
}

void XcpTl_ReleaseConnection(void)
{
    XcpTl_ResetConnectionStatus();
    // TODO: Reset connection status
    // Remove TCP bindings
}

void XcpTl_SaveConnection(void)
{
    g_xcp_eth_connectionstatus.isConnected = TRUE;
}

void XcpTl_HandleIncomingData(struct pbuf *rxBuf)
{
    static const uint16 header_length = 4;

    if (rxBuf->tot_len == 0u ||
        rxBuf->tot_len > XCP_TRANSPORT_LAYER_CTO_BUFFER_SIZE)
    {
        LWIP_DEBUGF(XCP_TL_UDP_DEBUG | LWIP_DBG_STATE, ("XcpTl_UdP: Received Packet too large.\n"));
        return;
    }

    if (rxBuf->tot_len > header_length)
    {
        Xcp_Eth_Header_t *header = rxBuf->payload;
        uint16 payload_len = rxBuf->tot_len - header_length;

        if (payload_len != header->len)
        {
            LWIP_DEBUGF(XCP_TL_UDP_DEBUG | LWIP_DBG_STATE, ("XcpTl_UdP: Length in Header mismatches with Buffer.\n"));
            return;
        }

        pbuf_copy_partial(rxBuf, Xcp_CtoIn.data, payload_len, header_length);
        Xcp_CtoIn.len = payload_len;

        Xcp_DispatchCommand(&Xcp_CtoIn);
    }
}
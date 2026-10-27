#include "aurix_eth.h"

#if XCP_ON_ETHERNET_PROTOCOL == XCP_ON_ETHERNET_USE_UDP

void XcpTl_Udp_ReceiveCallback(void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, u16_t port)
{
    g_xcp_eth_connectionstatus.remoteIpAddress.addr = addr->addr;
    g_xcp_eth_connectionstatus.remotePort = port;

    XcpTl_HandleIncomingData(p);
    
    /* Free RX buffer */
    pbuf_free(p);
}

void XcpTl_Udp_Init(void)
{
    g_xcp_eth_connectionstatus.tlUdpPcb = udp_new();

    LWIP_DEBUGF(XCP_TL_UDP_DEBUG | LWIP_DBG_STATE, ("XcpTl_UdP: Init.\n"));

    if (g_xcp_eth_connectionstatus.tlUdpPcb != NULL)
    {
        err_t err = udp_bind(g_xcp_eth_connectionstatus.tlUdpPcb, IP_ADDR_ANY, XCP_ON_ETHERNET_PORT);

        if (err == ERR_OK)
        {
            udp_recv(g_xcp_eth_connectionstatus.tlUdpPcb, XcpTl_Udp_ReceiveCallback, NULL);
        }
        else
        {
            LWIP_DEBUGF(XCP_TL_UDP_DEBUG | LWIP_DBG_STATE, ("XcpTl_UdP: Unable to Bind.\n"));
        }
    }
    else
    {
        LWIP_DEBUGF(XCP_TL_UDP_DEBUG | LWIP_DBG_STATE, ("XcpTl_UdP: Unable to create a UDP Control Block.\n"));
    }
}

void XcpTl_Udp_Send(uint8_t const *buf, uint16_t len)
{
    if (g_xcp_eth_connectionstatus.tlUdpPcb == NULL)
    {
        return;
    }

    struct pbuf *txBuf = pbuf_alloc(PBUF_TRANSPORT, len, PBUF_RAM);

    if (txBuf == NULL)
    {
        return;
    }

    memcpy(txBuf->payload, buf, len);

    err_t err = udp_sendto(g_xcp_eth_connectionstatus.tlUdpPcb, txBuf, &g_xcp_eth_connectionstatus.remoteIpAddress, g_xcp_eth_connectionstatus.remotePort);

    if (err != ERR_OK)
    {
        LWIP_DEBUGF(XCP_TL_UDP_DEBUG | LWIP_DBG_STATE, ("XcpTl_UdP: Unable to Send.\n"));
    }

    pbuf_free(txBuf);
}

#endif
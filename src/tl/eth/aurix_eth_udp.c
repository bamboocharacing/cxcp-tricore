#include "aurix_eth.h"

#if XCP_ON_ETHERNET_PROTOCOL == XCP_ON_ETHERNET_USE_UDP

udpPcb *g_xcpTlUdpPcb;
ip_addr_t remote_ip = IPADDR4_INIT(IPADDR_NONE);
uint16 remote_port = 0;

void XcpTl_Udp_HandleIncomingData(struct pbuf *rxBuf)
{
    static const uint16 header_length = 4;

    if (rxBuf->tot_len > rxBuf->len)
    {
        // TODO: Handle Chained Buffers, or is pbuf_copy_partial doing this for us?
    }

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

void XcpTl_Udp_ReceiveCallback(void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, u16_t port)
{
    struct pbuf *rxBuf = pbuf_alloc(PBUF_TRANSPORT, p->len, PBUF_RAM);

    if (rxBuf != NULL)
    {
        pbuf_take(rxBuf, p->payload, p->len);        
        remote_ip.addr = addr->addr;
        remote_port = port;

        XcpTl_Udp_HandleIncomingData(rxBuf);

        pbuf_free(rxBuf);
    }
    
    /* Free RX buffer */
    pbuf_free(p);
}

void XcpTl_Udp_Init(void)
{
    g_xcpTlUdpPcb = udp_new();

    LWIP_DEBUGF(XCP_TL_UDP_DEBUG | LWIP_DBG_STATE, ("XcpTl_UdP: Init.\n"));

    if (g_xcpTlUdpPcb != NULL)
    {
        err_t err = udp_bind(g_xcpTlUdpPcb, IP_ADDR_ANY, 5555);

        if (err == ERR_OK)
        {
            udp_recv(g_xcpTlUdpPcb, XcpTl_Udp_ReceiveCallback, NULL);
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
    // LWIP_DEBUGF(XCP_TL_UDP_DEBUG | LWIP_DBG_TRACE | LWIP_DBG_STATE, ("XcpTl_UdP: Send to IP: %"U16_F".%"U16_F".%"U16_F".%"U16_F"\n",
    //     ip4_addr1_16(&remote_ip),
    //     ip4_addr2_16(&remote_ip),
    //     ip4_addr3_16(&remote_ip),
    //     ip4_addr4_16(&remote_ip)));

    if (remote_ip.addr == IPADDR_NONE)
    {
        return;
    }

    if (g_xcpTlUdpPcb != NULL)
    {
        struct pbuf *txBuf = pbuf_alloc(PBUF_TRANSPORT, len, PBUF_RAM);
        memcpy(txBuf->payload, buf, len);

        err_t err = udp_sendto(g_xcpTlUdpPcb, txBuf, &remote_ip, remote_port);

        if (err != ERR_OK)
        {
            LWIP_DEBUGF(XCP_TL_UDP_DEBUG | LWIP_DBG_STATE, ("XcpTl_UdP: Unable to Send.\n"));
        }

        pbuf_free(txBuf);
    }
}

#endif
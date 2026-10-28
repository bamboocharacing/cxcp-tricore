#include "aurix_eth.h"

#if XCP_ENABLE_ETH_DISCOVERY == XCP_ON

typedef enum tagXcp_TlCommandType {
    GET_SERVER_ID           = UINT8(0xFF),
    GET_SERVER_ID_EXTENDED  = UINT8(0xFD),
    GET_DAQ_CLOCK_MULTICAST = UINT8(0xFA)
} Xcp_TlCommandType;

typedef struct IFX_PACKED
{
    Xcp_Eth_Header_t header;
    uint8 cmd;
    uint8 subCmd;
    uint16 responsePort;
    uint32 responseAddr;
    uint8 padding[12];
    uint8 mode;
} Xcp_Eth_Discovery_Req_Packet_t;

static struct udp_pcb *s_discoveryPcb;

#if !LWIP_NETIF_EXT_STATUS_CALLBACK
#error "XCP Ethernet discovery requires LWIP_NETIF_EXT_STATUS_CALLBACK"
#endif

static netif_ext_callback_t s_xcp_eth_netif_callback;

static void XcpTl_BindMulticast(void);

#if defined(XCP_ENABLE_TIME_CORRELATION) && (XCP_ENABLE_TIME_CORRELATION == XCP_ON)
#include "xcp_timecorr.h"

static struct udp_pcb *s_timeSyncPcb;
static ip4_addr_t s_timeSyncGroup;      /* Currently joined cluster group, any = none */

/* Cluster affiliation maps to 239.255.HI.LO */
static void XcpTl_ClusterToGroup(uint16 clusterId, ip4_addr_t *group)
{
    IP4_ADDR(group, 239, 255, XCP_HIBYTE(clusterId), XCP_LOBYTE(clusterId));
}

static void XcpTl_JoinTimeSyncGroup(uint16 clusterId)
{
    if (!ip4_addr_isany_val(s_timeSyncGroup))
    {
        igmp_leavegroup_netif(netif_default, &s_timeSyncGroup);
        ip4_addr_set_any(&s_timeSyncGroup);
    }

    ip4_addr_t group;
    XcpTl_ClusterToGroup(clusterId, &group);

    if (igmp_joingroup_netif(netif_default, &group) == ERR_OK)
    {
        ip4_addr_copy(s_timeSyncGroup, group);
    }
    else
    {
        LWIP_DEBUGF(XCP_TL_UDP_DEBUG | LWIP_DBG_STATE, ("XcpTl_UdP: [TimeSync] Unable to join group.\n"));
    }
}

static void XcpTl_TimeSync_ReceiveCallback(void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, u16_t port)
{
    /* Sample first: the EV_TIME_SYNC reports this as "upon physical reception" */
    const uint32 rxTimestamp = XcpHw_GetTimerCounter();

    LWIP_DEBUGF(XCP_TL_UDP_DEBUG | LWIP_DBG_STATE, ("XcpTl_UdP: [TimeSync] Packet incoming.\n"));

    if (p != NULL)
    {
        uint8 req[5]; /* 0xF2, 0xFA, CLUSTER_ID (LE), COUNTER */

        if (pbuf_copy_partial(p, req, sizeof(req), sizeof(Xcp_Eth_Header_t)) == sizeof(req) &&
            req[0] == XCP_TRANSPORT_LAYER_CMD &&
            req[1] == GET_DAQ_CLOCK_MULTICAST)
        {
            /* Core checks cluster affiliation, connection and response format, then answers via XcpTl_Send */
            XcpTimecorr_HandleMulticast(XCP_MAKEWORD(req[3], req[2]), req[4], rxTimestamp);
        }

        /* Free RX buffer */
        pbuf_free(p);
    }
}

static void XcpTl_BindTimeSync(void)
{
    if (s_timeSyncPcb != NULL)
    {
        /* Already bound: discovery bind is retried on address change, time sync must not be */
        return;
    }

    LWIP_DEBUGF(XCP_TL_UDP_DEBUG | LWIP_DBG_STATE, ("XcpTl_UdP: [TimeSync] Binding.\n"));

    s_timeSyncPcb = udp_new();

    if (s_timeSyncPcb != NULL)
    {
        err_t err = udp_bind(s_timeSyncPcb, IP_ADDR_ANY, XCP_TIMECORR_MULTICAST_PORT);

        if (err == ERR_OK)
        {
            XcpTl_JoinTimeSyncGroup(XcpTimecorr_GetClusterAffiliation());
            udp_recv(s_timeSyncPcb, XcpTl_TimeSync_ReceiveCallback, NULL);
        }
        else
        {
            LWIP_DEBUGF(XCP_TL_UDP_DEBUG | LWIP_DBG_STATE, ("XcpTl_UdP: [TimeSync] Unable to Bind.\n"));

            udp_remove(s_timeSyncPcb);
            s_timeSyncPcb = NULL;
        }
    }
    else
    {
        LWIP_DEBUGF(XCP_TL_UDP_DEBUG | LWIP_DBG_STATE, ("XcpTl_UdP: [TimeSync] Unable to create a UDP Control Block.\n"));
    }
}

/* Called by the core when TIME_CORRELATION_PROPERTIES sets a new cluster affiliation */
void XcpTl_UpdateMulticastGroup(uint16_t cluster_id)
{
    /* Before the first IP the group is joined on bind, using the then-current affiliation */
    if (s_timeSyncPcb != NULL)
    {
        XcpTl_JoinTimeSyncGroup(cluster_id);
    }
}
#endif

static void XcpTl_NetifStatusCallback(struct netif *netif, netif_nsc_reason_t reason, const netif_ext_callback_args_t *args)
{
    if ((reason & LWIP_NSC_IPV4_ADDRESS_CHANGED) && !ip4_addr_isany_val(*netif_ip4_addr(netif)))
    {
        if (s_discoveryPcb == NULL)
        {
            /* First valid address (e.g. DHCP bound): join now so the IGMP report carries a valid source */
            XcpTl_BindMulticast();
        }
        else
        {
            /* Address changed (e.g. new DHCP lease): announce the membership again */
            igmp_report_groups(netif);
        }
    }
}

static void XcpTl_Discovery_Respond(Xcp_Eth_Discovery_Req_Packet_t *req)
{
    const Xcp_GetIdType id = Xcp_GetId0;
    const boolean ext = req->subCmd == GET_SERVER_ID_EXTENDED;

    // (PID) + (IPv4 + 12) + port u16 + status u8 + resource u8 + id length u32 + id + mac
    const uint16 len   = (ext ? 1u : 0u) + 16u + 2u + 1u + 1u + 4u + id.len + (ext ? 6u : 0u);

    struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, sizeof(Xcp_Eth_Header_t) + len, PBUF_RAM);

    if (p == NULL)
    {
        return;
    }

    uint8 *b = p->payload;
    uint16 i = 0;
    Xcp_Eth_Header_t header = { .ctr = 0, .len = len };
    
    memcpy(&b[i], &header, sizeof(header));
    i += sizeof(header);

    if (ext)
    {
        b[i++] = 0xFF;
    }

    /* IPv4 address */
    memcpy(&b[i], netif_ip4_addr(netif_default), 4);
    i += 4;

    /* IPv6 padding */
    memset(&b[i], 0, 12);
    i += 12;

    /* Port */
    b[i++] = XCP_LOBYTE(XCP_ON_ETHERNET_PORT);
    b[i++] = XCP_HIBYTE(XCP_ON_ETHERNET_PORT);

    /* Status Bitfield: 0: UDP, 1: TCP, 2: IPv6, 3: Busy/Connected */
    uint8 status = 0u;
#if XCP_ON_ETHERNET_PROTOCOL == XCP_ON_ETHERNET_USE_UDP
    status |= 0x01u;
#endif
#if XCP_ON_ETHERNET_PROTOCOL == XCP_ON_ETHERNET_USE_TCP
    status |= 0x02u;
#endif
    
    if (g_xcp_eth_connectionstatus.isConnected || Xcp_IsBusy())
    {
        status |= 0x08u;
    }
    
    if (!ext)
    {
        status |= 0x10u;
    }

    b[i++] = status;

    /* Resource */
    b[i++] = Xcp_GetResourceMask();

    /* ID Len */
    const uint32 idLen = id.len;
    memcpy(&b[i], &idLen, sizeof(idLen));
    i += sizeof(idLen);

    /* ID */
    memcpy(&b[i], id.name, id.len);
    i += id.len;
    
    /* MAC */
    if (ext)
    {
        memcpy(&b[i], netif_default->hwaddr, netif_default->hwaddr_len);
    }

    ip_addr_t dst;
    memcpy(&ip_2_ip4(&dst)->addr, &req->responseAddr, 4);

    udp_sendto(s_discoveryPcb, p, &dst, req->responsePort);
    pbuf_free(p);
}

static void XcpTl_Multicast_ReceiveCallback(void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, u16_t port)
{
    Xcp_Eth_Discovery_Req_Packet_t discover_req_packet;

    if (p != NULL)
    {
        if (p->tot_len >= sizeof(discover_req_packet) &&
            pbuf_copy_partial(p, &discover_req_packet, sizeof(discover_req_packet), 0) == sizeof(discover_req_packet) &&
            discover_req_packet.cmd == XCP_TRANSPORT_LAYER_CMD &&
            (discover_req_packet.subCmd == GET_SERVER_ID || discover_req_packet.subCmd == GET_SERVER_ID_EXTENDED) &&
            discover_req_packet.mode == 0)
        {
            XcpTl_Discovery_Respond(&discover_req_packet);
        }

        /* Free RX buffer */
        pbuf_free(p);
    }
}

void XcpTl_InitMulticast(void)
{
    s_discoveryPcb = NULL;
#if defined(XCP_ENABLE_TIME_CORRELATION) && (XCP_ENABLE_TIME_CORRELATION == XCP_ON)
    s_timeSyncPcb = NULL;
    ip4_addr_set_any(&s_timeSyncGroup);
#endif
    netif_add_ext_callback(&s_xcp_eth_netif_callback, XcpTl_NetifStatusCallback);

    /* Static IP or DHCP already bound before we registered: no address-change event will come */
    if (!ip4_addr_isany_val(*netif_ip4_addr(netif_default)))
    {
        XcpTl_BindMulticast();
    }
}

static void XcpTl_BindMulticast(void)
{
    ip4_addr_t multicast_ip;
    IP4_ADDR(&multicast_ip,
        XCP_ETH_DISCOVERY_MCAST_IP0,
        XCP_ETH_DISCOVERY_MCAST_IP1,
        XCP_ETH_DISCOVERY_MCAST_IP2,
        XCP_ETH_DISCOVERY_MCAST_IP3);

    LWIP_DEBUGF(XCP_TL_UDP_DEBUG | LWIP_DBG_STATE, ("XcpTl_UdP: [Multicast] Binding.\n"));

    s_discoveryPcb = udp_new();

    if (s_discoveryPcb != NULL)
    {
        err_t err = udp_bind(s_discoveryPcb, IP_ADDR_ANY, XCP_ETH_DISCOVERY_MCAST_PORT);

        if (err == ERR_OK)
        {
            /* Join the multicast group, otherwise ip4_input drops datagrams sent to it */
            err = igmp_joingroup_netif(netif_default, &multicast_ip);

            if (err != ERR_OK)
            {
                LWIP_DEBUGF(XCP_TL_UDP_DEBUG | LWIP_DBG_STATE, ("XcpTl_UdP: [Multicast] Unable to join group.\n"));
            }

            udp_recv(s_discoveryPcb, XcpTl_Multicast_ReceiveCallback, NULL);
        }
        else
        {
            LWIP_DEBUGF(XCP_TL_UDP_DEBUG | LWIP_DBG_STATE, ("XcpTl_UdP: [Multicast] Unable to Bind.\n"));

            udp_remove(s_discoveryPcb);
            s_discoveryPcb = NULL;
        }
    }
    else
    {
        LWIP_DEBUGF(XCP_TL_UDP_DEBUG | LWIP_DBG_STATE, ("XcpTl_UdP: [Multicast] Unable to create a UDP Control Block.\n"));
    }

#if defined(XCP_ENABLE_TIME_CORRELATION) && (XCP_ENABLE_TIME_CORRELATION == XCP_ON)
    /* Same trigger as discovery: first valid IP, so the IGMP join has a valid source */
    XcpTl_BindTimeSync();
#endif
}

#endif
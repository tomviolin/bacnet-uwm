/*####COPYRIGHTBEGIN####
 -------------------------------------------
 Copyright (C) 2012 Steve Karg

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to:
 The Free Software Foundation, Inc.
 59 Temple Place - Suite 330
 Boston, MA  02111-1307, USA.

 As a special exception, if other files instantiate templates or
 use macros or inline functions from this file, or you compile
 this file and link it with other works to produce a work based
 on this file, this file does not by itself cause the resulting
 work to be covered by the GNU General Public License. However
 the source code for this file must still be made available in
 accordance with section (3) of the GNU General Public License.

 This exception does not invalidate any other reasons why a work
 based on this file might be covered by the GNU General Public
 License.
 -------------------------------------------
####COPYRIGHTEND####*/
#include <stdint.h> /* for standard integer types uint8_t etc. */
#include <stdbool.h> /* for the standard bool type. */
#include "bacnet/bacdef.h"
#include "bacnet/bacdcode.h"
#include "bacnet/bacint.h"
#include "bacnet/datalink/bip.h"
#include "bacnet/datalink/bvlc.h"
#include "bacnet/basic/services.h"
#include "bacport.h" /* custom per port */

/** @file bip.c  Configuration and Operations for BACnet/IP */

/* port to use - stored in network byte order */
static uint16_t BIP_Port = 0xBAC0U;
static bool BIP_Port_Changed;
/* IP Address - stored in network byte order */
static struct in_addr BIP_Address;
/* Broadcast Address - stored in network byte order */
static struct in_addr BIP_Broadcast_Address;
/* lwIP socket, of sorts */
static struct udp_pcb *Server_upcb;
/* track packets for diagnostics */
struct bacnet_stats {
    uint32_t xmit; /* Transmitted packets. */
    uint32_t recv; /* Received packets. */
    uint32_t drop; /* Dropped packets. */
};
struct bacnet_stats BIP_Stats;
#define BIP_STATS_INC(x) ++BIP_Stats.x

uint32_t bip_stats_xmit(void)
{
    return BIP_Stats.xmit;
}

uint32_t bip_stats_recv(void)
{
    return BIP_Stats.recv;
}

uint32_t bip_stats_drop(void)
{
    return BIP_Stats.drop;
}

void bip_set_addr(uint32_t net_address)
{ /* in network byte order */
    BIP_Address.s_addr = net_address;
}

/* returns network byte order */
uint32_t bip_get_addr(void)
{
    return BIP_Address.s_addr;
}

void bip_set_broadcast_addr(uint32_t net_address)
{ /* in network byte order */
    BIP_Broadcast_Address.s_addr = net_address;
}

/* returns network byte order */
uint32_t bip_get_broadcast_addr(void)
{
    return BIP_Broadcast_Address.s_addr;
}

/**
 * @brief Set the BACnet IPv4 UDP port number
 * @param port - IPv4 UDP port number - in host byte order
 */
void bip_set_port(uint16_t port)
{
    if (BIP_Port != htons(port)) {
        BIP_Port_Changed = true;
        BIP_Port = htons(port);
    }
}

bool bip_port_changed(void)
{
    return BIP_Port_Changed;
}

/* returns host byte order */
uint16_t bip_get_port(void)
{
    return ntohs(BIP_Port);
}

static void bip_mac_to_addr(struct ip_addr *address, uint8_t *mac)
{
    if (mac && address) {
        address->addr = ((u32_t)((((uint32_t)mac[0]) << 24) & 0xff000000));
        address->addr |= ((u32_t)((((uint32_t)mac[1]) << 16) & 0x00ff0000));
        address->addr |= ((u32_t)((((uint32_t)mac[2]) << 8) & 0x0000ff00));
        address->addr |= ((u32_t)(((uint32_t)mac[3]) & 0x000000ff));
    }
}

static void bip_addr_to_mac(uint8_t *mac, struct ip_addr *address)
{
    if (mac && address) {
        mac[0] = (uint8_t)(address->addr >> 24);
        mac[1] = (uint8_t)(address->addr >> 16);
        mac[2] = (uint8_t)(address->addr >> 8);
        mac[3] = (uint8_t)(address->addr);
    }
}

static int bip_decode_bip_address(BACNET_ADDRESS *bac_addr,
    struct ip_addr *address, /* in network format */
    uint16_t *port)
{ /* in network format */
    int len = 0;

    if (bac_addr) {
        bip_mac_to_addr(address, &bac_addr->mac[0]);
        memcpy(port, &bac_addr->mac[4], 2);
        len = 6;
    }

    return len;
}

/** Function to send a packet out the BACnet/IP socket (Annex J).
 * @ingroup DLBIP
 *
 * @param dst_ip [in] Destination address
 * @param port [in] UDP port number
 * @param pkt [in] PBUF packet
 *
 * @return true if the packet was sent
 */
static bool bip_send_mpdu(
    struct ip_addr *dst_ip, uint16_t port, struct pbuf *pkt)
{
    err_t status = ERR_OK;

    /* Send the packet */
    status = udp_sendto(Server_upcb, pkt, dst_ip, port);
    /* free the buffer pbuf */
    pbuf_free(pkt);
    if (status == ERR_OK) {
        BIP_STATS_INC(xmit);
    }

    return (status == ERR_OK);
}

/** Send the Original Broadcast or Unicast messages
 *
 * @param dest [in] Destination address (may encode an IP address and port #).
 * @param npdu_data [in] The NPDU header (Network) information (not used).
 * @param pdu [in] Buffer of data to be sent - may be null (why?).
 * @param pdu_len [in] Number of bytes in the pdu buffer.
 *
 * @return number of bytes sent
 */
int bip_send_pdu(BACNET_ADDRESS *dest, /* destination address */
    BACNET_NPDU_DATA *npdu_data, /* network information */
    uint8_t *pdu, /* any data to be sent - may be null */
    unsigned pdu_len)
{
    struct pbuf *pkt = NULL, *pkt0 = NULL;
    uint8_t mtu[4] = { 0 };
    int mtu_len = 0;
    /* addr and port in host format */
    struct ip_addr dst_ip;
    uint16_t port = 0;
    err_t status = ERR_OK;

    pkt0 = pbuf_alloc(PBUF_TRANSPORT, 4, PBUF_POOL);
    if (pkt0 == NULL) {
        return 0;
    }
    pkt = pbuf_alloc(PBUF_TRANSPORT, pdu_len, PBUF_POOL);
    if (pkt == NULL) {
        pbuf_free(pkt0);
        return 0;
    }
    mtu[0] = BVLL_TYPE_BACNET_IP;
    if (dest->net == BACNET_BROADCAST_NETWORK) {
        /* broadcast */
        dst_ip.addr = BIP_Broadcast_Address.s_addr;
        port = BIP_Port;
        mtu[1] = BVLC_ORIGINAL_BROADCAST_NPDU;
    } else if (dest->mac_len == 6) {
        /* unicast */
        bip_decode_bip_address(dest, &dst_ip, &port);
        mtu[1] = BVLC_ORIGINAL_UNICAST_NPDU;
    } else {
        /* invalid address */
        return -1;
    }
    mtu_len = pdu_len + 4 /*inclusive */;
    encode_unsigned16(&mtu[2], mtu_len);
    pbuf_take(pkt0, mtu, 4);
    pbuf_take(pkt, pdu, pdu_len);
    pbuf_cat(pkt0, pkt);
    status = bip_send_mpdu(&dst_ip, port, pkt0);
    if (!status) {
        mtu_len = 0;
    }

    return mtu_len;
}

/** Send the BVLC Result message
 *
 * @param addr [in] Destination address
 * @param result_code - BVLC result code
 *
 * @return number of bytes encoded
 */
static bool bvlc_send_result(struct ip_addr *addr, uint16_t result_code)
{
    struct pbuf *pkt = NULL;
    uint8_t mtu[6] = { 0 };
    uint16_t mtu_len = 6;

    pkt = pbuf_alloc(PBUF_TRANSPORT, mtu_len, PBUF_POOL);
    if (pkt == NULL) {
        return 0;
    }
    mtu[0] = BVLL_TYPE_BACNET_IP;
    mtu[1] = BVLC_RESULT;
    /* The 2-octet BVLC Length field is the length, in octets,
       of the entire BVLL message, including the two octets of the
       length field itself, most significant octet first. */
    encode_unsigned16(&mtu[2], 6);
    encode_unsigned16(&mtu[4], result_code);
    pbuf_take(pkt, mtu, mtu_len);

    return bip_send_mpdu(addr, BIP_Port, pkt);
}

/** LwIP BACnet service callback
 *
 * @param arg [in] optional argument from service
 * @param upcb [in] UDP control block
 * @param pkt [in] UDP packet - PBUF
 * @param addr [in] UDP source address
 * @param port [in] UDP port number
 */
void bip_server_callback(void *arg,
    struct udp_pcb *upcb,
    struct pbuf *pkt,
    struct ip_addr *addr,
    u16_t port)
{
    uint8_t function = 0;
    uint16_t pdu_len = 0;
    uint16_t pdu_offset = 0;
    BACNET_ADDRESS src = { 0 }; /* address where message came from */
    struct ip_addr sin_addr;
    uint16_t sin_port = 0;
    uint8_t *pdu = (uint8_t *)pkt->payload;

    /* the signature of a BACnet/IP packet */
    if ((pkt->tot_len >= 2) &&
        (pdu[0] == BVLL_TYPE_BACNET_IP)) {
        function = pdu[1];
        if ((function == BVLC_ORIGINAL_UNICAST_NPDU) ||
            (function == BVLC_ORIGINAL_BROADCAST_NPDU)) {
            /* ignore messages from me */
            if ((addr->addr == BIP_Address.s_addr) && (port == BIP_Port)) {
                pdu_len = 0;
            } else if (pkt->tot_len >= 4) {
                /* data in src->mac[] is in network format */
                src.mac_len = 6;
                bip_addr_to_mac(&src.mac[0], addr);
                memcpy(&src.mac[4], &port, 2);
                /* decode the length of the PDU
                   length is inclusive of BVLC */
                (void)decode_unsigned16(&pdu[2], &pdu_len);
                if (pdu_len > pkt->tot_len) {
                    /* BVLC length is too long - someone is lying */
                    pdu_len = 0;
                } else {
                    /* subtract off the BVLC header */
                    pdu_len -= 4;
                    pdu_offset = 4;
                }
            }
        } else if (function == BVLC_FORWARDED_NPDU) {
            if (pkt->tot_len >= 10) {
                IP4_ADDR(&sin_addr, pdu[4], pdu[5], pdu[6], pdu[7]);
                decode_unsigned16(&pdu[8], &sin_port);
                if ((sin_addr.addr == BIP_Address.s_addr) &&
                    (sin_port == BIP_Port)) {
                    /* ignore forwarded messages from me */
                    pdu_len = 0;
                } else {
                    /* data in src->mac[] is in network format */
                    src.mac_len = 6;
                    bip_addr_to_mac(&src.mac[0], &sin_addr);
                    memcpy(&src.mac[4], &sin_port, 2);
                    /* decode the length of the PDU
                       length is inclusive of BVLC */
                    (void)decode_unsigned16(&pdu[2], &pdu_len);
                    if (pdu_len > pkt->tot_len) {
                        /* BVLC length is too long - someone is lying */
                        pdu_len = 0;
                    } else {
                        /* subtract off the BVLC header */
                        pdu_len -= 10;
                        pdu_offset = 10;
                    }
                }
            }
        } else if (function == BVLC_WRITE_BROADCAST_DISTRIBUTION_TABLE) {
            bvlc_send_result(
                addr, BVLC_RESULT_WRITE_BROADCAST_DISTRIBUTION_TABLE_NAK);
        } else if (function == BVLC_READ_BROADCAST_DIST_TABLE) {
            bvlc_send_result(
                addr, BVLC_RESULT_READ_BROADCAST_DISTRIBUTION_TABLE_NAK);
        } else if (function == BVLC_REGISTER_FOREIGN_DEVICE) {
            bvlc_send_result(addr, BVLC_RESULT_REGISTER_FOREIGN_DEVICE_NAK);
        } else if (function == BVLC_READ_FOREIGN_DEVICE_TABLE) {
            bvlc_send_result(addr, BVLC_RESULT_READ_FOREIGN_DEVICE_TABLE_NAK);
        } else if (function == BVLC_DELETE_FOREIGN_DEVICE_TABLE_ENTRY) {
            bvlc_send_result(
                addr, BVLC_RESULT_DELETE_FOREIGN_DEVICE_TABLE_ENTRY_NAK);
        } else if (function == BVLC_DISTRIBUTE_BROADCAST_TO_NETWORK) {
            bvlc_send_result(addr, BVLC_RESULT_DISTRIBUTE_BROADCAST_TO_NETWORK_NAK);
        }
        if (pdu_len) {
            BIP_STATS_INC(recv);
            if ((function == BVLC_ORIGINAL_BROADCAST_NPDU) &&
                (npdu_confirmed_service(&pdu[pdu_offset], pdu_len))) {
                /* BTL test: verifies that the IUT will quietly discard any
                   Confirmed-Request-PDU, whose destination address is a
                   multicast or broadcast address, received from the
                   network layer. */
            } else {
                npdu_handler(&src, &pdu[pdu_offset], pdu_len);
            }
        } else {
            BIP_STATS_INC(rxdrop);
        }
    }
    /* free our packet */
    pbuf_free(pkt);
}

void bip_get_my_address(BACNET_ADDRESS *my_address)
{
    int i = 0;

    if (my_address) {
        my_address->mac_len = 6;
        memcpy(&my_address->mac[0], &BIP_Address.s_addr, 4);
        memcpy(&my_address->mac[4], &BIP_Port, 2);
        my_address->net = 0; /* local only, no routing */
        my_address->len = 0; /* no SLEN */
        for (i = 0; i < MAX_MAC_LEN; i++) {
            /* no SADR */
            my_address->adr[i] = 0;
        }
    }

    return;
}

void bip_get_broadcast_address(BACNET_ADDRESS *dest)
{ /* destination address */
    int i = 0; /* counter */

    if (dest) {
        dest->mac_len = 6;
        memcpy(&dest->mac[0], &BIP_Broadcast_Address.s_addr, 4);
        memcpy(&dest->mac[4], &BIP_Port, 2);
        dest->net = BACNET_BROADCAST_NETWORK;
        dest->len = 0; /* no SLEN */
        for (i = 0; i < MAX_MAC_LEN; i++) {
            /* no SADR */
            dest->adr[i] = 0;
        }
    }

    return;
}

/** Initialize the BACnet/IP services at the given interface.
 * @ingroup DLBIP
 * -# Gets the local IP address and local broadcast address from the system,
 *  and saves it into the BACnet/IP data structures.
 * -# Opens a UDP socket
 * -# Configures the socket for sending and receiving
 * -# Configures the socket so it can send broadcasts
 * -# Binds the socket to the local IP address at the specified port for
 *    BACnet/IP (by default, 0xBAC0 = 47808).
 *
 * @note For Windows, ifname is the dotted ip address of the interface.
 *
 * @param ifname [in] The named interface to use for the network layer.
 *        If NULL, the "eth0" interface is assigned.
 * @return True if the socket is successfully opened for BACnet/IP,
 *         else False if the socket functions fail.
 */
bool bip_init(char *ifname)
{
    (void)ifname;
    /* Create a new UDP control block  */
    Server_upcb = udp_new();
    if (Server_upcb == NULL) {
        /* increase MEMP_NUM_UDP_PCB in lwipopts.h */
        while (1) { };
    }
    /* Bind the upcb to the UDP_PORT port */
    /* Using IP_ADDR_ANY allow the upcb to be used by any local interface */
    udp_bind(Server_upcb, IP_ADDR_ANY, BIP_Port);
    /* Set a receive callback for the upcb */
    udp_recv(Server_upcb, bip_server_callback, NULL);

    return true;
}

/*
 *  OpenVPN -- An application to securely tunnel IP networks
 *             over a single TCP/UDP port, with support for SSL/TLS-based
 *             session authentication and key exchange,
 *             packet encryption, packet authentication, and
 *             packet compression.
 *
 *  Copyright (C) 2002-2021 OpenVPN Inc <sales@openvpn.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2
 *  as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/**
 * @file SSL control channel wrap/unwrap and decode functions. This file
 *        (and its .c file) is designed to to be included in units/etc without
 *        pulling in a lot of dependencies
 */

#ifndef SSL_PKT_H
#define SSL_PKT_H

#include "buffer.h"
#include "ssl_backend.h"
#include "ssl_common.h"

/* packet opcode (high 5 bits) and key-id (low 3 bits) are combined in one byte */
#define P_KEY_ID_MASK                  0x07
#define P_OPCODE_SHIFT                 3

/* packet opcodes -- the V1 is intended to allow protocol changes in the future */
#define P_CONTROL_HARD_RESET_CLIENT_V1 1     /* initial key from client, forget previous state */
#define P_CONTROL_HARD_RESET_SERVER_V1 2     /* initial key from server, forget previous state */
#define P_CONTROL_SOFT_RESET_V1        3     /* new key, graceful transition from old to new key */
#define P_CONTROL_V1                   4     /* control channel packet (usually TLS ciphertext) */
#define P_ACK_V1                       5     /* acknowledgement for packets received */
#define P_DATA_V1                      6     /* data channel packet */
#define P_DATA_V2                      9     /* data channel packet with peer-id */

/* indicates key_method >= 2 */
#define P_CONTROL_HARD_RESET_CLIENT_V2 7     /* initial key from client, forget previous state */
#define P_CONTROL_HARD_RESET_SERVER_V2 8     /* initial key from server, forget previous state */

/* indicates key_method >= 2 and client-specific tls-crypt key */
#define P_CONTROL_HARD_RESET_CLIENT_V3 10    /* initial key from client, forget previous state */

/* define the range of legal opcodes
 * Since we do no longer support key-method 1 we consider
 * the v1 op codes invalid */
#define P_FIRST_OPCODE                 3
#define P_LAST_OPCODE                  10

/*
 * Used in --mode server mode to check tls-auth signature on initial
 * packets received from new clients.
 */
struct tls_auth_standalone
{
    struct tls_wrap_ctx tls_wrap;
    struct frame frame;
};

enum first_packet_verdict {
    /** This packet is a valid reset packet from the peer */
    VERDICT_VALID_RESET,
    /** This packet is a valid control packet from the peer,
     * i.e. it has a valid session id hmac in it */
    VERDICT_VALID_CONTROL_V1,
    /** the packet failed on of the various checks */
    VERDICT_INVALID
};

/**
 * struct that stores the temporary data for the tls lite decrypt
 * functions
 */
struct tls_pre_decrypt_state {
    struct tls_wrap_ctx tls_wrap_tmp;
    struct buffer newbuf;
    struct session_id peer_session_id;
};

/**
 *
 * @param state
 */
void free_tls_pre_decrypt_state(struct tls_pre_decrypt_state *state);

/**
 * Inspect an incoming packet for which no VPN tunnel is active, and
 * determine whether a new VPN tunnel should be created.
 * @ingroup data_crypto
 *
 * This function receives the initial incoming packet from a client that
 * wishes to establish a new VPN tunnel, and determines the packet is a
 * valid initial packet.  It is only used when OpenVPN is running in
 * server mode.
 *
 * The tests performed by this function are whether the packet's opcode is
 * correct for establishing a new VPN tunnel, whether its key ID is 0, and
 * whether its size is not too large.  This function also performs the
 * initial HMAC firewall test, if configured to do so.
 *
 * The incoming packet and the local VPN tunnel state are not modified by
 * this function.  Its sole purpose is to inspect the packet and determine
 * whether a new VPN tunnel should be created.  If so, that new VPN tunnel
 * instance will handle processing of the packet.
 *
 * This function is only used in the UDP p2mp server code path
 *
 * @param tas - The standalone TLS authentication setting structure for
 *     this process.
 * @param from - The source address of the packet.
 * @param buf - A buffer structure containing the incoming packet.
 *
 * @return
 * @li True if the packet is valid and a new VPN tunnel should be created
 *     for this client.
 * @li False if the packet is not valid, did not pass the HMAC firewall
 *     test, or some other error occurred.
 */
enum first_packet_verdict
tls_pre_decrypt_lite(const struct tls_auth_standalone *tas,
                     struct tls_pre_decrypt_state *state,
                     const struct link_socket_actual *from,
                     const struct buffer *buf);

/*
 * Write a control channel authentication record.
 */
void
write_control_auth(struct tls_session *session,
                   struct key_state *ks,
                   struct buffer *buf,
                   struct link_socket_actual **to_link_addr,
                   int opcode,
                   int max_ack,
                   bool prepend_ack);


/*
 * Read a control channel authentication record.
 */
bool
read_control_auth(struct buffer *buf,
                  struct tls_wrap_ctx *ctx,
                  const struct link_socket_actual *from,
                  const struct tls_options *opt);

static inline const char *
packet_opcode_name(int op)
{
    switch (op)
    {
    case P_CONTROL_HARD_RESET_CLIENT_V1:
        return "P_CONTROL_HARD_RESET_CLIENT_V1";

    case P_CONTROL_HARD_RESET_SERVER_V1:
        return "P_CONTROL_HARD_RESET_SERVER_V1";

    case P_CONTROL_HARD_RESET_CLIENT_V2:
        return "P_CONTROL_HARD_RESET_CLIENT_V2";

    case P_CONTROL_HARD_RESET_SERVER_V2:
        return "P_CONTROL_HARD_RESET_SERVER_V2";

    case P_CONTROL_HARD_RESET_CLIENT_V3:
        return "P_CONTROL_HARD_RESET_CLIENT_V3";

    case P_CONTROL_SOFT_RESET_V1:
        return "P_CONTROL_SOFT_RESET_V1";

    case P_CONTROL_V1:
        return "P_CONTROL_V1";

    case P_ACK_V1:
        return "P_ACK_V1";

    case P_DATA_V1:
        return "P_DATA_V1";

    case P_DATA_V2:
        return "P_DATA_V2";

    default:
        return "P_???";
    }
}
#endif

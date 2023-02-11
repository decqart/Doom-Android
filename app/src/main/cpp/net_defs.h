//
// Copyright(C) 2005-2014 Simon Howard
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//  Definitions for use in networking code.
//

#ifndef NET_DEFS_H
#define NET_DEFS_H 

#include <stdio.h>

#include "doomtype.h"
#include "d_ticcmd.h"
#include "sha1.h"

// The maximum number of players, multiplayer/networking.
// This is the maximum supported by the networking code; individual games
// have their own values for MAXPLAYERS that can be smaller.

#define NET_MAXPLAYERS 8

// Networking and tick handling related.

#define BACKUPTICS 128

typedef struct _net_module_s net_module_t;
typedef struct _net_packet_s net_packet_t;
typedef struct _net_addr_s net_addr_t;

struct _net_packet_s {
    byte *data;
    size_t len;
    size_t alloced;
    unsigned int pos;
};

struct _net_module_s {
    // Initialize this module for use as a client

    boolean (*InitClient)(void);

    // Initialize this module for use as a server

    boolean (*InitServer)(void);

    // Send a packet

    void (*SendPacket)(net_addr_t *addr, net_packet_t *packet);

    // Check for new packets to receive
    //
    // Returns true if packet received

    boolean (*RecvPacket)(net_addr_t **addr, net_packet_t **packet);

    // Converts an address to a string

    void (*AddrToString)(net_addr_t *addr, char *buffer, int buffer_len);

    // Free back an address when no longer in use

    void (*FreeAddress)(net_addr_t *addr);

    // Try to resolve a name to an address

    net_addr_t *(*ResolveAddress)(char *addr);
};

struct _net_addr_s {
    net_module_t *module;
    void *handle;
};

// packet types

typedef enum
{
    NET_PACKET_TYPE_SYN,
    NET_PACKET_TYPE_ACK,
    NET_PACKET_TYPE_REJECTED,
    NET_PACKET_TYPE_KEEPALIVE,
    NET_PACKET_TYPE_WAITING_DATA,
    NET_PACKET_TYPE_GAMESTART,
    NET_PACKET_TYPE_GAMEDATA,
    NET_PACKET_TYPE_GAMEDATA_ACK,
    NET_PACKET_TYPE_DISCONNECT,
    NET_PACKET_TYPE_DISCONNECT_ACK,
    NET_PACKET_TYPE_RELIABLE_ACK,
    NET_PACKET_TYPE_GAMEDATA_RESEND,
    NET_PACKET_TYPE_CONSOLE_MESSAGE,
    NET_PACKET_TYPE_QUERY,
    NET_PACKET_TYPE_QUERY_RESPONSE,
    NET_PACKET_TYPE_LAUNCH,
} net_packet_type_t;

typedef enum
{
    NET_MASTER_PACKET_TYPE_ADD,
    NET_MASTER_PACKET_TYPE_ADD_RESPONSE,
    NET_MASTER_PACKET_TYPE_QUERY,
    NET_MASTER_PACKET_TYPE_QUERY_RESPONSE,
    NET_MASTER_PACKET_TYPE_GET_METADATA,
    NET_MASTER_PACKET_TYPE_GET_METADATA_RESPONSE,
    NET_MASTER_PACKET_TYPE_SIGN_START,
    NET_MASTER_PACKET_TYPE_SIGN_START_RESPONSE,
    NET_MASTER_PACKET_TYPE_SIGN_END,
    NET_MASTER_PACKET_TYPE_SIGN_END_RESPONSE,
} net_master_packet_type_t;

// Settings specified when the client connects to the server.

typedef struct
{
    int gamemode;
    int gamemission;
    int lowres_turn;
    int drone;
    int max_players;
    int is_freedoom;
    sha1_digest_t wad_sha1sum;
    sha1_digest_t deh_sha1sum;
    int player_class;
} net_connect_data_t;

// Game settings sent by client to server when initiating game start,
// and received from the server by clients when the game starts.

typedef struct
{
    int ticdup;
    int extratics;
    int deathmatch;
    int episode;
    int nomonsters;
    int fast_monsters;
    int respawn_monsters;
    int map;
    int skill;
    int gameversion;
    int lowres_turn;
    int new_sync;
    int timelimit;
    int loadgame;
    int random;  // [Strife only]

    // These fields are only used by the server when sending a game
    // start message:

    int num_players;
    int consoleplayer;

    // Hexen player classes:

    int player_classes[NET_MAXPLAYERS];

} net_gamesettings_t;

// Data sent by the server while waiting for the game to start.

#endif /* NET_DEFS_H */

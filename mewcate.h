#ifndef MEWCATE_H
#define MEWCATE_H

#include "xmcomp/common.h"

#include "room.h"
#include "builder.h"
#include "router.h"

BOOL mewcate_handle(Room *room,
		ParticipantEntry *sender, ParticipantEntry *receiver,
		BuilderPacket *egress, SendCallback *send);

#endif

#ifndef MEWCAT_H
#define MEWCAT_H

#include "xmcomp/src/common.h"

#include "room/room.h"
#include "builder.h"

BOOL mewcat_handle(Room *room,
		Participant *sender, Participant *receiver,
		BuilderPacket *egress);

#endif

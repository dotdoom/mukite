#ifndef COMPONENT_H
#define COMPONENT_H

typedef struct {
	Rooms rooms;
	ACLConfig acl;
	BufferPtr hostname;
} Component;

void component_process(IncomingPacket *);

#endif

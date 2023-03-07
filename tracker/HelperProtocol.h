#ifndef _HELPER_PROTOCOL_H_
#define _HELPER_PROTOCOL_H_

#include <OS.h>
#include <SupportDefs.h>
#include <Entry.h>

#define HELPER_PORT_NAME	"TrackerHelper"

class BNode;

namespace BPrivate {

enum {
	HELPER_HANDSHAKE = 1,
	HELPER_GET_ICON = 2,
	HELPER_STATUS = 3
};

typedef struct extended_icon_info_s {
	port_id		notify;

	// icon specifics
	int32		size;
	entry_ref	ref;
	char		name[1024];
	char		prefix[1024];
	char		suffix[1024];
} IconRequest;

} // namespace BPrivate

#endif

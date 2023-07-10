#ifndef OUTER_H
#define OUTER_H

#include "sfo.h"
#include "util.h"
#include "fault.h"

#define IRD_LINK "http://ps3ird.free.fr"
#define IRD_REQ "script.php?ird"

#define PUP_LINK "http://archive.midnightchannel.net"
#define PUP_REQ "SonyPS/Firmware/?cat=CEX&disc=1&ver"

error_state_t download_ird(sfo_t *sfo, char *ird_path);
error_state_t download_pup(sfo_t *sfo, char *pup_path);

#endif

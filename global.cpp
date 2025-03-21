#include "global.h"


const char* ERROR[] = { "UNABLE", "BUS BUSY", "BUS ERROR",
                       "BUFFER FULL", "CAN ERROR", "DATA ERROR",
                       "ERROR", "STOPPED", "TIMEOUT", "?", "SEARCH", "NODATA",
                       "NO DATA", "UNABLETOCONNECT", "<", ">" };

QStringList initializeCommands = {LINEFEED_OFF, ECHO_OFF, HEADERS_OFF, ADAPTIF_TIMING_AUTO2, PROTOCOL_ISO_9141_2, MONITOR_STATUS};
QStringList runtimeCommands = {};
int interval = 250;  // Default value

//<- Pids:  0104,0105,010B,010C,010D,010F,0110,0111,011C

#include "global.h"


const char* ERROR[] = { "UNABLE", "BUS INIT", "BUS BUSY", "BUS ERROR",
                       "BUFFER FULL", "CAN ERROR", "DATA ERROR", "NO DATA",
                       "ERROR", "STOPPED", "TIMEOUT", "?", "SEARCH", "NODATA",
                       "NO DATA", "UNABLETOCONNECT", "<", ">" };

QStringList initializeCommands = {LINEFEED_OFF, ECHO_OFF, HEADERS_OFF, ADAPTIF_TIMING_AUTO2, PROTOCOL_ISO_9141_2, MONITOR_STATUS};
QStringList runtimeCommands = {};
int interval = 100;  // Default value

#include "global.h"


const char* ERROR[] = { "UNABLE", "BUS INIT", "BUS BUSY", "BUS ERROR",
                       "BUFFER FULL", "CAN ERROR", "DATA ERROR", "NO DATA",
                       "ERROR", "STOPPED", "TIMEOUT", "?", "SEARCH", "NODATA",
                       "NO DATA", "UNABLETOCONNECT", "<", ">" };

QStringList initializeCommands = {ECHO_OFF, LINEFEED_OFF, HEADERS_ON, TIMEOUT_100, ADAPTIF_TIMING_AUTO2, PROTOCOL_AUTO, MONITOR_STATUS};
QStringList runtimeCommands = {};
int interval = 100;  // Default value

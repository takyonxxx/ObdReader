#include "global.h"


const char* ERROR[] = { "UNABLE", "BUS BUSY", "BUS ERROR",
                       "BUFFER FULL", "CAN ERROR", "DATA ERROR",
                       "ERROR", "STOPPED", "TIMEOUT", "?", "SEARCH", "NODATA",
                       "NO DATA", "UNABLETOCONNECT", "<", ">" };

QStringList initializeCommands = {
    LINEFEED_OFF,          // ATL0 - Turn off linefeed
    ECHO_OFF,              // ATE0 - Turn off echo
    HEADERS_OFF,           // ATH0 - Turn off headers
    ADAPTIF_TIMING_AUTO2,  // ATAT2 - Adaptive timing
    PROTOCOL_ISO_9141_2,   // ATSP3 - Set protocol to ISO 9141-2
    MONITOR_STATUS         // AT MA - Monitor all
};
QStringList runtimeCommands = {};
int interval = 250;  // Default value

//<- Pids:  0104,0105,010B,010C,010D,010F,0110,0111,011C

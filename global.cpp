#include "global.h"
const char* ERROR[] = { "UNABLE", "BUS BUSY", "BUS ERROR",
                       "BUFFER FULL", "CAN ERROR", "DATA ERROR",
                       "ERROR", "STOPPED", "TIMEOUT", "?", "SEARCH", "NODATA",
                       "NO DATA", "UNABLETOCONNECT", "<", ">" };

// Global variable definitions - put these in your implementation file
// QList<ELM327Command> initializeCommands = {
//     ELM327Command("ATZ", "ELM327", 7500),           // Reset ELM327
//     ELM327Command("ATSP3", "OK"),                   // Set protocol to ISO 9141-2/KWP2000
//     ELM327Command("ATWM8115F13E", "OK"),            // Set wakeup message
//     ELM327Command("ATSH8115F1", "OK"),              // Set header
//     ELM327Command("ATFI", "BUS INIT: OK"),          // Fast initialization
//     ELM327Command("81", "C1 EF 8F"),                // Communication test
//     ELM327Command("27 01", "67 01"),                // Security access request
//     ELM327Command("27 02 CD 46", "7F 27"),          // Security access with key
//     ELM327Command("31 25 00", "71 25")              // Start diagnostic session
// };

QStringList runtimeCommands = {};
int interval = 250;  // Default polling interval

QList<ELM327Command> initializeCommands = {
    ELM327Command("ATZ", "ELM327", interval),           // Reset ELM327
    ELM327Command("ATE0", "OK", interval),              // Echo off
    ELM327Command("ATL0", "OK", interval),              // Linefeed off
    ELM327Command("ATH0", "OK", interval),              // Headers off
    ELM327Command("ATS0", "OK", interval),              // Spaces off
    ELM327Command("ATSP3", "OK", interval),             // Set protocol to ISO 9141-2
    ELM327Command("ATIB10", "OK", interval),            // Set ISO baud rate to 10400
    ELM327Command("ATIIA13", "OK", interval),           // Set ISO init address to 0x13 (common for many cars)
    ELM327Command("ATWS", "OK", interval),              // Warm start - this forces the 5-baud init
    ELM327Command("ATDP", "", interval),                // Verify protocol
};



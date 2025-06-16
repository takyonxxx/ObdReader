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

QList<ELM327Command> initializeCommands = {
    ELM327Command("ATZ", "ELM327", 7500),           // Reset ELM327
    ELM327Command("ATE0", "OK", 1000),              // Echo off
    ELM327Command("ATL0", "OK", 1000),              // Linefeed off
    ELM327Command("ATH0", "OK", 1000),              // Headers off
    ELM327Command("ATS0", "OK", 1000),              // Spaces off
    ELM327Command("ATSP3", "OK", 1000),             // Set protocol to ISO 9141-2
    ELM327Command("ATIB10", "OK", 1000),            // Set ISO baud rate to 10400
    ELM327Command("ATIIA13", "OK", 1000),           // Set ISO init address to 0x13 (common for many cars)
    ELM327Command("ATWS", "OK", 3000),              // Warm start - this forces the 5-baud init
    ELM327Command("ATDP", "", 1000),                // Verify protocol
};

QStringList runtimeCommands = {};
int interval = 250;  // Default polling interval

// Optional: Keep your original simple command list for basic initialization
QStringList basicInitCommands = {
    "ATZ",      // Reset
    "ATE0",     // Echo off
    "ATL0",     // Linefeed off
    "ATH0",     // Headers off
    "ATSP3",    // ISO 9141-2 protocol
    "ATAT2"     // Adaptive timing
};

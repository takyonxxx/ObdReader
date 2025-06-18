#include "elm.h"
#include "connectionmanager.h"

std::string DecimalToBinaryString(unsigned long a)
{
    uint b = static_cast<uint>(a);
    std::string binary = "";
    uint mask = 0x80000000u;
    while (mask > 0)
    {
        binary += ((b & mask) == 0) ? '0' : '1';
        mask >>= 1;
    }
    return binary;
}

ELM* ELM::theInstance_ = nullptr;

ELM *ELM::getInstance()
{
    if (theInstance_ == nullptr)
    {
        theInstance_ = new ELM();
    }
    return theInstance_;
}

ELM::ELM()
{
    // Initialize map for DTC prefix lookup
    dtcPrefix['0'] = "P0";
    dtcPrefix['1'] = "P1";
    dtcPrefix['2'] = "P2";
    dtcPrefix['3'] = "P3";
    dtcPrefix['4'] = "C0";
    dtcPrefix['5'] = "C1";
    dtcPrefix['6'] = "C2";
    dtcPrefix['7'] = "C3";
    dtcPrefix['8'] = "B0";
    dtcPrefix['9'] = "B1";
    dtcPrefix['A'] = "B2";
    dtcPrefix['B'] = "B3";
    dtcPrefix['C'] = "U0";
    dtcPrefix['D'] = "U1";
    dtcPrefix['E'] = "U2";
    dtcPrefix['F'] = "U3";

    resetPids();
}

std::vector<QString> ELM::prepareResponseToDecode(const QString &response_str)
{
    std::vector<QString> result;
    result.reserve(16); // Reserve space for typical response size

    // Clean the input string
    QString cleanedResponse = response_str.trimmed();

    // Special case for raw CAN responses like "7E8034105F"
    if (cleanedResponse.startsWith("7E", Qt::CaseInsensitive) && !cleanedResponse.contains(" ")) {
        // Insert spaces for better processing (7E8034105F -> 7E8 03 41 05 5F)
        QString spacedResponse;

        // First 3 characters are usually the ECU ID
        if (cleanedResponse.length() >= 3) {
            spacedResponse = cleanedResponse.left(3) + " ";
        }

        // Insert spaces every 2 characters after the ECU ID
        for (int i = 3; i < cleanedResponse.length(); i += 2) {
            if (i + 1 < cleanedResponse.length()) {
                spacedResponse += cleanedResponse.mid(i, 2) + " ";
            } else {
                spacedResponse += cleanedResponse.mid(i, 1);
            }
        }

        cleanedResponse = spacedResponse.trimmed();
    }

    // Handle CAN format responses (e.g., "7E8 03 41 0C 20 00")
    if (cleanedResponse.startsWith("7E", Qt::CaseInsensitive) && cleanedResponse.contains(" ")) {
        QStringList parts = cleanedResponse.split(" ", Qt::SkipEmptyParts);

        // Check if it's a valid CAN response with enough parts
        if (parts.size() >= 3) {
            // Find index of mode response (41, 42, etc.)
            int modeIndex = -1;
            for (int i = 0; i < parts.size(); i++) {
                if (parts[i].startsWith("4", Qt::CaseInsensitive) && parts[i].length() == 2) {
                    // Mode responses are 41 (mode 1), 42 (mode 2), etc.
                    char secondChar = parts[i].at(1).toLatin1();
                    if ((secondChar >= '0' && secondChar <= '9') ||
                        (secondChar >= 'A' && secondChar <= 'F')) {
                        modeIndex = i;
                        break;
                    }
                }
            }

            // If we found a mode response, extract it and following data
            if (modeIndex >= 0) {
                // Extract mode, PID, and data bytes
                for (int i = modeIndex; i < parts.size(); i++) {
                    if (!parts[i].isEmpty()) {
                        result.push_back(parts[i]);
                    }
                }
                return result;
            }
        }
    }

    // Handle ISO_14230_4_KWP_FAST, ISO 14230-4 (KWP2000), and generic responses
    // These typically come in formats like "41 0C 20 00" without header bytes
    if (cleanedResponse.contains(QRegularExpression("^[0-9A-F]{2} [0-9A-F]{2}",
                                                    QRegularExpression::CaseInsensitiveOption))) {
        QStringList parts = cleanedResponse.split(" ", Qt::SkipEmptyParts);

        // Check if it's a valid OBD response
        if (parts.size() >= 2) {
            // Handle standard responses
            if (parts[0].startsWith("4", Qt::CaseInsensitive) && parts[0].length() == 2) {
                // Add all parts to result
                for (const auto& part : parts) {
                    if (!part.isEmpty()) {
                        result.push_back(part);
                    }
                }

                return result;
            }
        }
    }

    // Handle SAE J1850 PWM, SAE J1850 VPW, and raw unspaced responses
    // These might come as continuous hex strings like "410C2000"
    if (cleanedResponse.contains(QRegularExpression("^[0-9A-F]{4,}",
                                                    QRegularExpression::CaseInsensitiveOption))) {
        // Check if it starts with a valid mode response
        if (cleanedResponse.startsWith("41", Qt::CaseInsensitive) ||
            cleanedResponse.startsWith("42", Qt::CaseInsensitive) ||
            cleanedResponse.startsWith("43", Qt::CaseInsensitive) ||
            cleanedResponse.startsWith("44", Qt::CaseInsensitive) ||
            cleanedResponse.startsWith("45", Qt::CaseInsensitive) ||
            cleanedResponse.startsWith("46", Qt::CaseInsensitive)) {

            // Split into 2-character chunks
            for (int i = 0; i < cleanedResponse.length(); i += 2) {
                if (i + 1 < cleanedResponse.length()) {
                    result.push_back(cleanedResponse.mid(i, 2));
                }
            }
            return result;
        }
    }

    // Handle multiline responses and merge them if needed
    if (cleanedResponse.contains(">") || cleanedResponse.contains("\r")) {
        QStringList lines = cleanedResponse.split(QRegularExpression("[>\r\n]"), Qt::SkipEmptyParts);
        QString mergedResponse;

        for (const auto& line : lines) {
            mergedResponse += line.trimmed() + " ";
        }

        // Recursively process the merged response
        return prepareResponseToDecode(mergedResponse.trimmed());
    }

    // Final fallback - if we can't identify the format but the string is longer than 4 chars
    // Try to extract a mode response (41, 42, etc.) from anywhere in the string
    if (cleanedResponse.length() > 4) {
        for (int i = 0; i < cleanedResponse.length() - 1; i++) {
            QString potential = cleanedResponse.mid(i, 2);
            if (potential.startsWith("4", Qt::CaseInsensitive)) {
                char secondChar = potential.at(1).toLatin1();
                if ((secondChar >= '0' && secondChar <= '9') ||
                    (secondChar >= 'A' && secondChar <= 'F')) {

                    // Found a potential mode response - extract from here to the end
                    QString subResponse = cleanedResponse.mid(i);

                    // Split into 2-character chunks
                    for (int j = 0; j < subResponse.length(); j += 2) {
                        if (j + 1 < subResponse.length()) {
                            result.push_back(subResponse.mid(j, 2));
                        }
                    }

                    if (!result.empty()) {
                        return result;
                    }
                }
            }
        }
    }

    // Absolute last resort - just split the entire string into 2-char chunks
    if (result.empty()) {
        for (int i = 0; i < cleanedResponse.length(); i += 2) {
            if (i + 1 < cleanedResponse.length()) {
                result.push_back(cleanedResponse.mid(i, 2));
            } else if (i < cleanedResponse.length()) {
                result.push_back(cleanedResponse.mid(i, 1));
            }
        }
    }  

    return result;
}

QString ELM::getLastHeader() const
{
    return m_lastHeader;
}

void ELM::setLastHeader(const QString &newLastHeader)
{
    m_lastHeader = newLastHeader;
}

std::vector<QString> ELM::decodeDTC(const std::vector<QString> &hex_vals)
{
    std::vector<QString> dtc_codes;

    // Process hex values in pairs (each DTC is encoded in 2 bytes)
    for(size_t i = 0; i < hex_vals.size(); i += 2)
    {
        // Make sure we have a complete pair
        if(i + 1 >= hex_vals.size())
            break;

        QString byte1 = hex_vals[i];
        QString byte2 = hex_vals[i + 1];

        // Skip empty codes
        if(byte1.isEmpty() || byte2.isEmpty())
            continue;

        // Skip "00" values which indicate no DTC data
        if (byte1 == "00" && byte2 == "00")
            continue;

        // Get first character to determine DTC type (P, C, B, U)
        char firstChar = byte1[0].toLatin1();

        // Convert the first nibble to determine code type
        int typeValue = 0;
        if(firstChar >= '0' && firstChar <= '9')
            typeValue = firstChar - '0';
        else if(firstChar >= 'A' && firstChar <= 'F')
            typeValue = 10 + (firstChar - 'A');

        // Use the map for prefix lookup if available
        QString dtc_code;
        auto itMap = dtcPrefix.find(firstChar);
        if (itMap != dtcPrefix.end()) {
            dtc_code = (*itMap).second;

            // Add the rest of the first byte (excluding the first character)
            if(byte1.length() > 1) {
                dtc_code.append(byte1.right(byte1.length() - 1));
            }

            // Add the second byte
            dtc_code.append(byte2);
        } else {
            // Fallback to the calculation method
            QString prefix;
            switch(typeValue >> 2) {  // Get the first 2 bits of the nibble
            case 0: prefix = "P"; break;
            case 1: prefix = "C"; break;
            case 2: prefix = "B"; break;
            case 3: prefix = "U"; break;
            default: prefix = "P"; break;
            }

            // Build the DTC code
            dtc_code = prefix;

            // Add the second bit of the first nibble to determine if standard or manufacturer specific
            dtc_code.append(QString::number((typeValue >> 1) & 0x01));

            // Add the rest of the first byte (excluding the first nibble we already processed)
            if(byte1.length() > 1) {
                dtc_code.append(byte1.right(byte1.length() - 1));
            }

            // Add the second byte
            dtc_code.append(byte2);
        }

        // Don't add P0000 (no trouble code)
        if(dtc_code.compare("P0000", Qt::CaseInsensitive) != 0) {
            dtc_codes.push_back(dtc_code);
        }
    }

    return dtc_codes;
}

std::pair<int, bool> ELM::decodeNumberOfDtc(const std::vector<QString> &hex_vals)
{
    int dtcNumber = 0;
    bool milOn = false;

    if(hex_vals.empty())
        return std::make_pair(dtcNumber, milOn);

    // First byte contains both MIL status (bit 7) and number of DTCs (bits 0-6)
    try {
        int firstByte = std::stoi(hex_vals[0].toStdString(), nullptr, 16);

        // Check if MIL is on (bit 7 set)
        milOn = (firstByte & 0x80) != 0;

        // DTCs are in bits 0-6
        dtcNumber = firstByte & 0x7F;
    } catch (const std::exception& e) {
        // Handle conversion error
        qDebug() << "Error converting hex value:" << hex_vals[0] << " - " << e.what();
    }

    return std::make_pair(dtcNumber, milOn);
}

void ELM::resetPids()
{
    // initialize supported pid list
    for (int h = 0; h < 256; h++) {
        available_pids[h] = false;
    }
    available_pids_checked = false;
}

QString ELM::get_available_pids()
{
    if (!available_pids_checked) {
        update_available_pids();
    }

    QString data = "";
    bool first = true;
    for (int i = 1; i <= 255; i++)  // Start from 1, not 0
    {
        if (available_pids[i])
        {
            QString hexvalue = QString("01") + QString("%1").arg(i, 2, 16, QLatin1Char( '0' ));

            // Skip the PID availability commands
            if(hexvalue != "0101" &&
                hexvalue != "0120" &&
                hexvalue != "0140" &&
                hexvalue != "0160" &&
                hexvalue != "0180" &&
                hexvalue != "01A0" &&
                hexvalue != "01C0")
            {
                if (first)
                {
                    first = false;
                }
                else
                {
                    data.append(",");
                }

                data.append(hexvalue.toUpper());
            }
        }
    }
    return data;
}

void ELM::update_available_pids()
{
    resetPids();

    available_pids[0] = false; // PID0 is always supported and can't be checked for support
    update_available_pidset(1);

    // Check if pid 0x20 is available (meaning next set is supported)
    if (available_pids[0x20]) {
        update_available_pidset(2);
        if (available_pids[0x40]) {
            update_available_pidset(3);
            if (available_pids[0x60]) {
                update_available_pidset(4);
                if (available_pids[0x80]) {
                    update_available_pidset(5);
                    if (available_pids[0xA0]) {
                        update_available_pidset(6);
                        if (available_pids[0xC0]) {
                            update_available_pidset(7);
                        }
                    }
                }
            }
        }
    }
    available_pids_checked = true;
}

QString ELM::cleanData(const QString& input)
{
    QString cleaned = input.trimmed().simplified();
    cleaned.remove(QRegularExpression(QString("[\\n\\t\\r]")));
    cleaned.remove(QRegularExpression(QString("[^a-zA-Z0-9]+")));
    return cleaned;
}

void ELM::update_available_pidset(quint8 set)
{
    QString cmd1;

    // Select command
    switch (set) {
    case 1:
        cmd1 = "0100";
        break;
    case 2:
        cmd1 = "0120";
        break;
    case 3:
        cmd1 = "0140";
        break;
    case 4:
        cmd1 = "0160";
        break;
    case 5:
        cmd1 = "0180";
        break;
    case 6:
        cmd1 = "01A0";
        break;
    case 7:
        cmd1 = "01C0";
        break;
    default:
        cmd1 = "0100";
        break;
    }

    // Get PID support data
    QString cmd{};
    int retryCount = 0;
    const int maxRetries = 3;

    while(cmd.isEmpty() && retryCount < maxRetries)
    {
        cmd = ConnectionManager::getInstance()->readData(cmd1).toUpper();
        cmd = cleanData(cmd);
        retryCount++;

        if (cmd.isEmpty()) {
            // Add a small delay before retry
            QThread::msleep(100);
        }
    }

    if(!cmd.startsWith(QString("41")))
    {
        // If we can't get proper PID support data, set some common PIDs as available
        if (set == 1) {  // Only set defaults for the first set
            available_pids[3]  = true;  // 04  Calculated engine load
            available_pids[4]  = true;  // 05  Engine coolant temperature
            available_pids[10] = true;  // 0B  Intake manifold absolute pressure
            available_pids[11] = true;  // 0C  Engine RPM
            available_pids[12] = true;  // 0D  Vehicle speed
            available_pids[15] = true;  // 0F  Maf air flow rate
        }
        return;
    }

    auto list = cmd.split("41");
    for(auto &item: list)
    {
        if (item.isEmpty())
            continue;

        QString setPart;
        if (item.length() >= 2) {
            setPart = item.mid(0, 2);
        } else {
            continue;  // Skip invalid data
        }

        // trim to continuous 32bit hex string
        QString dataPart;
        if (item.length() > 2) {
            dataPart = item.mid(2);
        } else {
            continue;  // Skip if no data part
        }

        try {
            const char *str;
            QByteArray byteArray = dataPart.toLatin1();
            str = byteArray.data();
            unsigned long longData = strtoul(str, nullptr, 16);

            if (errno == ERANGE) {
                qDebug() << "Error: Number out of range when converting" << dataPart;
                continue;
            }

            auto binaryString = DecimalToBinaryString(longData);
            int m = (set-1) * 32;

            // fill supported pid list
            for (int i = 0; i < binaryString.length() && i < 32; i++) {
                if (binaryString[i] == '1') {
                    available_pids[i+m+1] = true;  // +1 because PID numbers start at 1, not 0
                }
            }
        } catch (const std::exception& e) {
            qDebug() << "Error processing PID data:" << e.what();
        }
    }
}

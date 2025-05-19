//
// Created by DVIR on 3/23/2025.
//

#include "timeUtil.h"
std::string timeUtil::convertSecondsToTime(const int total_seconds) {
    int hours = total_seconds / 3600;
    int minutes = (total_seconds % 3600) / 60;
    int seconds = total_seconds % 60;

    std::ostringstream oss;
    oss << (hours < 10 ? "0" : "") << hours << ":"
        << (minutes < 10 ? "0" : "") << minutes << ":"
        << (seconds < 10 ? "0" : "") << seconds;

    return oss.str();
}
int timeUtil::calcTimeInSeconds(std::string time) {
    time = timeUtil::trim(time); // Remove any leading/trailing whitespace
    // Find positions of the colons
    size_t pos1 = time.find(':');
    size_t pos2 = time.find(':', pos1 + 1);

    if (pos1 == std::string::npos || pos2 == std::string::npos) {
        std::cerr << "Invalid time format!" << std::endl;
        return 1;
    }

    // Extract hours, minutes, and seconds substrings
    std::string hour_str = time.substr(0, pos1);
    std::string min_str = time.substr(pos1 + 1, pos2 - pos1 - 1);
    std::string sec_str = time.substr(pos2 + 1);

    int hours = 0, minutes = 0, seconds = 0;

    // Use std::from_chars for fast conversion from string to int
    std::from_chars(hour_str.data(), hour_str.data() + hour_str.size(), hours);
    std::from_chars(min_str.data(), min_str.data() + min_str.size(), minutes);
    std::from_chars(sec_str.data(), sec_str.data() + sec_str.size(), seconds);


    // Compute total seconds
    int total_seconds = hours * 3600 + minutes * 60 + seconds;
    return total_seconds;
}
std::string timeUtil::trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\n\r");
    if (start == std::string::npos)
        return "";
    size_t end = str.find_last_not_of(" \t\n\r");
    return str.substr(start, end - start + 1);
}

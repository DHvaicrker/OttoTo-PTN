//
// Created by DVIR on 3/23/2025.
//

#ifndef TIMEUTIL_H
#define TIMEUTIL_H
#include <charconv>
#include <iostream>
#include <sstream>


class timeUtil {
public:
    static std::string trim(const std::string& str);
    static int calcTimeInSeconds(std::string time);
    static std::string convertSecondsToTime(int total_seconds) ;
};



#endif //TIMEUTIL_H

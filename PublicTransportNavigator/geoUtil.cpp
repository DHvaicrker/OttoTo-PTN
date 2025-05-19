#include "geoUtil.h"

#include <iostream>
#include <string>
#include <vector>
#include <cmath>

// Base32 alphabet used in geohashing
static const std::string base32_chars = "0123456789bcdefghjkmnpqrstuvwxyz";

// --- Function 1: Encode Geohash ---
std::string Geohash::encodeGeohash(double latitude, double longitude, int precision) {
    double lat_min = -90.0, lat_max = 90.0;
    double lon_min = -180.0, lon_max = 180.0;
    std::string geohash;
    bool is_even = true;
    int bit = 0;
    int ch = 0;

    while (geohash.size() < static_cast<size_t>(precision)) {
        double mid;
        if (is_even) {
            mid = (lon_min + lon_max) / 2;
            if (longitude > mid) {
                ch |= 1 << (4 - bit);
                lon_min = mid;
            } else {
                lon_max = mid;
            }
        } else {
            mid = (lat_min + lat_max) / 2;
            if (latitude > mid) {
                ch |= 1 << (4 - bit);
                lat_min = mid;
            } else {
                lat_max = mid;
            }
        }
        is_even = !is_even;
        if (bit < 4) {
            ++bit;
        } else {
            geohash.push_back(base32_chars[ch]);
            bit = 0;
            ch = 0;
        }
    }
    return geohash;
}


// --- Function 2: Decode a Geohash into a Bounding Box ---
BoundingBox  Geohash::decodeGeohash(const std::string &geohash) {
    double lat_min = -90.0, lat_max = 90.0;
    double lon_min = -180.0, lon_max = 180.0;
    bool is_even = true;
    for (char c : geohash) {
        int cd = base32_chars.find(c);
        for (int mask = 16; mask; mask >>= 1) {
            if (is_even) {
                double mid = (lon_min + lon_max) / 2;
                if (cd & mask) {
                    lon_min = mid;
                } else {
                    lon_max = mid;
                }
            } else {
                double mid = (lat_min + lat_max) / 2;
                if (cd & mask) {
                    lat_min = mid;
                } else {
                    lat_max = mid;
                }
            }
            is_even = !is_even;
        }
    }
    return {lat_min, lat_max, lon_min, lon_max};
}

// --- Function 3: Get Adjacent (Neighbor) Geohashes ---
std::vector<std::string>  Geohash::getGeohashNeighbors(const std::string &geohash) {
    // return the 8 directions includoing the geohash input box so 9 in total
    BoundingBox bbox = decodeGeohash(geohash);
    double lat_center = (bbox.lat_min + bbox.lat_max) / 2;
    double lon_center = (bbox.lon_min + bbox.lon_max) / 2;
    double lat_err = (bbox.lat_max - bbox.lat_min) / 2;
    double lon_err = (bbox.lon_max - bbox.lon_min) / 2;

    std::vector<std::string> neighbors;
    // Offsets: north, south, east, west, NE, NW, SE, SW
    std::vector<std::pair<double, double>> offsets = {
        { lat_err,      0          },   // north
        {-lat_err,      0          },   // south
        { 0,            lon_err    },   // east
        { 0,           -lon_err    },   // west
        { lat_err,      lon_err    },   // northeast
        { lat_err,     -lon_err    },   // northwest
        {-lat_err,      lon_err    },   // southeast
        {-lat_err,     -lon_err    }    // southwest
    };

    for (const auto &offset : offsets) {
        double neighbor_lat = lat_center + offset.first;
        double neighbor_lon = lon_center + offset.second;
        std::string neighbor_hash = encodeGeohash(neighbor_lat, neighbor_lon, geohash.size());
        neighbors.push_back(neighbor_hash);
    }
    neighbors.push_back(geohash); // push also the middle box => the user box

    return neighbors;
}


// Haversine formula (in meters)
double haversineDistance(double lat1, double lon1, double lat2, double lon2) {
    const double R = 6371000; // Earth radius in meters
    double dLat = (lat2 - lat1) * M_PI / 180.0;
    double dLon = (lon2 - lon1) * M_PI / 180.0;
    double a = sin(dLat/2)*sin(dLat/2) +
               cos(lat1 * M_PI / 180.0) * cos(lat2 * M_PI / 180.0) *
               sin(dLon/2)*sin(dLon/2);
    double c = 2 * atan2(sqrt(a), sqrt(1 - a));
    return R * c;
}

// Walking time: speed is 3 km/h => 1.111 m/s
int calculateWalkTime(double distanceMeters) {
    double speed = 1.111; // m/s
    return distanceMeters / speed; // calculate in seconds
}


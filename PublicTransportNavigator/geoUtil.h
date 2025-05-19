//
// Created by DVIR on 3/20/2025.
//

#ifndef GEOHASH_H
#define GEOHASH_H
#include <string>
#include <vector>


// --- Helper: Bounding Box Structure ---
struct BoundingBox {
    double lat_min, lat_max, lon_min, lon_max;
};
class Geohash {
    public:
    Geohash();
    ~Geohash();
   static std::string encodeGeohash(double latitude, double longitude, int precision );
    static BoundingBox decodeGeohash(const std::string &geohash);
    static std::vector<std::string> getGeohashNeighbors(const std::string &geohash);
};
double haversineDistance(double lat1, double lon1, double lat2, double lon2);
int calculateWalkTime(double distanceMeters) ;
#endif //GEOHASH_H

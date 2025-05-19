//
// Created by DVIR on 3/16/2025.
//

#ifndef PREPROCESS_H
#define PREPROCESS_H

#define NUM_OF_STOPS 51229 // this is diffrent than stops which have thier id start from 1 i want it to start from 0
#define NUM_OF_ALGO_ROUTES 7377
#define NUM_OF_DAYS 7
#define NUM_OF_REAL_TRIPS 282523 // the actual size is 282523  i decide such that the id would start from 0 not 1

#define GEO_HASH_PRESITION 5
#define MAX_WALK_DISTANCE 1000 // ie 1 km

#include <charconv>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include "transport_generated.h"
#include  <unordered_map>
#include <vector>
#include "geoUtil.h"
#include <chrono>
#include <iomanip>

struct TripStop {
    int   id;
    int stopSeqIndex;
    int  depTime;
    int arrTime;

    // Equality operator for TripStop
    bool operator==(const TripStop& other) const {
        return id == other.id && stopSeqIndex == other.stopSeqIndex;
    }
};

struct ARouteStop {
    int   id;
    int stopSeqIndex;
    // Constructor that initializes ARouteStop from a TripStop
    ARouteStop(const TripStop& tripStop)
        : id(tripStop.id), stopSeqIndex(tripStop.stopSeqIndex) {}
    // Equality operator for ARouteStop
    bool operator==(const ARouteStop& other) const {
        return id == other.id && stopSeqIndex == other.stopSeqIndex;
    }

};
struct ATrip{
    int tripId;
    int startDate;
    int endDate;
    std::string lineName; // could be a bus number or names of 2 train stations
};

struct StopData {
    int id;
    std::string name;
    double lat;
    double lon;
    std::string geohash;
};
struct Footpath {
    int otherStopId;
    int walkTime;
};

struct AStop {
    std::vector<int> routes; // route ids that serve this stop
    std::vector<Footpath> footpaths;
};



struct MyService {
    int startDate;
    int endDate;
    std::array<int,NUM_OF_DAYS> weekArr;
};
// Custom hash for a single ARouteStop, i dont need those in my algorithm, it is for the preprosccing to group toghther trips under a route
struct RouteStopHash {
    std::size_t operator()(const ARouteStop& ts) const {
        // Combine the two integers' hash values
        std::size_t h1 = std::hash<int>()(ts.id);
        std::size_t h2 = std::hash<int>()(ts.stopSeqIndex);
        // Combine using a hash-combine formula (similar to boost::hash_combine)
        std::size_t seed = h1;
        seed ^= h2 + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
        return seed;
    }
};

// Custom hash for a vector of TripStop
struct VectorRouteStopHash {
    std::size_t operator()(const std::vector<ARouteStop>& vec) const {
        std::size_t seed = vec.size();
        for (const auto& ts : vec) {
            // Use RouteStopHash to hash each element and combine it
            std::size_t h = RouteStopHash()(ts);
            seed ^= h + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
        }
        return seed;
    }
};
struct MyTrip {
    int tripId;
    int serviceId;
    std::string lineName;
    MyTrip& operator=(const MyTrip& other) {
        if (this != &other) { // Check for self-assignment
            tripId = other.tripId;
            serviceId = other.serviceId;
             lineName = other.lineName;
        }
        return *this;
    }
};


struct StopsComparator { // compare stops by their departure time
    bool operator()(const TripStop& lhs,const TripStop& rhs) const {
        return lhs.stopSeqIndex < rhs.stopSeqIndex; // Greater-than for descending order
    }
};

template <typename T1, typename T2, typename T3>
struct Triple {
    T1 first;
    T2 second;
    T3 third;

    Triple() = default;
    Triple(const T1& first, const T2& second, const T3& third)
        : first(first), second(second), third(third) {}

    Triple& operator=(const Triple& other) {
        if (this != &other) { // Check for self-assignment
            first = other.first;
            second = other.second;
            third = other.third;
        }
        return *this;
    }
};
class Preprocessor {
public:
    virtual ~Preprocessor() = default;

    // must have data structure
    std::array< MyTrip,NUM_OF_REAL_TRIPS> tripsData = {}; // key  = trip_id
    std::array< std::vector<TripStop>,NUM_OF_REAL_TRIPS> trips; // key  = trip_id holds the trip and the stop times he visit
    // the main data structure, route to stops to trips within day
    // need to change the trip stop to a RStop
    std::array<Triple<std::vector<ARouteStop>,std::vector<ARouteStop>,std::array<std::vector<ATrip>,NUM_OF_DAYS>>,NUM_OF_ALGO_ROUTES> Aroutes = {};// the final data structure for the algorithm
    std::array<AStop,NUM_OF_STOPS> Astops = {}; // the stops data strutcure i am gonna use for my algorithm, maps between stop to routes that serve it
    std::array<StopData,NUM_OF_STOPS> stopsData = {} ; // hold the data about a stop - name, lat/lon not used in the algorithm but for later purpuse


    std::unordered_map<std::string,std::vector<int>>geohashStops;
    virtual void process() = 0; // Pure virtual function

};
class Preprocess : public Preprocessor {
public:

    void process() override;
     ~Preprocess() override = default ;
private:
    std::unordered_map<int,std::string> gftsRouteIdToLineName = {};
    std::unordered_map<std::string,int> tripsIdsMap; // maps between the string id of the gtfs to my int id for efficent
    // all of the arrays are serve as a hasmap with direct acsses such that the key is simply the index
    std::unordered_map<std::vector<ARouteStop>,std::vector<int>,VectorRouteStopHash> algoRoutesMap;
    std::unordered_map<int,MyService> services; // not sequential -> therefore the hashmap
    // for testing only -  a map between a seqOfStops to it route id***************
    std::unordered_map<std::vector<ARouteStop>,int,VectorRouteStopHash>stopsSeqToRouteIdMap;

    void build_trip_stops();
    void lineNamesBuilder();
    void build_trip_data();
    void serviceBuilder();
    void stopsBuilder();
    void footpathBuilder();
    void algoRouteBuilder();
    void buildAStops(int routeId, const std::vector<ARouteStop>& routeStopsVector);
    void checker();
    void printStopFootpaths(int stopId);
    void printRoutesGivenStop(int stopId);
    void printTrip(const std::vector<ATrip>& tripsForHerOnDay);
    void printService(const MyService& service);
    int findTripWithStops();
    int getStopId(int gftsId);
    std::vector<ARouteStop> getRouteStopsFromTripStops(int tripId);
};
#endif //PREPROCESS_H

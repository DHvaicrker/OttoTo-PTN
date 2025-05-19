//
// Created by DVIR on 3/22/2025.
//
#include <unordered_set>
#ifndef ROUTINGALGORITHM_H
#include"preprocess.h"
#define ROUTINGALGORITHM_H
#define MIN_DISTANCE_FOR_PUBLIC_TRANSPORT 200 // 200 meters
#define MIN_TRANSFER_TIME 2 // 2 mintutes are the minum time that is allowed bweet switching trips
#define MAX_NUM_OF_TRANSFERS 7
#define DEST_STOP_ID 60000
#define START_STOP_ID 70000
#define FOOTPATH_TRIP_ID 1303449// the number of trips is 303449 so i chose an id that is much higher and out of the range of the real trips
#define BEST_ARRIVAL_TIME 1
#define SAFEST_JOURNEY 2
#define LEAST_WALKING 3
#define SAFE_LEVEL 0
struct StopLocation
{
     double lat;
    double lon;
};
struct Time {
    int curHourInSeconds;
    int dayInWeek;
    int date;
};
struct RAPTORStopState {
    // represent a connection between 2 stops: thier id and which trip connected them
    // at which time we aborded on it from the start stop
    // the arrival time to the end stop and the dep time in the first stop
    int depStopId; // the stop which the trip was aboraded on
    int arrStopId;
    int tripId;
    int aboardedTime;
    int arrTime;


    // Equality operator for RAPTORStopState
    bool operator==(const RAPTORStopState& other) const {
        return depStopId == other.depStopId &&
               arrStopId == other.arrStopId &&
               tripId == other.tripId &&
               aboardedTime == other.aboardedTime &&
               arrTime == other.arrTime;
    }
};
struct UserStopState {
    const std::string& depStopName;
    const std::string& arrStopName;
    const std::string& tripName;
    int aboardedTime;
    int arrTime;
    int walkingTime;
    // Equality operator for RAPTORStopState
    bool operator==(const UserStopState& other) const {
        return aboardedTime == other.aboardedTime &&
               arrTime == other.arrTime ;
    }
};

// mach between stop id and its best arrival time
typedef std::unordered_map<int, int> ArrivalTimeMap;
// the set of stops state by each round
typedef  std::array<std::unordered_map<int,RAPTORStopState>,MAX_NUM_OF_TRANSFERS+1> RoundBasedParetoSet;
typedef  std::array<std::vector<UserStopState>,MAX_NUM_OF_TRANSFERS+1> JourneysToDest;// the journys to dest by each round with each critirea optimization
class RoutingAlgorithm {
    public:
    std::array<std::vector<TripStop>, NUM_OF_REAL_TRIPS>& trips;
    std::array<Triple<std::vector<ARouteStop>, std::vector<ARouteStop>, std::array<std::vector<ATrip>, NUM_OF_DAYS>>, NUM_OF_ALGO_ROUTES>& Aroutes;
    std::array<AStop, NUM_OF_STOPS>& Astops;
    std::array<StopData, NUM_OF_STOPS>& stopsData;
    std::unordered_map<std::string, std::vector<int>>& geohashStops;
    std::array< MyTrip,NUM_OF_REAL_TRIPS>& tripsData;
    RoutingAlgorithm(
         std::array<std::vector<TripStop>, NUM_OF_REAL_TRIPS>& trips_,
         std::array<Triple<std::vector<ARouteStop>, std::vector<ARouteStop>, std::array<std::vector<ATrip>, NUM_OF_DAYS>>, NUM_OF_ALGO_ROUTES>& Aroutes_,
         std::array<AStop, NUM_OF_STOPS>& Astops_,
         std::array<StopData, NUM_OF_STOPS>& stopsData_,
         std::unordered_map<std::string, std::vector<int>>& geohashStops_,
         std::array< MyTrip,NUM_OF_REAL_TRIPS>& tripsData_
    ) : trips(trips_), Aroutes(Aroutes_), Astops(Astops_), stopsData(stopsData_), geohashStops(geohashStops_) ,tripsData(tripsData_) {}
    int arrTimeToStopViaTrip(int tripId,int stopSeqIndex);
    int earliestTrip(int routeId,int stopSeqIndex,int bestArrivalTimeToStopInPrevRound,const Time& curTime);

    virtual JourneysToDest run(StopLocation startStop,StopLocation endStop,Time curTime) = 0; // Pure virtual function
    int findStopSeq(int routeId,int stopId) ;
    virtual ~RoutingAlgorithm() = default; // Virtual destructor
};

class RAPTOR : public RoutingAlgorithm {
public:
    RAPTOR(
        std::array<std::vector<TripStop>, NUM_OF_REAL_TRIPS>& trips_,
        std::array<Triple<std::vector<ARouteStop>, std::vector<ARouteStop>, std::array<std::vector<ATrip>, NUM_OF_DAYS>>, NUM_OF_ALGO_ROUTES>& Aroutes_,
        std::array<AStop, NUM_OF_STOPS>& Astops_,
        std::array<StopData, NUM_OF_STOPS>& stopsData_,
        std::unordered_map<std::string, std::vector<int>>& geohashStops_,
        std::array< MyTrip,NUM_OF_REAL_TRIPS>& tripsData_
    ) : RoutingAlgorithm(trips_, Aroutes_, Astops_, stopsData_, geohashStops_,tripsData_) {}
    // check if current trip is better than other in the
    ~RAPTOR() override = default; // Virtual destructor
    JourneysToDest convert_to_journeys_output(RoundBasedParetoSet& round_pareto_set);
    int incrementDate(int date) ;
    UserStopState convert_algo_state_to_user_state(const RAPTORStopState &algo_state);

    std::vector<UserStopState> reconstructJourney(
    const RoundBasedParetoSet& round_pareto_set,
    int round_num);


    JourneysToDest run(StopLocation startStop, StopLocation endStop, Time curTime) override;
    bool updateStopWithPruning(
        ArrivalTimeMap& best_arr_time_map,
        RoundBasedParetoSet& round_pareto_set,
        std::unordered_set<int>& markedStopIds,
        int cur_stop_id,
        int dep_time,
        int arrTime,
        int cur_trip_id,
        int boarding_stop_seq_index,
        int cur_round);
    std::vector<Footpath> getFootpathsFromStop(StopLocation stop);
private:

    int findMinStopId(int routeId, int markedStopId1, int Q_stopId);
    void initFootpathStoTDirect(StopLocation s, StopLocation t, ArrivalTimeMap &best_pareto_set, RoundBasedParetoSet &round_pareto_set, Time curTime);

};
#endif //ROUTINGALGORITHM_H

//
// Created by DVIR on 3/22/2025.
//
#include <unordered_map>
#include "routingAlgorithm.h"
#include <limits>
#include <iostream>
#include <ostream>
#include <stack>

#include "geoUtil.h"
#include <unordered_set>


int RoutingAlgorithm::findStopSeq(int const routeId,int const stopId) {
    // return the stop seq index
    const std::vector<ARouteStop> & sortedByIds  =  Aroutes[routeId].first;
    // Use binary search with a custom comparator
    auto it = std::lower_bound(
        sortedByIds.begin(),
        sortedByIds.end(),
        stopId,
        [](const ARouteStop& a, int id) {
            return a.id < id; // Compare by id
        }
    );

    // Check if the element was found
    if (it != sortedByIds.end() && it->id == stopId) {
        return it->stopSeqIndex; // return the stop seq index
    }
    return -1; // incase it didnt found
}

// the arr(t,p) function from the algorithm pseoudo code
int RoutingAlgorithm::arrTimeToStopViaTrip(const int tripId,const int stopSeqIndex) {

    return trips[tripId][stopSeqIndex].arrTime;
}
// the et(r,p) function from the algorithm pseoudo code
int RoutingAlgorithm::earliestTrip(const int routeId,int stopSeqIndex, const int bestArrivalTimeToStopInPrevRound,const Time& curTime) {
    const std::vector<ATrip>& avaiableTrips = Aroutes[routeId].third[curTime.dayInWeek-1];
    // Using std::upper_bound with a lambda comparator - do a binray search
    auto it = std::upper_bound(avaiableTrips.begin(), avaiableTrips.end(), bestArrivalTimeToStopInPrevRound,
                               [stopSeqIndex, this](int bestArrTime, const ATrip& trip) {
                                   return bestArrTime < trips[trip.tripId][stopSeqIndex].depTime;
                               });

    if (it != avaiableTrips.end()) {
        int tripIndex = std::distance(avaiableTrips.begin(), it);
        int numOfTrips = avaiableTrips.size();
        // find a valid trip
        while (tripIndex<numOfTrips) {
            bool is_trip_activate = (avaiableTrips[tripIndex].endDate >= curTime.date &&curTime.date>=avaiableTrips[tripIndex].startDate);
            bool is_trip_safe = bestArrivalTimeToStopInPrevRound+MIN_TRANSFER_TIME*60 <= trips[avaiableTrips[tripIndex].tripId][stopSeqIndex].depTime;
            if (is_trip_activate&&is_trip_safe) {
                return avaiableTrips[tripIndex].tripId;
                break;
            }
            tripIndex++;
        }

        return -1;

    }

    return -1;


}
std::vector<Footpath> RAPTOR::getFootpathsFromStop(StopLocation stop) {
    std::vector<Footpath> footpaths;
    std::unordered_set<int> processedStops;  // Track which stops we've already added

    int i = 0;
    int maxIterations = 2;  // Limit iterations to prevent infinite loop

    while ((i == 0 || footpaths.empty()) && i < maxIterations) {
        std::vector<std::string> geohashBoxs = Geohash::getGeohashNeighbors(
            Geohash::encodeGeohash(stop.lat, stop.lon, GEO_HASH_PRESITION));

        for (const std::string& geohashBox : geohashBoxs) {
            for (const int& stopId : geohashStops[geohashBox]) {
                // Skip if we've already processed this stop
                if (processedStops.contains(stopId))
                    continue;

                processedStops.insert(stopId);

                double distance = haversineDistance(
                    stop.lat, stop.lon,
                    stopsData.at(stopId).lat, stopsData.at(stopId).lon);

                if (distance < MAX_WALK_DISTANCE * (i + 1)) {
                    int walkTime = calculateWalkTime(distance);
                    footpaths.push_back({stopId, walkTime});
                }
            }
        }
        i++;
    }

    return footpaths;
}
int RAPTOR::findMinStopId(int routeId, int markedStopId1, int Q_stopId) {
    int seq1 = findStopSeq(routeId, markedStopId1);
    int seq2 = findStopSeq(routeId, Q_stopId);

    // Return the stop ID associated with the smaller sequence value
    return (seq1 <= seq2) ? markedStopId1 : Q_stopId;

}

JourneysToDest RAPTOR::convert_to_journeys_output(RoundBasedParetoSet& round_pareto_set) {
    JourneysToDest journeys_to_dest  = {};
    for (int round = 0; round <= MAX_NUM_OF_TRANSFERS; ++round) {
        if (round_pareto_set[round].empty() || !round_pareto_set[round].contains(DEST_STOP_ID)) {
            continue;
        }
        journeys_to_dest[round] = reconstructJourney(round_pareto_set,round);
    }

    return journeys_to_dest;
}
UserStopState RAPTOR::convert_algo_state_to_user_state(const RAPTORStopState& algo_state) {
    // Static string caches to ensure strings persist
    static std::string start_stop_str = "start stop";
    static std::string dest_stop_str = "destination stop";
    static std::string by_foot_str = "by foot";

    const std::string& depStopName = (algo_state.depStopId == START_STOP_ID) ?
        start_stop_str : stopsData[algo_state.depStopId].name;

    const std::string& arrStopName = (algo_state.arrStopId == DEST_STOP_ID) ?
        dest_stop_str : stopsData[algo_state.arrStopId].name;

    const std::string& tripName = (algo_state.tripId == FOOTPATH_TRIP_ID) ?
        by_foot_str : tripsData[algo_state.tripId].lineName;

    return {depStopName, arrStopName, tripName,
            algo_state.aboardedTime, algo_state.arrTime};
}
std::vector<UserStopState> RAPTOR::reconstructJourney(
    const RoundBasedParetoSet& round_pareto_set,
    int round_num) {
    std::vector<UserStopState> journey;
    // Use stack to reverse the order
    std::stack<UserStopState> path;
    // Start with the destination stop.
     RAPTORStopState currentState = round_pareto_set[round_num].at(DEST_STOP_ID);
    // Start with current state (at destination)
    path.push(convert_algo_state_to_user_state(currentState));
    int depStopId = currentState.depStopId;
    // Backtrack through rounds to retrieve the full route.
    int cur_round = round_num;
    while (depStopId!=START_STOP_ID) {
        currentState = round_pareto_set[cur_round].at(depStopId);
        depStopId = currentState.depStopId;
        int tripId = currentState.tripId;
        UserStopState current_user_state = convert_algo_state_to_user_state(currentState);
        if (tripId==FOOTPATH_TRIP_ID) {
            // this is for combining footpaths: merges consecutive footpath segments
            if ( path.top().tripName == "by foot") {
                UserStopState last_state_footpath = path.top();
                path.pop();
                UserStopState new_footpath_state = {current_user_state.depStopName,last_state_footpath.arrStopName,last_state_footpath.tripName,current_user_state.aboardedTime,last_state_footpath.arrTime};
                path.push(new_footpath_state);

            }
            else {
                path.push(current_user_state);
            }
            if (!round_pareto_set[cur_round].contains(depStopId)) {
                cur_round--;
            }
        }
        else {
            // becasue if it is a trip we need to serach how we got to the dep stop where we aboarded the trip
            cur_round--;
            path.push(current_user_state);
        }

    }
    // Convert stack to vector (correct order)
    while (!path.empty()) {
        journey.push_back(path.top());
        path.pop();
    }
    return journey;

}
bool RAPTOR::updateStopWithPruning(
    ArrivalTimeMap& best_arr_time_map,
    RoundBasedParetoSet& round_pareto_set,
    std::unordered_set<int>& markedStopIds,
    int cur_stop_id,
    int dep_time,
    int arrTime,
    int boarding_stop_id,
    int cur_trip_id,
    int cur_round) {

    // If we've seen this stop before
    if (best_arr_time_map.contains(cur_stop_id)) {
        int bestArrTimeToCurStop = best_arr_time_map.at(cur_stop_id);
        // Local/target pruning: only if it's better than destination and better than best time to this stop
        if (arrTime < std::min(bestArrTimeToCurStop, best_arr_time_map[DEST_STOP_ID])) {
            best_arr_time_map[cur_stop_id] = arrTime;
            round_pareto_set[cur_round][cur_stop_id] = {
                boarding_stop_id,
                cur_stop_id,
                cur_trip_id,
                dep_time,
                arrTime
            };
            if (cur_stop_id!=DEST_STOP_ID) {markedStopIds.insert(cur_stop_id);}
            return true;
        }
    } else {
        // Target pruning only for newly discovered stops
        if (arrTime < best_arr_time_map[DEST_STOP_ID]) {
            best_arr_time_map[cur_stop_id] = arrTime;
            round_pareto_set[cur_round][cur_stop_id] = {
                boarding_stop_id,
                cur_stop_id,
                cur_trip_id,
                dep_time,
                arrTime
            };
            if (cur_stop_id!=DEST_STOP_ID) {markedStopIds.insert(cur_stop_id);}

            return true;
        }
    }
    return false;
}
// now left to deal with the edge case of close stops and recunstruct the solution for the user.!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
JourneysToDest RAPTOR::run(const StopLocation startStop, const StopLocation endStop, Time curTime) {
    if (haversineDistance(startStop.lat,startStop.lon,endStop.lat,endStop.lon)<MIN_DISTANCE_FOR_PUBLIC_TRANSPORT) {
        std::cout << "You can walk by foot to your dest" << std::endl;
        return {};
    }
    std::unordered_set<int> markedStopIds;
    std::vector<Footpath> footpathsFromStart = getFootpathsFromStop(startStop);
    std::vector<Footpath> footpathsFromDest  = getFootpathsFromStop(endStop);
    ArrivalTimeMap best_arr_time_map = {}; // array of the best arrival times to stops for each round - each num of tranfers
    RoundBasedParetoSet round_pareto_set = {}; // the result DS it is a result per round that each sop point to how we got to that stop: from what stop and with what trip and in what time we baorded on
    int NoTranfers = 0;
    // start footpath to mark start stops:
    // go over the footpath from the start stop and update the rest of the stops time - this isnt consider a trip becasue it is getting to s public transport stop by foot
    for (const Footpath& footpath : footpathsFromStart ) {
        int arrTime = curTime.curHourInSeconds +footpath.walkTime;

        RAPTORStopState start_state = {START_STOP_ID,footpath.otherStopId,FOOTPATH_TRIP_ID,curTime.curHourInSeconds,arrTime};
        best_arr_time_map[footpath.otherStopId]=arrTime ;
        round_pareto_set[NoTranfers][footpath.otherStopId]=start_state;
        markedStopIds.insert(footpath.otherStopId);
    }
    // init arr time to destenation
    best_arr_time_map[DEST_STOP_ID]=std::numeric_limits<int>::max(); ;

    for (int cur_round = 1; cur_round<=MAX_NUM_OF_TRANSFERS ; cur_round++) {
        std::unordered_map<int,int>Q; // route to the earliest marked stop_id in the route
        // find routes that serve marked stops

        for (auto it = markedStopIds.begin(); it != markedStopIds.end(); ) {
            int markedStopId = *it;

            for (int route_id : Astops[markedStopId].routes) { // routes that serve this stop
                if (Q.contains(route_id)) {
                    // Substitute (r; p0) by (r; p) in Q if p comes before p0 in r
                    Q[route_id] = findMinStopId(route_id, markedStopId, Q.at(route_id));
                } else {
                    Q[route_id] = markedStopId;
                }
            }
            // Erase the element and update the iterator
            it = markedStopIds.erase(it);  // erase returns the next iterator
        }

        for (const auto& [route_id, stop_id] : Q) {
            const auto& cur_route = Aroutes[route_id];
            const size_t num_of_stops = cur_route.first.size();
            int boarding_stop_seq_index = findStopSeq(route_id, stop_id)-1;
            // For each marked stop on this route
            int boarding_stop_id = cur_route.second[boarding_stop_seq_index].id;

            int cur_aborded_trip_id = -1;

            for (int cur_stop_seq_index = boarding_stop_seq_index; cur_stop_seq_index < num_of_stops; cur_stop_seq_index++) {

                int arrTime = std::numeric_limits<int>::max();
                int cur_stop_id = cur_route.second[cur_stop_seq_index].id;
                if (cur_aborded_trip_id!=-1) {

                    arrTime = arrTimeToStopViaTrip(cur_aborded_trip_id,cur_stop_seq_index);
                    // local and target purning:
                    // 1. target purning: if the arrival time is later than the dest it isn't relevant because we already reach the dest
                    // 2. update the arr time to that stop only if it is the best arrival time to that stop that has been found so far
                    updateStopWithPruning( best_arr_time_map, round_pareto_set,markedStopIds,cur_stop_id,trips[cur_aborded_trip_id][boarding_stop_seq_index].depTime,arrTime,boarding_stop_id,cur_aborded_trip_id,cur_round);
                }
                // if this trip doesnt improve the arrival time to a stop maybe there is an earlier trip that does
                // dont change the bestArrTimeByRounds to bestArrTime instead because here we are trying the aboard on a one extra trip only from a given stop
                // if we will do it with bestArrTime we aill try to aboard on couple of trips on the same round which can lead to use aboarding on 2 or more trips = 2 or more switches on the same iteration

                // we are doing it for every stop along the way becasue in the phase of Q if we have 2 stops under the same route we will only enter the first one
                // but the latter one maybe has better arrival times  so it could improve latter stops after it by taking a trip from it.
                if (round_pareto_set[cur_round-1].contains(cur_stop_id)&& round_pareto_set[cur_round-1][cur_stop_id].arrTime<arrTime) {
                    // if we havent reach this stop yet in prev round this isnt relevant. we only do this because thier might exsit a an earlier trip for that stop with in prev round we reach it eearlier with another route
                    int prevTripId=cur_aborded_trip_id;
                    cur_aborded_trip_id = earliestTrip(route_id,cur_stop_seq_index,round_pareto_set[cur_round-1][cur_stop_id].arrTime,curTime);
                    if (prevTripId!=cur_aborded_trip_id) {
                        boarding_stop_id = cur_stop_id;
                        boarding_stop_seq_index = cur_stop_seq_index;
                    }


                }


            }



        }

        for (const auto&[boarding_stop_id, walkTime] : footpathsFromDest ) {
            if (round_pareto_set[cur_round].contains(boarding_stop_id) && markedStopIds.contains(boarding_stop_id)) {
                const RAPTORStopState& state = round_pareto_set[cur_round][boarding_stop_id];
                int dep_time = state.arrTime;
                int arrTime = state.arrTime+walkTime;
                updateStopWithPruning( best_arr_time_map, round_pareto_set,markedStopIds,DEST_STOP_ID,dep_time,arrTime,boarding_stop_id,FOOTPATH_TRIP_ID,cur_round);


            }

        }

        std::unordered_set<int> markedStopIdsForFootpath ;
        // go over footpath in marked stop
        for (int boarding_stop_id: markedStopIds) {
            for ( const auto&[arr_stop_id, walkTime]: Astops[boarding_stop_id].footpaths) {
                const RAPTORStopState& state = round_pareto_set[cur_round][boarding_stop_id];
                int dep_time = state.arrTime;
                int arrTime = state.arrTime+walkTime;
                updateStopWithPruning( best_arr_time_map, round_pareto_set,markedStopIdsForFootpath,arr_stop_id, dep_time, arrTime,boarding_stop_id,FOOTPATH_TRIP_ID,cur_round);
            }

        }
        for (int stopId:  markedStopIdsForFootpath) {
            markedStopIds.insert(stopId);
        }
        if (markedStopIds.empty()) {
            // stoping critera becasue if now by taking an extra trip no stop has improve then also by nither 2 switches there fore we can exit the loop

            if (cur_round == 1) {
                // Prepare time for the next day (00:05:00)
                Time nextDayTime = {
                     5*3600 ,  // Set to 00:05:00
                    curTime.dayInWeek % 7 + 1,  // Increment day of week (wrap from 7 to 1)
                    incrementDate(curTime.date)  // Increment the date
                };

                std::cout << "No routes found today. Searching for routes starting at "
                          << "00:00:01 on the next day..." << std::endl;

                return run(startStop, endStop, nextDayTime);
            }
            else {
                std::cout<<"stoping the algorithm after: "<<cur_round<<std::endl;
                return convert_to_journeys_output(round_pareto_set);

            }

        }
    }
    return convert_to_journeys_output(round_pareto_set);
}
int RAPTOR::incrementDate(int date) {
    // Extract year, month, day
    int year = date / 10000;
    int month = (date / 100) % 100;
    int day = date % 100;

    // Calculate days in the current month
    int daysInMonth[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    // Check for leap year
    if (month == 2 && ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0))) {
        daysInMonth[2] = 29;
    }

    // Increment day
    day++;

    // Handle month rollover
    if (day > daysInMonth[month]) {
        day = 1;
        month++;

        // Handle year rollover
        if (month > 12) {
            month = 1;
            year++;
        }
    }

    return year * 10000 + month * 100 + day;
}

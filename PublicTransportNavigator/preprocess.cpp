#include "preprocess.h"
#include "timeUtil.h"


void Preprocess::process()  {
    auto start = std::chrono::high_resolution_clock::now();

    build_trip_stops(); // first load the trips that exsist with thier stops from stop_times
    lineNamesBuilder(); // save the line names - connect trip id to a line name
    build_trip_data(); // this include service id - which later be translated intp working days and the line name based on my id


    serviceBuilder();// connect between service id to its working days and start/end dates
    stopsBuilder(); // save information about the actual stops - names and location
    algoRouteBuilder(); // connect trips with the same stop sequence to be under the same route
    footpathBuilder(); // create footpath for each stop to other walkable stop
    // testing - see in the terminal
    checker();
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(end - start);

    std::cout << "Execution time: " << duration.count() << "seconds" << std::endl;
    std::cout << "finished Processing..." << std::endl;
}




//*********************** Testing function: *****************************************

void Preprocess::checker()
{
    // this checker function print all the trips on friday from herzelia train to jerusalm train and friday early morining
    // and the 29 line stop on thuersday and all the routes that go throw herzelia train station
    std::string herzelia29 = "17020800_280325";
    std::string herzeliaTrainJerusalmId  ="1_293699";
    int herTrainRouteId = stopsSeqToRouteIdMap[getRouteStopsFromTripStops(tripsIdsMap[herzeliaTrainJerusalmId])];
    std::vector<ATrip> tripsForHerOnDay = Aroutes[herTrainRouteId].third[5]; // on friday
    // for entire route between herzelia and jerusalm on  during the daylight
    printTrip(tripsForHerOnDay);
    std::cout << "  "<< std::endl;
    std::cout << "*****************************"<< std::endl;
    std::cout << "  "<< std::endl;
    // for late night route
    int lateHerRouteId = stopsSeqToRouteIdMap[getRouteStopsFromTripStops(findTripWithStops())]; // for night, a located the stops at night and found a trip
    tripsForHerOnDay = Aroutes[lateHerRouteId].third[5]; // on friday
    // for entire route between herzelia and jerusalm
    printTrip(tripsForHerOnDay);

    std::cout << "  "<< std::endl;
    std::cout << "*****************************"<< std::endl;
    std::cout << "  "<< std::endl;
    // print the bus 29 in herzelia:

    int herBusRouteId = stopsSeqToRouteIdMap[getRouteStopsFromTripStops(tripsIdsMap[herzelia29])];
    tripsForHerOnDay = Aroutes[herBusRouteId].third[4]; // on thuersday
    printTrip(tripsForHerOnDay);


    // now print all the routes id given a stop id of herzelia and print some trip name under this route:
    int herTrainStatiomId = 37361;
    printRoutesGivenStop(herTrainStatiomId);
    printStopFootpaths(herTrainStatiomId);


    for (ATrip trip :Aroutes[5675].third[0]) {
        if (trip.tripId>= NUM_OF_REAL_TRIPS) {
            int x = 0;
        }
    }

 }
void Preprocess::printStopFootpaths(int stopId) {
    std::cout << " ******************** foopaths *********************** "<< std::endl;
    std::cout << "Footpaths from Stop ID: " << stopId << " (" << stopsData[stopId].name << ")\n";
    for (const Footpath& footpath : Astops[stopId].footpaths) {
        std::cout << "  To Stop ID: " << footpath.otherStopId << " (" << stopsData[footpath.otherStopId].name << ") , lat lon: "<<stopsData[footpath.otherStopId].lat<<" " <<stopsData[footpath.otherStopId].lon<<" \n";
        std::cout << "    Walk Time: " << footpath.walkTime/60 << " minutes\n";
    }
}
void Preprocess::printRoutesGivenStop(int stopId) {
    std::cout << " ******************** routes that serve stops *********************** "<< std::endl;
    std::cout << "Stop Name: " << stopsData[stopId].name << " (ID: " << stopId << ")\n";
    std::cout << "Routes:\n";
    int SUNDAY = 0;
    for (const int& routeId : Astops[stopId].routes) {
        std::vector<ATrip> tripsOnDay = Aroutes[routeId].third[SUNDAY];
        if (tripsOnDay.size() > 0) {
            std::cout << "  Route ID: " << routeId << "\n";
            std::cout << "    Trip Name: " << tripsOnDay.at(0).lineName << "\n"; // printing on Sunday the first trip
        }

    }
}
std::vector<ARouteStop> Preprocess::getRouteStopsFromTripStops(int tripId) {
    // given a trip id get its corresponed route stops vector
    std::vector<ARouteStop>routeStopsVector;
    routeStopsVector.reserve(trips[tripId].size()); // Reserve space to avoid multiple allocations
    for (const auto& tripStop : trips[tripId]) {
        routeStopsVector.emplace_back(tripStop);
    }
    return routeStopsVector;
}
void Preprocess::printTrip(const std::vector<ATrip>& tripsForHerOnDay) {
    for (const ATrip& Atrip: tripsForHerOnDay) {
        std::cout<<"current trip id: "<<Atrip.tripId<< "and the name is: "<<Atrip.lineName<<std::endl;
        std::cout<<"---trip start date: "<<Atrip.startDate<<", end date: "<<Atrip.endDate<<std::endl;


        for (const TripStop& stop : trips.at(Atrip.tripId)) {
            std::cout <<"------------"<<stopsData[stop.id].name<< ", arrival time: "<<timeUtil::convertSecondsToTime(stop.arrTime)<< ", departure time: "<<timeUtil::convertSecondsToTime(stop.depTime)<<"stop id:"<<stop.id<<std::endl;

        }
        std::cout<<" "<<std::endl;
    }
}
int Preprocess::findTripWithStops() {
    std::vector<int> targetStops = {37361, 37357, 37305, 42285};

    for (int tripId = 0; tripId < trips.size(); ++tripId) {
        const std::vector<TripStop>& tripStops = trips[tripId];
        if (tripStops.size() < targetStops.size()) {
            continue; // Skip trips that are too short
        }

        bool match = true;
        for (size_t i = 0; i < targetStops.size(); ++i) {
            if (tripStops[i].id != targetStops[i]) {
                match = false;
                break;
            }
        }

        if (match) {
            std::cout << "Found trip with ID: " << tripId << std::endl;
             return tripId;
        }
    }

    std::cout << "No matching trip found." << std::endl;
    return -1;
}
int Preprocess::getStopId(int gftsId) {
    return gftsId-1;
}
void Preprocess::printService(const MyService& service) {
    std::cout << "Service Details:" << std::endl;
    std::cout << "Start Date: " << service.startDate << std::endl;
    std::cout << "End Date: " << service.endDate << std::endl;
    std::cout << "Weekdays: ";
    for (int i = 0; i < NUM_OF_DAYS; ++i) {
        std::cout << service.weekArr[i] << " ";
    }
    std::cout << std::endl;
}
//*********************** Testing End *****************************************

void Preprocess::footpathBuilder() {

    for (const StopData& stop : stopsData) {
        if (!stop.name.empty()) {
            // get all og the suspects in the cirece :  the 9 boxes
            // for each stop suspect calcuate the actual distance using the formula if it is under 500mm
            std::vector<std::string>geohashBoxs=Geohash::getGeohashNeighbors(stop.geohash);
            for (const std::string& geohashBox : geohashBoxs) { // 9 in total
                for (const int& stopId : geohashStops[geohashBox]) { // for each stop calculate its distance using haversineDistance

                    double distance = haversineDistance(stop.lat,stop.lon,stopsData.at(stopId).lat,stopsData.at(stopId).lon);
                    if (distance<MAX_WALK_DISTANCE) {
                        int walkTime = calculateWalkTime(distance);
                        Astops[stop.id].footpaths.push_back({stopId,walkTime});
                    }
                }

            }

        }
    }
}


void Preprocess::algoRouteBuilder() {
    // pack together trips with the same sequnce of stops to be under the same route
    // using the stop sequnce as a key for a hashmap with my own hash function
    for (int tripId = 0; tripId < trips.size(); tripId++) {

            std::vector<ARouteStop>routeStopsVector;
            routeStopsVector.reserve(trips[tripId].size()); // Reserve space to avoid multiple allocations
            for (const auto& tripStop : trips[tripId]) {
                routeStopsVector.emplace_back(tripStop);
            }
            algoRoutesMap[routeStopsVector].push_back(tripId);


    }
    // the main function that creates the main data structre - Aroute and Astop
    int routeId = 0;
    std::cout<< algoRoutesMap.size()<<std::endl;
    for ( auto& [routeStopsVector,tripIdsVector] : algoRoutesMap) {
        std::vector<ARouteStop> routeStopsVectorSortedBySeq = routeStopsVector; // already sorted by seq
        std::vector<ARouteStop> routeStopsVectorSortedById = routeStopsVector;

        std::sort(routeStopsVectorSortedById.begin(), routeStopsVectorSortedById.end(),
         [this](const ARouteStop& stop1, const ARouteStop& stop2) {
             return stop1.id < stop2.id;
         });


        std::array<std::vector<ATrip>,NUM_OF_DAYS> tripDays = {};
        // create the tripsDay array that for each day it will hold all the active trips on that day(under a route)
        for(const int& tripId : tripIdsVector) {
             MyService tripService = services[ tripsData[tripId].serviceId];
            const std::string lineName = tripsData[tripId].lineName;
           for(int day = 0;day<NUM_OF_DAYS;day++){
                if(tripService.weekArr[day]){
                    tripDays[day].push_back({tripId,tripService.startDate, tripService.endDate,lineName});
                }

            }
        }
        // for each day sort its trip by their dep time
        // sort based on the depTime at the first stop because thats enought
        for(int day = 0;day<NUM_OF_DAYS;day++){
                std::sort(tripDays[day].begin(), tripDays[day].end(),
             [this](const ATrip& trip1, const ATrip&  trip2) {
                 return trips[trip1.tripId][0].depTime < trips[trip2.tripId][0].depTime;
             });
        }
        // after adding all the trips add sorting to each day
        Aroutes [routeId] = {routeStopsVectorSortedById,routeStopsVectorSortedBySeq,tripDays};
        buildAStops(routeId,routeStopsVectorSortedById);
        stopsSeqToRouteIdMap[routeStopsVectorSortedBySeq] = routeId;
        routeId++;
    }
    std::cout<<"num of real routes: "<<algoRoutesMap.size()<<std::endl;
}
void Preprocess::buildAStops(int routeId,const std::vector<ARouteStop>& routeStopsVector) {
    // a function that builds that Astops data structe - connect stop_id to routes_ids and footpath
    for (const ARouteStop& routeStop : routeStopsVector) {
        if (Astops[routeStop.id].routes.empty()) {
            Astops[routeStop.id].routes = std::vector<int>(); // Initialize the routes vector if it's empty
        }
        Astops[routeStop.id].routes.push_back(routeId);
        // add also footpath logic here
    }
}
void Preprocess::serviceBuilder() {
    std::string filename = "data/calendar.txt";
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Could not open file: " << filename << std::endl;
        return;
    }
    std::string line;
    MyService service{};
    std::string serviceIdStr, startDateStr, endDateStr;
    std::getline(file, line); // Skip header
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        std::istringstream iss(line);

        std::getline(iss, serviceIdStr, ',');
        for (int i = 0; i < 7; ++i) {
            std::string day;
            std::getline(iss, day, ',');
            std::from_chars(day.data(), day.data() + day.size(), service.weekArr[i]);
        }
        std::getline(iss, startDateStr, ',');
        std::getline(iss, endDateStr, ',');
        std::from_chars(startDateStr.data(), startDateStr.data() + startDateStr.size(), service.startDate);
        std::from_chars(endDateStr.data(), endDateStr.data() + endDateStr.size(), service.endDate);
        int serviceId;
        std::from_chars(serviceIdStr.data(), serviceIdStr.data() + serviceIdStr.size(), serviceId);
        services[serviceId] = service;
    }
}
void Preprocess::stopsBuilder() {
    std::string filename = "data/stops.txt";
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Could not open file: " << filename << std::endl;
        return;
    }

    //  header: stop_id,stop_code,stop_name,stop_desc,stop_lat,stop_lon
    std::string line;
    std::string stopIdStr, stopCode, stopName, stopDesc; // stopDesc is unused, so we can skip it
    std::string stopLatStr, stopLonStr;
    int stopId;
    double lat, lon;

    std::getline(file, line); // Skip the header line
    while (std::getline(file, line)) {
        if (line.empty()) {
            continue; // Skip empty lines
        }
        std::istringstream iss(line);
        // Read fields in order:
        std::getline(iss, stopIdStr, ',');  // stop_id
        std::getline(iss, stopCode, ',');     // stop_code (skip)
        std::getline(iss, stopName, ',');     // stop_name
        std::getline(iss, stopDesc, ',');     // stop_desc (skip)
        std::getline(iss, stopLatStr, ',');   // stop_lat
        std::getline(iss, stopLonStr, ',');   // stop_lon

        // Convert strings to numeric values:
        std::from_chars(stopIdStr.data(), stopIdStr.data() + stopIdStr.size(), stopId);
        std::from_chars(stopLatStr.data(), stopLatStr.data() + stopLatStr.size(), lat);
        std::from_chars(stopLonStr.data(), stopLonStr.data() + stopLonStr.size(), lon);

        std::string geohash = Geohash::encodeGeohash(lat, lon, GEO_HASH_PRESITION);
        int myStopId = getStopId(stopId);
        stopsData[myStopId] = {myStopId,stopName, lat, lon,geohash};
        geohashStops[geohash] .push_back(myStopId) ; // grouping together stops under that same geohash box
    }
}
void Preprocess::build_trip_data() {
    std::string filename = "data/trips.txt";
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Could not open file: " << filename << std::endl;
        return;
    }
    int routeId, serviceId,tripIntId;
    std::string line,tripId,serviceIdStr,routeIdStr;
    std::getline(file, line); // Skip header
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        std::istringstream iss(line);
        std::getline(iss, routeIdStr, ',');
        std::getline(iss, serviceIdStr, ',');
        std::getline(iss, tripId, ',');


        std::from_chars(routeIdStr.data(), routeIdStr.data() + routeIdStr.size(), routeId);
        std::from_chars(serviceIdStr.data(), serviceIdStr.data() + serviceIdStr.size(), serviceId);
        if (tripId.empty()) {
            continue;
        }
        if (tripsIdsMap.contains(tripId)) {
            std::string lineName = gftsRouteIdToLineName.at(routeId);
            tripIntId=tripsIdsMap[tripId]; // that way i am taking the trip information only with trips that i sure that exsist with stops in it
            tripsData [tripIntId ] = { tripIntId,serviceId,lineName}; // add the line name here***************************************************************************************************************
        }



    }

}
void Preprocess::build_trip_stops()  {
    std::string filename = "data/stop_times.txt";
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Could not open file: " << filename << std::endl;
        return;
    }

    std::string line,tripId, arrivalTime,departureTime, stopIdStr,stopSeqIndexStr;
    int id, stopSeqIndex;
    int tripIntId = -1; // in the first time it is gonna be 0
    std::getline(file, line) ;// Skip the header line
    while (std::getline(file, line)) {
        if (line.empty()) {
            continue; // Skip empty lines
        }
        std::istringstream iss(line);
        std::getline(iss, tripId, ',');
        std::getline(iss,arrivalTime , ',');
        std::getline(iss,departureTime , ',');
        std::getline(iss, stopIdStr, ',');
        std::getline(iss, stopSeqIndexStr, ',');

        std::from_chars(stopIdStr.data(), stopIdStr.data() + stopIdStr.size(), id);
        id = getStopId(id);// doing -1 for my convetion
        std::from_chars(stopSeqIndexStr.data(), stopSeqIndexStr.data() + stopSeqIndexStr.size(),  stopSeqIndex);
        // only if i am encoutring a trip then incerment the trip counter becasue in route.txt file there are trips that doesnt exsist = not stops
        if (!tripsIdsMap.contains(tripId)) {
            tripIntId++;
            tripsIdsMap[tripId]=tripIntId;
            trips[tripIntId].push_back({id, stopSeqIndex,timeUtil::calcTimeInSeconds(departureTime),timeUtil::calcTimeInSeconds(arrivalTime)});

        }
        else {
            trips[tripIntId].push_back({id, stopSeqIndex,timeUtil::calcTimeInSeconds(departureTime),timeUtil::calcTimeInSeconds(arrivalTime)});
        }


    }
    std::cout << tripsIdsMap.size() << " " <<tripIntId<<std::endl;
    for (auto& trip : trips) {
        // sort the stops inside every trip by thier seq_index
        std::sort(trip.begin(), trip.end(), StopsComparator());
    }

}
void Preprocess::lineNamesBuilder() {
    // here i am working with the gfts route id and not mine, this function is for linking a trip with its public name
    std::string filename = "data/routes.txt";
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Could not open file: " << filename << std::endl;
        return;
    }

    int routeId;
    std::string line,routeIdStr,agencyIdStr,routeShortName,routeLongName;
    std::getline(file, line); // Skip header
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        std::istringstream iss(line);
        std::getline(iss, routeIdStr, ',');
        std::getline(iss, agencyIdStr, ',');
        std::getline(iss, routeShortName, ',');
        std::getline(iss, routeLongName, ',');

        std::from_chars(routeIdStr.data(), routeIdStr.data() + routeIdStr.size(), routeId);
        // if (routeId == 30276) {// herzelia-jeruslam
        //     int x = 0;
        // }
        if (routeShortName.empty()) { // a train station
            gftsRouteIdToLineName[routeId] = routeLongName;
        }
        else {
            gftsRouteIdToLineName[routeId] = routeShortName;
        }
    }
}



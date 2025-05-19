#include <iostream>
#include <fstream>
#include "preprocess.h"
#include "routingAlgorithm.h"
#include "timeUtil.h"
#include <stack>



void print_journey(std::vector<UserStopState>&journey) {

    std::cout<<"-- printing the journey instruction..."<<std::endl;
    // Print the route in order from source to destination.
    for (const UserStopState& stopInfo : journey) {
        std::cout << "---- taking a trip from stop "<<stopInfo.depStopName<< " at time "<<timeUtil::convertSecondsToTime(stopInfo.aboardedTime)
        <<" to stop "<<stopInfo.arrStopName
        <<" at time "
          << timeUtil::convertSecondsToTime(stopInfo.arrTime)
          << " with trip: "
          << stopInfo.tripName<< std::endl;

    }
}
void print_algo_results(RAPTOR& raptor,JourneysToDest& journeys_to_dest) {
    // Iterate over each transfer round (including zero transfers).
    for (int numTransfers = 0; numTransfers <= MAX_NUM_OF_TRANSFERS; ++numTransfers) {
        // Only process if there is a recorded time for the destination stop.
        if (!journeys_to_dest[numTransfers].empty()) {
            if (!journeys_to_dest[numTransfers].empty()) {
                std::cout<<"****  found a journey with  "<< numTransfers<< " transfer" <<std::endl;
                print_journey(journeys_to_dest[numTransfers] );
                std::cout<<" "<< numTransfers<<std::endl;
            }


        }
    }


}
void score_algorithm_results(){}
int main() {

    std::unique_ptr<Preprocessor> preprocessorPtr = std::make_unique<Preprocess>();

    preprocessorPtr->process();
    RAPTOR raptor(preprocessorPtr->trips,preprocessorPtr->Aroutes,preprocessorPtr->Astops,preprocessorPtr->stopsData,preprocessorPtr->geohashStops,preprocessorPtr->tripsData);

    StopLocation startStop = {32.168997, 34.844180}; // h
    StopLocation endStop ={32.072571, 34.789531}; //Eilat 32.169319, 34.844108
    Time startTime = {timeUtil::calcTimeInSeconds("08:00:00"),4,20250507};

    std::cout<<"starting running algorithm..."<<std::endl;
    auto start = std::chrono::high_resolution_clock::now();
    // Run the Raptor algorithm and capture both the time table and trip data for each round.
    JourneysToDest journeys_to_dest = raptor.run(startStop, endStop, startTime);

    // Measure and display the execution time.
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - start);
    std::cout << "Execution time of the algorithm: " << duration.count() / 1000 << " seconds::"
              << duration.count() % 1000 << " milliseconds" << std::endl;
    std::cout << "Start printing results..." << std::endl;
    print_algo_results(raptor,journeys_to_dest);

    return 0;
}

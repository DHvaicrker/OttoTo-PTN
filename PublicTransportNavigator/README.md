# OttoTo_PTN: Public Transport Navigator

OttoTo_PTN is a high-performance offline public transportation routing system designed to compute optimal journeys in public transit networks. 
It is based on the **RAPTOR (Round-based Public Transit Routing)** algorithm, which is known for its speed and efficiency. 
The project processes **GTFS (General Transit Feed Specification)** for transit data.


## Comparison to Google Maps

The images below compares OttoTo_PTN's routing results with Google Maps. OttoTo_PTN achieves comparable results in terms of accuracy and efficiency while being tailored for specific use cases, such as custom transit networks or offline routing.

### Mount Hermon to Eilat
The image shows a sample journey from the northernmost point in Israel to the southernmost, where OttoTo_PTN provides a detailed breakdown of the route, including transfers and walking segments.
![Comparison to Google Maps - Mount Hermon to Eilat ](images/OTTO_vs_Maps_mtHermon-Eilat.png)

### Ben Gurion Airport to Tel Aviv
This image shows a sample journey from an airport to Tel Aviv, where OttoTo_PTN provides a detailed breakdown of the route, including transfers and walking segments.
![Comparison to Google Maps - Ben Gurion Airport to Tel Aviv](images/OTTO_vs_Maps_Airport_to_Tel-Aviv.png)

*Figure: OttoTo_PTN vs. Google Maps for a sample journey.*


## Key Features

- **RAPTOR Algorithm**: Implements the RAPTOR algorithm to compute the fastest routes with minimal transfers.
- **GTFS Data Integration**: Processes GTFS data to extract stops, routes, trips, and schedules.
- **Preprocessing Pipeline**: Efficiently preprocesses transit data to optimize runtime performance.
- **Geohashing for Spatial Indexing**: Uses geohashing to group stops spatially for quick lookups.
- **Dynamic Footpath Handling**: Calculates walking paths between stops for seamless integration with public transit.
- **Edge Case Handling**: Handles scenarios like walking-only journeys for short distances or trips spanning midnight or very long walks to reach a stop.


## How It Works

### 1. **RAPTOR Algorithm**
The RAPTOR algorithm is a round-based public transit routing algorithm that iteratively explores routes and stops to compute optimal journeys. It is designed to:
- Minimize travel time.
- Minimize the number of transfers.
- Efficiently prune suboptimal paths to improve performance.

### 2. **GTFS Data**
GTFS (General Transit Feed Specification) is a standard format for public transportation schedules and associated geographic information. OttoTo_PTN uses GTFS files to extract:
- **Stops**: Locations where passengers can board or alight.
- **Routes**: Paths taken by transit vehicles.
- **Trips**: Specific instances of routes with schedules.
- **Stop Times**: Arrival and departure times for each stop.
- **Calendar**: Service availability based on days and dates.

### 3. **Preprocessing**
To optimize runtime performance, OttoTo_PTN preprocesses GTFS data into efficient data structures. The preprocessing steps include:
- **Building Trip Stops**: Extracting stops for each trip from `stop_times.txt`.
- **Service Mapping**: Mapping service IDs to their active days and date ranges using `calendar.txt`.
- **Stop Data Construction**: Parsing stop names, locations, and geohashes from `stops.txt`.
- **Route Aggregation**: Grouping trips with identical stop sequences into routes.
- **Footpath Generation**: Calculating walking paths between nearby stops using geohashing and haversine distance.

The preprocessing pipeline ensures that the routing engine can quickly access and process the required data during runtime.

---

## Example Usage

### Input
- **Start Location**: Latitude and longitude of the starting point.
- **End Location**: Latitude and longitude of the destination.
- **Departure Time**: Desired departure time.

### Output
- **Optimal Journey**: Includes stops, trips, walking segments, and transfer details.(As shown in the images above)


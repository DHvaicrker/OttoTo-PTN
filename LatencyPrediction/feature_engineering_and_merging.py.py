import warnings
from sklearn.pipeline import Pipeline
import numpy as np
import pandas as pd
from meteostat import Point, Hourly
from sklearn.compose import ColumnTransformer
from sklearn.preprocessing import OneHotEncoder
import os

# Define column names based on the data structure
NUM_OF_WEATHER_LINES = 98123
TEL_AVIV_LAT ,TEL_AVIV_LON = 32.0853, 34.7818
# Combining history and GTFS data:
def filter_by_cluster(df,cluster_id = 11,cluster_name = "תל אביב"):
    """
    Filters the dataset by the specified cluster ID and name.

    Args:
        cluster_id (int): The ID of the cluster to filter by.
        cluster_name (str): The name of the cluster to filter by.

    Returns:
        pd.DataFrame: Filtered DataFrame containing only the specified cluster.
    """
    # Placeholder for actual data filtering logic
    if 'ClusterId' not in df.columns:
        raise ValueError("The input DataFrame must contain a 'cluster_id' column.")

        # Filter the DataFrame by the specified cluster_id
    filtered_df = df[df['ClusterId'] == cluster_id]
    # remove rows with na values at every column
    filtered_df = filtered_df.dropna(subset=['OfficeLineId'])
    return filtered_df

def merge_routes_with_history(route_df, history_df):
    """
    Merges route_df with history_df based on line_id and direction.
    The route_df is expected to have a 'route_desc' column formatted as 'line_id-direction-alt'.
    The history_df is expected to have 'OfficeLineId', 'Direction', and 'LineAlternative' columns.
    The function extracts line_id, direction, and alt from the route_desc column in route_df,
    and matches them with the corresponding columns in history_df.
    The function returns a DataFrame with the merged results.

    :param route_df: DataFrame containing route information with a 'route_desc' column.
    :param history_df: DataFrame containing historical trip information with 'OfficeLineId', 'Direction', and 'LineAlternative'
    :return: DataFrame with merged results, including matched rows from history_df and corresponding route information.
    """

    # --- Step 1: Prepare route_df ---
    route_parts = route_df['route_desc'].astype(str).str.strip().str.extract(
        r'(?P<line_id>\d{5})-(?P<direction>\d)-(?P<alt>.+)')
    route_df['line_id'] = route_parts['line_id']
    route_df['direction'] = route_parts['direction']
    route_df['alt'] = route_parts['alt']

    # Index route_df by (line_id, direction)
    from collections import defaultdict

    route_lookup = defaultdict(list)
    for _, row in route_df.iterrows():
        key = (row['line_id'], row['direction'])
        route_lookup[key].append(row)

    # --- Step 2: Prepare history_df ---
    # Check for NaN or non-numeric values in OfficeLineId
    if history_df['OfficeLineId'].isna().any():
        print("Warning: NaN values found in 'OfficeLineId'. Dropping these rows.")
        history_df = history_df.dropna(subset=['OfficeLineId'])

    # Ensure OfficeLineId is numeric
    history_df['OfficeLineId'] = pd.to_numeric(history_df['OfficeLineId'], errors='coerce')
    if history_df['OfficeLineId'].isna().any():
        print("Warning: Non-numeric values found in 'OfficeLineId'. Dropping these rows.")
        history_df = history_df.dropna(subset=['OfficeLineId'])

    # Convert OfficeLineId to string with zero-padding
    history_df['OfficeLineId'] = history_df['OfficeLineId'].astype(int).astype(str).str.zfill(5)
    history_df['Direction'] = history_df['Direction'].astype(int).astype(str)
    history_df['LineAlternative'] = history_df['LineAlternative'].astype(int).astype(str)

    # --- Step 3: Match using lookup ---
    matched_rows = []

    for idx, trip in history_df.iterrows():
        key = (trip['OfficeLineId'], trip['Direction'])
        if key in route_lookup:
            for route in route_lookup[key]:
                # Match on exact alternative or wildcard '#'
                if route['alt'] == '#' or route['alt'] == trip['LineAlternative']:
                    merged = trip.to_dict()
                    for col in route.index:
                        merged[f'route_{col}'] = route[col]
                    matched_rows.append(merged)

    # Convert result to DataFrame
    matched_df = pd.DataFrame(matched_rows)
    return matched_df

def add_scheduled_times(df, gtfs_stop_times_path):
    """
    Given a DataFrame with 'gtfs_trip_id', append scheduled start and end times from GTFS stop_times.txt.

    Parameters:
    - df: DataFrame with 'gtfs_trip_id' column
    - gtfs_stop_times_path: path to GTFS stop_times.txt file

    Returns:
    - df with two new columns: 'scheduled_start_time', 'scheduled_end_time'
    """

    # Load GTFS stop_times.txt
    stop_times = pd.read_csv(gtfs_stop_times_path)

    # Ensure time columns are parsed correctly (HH:MM:SS even >24h)
    stop_times['arrival_time'] = pd.to_timedelta(stop_times['arrival_time'])
    stop_times['departure_time'] = pd.to_timedelta(stop_times['departure_time'])

    # Get the first and last stop per trip
    agg = stop_times.groupby('trip_id').agg(
        scheduled_start_time=('departure_time', 'first'),
        scheduled_end_time=('arrival_time', 'last')
    ).reset_index()

    # Merge into your DataFrame
    df = df.merge(agg, left_on='gtfs_trip_id', right_on='trip_id', how='left')

    # Drop duplicate 'trip_id' from GTFS (optional)
    df.drop(columns=['trip_id'], inplace=True)

    return df

def add_delay_columns(df):
    """
    Adds start, end, and total delay columns (in minutes) to the DataFrame.

    Assumes the DataFrame contains:
        - 'actual_start_time' (datetime)
        - 'actual_end_time' (datetime)
        - 'scheduled_start_time' (datetime or timedelta)
        - 'scheduled_end_time' (datetime or timedelta)
    """
    import numpy as np
    from pandas import to_datetime

    # Convert timedelta to datetime if needed, assuming all on same date for scheduled
    if pd.api.types.is_timedelta64_dtype(df['scheduled_start_time']):
        df['scheduled_start_time'] = df['actual_start_time'].dt.normalize() + df['scheduled_start_time']

    if pd.api.types.is_timedelta64_dtype(df['scheduled_end_time']):
        df['scheduled_end_time'] = df['actual_start_time'].dt.normalize() + df['scheduled_end_time']

    # Calculate delays
    df['start_delay_min'] = (df['actual_start_time'] - df['scheduled_start_time']).dt.total_seconds() / 60.0
    df['end_delay_min'] = (df['actual_end_time'] - df['scheduled_end_time']).dt.total_seconds() / 60.0

    # Total delay is the difference in duration
    actual_duration = (df['actual_end_time'] - df['actual_start_time']).dt.total_seconds() / 60.0
    scheduled_duration = (df['scheduled_end_time'] - df['scheduled_start_time']).dt.total_seconds() / 60.0
    df['total_delay_min'] = actual_duration - scheduled_duration

    # Optional: round to 2 decimal places
    df[['start_delay_min', 'end_delay_min', 'total_delay_min']] = df[[
        'start_delay_min', 'end_delay_min', 'total_delay_min']].round(2)

    return df

def add_trip_info(history_routes_df, trips_df, stop_times_df, shapes_df):
    """
    Enriches the `history_routes_df` DataFrame with additional trip-related information
    by merging data from `trips_df`, `stop_times_df`, and `shapes_df`.

    This function performs the following steps:
    1. Parses and calculates the scheduled start times for trips from `stop_times_df`.
    2. Merges the scheduled start times into `trips_df`.
    3. Matches trips in `history_routes_df` with the closest scheduled trips in `trips_df` based on start times.
    4. Maps `shape_id` from `trips_df` to `history_routes_df`.
    5. Computes the total number of planned stops for each trip using `stop_times_df`.
    6. Calculates the total trip distance for each shape using `shapes_df`.
    7. Adds scheduled start and end times to `history_routes_df`.
    8. Computes delay-related columns (start delay, end delay, and total delay).

    Args:
        history_routes_df (pd.DataFrame): DataFrame containing historical trip data to be enriched including route information.
        trips_df (pd.DataFrame): DataFrame containing trip information (e.g., `trip_id`, `route_id`, `shape_id`).
        stop_times_df (pd.DataFrame): DataFrame containing stop times for trips (e.g., `trip_id`, `departure_time`).
        shapes_df (pd.DataFrame): DataFrame containing shape information (e.g., `shape_id`, `shape_pt_lat`, `shape_pt_lon`).

    Returns:
        pd.DataFrame: Enriched DataFrame with additional trip-related information, including:
            - `gtfs_trip_id`: Closest matching trip ID.
            - `shape_id`: Shape ID for the trip.
            - `num_planned_stops`: Total number of planned stops for the trip.
            - `total_trip_distance_km`: Total distance of the trip in kilometers.
            - `scheduled_start_time`: Scheduled start time of the trip.
            - `scheduled_end_time`: Scheduled end time of the trip.
            - `start_delay_min`: Delay at the start of the trip (in minutes).
            - `end_delay_min`: Delay at the end of the trip (in minutes).
            - `total_delay_min`: Total delay of the trip (in minutes).
    """
    # 1. Parse scheduled start times
    stop_times_df['departure_time'] = pd.to_timedelta(stop_times_df['departure_time'])
    scheduled_start_times = stop_times_df.groupby('trip_id', sort=False)['departure_time'].min().reset_index()
    scheduled_start_times.rename(columns={'departure_time': 'scheduled_start_time'}, inplace=True)

    # 2. Merge scheduled start times with trips_df
    trips_with_start_times = pd.merge(trips_df, scheduled_start_times, on='trip_id', how='left')

    # 3. Prepare for closest trip matching
    history_routes_df['actual_start_time'] = pd.to_datetime(history_routes_df['bitzua_history_start_dt'], errors='coerce')
    history_routes_df['actual_end_time'] = pd.to_datetime(history_routes_df['bitzua_history_end_dt'], errors='coerce')

    # Group trips by route_id for faster lookup
    trips_grouped = trips_with_start_times.groupby('route_id')
    print("before the hard part")
    def find_closest_trip_cached(row):
        route_id = row['route_route_id']
        actual_start = row['actual_start_time']

        if pd.isna(actual_start) or route_id not in trips_grouped.groups:
            return None

        route_trips = trips_grouped.get_group(route_id).copy()
        route_trips['aligned_scheduled_time'] = actual_start.normalize() + route_trips['scheduled_start_time']
        time_diff = (route_trips['aligned_scheduled_time'] - actual_start).abs()
        return route_trips.loc[time_diff.idxmin(), 'trip_id']

    history_routes_df['gtfs_trip_id'] = history_routes_df.apply(find_closest_trip_cached, axis=1)
    print("after the hard part")
    # 4. Map shape_id from trip_id
    history_routes_df['shape_id'] = history_routes_df['gtfs_trip_id'].map(dict(zip(trips_df['trip_id'], trips_df['shape_id'])))

    # 5. Compute stop counts
    stop_counts = stop_times_df['trip_id'].value_counts().rename('num_planned_stops').reset_index()
    stop_counts.rename(columns={'index': 'trip_id'}, inplace=True)

    # 6. Compute distances per shape
    def compute_all_distances(shapes_df):
        distances = {}
        grouped = shapes_df.sort_values('shape_pt_sequence').groupby('shape_id')
        for shape_id, group in grouped:
            lats = group['shape_pt_lat'].values
            lons = group['shape_pt_lon'].values
            if len(lats) < 2:
                distances[shape_id] = 0
                continue
            lat_rad = np.radians(lats)
            lon_rad = np.radians(lons)
            dlat = lat_rad[1:] - lat_rad[:-1]
            dlon = lon_rad[1:] - lon_rad[:-1]
            a = np.sin(dlat / 2) ** 2 + np.cos(lat_rad[:-1]) * np.cos(lat_rad[1:]) * np.sin(dlon / 2) ** 2
            c = 2 * np.arctan2(np.sqrt(a), np.sqrt(1 - a))
            distances[shape_id] = 6371.0 * np.sum(c)
        return distances

    shape_distances_dict = compute_all_distances(shapes_df)
    history_routes_df['total_trip_distance_km'] = history_routes_df['shape_id'].map(shape_distances_dict)

    # 7. Merge stop counts
    history_routes_df = history_routes_df.merge(stop_counts, left_on='gtfs_trip_id', right_on='trip_id', how='left')
    history_routes_df.drop(columns=['trip_id'], inplace=True)

    # 8. Add scheduled and delay columns
    history_routes_df = add_scheduled_times(history_routes_df, r"data\stop_times.txt")
    history_routes_df.to_csv("temp.csv", index=False, encoding='utf-8-sig')
    history_routes_df = add_delay_columns(history_routes_df)

    return history_routes_df

def combine_history_with_GTFS():
    """
       Combines historical trip data with GTFS (General Transit Feed Specification) data
       to create a comprehensive dataset for analysis.

       This function performs the following steps:
       1. Filters historical trip data for the Tel Aviv cluster.
       2. Merges the filtered historical data with route information from GTFS.
       3. Enriches the merged data with trip-related details such as trip ID, number of stops,
          and total trip distance using GTFS files (`trips.txt`, `stop_times.txt`, `shapes.txt`).
       4. Adds scheduled start and end times to the dataset.
       5. Computes delay-related columns (start delay, end delay, and total delay).
       6. Saves the final enriched dataset to a CSV file.

       Args:
           None

       Returns:
           None: The function saves the enriched dataset to a file at `data/Tel-Aviv_latency_GFTS.csv`.

       Notes:
           - Input files required:
               - `data/trip_latency_israel.csv`: Historical trip data.
               - `data/routes.txt`: GTFS route information.
               - `data/trips.txt`: GTFS trip information.
               - `data/stop_times.txt`: GTFS stop times.
               - `data/shapes.txt`: GTFS shape information.
           - Output file:
               - `data/Tel-Aviv_latency_GFTS.csv`: Enriched dataset with historical and GTFS data.
       """
    # get Tel Aviv history data:
    history_file_path = r'data\trip_latency_israel.csv'
    history_df = pd.read_csv(history_file_path, delimiter='|' ,skiprows=1)
    filtered_tel_aviv_df = filter_by_cluster(history_df)
    print("Filtered Tel Aviv DataFrame shape:", filtered_tel_aviv_df.shape)
    #mergde routes with history
    route_df = pd.read_csv(r'data\routes.txt')
    print("Route DataFrame shape:", route_df.shape)
    print("start combining routees with filtered history")
    merged_route_history_df = merge_routes_with_history(route_df, filtered_tel_aviv_df)
    print("Merged route and history DataFrame shape:", merged_route_history_df.shape)
    #merge with GTFS stop_times
    merged_route_history_df.to_csv("routes_history_merged.csv", index=False, encoding='utf-8-sig')
    merged_route_history_df = pd.read_csv(r'routes_history_merged.csv')
    print("start combining trip_id , num stops and distance with history")
    # Load GTFS files
    trips_df = pd.read_csv(r"data\trips.txt")  # contains route_id, trip_id, shape_id
    stop_times_df = pd.read_csv(r"data\stop_times.txt")  # contains trip_id, stop_id
    shapes_df = pd.read_csv(r"data\shapes.txt")  # contains shape_id, shape_pt_lat, shape_pt_lon, shape_pt_sequence
    gtfs_history_merged=add_trip_info(merged_route_history_df, trips_df, stop_times_df, shapes_df)
    print("Merged GTFS data with history DataFrame shape:", gtfs_history_merged.shape)
    # Save the merged DataFrame to a CSV file
    output_file_path = r'data\Tel-Aviv_latency_GFTS.csv'
    gtfs_history_merged.to_csv(output_file_path, index=False, encoding='utf-8-sig')

# adding weather features to the data(history and GTFS):

def get_weather_features(lat, lon, start_dt, end_dt):
    """
       Fetches weather features (temperature, precipitation, and wind speed) for a given location and time range.

       This function uses the Meteostat library to retrieve hourly weather data for the specified latitude, longitude,
       and time range. It calculates the average temperature, total precipitation, and average wind speed over the
       specified period.

       Args:
           lat (float): Latitude of the location.
           lon (float): Longitude of the location.
           start_dt (str or pd.Timestamp): Start of the time range (in a format parsable by pandas).
           end_dt (str or pd.Timestamp): End of the time range (in a format parsable by pandas).

       Returns:
           pd.Series: A pandas Series containing the following weather features:
               - `temp_C` (float): Average temperature in degrees Celsius.
               - `precip_mm` (float): Total precipitation in millimeters.
               - `wind_kph` (float): Average wind speed in kilometers per hour.

       Notes:
           - If the time range is invalid (e.g., end time is before start time), or if no data is available for the
             specified location and time range, the function returns a Series with `None` values.
           - The function rounds the start and end times to the nearest hour for consistency with Meteostat's hourly data.
       """
    # 1) parse to Timestamp, coerce invalid → NaT
    start_dt = pd.to_datetime(start_dt, errors="coerce")
    end_dt   = pd.to_datetime(end_dt,   errors="coerce")

    # 2) return None if either is NaT
    if pd.isna(start_dt) or pd.isna(end_dt):
        return pd.Series({'temp_C': None, 'precip_mm': None, 'wind_kph': None})

    # 3) round to nearest hour — lowercase 'h' only
    start_dt = start_dt.round("h")
    end_dt   = end_dt.round("h")

    if end_dt < start_dt:
        print(f"Buffered end before start: {start_dt} → {end_dt}")
        return pd.Series({'temp_C': None, 'precip_mm': None, 'wind_kph': None})

    try:
        loc = Point(lat, lon)
        with warnings.catch_warnings():
            warnings.filterwarnings(
                "ignore",
                message=".*'H' is deprecated.*",
                category=FutureWarning
            )
            df = Hourly(loc, start_dt, end_dt).fetch()

        if df.empty:
            print(f"No data for {lat},{lon} from {start_dt} to {end_dt}")
            return pd.Series({'temp_C': None, 'precip_mm': None, 'wind_kph': None})

        return pd.Series({
            'temp_C'   : round(df['temp'].mean(), 2)   if pd.notna(df['temp'].mean())   else None,
            'precip_mm': round(df['prcp'].sum(), 2)    if pd.notna(df['prcp'].sum())    else None,
            'wind_kph' : round(df['wspd'].mean(), 2)   if pd.notna(df['wspd'].mean())   else None
        })

    except Exception as e:
        print(f"Error fetching weather for {lat},{lon} at {start_dt}: {e}")
        return pd.Series({'temp_C': None, 'precip_mm': None, 'wind_kph': None})

def add_weather_features(df, max_rows=200000):
    """
    Enrich up to max_rows of `df` with weather features in one go,
    then drop any rows where weather info is missing, and save once.
    """
    OUTPUT_FILENAME = r"data\Tel-Aviv_latency_GFTS_weather_combine.csv"
    os.makedirs(os.path.dirname(OUTPUT_FILENAME), exist_ok=True)

    # 1) Randomly sample up to max_rows rows
    # sub = df.sample(n=min(len(df), max_rows), random_state=42).copy()
    sub = df[:max_rows].copy()
    print(f"Processing {len(sub)} rows in one go…")

    # 2) Build (start, end) pairs rounded to 'h'
    pairs = sub[["bitzua_history_start_dt", "bitzua_history_end_dt"]].apply(
        lambda r: (
            pd.to_datetime(r.iloc[0], errors="coerce").round("h"),
            pd.to_datetime(r.iloc[1], errors="coerce").round("h")
        ),
        axis=1
    )

    # 3) Fetch each unique window once
    lookup = {
        p: get_weather_features(TEL_AVIV_LAT, TEL_AVIV_LON, *p)
        for p in pairs.unique()
    }

    # 4) Map back, turn into DataFrame of weather cols
    weather_df = pairs.map(lookup).apply(pd.Series)

    # 5) Concatenate, drop any rows with missing weather values
    enriched = pd.concat([sub.reset_index(drop=True), weather_df], axis=1)
    enriched = enriched.dropna(subset=["temp_C", "precip_mm", "wind_kph"])
    print(f"Dropped {len(sub) - len(enriched)} rows with missing weather info.")

    # 6) Save once
    enriched.to_csv(OUTPUT_FILENAME, index=False, encoding="utf-8-sig")
    print(f"Saved enriched data ({len(enriched)} rows) to {OUTPUT_FILENAME}")

    return enriched


# add avg_speed, avg_delay, is_rush_hour, is_school_day features:
def add_historical_binary_features(df):
    """
       Adds historical binary and aggregated features to the input DataFrame.

       This function performs the following steps:
       1. Calculates the average delay (`avg_delay`) for each route based on the `route_route_short_name`.
       2. Computes the duration of each trip in hours and the individual trip speed.
       3. Calculates the average speed (`avg_speed`) for each route based on the `route_route_short_name`.
       4. Adds binary features:
           - `is_rush_hour`: Indicates whether the trip occurred during rush hours (7-9 AM, 4-7 PM).
           - `is_school_day`: Indicates whether the trip occurred on a school day (Monday to Saturday).
       5. Drops intermediate columns used for calculations to reduce redundancy.

       Args:
           df (pd.DataFrame): The input DataFrame containing trip data.
               Expected columns:
               - `route_route_short_name`
               - `total_delay_min`
               - `actual_start_time`
               - `actual_end_time`
               - `trip_day_in_week`
               - `scheduled_start_time`

       Returns:
           pd.DataFrame: The input DataFrame enriched with the following new features:
               - `avg_delay`: Average delay for each route.
               - `avg_speed`: Average speed for each route.
               - `is_rush_hour`: Binary indicator for rush hours.
               - `is_school_day`: Binary indicator for school days.

       Notes:
           - The function assumes that `actual_start_time` and `actual_end_time` are in a datetime-compatible format.
           - Rush hours are defined as 7-9 AM and 4-7 PM.
           - School days are defined as Monday (1) to Saturday (6).
       """
    df['avg_delay'] = df.groupby('route_route_short_name')['total_delay_min'].transform('mean')
    time_difference = pd.to_datetime(df['actual_end_time'], errors='coerce') - pd.to_datetime(df['actual_start_time'],
                                                                                              errors='coerce')
    df['trip_duration_hours'] = time_difference.dt.total_seconds() / 3600.0
    df['individual_trip_speed'] = df['total_trip_distance_km'] / df['trip_duration_hours']
    df['avg_speed'] = df.groupby('route_route_short_name')['individual_trip_speed'].transform('mean')
    df = df.drop(columns=['trip_duration_hours', 'individual_trip_speed'])


    df['scheduled_start_hour'] = pd.to_datetime(df['scheduled_start_time'], errors='coerce').dt.hour
    df['is_rush_hour'] = df['scheduled_start_hour'].apply(lambda x: 1 if x in [7, 8, 9, 16, 17, 18, 19] else 0)
    df = df.drop(columns=['scheduled_start_hour'])
    df['is_school_day'] = df['trip_day_in_week'].apply(lambda x: 1 if x in [1, 2, 3, 4, 5, 6] else 0)
    return df


def preprocess_features(df):
    """
    Preprocesses the input DataFrame by performing feature engineering, removing unnecessary columns,
    and creating new features for machine learning models.

    This function performs the following steps:
    1. Removes irrelevant or redundant columns from the DataFrame.
    2. Converts datetime columns to appropriate formats and extracts useful components (e.g., hour, minute).
    3. Creates cyclical features (sine and cosine transformations) for time-based features such as hours, minutes, and months.
    4. Adds interaction features between categorical and numerical columns (e.g., rush hour and hour cosine).
    5. Encodes categorical features using one-hot encoding.
    6. Returns the processed DataFrame and a list of feature names.

    Args:
        df (pd.DataFrame): The input DataFrame containing raw features.

    Returns:
        tuple:
            - pd.DataFrame: The processed DataFrame ready for machine learning models.
            - list: A list of feature names in the processed DataFrame.

    Notes:
        - The function assumes specific column names in the input DataFrame, such as `scheduled_start_time`,
          `scheduled_end_time`, `trip_month`, and `trip_day_in_week`.
        - Cyclical transformations are applied to preserve the periodic nature of time-based features.
        - Interaction features are created to capture relationships between categorical and numerical features.
        - Columns not relevant to the model are dropped to reduce noise and improve performance.
    """
    df_processed = df.copy()
    # add avg_speed, avg_delay, is_rush_hour, is_school_day features:
    df_processed = add_historical_binary_features(df_processed)
    features_to_remove = [
        'cluster_nm','OperatorId','operator_nm','OperatorLineId','OfficeLineId',
        'TripId','actual_start_time','actual_end_time','end_delay_min',
        'total_delay_min','gtfs_trip_id','shape_id','route_route_id',
        'route_agency_id','route_line_id','route_route_long_name',
        'route_route_desc','route_route_color','route_alt','erua_hachraga_ind',
        'bitzua_history_end_dt','bitzua_history_start_dt','Viechle_num',
        'trip_time','LineAlternative','start_delay_min'
    ]
    df_processed.drop(columns=[c for c in features_to_remove if c in df_processed],
                      inplace=True, errors='ignore')

    df_processed['scheduled_start_time'] = pd.to_datetime(
        df_processed['scheduled_start_time'], errors='coerce')
    df_processed['scheduled_end_time'] = pd.to_datetime(
        df_processed['scheduled_end_time'], errors='coerce')

    df_processed['scheduled_start_month_sin'] = np.sin(2 * np.pi * df_processed['trip_month'] / 12)
    df_processed['scheduled_start_month_cos'] = np.cos(2 * np.pi * df_processed['trip_month'] / 12)

    if 'scheduled_start_time' in df_processed.columns:
        df_processed['scheduled_start_hour'] = df_processed['scheduled_start_time'].dt.hour
        df_processed['scheduled_start_minute'] = df_processed['scheduled_start_time'].dt.minute

        df_processed['scheduled_start_hour_sin'] = np.sin(2 * np.pi * df_processed['scheduled_start_hour'] / 24)
        df_processed['scheduled_start_hour_cos'] = np.cos(2 * np.pi * df_processed['scheduled_start_hour'] / 24)

        df_processed['scheduled_start_minute_sin'] = np.sin(2 * np.pi * df_processed['scheduled_start_minute'] / 60)
        df_processed['scheduled_start_minute_cos'] = np.cos(2 * np.pi * df_processed['scheduled_start_minute'] / 60)

        df_processed['time_since_midnight_minutes'] = df_processed['scheduled_start_hour'] * 60 + df_processed['scheduled_start_minute']


    if 'scheduled_end_time' in df_processed.columns:
        df_processed['scheduled_end_hour'] = df_processed['scheduled_end_time'].dt.hour
        df_processed['scheduled_end_minute'] = df_processed['scheduled_end_time'].dt.minute
        df_processed['scheduled_end_hour_sin'] = np.sin(2 * np.pi * df_processed['scheduled_end_hour'] / 24)
        df_processed['scheduled_end_hour_cos'] = np.cos(2 * np.pi * df_processed['scheduled_end_hour'] / 24)

        df_processed['scheduled_end_minute_sin'] = np.sin(2 * np.pi * df_processed['scheduled_end_minute'] / 60)
        df_processed['scheduled_end_minute_cos'] = np.cos(2 * np.pi * df_processed['scheduled_end_minute'] / 60)

        if 'scheduled_start_time' in df_processed.columns:
             df_processed['scheduled_duration_minutes'] = (df_processed['scheduled_end_time'] - df_processed['scheduled_start_time']).dt.total_seconds() / 60.0


    if 'scheduled_start_time' in df_processed.columns:
         df_processed = df_processed.drop(columns=['scheduled_start_time'], errors='ignore')
    if 'scheduled_end_time' in df_processed.columns:
         df_processed = df_processed.drop(columns=['scheduled_end_time'], errors='ignore')


    if 'trip_dt' in df_processed:
        df_processed.drop(columns='trip_dt', inplace=True)

    if 'trip_day_in_week' in df_processed:
        df_processed['trip_day_in_week_sin'] = np.sin(2*np.pi*df_processed['trip_day_in_week']/7)
        df_processed['trip_day_in_week_cos'] = np.cos(2*np.pi*df_processed['trip_day_in_week']/7)

    if 'is_rush_hour' in df_processed.columns and 'scheduled_start_hour_cos' in df_processed.columns:
        df_processed['is_rush_hour_x_hour_cos'] = df_processed['is_rush_hour'] * df_processed['scheduled_start_hour_cos']
    if 'is_rush_hour' in df_processed.columns and 'scheduled_start_hour_sin' in df_processed.columns:
        df_processed['is_rush_hour_x_hour_sin'] = df_processed['is_rush_hour'] * df_processed['scheduled_start_hour_sin']
    if 'is_school_day' in df_processed.columns and 'scheduled_start_hour_cos' in df_processed.columns:
        df_processed['is_school_day_x_hour_cos'] = df_processed['is_school_day'] * df_processed['scheduled_start_hour_cos']
    if 'is_school_day' in df_processed.columns and 'scheduled_start_hour_sin' in df_processed.columns:
        df_processed['is_school_day_x_hour_sin'] = df_processed['is_school_day'] * df_processed['scheduled_start_hour_sin']


    categorical_features = [
        'ClusterId','Direction','LineAlternative','route_route_short_name',
        'route_route_type','route_direction','is_rush_hour',
        'is_school_day'
    ]
    categorical_features = [c for c in categorical_features if c in df_processed.columns]

    preprocessor = ColumnTransformer(
        transformers=[
            ('cat', OneHotEncoder(handle_unknown='ignore'),
             categorical_features)
        ],
        remainder='passthrough'
    )

    pipe = Pipeline([('prep', preprocessor)])
    X_trans = pipe.fit_transform(df_processed)
    feature_names = []
    if categorical_features:
        ohe_names = pipe.named_steps['prep'].named_transformers_['cat']\
                          .get_feature_names_out(categorical_features)
        feature_names.extend(ohe_names)
    passthrough_cols = [c for c in df_processed.columns
                        if c not in categorical_features]
    feature_names.extend(passthrough_cols)
    if hasattr(X_trans, "toarray"):
        X_trans = X_trans.toarray()
    X_df = pd.DataFrame(X_trans, columns=feature_names, index=df_processed.index)
    if 'total_delay_min' in df.columns:
        X_df['total_delay_min'] = df['total_delay_min']

    return X_df, list(X_df.columns)


if __name__ == "__main__":
    df = pd.read_csv(r"data\Tel-Aviv_latency_GFTS_weather_combine.csv", encoding="utf-8-sig")
    data, list_of_columns = preprocess_features(df)
    print(f" The shape of the final samples-'Tel-Aviv_model_samples' {data.shape}")
    data.to_csv(r"data\Tel-Aviv_model_samples.csv", index=False, encoding="utf-8-sig")




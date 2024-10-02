import boto3
import math
import json
from boto3.dynamodb.conditions import Key
from botocore.exceptions import ClientError

BUS_IDS = ['24T', '34']

# Initialize DynamoDB resource
dynamodb = boto3.resource('dynamodb', region_name='eu-west-1')
dynamodb_timings = boto3.client('dynamodb')

# Define the tables
weather_table = dynamodb.Table('weather_table')
stop_requests_table = dynamodb.Table('stop_requests_table')
location_occupancy_table = dynamodb.Table('location_occupancy_table')

def get_latest_item(table):
    try:
        # Scan the table and sort items by timestamp to get the latest one
        response = table.scan()
        if 'Items' in response and response['Items']:
            items = sorted(response['Items'], key=lambda x: x['ts'], reverse=True)
            latest_item = items[0]
            return latest_item
        else:
            return None
    except Exception as e:
        print(f"Error retrieving data from {table.name}: {e}")
        return None



def compute_eta(latitude, longitude):
    target_angle = 0.0  # Target angle in radians for point (0, 1)
    radius = 1.0  # km
    velocity_kmh = 50.0  # km/h
    omega_rad_sec = velocity_kmh / radius / 3600.0  # Angular velocity in rad/s

    # Calculate the current angle based on latitude and longitude
    current_angle = math.atan2(latitude, longitude)
    
    # Adjust angles to ensure they are within [0, 2*pi]
    current_angle = current_angle % (2 * math.pi)
    target_angle = target_angle % (2 * math.pi)

    # Calculate the angular distance to the target
    if current_angle <= target_angle:
        angular_distance = target_angle - current_angle
    else:
        angular_distance = (2 * math.pi - current_angle) + target_angle

    # Convert angular distance to time
    eta_seconds = angular_distance / omega_rad_sec
    eta_minutes = int(eta_seconds / 60.0)
    
    return eta_minutes

def get_latest_position():
    try:
        # Fetch data from DynamoDB
        response = dynamodb_timings.scan(TableName='location_occupancy_table')
        items = response['Items']
        
        # Filter and sort the items for the desired bus IDs
        latest_data = {}
        for item in items:
            bus_id = item.get('bus_id', {}).get('S')
            ts = item.get('ts', {}).get('S')
            latitude = item.get('latitude', {}).get('S')
            longitude = item.get('longitude', {}).get('S')
            occupancy = item.get('occupancy', {}).get('S')
            
            eta = compute_eta(float(latitude), float(longitude))
            
            if bus_id in BUS_IDS and ts:
                if bus_id not in latest_data or ts > latest_data[bus_id]['ts']:
                    latest_data[bus_id] = {
                        'bus_id': bus_id,
                        'ts': ts,
                        'latitude': latitude,
                        'longitude': longitude,
                        'occupancy': occupancy,
                        'eta': eta
                    }
        
        #print(latest_data)
        #latest_data[bus_id]['longitude']
        
        return latest_data
    
    except Exception as e:
        print(f"Error retrieving data from location_occupancy_table: {e}")
        return None



def main():
    # Fetch the latest weather data
    latest_weather = get_latest_item(weather_table)
    if latest_weather:
        print("Latest Weather Data:")
        print(f"Temperature: {latest_weather.get('Temperature', 'N/A')}°C")
        print(f"Humidity: {latest_weather.get('Humidity', 'N/A')}%")
        print(f"Pollution: {latest_weather.get('Pollution', 'N/A')}")
        print(f"Light: {latest_weather.get('Light', 'N/A')}")
    else:
        print("No weather data available.")

    # Fetch the latest stop request data
    latest_stop_request = get_latest_item(stop_requests_table)
    if latest_stop_request:
        print("Latest stop requests data:")
        print(f"stop 24T: {latest_stop_request.get('bus_24T', 'N/A')}")
        print(f"stop 34: {latest_stop_request.get('bus_34', 'N/A')}")

    # Fetch the latest bus location data
    latest_location_data = get_latest_position()
    if latest_location_data:
        print("Latest stop requests data:")
        print(f"bus 24T:\n -Location: ({latest_location_data['24T']['latitude']},{latest_location_data['24T']['longitude']})\n -Passengers: {latest_location_data['24T']['occupancy']}\n -ETA to stop 1: {latest_location_data['24T']['eta']}")
        
        print(f"bus 34:\n -Location: ({latest_location_data['34']['latitude']},{latest_location_data['34']['longitude']})\n -Passengers: {latest_location_data['34']['occupancy']}\n -ETA to stop 1: {latest_location_data['34']['eta']}")
        

if __name__ == '__main__':
    main()

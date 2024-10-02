import boto3
import math
import json
import threading
from flask import Flask, jsonify, render_template
from boto3.dynamodb.conditions import Key
from botocore.exceptions import ClientError
from datetime import datetime

app = Flask(__name__)

BUS_IDS = ['24T', '34']

# Initialize DynamoDB resource
dynamodb = boto3.resource('dynamodb', region_name='eu-west-1')
dynamodb_timings = boto3.client('dynamodb')

# Define the tables
weather_table = dynamodb.Table('weather_table')
stop_requests_table = dynamodb.Table('stop_requests_table')
location_occupancy_table = dynamodb.Table('location_occupancy_table')

latest_data = {
    "weather": None,
    "stop_requests": None,
    "location": None
}

def get_latest_item(table):
    try:
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
    target_angle = 0.0
    radius = 1.0
    velocity_kmh = 50.0
    omega_rad_sec = velocity_kmh / radius / 3600.0
    current_angle = math.atan2(latitude, longitude)
    current_angle = current_angle % (2 * math.pi)
    target_angle = target_angle % (2 * math.pi)
    if current_angle <= target_angle:
        angular_distance = target_angle - current_angle
    else:
        angular_distance = (2 * math.pi - current_angle) + target_angle
    eta_seconds = angular_distance / omega_rad_sec
    eta_minutes = int(eta_seconds / 60.0)
    return eta_minutes

def get_latest_position():
    try:
        response = dynamodb_timings.scan(TableName='location_occupancy_table')
        items = response['Items']
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
        return latest_data
    except Exception as e:
        print(f"Error retrieving data from location_occupancy_table: {e}")
        return None

def update_data():
    global latest_data
    threading.Timer(5.0, update_data).start()
    latest_data['weather'] = get_latest_item(weather_table)
    latest_data['stop_requests'] = get_latest_item(stop_requests_table)
    latest_data['location'] = get_latest_position()

@app.route('/data')
def get_data():
    return jsonify(latest_data)

@app.route('/')
def index():
    return render_template('index.html')

if __name__ == '__main__':
    update_data()
    app.run(debug=True)

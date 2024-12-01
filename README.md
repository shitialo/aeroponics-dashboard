# Aeroponics Dashboard

A web interface for monitoring and controlling an aeroponic system. This dashboard communicates with an ESP32 microcontroller to display real-time sensor data and control system parameters.

## Features

- Real-time monitoring of:
  - Temperature
  - Humidity
  - VPD (Vapor Pressure Deficit)
  - pH
  - Water Level
  - Reservoir Volume
  - Light Intensity
- Control interface for:
  - Light threshold adjustment
  - pH target adjustment

## Setup

1. Clone this repository
2. Update the `API_BASE_URL` in script.js to point to your ESP32's IP address
3. Deploy to a web server or open index.html locally

## Note

Make sure your ESP32 is properly configured and accessible on your network before using this dashboard. 
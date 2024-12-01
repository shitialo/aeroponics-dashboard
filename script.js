// Configuration
const API_BASE_URL = 'http://your-esp32-ip-address'; // Change this to your ESP32's IP address
const UPDATE_INTERVAL = 2000; // Update interval in milliseconds

let updateInProgress = false;

// Utility function for throttling
const throttle = (func, limit) => {
    let inThrottle;
    return function(...args) {
        if (!inThrottle) {
            func.apply(this, args);
            inThrottle = true;
            setTimeout(() => inThrottle = false, limit);
        }
    }
}

// Fetch sensor data from the ESP32
async function updateSensorData() {
    if (updateInProgress) return;
    updateInProgress = true;
    
    try {
        const response = await fetch(`${API_BASE_URL}/data`);
        const data = await response.json();
        
        // Update DOM elements with sensor data
        document.getElementById('temp').textContent = data.Temperature;
        document.getElementById('hum').textContent = data.Humidity;
        document.getElementById('vpd').textContent = data.VPD;
        document.getElementById('ph').textContent = data.pH;
        document.getElementById('wl').textContent = data.WaterLevel;
        document.getElementById('rv').textContent = data.ReservoirVolume;
        document.getElementById('li').textContent = data.LightIntensity;
    } catch (error) {
        console.error('Error fetching data:', error);
    } finally {
        updateInProgress = false;
    }
}

// Send control updates to the ESP32
async function updateControl(control, value) {
    try {
        const response = await fetch(`${API_BASE_URL}/control`, {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({ [control]: value })
        });
        
        if (!response.ok) {
            throw new Error(`HTTP error! status: ${response.status}`);
        }
    } catch (error) {
        console.error('Error updating control:', error);
    }
}

// Event Listeners
document.getElementById('lightThreshold').addEventListener('input', 
    throttle(function() {
        const value = this.value;
        document.getElementById('lightThresholdValue').textContent = value;
        updateControl('lightThreshold', value);
    }, 250)
);

document.getElementById('pHTarget').addEventListener('input',
    throttle(function() {
        const value = this.value;
        document.getElementById('pHTargetValue').textContent = value;
        updateControl('pHTarget', value);
    }, 250)
);

// Initialize updates
setInterval(updateSensorData, UPDATE_INTERVAL);
updateSensorData(); 
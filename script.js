// Configuration
const API_BASE_URL = window.config?.API_BASE_URL || 'http://192.168.1.XXX'; // Replace with your ESP32's IP
const UPDATE_INTERVAL = 2000;
const MAX_RETRY_ATTEMPTS = 3;
const RETRY_DELAY = 1000;

let updateInProgress = false;
let connectionStatus = false;
let retryCount = 0;

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

// Check connection status
async function checkConnection() {
    try {
        const response = await fetch(`${API_BASE_URL}/status`);
        if (response.ok) {
            const data = await response.json();
            connectionStatus = true;
            retryCount = 0;
            updateConnectionStatus(true);
            console.log('Connected to ESP32:', data);
            return true;
        }
    } catch (error) {
        console.error('Connection check failed:', error);
        connectionStatus = false;
        updateConnectionStatus(false);
        return false;
    }
    return false;
}

// Update UI to show connection status
function updateConnectionStatus(isConnected) {
    const statusElement = document.getElementById('connection-status');
    if (statusElement) {
        statusElement.textContent = isConnected ? 'Connected' : 'Disconnected';
        statusElement.className = isConnected ? 'status-connected' : 'status-disconnected';
    }
}

// Enhanced fetch with retry logic
async function fetchWithRetry(url, options = {}, attempts = MAX_RETRY_ATTEMPTS) {
    try {
        const response = await fetch(url, options);
        if (!response.ok) {
            throw new Error(`HTTP error! status: ${response.status}`);
        }
        return response;
    } catch (error) {
        if (attempts === 1) throw error;
        await new Promise(resolve => setTimeout(resolve, RETRY_DELAY));
        return fetchWithRetry(url, options, attempts - 1);
    }
}

// Updated sensor data fetch
async function updateSensorData() {
    if (updateInProgress) return;
    updateInProgress = true;
    
    try {
        if (!connectionStatus && !(await checkConnection())) {
            throw new Error('Device not connected');
        }

        const response = await fetchWithRetry(`${API_BASE_URL}/data`);
        const data = await response.json();
        
        Object.entries(data).forEach(([key, value]) => {
            const element = document.getElementById(key.toLowerCase());
            if (element) {
                element.textContent = value;
            }
        });

        retryCount = 0;
    } catch (error) {
        console.error('Error fetching data:', error);
        retryCount++;
        
        if (retryCount >= MAX_RETRY_ATTEMPTS) {
            connectionStatus = false;
            updateConnectionStatus(false);
        }
    } finally {
        updateInProgress = false;
    }
}

// Updated control function
async function updateControl(control, value) {
    try {
        if (!connectionStatus && !(await checkConnection())) {
            throw new Error('Device not connected');
        }

        const response = await fetchWithRetry(`${API_BASE_URL}/control`, {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({ [control]: value })
        });
        
        showFeedback(`${control} updated successfully`, 'success');
    } catch (error) {
        console.error('Error updating control:', error);
        showFeedback(`Failed to update ${control}`, 'error');
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

// Add feedback UI
function showFeedback(message, type) {
    const feedback = document.getElementById('feedback') || createFeedbackElement();
    feedback.textContent = message;
    feedback.className = `feedback ${type}`;
    feedback.style.opacity = '1';
    
    setTimeout(() => {
        feedback.style.opacity = '0';
    }, 3000);
}

function createFeedbackElement() {
    const feedback = document.createElement('div');
    feedback.id = 'feedback';
    document.body.appendChild(feedback);
    return feedback;
}

// Initialize updates
checkConnection().then(() => {
    setInterval(updateSensorData, UPDATE_INTERVAL);
    updateSensorData();
}); 
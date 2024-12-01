const fs = require('fs');

const config = {
    API_BASE_URL: process.env.API_BASE_URL || 'http://your-esp32-ip-address'
};

const configFile = `window.config = ${JSON.stringify(config, null, 2)};`;
fs.writeFileSync('config.js', configFile); 
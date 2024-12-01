const fs = require('fs');

const config = {
    API_BASE_URL: process.env.API_BASE_URL || 'http://192.168.0.100'
};

const configFile = `window.config = ${JSON.stringify(config, null, 2)};`;
fs.writeFileSync('config.js', configFile); 
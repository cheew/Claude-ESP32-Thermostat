// Setup Wizard JavaScript
let currentStep = 1;
const totalSteps = 7;
let profiles = [];
let selectedProfile = null;
let selectedClimate = 'auto';
let selectedSeason = 'standard';
let profileData = null;
let isAPMode = false;

// Initialize wizard
document.addEventListener('DOMContentLoaded', function() {
    loadProfiles();
    loadSensors();
    loadDeviceInfo();
    loadWifiStatus();
    document.getElementById('mqtt-enable').addEventListener('change', function() {
        document.getElementById('mqtt-settings').style.display = this.checked ? 'block' : 'none';
    });
    updateProgress();
});

async function loadProfiles() {
    try {
        const resp = await fetch('/api/v1/profiles');
        const data = await resp.json();
        profiles = data.profiles || [];
        renderAnimalGrid();
    } catch (e) {
        console.error('Failed to load profiles:', e);
    }
}

async function loadSensors() {
    try {
        const resp = await fetch('/api/sensors');
        const data = await resp.json();
        const sensors = data.sensors || [];
        const el = document.getElementById('sensors-found');
        if (sensors.length === 0) {
            el.innerHTML = '<em>No sensors found. Check wiring.</em>';
        } else {
            let html = '';
            sensors.forEach(function(s, i) {
                html += '<div style="padding:5px 0">' + (s.name || 'Sensor ' + (i+1)) +
                        ': ' + (s.temp !== undefined ? s.temp.toFixed(1) + ' C' : 'N/A') + '</div>';
            });
            el.innerHTML = html;
        }
    } catch (e) {
        document.getElementById('sensors-found').innerHTML = '<em>Could not scan sensors</em>';
    }
}

async function loadDeviceInfo() {
    try {
        const resp = await fetch('/api/info');
        const data = await resp.json();
        if (data.name) {
            document.getElementById('device-name').value = data.name;
        }
    } catch (e) {}
}

async function loadWifiStatus() {
    try {
        const resp = await fetch('/api/info');
        const data = await resp.json();
        var statusEl = document.getElementById('wifi-status');
        var descEl = document.getElementById('wifi-description');
        var configEl = document.getElementById('wifi-config');

        if (data.ap_mode) {
            isAPMode = true;
            statusEl.innerHTML = 'WiFi: <strong>Access Point Mode</strong> (not connected to a network)';
            descEl.textContent = 'Enter your WiFi credentials to connect the thermostat to your network. MQTT is optional.';
            configEl.style.display = 'block';
        } else if (data.wifi_connected) {
            isAPMode = false;
            statusEl.innerHTML = 'WiFi: <strong>Connected</strong> to ' +
                (data.wifi_ssid || 'network') + ' (' + (data.ip || '') + ')';
            descEl.textContent = 'WiFi is connected. Optionally set up MQTT for Home Assistant integration.';
            configEl.style.display = 'none';
        } else {
            isAPMode = false;
            statusEl.innerHTML = 'WiFi: <strong>Disconnected</strong>';
            descEl.textContent = 'WiFi is not connected. Enter credentials below.';
            configEl.style.display = 'block';
        }
    } catch (e) {
        document.getElementById('wifi-status').innerHTML = 'WiFi: <em>Could not check status</em>';
    }
}

function renderAnimalGrid() {
    const grid = document.getElementById('animal-grid');
    const icons = {
        'bearded_dragon': '&#x1F98E;',
        'ball_python': '&#x1F40D;',
        'leopard_gecko': '&#x1F98E;',
        'crested_gecko': '&#x1F98E;',
        'corn_snake': '&#x1F40D;'
    };
    let html = '';
    profiles.forEach(function(p) {
        html += '<div class="animal-card" onclick="selectAnimal(\'' + p.id + '\')" id="card-' + p.id + '">' +
                '<div class="animal-icon">' + (icons[p.id] || '&#x1F98E;') + '</div>' +
                '<div class="animal-name">' + p.name + '</div></div>';
    });
    html += '<div class="animal-card" onclick="selectAnimal(\'custom\')" id="card-custom">' +
            '<div class="animal-icon">&#9881;</div>' +
            '<div class="animal-name">Custom Setup</div>' +
            '<div class="animal-desc">Configure manually</div></div>';
    grid.innerHTML = html;
}

async function selectAnimal(id) {
    // Update visual selection
    document.querySelectorAll('.animal-card').forEach(function(c) { c.classList.remove('selected'); });
    var card = document.getElementById('card-' + id);
    if (card) card.classList.add('selected');

    selectedProfile = id;
    var preview = document.getElementById('profile-preview');

    if (id === 'custom') {
        preview.style.display = 'none';
        profileData = null;
        return;
    }

    // Fetch full profile data
    try {
        const profileResp = await fetch('/profiles/' + id + '.json');
        profileData = await profileResp.json();

        var dayRange = profileData.temp_day_range || [25, 30];
        var nightRange = profileData.temp_night_range || [20, 25];

        document.getElementById('preview-name').textContent = profileData.name + ' (' + profileData.species + ')';
        document.getElementById('preview-day-temp').textContent = dayRange[0] + '-' + dayRange[1] + ' C';
        document.getElementById('preview-night-temp').textContent = nightRange[0] + '-' + nightRange[1] + ' C';
        document.getElementById('preview-basking').textContent = (profileData.basking_temp || 35) + ' C';
        document.getElementById('preview-photo').textContent = (profileData.photoperiod_hours || 12) + 'h';
        preview.style.display = 'block';
    } catch (e) {
        console.error('Failed to load profile:', e);
        preview.style.display = 'none';
    }
}

function selectClimate(mode) {
    selectedClimate = mode;
    document.querySelectorAll('#step-3 .option-card').forEach(function(c) { c.classList.remove('selected'); });
    event.currentTarget.classList.add('selected');
    document.getElementById('manual-temps').style.display = mode === 'manual' ? 'block' : 'none';
    document.getElementById('weather-config').style.display = mode === 'weather' ? 'block' : 'none';
}

function selectSeason(mode) {
    selectedSeason = mode;
    document.querySelectorAll('#step-4 .option-card').forEach(function(c) { c.classList.remove('selected'); });
    event.currentTarget.classList.add('selected');
}

function updateProgress() {
    var pct = ((currentStep - 1) / (totalSteps - 1)) * 100;
    document.getElementById('progress-fill').style.width = pct + '%';
    document.getElementById('step-indicator').textContent = 'Step ' + currentStep + ' of ' + totalSteps;

    // Show/hide nav buttons
    document.getElementById('btn-prev').style.display = currentStep > 1 ? 'inline-block' : 'none';
    document.getElementById('btn-next').style.display = currentStep < totalSteps ? 'inline-block' : 'none';
    document.getElementById('btn-finish').style.display = currentStep === totalSteps ? 'inline-block' : 'none';

    // Show active step
    for (var i = 1; i <= totalSteps; i++) {
        var el = document.getElementById('step-' + i);
        if (el) el.classList.toggle('active', i === currentStep);
    }

    // Build review on last step
    if (currentStep === totalSteps) buildReview();
}

function nextStep() {
    if (currentStep < totalSteps) {
        currentStep++;
        updateProgress();
    }
}

function prevStep() {
    if (currentStep > 1) {
        currentStep--;
        updateProgress();
    }
}

function buildReview() {
    var html = '';
    var deviceName = document.getElementById('device-name').value || 'ESP32-Thermostat';
    var animal = selectedProfile === 'custom' ? 'Custom' : (profileData ? profileData.name : selectedProfile);
    var climate = selectedClimate === 'auto' ? 'Auto from Profile' : selectedClimate === 'manual' ? 'Manual Custom' : 'Weather Sync';
    var season = selectedSeason === 'standard' ? 'Year-Round' : selectedSeason === 'dry' ? 'Dry Season' : 'Wet Season';

    html += '<div class="review-row"><span class="review-label">Device Name</span><span class="review-value">' + deviceName + '</span></div>';
    html += '<div class="review-row"><span class="review-label">Animal</span><span class="review-value">' + animal + '</span></div>';
    html += '<div class="review-row"><span class="review-label">Climate Mode</span><span class="review-value">' + climate + '</span></div>';
    html += '<div class="review-row"><span class="review-label">Season</span><span class="review-value">' + season + '</span></div>';

    // WiFi info
    var wifiSsidEl = document.getElementById('wifi-ssid');
    if (isAPMode && wifiSsidEl && wifiSsidEl.value) {
        html += '<div class="review-row"><span class="review-label">WiFi Network</span><span class="review-value">' + wifiSsidEl.value + '</span></div>';
    }

    for (var i = 1; i <= 3; i++) {
        var name = document.getElementById('out' + i + '-name').value || 'Output ' + i;
        var mode = document.getElementById('out' + i + '-mode').value;
        html += '<div class="review-row"><span class="review-label">' + name + '</span><span class="review-value">' + mode.toUpperCase() + '</span></div>';
    }

    var mqttEnabled = document.getElementById('mqtt-enable').checked;
    html += '<div class="review-row"><span class="review-label">MQTT</span><span class="review-value">' + (mqttEnabled ? 'Enabled' : 'Disabled') + '</span></div>';

    document.getElementById('review-content').innerHTML = html;
}

async function applyWizard() {
    var btn = document.getElementById('btn-finish');
    btn.textContent = 'Applying...';
    btn.disabled = true;

    try {
        var deviceName = document.getElementById('device-name').value || 'ESP32-Thermostat';

        // 1. Apply animal profile if selected
        if (selectedProfile && selectedProfile !== 'custom') {
            await fetch('/api/v1/profiles/apply', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ id: selectedProfile })
            });
        }

        // 2. Configure outputs
        for (var i = 1; i <= 3; i++) {
            var outName = document.getElementById('out' + i + '-name').value;
            var outMode = document.getElementById('out' + i + '-mode').value;
            var data = {};
            if (outName) data.name = outName;
            data.enabled = outMode !== 'off';

            await fetch('/api/output/' + i + '/config', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(data)
            });

            // Set mode and target
            var controlData = { mode: outMode };
            if (selectedProfile !== 'custom' && profileData) {
                if (i === 1) controlData.target = profileData.basking_temp || 35;
                else if (i === 2) controlData.target = (profileData.temp_day_range || [26, 30])[1];
                else controlData.target = (profileData.temp_day_range || [24, 28])[0];
            }
            await fetch('/api/output/' + i + '/control', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(controlData)
            });
        }

        // 3. Build save-settings body with all fields
        var body = 'device_name=' + encodeURIComponent(deviceName);
        body += '&wizard_complete=1';

        // WiFi credentials (if in AP mode and user provided them)
        var wifiSsidEl = document.getElementById('wifi-ssid');
        var wifiPassEl = document.getElementById('wifi-pass-input');
        if (wifiSsidEl && wifiSsidEl.value) {
            body += '&wifi_ssid=' + encodeURIComponent(wifiSsidEl.value);
            if (wifiPassEl && wifiPassEl.value) {
                body += '&wifi_pass=' + encodeURIComponent(wifiPassEl.value);
            }
        }

        // MQTT settings
        var mqttEnabled = document.getElementById('mqtt-enable').checked;
        if (mqttEnabled) {
            var broker = document.getElementById('mqtt-broker').value;
            var port = document.getElementById('mqtt-port').value;
            var user = document.getElementById('mqtt-user').value;
            var pass = document.getElementById('mqtt-pass').value;
            if (broker) body += '&mqtt_broker=' + encodeURIComponent(broker);
            if (port) body += '&mqtt_port=' + encodeURIComponent(port);
            if (user) body += '&mqtt_user=' + encodeURIComponent(user);
            if (pass) body += '&mqtt_pass=' + encodeURIComponent(pass);
        }

        // Save all settings and mark wizard complete
        await fetch('/api/save-settings', {
            method: 'POST',
            headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
            body: body
        });

        alert('Setup complete! Your thermostat is configured and will restart.');
        window.location.href = '/';
    } catch (e) {
        alert('Error applying settings: ' + e.message);
        btn.textContent = 'Apply Settings';
        btn.disabled = false;
    }
}

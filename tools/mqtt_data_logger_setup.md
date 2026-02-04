# MQTT Data Logger Setup (Raspberry Pi)

## Prerequisites
- Raspberry Pi (any model) with Python 3.7+
- HiveMQ Cloud account with credentials configured
- ESP32 thermostat publishing to HiveMQ Cloud

## Install

```bash
# Install paho-mqtt
pip3 install paho-mqtt
```

## Run

### Option 1: Command line arguments
```bash
python3 mqtt_data_logger.py \
    --host abc123.s1.eu.hivemq.cloud \
    --user your_username \
    --password your_password
```

### Option 2: Environment variables
```bash
export MQTT_HOST=abc123.s1.eu.hivemq.cloud
export MQTT_USER=your_username
export MQTT_PASS=your_password
python3 mqtt_data_logger.py
```

### Options
| Flag | Env Var | Default | Description |
|------|---------|---------|-------------|
| `--host` | `MQTT_HOST` | (required) | HiveMQ Cloud hostname |
| `--port` | `MQTT_PORT` | 8883 | MQTT broker port |
| `--user` | `MQTT_USER` | (required) | MQTT username |
| `--password` | `MQTT_PASS` | (required) | MQTT password |
| `--db` | `DB_PATH` | thermostat_data.db | SQLite database file path |

## Query the data

```bash
# View recent readings
sqlite3 thermostat_data.db "SELECT * FROM readings ORDER BY timestamp DESC LIMIT 10;"

# Average temperature for output 1 in the last 24 hours
sqlite3 thermostat_data.db "SELECT AVG(temperature) FROM readings WHERE output_num=1 AND timestamp > datetime('now', '-1 day');"

# Count readings per output
sqlite3 thermostat_data.db "SELECT output_num, COUNT(*) FROM readings GROUP BY output_num;"
```

## Run as a systemd service (auto-start)

Create `/etc/systemd/system/thermostat-logger.service`:

```ini
[Unit]
Description=Thermostat MQTT Data Logger
After=network-online.target
Wants=network-online.target

[Service]
Type=simple
User=pi
Environment=MQTT_HOST=abc123.s1.eu.hivemq.cloud
Environment=MQTT_USER=your_username
Environment=MQTT_PASS=your_password
Environment=DB_PATH=/home/pi/thermostat_data.db
ExecStart=/usr/bin/python3 /home/pi/mqtt_data_logger.py
Restart=always
RestartSec=10

[Install]
WantedBy=multi-user.target
```

Then enable and start:
```bash
sudo systemctl daemon-reload
sudo systemctl enable thermostat-logger
sudo systemctl start thermostat-logger

# Check status
sudo systemctl status thermostat-logger

# View logs
journalctl -u thermostat-logger -f
```

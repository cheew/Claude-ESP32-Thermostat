#!/usr/bin/env python3
"""
MQTT Data Logger for ESP32 Thermostat
Subscribes to HiveMQ Cloud and stores readings in SQLite.

Usage:
    python3 mqtt_data_logger.py --host abc123.s1.eu.hivemq.cloud --user myuser --password mypass

Or via environment variables:
    export MQTT_HOST=abc123.s1.eu.hivemq.cloud
    export MQTT_USER=myuser
    export MQTT_PASS=mypass
    python3 mqtt_data_logger.py
"""

import os
import sys
import json
import sqlite3
import ssl
import re
import time
import argparse
import logging
from datetime import datetime

try:
    import paho.mqtt.client as mqtt
except ImportError:
    print("Error: paho-mqtt not installed. Run: pip install paho-mqtt")
    sys.exit(1)

# Logging setup
logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(message)s",
    datefmt="%Y-%m-%d %H:%M:%S"
)
log = logging.getLogger("mqtt_logger")

# Topic pattern: thermostat/{device_id}/output{N}/status
TOPIC_PATTERN = re.compile(r"^thermostat/([^/]+)/output(\d+)/status$")

DB_SCHEMA = """
CREATE TABLE IF NOT EXISTS readings (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp TEXT NOT NULL,
    device_id TEXT NOT NULL,
    output_num INTEGER NOT NULL,
    temperature REAL,
    setpoint REAL,
    heating INTEGER,
    mode TEXT,
    power INTEGER,
    wifi_rssi INTEGER,
    free_heap INTEGER,
    uptime INTEGER
);

CREATE INDEX IF NOT EXISTS idx_readings_time ON readings(timestamp);
CREATE INDEX IF NOT EXISTS idx_readings_device_output ON readings(device_id, output_num);
"""


def init_database(db_path):
    """Initialize SQLite database with schema."""
    conn = sqlite3.connect(db_path)
    conn.executescript(DB_SCHEMA)
    conn.commit()
    log.info(f"Database ready: {db_path}")
    return conn


def store_reading(conn, device_id, output_num, data):
    """Insert a reading into the database."""
    conn.execute(
        """INSERT INTO readings
           (timestamp, device_id, output_num, temperature, setpoint,
            heating, mode, power, wifi_rssi, free_heap, uptime)
           VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)""",
        (
            datetime.utcnow().isoformat(),
            device_id,
            output_num,
            data.get("temperature"),
            data.get("setpoint"),
            1 if data.get("heating") else 0,
            data.get("mode"),
            data.get("power"),
            data.get("wifi_rssi"),
            data.get("free_heap"),
            data.get("uptime"),
        ),
    )
    conn.commit()


def on_connect(client, userdata, flags, rc):
    """Called when connected to MQTT broker."""
    if rc == 0:
        log.info("Connected to MQTT broker")
        # Subscribe to all thermostat status topics
        topic = "thermostat/+/output+/status"
        client.subscribe(topic)
        log.info(f"Subscribed to: {topic}")
        # Also subscribe with wildcard for output numbers
        client.subscribe("thermostat/+/output1/status")
        client.subscribe("thermostat/+/output2/status")
        client.subscribe("thermostat/+/output3/status")
        # Subscribe to online status
        client.subscribe("thermostat/+/status/online")
    else:
        log.error(f"Connection failed with code {rc}")


def on_disconnect(client, userdata, rc):
    """Called when disconnected from MQTT broker."""
    if rc != 0:
        log.warning(f"Unexpected disconnect (rc={rc}), will reconnect...")
    else:
        log.info("Disconnected from MQTT broker")


def on_message(client, userdata, msg):
    """Called when a message is received."""
    conn = userdata["db"]
    topic = msg.topic

    # Check for online/offline status
    if topic.endswith("/status/online"):
        try:
            status = msg.payload.decode("utf-8")
            log.info(f"Device status: {topic} = {status}")
        except Exception:
            pass
        return

    # Parse topic to extract device_id and output_num
    match = TOPIC_PATTERN.match(topic)
    if not match:
        return

    device_id = match.group(1)
    output_num = int(match.group(2))

    # Parse JSON payload
    try:
        data = json.loads(msg.payload.decode("utf-8"))
    except (json.JSONDecodeError, UnicodeDecodeError) as e:
        log.warning(f"Failed to parse payload on {topic}: {e}")
        return

    # Store reading
    try:
        store_reading(conn, device_id, output_num, data)
        temp = data.get("temperature", "?")
        mode = data.get("mode", "?")
        log.info(f"Stored: {device_id} output{output_num} temp={temp} mode={mode}")
    except sqlite3.Error as e:
        log.error(f"Database error: {e}")


def main():
    parser = argparse.ArgumentParser(description="MQTT Data Logger for ESP32 Thermostat")
    parser.add_argument("--host", default=os.environ.get("MQTT_HOST", ""),
                        help="MQTT broker hostname")
    parser.add_argument("--port", type=int, default=int(os.environ.get("MQTT_PORT", "8883")),
                        help="MQTT broker port (default: 8883)")
    parser.add_argument("--user", default=os.environ.get("MQTT_USER", ""),
                        help="MQTT username")
    parser.add_argument("--password", default=os.environ.get("MQTT_PASS", ""),
                        help="MQTT password")
    parser.add_argument("--db", default=os.environ.get("DB_PATH", "thermostat_data.db"),
                        help="SQLite database path (default: thermostat_data.db)")
    args = parser.parse_args()

    if not args.host:
        log.error("MQTT host required. Use --host or set MQTT_HOST env var.")
        sys.exit(1)
    if not args.user or not args.password:
        log.error("MQTT credentials required. Use --user/--password or MQTT_USER/MQTT_PASS env vars.")
        sys.exit(1)

    # Initialize database
    conn = init_database(args.db)

    # Setup MQTT client
    client = mqtt.Client(userdata={"db": conn})
    client.username_pw_set(args.user, args.password)

    # Configure TLS
    client.tls_set(tls_version=ssl.PROTOCOL_TLSv1_2)

    # Set callbacks
    client.on_connect = on_connect
    client.on_disconnect = on_disconnect
    client.on_message = on_message

    # Connect
    log.info(f"Connecting to {args.host}:{args.port}...")
    try:
        client.connect(args.host, args.port, keepalive=60)
    except Exception as e:
        log.error(f"Failed to connect: {e}")
        sys.exit(1)

    # Run forever (handles reconnection automatically)
    log.info("Data logger running. Press Ctrl+C to stop.")
    try:
        client.loop_forever()
    except KeyboardInterrupt:
        log.info("Shutting down...")
        client.disconnect()
        conn.close()


if __name__ == "__main__":
    main()

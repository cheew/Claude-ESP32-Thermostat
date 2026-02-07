# Claude Starter Notes (from More updates 7.2.26.md)

Date of source notes: 2026-02-07
Project: ESP32 Multi-Output Thermostat

## UI and UX Requests
- Home screen: indicate per output when a temperature is syncing with local weather (if that feature is active).
- Header bar: show the animal symbol when one is selected (only if feasible).
- Dark mode: apply the advanced home screen fixes to the simple home screen; remaining simple-mode dark issues need alignment.
- Wizard: update the color scheme to match the web server styling.
- Schedule page: add a 24-hour graph at the top showing the current scheduled temperature (or on/off for relay outputs). This graph should reflect the currently selected output and update live as schedule edits are made or when a CSV is uploaded.
- Simple mode: if possible, include the schedule graph there as well.

## Weather Sync Logic
- Weather data must match the same time of day as the user’s location (example: user is in UK at midday; do not use Australia data when it is midnight there).
- If the weather API only provides current data for other locations, delay or choose data from the last time the target location had the same local time. This should be implemented using time zones or similar.
- Need to verify what time-based data is available from the weather API to support this.

## Logging
- Logs and console should include real-time timestamps (clock time), not only uptime-based timing.

## Help and Documentation
- Create help pages for:
  - Outputs
  - Sensors
  - Schedules
- Consider an onboard wiki or help file for general usage.

## Open Questions
- With the new filesystem, what is the manual update process? Is it still a single .bin file?
- How do we add species to the wizard?

## Notes
- These items came from a raw brainstorming list and may need prioritization.

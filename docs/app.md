# Website guide

Run the commands in this guide from the repository root. Website source lives in [`app/`](../app/).

Brain Buddy's brain mascot is named **Brain Buddy**. The browser favicon and home-screen app icons use that same mascot artwork.

The app is caregiver-only, with no patient view switch. It opens with a **You’ve got a message!** bubble and Brain Buddy on a pale blue background matching the map’s ocean. Tapping the bubble or mascot moves Brain Buddy to the left and opens the conversation. The conversation opens at the newest messages in chronological order. The opening invitation, incoming and outgoing messages, and recording controls all use the same pale lavender as the clouds. Outgoing messages are aligned to the right and labeled “You.” The scroll area shows up to four compact messages at once. Scroll upward to read older messages; additional history loads automatically, preserving the message you are reading. New arrivals do not jump the scroll position while you are reading older messages. Voice messages have play/pause buttons beside their text. **Record a voice message** stays available above the smaller **or text your message.** option. Tapping Brain Buddy again closes the conversation. Message bubbles have rounded corners and curved tails on the last incoming bubble and the outgoing composer; earlier replies have no tail. Motion respects reduced motion settings.

A scalloped cloud shape without an outline leads into the routine clock and weekly progress. The **Messages / Routine / Location / Updates** bar stays at the bottom of the screen with bold labels. It opens messages or scrolls to the selected section, with space below the content for the toolbar. The routine editor sits below the clock, followed by the map and **Updates**, which combines bracelet connection status, alerts, device status, and recent activity. Caregiver contact settings and calling controls have been removed.

Caregivers can add and remove activities under **Routine Editor**, using a name, time, and icon. The activity list starts collapsed under **Routine list**; select that row to expand or collapse it. The schedule is saved in this browser and synchronized between open tabs; it is not sent to the wristband. Removing every activity shows an empty routine. Crowded times use additional clock rings with horizontal scrolling on smaller screens.

The **Daily Routine** clock shows the current time in the browser’s local time zone, refreshes at each minute boundary, and catches up when the page becomes visible again. Elapsed activities also follow the current local time.

The clock initially displays 12 sample activities from 8 AM through 7 PM, with AM/PM labels on each symbol. These are dashboard schedule entries; actual bracelet prompts are configured separately in the firmware. Received and missed states update when matching prompt events arrive.

The map and updates are expanded on the page, with no dropdown. The interactive street map comes with pan/zoom, a bracelet marker, GPS accuracy circle, recenter control, and a link to open the coordinates in Google Maps. The map uses locally bundled [Leaflet 1.9.4](https://leafletjs.com/) with [OpenStreetMap](https://www.openstreetmap.org/copyright) street tiles. No map API key is needed. Street tiles require internet access; they use normal browser caching and are not downloaded for offline use. Location details remain readable if tiles fail.

**GPS integration is pending.** The firmware provides `gps.fix`, `gps.lat`, and `gps.lon`; the website currently expects the `latitude` / `longitude` shape below, so an adapter is still needed. The map has no sample marker or demo labels. It waits for real GPS coordinates and ignores routine replay data. Room estimates cannot determine a street position, and the site never uses the browser’s own location as the bracelet’s. Missing or invalid coordinates show no marker; old GPS timestamps are labeled as last reported. Off-wrist status refers to the bracelet rather than the person.

To connect real GPS, add this object to the bracelet’s `/state` response (example values only):

```json
"gps": { "latitude": 42.3601, "longitude": -71.0942, "accuracy": 25, "ts": 1758290400 }
```

`latitude` and `longitude` are numeric degrees; `accuracy` is an optional radius in meters. `ts` is the GPS fix’s Unix timestamp in seconds (the state’s `ts` is the fallback). Send `gps: null` or `valid: false` inside `gps` when there is no fix. Do not refresh the fix timestamp unless GPS supplies a new measurement. Coordinates marked `demo: true` are also rejected. Existing `room`, `worn`, and room/wrist events still update the text below the map.

## Voice messages

Run the local message server with Python 3. One patient and one caregiver are created automatically; there is no account setup, password, PIN, or sign in. The website always sends as the caregiver and displays incoming patient messages. The API still accepts patient messages for device integrations, and existing messages are preserved. Record a voice message, preview it, and send it. **or text your message.** opens a text field that can be sent without a recording or microphone permission. Closing the conversation preserves the draft during the page visit and stops any recording in progress. Previous demo role selections no longer affect the UI or message sender.

New microphone recordings include speech to text when the browser supports the Web Speech API. The browser's speech service may process audio online. The message form keeps the simplified bubble design without instructional text. Transcripts can be edited before sending. Audio and the edited transcript are saved together in SQLite. Text messages are saved with no audio and display without a player. Existing recordings are preserved. Browsers without speech recognition support provide a field for typing the words after recording. Voice messages are recorded with the microphone; file uploads are not offered. Speech recognition can fail or miss words, so the original recording stays available. See [browser speech recognition support](https://developer.mozilla.org/en-US/docs/Web/API/SpeechRecognition).

```sh
python3 tools/message_server.py
```

Open `http://127.0.0.1:8787/`. For a live wristband, append `?device=ESP32_IP`. Participants, recordings, and transcripts are stored in the local SQLite file `var/voice_messages.sqlite3`; previous recordings are preserved. This is a demo without authentication: anyone who can access the server can view and send messages as either role. The database is ignored by Git. To use separate devices, host this server on a trusted HTTPS connection using `VOICE_HOST`, `VOICE_TLS_CERT`, and `VOICE_TLS_KEY`; browsers require a secure context for microphone recording. The static page still shows the routine but cannot send or receive messages without the server.

The page uses Open Sans throughout, with bold headings, and follows the browser's default text size. Font files are bundled locally and cached for offline use. Clock symbols show reminder status with colors and accessible labels; there is no separate morning routine reminder box or text-size control. The clock has a **Weekly Routine** tab with seven vertical stacked bars (purple for received, red for missed, amber for unconfirmed) on a shared count axis. It summarizes only `prompt_fired`, `prompt_acked`, and `prompt_missed` events the browser has observed. Live history is stored in that browser for up to 45 days; replay history is kept separate and resets when replay starts. A blank day means there is no recorded data, not that the person missed every task.

ESP32 wearable for people with dementia/TBI. Gentle haptic prompts tied to time + room-level location, with a caregiver dashboard and local voice messaging. ~$15 of parts for the original wearable prototype, excluding the message server.

## Browser-only messaging demo

On GitHub Pages, text messages and voice recordings are saved in this browser using IndexedDB. Recordings can be replayed after reloading; text-only messages have no audio control. A small “Demo · this browser only” label distinguishes this from delivery to another person. Use `?messages=local` to enable this mode on other hosts. Browser messages are separate from the Python server database and are removed if site data is cleared.

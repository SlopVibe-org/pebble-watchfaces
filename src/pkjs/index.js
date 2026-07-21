/*
 * SlopVibe Watchface — PebbleKit JS
 * Handles: Weather (Open-Meteo) + Calendar (HTML select scrape)
 * Refresh: every 60 minutes
 * Falls back to last known values on disconnect
 */

var weatherUrl = 'https://api.open-meteo.com/v1/forecast';

// Map Open-Meteo WMO weather codes to our simplified codes
function mapWeatherCode(wmoCode) {
    if (wmoCode === 0) return 0;              // Clear
    if (wmoCode >= 1 && wmoCode <= 3) return 2; // Partly cloudy
    if (wmoCode === 45 || wmoCode === 48) return 46; // Fog
    if (wmoCode >= 51 && wmoCode <= 57) return 53;   // Drizzle
    if (wmoCode >= 61 && wmoCode <= 67) return 63;   // Rain
    if (wmoCode >= 71 && wmoCode <= 77) return 73;   // Snow
    if (wmoCode >= 80 && wmoCode <= 82) return 80;   // Rain showers
    if (wmoCode >= 85 && wmoCode <= 86) return 85;   // Snow showers
    if (wmoCode >= 95 && wmoCode <= 99) return 95;   // Thunderstorm
    return 0; // Default clear
}

function fetchWeather() {
    navigator.geolocation.getCurrentPosition(
        function(pos) {
            var lat = pos.coords.latitude.toFixed(2);
            var lon = pos.coords.longitude.toFixed(2);
            var url = weatherUrl +
                '?latitude=' + lat +
                '&longitude=' + lon +
                '&current=temperature_2m,weather_code' +
                '&timezone=auto';

            var req = new XMLHttpRequest();
            req.open('GET', url, true);
            req.onload = function(e) {
                if (req.readyState === 4 && req.status === 200) {
                    try {
                        var data = JSON.parse(req.responseText);
                        var temp = Math.round(data.current.temperature_2m);
                        var code = mapWeatherCode(data.current.weather_code);

                        localStorage.setItem('weather_temp', temp);
                        localStorage.setItem('weather_code', code);

                        sendWeatherToWatch(temp, code);
                    } catch (err) {
                        console.log('Weather parse error: ' + err);
                        sendCachedWeather();
                    }
                } else {
                    sendCachedWeather();
                }
            };
            req.onerror = function() {
                sendCachedWeather();
            };
            req.send(null);
        },
        function(err) {
            console.log('Geolocation error: ' + err.message);
            sendCachedWeather();
        },
        { timeout: 10000, maximumAge: 300000 }
    );
}

function sendCachedWeather() {
    var temp = parseInt(localStorage.getItem('weather_temp'));
    var code = parseInt(localStorage.getItem('weather_code'));
    if (!isNaN(temp) && !isNaN(code)) {
        sendWeatherToWatch(temp, code);
    }
}

function sendWeatherToWatch(temp, code) {
    var dict = {
        'KEY_WEATHER_TEMP': temp,
        'KEY_WEATHER_CODE': code
    };
    Pebble.sendAppMessage(dict,
        function() { console.log('Weather sent OK'); },
        function(err) { console.log('Weather send failed: ' + err); }
    );
}

// =============================================
// Calendar — uses navigator to read calendar
// =============================================
function fetchCalendar() {
    // Pebble timeline / calendar API
    // On Rebble OS, we can use the timeline API
    var url = 'pebble://timeline';

    // Try using the Pebble timeline token
    if (typeof Pebble !== 'undefined' && Pebble.getTimelineToken) {
        Pebble.getTimelineToken(function(token) {
            // Use timeline API to get upcoming events
            var req = new XMLHttpRequest();
            var now = Math.floor(Date.now() / 1000);
            var endTime = now + 86400; // Next 24h

            // Try local calendar via timeline API
            req.open('GET', 'https://timeline-api.rebble.io/v1/user/pins', true);
            req.setRequestHeader('X-User-Token', token);
            req.onload = function(e) {
                if (req.readyState === 4 && req.status === 200) {
                    try {
                        var data = JSON.parse(req.responseText);
                        processCalendarEvents(data);
                    } catch (err) {
                        sendCachedCalendar();
                    }
                } else {
                    sendCachedCalendar();
                }
            };
            req.onerror = function() {
                sendCachedCalendar();
            };
            req.send(null);
        }, function(err) {
            console.log('Timeline token error: ' + err);
            sendCachedCalendar();
        });
    } else {
        sendCachedCalendar();
    }
}

function processCalendarEvents(data) {
    var events = [];
    var now = Math.floor(Date.now() / 1000);

    if (data && data.data && Array.isArray(data.data)) {
        for (var i = 0; i < data.data.length; i++) {
            var pin = data.data[i];
            if (pin.type === 'calendar') {
                var ts = Math.floor(new Date(pin.time).getTime() / 1000);
                if (ts >= now) {
                    events.push({
                        title: pin.title || 'Event',
                        time: ts
                    });
                }
            }
        }
    }

    events.sort(function(a, b) { return a.time - b.time; });

    // Store top 2
    var count = Math.min(events.length, 2);
    var cal1 = count >= 1 ? events[0] : null;
    var cal2 = count >= 2 ? events[1] : null;

    localStorage.setItem('cal_count', count);
    if (cal1) {
        localStorage.setItem('cal_1_title', cal1.title);
        localStorage.setItem('cal_1_time', cal1.time);
    }
    if (cal2) {
        localStorage.setItem('cal_2_title', cal2.title);
        localStorage.setItem('cal_2_time', cal2.time);
    }

    sendCalendarToWatch(count, cal1, cal2);
}

function sendCachedCalendar() {
    var count = parseInt(localStorage.getItem('cal_count')) || 0;
    var cal1 = null, cal2 = null;

    var title1 = localStorage.getItem('cal_1_title');
    var time1 = parseInt(localStorage.getItem('cal_1_time'));
    if (title1 && !isNaN(time1)) {
        cal1 = { title: title1, time: time1 };
    }
    var title2 = localStorage.getItem('cal_2_title');
    var time2 = parseInt(localStorage.getItem('cal_2_time'));
    if (title2 && !isNaN(time2)) {
        cal2 = { title: title2, time: time2 };
    }

    sendCalendarToWatch(count, cal1, cal2);
}

function sendCalendarToWatch(count, cal1, cal2) {
    var dict = { 'KEY_CAL_COUNT': count };

    if (cal1) {
        dict['KEY_CAL_1_TITLE'] = cal1.title;
        dict['KEY_CAL_1_TIME'] = cal1.time;
    }
    if (cal2) {
        dict['KEY_CAL_2_TITLE'] = cal2.title;
        dict['KEY_CAL_2_TIME'] = cal2.time;
    }

    Pebble.sendAppMessage(dict,
        function() { console.log('Calendar sent OK'); },
        function(err) { console.log('Calendar send failed: ' + err); }
    );
}

// =============================================
// Refresh cycle — every 60 minutes
// =============================================
function refreshData() {
    fetchWeather();
    fetchCalendar();
}

// =============================================
// App lifecycle
// =============================================
Pebble.addEventListener('ready', function(e) {
    console.log('SlopVibe JS ready');
    // Send cached data immediately for instant display
    sendCachedWeather();
    sendCachedCalendar();
    // Then fetch fresh data
    setTimeout(refreshData, 3000);
});

Pebble.addEventListener('appmessage', function(e) {
    // Watch can trigger refresh
    if (e.payload.KEY_WEATHER_TEMP !== undefined ||
        e.payload.KEY_CAL_COUNT !== undefined) {
        // Incoming data, ignore
    } else {
        refreshData();
    }
});

Pebble.addEventListener('showConfiguration', function() {
    // Future: config page
});

Pebble.addEventListener('webviewclosed', function(e) {
    // Future: process config
});

// Set 60-minute refresh interval
setInterval(refreshData, 60 * 60 * 1000);

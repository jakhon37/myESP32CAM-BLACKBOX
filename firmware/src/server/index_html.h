#pragma once

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>ESP32-CAM</title>
  <style>
*, *::before, *::after { box-sizing: border-box; margin: 0; padding: 0; }
body {
  background: #0f0f0f;
  color: #e0e0e0;
  font-family: -apple-system, sans-serif;
  min-height: 100vh;
  padding: 16px;
}
h1 { font-size: 16px; font-weight: 500; margin-bottom: 16px;
     color: #fff; letter-spacing: 0.5px; }

/* stream */
.stream-wrap {
  background: #1a1a1a;
  border-radius: 10px;
  overflow: hidden;
  margin-bottom: 16px;
  position: relative;
}
#stream {
  width: 100%;
  display: block;
  max-height: 400px;
  object-fit: contain;
  background: #000;
}
.stream-overlay {
  position: absolute; top: 8px; right: 8px;
  background: rgba(0,0,0,0.6);
  border-radius: 6px; padding: 4px 10px;
  font-size: 11px; color: #4caf50;
}
.stream-overlay.off { color: #f44336; }

/* controls grid */
.grid { display: grid; grid-template-columns: 1fr 1fr; gap: 12px;
        margin-bottom: 16px; }
.card {
  background: #1a1a1a;
  border-radius: 10px;
  padding: 14px;
}
.card h2 { font-size: 12px; color: #888; font-weight: 400;
           margin-bottom: 10px; text-transform: uppercase;
           letter-spacing: 0.5px; }

/* buttons */
button {
  width: 100%;
  padding: 9px;
  border: none;
  border-radius: 7px;
  font-size: 13px;
  cursor: pointer;
  background: #2a2a2a;
  color: #e0e0e0;
  margin-bottom: 6px;
  transition: all .2s;
}
button:disabled { opacity: 0.5; cursor: not-allowed; }
button:last-child { margin-bottom: 0; }
button:active { transform: scale(0.98); }
button.active { background: #1e4d2b; color: #4caf50; border: 1px solid #4caf50; }
button.loading { background: #333; color: #888; }
button.success { background: #1e4d2b; color: #fff; }

/* resolution & fps */
.select-wrap { margin-bottom: 10px; }
select {
  width: 100%;
  padding: 9px;
  border-radius: 7px;
  background: #2a2a2a;
  color: #e0e0e0;
  border: 1px solid #333;
  font-size: 13px;
  margin-top: 4px;
}

/* snapshot preview */
.snap-wrap { margin-bottom: 16px; display: none; }
.snap-wrap img {
  width: 100%;
  border-radius: 10px;
  display: block;
}
.snap-label {
  font-size: 11px; color: #666;
  margin-top: 6px; text-align: center;
}

/* sd card */
.sd-info {
  background: #1a1a1a;
  border-radius: 10px;
  padding: 14px;
  margin-bottom: 16px;
}
.sd-info h2 { font-size: 12px; color: #888; font-weight: 400;
              margin-bottom: 10px; text-transform: uppercase;
              letter-spacing: 0.5px; }
.sd-row { display: flex; justify-content: space-between;
          font-size: 13px; padding: 4px 0;
          border-bottom: 1px solid #252525; }
.sd-row:last-child { border-bottom: none; }
.sd-val { color: #4caf50; }
.file-list { margin-top: 10px; max-height: 250px;
             overflow-y: auto; }
.file-item { font-size: 12px; color: #aaa; padding: 6px 0;
             border-bottom: 1px solid #222; display: flex;
             justify-content: space-between; align-items: center; }
.file-actions { display: flex; gap: 8px; }
.file-actions a, .file-actions span { 
  color: #4caf50; cursor: pointer; text-decoration: none; 
  font-weight: 500; padding: 2px 6px; background: #222; border-radius: 4px;
}
.file-actions span.del { color: #f44336; }

/* status bar */
.status-bar {
  background: #1a1a1a;
  border-radius: 10px;
  padding: 10px 14px;
  font-size: 12px;
  color: #666;
  display: flex;
  justify-content: space-between;
}
.status-bar span { color: #888; }

@keyframes blink {
  0% { background: #f44336; box-shadow: 0 0 5px #f44336; }
  50% { background: #333; box-shadow: none; }
  100% { background: #f44336; box-shadow: 0 0 5px #f44336; }
}
.blink { animation: blink 1s infinite; }

</style>
</head>
<body>

<h1>ESP32-CAM</h1>

<!-- stream -->
<div class="stream-wrap">
  <img id="stream" alt="stream">
  <div class="stream-overlay" id="stream-status">LIVE</div>
</div>

<!-- snapshot preview -->
<div class="snap-wrap" id="snap-wrap">
  <img id="snap-img" alt="snapshot">
  <div class="snap-label" id="snap-label">snapshot</div>
</div>

<!-- controls -->
<div class="grid">
  <div class="card">
    <h2>Camera</h2>
    <button id="snap-btn" onclick="takeSnapshot()">Take Snapshot</button>
    <button id="stream-btn" onclick="toggleStream()">Start Stream</button>
  </div>
  <div class="card">
    <h2>LEDs</h2>
    <div style="margin-bottom: 10px">
      <label style="font-size:10px; color:#666">FLASH (GPIO 4)</label>
      <button id="flash-btn" onclick="toggleLed('flash')">Flash: OFF</button>
    </div>
    <div>
      <label style="font-size:10px; color:#666">STATUS (GPIO 33)</label>
      <button id="status-btn" onclick="toggleLed('status')">Status: OFF</button>
    </div>
  </div>
  <div class="card">
    <h2>Black Box</h2>
    <div style="display: flex; align-items: center; margin-bottom: 10px;">
      <div id="rec-dot" style="width:10px; height:10px; background:#444; border-radius:50%; margin-right:8px;"></div>
      <span id="rec-status" style="font-size:12px; color:#888;">Standby</span>
    </div>
    <button id="rec-btn" onclick="toggleRecording()">Start Recording</button>
    <div class="select-wrap">
      <label style="font-size:10px; color:#666">STORAGE FULL</label>
      <select id="rec-policy">
        <option value="stop">Stop Recording</option>
        <option value="overwrite">Overwrite Oldest</option>
      </select>
    </div>
  </div>
  <div class="card">
    <h2>Settings</h2>
    <div class="select-wrap">
      <label style="font-size:10px; color:#666">RESOLUTION</label>
      <select id="res-select" onchange="setResolution(this.value)">
        <option value="QQVGA">160×120 QQVGA</option>
        <option value="QVGA">320×240 QVGA</option>
        <option value="VGA" selected>640×480 VGA</option>
        <option value="SVGA">800×600 SVGA</option>
        <option value="XGA">1024×768 XGA</option>
      </select>
    </div>
    <div class="select-wrap">
      <label style="font-size:10px; color:#666">MAX FPS</label>
      <select id="fps-select" onchange="setFPS(this.value)">
        <option value="1">1 FPS (Lowest CPU)</option>
        <option value="5">5 FPS</option>
        <option value="10" selected>10 FPS</option>
        <option value="20">20 FPS</option>
        <option value="0">Max (Heavy CPU)</option>
      </select>
    </div>
  </div>
  <div class="card">
    <h2>Display</h2>
    <div class="select-wrap">
      <label style="font-size:10px; color:#666">WINDOW SIZE</label>
      <select id="size-select" onchange="setDisplaySize(this.value)">
        <option value="300px">Small</option>
        <option value="400px" selected>Medium</option>
        <option value="600px">Large</option>
        <option value="800px">X-Large</option>
        <option value="100%">Full Width</option>
      </select>
    </div>
  </div>
  <div class="card">
    <h2>System</h2>
    <button id="stat-btn" onclick="refreshStatus()">Update Status</button>
    <button onclick="setCamIP()">Change IP</button>
  </div>
</div>

<!-- SD card -->
<div class="sd-info" id="sd-info">
  <h2>SD card</h2>
  <div id="sd-content">
    <div class="sd-row"><span>Status</span><span class="sd-val" id="sd-status">checking...</span></div>
    <div class="sd-row"><span>Total</span><span class="sd-val" id="sd-total">—</span></div>
    <div class="sd-row"><span>Used</span><span class="sd-val" id="sd-used">—</span></div>
  </div>
  <div class="file-list" id="file-list"></div>
</div>

<!-- status -->
<div class="status-bar">
  <span>Heap: <span id="heap">—</span></span>
  <span>PSRAM: <span id="psram">—</span></span>
  <span id="cam-ip">—</span>
</div>

<script>
// ---- config ----
let CAM = window.location.origin;

// If running on dev server (port 8080), use local storage or fallback to AP IP
if (window.location.port === '8080') {
  const savedIP = localStorage.getItem('esp32_cam_ip');
  CAM = savedIP ? savedIP : 'http://192.168.4.1';
}

console.log('[CONF] Targeting ESP32 at:', CAM);

function setCamIP() {
  const ip = prompt('Enter ESP32-CAM IP (e.g. http://192.168.1.50):', CAM);
  if (ip) {
    const formattedIP = ip.startsWith('http') ? ip : 'http://' + ip;
    localStorage.setItem('esp32_cam_ip', formattedIP);
    location.reload();
  }
}

// ---- helpers ----
function btnFeedback(id, type) {
  const btn = document.getElementById(id);
  if (!btn) return function() {}; // Return dummy function if button not found

  const oldTxt = btn.textContent;
  const oldClass = btn.className;

  if (type === 'loading') {
    btn.disabled = true;
    btn.className = 'loading';
    return function(newTxt, newClass) { 
      btn.disabled = false; 
      btn.className = newClass !== undefined ? newClass : oldClass; 
      btn.textContent = newTxt !== undefined ? newTxt : oldTxt;
    };
  }

  if (type === 'success') {
    btn.className = 'success';
    btn.textContent = 'OK!';
    setTimeout(function() { 
      btn.className = oldClass; 
      btn.textContent = oldTxt; 
    }, 1000);
  }
  return function() {};
}

let streaming = false;

// ---- stream ----
function startStream() {
  streaming = true;
  const img = document.getElementById('stream');
  const status = document.getElementById('stream-status');
  const btn = document.getElementById('stream-btn');

  if (img) img.src = CAM + '/stream?t=' + Date.now();
  if (btn) {
    btn.textContent = 'Stop Stream';
    btn.className = 'active';
  }

  if (img && status) {
    img.onload  = function() { 
      status.textContent = 'LIVE'; 
      status.className = 'stream-overlay'; 
    };
    img.onerror = function() {
      if (streaming) {
        status.textContent = 'OFF';
        status.className = 'stream-overlay off';
      }
    };
  }
}

function stopStream() {
  streaming = false;
  const img = document.getElementById('stream');
  const status = document.getElementById('stream-status');
  const btn = document.getElementById('stream-btn');

  if (img) img.src = '';
  if (btn) {
    btn.textContent = 'Start Stream';
    btn.className = '';
  }
  if (status) {
    status.textContent = 'IDLE';
    status.className = 'stream-overlay off';
  }
}

function toggleStream() {
  if (streaming) stopStream();
  else           startStream();
}

// ---- snapshot ----
function takeSnapshot() {
  const done = btnFeedback('snap-btn', 'loading');
  const wrap  = document.getElementById('snap-wrap');
  const img   = document.getElementById('snap-img');
  const label = document.getElementById('snap-label');
  
  if (!img) { done(); return; }

  img.src = CAM + '/capture?t=' + Date.now();
  img.onload = function() {
    if (wrap) wrap.style.display = 'block';
    if (label) label.textContent = 'saved at ' + new Date().toLocaleTimeString();
    done();
    btnFeedback('snap-btn', 'success');
  };
  img.onerror = function(e) { 
    console.error('[ERR] Capture failed:', e);
    done(); 
    alert('Capture failed. Check connection.'); 
  };
}

let ledStates = { flash: false, status: false };

// ---- LED ----
function toggleLed(type) {
  const newState = !ledStates[type] ? 'on' : 'off';
  const done = btnFeedback(type + '-btn', 'loading');

  fetch(CAM + '/led?type=' + type + '&state=' + newState)
    .then(function(r) { return r.json(); })
    .then(function(d) {
      ledStates[type] = (d.led === 'on');
      const label = type.charAt(0).toUpperCase() + type.slice(1);
      const newTxt = label + ': ' + (ledStates[type] ? 'ON' : 'OFF');
      const newClass = ledStates[type] ? 'active' : '';
      done(newTxt, newClass);
    })
    .catch(function(err) { 
      console.error('[ERR] LED failed:', err);
      done(); 
      alert(type + ' LED command failed'); 
    });
}

// ---- settings ----
let recording = false;

function toggleRecording() {
  const action = !recording ? 'start' : 'stop';
  const policyEl = document.getElementById('rec-policy');
  const policy = policyEl ? policyEl.value : 'stop';
  const done = btnFeedback('rec-btn', 'loading');

  fetch(CAM + '/record?action=' + action + '&policy=' + policy)
    .then(function(r) { return r.json(); })
    .then(function(d) {
      recording = d.recording;
      const newTxt = recording ? 'Stop Recording' : 'Start Recording';
      const newClass = recording ? 'active' : '';
      done(newTxt, newClass);
      updateRecordingUI(d);
    })
    .catch(function(err) { 
      console.error('[ERR] Record failed:', err);
      done(); 
      alert('Recording failed'); 
    });
}

function updateRecordingUI(d) {
  recording = d.recording;
  const btn = document.getElementById('rec-btn');
  const dot = document.getElementById('rec-dot');
  const status = document.getElementById('rec-status');

  if (btn) {
    btn.textContent = recording ? 'Stop Recording' : 'Start Recording';
    btn.className = recording ? 'active' : '';
  }
  if (dot) {
    dot.className = recording ? 'blink' : '';
    dot.style.background = recording ? '' : '#444';
  }
  if (status) {
    status.textContent = recording ? 'REC: ' + d.duration + 's' : 'Standby';
    status.style.color = recording ? '#f44336' : '#888';
  }
}

function setResolution(size) {
  const select = document.getElementById('res-select');
  if (select) select.disabled = true;
  
  const wasStreaming = streaming;
  if (wasStreaming) stopStream();
  
  // Give it a moment to ensure stream task is yielded
  setTimeout(function() {
    fetch(CAM + '/resolution?size=' + size)
      .then(function(res) {
        if (!res.ok) throw new Error('Status ' + res.status);
        console.log('[RES] Switched to:', size);
        if (wasStreaming) {
          setTimeout(startStream, 1000); // Wait for sensor to settle
        }
      })
      .catch(function(err) {
        console.error('[ERR] Resolution failed:', err);
        alert('Resolution change failed');
      })
      .finally(function() {
        if (select) select.disabled = false;
      });
  }, 200);
}

function setFPS(val) {
  const select = document.getElementById('fps-select');
  if (select) select.disabled = true;
  fetch(CAM + '/fps?val=' + val)
    .then(function() { console.log('[FPS] Max set to:', val); })
    .catch(function(err) { console.error('[ERR] FPS failed:', err); })
    .finally(function() { if (select) select.disabled = false; });
}

function setDisplaySize(val) {
  const img = document.getElementById('stream');
  if (img) img.style.maxHeight = val;
  localStorage.setItem('esp32_display_size', val);
}

// ---- status ----
function refreshStatus() {
  const done = btnFeedback('stat-btn', 'loading');
  
  fetch(CAM + '/status')
    .then(function(r) { return r.json(); })
    .then(function(d) {
      const heapEl = document.getElementById('heap');
      const psramEl = document.getElementById('psram');
      const ipEl = document.getElementById('cam-ip');
      
      if (heapEl) heapEl.textContent = Math.round(d.free_heap / 1024) + ' KB';
      if (psramEl) psramEl.textContent = d.psram ? 'yes' : 'no';
      if (ipEl) ipEl.textContent = CAM;
      
      done();
      btnFeedback('stat-btn', 'success');
    })
    .catch(function(err) {
      console.error('[ERR] Status fetch failed:', err);
      done();
    });

  // check recording status
  fetch(CAM + '/record')
    .then(function(r) { return r.json(); })
    .then(function(d) { updateRecordingUI(d); })
    .catch(function() {});
}

// ---- SD ----
function deleteFile(name) {
  if (!confirm('Delete ' + name + '?')) return;
  fetch(CAM + '/delete?file=' + name)
    .then(function() { refreshSD(); })
    .catch(function(err) { console.error('[ERR] Delete failed:', err); });
}

function refreshSD() {
  fetch(CAM + '/sdinfo')
    .then(function(r) { return r.json(); })
    .then(function(d) {
      const statusEl = document.getElementById('sd-status');
      const totalEl = document.getElementById('sd-total');
      const usedEl = document.getElementById('sd-used');
      
      if (statusEl) statusEl.textContent = d.mounted ? 'mounted' : 'not found';
      if (totalEl) totalEl.textContent = d.mounted ? d.total_mb + ' MB' : '—';
      if (usedEl) usedEl.textContent = d.mounted ? d.used_mb + ' MB' : '—';
    })
    .catch(function(err) { console.error('[ERR] SD Info failed:', err); });

  fetch(CAM + '/sdfiles')
    .then(function(r) { return r.json(); })
    .then(function(d) {
      const list = document.getElementById('file-list');
      if (!list) return;
      
      if (!d.files || d.files.length === 0) {
        list.innerHTML = '<div class="file-item">no files</div>';
      } else {
        list.innerHTML = d.files.reverse().map(function(f) {
          const name = f.startsWith('/') ? f.substring(1) : f;
          return '<div class="file-item">' +
                 '<span>' + name + '</span>' +
                 '<div class="file-actions">' +
                 '<a href="' + CAM + '/download?file=' + name + '" target="_blank">DL</a>' +
                 '<span class="del" onclick="deleteFile(\'' + name + '\')">X</span>' +
                 '</div></div>';
        }).join('');
      }
    })
    .catch(function(err) {
       console.error('[ERR] SD Files failed:', err);
       const list = document.getElementById('file-list');
       if (list) list.innerHTML = '<div class="file-item" style="color:#f44336">Error loading files</div>';
    });
}

// ---- init ----
(function() {
  const savedSize = localStorage.getItem('esp32_display_size');
  if (savedSize) {
    const img = document.getElementById('stream');
    if (img) img.style.maxHeight = savedSize;
    const select = document.getElementById('size-select');
    if (select) select.value = savedSize;
  }

  stopStream();
  refreshStatus();
  refreshSD();

  setInterval(refreshStatus, 10000);

  setInterval(function() {
    if (recording) {
      fetch(CAM + '/record')
        .then(function(r) { return r.json(); })
        .then(function(d) { updateRecordingUI(d); })
        .catch(function() {});
    }
  }, 2000);
})();

</script>
</body>
</html>

)rawliteral";

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

// ESP32-CAM ONVIF Web Interface - JavaScript
// Main Application Logic - Enhanced Version with Advanced Features

const el = i => document.getElementById(i);

// Optimized API helper with timeout and abort controller
const api = async (ep, opts = {}, timeoutMs = 10000) => {
    const controller = new AbortController();
    const timeout = setTimeout(() => controller.abort(), timeoutMs);

    try {
        const r = await fetch(ep, { ...opts, signal: controller.signal });
        clearTimeout(timeout);

        if (!r.ok) {
            throw new Error(`HTTP ${r.status}: ${r.statusText}`);
        }

        const contentType = r.headers.get('content-type');
        if (contentType && contentType.includes('application/json')) {
            return await r.json();
        }

        return await r.text();
    } catch (e) {
        clearTimeout(timeout);
        if (e.name === 'AbortError') {
            console.error('Request timeout:', ep);
            return { error: 'Request timeout' };
        }
        console.error('API error:', ep, e);
        return { error: e.message };
    }
};

// Enhanced toast system with types and stacking
function showToast(msg, type = 'info') {
    const container = el('toast-container');
    if (!container) {
        // Fallback to old toast if container doesn't exist
        const t = el('rec-toast');
        if (t) { t.innerText = msg; t.classList.add('show'); setTimeout(() => t.classList.remove('show'), 3000); }
        return;
    }
    const toast = document.createElement('div');
    toast.className = `toast-item toast-${type}`;
    const icons = { success: '✓', error: '✕', warning: '⚠', info: 'ℹ' };
    
    const iconSpan = document.createElement('span');
    iconSpan.style.cssText = 'font-size:1.1rem;min-width:16px;text-align:center';
    iconSpan.textContent = icons[type] || icons.info;
    
    const textSpan = document.createElement('span');
    textSpan.style.flex = '1';
    textSpan.textContent = msg;
    
    const progressDiv = document.createElement('div');
    progressDiv.className = 'toast-progress';
    
    toast.appendChild(iconSpan);
    toast.appendChild(textSpan);
    toast.appendChild(progressDiv);
    container.appendChild(toast);
    // Auto-remove after 3.2s
    setTimeout(() => {
        toast.classList.add('removing');
        setTimeout(() => toast.remove(), 250);
    }, 3200);
    // Max 4 toasts
    while (container.children.length > 4) container.firstChild.remove();
}

// ==================== TABS ====================
function setTab(id, event) {
    document.querySelectorAll('.panel').forEach(p => p.classList.remove('active'));
    document.querySelectorAll('.tab-btn').forEach(b => b.classList.remove('active'));
    el('tab-' + id).classList.add('active');
    if (event && event.target) {
        event.target.classList.add('active');
    }
    if (id === 'net') { updateWifi(); updateBt(); }
    if (id === 'sys') { updateSystemInfo(); updateSDInfo(); checkForUpdates(); }
    if (id === 'events') updateEventLog();
    if (id === 'recordings') loadRecordings();
}

// ==================== STREAM PERFORMANCE = MONITOR ====================
let streamStats = {
    frames: [],
    bytesReceived: 0,
    lastFrameTime: 0,
    droppedFrames: 0
};

function updateStreamPerformance() {
    const now = Date.now();
    streamStats.frames.push(now);

    // Keep only last 30 frames for FPS calculation
    if (streamStats.frames.length > 30) {
        streamStats.frames.shift();
    }

    // Calculate FPS
    if (streamStats.frames.length >= 2) {
        const timeSpan = (streamStats.frames[streamStats.frames.length - 1] - streamStats.frames[0]) / 1000;
        const fps = streamStats.frames.length / timeSpan;

        if (el('perf-fps')) {
            el('perf-fps').innerText = fps.toFixed(1) + ' FPS';
        }
    }

    // Detect dropped frames
    if (streamStats.lastFrameTime > 0) {
        const gap = now - streamStats.lastFrameTime;
        if (gap > 2000) { // More than 2 second gap
            streamStats.droppedFrames++;
            if (el('perf-dropped')) {
                el('perf-dropped').innerText = streamStats.droppedFrames + ' frames';
            }
        }
    }

    streamStats.lastFrameTime = now;
}

// ==================== CAMERA ====================
function toggleStream() {
    const img = el('stream');
    if (img.src.includes('/stream')) {
        img.src = ""; // Stop stream
        img.alt = "Paused";
        el('status-text').innerText = "Paused";
    } else {
        img.src = "/stream?t=" + Date.now(); // Start with cache bust
        img.alt = "Connecting...";
        el('status-text').innerText = "Connecting...";
    }
}

function snap() {
    const a = document.createElement('a');
    a.href = '/snapshot';
    a.download = `snap_${Date.now()}.jpg`;
    a.click();
}

// Monitor stream for performance
const streamImg = el('stream');
if (streamImg) {
    streamImg.addEventListener('load', updateStreamPerformance);
}

// ==================== RECORDING ====================
let isRecording = false;
let mediaRecorder;
let recordedChunks = [];
let recCanvas, recCtx, recLoop;

async function toggleRecord() {
    const mode = el('rec-mode').value;
    const btn = el('btn-record');

    if (!isRecording) {
        // Start
        if (mode === 'device') {
            startClientRecord();
            showToast("Recording to device", 'success');
        } else {
            // SD Card
            await api('/api/record', { method: 'POST', body: JSON.stringify({ action: 'start' }) });
            showToast("Recording to SD Card", 'success');
        }
        isRecording = true;
        btn.innerHTML = '⬛'; // Stop icon
        btn.classList.add('pulse');
        // Add recording visual state to video container
        const vcont = el('vcont');
        if (vcont) vcont.classList.add('recording');
    } else {
        // Stop
        if (mode === 'device') {
            stopClientRecord();
        } else {
            await api('/api/record', { method: 'POST', body: JSON.stringify({ action: 'stop' }) });
            showToast("Recording stopped", 'info');
        }
        isRecording = false;
        btn.innerHTML = '🔴';
        btn.classList.remove('pulse');
        // Remove recording visual state
        const vcont = el('vcont');
        if (vcont) vcont.classList.remove('recording');
    }
}

function startClientRecord() {
    const img = el('stream');

    // Dynamic Canvas Sizing
    let w = img.naturalWidth;
    let h = img.naturalHeight;

    if (w === 0 || h === 0) {
        console.warn("Stream not fully loaded, using default 640x480");
        w = 640;
        h = 480;
    }

    recCanvas = document.createElement('canvas');
    recCanvas.width = w;
    recCanvas.height = h;
    recCtx = recCanvas.getContext('2d');

    console.log(`Starting Local Rec: ${w}x${h}`);

    const draw = () => {
        if (!isRecording) return;
        if (img.complete && img.naturalWidth > 0) {
            recCtx.drawImage(img, 0, 0, w, h);
        }
        requestAnimationFrame(draw);
    };
    draw();

    const stream = recCanvas.captureStream(20); // 20 FPS

    // Prioritize MP4 -> WebM -> VP9
    let mime = 'video/webm';
    let ext = 'webm';

    if (MediaRecorder.isTypeSupported('video/mp4')) {
        mime = 'video/mp4';
        ext = 'mp4';
    } else if (MediaRecorder.isTypeSupported('video/webm;codecs=vp9')) {
        mime = 'video/webm;codecs=vp9';
    }

    console.log("Recording using:", mime);

    try {
        mediaRecorder = new MediaRecorder(stream, { mimeType: mime });
    } catch (e) {
        console.error("MediaRecorder fail, trying default:", e);
        mediaRecorder = new MediaRecorder(stream);
        ext = 'webm';
    }

    recordedChunks = [];
    mediaRecorder.ondataavailable = e => {
        if (e.data && e.data.size > 0) {
            console.log('Chunk received:', e.data.size, 'bytes');
            recordedChunks.push(e.data);
        }
    };

    mediaRecorder.onstop = () => {
        console.log('Recording stopped. Total chunks:', recordedChunks.length);

        if (recordedChunks.length === 0) {
            showToast('Recording failed - no data captured');
            return;
        }

        const blob = new Blob(recordedChunks, { type: mime });
        console.log('Created blob:', blob.size, 'bytes');

        if (blob.size === 0) {
            showToast('Recording failed - empty file');
            return;
        }

        const url = URL.createObjectURL(blob);
        const a = document.createElement('a');
        a.href = url;
        a.download = `rec_${Date.now()}.${ext}`;
        a.click();
        URL.revokeObjectURL(url);
        showToast(`Saved ${(blob.size / 1024 / 1024).toFixed(2)} MB as .${ext.toUpperCase()}`);
    };

    mediaRecorder.onerror = (e) => {
        console.error('MediaRecorder error:', e);
        showToast('Recording error occurred');
    };

    // Start recording with timeslice to ensure data is collected
    mediaRecorder.start(100); // Request data every 100ms
    console.log('MediaRecorder started with state:', mediaRecorder.state);
}

function stopClientRecord() {
    if (mediaRecorder && mediaRecorder.state !== 'inactive') {
        console.log('Stopping MediaRecorder, state:', mediaRecorder.state);
        mediaRecorder.stop();

        // Stop all tracks in the stream
        const stream = recCanvas.captureStream();
        stream.getTracks().forEach(track => track.stop());
    }
}

// ==================== FLASH ====================
let flash = false;
function toggleFlash() {
    flash = !flash;
    el('btn-flash').style.color = flash ? '#fbbf24' : 'white';
    api('/api/flash', { method: 'POST', body: JSON.stringify({ state: flash }) });
}

// ==================== FULLSCREEN ====================
function toggleFS() {
    const c = el('vcont');
    if (!document.fullscreenElement) {
        c.requestFullscreen().catch(e => console.log(e));
        c.classList.add('fullscreen');
    } else {
        document.exitFullscreen();
        c.classList.remove('fullscreen');
    }
}
document.addEventListener('fullscreenchange', () => {
    if (!document.fullscreenElement) el('vcont').classList.remove('fullscreen');
});

// ==================== CONFIG ====================
function cfg(k, v) { api('/api/config', { method: 'POST', body: JSON.stringify({ [k]: v }) }); }

// ==================== WIFI SIGNAL STRENGTH ====================
function getSignalBars(rssi) {
    if (rssi > -50) return { bars: '▂▄▆█', color: '#10b981', text: 'Excellent' };
    if (rssi > -60) return { bars: '▂▄▆▁', color: '#10b981', text: 'Good' };
    if (rssi > -70) return { bars: '▂▄▁▁', color: '#fbbf24', text: 'Fair' };
    if (rssi > -80) return { bars: '▂▁▁▁', color: '#ef4444', text: 'Poor' };
    return { bars: '▁▁▁▁', color: '#ef4444', text: 'Very Poor' };
}

// ==================== CAMERA PRESETS ====================
async function applyPreset(preset) {
    await api('/api/camera/preset', { method: 'POST', body: JSON.stringify({ preset }) });
    showToast(`Applied ${preset} Quality Preset`);
    setTimeout(() => updateSystemInfo(), 500); // Refresh UI
}

// ==================== CAMERA PROFILES ====================
async function loadProfilesList() {
    const d = await api('/api/profiles');
    if (d && d.profiles && el('profile-list')) {
        const select = el('profile-list');
        select.innerHTML = '<option value="">Select Profile...</option>';
        d.profiles.forEach(p => {
            select.innerHTML += `<option value="${p}">${p}</option>`;
        });
    }
}

async function saveProfile() {
    const name = prompt("Enter profile name:");
    if (!name) return;

    const result = await api('/api/profiles/save', { method: 'POST', body: JSON.stringify({ name }) });
    if (result && result.ok) {
        showToast(`Profile "${name}" saved!`);
        loadProfilesList();
    } else {
        alert('Failed to save profile');
    }
}

async function loadProfile() {
    const select = el('profile-list');
    const name = select.value;
    if (!name) return;

    const result = await api('/api/profiles/load', { method: 'POST', body: JSON.stringify({ name }) });
    if (result && result.ok) {
        showToast(`Profile "${name}" loaded!`);
        setTimeout(() => updateSystemInfo(), 500);
    } else {
        alert('Failed to load profile');
    }
}

async function deleteProfile() {
    const select = el('profile-list');
    const name = select.value;
    if (!name) return;

    if (!confirm(`Delete profile "${name}"?`)) return;

    const result = await api('/api/profiles/delete', { method: 'DELETE', body: JSON.stringify({ name }) });
    if (result && result.ok) {
        showToast(`Profile "${name}" deleted!`);
        loadProfilesList();
    } else {
        alert('Failed to delete profile');
    }
}

// ==================== EVENT LOG ====================
async function updateEventLog() {
    const d = await api('/api/events');
    if (d && d.events && el('event-list')) {
        const list = el('event-list');
        list.innerHTML = '';

        d.events.reverse().forEach(e => {
            const timeStr = new Date(e.timestamp).toLocaleTimeString();
            const icon = e.type === 'boot' ? '🟢' :
                e.type === 'motion' ? '🚨' :
                    e.type === 'recording' ? '🎥' :
                        e.type === 'wifi' ? '📡' :
                            e.type === 'auth' ? '🔐' : '⚠️';

            const item = document.createElement('div');
            item.className = `event-item ${e.type}`;
            item.innerHTML = `
                <span class="event-time">${timeStr}</span>
                <span class="event-icon">${icon}</span>
                <span class="event-msg">${e.message}</span>
            `;
            list.appendChild(item);
        });
    }
}

async function clearEventLog() {
    if (!confirm('Clear all events?')) return;
    await api('/api/events', { method: 'DELETE' });
    updateEventLog();
    showToast('Event log cleared');
}

// ==================== RECORDINGS ====================
async function loadRecordings() {
    const d = await api('/api/recordings');
    if (d && d.recordings && el('recordings-list')) {
        const list = el('recordings-list');
        list.innerHTML = '';

        if (d.recordings.length === 0) {
            list.innerHTML = '<p style="color:var(--text-muted)">No recordings found</p>';
            return;
        }

        d.recordings.forEach(r => {
            const sizeMB = (r.size / 1024 / 1024).toFixed(2);
            const date = new Date(r.time * 1000).toLocaleString();

            const item = document.createElement('div');
            item.className = 'recording-item';
            item.innerHTML = `
                <div>
                    <b>${r.name}</b><br>
                    <small>${sizeMB} MB • ${date}</small>
                </div>
                <div>
                    <button class="btn" onclick="playRecording('${r.name}')">▶ Play</button>
                    <button class="btn" onclick="deleteRecording('${r.name}')">🗑 Delete</button>
                </div>
            `;
            list.appendChild(item);
        });
    }
}

function playRecording(filename) {
    const player = el('video-player');
    const modal = el('video-modal');
    player.src = `/api/recordings/stream?file=${encodeURIComponent(filename)}`;
    modal.style.display = 'flex';
}

function closeVideoPlayer() {
    const player = el('video-player');
    const modal = el('video-modal');
    player.pause();
    player.src = '';
    modal.style.display = 'none';
}

async function deleteRecording(filename) {
    if (!confirm(`Delete ${filename}?`)) return;

    const result = await api('/api/recordings/delete', { method: 'DELETE', body: JSON.stringify({ file: filename }) });
    if (result && result.ok) {
        showToast('Recording deleted');
        loadRecordings();
    } else {
        alert('Failed to delete recording');
    }
}

// ==================== NETWORK DIAGNOSTICS ====================
async function runPingTest() {
    const btn = el('btn-ping');
    btn.innerText = 'Testing...';

    const start = Date.now();
    const result = await api('/api/network/ping');
    const latency = Date.now() - start;

    btn.innerText = 'Run Ping Test';

    if (result && el('ping-result')) {
        el('ping-result').innerHTML = `
            <b>Latency:</b> ${latency}ms<br>
            <b>RSSI:</b> ${result.rssi} dBm<br>
            <b>Status:</b> ${latency < 50 ? '✅ Excellent' : latency < 100 ? '✅ Good' : '⚠️ Slow'}
        `;
    }
}

// ==================== QUICK ACTIONS ====================
function toggleQuickActions() {
    const panel = el('quick-actions');
    panel.classList.toggle('show');
}

function quickReboot() {
    if (!confirm('Reboot device?')) return;
    api('/reboot', { method: 'POST' }).then(() => {
        showToast('Rebooting...');
        setTimeout(() => location.reload(), 3000);
    });
}

function quickToggleONVIF() {
    const current = el('chk-onvif').checked;
    el('chk-onvif').checked = !current;
    api('/api/onvif/toggle', { method: 'POST', body: JSON.stringify({ enabled: !current }) });
    showToast(`ONVIF ${!current ? 'Enabled' : 'Disabled'}`);
}

// ==================== System Information ====================
async function updateSystemInfo() {
    loadIntegrations();
    const d = await api('/api/system/info');
    if (d) {
        if (el('info-resolution')) el('info-resolution').innerText = d.resolution;
        if (el('info-codec')) el('info-codec').innerText = d.codec;
        if (el('info-quality')) el('info-quality').innerText = d.quality;

        // Flash storage
        if (el('info-flash')) {
            const usedMB = (d.flash_used / 1024 / 1024).toFixed(1);
            const totalMB = (d.flash_total / 1024 / 1024).toFixed(1);
            el('info-flash').innerText = `${usedMB} / ${totalMB} MB`;
        }

        // PSRAM
        if (el('info-psram') && d.psram_total > 0) {
            const freeMB = (d.psram_free / 1024 / 1024).toFixed(1);
            const totalMB = (d.psram_total / 1024 / 1024).toFixed(1);
            el('info-psram').innerText = `${freeMB} / ${totalMB} MB Free`;
        } else if (el('info-psram')) {
            el('info-psram').innerText = 'N/A';
        }

        // Saturation control
        if (el('inp-saturation')) {
            el('inp-saturation').value = d.saturation;
            el('lbl-saturation').innerText = d.saturation;
        }
    }
}

// ==================== SD CARD INFO ====================
async function updateSDInfo() {
    const d = await api('/api/sd/info');
    if (d && d.total > 0) {
        const usedGB = (d.used / 1024 / 1024).toFixed(2);
        const totalGB = (d.total / 1024 / 1024).toFixed(2);
        const freeGB = (d.free / 1024 / 1024).toFixed(2);
        const usedPct = ((d.used / d.total) * 100).toFixed(1);

        if (el('sd-total')) el('sd-total').innerText = totalGB + ' GB';
        if (el('sd-used')) el('sd-used').innerText = `${usedGB} GB (${usedPct}%)`;
        if (el('sd-free')) el('sd-free').innerText = freeGB + ' GB';
        if (el('sd-files')) el('sd-files').innerText = d.file_count;

        // Progress bar
        if (el('sd-progress-bar')) {
            el('sd-progress-bar').style.width = usedPct + '%';
            el('sd-progress-bar').style.backgroundColor = usedPct > 90 ? '#ef4444' : usedPct > 70 ? '#fbbf24' : '#10b981';
        }
    }
}

// ==================== PTZ CONTROLS ====================
function ptzControl(action) {
    api('/api/ptz/control', { method: 'POST', body: JSON.stringify({ action }) });
}

function ptzMove(pan, tilt) {
    api('/api/ptz/control', { method: 'POST', body: JSON.stringify({ pan, tilt }) });
}

function ptzStep(axis, direction) {
    const stepSize = parseInt(el('ptz-step-size')?.value) || 5;
    const pan = axis === 'pan' ? direction * stepSize : 0;
    const tilt = axis === 'tilt' ? direction * stepSize : 0;
    ptzMove(pan, tilt);
}

function ptzHome() {
    ptzControl('home');
}

function togglePatrol() {
    const btn = el('btn-ptz-patrol');
    if (btn.innerText.includes('Start')) {
        btn.innerText = 'Stop Patrol Sweep';
        api('/api/ptz/control', { method: 'POST', body: JSON.stringify({ action: 'patrol', state: 'start' }) });
        showToast('PTZ patrol started');
    } else {
        btn.innerText = 'Start Patrol Sweep';
        api('/api/ptz/control', { method: 'POST', body: JSON.stringify({ action: 'patrol', state: 'stop' }) });
        showToast('PTZ patrol stopped');
    }
}

function togglePtzTracking(enabled) {
    api('/api/ptz/tracking', { method: 'POST', body: JSON.stringify({ enabled }) });
    showToast(enabled ? 'Human auto-tracking enabled' : 'Human auto-tracking disabled');
}

// ==================== SETTINGS EXPORT / IMPORT ====================
async function exportSettings() {
    const a = document.createElement('a');
    a.href = '/api/settings/export';
    a.download = `esp32cam-config-${Date.now()}.json`;
    a.click();
    showToast("Settings Exported");
}

async function importSettings() {
    const fileInput = el('import-file');
    if (!fileInput.files[0]) {
        alert('Please select a JSON file');
        return;
    }

    const reader = new FileReader();
    reader.onload = async (e) => {
        const json = e.target.result;
        const result = await api('/api/settings/import', {
            method: 'POST',
            body: json,
            headers: { 'Content-Type': 'application/json' }
        });

        if (result && result.ok) {
            showToast("Settings Imported Successfully");
            setTimeout(() => location.reload(), 1500);
        } else {
            alert('Import failed - invalid file');
        }
    };
    reader.readAsText(fileInput.files[0]);
}

// ==================== WIFI ====================
async function connectWifi(ssid) {
    const password = prompt(`Enter password for "${ssid}":`);
    if (password === null) return;
    
    showToast(`Connecting to ${ssid}...`);
    const result = await api('/api/wifi/connect', { 
        method: 'POST', 
        body: JSON.stringify({ ssid, password }) 
    });
    
    if (result && result.ok) {
        showToast(`Connected to ${ssid}!`);
        setTimeout(updateWifi, 2000);
    } else {
        showToast('Connection failed. Check password.');
    }
}

async function updateWifi() {
    const d = await api('/api/wifi/status');
    if (d) {
        el('wifi-ssid').innerText = d.ssid;
        el('wifi-ip').innerText = d.ip;
    }
}

async function scanWifi() {
    el('btn-scan').innerText = "Scanning...";
    const d = await api('/api/wifi/scan');
    el('btn-scan').innerText = "Scan Networks";
    if (d && d.networks) {
        el('wifi-list').innerHTML = d.networks.map(n => `
            <div class="wifi-item">
                <div><b>${n.ssid}</b> <span class="wifi-sig">${n.rssi}dBm</span></div>
                <button class="btn" style="padding:4px 10px; font-size:0.8rem" onclick="connectWifi('${n.ssid}')">Connect</button>
            </div>
        `).join('');
    }
}

// ==================== BT & AUDIO ====================
function cfgBt(obj) {
    api('/api/bt/config', { method: 'POST', body: JSON.stringify(obj) });
}

async function updateBt() {
    const d = await api('/api/bt/status');
    if (d) {
        if (el('bt-enable')) el('bt-enable').checked = d.enabled;
        if (el('bt-stealth')) el('bt-stealth').checked = d.stealth;
        if (el('bt-mac')) el('bt-mac').value = d.mac;
        if (el('audio-src')) el('audio-src').value = d.audioSource;
        if (el('inp-gain')) { el('inp-gain').value = d.gain; el('lbl-gain').innerText = d.gain + '%'; }
        if (el('inp-timeout')) { el('inp-timeout').value = d.timeout; el('lbl-timeout').innerText = d.timeout + 's'; }
    }
}

async function scanBt() {
    const btn = el('btn-scan-bt');
    btn.innerText = "Scanning...";
    const r = await api('/api/bt/scan');
    btn.innerText = "Scan";

    let list = r;
    if (typeof r === 'string') try { list = JSON.parse(r); } catch (e) { }

    if (Array.isArray(list)) {
        el('bt-scan-list').innerHTML = list.map(n => `
            <div class="wifi-item">
                <div style="font-family:monospace"><b>${n.mac}</b> <span class="wifi-sig">${n.rssi}dBm</span><br>${n.name || 'Unknown'}</div>
                <button class="btn" style="padding:4px 10px;" onclick="el('bt-mac').value='${n.mac}';cfgBt({mac:'${n.mac}'})">Select</button>
            </div>`).join('');
    } else {
        el('bt-scan-list').innerHTML = '<small style="color:var(--text-muted)">Click device to set as presence monitor</small>';
    }
}

// ==================== BLUETOOTH DEVICE CONNECTION ====================
async function scanBtDevices() {
    const btn = event.target;
    const list = el('bt-devices-list');

    btn.disabled = true;
    btn.innerText = '🔄 Scanning...';
    list.innerHTML = '<div style="text-align:center; padding:1rem; color:var(--text-muted)">Scanning for Bluetooth devices...</div>';

    try {
        const result = await api('/api/bt/scan');

        if (result && result.devices && result.devices.length > 0) {
            list.innerHTML = '';

            result.devices.forEach(device => {
                const item = document.createElement('div');
                item.className = 'bt-device-item';
                item.style.cssText = `
                    background: rgba(0,0,0,0.3);
                    padding: 0.75rem;
                    margin-bottom: 0.5rem;
                    border-radius: 6px;
                    border-left: 3px solid var(--primary);
                `;

                // Determine device type
                const deviceType = getDeviceType(device.name || 'Unknown');
                const icon = deviceType.icon;
                const badge = deviceType.badge;

                item.innerHTML = `
                    <div style="display: flex; justify-content: space-between; align-items: center;">
                        <div style="flex: 1;">
                            <div style="font-weight: 600; margin-bottom: 0.25rem;">
                                ${icon} ${device.name || 'Unknown Device'}
                                ${badge ? `<span style="background: var(--primary); padding: 2px 6px; border-radius: 3px; font-size: 0.7rem; margin-left: 0.5rem;">${badge}</span>` : ''}
                            </div>
                            <div style="font-size: 0.75rem; color: var(--text-muted);">
                                ${device.address}
                                ${device.rssi ? ` • Signal: ${device.rssi} dBm` : ''}
                            </div>
                        </div>
                        <div style="display: flex; gap: 0.5rem;">
                            ${device.connected ?
                        `<button class="btn" style="background: var(--danger); font-size: 0.85rem;" onclick="disconnectBtDevice('${device.address}')">Disconnect</button>` :
                        `<button class="btn btn-primary" style="font-size: 0.85rem;" onclick="connectBtDevice('${device.address}', '${device.name}')">Connect</button>`
                    }
                        </div>
                    </div>
                `;

                list.appendChild(item);
            });
        } else {
            list.innerHTML = '<div style="text-align:center; padding:1rem; color:var(--text-muted)">No devices found. Make sure device is in pairing mode.</div>';
        }
    } catch (err) {
        list.innerHTML = '<div style="text-align:center; padding:1rem; color:var(--danger)">Scan failed. Enable Bluetooth first.</div>';
    }

    btn.disabled = false;
    btn.innerText = '🔍 Scan for Devices';
}

function getDeviceType(name) {
    const nameLower = name.toLowerCase();

    if (nameLower.includes('headset') || nameLower.includes('airpod') || nameLower.includes('earbud')) {
        return { icon: '🎧', badge: 'HEADSET' };
    } else if (nameLower.includes('speaker') || nameLower.includes('sound')) {
        return { icon: '🔊', badge: 'SPEAKER' };
    } else if (nameLower.includes('mic') || nameLower.includes('microphone')) {
        return { icon: '🎤', badge: 'MIC' };
    } else if (nameLower.includes('phone') || nameLower.includes('mobile')) {
        return { icon: '📱', badge: 'PHONE' };
    }

    return { icon: '📱', badge: null };
}

async function connectBtDevice(address, name) {
    const btn = event.target;
    btn.disabled = true;
    btn.innerText = '⏳ Connecting...';

    try {
        const result = await api('/api/bt/connect', {
            method: 'POST',
            body: JSON.stringify({ address, name })
        });

        if (result && !result.error) {
            showToast(`Connected to ${name}`);
            playSound('success');
            updateBtStatus();
            // Refresh device list
            setTimeout(() => scanBtDevices(), 1000);
        } else {
            showToast('Connection failed: ' + (result.error || 'Unknown error'));
            btn.disabled = false;
            btn.innerText = 'Connect';
        }
    } catch (err) {
        showToast('Connection error');
        btn.disabled = false;
        btn.innerText = 'Connect';
    }
}

async function disconnectBtDevice(address) {
    if (!confirm('Disconnect this device?')) return;

    try {
        const result = await api('/api/bt/disconnect', {
            method: 'POST',
            body: JSON.stringify({ address })
        });

        if (result && !result.error) {
            showToast('Device disconnected');
            updateBtStatus();
            // Refresh device list
            setTimeout(() => scanBtDevices(), 500);
        } else {
            showToast('Disconnect failed');
        }
    } catch (err) {
        showToast('Disconnect error');
    }
}

async function updateBtStatus() {
    try {
        const status = await api('/api/bt/status');

        if (status) {
            el('bt-status').innerText = status.connected ? 'Connected' : 'Disconnected';
            el('bt-status').style.color = status.connected ? 'var(--success)' : 'var(--text-muted)';

            if (status.device && status.device.name) {
                el('bt-connected-name').innerText = status.device.name;
                el('bt-connected-name').style.color = 'var(--success)';
            } else {
                el('bt-connected-name').innerText = 'None';
                el('bt-connected-name').style.color = 'var(--text-muted)';
            }

            // Update audio source if BT connected
            if (status.connected && status.device) {
                el('audio-src').value = '2'; // Bluetooth HFP
            }
        }
    } catch (err) {
        console.error('Failed to update BT status:', err);
    }
}

// ==================== OTA ====================
function startOTA() {
    const f = el('ota-file').files[0];
    if (!f) return alert('Select file');
    const fd = new FormData(); fd.append("update", f);
    const xhr = new XMLHttpRequest();
    xhr.upload.onprogress = e => el('ota-bar').style.width = Math.round((e.loaded / e.total) * 100) + '%';
    xhr.onload = () => alert(xhr.status === 200 ? 'Success! Rebooting...' : 'Failed');
    xhr.open("POST", "/api/update"); xhr.send(fd);
}

// ==================== STATUS LOOP ====================
let resGraph = { ts: [], heap: [], psram: [] };
async function updateStatus() {
    const d = await api('/api/status');
    if (d) {
        el('status-pill').classList.remove('offline');
        el('status-text').innerText = "Online";
        if (el('val-uptime')) el('val-uptime').innerText = Math.floor(d.uptime / 60) + "m";
        if (el('val-heap')) el('val-heap').innerText = Math.round(d.heap / 1024) + "KB";

        // System monitor (Live Resource Monitor)
        const pad0 = n => n < 10 ? '0'+n : n;
        const hh = Math.floor(d.uptime / 3600);
        const mm = Math.floor((d.uptime % 3600) / 60);
        const ss = d.uptime % 60;
        if (el('sys-uptime')) el('sys-uptime').innerText = `${pad0(hh)}:${pad0(mm)}:${pad0(ss)}`;
        if (el('sys-heap')) el('sys-heap').innerText = (d.heap / 1024).toFixed(1) + " KB";
        if (el('sys-min-heap')) el('sys-min-heap').innerText = (d.min_heap / 1024).toFixed(1) + " KB";
        if (el('sys-psram')) el('sys-psram').innerText = (d.psram_free / 1024 / 1024).toFixed(2) + " MB";

        // Graphing
        const cvs = el('resource-graph');
        if (cvs) {
            if (resGraph.ts.length > 50) { resGraph.ts.shift(); resGraph.heap.shift(); resGraph.psram.shift(); }
            resGraph.ts.push(new Date());
            resGraph.heap.push(d.heap / 1024);
            resGraph.psram.push(d.psram_free / 1024 / 1024);

            const ctx = cvs.getContext('2d');
            cvs.width = cvs.clientWidth * window.devicePixelRatio;
            cvs.height = cvs.clientHeight * window.devicePixelRatio;
            ctx.scale(window.devicePixelRatio, window.devicePixelRatio);
            const w = cvs.clientWidth, h = cvs.clientHeight;

            ctx.clearRect(0, 0, w, h);

            if (resGraph.heap.length > 1) {
                const maxHeap = Math.max(...resGraph.heap) * 1.2 || 1;
                const maxPsram = Math.max(...resGraph.psram) * 1.2 || 1;
                const minHeap = 0;
                
                // Draw grid lines
                ctx.strokeStyle = 'rgba(255,255,255,0.05)';
                ctx.lineWidth = 1;
                ctx.beginPath();
                for (let i = 1; i < 4; i++) {
                    const y = h - (h / 4) * i;
                    ctx.moveTo(0, y); ctx.lineTo(w, y);
                }
                ctx.stroke();

                // Draw Heap Line
                ctx.beginPath();
                ctx.strokeStyle = '#3b82f6';
                ctx.lineWidth = 2;
                resGraph.heap.forEach((val, i) => {
                    const x = (i / 50) * w;
                    const y = h - ((val - minHeap) / (maxHeap - minHeap)) * h;
                    if (i === 0) ctx.moveTo(x, y);
                    else ctx.lineTo(x, y);
                });
                ctx.stroke();
                
                // Fill under Heap
                ctx.lineTo(w, h);
                ctx.lineTo(0, h);
                ctx.fillStyle = 'rgba(59, 130, 246, 0.1)';
                ctx.fill();

                // Draw PSRAM Line
                if (maxPsram > 0 && resGraph.psram.some(p => p > 0)) {
                    ctx.beginPath();
                    ctx.strokeStyle = '#10b981'; // Emerald/Accent color
                    ctx.lineWidth = 2;
                    resGraph.psram.forEach((val, i) => {
                        const x = (i / 50) * w;
                        const y = h - (val / maxPsram) * h;
                        if (i === 0) ctx.moveTo(x, y);
                        else ctx.lineTo(x, y);
                    });
                    ctx.stroke();
                }

                // Draw Legend
                ctx.font = '10px sans-serif';
                ctx.fillStyle = '#3b82f6';
                ctx.fillText('Heap', 10, 20);
                if (maxPsram > 0 && resGraph.psram.some(p => p > 0)) {
                    ctx.fillStyle = '#10b981';
                    ctx.fillText('PSRAM', 40, 20);
                }
            }
        }

        // WiFi Signal Strength
        if (el('val-rssi') && d.rssi !== undefined) {
            const signal = getSignalBars(d.rssi);
            el('val-rssi').innerHTML = `<span style="color:${signal.color}">${signal.bars}</span> ${d.rssi} dBm`;
        }

        // Motion Detection
        if (el('val-motion')) {
            el('val-motion').innerText = d.motion ? "Detected" : "None";
            el('val-motion').style.color = d.motion ? '#ef4444' : '#10b981';
        }

        if (el('chk-autoflash')) el('chk-autoflash').checked = d.autoflash;
        if (el('chk-onvif')) el('chk-onvif').checked = d.onvif_enabled;

        // Handle SD Mount Status
        const sdOpt = el('rec-mode')?.querySelector('option[value="sd"]');
        if (sdOpt) {
            if (!d.sd_mounted) {
                sdOpt.disabled = true;
                sdOpt.innerText = "SD Card (Not Found)";
                if (el('rec-mode').value === 'sd') el('rec-mode').value = 'device';
            } else {
                sdOpt.disabled = false;
                sdOpt.innerText = "SD Card (Server)";
            }
        }

        // Sync recording status if SD mode is active
        if (el('rec-mode') && el('rec-mode').value === 'sd') {
            el('sd-rec-status-row').style.display = 'flex';
            el('sd-status').innerText = d.recording ? "Recording..." : "Ready";
            if (d.recording && !isRecording) {
                isRecording = true;
                el('btn-record').innerHTML = '⬛';
                el('btn-record').classList.add('pulse');
            } else if (!d.recording && isRecording && el('rec-mode').value === 'sd') {
                isRecording = false;
                el('btn-record').innerHTML = '🔴';
                el('btn-record').classList.remove('pulse');
            }
        } else if (el('sd-rec-status-row')) {
            el('sd-rec-status-row').style.display = 'none';
        }
    } else {
        el('status-pill').classList.add('offline');
        el('status-text').innerText = "Offline";
    }
}

// Status update loop
setInterval(updateStatus, 2000);

// ==================== STREAM WATCHDOG ====================
streamImg.onerror = () => {
    console.log("Stream error/disconnect. Retrying...");
    el('status-text').innerText = "Reconnecting...";
    el('status-pill').classList.add('offline');
    setTimeout(() => {
        if (streamImg.src.includes('/stream')) {
            streamImg.src = '/stream?t=' + Date.now();
        }
    }, 2000);
    // Show offline overlay
    const overlay = el('stream-offline-overlay');
    if (overlay) overlay.classList.add('visible');
};

streamImg.onload = () => {
    el('status-pill').classList.remove('offline');
    el('status-text').innerText = "Online";
    // Hide offline overlay
    const overlay = el('stream-offline-overlay');
    if (overlay) overlay.classList.remove('visible');
}

// ==================== CLEANUP & OPTIMIZATION ====================
function cleanup() {
    // Stop recording if active
    if (isRecording && mediaRecorder) {
        stopClientRecord();
    }

    // Stop object detection
    if (objectDetection.isEnabled) {
        objectDetection.stopDetection();
    }

    // Stop time-lapse
    if (timeLapse.isRecording) {
        timeLapse.stop();
    }

    // Clear all intervals
    const highestId = setInterval(() => { }, 0);
    for (let i = 0; i < highestId; i++) {
        clearInterval(i);
    }

    console.log('Cleanup completed');
}

// Add cleanup on page unload
window.addEventListener('beforeunload', cleanup);

// ==================== INIT ====================
window.onload = () => {
    // Restore theme preference
    const savedTheme = localStorage.getItem('esp32cam_theme');
    if (savedTheme === 'light') document.body.classList.add('light-theme');

    el('stream').src = '/stream?t=' + Date.now();
    updateStatus();
    updateWifi();
    updateSystemInfo();
    loadProfilesList();
    initClientSideFeatures(); // Initialize all browser features
}

// ==================== CLIENT-SIDE FEATURES ====================

// ==================== 1. SNAPSHOT GALLERY ====================
const snapshotGallery = {
    maxSnapshots: 50,

    init() {
        if (!localStorage.getItem('esp32cam_snapshots')) {
            localStorage.setItem('esp32cam_snapshots', JSON.stringify([]));
        }
    },

    saveSnapshot() {
        const img = el('stream');
        const canvas = document.createElement('canvas');
        canvas.width = img.naturalWidth || 640;
        canvas.height = img.naturalHeight || 480;
        const ctx = canvas.getContext('2d');
        ctx.drawImage(img, 0, 0);

        const dataURL = canvas.toDataURL('image/jpeg', 0.9);
        const snapshots = this.getAll();

        snapshots.push({
            id: 'snap_' + Date.now(),
            timestamp: Date.now(),
            dataURL: dataURL,
            resolution: `${canvas.width}x${canvas.height}`
        });

        // Keep only last N snapshots
        if (snapshots.length > this.maxSnapshots) {
            snapshots.shift();
        }

        localStorage.setItem('esp32cam_snapshots', JSON.stringify(snapshots));
        showToast('Snapshot saved to gallery');
        playSound('snapshot');
    },

    getAll() {
        return JSON.parse(localStorage.getItem('esp32cam_snapshots') || '[]');
    },

    delete(id) {
        const snapshots = this.getAll().filter(s => s.id !== id);
        localStorage.setItem('esp32cam_snapshots', JSON.stringify(snapshots));
    },

    clear() {
        localStorage.setItem('esp32cam_snapshots', JSON.stringify([]));
    }
};

function openGallery() {
    const modal = el('gallery-modal');
    const grid = el('gallery-grid');
    const snapshots = snapshotGallery.getAll();

    grid.innerHTML = '';

    if (snapshots.length === 0) {
        grid.innerHTML = '<p style="color:var(--text-muted); grid-column: 1/-1; text-align:center">No snapshots yet. Press S to capture!</p>';
    } else {
        snapshots.reverse().forEach(snap => {
            const item = document.createElement('div');
            item.className = 'gallery-item';
            item.innerHTML = `
                <img src="${snap.dataURL}" alt="Snapshot">
                <div class="gallery-item-actions">
                    <button class="btn-mini" onclick="viewSnapshot('${snap.id}')">View</button>
                    <button class="btn-mini" onclick="downloadSnapshot('${snap.id}')">Download</button>
                    <button class="btn-mini" onclick="deleteSnapshot('${snap.id}')">Delete</button>
                </div>
                <small>${new Date(snap.timestamp).toLocaleString()}</small>
            `;
            grid.appendChild(item);
        });
    }

    modal.style.display = 'flex';
}

function closeGallery() {
    el('gallery-modal').style.display = 'none';
}

function viewSnapshot(id) {
    const snap = snapshotGallery.getAll().find(s => s.id === id);
    if (snap) {
        const viewer = el('snapshot-viewer');
        el('snapshot-view-img').src = snap.dataURL;
        viewer.style.display = 'flex';
    }
}

function closeSnapshotViewer() {
    el('snapshot-viewer').style.display = 'none';
}

function downloadSnapshot(id) {
    const snap = snapshotGallery.getAll().find(s => s.id === id);
    if (snap) {
        const a = document.createElement('a');
        a.href = snap.dataURL;
        a.download = `snapshot_${snap.id}.jpg`;
        a.click();
    }
}

function deleteSnapshot(id) {
    if (confirm('Delete this snapshot?')) {
        snapshotGallery.delete(id);
        openGallery(); // Refresh
    }
}

function clearAllSnapshots() {
    if (confirm('Delete all snapshots?')) {
        snapshotGallery.clear();
        closeGallery();
        showToast('All snapshots deleted');
    }
}

// ==================== 2. KEYBOARD SHORTCUTS ====================
const keyboardShortcuts = {
    enabled: true,

    init() {
        document.addEventListener('keydown', (e) => {
            if (!this.enabled) return;

            // Ignore if typing in input
            if (e.target.tagName === 'INPUT' || e.target.tagName === 'TEXTAREA') return;

            // Ignore if modifier keys are pressed (Ctrl, Alt, Meta)
            if (e.ctrlKey || e.altKey || e.metaKey) return;

            const key = e.key.toLowerCase();

            switch (key) {
                case ' ':
                    e.preventDefault();
                    toggleStream();
                    break;
                case 's':
                    e.preventDefault();
                    snapshotGallery.saveSnapshot();
                    break;
                case 'r':
                    e.preventDefault();
                    toggleRecord();
                    break;
                case 'f':
                    e.preventDefault();
                    toggleFlash();
                    break;
                case 'k':
                    e.preventDefault();
                    toggleFS();
                    break;
                case 'g':
                    e.preventDefault();
                    openGallery();
                    break;
                case 'q':
                    e.preventDefault();
                    toggleQuickActions();
                    break;
                case 'p':
                    e.preventDefault();
                    togglePiP();
                    break;
                case 'm':
                    e.preventDefault();
                    toggleKioskMode();
                    break;
                case 'd':
                    e.preventDefault();
                    toggleDetection();
                    break;
                case '?':
                    e.preventDefault();
                    showShortcutsHelp();
                    break;
                case 'escape':
                    closeAllModals();
                    break;
                case '1': setTabByIndex(0); break;
                case '2': setTabByIndex(1); break;
                case '3': setTabByIndex(2); break;
                case '4': setTabByIndex(3); break;
                case '5': setTabByIndex(4); break;
                case '6': setTabByIndex(5); break;
            }
        });
    }
};

function setTabByIndex(index) {
    const tabs = ['dash', 'cam', 'net', 'events', 'recordings', 'sys'];
    if (tabs[index]) {
        document.querySelectorAll('.tab-btn')[index]?.click();
    }
}

function closeAllModals() {
    el('gallery-modal').style.display = 'none';
    el('snapshot-viewer').style.display = 'none';
    el('video-modal').style.display = 'none';
    el('shortcuts-modal').style.display = 'none';
    el('comparison-modal').style.display = 'none';
}

function showShortcutsHelp() {
    el('shortcuts-modal').style.display = 'flex';
}

function closeShortcutsHelp() {
    el('shortcuts-modal').style.display = 'none';
}

// ==================== 3. PICTURE-IN-PICTURE ====================
async function togglePiP() {
    const video = el('stream');

    try {
        if (document.pictureInPictureElement) {
            await document.exitPictureInPicture();
            showToast('PiP disabled');
        } else {
            // Create video element from image stream
            const videoEl = document.createElement('video');
            videoEl.src = video.src;
            videoEl.autoplay = true;
            videoEl.muted = true;

            await videoEl.requestPictureInPicture();
            showToast('PiP enabled');
        }
    } catch (err) {
        console.error('PiP failed:', err);
        showToast('PiP not supported');
    }
}

// ==================== 4. VIDEO FILTERS ====================
const videoFilters = {
    brightness: 100,
    contrast: 100,
    saturation: 100,
    hue: 0,
    blur: 0,
    grayscale: 0,
    sepia: 0,
    invert: 0,

    init() {
        const saved = localStorage.getItem('esp32cam_filters');
        if (saved) {
            Object.assign(this, JSON.parse(saved));
        }
        this.apply();
    },

    apply() {
        const img = el('stream');
        img.style.filter = `
            brightness(${this.brightness}%)
            contrast(${this.contrast}%)
            saturate(${this.saturation}%)
            hue-rotate(${this.hue}deg)
            blur(${this.blur}px)
            grayscale(${this.grayscale}%)
            sepia(${this.sepia}%)
            invert(${this.invert}%)
        `;
        localStorage.setItem('esp32cam_filters', JSON.stringify(this));
    },

    reset() {
        this.brightness = 100;
        this.contrast = 100;
        this.saturation = 100;
        this.hue = 0;
        this.blur = 0;
        this.grayscale = 0;
        this.sepia = 0;
        this.invert = 0;
        this.apply();
    }
};

function updateFilter(filter, value) {
    videoFilters[filter] = value;
    videoFilters.apply();
    el('lbl-filter-' + filter).innerText = value + (filter === 'hue' ? '°' : '%');
}

function resetFilters() {
    videoFilters.reset();
    // Update UI sliders
    Object.keys(videoFilters).forEach(filter => {
        const slider = el('filter-' + filter);
        if (slider) {
            slider.value = videoFilters[filter];
            el('lbl-filter-' + filter).innerText = videoFilters[filter] + (filter === 'hue' ? '°' : '%');
        }
    });
}

// ==================== 5. FULLSCREEN KIOSK MODE ====================
let kioskMode = false;
let kioskTimeout;

function toggleKioskMode() {
    if (!kioskMode) {
        enterKioskMode();
    } else {
        exitKioskMode();
    }
}

function enterKioskMode() {
    document.documentElement.requestFullscreen();
    document.body.classList.add('kiosk-mode');
    kioskMode = true;

    // Auto-hide controls
    resetKioskTimeout();
    document.addEventListener('mousemove', resetKioskTimeout);

    showToast('Kiosk Mode - Press M or Esc to exit');
}

function exitKioskMode() {
    if (document.fullscreenElement) {
        document.exitFullscreen();
    }
    document.body.classList.remove('kiosk-mode');
    kioskMode = false;

    document.removeEventListener('mousemove', resetKioskTimeout);
    clearTimeout(kioskTimeout);
}

function resetKioskTimeout() {
    document.body.classList.remove('kiosk-hide-ui');
    clearTimeout(kioskTimeout);
    kioskTimeout = setTimeout(() => {
        document.body.classList.add('kiosk-hide-ui');
    }, 3000);
}

document.addEventListener('fullscreenchange', () => {
    if (!document.fullscreenElement && kioskMode) {
        exitKioskMode();
    }
});

// ==================== 6. COMPARISON SLIDER ====================
const comparisonTool = {
    beforeImage: null,
    afterImage: null,

    captureBefore() {
        const img = el('stream');
        const canvas = document.createElement('canvas');
        canvas.width = img.naturalWidth || 640;
        canvas.height = img.naturalHeight || 480;
        const ctx = canvas.getContext('2d');
        ctx.drawImage(img, 0, 0);
        this.beforeImage = canvas.toDataURL('image/jpeg', 0.9);
        showToast('Before image captured');
    },

    captureAfter() {
        const img = el('stream');
        const canvas = document.createElement('canvas');
        canvas.width = img.naturalWidth || 640;
        canvas.height = img.naturalHeight || 480;
        const ctx = canvas.getContext('2d');
        ctx.drawImage(img, 0, 0);
        this.afterImage = canvas.toDataURL('image/jpeg', 0.9);
        showToast('After image captured');
    },

    showComparison() {
        if (!this.beforeImage || !this.afterImage) {
            alert('Capture both before and after images first');
            return;
        }

        el('comparison-before').src = this.beforeImage;
        el('comparison-after').src = this.afterImage;
        el('comparison-modal').style.display = 'flex';
    }
};

function captureComparisonBefore() {
    comparisonTool.captureBefore();
}

function captureComparisonAfter() {
    comparisonTool.captureAfter();
}

function showComparison() {
    comparisonTool.showComparison();
}

function closeComparison() {
    el('comparison-modal').style.display = 'none';
}

function updateComparisonSlider(value) {
    el('comparison-after').style.clipPath = `inset(0 ${100 - value}% 0 0)`;
}

// ==================== 7. CONNECTION HISTORY TIMELINE ====================
const connectionHistory = {
    data: [],
    maxPoints: 43200, // 24 hours at 2s interval

    init() {
        const saved = localStorage.getItem('esp32cam_history');
        if (saved) {
            this.data = JSON.parse(saved);
            // Remove old data (>24h)
            const cutoff = Date.now() - (24 * 60 * 60 * 1000);
            this.data = this.data.filter(d => d.timestamp > cutoff);
        }
    },

    add(rssi, fps, online) {
        this.data.push({
            timestamp: Date.now(),
            rssi: rssi,
            fps: fps,
            online: online
        });

        // Remove old
        if (this.data.length > this.maxPoints) {
            this.data.shift();
        }

        localStorage.setItem('esp32cam_history', JSON.stringify(this.data));
    },

    drawGraph() {
        const canvas = el('timeline-canvas');
        if (!canvas) return;

        const ctx = canvas.getContext('2d');
        const width = canvas.width;
        const height = canvas.height;

        ctx.clearRect(0, 0, width, height);

        if (this.data.length < 2) return;

        // Draw RSSI graph
        ctx.strokeStyle = '#10b981';
        ctx.lineWidth = 2;
        ctx.beginPath();

        this.data.forEach((d, i) => {
            const x = (i / this.data.length) * width;
            const y = height - ((d.rssi + 100) / 50 * height); // -100 to -50 dBm

            if (i === 0) ctx.moveTo(x, y);
            else ctx.lineTo(x, y);
        });

        ctx.stroke();

        // Draw FPS graph
        ctx.strokeStyle = '#6366f1';
        ctx.lineWidth = 2;
        ctx.beginPath();

        this.data.forEach((d, i) => {
            const x = (i / this.data.length) * width;
            const y = height - (d.fps / 30 * height); // 0-30 FPS

            if (i === 0) ctx.moveTo(x, y);
            else ctx.lineTo(x, y);
        });

        ctx.stroke();
    }
};

// ==================== 8. SOUND NOTIFICATIONS ====================
const soundSystem = {
    enabled: true,
    volume: 0.5,

    init() {
        const prefs = JSON.parse(localStorage.getItem('esp32cam_preferences') || '{}');
        this.enabled = prefs.soundsEnabled !== false;
        this.volume = prefs.soundVolume || 0.5;
    },

    play(type) {
        if (!this.enabled) return;

        const audioCtx = new (window.AudioContext || window.webkitAudioContext)();
        const oscillator = audioCtx.createOscillator();
        const gainNode = audioCtx.createGain();

        oscillator.connect(gainNode);
        gainNode.connect(audioCtx.destination);

        gainNode.gain.value = this.volume;

        // Different frequencies for different events
        const frequencies = {
            snapshot: 800,
            recording: 600,
            motion: 1000,
            error: 400,
            success: 1200
        };

        oscillator.frequency.value = frequencies[type] || 800;
        oscillator.type = 'sine';

        oscillator.start();
        oscillator.stop(audioCtx.currentTime + 0.1);
    },

    save() {
        const prefs = JSON.parse(localStorage.getItem('esp32cam_preferences') || '{}');
        prefs.soundsEnabled = this.enabled;
        prefs.soundVolume = this.volume;
        localStorage.setItem('esp32cam_preferences', JSON.stringify(prefs));
    }
};

function playSound(type) {
    soundSystem.play(type);
}

function toggleSounds() {
    soundSystem.enabled = !soundSystem.enabled;
    soundSystem.save();
    showToast(soundSystem.enabled ? 'Sounds enabled' : 'Sounds disabled');
}

// ==================== 9. TIME-LAPSE CREATOR ====================
const timeLapse = {
    frames: [],
    isRecording: false,
    interval: null,
    intervalMs: 1000,

    start() {
        this.frames = [];
        this.isRecording = true;

        this.interval = setInterval(() => {
            this.captureFrame();
        }, this.intervalMs);

        showToast(`Time-lapse started (${this.intervalMs / 1000}s interval)`);
    },

    stop() {
        this.isRecording = false;
        clearInterval(this.interval);
        showToast(`Time-lapse stopped (${this.frames.length} frames)`);
    },

    captureFrame() {
        const img = el('stream');
        const canvas = document.createElement('canvas');
        canvas.width = img.naturalWidth || 640;
        canvas.height = img.naturalHeight || 480;
        const ctx = canvas.getContext('2d');
        ctx.drawImage(img, 0, 0);

        this.frames.push(canvas.toDataURL('image/jpeg', 0.8));

        if (el('timelapse-count')) {
            el('timelapse-count').innerText = this.frames.length + ' frames';
        }

        // Limit to prevent crash
        if (this.frames.length >= 100) {
            this.stop();
            showToast('Max frames reached (100)');
        }
    },

    export() {
        if (this.frames.length < 2) {
            alert('Need at least 2 frames');
            return;
        }

        showToast('Exporting time-lapse...');

        // Create video using MediaRecorder
        const canvas = document.createElement('canvas');
        canvas.width = 640;
        canvas.height = 480;
        const ctx = canvas.getContext('2d');
        const stream = canvas.captureStream(10); // 10 FPS

        const recorder = new MediaRecorder(stream);
        const chunks = [];

        recorder.ondataavailable = e => chunks.push(e.data);
        recorder.onstop = () => {
            const blob = new Blob(chunks, { type: 'video/webm' });
            const url = URL.createObjectURL(blob);
            const a = document.createElement('a');
            a.href = url;
            a.download = `timelapse_${Date.now()}.webm`;
            a.click();
            URL.revokeObjectURL(url);
            showToast('Time-lapse exported');
        };

        recorder.start();

        // Draw frames
        let frameIndex = 0;
        const drawInterval = setInterval(() => {
            if (frameIndex >= this.frames.length) {
                clearInterval(drawInterval);
                recorder.stop();
                return;
            }

            const img = new Image();
            img.onload = () => {
                ctx.drawImage(img, 0, 0, 640, 480);
            };
            img.src = this.frames[frameIndex];
            frameIndex++;
        }, 100); // 10 FPS
    }
};

// ==================== INITIALIZATION ====================
function initClientSideFeatures() {
    snapshotGallery.init();
    keyboardShortcuts.init();
    videoFilters.init();
    connectionHistory.init();
    soundSystem.init();

    // Update connection history every 2 seconds
    setInterval(() => {
        const rssi = parseInt(el('val-rssi')?.innerText) || -70;
        const fps = parseFloat(el('perf-fps')?.innerText) || 20;
        const online = !el('status-pill')?.classList.contains('offline');
        connectionHistory.add(rssi, fps, online);
    }, 2000);

    // Draw timeline graph every 5 seconds
    setInterval(() => {
        connectionHistory.drawGraph();
    }, 5000);

    console.log('Client-side features initialized');
}

// ==================== 10. OBJECT DETECTION (AI) ====================
const objectDetection = {
    model: null,
    isEnabled: false,
    isLoading: false,
    detectionCanvas: null,
    detectionCtx: null,
    detectionLoop: null,
    lastDetections: [],

    async init() {
        if (this.model) return; // Already loaded

        this.isLoading = true;
        showToast('Loading AI model...');

        try {
            // Load COCO-SSD model
            this.model = await cocoSsd.load();
            showToast('AI model loaded! Press D to enable detection');
            this.isLoading = false;

            // Create detection canvas overlay
            this.createOverlay();
        } catch (err) {
            console.error('Failed to load model:', err);
            showToast('AI model failed to load');
            this.isLoading = false;
        }
    },

    createOverlay() {
        const container = el('vcont');
        if (!this.detectionCanvas) {
            this.detectionCanvas = document.createElement('canvas');
            this.detectionCanvas.id = 'detection-canvas';
            this.detectionCanvas.className = 'detection-overlay';
            container.appendChild(this.detectionCanvas);
            this.detectionCtx = this.detectionCanvas.getContext('2d');
        }
    },

    async toggle() {
        if (!this.model) {
            await this.init();
            return;
        }

        this.isEnabled = !this.isEnabled;

        // Update button UI
        const btn = el('btn-detection');
        if (btn) {
            if (this.isEnabled) {
                btn.innerText = '🤖 Disable AI Detection';
                btn.style.background = '#ef4444'; // Red when active
            } else {
                btn.innerText = '🤖 Enable AI Detection';
                btn.style.background = ''; // Reset to primary
            }
        }

        if (this.isEnabled) {
            this.startDetection();
            showToast('Object detection ON');
            playSound('success');
        } else {
            this.stopDetection();
            showToast('Object detection OFF');
        }
    },

    startDetection() {
        const img = el('stream');
        const canvas = this.detectionCanvas;
        const ctx = this.detectionCtx;

        // Size canvas to match video
        const updateCanvasSize = () => {
            canvas.width = img.clientWidth;
            canvas.height = img.clientHeight;
        };
        updateCanvasSize();
        window.addEventListener('resize', updateCanvasSize);

        // Detection loop
        const detect = async () => {
            if (!this.isEnabled) return;

            try {
                // Detect objects
                const predictions = await this.model.detect(img);
                this.lastDetections = predictions;

                // Clear canvas
                ctx.clearRect(0, 0, canvas.width, canvas.height);

                // Draw detections
                predictions.forEach(prediction => {
                    const [x, y, width, height] = prediction.bbox;

                    // Scale coordinates to canvas size
                    const scaleX = canvas.width / img.naturalWidth;
                    const scaleY = canvas.height / img.naturalHeight;

                    const scaledX = x * scaleX;
                    const scaledY = y * scaleY;
                    const scaledWidth = width * scaleX;
                    const scaledHeight = height * scaleY;

                    // Enhanced bounding box with glow effect
                    const color = '#10b981'; // Emerald green
                    const cornerLength = 20; // Length of corner markers

                    // Draw shadow/glow effect
                    ctx.shadowColor = color;
                    ctx.shadowBlur = 10;
                    ctx.strokeStyle = color;
                    ctx.lineWidth = 4;

                    // Draw main rectangle
                    ctx.strokeRect(scaledX, scaledY, scaledWidth, scaledHeight);

                    // Draw corner markers (L-shapes at each corner)
                    ctx.lineWidth = 5;
                    ctx.shadowBlur = 15;

                    // Top-left corner
                    ctx.beginPath();
                    ctx.moveTo(scaledX, scaledY + cornerLength);
                    ctx.lineTo(scaledX, scaledY);
                    ctx.lineTo(scaledX + cornerLength, scaledY);
                    ctx.stroke();

                    // Top-right corner
                    ctx.beginPath();
                    ctx.moveTo(scaledX + scaledWidth - cornerLength, scaledY);
                    ctx.lineTo(scaledX + scaledWidth, scaledY);
                    ctx.lineTo(scaledX + scaledWidth, scaledY + cornerLength);
                    ctx.stroke();

                    // Bottom-left corner
                    ctx.beginPath();
                    ctx.moveTo(scaledX, scaledY + scaledHeight - cornerLength);
                    ctx.lineTo(scaledX, scaledY + scaledHeight);
                    ctx.lineTo(scaledX + cornerLength, scaledY + scaledHeight);
                    ctx.stroke();

                    // Bottom-right corner
                    ctx.beginPath();
                    ctx.moveTo(scaledX + scaledWidth - cornerLength, scaledY + scaledHeight);
                    ctx.lineTo(scaledX + scaledWidth, scaledY + scaledHeight);
                    ctx.lineTo(scaledX + scaledWidth, scaledY + scaledHeight - cornerLength);
                    ctx.stroke();

                    // Reset shadow for label
                    ctx.shadowBlur = 0;

                    // Draw semi-transparent fill
                    ctx.fillStyle = 'rgba(16, 185, 129, 0.1)';
                    ctx.fillRect(scaledX, scaledY, scaledWidth, scaledHeight);

                    // Draw label background
                    const label = `${prediction.class} ${(prediction.score * 100).toFixed(0)}%`;
                    ctx.font = 'bold 16px Inter, sans-serif';
                    const textWidth = ctx.measureText(label).width;

                    ctx.fillStyle = color;
                    ctx.fillRect(scaledX, scaledY - 30, textWidth + 16, 30);

                    // Draw label text
                    ctx.fillStyle = '#000';
                    ctx.fillText(label, scaledX + 8, scaledY - 9);
                });

                // Update detection count
                if (el('detection-count')) {
                    el('detection-count').innerText = predictions.length + ' objects';
                }

            } catch (err) {
                console.error('Detection error:', err);
            }

            // Continue loop
            this.detectionLoop = requestAnimationFrame(detect);
        };

        detect();
    },

    stopDetection() {
        if (this.detectionLoop) {
            cancelAnimationFrame(this.detectionLoop);
            this.detectionLoop = null;
        }

        if (this.detectionCtx && this.detectionCanvas) {
            this.detectionCtx.clearRect(0, 0, this.detectionCanvas.width, this.detectionCanvas.height);
        }

        if (el('detection-count')) {
            el('detection-count').innerText = '0 objects';
        }
    },

    getDetectedObjects() {
        return this.lastDetections.map(d => ({
            class: d.class,
            confidence: (d.score * 100).toFixed(1) + '%',
            bbox: d.bbox
        }));
    }
};

// Add to keyboard shortcuts
function toggleDetection() {
    objectDetection.toggle();
}

// ==================== AI VISION - GEMINI BYOK ====================
// All processing happens in-browser. Zero ESP32 overhead.

function saveApiKey() {
    const key = el('ai-api-key').value.trim();
    if (!key) { showToast('Please enter an API key'); return; }
    localStorage.setItem('gemini_api_key', key);
    showToast('API key saved securely in browser');
}

// Restore saved key on load
(function initAiVision() {
    const savedKey = localStorage.getItem('gemini_api_key');
    if (savedKey && el('ai-api-key')) {
        el('ai-api-key').value = savedKey;
    }
    renderAiHistory();
})();

let _aiAbort = null; // AbortController for cancelling requests

async function aiAnalyze(prompt) {
    if (!prompt || !prompt.trim()) {
        showToast('Please enter a prompt');
        return;
    }

    const apiKey = localStorage.getItem('gemini_api_key');
    if (!apiKey) {
        showToast('Please save your Gemini API key first');
        return;
    }

    const output = el('ai-output');
    const statusEl = el('ai-status');
    const tokensEl = el('ai-tokens');
    const btn = el('btn-ai-analyze');

    // Cancel any in-flight request
    if (_aiAbort) _aiAbort.abort();
    _aiAbort = new AbortController();

    // UI: loading state
    output.innerHTML = '<span class="ai-loading"></span> Capturing snapshot and analyzing...';
    statusEl.innerText = 'Analyzing...';
    tokensEl.innerText = '';
    btn.disabled = true;
    btn.innerText = 'Analyzing...';

    try {
        // 1. Capture current frame from stream
        const img = el('stream');
        let base64Data;

        if (img && img.src && img.src.includes('/stream') && img.naturalWidth > 0) {
            // Capture from live stream via canvas
            const canvas = document.createElement('canvas');
            canvas.width = img.naturalWidth;
            canvas.height = img.naturalHeight;
            canvas.getContext('2d').drawImage(img, 0, 0);
            base64Data = canvas.toDataURL('image/jpeg', 0.85).split(',')[1];
        } else {
            // Fallback: fetch snapshot directly
            output.innerHTML = '<span class="ai-loading"></span> Fetching snapshot...';
            const snapResp = await fetch('/snapshot');
            if (!snapResp.ok) throw new Error('Failed to capture snapshot');
            const blob = await snapResp.blob();
            base64Data = await new Promise((resolve) => {
                const reader = new FileReader();
                reader.onloadend = () => resolve(reader.result.split(',')[1]);
                reader.readAsDataURL(blob);
            });
        }

        // Show the captured image in the UI
        if (el('ai-img-preview-container') && el('ai-img-preview')) {
            el('ai-img-preview-container').style.display = 'flex';
            el('ai-img-preview').src = 'data:image/jpeg;base64,' + base64Data;
        }

        output.innerHTML = '<span class="ai-loading"></span> Sending to Gemini...';

        // 2. Call Gemini API with streaming (SSE)
        const response = await fetch(
            `https://generativelanguage.googleapis.com/v1beta/models/gemini-2.0-flash:streamGenerateContent?alt=sse&key=${apiKey}`,
            {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                signal: _aiAbort.signal,
                body: JSON.stringify({
                    contents: [{
                        parts: [
                            { text: prompt },
                            { inline_data: { mime_type: 'image/jpeg', data: base64Data } }
                        ]
                    }]
                })
            }
        );

        if (!response.ok) {
            const errBody = await response.text();
            let errMsg = `API Error ${response.status}`;
            try {
                const errJson = JSON.parse(errBody);
                errMsg = errJson.error?.message || errMsg;
            } catch (_) { }
            throw new Error(errMsg);
        }

        // 3. Stream the response
        output.innerHTML = '';
        let fullText = '';
        let totalTokens = 0;

        const reader = response.body.getReader();
        const decoder = new TextDecoder();
        let buffer = '';

        while (true) {
            const { done, value } = await reader.read();
            if (done) break;

            buffer += decoder.decode(value, { stream: true });

            // Parse SSE lines
            const lines = buffer.split('\n');
            buffer = lines.pop(); // Keep incomplete line in buffer

            for (const line of lines) {
                if (!line.startsWith('data: ')) continue;
                const jsonStr = line.slice(6).trim();
                if (!jsonStr || jsonStr === '[DONE]') continue;

                try {
                    const data = JSON.parse(jsonStr);

                    // Extract text from candidates
                    if (data.candidates && data.candidates[0]) {
                        const parts = data.candidates[0].content?.parts;
                        if (parts) {
                            for (const part of parts) {
                                if (part.text) {
                                    fullText += part.text;
                                }
                            }
                        }
                    }

                    // Extract token count
                    if (data.usageMetadata) {
                        totalTokens = data.usageMetadata.totalTokenCount || 0;
                    }
                } catch (parseErr) {
                    // Skip malformed JSON chunks
                }
            }

            // Render with basic markdown formatting
            output.innerHTML = renderMarkdown(fullText) + '<span class="ai-cursor"></span>';
            output.scrollTop = output.scrollHeight;
            statusEl.innerText = 'Streaming...';
        }

        // Final render (remove cursor)
        output.innerHTML = renderMarkdown(fullText);
        statusEl.innerText = 'Complete';
        if (totalTokens > 0) {
            tokensEl.innerText = totalTokens + ' tokens';
        }

        // Save to history
        saveAiHistory(prompt, fullText);

    } catch (err) {
        if (err.name === 'AbortError') {
            output.innerHTML = '<span style="color:var(--text-muted)">Analysis cancelled.</span>';
            statusEl.innerText = 'Cancelled';
        } else {
            output.innerHTML = `<span style="color:var(--danger)">Error: ${escapeHtml(err.message)}</span>`;
            statusEl.innerText = 'Error';
            console.error('AI Vision error:', err);
        }
    } finally {
        btn.disabled = false;
        btn.innerText = 'Analyze';
        _aiAbort = null;
    }
}

// Simple markdown renderer for AI output
function renderMarkdown(text) {
    let html = escapeHtml(text);
    // Bold: **text**
    html = html.replace(/\*\*(.+?)\*\*/g, '<strong>$1</strong>');
    // Italic: *text*
    html = html.replace(/(?<!\*)\*(?!\*)(.+?)(?<!\*)\*(?!\*)/g, '<em>$1</em>');
    // Inline code: `code`
    html = html.replace(/`([^`]+)`/g, '<code style="background:rgba(255,255,255,0.1);padding:1px 4px;border-radius:3px">$1</code>');
    // Numbered lists
    html = html.replace(/^(\d+)\.\s/gm, '<br>$1. ');
    // Bullet lists
    html = html.replace(/^[\-\*]\s/gm, '<br>• ');
    // Headers
    html = html.replace(/^### (.+)$/gm, '<br><strong style="font-size:1rem;color:var(--accent)">$1</strong>');
    html = html.replace(/^## (.+)$/gm, '<br><strong style="font-size:1.1rem;color:var(--primary)">$1</strong>');
    // Line breaks
    html = html.replace(/\n/g, '<br>');
    return html;
}

function escapeHtml(text) {
    const map = { '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#039;' };
    return text.replace(/[&<>"']/g, c => map[c]);
}

// History management (localStorage, max 10)
function saveAiHistory(prompt, response) {
    let history = JSON.parse(localStorage.getItem('ai_history') || '[]');
    history.unshift({
        prompt: prompt,
        response: response,
        time: new Date().toISOString()
    });
    if (history.length > 10) history = history.slice(0, 10);
    localStorage.setItem('ai_history', JSON.stringify(history));
    renderAiHistory();
}

function renderAiHistory() {
    const container = el('ai-history');
    if (!container) return;

    const history = JSON.parse(localStorage.getItem('ai_history') || '[]');

    if (history.length === 0) {
        container.innerHTML = '<span style="color:var(--text-muted)">No analyses yet</span>';
        return;
    }

    container.innerHTML = history.map((item, i) => {
        const time = new Date(item.time).toLocaleString();
        const preview = item.response.substring(0, 120) + (item.response.length > 120 ? '...' : '');
        return `
            <div class="ai-history-item" onclick="loadAiHistory(${i})">
                <div class="ai-h-prompt">${escapeHtml(item.prompt.substring(0, 60))}</div>
                <div class="ai-h-time">${time}</div>
                <div class="ai-h-preview">${escapeHtml(preview)}</div>
            </div>
        `;
    }).join('');
}

function loadAiHistory(index) {
    const history = JSON.parse(localStorage.getItem('ai_history') || '[]');
    if (history[index]) {
        el('ai-output').innerHTML = renderMarkdown(history[index].response);
        el('ai-status').innerText = 'Loaded from history';
        el('ai-custom-prompt').value = history[index].prompt;
    }
}

function clearAiHistory() {
    if (!confirm('Clear all AI analysis history?')) return;
    localStorage.removeItem('ai_history');
    renderAiHistory();
    showToast('AI history cleared');
}

// Auto-start stream on page load
window.addEventListener('DOMContentLoaded', () => {
    const img = el('stream');
    if (!img.src || img.src === window.location.href || img.getAttribute('src') === '') {
        toggleStream();
    }

    // Load custom theme
    const savedTheme = localStorage.getItem('ui-accent');
    if (savedTheme) {
        updateThemeColor(savedTheme);
    }
});

// ==================== IMPRESSIVE FEATURES ====================

// Custom Theme Handler
function updateThemeColor(color) {
    document.documentElement.style.setProperty('--primary', color);
    localStorage.setItem('ui-accent', color);
    const picker = el('theme-color-picker');
    if (picker && picker.value !== color) {
        picker.value = color; // sync picker UI reset
    }
}

// Double-click to fullscreen
window.addEventListener('DOMContentLoaded', () => {
    const wrapper = el('video-wrapper');
    if (wrapper) {
        wrapper.addEventListener('dblclick', toggleFS);
    }
});

// Quick Control APIs
async function toggleOption(opt) {
    // Determine the current checkbox state if available in the UI
    const checkbox = el(`camera-${opt}`);
    let newVal;
    if (checkbox) {
        checkbox.checked = !checkbox.checked;
        newVal = checkbox.checked ? 1 : 0;
    } else {
        // Blind toggle fallback
        newVal = 1;
    }

    const payload = {};
    payload[opt] = newVal;

    await api('/api/config', { method: 'POST', body: JSON.stringify(payload) });
    showToast(`${opt} toggled`);
}

async function applyFrameSize(val) {
    const resString = val === 10 ? 'HD' : 'QVGA'; // 10 is FRAMESIZE_HD, 5 is FRAMESIZE_QVGA
    await api('/api/config', { method: 'POST', body: JSON.stringify({ resolution: resString }) });
    showToast(`Resolution Set to ${resString}`);
}

// 2. Picture-in-Picture logic
let pipVideo = null;
let pipCanvas = null;
let pipCtx = null;
let pipInterval = null;

async function togglePiP() {
    if (document.pictureInPictureElement) {
        await document.exitPictureInPicture();
        return;
    }

    if (!('pictureInPictureEnabled' in document)) {
        showToast("PiP not supported in this browser 😢");
        return;
    }

    const img = el('stream');
    if (!img || img.naturalWidth === 0) {
        showToast("Stream must be playing first!");
        return;
    }

    if (!pipVideo) {
        pipCanvas = document.createElement('canvas');
        pipCtx = pipCanvas.getContext('2d');
        pipVideo = document.createElement('video');
        pipVideo.muted = true;
        pipVideo.autoplay = true;
        pipVideo.playsInline = true;

        // Listeners to clean up loop
        pipVideo.addEventListener('leavepictureinpicture', () => {
            clearInterval(pipInterval);
            pipVideo.pause();
        });
    }

    pipCanvas.width = img.naturalWidth || 640;
    pipCanvas.height = img.naturalHeight || 480;

    if ("captureStream" in pipCanvas) {
        pipVideo.srcObject = pipCanvas.captureStream(15);
    } else if ("mozCaptureStream" in pipCanvas) {
        pipVideo.srcObject = pipCanvas.mozCaptureStream(15);
    }

    // CRITICAL FIX: Start drawing loop for PiP before awaiting play()
    if (pipInterval) clearInterval(pipInterval);
    pipCtx.drawImage(img, 0, 0, pipCanvas.width, pipCanvas.height);

    pipInterval = setInterval(() => {
        if (img.complete && img.naturalWidth > 0) {
            pipCtx.drawImage(img, 0, 0, pipCanvas.width, pipCanvas.height);
        }
    }, 1000 / 15);

    try {
        await pipVideo.play();
        await pipVideo.requestPictureInPicture();
        showToast("Picture in Picture Started 🔲");
    } catch (e) {
        console.error("PiP Error:", e);
        showToast("Error starting PiP!");
        clearInterval(pipInterval);
    }
}

// 3. Digital PTZ (Pan/Tilt/Zoom)
let ptzZoom = 1;
let ptzPanX = 0;
let ptzPanY = 0;
let isDraggingPTZ = false;
let startX, startY;

window.addEventListener('DOMContentLoaded', () => {
    const wrapper = el('video-wrapper');
    const feed = el('stream');
    if (!wrapper || !feed) return;

    // Mouse wheel zoom
    wrapper.addEventListener('wheel', e => {
        e.preventDefault();
        ptzZoom += e.deltaY * -0.002;
        ptzZoom = Math.min(Math.max(1, ptzZoom), 6); // Clamp 1x to 6x
        applyPTZ();
    });

    // Panning logic
    wrapper.addEventListener('mousedown', e => {
        if (ptzZoom <= 1) return;
        isDraggingPTZ = true;
        startX = e.clientX - ptzPanX;
        startY = e.clientY - ptzPanY;
        feed.style.cursor = 'grabbing';
    });

    // Touch support (mobile pinch/pan)
    let initialPinchDistance = null;
    let initialZoom = 1;
    wrapper.addEventListener('touchstart', e => {
        if (e.touches.length === 2) {
            initialPinchDistance = Math.hypot(
                e.touches[0].clientX - e.touches[1].clientX,
                e.touches[0].clientY - e.touches[1].clientY
            );
            initialZoom = ptzZoom;
        } else if (e.touches.length === 1 && ptzZoom > 1) {
            isDraggingPTZ = true;
            startX = e.touches[0].clientX - ptzPanX;
            startY = e.touches[0].clientY - ptzPanY;
        }
    });

    wrapper.addEventListener('touchmove', e => {
        if (e.touches.length === 2 && initialPinchDistance) {
            e.preventDefault(); // Stop page scaling
            const dist = Math.hypot(
                e.touches[0].clientX - e.touches[1].clientX,
                e.touches[0].clientY - e.touches[1].clientY
            );
            const scale = dist / initialPinchDistance;
            ptzZoom = Math.min(Math.max(1, initialZoom * scale), 6);
            applyPTZ();
        } else if (e.touches.length === 1 && isDraggingPTZ) {
            e.preventDefault(); // Stop scroll
            ptzPanX = e.touches[0].clientX - startX;
            ptzPanY = e.touches[0].clientY - startY;
            applyPTZ();
        }
    }, { passive: false });

    window.addEventListener('touchend', () => {
        isDraggingPTZ = false;
        initialPinchDistance = null;
    });

    window.addEventListener('mouseup', () => {
        isDraggingPTZ = false;
        feed.style.cursor = 'grab';
    });

    window.addEventListener('mousemove', e => {
        if (!isDraggingPTZ || ptzZoom === 1) return;
        ptzPanX = e.clientX - startX;
        ptzPanY = e.clientY - startY;
        applyPTZ();
    });

    function applyPTZ() {
        if (ptzZoom === 1) {
            ptzPanX = 0; ptzPanY = 0;
            feed.style.transform = `scale(1) translate(0px, 0px)`;
            return;
        }

        // Calculate boundaries so we don't pan out of the image completely
        const rect = wrapper.getBoundingClientRect();
        const maxPanX = (rect.width * ptzZoom - rect.width) / 2 / ptzZoom;
        const maxPanY = (rect.height * ptzZoom - rect.height) / 2 / ptzZoom;

        ptzPanX = Math.min(Math.max(-maxPanX * 2, ptzPanX), maxPanX * 2);
        ptzPanY = Math.min(Math.max(-maxPanY * 2, ptzPanY), maxPanY * 2);

        // Uses transform-origin 0 0 from CSS, but visual center bounds might differ slightly based on object-fit
        // The most robust way visually is simply translating the center point.
        feed.style.transform = `scale(${ptzZoom}) translate(${ptzPanX / ptzZoom}px, ${ptzPanY / ptzZoom}px)`;
    }
});

// ==================== OTA FIRMWARE UPDATE ====================
function startOTA() {
    const fileInput = el('ota-file');
    if (!fileInput.files.length) {
        showToast("Please select a .bin file first");
        return;
    }

    const file = fileInput.files[0];
    if (!file.name.endsWith('.bin')) {
        showToast("Invalid file format. Must be .bin");
        return;
    }

    if (!confirm("Are you sure you want to update firmware? Do not power off the device!")) return;

    const fd = new FormData();
    fd.append("update", file, file.name);

    el('ota-progress-container').style.display = 'block';
    el('ota-status-text').style.display = 'block';
    el('ota-progress-bar').style.width = '0%';
    el('ota-progress-bar').innerText = '0%';
    el('ota-status-text').innerText = 'Uploading... Please do not power off.';

    const xhr = new XMLHttpRequest();
    xhr.open("POST", "/api/update", true);

    // The request needs authentication if we use authenticate() in backend,
    // but the backend uses basic auth. The browser will handle it or we need to pass it.
    // If user is already logged in, the browser sends the session cookies / auth header automatically.

    xhr.upload.addEventListener("progress", function (evt) {
        if (evt.lengthComputable) {
            const pct = Math.round((evt.loaded / evt.total) * 100);
            el('ota-progress-bar').style.width = pct + "%";
            el('ota-progress-bar').innerText = pct + "%";
        }
    });

    xhr.onload = function () {
        if (xhr.status === 200) {
            el('ota-progress-bar').style.backgroundColor = 'var(--success)';
            el('ota-status-text').innerText = 'Update Successful! Rebooting device...';
            showToast("Update Successful! Rebooting...");
            setTimeout(() => { location.reload(); }, 15000); // wait 15 seconds to reload
        } else {
            el('ota-progress-bar').style.backgroundColor = 'var(--danger)';
            el('ota-status-text').innerText = 'Update Failed!';
            showToast("Update Failed: " + xhr.responseText);
        }
    };

    xhr.onerror = function () {
        el('ota-progress-bar').style.backgroundColor = 'var(--danger)';
        el('ota-status-text').innerText = 'Network error during upload.';
        showToast("Network Error during update");
    };

    xhr.send(fd);
}

let githubDownloadUrl = "";

async function checkForUpdates() {
    const btn = el('btn-check-update');
    const info = el('github-update-info');
    const curVer = el('current-version');
    const latestVer = el('latest-version');

    btn.innerText = "Checking...";
    btn.disabled = true;

    const d = await api('/api/update/check');
    btn.disabled = false;
    btn.innerText = "Check for GitHub Updates";

    if (d && !d.error) {
        curVer.innerText = d.current_version;
        latestVer.innerText = d.latest_version;

        if (d.latest_version !== "" && d.latest_version !== d.current_version) {
            info.style.display = 'block';
            githubDownloadUrl = d.download_url;
        } else {
            info.style.display = 'none';
            showToast("You are already on the latest version.");
        }
    } else {
        showToast(d.error || "Failed to check for updates");
        curVer.innerText = "Error";
    }
}

async function installGithubUpdate() {
    if (!githubDownloadUrl) return;

    if (!confirm("Are you sure you want to install the latest firmware from GitHub? The device will reboot automatically.")) return;

    el('btn-install-update').disabled = true;
    el('btn-install-update').innerText = "Initiating Download...";

    el('ota-progress-container').style.display = 'block';
    el('ota-status-text').style.display = 'block';
    el('ota-progress-bar').style.width = '0%';
    el('ota-progress-bar').innerText = '0%';
    el('ota-status-text').innerText = "Starting background download...";

    // Initiate background task
    const d = await api('/api/update/github', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ url: githubDownloadUrl })
    });

    if (d && d.success) {
        // Start polling for status
        let pollInterval = setInterval(async () => {
            const status = await api('/api/update/status');
            if (status && status.progress !== undefined) {
                if (status.progress >= 0 && status.progress <= 100) {
                    el('ota-progress-bar').style.width = status.progress + '%';
                    el('ota-progress-bar').innerText = status.progress + '%';
                    el('ota-status-text').innerText = status.message || "Downloading...";

                    if (status.progress === 100) {
                        clearInterval(pollInterval);
                        el('ota-progress-bar').style.backgroundColor = 'var(--success)';
                        showToast("Update Successful! Rebooting...");
                        setTimeout(() => { location.reload(); }, 15000);
                    }
                } else if (status.progress < 0) { // Error
                    clearInterval(pollInterval);
                    el('btn-install-update').disabled = false;
                    el('btn-install-update').innerText = "Install OTA Update";
                    el('ota-progress-bar').style.backgroundColor = 'var(--danger)';
                    el('ota-status-text').innerText = "Update Error: " + (status.message || "Failed");
                    showToast("Update Error: " + (status.message || "Failed"));
                }
            } else {
                // If API fails to respond during reboot or heavy load, ignore temporarily
                console.warn("Status poll failed or delayed...");
            }
        }, 1000); // poll every 1 second
    } else {
        el('btn-install-update').disabled = false;
        el('btn-install-update').innerText = "Install OTA Update";
        el('ota-status-text').innerText = "Failed to start update: " + (d.error || "Unknown error");
        showToast(d.error || "Failed to start update");
    }
}

// ==================== INTEGRATIONS (MQTT & TELEGRAM) ====================
async function loadIntegrations() {
    const d = await api('/api/settings');
    if (d && !d.error) {
        if (el('inp-mqtt-en')) el('inp-mqtt-en').checked = d.mqttEnabled;
        if (el('inp-mqtt-broker')) el('inp-mqtt-broker').value = d.mqttBroker || '';
        if (el('inp-mqtt-port')) el('inp-mqtt-port').value = d.mqttPort || 1883;
        if (el('inp-mqtt-user')) el('inp-mqtt-user').value = d.mqttUser || '';
        if (el('inp-mqtt-pass')) el('inp-mqtt-pass').value = d.mqttPass || '';

        if (el('inp-tg-en')) el('inp-tg-en').checked = d.tgEnabled;
        if (el('inp-tg-token')) el('inp-tg-token').value = d.tgToken || '';
        if (el('inp-tg-chat')) el('inp-tg-chat').value = d.tgChatId || '';

        if (el('inp-gd-en')) el('inp-gd-en').checked = !!d.gdEnabled;
        if (el('inp-gd-motion')) el('inp-gd-motion').checked = d.gdMotion !== undefined ? !!d.gdMotion : true;
        if (el('inp-gd-url')) el('inp-gd-url').value = d.gdUrl || '';
        
        // Load local preferences for GDrive
        if (el('inp-gd-settings')) el('inp-gd-settings').checked = localStorage.getItem('esp32cam_gd_backup_settings') === '1';

        if (el('inp-webdav-en')) el('inp-webdav-en').checked = !!d.webDav;
        if (el('inp-cont-rec')) el('inp-cont-rec').checked = !!d.contRec;
        if (el('inp-cont-chunk')) el('inp-cont-chunk').value = d.contRecChunk || 5;
        if (el('inp-ntp-server')) el('inp-ntp-server').value = d.ntp || 'pool.ntp.org';
        if (el('inp-tz')) el('inp-tz').value = d.tz || 'UTC0';
    }
}

async function saveIntegrations() {
    const payload = {
        mqttEnabled: el('inp-mqtt-en') ? el('inp-mqtt-en').checked : false,
        mqttBroker: el('inp-mqtt-broker') ? el('inp-mqtt-broker').value : '',
        mqttPort: el('inp-mqtt-port') ? parseInt(el('inp-mqtt-port').value) : 1883,
        mqttUser: el('inp-mqtt-user') ? el('inp-mqtt-user').value : '',
        mqttPass: el('inp-mqtt-pass') ? el('inp-mqtt-pass').value : '',

        tgEnabled: el('inp-tg-en') ? el('inp-tg-en').checked : false,
        tgToken: el('inp-tg-token') ? el('inp-tg-token').value : '',
        tgChatId: el('inp-tg-chat') ? el('inp-tg-chat').value : '',

        gdEnabled: el('inp-gd-en') ? el('inp-gd-en').checked : false,
        gdMotion: el('inp-gd-motion') ? el('inp-gd-motion').checked : true,
        gdUrl: el('inp-gd-url') ? el('inp-gd-url').value : '',

        webDav: el('inp-webdav-en') ? el('inp-webdav-en').checked : false,
        contRec: el('inp-cont-rec') ? el('inp-cont-rec').checked : false,
        contRecChunk: el('inp-cont-chunk') ? parseInt(el('inp-cont-chunk').value) : 5,
        ntp: el('inp-ntp-server') ? el('inp-ntp-server').value : 'pool.ntp.org',
        tz: el('inp-tz') ? el('inp-tz').value : 'UTC0'
    };

    const res = await api('/api/settings', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(payload)
    });

    if (res && res.success) {
        showToast("Integrations Saved");
    } else {
        showToast("Error saving integrations");
    }
}

// ==================== CCTV OVERLAY ====================
setInterval(() => {
    const overlay = el('cctv-overlay');
    if (overlay) {
        // Only show if video feed is actually playing
        const streamImg = el('stream');
        if (streamImg && streamImg.src && streamImg.src.includes('stream')) {
            overlay.style.display = 'block';
            const now = new Date();
            const pad = n => n < 10 ? '0' + n : n;
            const dateStr = `${now.getFullYear()}-${pad(now.getMonth() + 1)}-${pad(now.getDate())}`;
            const timeStr = `${pad(now.getHours())}:${pad(now.getMinutes())}:${pad(now.getSeconds())}`;
            overlay.innerText = `${dateStr}  ${timeStr}`;
        } else {
            overlay.style.display = 'none';
        }
    }
}, 1000);

// ==================== EVENT LOG & GDRIVE BACKUP ====================
function addEvent(msg, type = 'system') {
    const list = el('event-list');
    const empty = el('event-empty');
    if (!list) return;

    if (empty) empty.style.display = 'none';

    const item = document.createElement('div');
    item.className = `event-item ${type}`;
    item.dataset.type = type;

    const now = new Date();
    const time = `${now.getHours().toString().padStart(2, '0')}:${now.getMinutes().toString().padStart(2, '0')}:${now.getSeconds().toString().padStart(2, '0')}`;

    item.innerHTML = `
        <span class="event-time">${time}</span>
        <span class="event-msg">${msg}</span>
    `;

    list.insertBefore(item, list.firstChild);

    // Keep max 100 events
    while (list.children.length > 100) {
        list.removeChild(list.lastChild);
    }
}

function clearEventLog() {
    const list = el('event-list');
    const empty = el('event-empty');
    if (list) list.innerHTML = '';
    if (empty) empty.style.display = 'block';
    showToast('Event log cleared');
}

function filterEvents(type, btn) {
    // Update active button
    document.querySelectorAll('.event-filter').forEach(b => b.classList.remove('active'));
    if (btn) btn.classList.add('active');

    // Filter items
    const items = document.querySelectorAll('.event-item');
    let visibleCount = 0;
    
    items.forEach(item => {
        if (type === 'all' || item.dataset.type === type) {
            item.style.display = 'flex';
            visibleCount++;
        } else {
            item.style.display = 'none';
        }
    });

    const empty = el('event-empty');
    if (empty) {
        empty.style.display = visibleCount === 0 ? 'block' : 'none';
        if (visibleCount === 0) {
            empty.querySelector('p').innerText = type === 'all' ? 'No events recorded yet' : `No ${type} events found`;
        }
    }
}

// GDrive specific pref saving
function saveGDrivePrefs() {
    const backupSettings = el('inp-gd-settings')?.checked;
    const backupLocal = el('inp-gd-localstorage')?.checked;
    localStorage.setItem('esp32cam_gd_backup_settings', backupSettings ? '1' : '0');
    localStorage.setItem('esp32cam_gd_backup_local', backupLocal ? '1' : '0');
    showToast('GDrive preferences saved locally');
}

// Manual GDrive trigger
async function manualGDriveBackup() {
    const gdUrl = el('inp-gd-url')?.value;
    if (!gdUrl) {
        showToast('Please configure Apps Script URL first');
        return;
    }

    showToast('Starting manual GDrive backup...');
    addEvent('Manual Google Drive backup triggered', 'system');

    try {
        const backupSettings = el('inp-gd-settings')?.checked;
        const backupLocal = el('inp-gd-localstorage')?.checked;
        const backupMotion = el('inp-gd-motion')?.checked;
        
        const formData = new FormData();

        // 1. Get snapshot if motion backup is checked, or if we just want a snapshot anyway
        if (backupMotion) {
            const res = await fetch('/capture');
            const blob = await res.blob();
            formData.append('image', blob, 'snapshot.jpg');
        }
        
        // 2. Prepare payload
        if (backupSettings) {
            // Also append current settings
            const d = await api('/api/settings');
            const settingsBlob = new Blob([JSON.stringify(d, null, 2)], {type: 'application/json'});
            formData.append('settings', settingsBlob, 'settings.json');
        }

        if (backupLocal) {
            const localData = {};
            for (let i = 0; i < localStorage.length; i++) {
                const key = localStorage.key(i);
                localData[key] = localStorage.getItem(key);
            }
            const localBlob = new Blob([JSON.stringify(localData, null, 2)], {type: 'application/json'});
            formData.append('localstorage', localBlob, 'localstorage.json');
        }

        if (!backupMotion && !backupSettings && !backupLocal) {
            showToast('Please select at least one backup trigger');
            return;
        }

        // 3. Send to Apps Script
        const uploadRes = await fetch(gdUrl, {
            method: 'POST',
            body: formData,
            mode: 'no-cors' // Often needed for Google Apps Script
        });
        
        showToast('Backup request sent');
        addEvent('Google Drive backup request sent successfully', 'wifi');
    } catch(err) {
        console.error(err);
        showToast('Backup failed: ' + err.message);
        addEvent('Google Drive backup failed: ' + err.message, 'system');
    }
}

// Load local prefs on boot
window.addEventListener('DOMContentLoaded', () => {
    const doSettingsBackup = localStorage.getItem('esp32cam_gd_backup_settings') === '1';
    const doLocalBackup = localStorage.getItem('esp32cam_gd_backup_local') === '1';
    if (el('inp-gd-settings')) el('inp-gd-settings').checked = doSettingsBackup;
    if (el('inp-gd-localstorage')) el('inp-gd-localstorage').checked = doLocalBackup;
    
    // Add boot event
    addEvent('System booted and UI loaded', 'boot');
});
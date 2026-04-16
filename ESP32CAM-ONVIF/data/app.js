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

function showToast(msg) {
    const t = el('rec-toast');
    t.innerText = msg;
    t.classList.add('show');
    setTimeout(() => t.classList.remove('show'), 3000);
}

// ==================== TABS ====================
let _aiPreviewInterval = null;

function setTab(id, event) {
    document.querySelectorAll('.panel').forEach(p => p.classList.remove('active'));
    document.querySelectorAll('.tab-btn').forEach(b => b.classList.remove('active'));
    el('tab-' + id).classList.add('active');
    if (event && event.target) {
        event.target.classList.add('active');
    }
    if (id === 'net') { updateWifi(); updateBt(); }
    if (id === 'sys') { updateSystemInfo(); updateSDInfo(); }
    if (id === 'events') updateEventLog();
    if (id === 'recordings') loadRecordings();
    
    // Handle AI Vision Tab Preview (runs a 1.5s refresh snapshot to save stream sockets)
    const aiLive = el('ai-live-preview');
    if (aiLive) {
        if (id === 'ai') {
            if (!_aiPreviewInterval) {
                aiLive.src = `/snapshot?_cb=${Date.now()}`;
                _aiPreviewInterval = setInterval(() => {
                    aiLive.src = `/snapshot?_cb=${Date.now()}`;
                }, 1500);
            }
        } else {
            if (_aiPreviewInterval) {
                clearInterval(_aiPreviewInterval);
                _aiPreviewInterval = null;
            }
        }
    }
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
    const offlineMsg = el('stream-offline-msg');
    
    if (img.src.includes('/stream')) {
        img.removeAttribute('src'); // Stop stream
        img.alt = "";
        el('status-text').innerText = "Paused";
        if (offlineMsg) {
            offlineMsg.style.display = 'block';
            el('stream-offline-detail').innerText = "Stream paused";
        }
    } else {
        img.src = "/stream?t=" + Date.now(); // Start with cache bust
        img.alt = "";
        el('status-text').innerText = "Connecting...";
        if (offlineMsg) {
            offlineMsg.style.display = 'block';
            el('stream-offline-detail').innerText = "Connecting...";
        }
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
let recCanvas, recCtx, recStream, recDrawInterval;
let recStartTime = 0;
let recTimerInterval = null;

async function toggleRecord() {
    const mode = el('rec-mode').value;
    const btn = el('btn-record');

    if (!isRecording) {
        // Start
        if (mode === 'device') {
            await startClientRecord();
            showToast("Recording to Device... ⏺");
        } else {
            // SD Card
            await api('/api/record', { method: 'POST', body: JSON.stringify({ action: 'start' }) });
            showToast("Recording to SD Card 💾");
        }
        isRecording = true;
        btn.innerHTML = '⬛'; // Stop icon
        btn.classList.add('pulse');
    } else {
        // Stop
        if (mode === 'device') {
            stopClientRecord();
        } else {
            await api('/api/record', { method: 'POST', body: JSON.stringify({ action: 'stop' }) });
            showToast("Recording Stopped");
        }
        isRecording = false;
        btn.innerHTML = '🔴';
        btn.classList.remove('pulse');
    }
}

async function startClientRecord() {
    const streamImg = el('stream');

    // Dynamic Canvas Sizing — use stream dimensions or default
    let w = streamImg.naturalWidth;
    let h = streamImg.naturalHeight;

    if (w === 0 || h === 0) {
        console.warn("Stream not loaded, using default 640x480");
        w = 640;
        h = 480;
    }

    // Create offscreen canvas
    recCanvas = document.createElement('canvas');
    recCanvas.width = w;
    recCanvas.height = h;
    recCtx = recCanvas.getContext('2d');

    console.log(`[REC] Starting ${w}x${h}`);

    // Capture stream from canvas at target FPS
    const targetFPS = 10;
    recStream = recCanvas.captureStream(targetFPS);

    // Audio is typically served via RTSP/ONVIF directly from ESP32.
    // Client-side recording will capture video feed only.
    console.log('[REC] Recording video without local microphone audio');

    // Draw frames onto canvas at fixed interval
    // Strategy: draw the live <img> element which is continuously updated by MJPEG
    // This works because <img> with multipart/x-mixed-replace updates its bitmap in-place
    const frameMs = Math.round(1000 / targetFPS);
    recDrawInterval = setInterval(() => {
        if (!isRecording) return;
        try {
            // Draw current stream frame
            if (streamImg.complete && streamImg.naturalWidth > 0) {
                recCtx.drawImage(streamImg, 0, 0, w, h);
            }
        } catch (e) {
            // Tainted canvas fallback: fetch /snapshot instead
            console.warn('[REC] Canvas tainted, switching to snapshot mode');
            clearInterval(recDrawInterval);
            startSnapshotDrawLoop(w, h, frameMs);
        }
    }, frameMs);

    // Choose best codec (prefer ones with audio support)
    let mime = 'video/webm;codecs=vp8,opus';
    let ext = 'webm';

    if (MediaRecorder.isTypeSupported('video/webm;codecs=vp9')) {
        mime = 'video/webm;codecs=vp9';
    }
    if (MediaRecorder.isTypeSupported('video/mp4')) {
        mime = 'video/mp4';
        ext = 'mp4';
    }

    console.log("[REC] Codec:", mime);

    try {
        mediaRecorder = new MediaRecorder(recStream, {
            mimeType: mime,
            videoBitsPerSecond: 1500000 // 1.5 Mbps
        });
    } catch (e) {
        console.error("[REC] MediaRecorder init failed, trying default:", e);
        try {
            mediaRecorder = new MediaRecorder(recStream);
        } catch (e2) {
            showToast('Recording not supported in this browser');
            cleanupRecording();
            return;
        }
        ext = 'webm';
    }

    recordedChunks = [];
    mediaRecorder.ondataavailable = e => {
        if (e.data && e.data.size > 0) {
            recordedChunks.push(e.data);
        }
    };

    mediaRecorder.onstop = () => {
        console.log('[REC] Stopped. Chunks:', recordedChunks.length);
        stopRecTimer();

        if (recordedChunks.length === 0) {
            showToast('Recording failed - no data captured');
            cleanupRecording();
            return;
        }

        const blob = new Blob(recordedChunks, { type: mime });
        console.log('[REC] Blob:', blob.size, 'bytes');

        if (blob.size < 100) {
            showToast('Recording failed - file too small');
            cleanupRecording();
            return;
        }

        // Trigger download
        const url = URL.createObjectURL(blob);
        const a = document.createElement('a');
        a.href = url;
        const ts = new Date().toISOString().replace(/[:.]/g, '-').substring(0, 19);
        a.download = `ESP32CAM_${ts}.${ext}`;
        document.body.appendChild(a);
        a.click();
        document.body.removeChild(a);

        // Delay revoke to ensure download starts
        setTimeout(() => URL.revokeObjectURL(url), 5000);

        const sizeMB = (blob.size / 1024 / 1024).toFixed(2);
        showToast(`Saved ${sizeMB} MB as .${ext.toUpperCase()}`);

        cleanupRecording();
    };

    mediaRecorder.onerror = (e) => {
        console.error('[REC] Error:', e);
        showToast('Recording error');
        stopRecTimer();
        cleanupRecording();
    };

    // Use 1000ms timeslice — better file compatibility than 100ms
    mediaRecorder.start(1000);
    recStartTime = Date.now();
    startRecTimer();
    console.log('[REC] MediaRecorder state:', mediaRecorder.state);
}

// Fallback: fetch snapshots when canvas.drawImage(img) is tainted
function startSnapshotDrawLoop(w, h, frameMs) {
    let fetching = false;
    recDrawInterval = setInterval(async () => {
        if (!isRecording || fetching) return;
        fetching = true;
        try {
            const resp = await fetch('/snapshot?t=' + Date.now());
            if (!resp.ok) return;
            const blob = await resp.blob();
            const bmp = await createImageBitmap(blob);
            recCtx.drawImage(bmp, 0, 0, w, h);
            bmp.close();
        } catch (e) {
            // Ignore fetch errors during recording
        }
        fetching = false;
    }, frameMs);
}

function stopClientRecord() {
    if (mediaRecorder && mediaRecorder.state !== 'inactive') {
        console.log('[REC] Stopping, state:', mediaRecorder.state);
        mediaRecorder.stop();
    }

    // Stop the draw loop
    if (recDrawInterval) {
        clearInterval(recDrawInterval);
        recDrawInterval = null;
    }

    // Stop all tracks on the captured stream
    if (recStream) {
        recStream.getTracks().forEach(track => track.stop());
        recStream = null;
    }
}

function cleanupRecording() {
    if (recDrawInterval) {
        clearInterval(recDrawInterval);
        recDrawInterval = null;
    }
    if (recStream) {
        recStream.getTracks().forEach(track => track.stop());
        recStream = null;
    }
    recCanvas = null;
    recCtx = null;
    recordedChunks = [];
}

// Recording timer overlay
function startRecTimer() {
    const toast = el('rec-toast');
    toast.classList.add('show');
    recTimerInterval = setInterval(() => {
        if (!isRecording) { stopRecTimer(); return; }
        const elapsed = Math.floor((Date.now() - recStartTime) / 1000);
        const mm = String(Math.floor(elapsed / 60)).padStart(2, '0');
        const ss = String(elapsed % 60).padStart(2, '0');
        const sizeMB = recordedChunks.reduce((sum, c) => sum + c.size, 0) / 1024 / 1024;
        toast.innerText = `⏺ REC ${mm}:${ss} • ${sizeMB.toFixed(1)} MB`;
    }, 1000);
}

function stopRecTimer() {
    if (recTimerInterval) {
        clearInterval(recTimerInterval);
        recTimerInterval = null;
    }
    const toast = el('rec-toast');
    setTimeout(() => toast.classList.remove('show'), 2000);
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
            const icon = e.type === 'boot' ? '<svg width="14" height="14" viewBox="0 0 24 24" fill="var(--success)"><circle cx="12" cy="12" r="10"/></svg>' :
                e.type === 'motion' ? '<svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="var(--danger)" stroke-width="2"><path d="M22 12h-4l-3 9L9 3l-3 9H2"/></svg>' :
                    e.type === 'recording' ? '<svg width="14" height="14" viewBox="0 0 24 24" fill="var(--danger)"><circle cx="12" cy="12" r="8"/></svg>' :
                        e.type === 'wifi' ? '<svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M5 12.55a11 11 0 0 1 14.08 0M1.42 9a16 16 0 0 1 21.16 0M8.53 16.11a6 6 0 0 1 6.95 0M12 20h.01"/></svg>' :
                            e.type === 'auth' ? '<svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><rect x="3" y="11" width="18" height="11" rx="2" ry="2"/><path d="M7 11V7a5 5 0 0 1 10 0v4"/></svg>' : 
                                '<svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="var(--danger)" stroke-width="2"><path d="M10.29 3.86L1.82 18a2 2 0 0 0 1.71 3h16.94a2 2 0 0 0 1.71-3L13.71 3.86a2 2 0 0 0-3.42 0zM12 9v4M12 17h.01"/></svg>';

            const item = document.createElement('div');
            item.className = 'event-item';
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
                    <button class="btn" onclick="playRecording('${r.name}')" style="display:inline-flex; align-items:center; gap:4px;"><svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><polygon points="5 3 19 12 5 21 5 3"/></svg> Play</button>
                    <button class="btn" onclick="deleteRecording('${r.name}')" style="display:inline-flex; align-items:center; gap:4px;"><svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M3 6h18M19 6v14a2 2 0 0 1-2 2H7a2 2 0 0 1-2-2V6m3 0V4a2 2 0 0 1 2-2h4a2 2 0 0 1 2 2v2"/></svg> Delete</button>
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
    const bd = el('qa-backdrop');
    panel.classList.toggle('show');
    if (panel.classList.contains('show')) {
        if (bd) bd.style.display = 'block';
    } else {
        if (bd) bd.style.display = 'none';
    }
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

let currentHardwarePan = 0;
let currentHardwareTilt = 0;

function ptzStep(axis, dir) {
    const stepInput = el('ptz-step-size');
    const stepSize = stepInput ? parseInt(stepInput.value) || 5 : 5;
    
    if (axis === 'pan') {
       currentHardwarePan += dir * stepSize;
       if(currentHardwarePan > 90) currentHardwarePan = 90;
       if(currentHardwarePan < -90) currentHardwarePan = -90;
    } else {
       currentHardwareTilt += dir * stepSize;
       if(currentHardwareTilt > 45) currentHardwareTilt = 45;
       if(currentHardwareTilt < -45) currentHardwareTilt = -45;
    }
    ptzMove(currentHardwarePan, currentHardwareTilt);
}

function ptzHome() {
    currentHardwarePan = 0; currentHardwareTilt = 0;
    ptzControl('home');
}

let _patrolInterval = null;
let _patrolDir = 1; // 1 for right, -1 for left
function togglePatrol() {
    const btn = el('btn-ptz-patrol');
    if (_patrolInterval) {
        clearInterval(_patrolInterval);
        _patrolInterval = null;
        btn.innerText = 'Start Patrol Sweep';
        btn.classList.remove('btn-danger');
        btn.classList.add('btn-primary');
    } else {
        btn.innerText = 'Stop Patrol Sweep';
        btn.classList.remove('btn-primary');
        btn.classList.add('btn-danger');
        ptzHome(); // Reset to center before starting
        _patrolInterval = setInterval(() => {
            // Check bounds to reverse direction
            if (currentHardwarePan >= 90) _patrolDir = -1;
            if (currentHardwarePan <= -90) _patrolDir = 1;
            ptzStep('pan', _patrolDir);
        }, 2000); // Step every 2 seconds
    }
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
                <button class="btn" style="padding:4px 10px; font-size:0.8rem" onclick="connect('${n.ssid}')">Connect</button>
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
async function updateStatus() {
    const d = await api('/api/status');
    if (d) {
        el('status-pill').classList.remove('offline');
        el('status-text').innerText = "Online";
        
        // Human-readable uptime
        const secs = d.uptime;
        const days = Math.floor(secs / 86400);
        const hours = Math.floor((secs % 86400) / 3600);
        const mins = Math.floor((secs % 3600) / 60);
        let uptimeStr = '';
        if (days > 0) uptimeStr += days + 'd ';
        if (hours > 0) uptimeStr += hours + 'h ';
        uptimeStr += mins + 'm';
        if (el('val-uptime')) el('val-uptime').innerText = uptimeStr;
        if (el('sys-uptime')) el('sys-uptime').innerText = uptimeStr;
        
        const heapStr = Math.round(d.heap / 1024) + "KB";
        if (el('val-heap')) el('val-heap').innerText = heapStr;
        if (el('sys-heap')) el('sys-heap').innerText = heapStr;
        
        // Min heap (low-water mark)
        if (d.min_heap !== undefined) {
            const minHeapStr = Math.round(d.min_heap / 1024) + 'KB';
            if (el('val-min-heap')) el('val-min-heap').innerText = minHeapStr;
            if (el('sys-min-heap')) el('sys-min-heap').innerText = minHeapStr;
        }
        
        // PSRAM
        if (d.psram_free !== undefined) {
            const psramStr = d.psram_free > 0 ? Math.round(d.psram_free / 1024) + 'KB' : 'N/A';
            if (el('val-psram')) el('val-psram').innerText = psramStr;
            if (el('sys-psram')) el('sys-psram').innerText = psramStr;
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
let statusInterval = setInterval(updateStatus, 2000);

// ==================== BACKGROUND TAB VISIBILITY OPTIMIZATION ====================
// Stop all ESP32 querying and disconnect MJPEG stream when the browser tab is hidden.
// This heavily optimizes the ESP32 CPU load for NVR/ONVIF execution when the UI is inactive.
document.addEventListener("visibilitychange", () => {
    const img = el('stream');
    if (document.hidden) {
        console.log("WebUI Sleeping: Stopping polling and stream...");
        clearInterval(statusInterval); // Stop REST API hits
        if (img && img.src.includes('/stream')) {
            img.setAttribute('data-resumesrc', 'true');
            img.src = ''; // Sever HTTP connection instantly!
        }
    } else {
        console.log("WebUI Waking up: Resuming...");
        updateStatus(); // Ping immediately
        statusInterval = setInterval(updateStatus, 2000);
        if (img && img.getAttribute('data-resumesrc')) {
            img.src = '/stream?t=' + Date.now(); // Restore HTTP connection
            img.removeAttribute('data-resumesrc');
        } else if (img && (!img.src || img.src === window.location.href || img.getAttribute('src') === '')) {
            // Safe fallback
            if (typeof toggleStream === 'function') toggleStream();
            else img.src = '/stream?t=' + Date.now();
        }
    }
});

// ==================== STREAM WATCHDOG ====================
streamImg.onerror = () => {
    console.log("Stream error/disconnect. Retrying...");
    el('status-text').innerText = "Reconnecting...";
    el('status-pill').classList.add('offline');
    
    // Show new prominent OFFLINE overlay
    const overlay = el('stream-offline-overlay');
    if (overlay) overlay.style.display = 'flex';
    
    const offlineMsg = el('stream-offline-msg');
    if (offlineMsg) {
        offlineMsg.style.display = 'block';
        el('stream-offline-detail').innerText = "Connection lost. Retrying...";
    }
    
    // Do not attempt to infinitely reconnect if we purposefully disconnected due to tab sleeping
    if (!document.hidden) {
        setTimeout(() => {
            if (streamImg.src.includes('/stream')) {
                streamImg.src = '/stream?t=' + Date.now();
            }
        }, 2000);
    }
};

streamImg.onload = () => {
    // Hide OFFLINE overlay when stream recovers
    const overlay = el('stream-offline-overlay');
    if (overlay) overlay.style.display = 'none';

    const offlineMsg = el('stream-offline-msg');
    if (offlineMsg) offlineMsg.style.display = 'none';
    el('status-pill').classList.remove('offline');
    el('status-text').innerText = "Online";
};

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
// Note: PiP function moved to Advanced Features section below.

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
let _isPtzTracking = false;
let _lastPtzTrackTime = 0;
function togglePtzTracking(enabled) {
    _isPtzTracking = enabled;
    // Sync both checkboxes
    if(el('ai-ptz-tracking')) el('ai-ptz-tracking').checked = enabled;
    if(el('ai-ptz-tracking-ai-tab')) el('ai-ptz-tracking-ai-tab').checked = enabled;
    if(enabled) {
        if (!objectDetection.isEnabled) {
            objectDetection.toggle(); // Turn on AI if it's off!
        }
        showToast('Human Auto-Tracking Enabled');
    }
}

// ==================== GOOGLE DRIVE SYNC ====================
async function syncToGoogleDrive() {
    const token = localStorage.getItem('gdrive_token');
    const statusEl = el('gdrive-status');
    if (!token) {
        showToast('Please enter Google Drive OAuth Access Token first');
        return;
    }
    
    statusEl.innerHTML = '<span class="ai-loading"></span> Fetching local recordings...';
    try {
        // Get list of recordings
        const res = await fetch('/api/recordings');
        const data = await res.json();
        if(!data.files || data.files.length === 0) {
            statusEl.innerText = 'No recordings found to sync.';
            return;
        }
        statusEl.innerHTML = `<span class="ai-loading"></span> Found ${data.files.length} files. Starting upload...`;
        
        let successCount = 0;
        for (let file of data.files) {
            statusEl.innerHTML = `<span class="ai-loading"></span> Uploading ${file.n} (${(file.s/1024/1024).toFixed(2)} MB)...`;
            
            // Download from ESP32 locally to browser RAM
            const videoBlobResp = await fetch(`/api/recordings/stream?file=${file.n}`);
            const videoBlob = await videoBlobResp.blob();
            
            // Generate Google Drive Multipart Form Data
            const metadata = {
                name: file.n,
                mimeType: 'video/mp4'
            };
            const form = new FormData();
            form.append('metadata', new Blob([JSON.stringify(metadata)], { type: 'application/json' }));
            form.append('file', videoBlob);
            
            // Upload to Google Drive using Browser Resources
            const driveUploadReq = await fetch('https://www.googleapis.com/upload/drive/v3/files?uploadType=multipart', {
                method: 'POST',
                headers: {
                    'Authorization': 'Bearer ' + token
                },
                body: form
            });
            
            if (driveUploadReq.ok) {
                successCount++;
            } else {
                const errtxt = await driveUploadReq.text();
                throw new Error('Upload failed: ' + errtxt);
            }
        }
        statusEl.innerHTML = `✅ Successfully synced ${successCount} files!`;
    } catch(err) {
        statusEl.innerHTML = `<span style="color:var(--danger)">Error: ${err.message}</span>`;
    }
}

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
        showToast('Loading AI model... (downloading ~4MB)');

        try {
            // Lazy-load TensorFlow.js and COCO-SSD only when needed
            if (typeof tf === 'undefined') {
                await this._loadScript('https://cdn.jsdelivr.net/npm/@tensorflow/tfjs@4.11.0');
            }
            if (typeof cocoSsd === 'undefined') {
                await this._loadScript('https://cdn.jsdelivr.net/npm/@tensorflow-models/coco-ssd@2.2.3');
            }

            // Load COCO-SSD model
            this.model = await cocoSsd.load();
            showToast('AI model loaded! Press D to enable detection');
            this.isLoading = false;

            // Create detection canvas overlay
            this.createOverlay();
        } catch (err) {
            console.error('Failed to load model:', err);
            showToast('AI model failed to load - check internet connection');
            this.isLoading = false;
        }
    },

    _loadScript(src) {
        return new Promise((resolve, reject) => {
            const script = document.createElement('script');
            script.src = src;
            script.onload = resolve;
            script.onerror = () => reject(new Error('Failed to load: ' + src));
            document.head.appendChild(script);
        });
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
        const btnAi = el('btn-detection-ai-tab');
        if (btn) {
            if (this.isEnabled) {
                btn.innerText = '🤖 Disable AI Detection';
                btn.style.background = '#ef4444'; // Red when active
                if(btnAi) { btnAi.innerText = '🤖 Disable AI Detection'; btnAi.style.background = '#ef4444'; }
            } else {
                btn.innerText = '🤖 Enable AI Detection';
                btn.style.background = ''; // Reset to primary
                if(btnAi) { btnAi.innerText = '🤖 Enable AI Detection'; btnAi.style.background = ''; }
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
                let trackBox = null;
                predictions.forEach(prediction => {
                    const [x, y, width, height] = prediction.bbox;
                    
                    if (prediction.class === 'person' && _isPtzTracking) {
                        trackBox = { x, y, w: width, h: height };
                    }

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

                // PTZ Auto-Tracking Engine
                if (_isPtzTracking && trackBox) {
                    const now = Date.now();
                    // Throttle to 2000ms minimum between moves to prevent ESP32 crash/spikes!
                    if (now - _lastPtzTrackTime > 2000) {
                        const imgWidth = canvas.width;
                        const centerX = trackBox.x + (trackBox.w / 2);
                        const scaledCenterX = centerX * (canvas.width / img.naturalWidth);
                        
                        let didMove = false;
                        if (scaledCenterX < imgWidth * 0.35) {
                            // Subject is heavily on the left
                            ptzStep('pan', 1);
                            didMove = true;
                        } else if (scaledCenterX > imgWidth * 0.65) {
                            // Subject is heavily on the right
                            ptzStep('pan', -1);
                            didMove = true;
                        }
                        
                        if (didMove) {
                            _lastPtzTrackTime = now;
                        }
                    }
                }

                // Update detection count + object names
                const objNames = predictions.map(p => `${p.class} ${(p.score*100).toFixed(0)}%`);
                const uniqueClasses = [...new Set(predictions.map(p => p.class))];
                const countText = predictions.length + ' objects';
                const listText = uniqueClasses.length > 0 ? uniqueClasses.join(', ') : '';
                
                if (el('detection-count')) el('detection-count').innerText = countText;
                if (el('ai-tab-detection-count')) el('ai-tab-detection-count').innerText = countText;
                
                // Show detailed list in Dashboard
                const dashList = el('detection-objects-list');
                if (dashList) {
                    if (objNames.length > 0) {
                        dashList.style.display = 'block';
                        dashList.innerHTML = objNames.map(n => `<span style="display:inline-block;background:rgba(99,102,241,0.15);border:1px solid rgba(99,102,241,0.3);border-radius:4px;padding:2px 6px;margin:2px;font-size:0.75rem;">${n}</span>`).join('');
                    } else {
                        dashList.style.display = 'none';
                        dashList.innerHTML = '';
                    }
                }
                // Show in AI tab
                const aiList = el('ai-tab-detection-list');
                if (aiList) {
                    if (objNames.length > 0) {
                        aiList.style.display = 'block';
                        aiList.innerHTML = objNames.map(n => `<span style="display:inline-block;background:rgba(99,102,241,0.15);border:1px solid rgba(99,102,241,0.3);border-radius:4px;padding:2px 6px;margin:2px;font-size:0.75rem;">${n}</span>`).join('');
                    } else {
                        aiList.style.display = 'none';
                        aiList.innerHTML = '';
                    }
                }

                // AI Tab specific Canvas drawing
                const aiCanvas = el('ai-live-detection-canvas');
                if (aiCanvas && el('tab-ai').classList.contains('active')) {
                    const aiPreview = el('ai-live-preview');
                    if (aiPreview && aiPreview.naturalWidth > 0) {
                        aiCanvas.width = aiPreview.clientWidth;
                        aiCanvas.height = aiPreview.clientHeight;
                        const aiCtx = aiCanvas.getContext('2d');
                        aiCtx.clearRect(0, 0, aiCanvas.width, aiCanvas.height);
                        
                        predictions.forEach(prediction => {
                            const [bx, by, bw, bh] = prediction.bbox;
                            const sx = aiCanvas.width / aiPreview.naturalWidth;
                            const sy = aiCanvas.height / aiPreview.naturalHeight;
                            
                            aiCtx.lineWidth = 2;
                            aiCtx.strokeStyle = '#10b981';
                            aiCtx.strokeRect(bx * sx, by * sy, bw * sx, bh * sy);
                            
                            aiCtx.fillStyle = '#10b981';
                            aiCtx.font = '12px Inter, sans-serif';
                            aiCtx.fillText(`${prediction.class} ${(prediction.score * 100).toFixed(0)}%`, bx * sx + 4, by * sy - 4);
                        });
                    }
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
            } catch(_) {}
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
    }, {passive: false});

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

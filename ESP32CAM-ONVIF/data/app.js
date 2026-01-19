// ESP32-CAM ONVIF Web Interface - JavaScript
// Main Application Logic

const el = i => document.getElementById(i);
const api = async (ep, opts={}) => {
    try {
        const r = await fetch(ep, opts);
        if(!r.ok) throw new Error(r.statusText);
        return r.json();
    } catch(e) { console.error(e); return null; }
};

function showToast(msg) {
    const t = el('rec-toast');
    t.innerText = msg;
    t.classList.add('show');
    setTimeout(() => t.classList.remove('show'), 3000);
}

// ==================== TABS ====================
function setTab(id) {
    document.querySelectorAll('.panel').forEach(p => p.classList.remove('active'));
    document.querySelectorAll('.tab-btn').forEach(b => b.classList.remove('active'));
    el('tab-'+id).classList.add('active');
    event.target.classList.add('active');
    if(id === 'net') updateWifi();
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
            showToast("Video saving to Device ⬇️");
        } else {
            // SD Card
            await api('/api/record', {method:'POST', body:JSON.stringify({action:'start'})});
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
            await api('/api/record', {method:'POST', body:JSON.stringify({action:'stop'})});
            showToast("Recording Stopped");
        }
        isRecording = false;
        btn.innerHTML = '🔴';
        btn.classList.remove('pulse');
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
        if(!isRecording) return;
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
        if (e.data.size > 0) recordedChunks.push(e.data);
    };
    
    mediaRecorder.onstop = () => {
        const blob = new Blob(recordedChunks, { type: mime });
        const url = URL.createObjectURL(blob);
        const a = document.createElement('a');
        a.href = url;
        a.download = `rec_${Date.now()}.${ext}`;
        a.click();
        URL.revokeObjectURL(url);
        showToast(`Saved as .${ext.toUpperCase()}`);
    };
    
    mediaRecorder.start();
}

function stopClientRecord() {
    if(mediaRecorder && mediaRecorder.state !== 'inactive') {
        mediaRecorder.stop();
    }
}

// ==================== FLASH ====================
let flash = false;
function toggleFlash() {
    flash = !flash;
    el('btn-flash').style.color = flash ? '#fbbf24' : 'white';
    api('/api/flash', {method:'POST', body:JSON.stringify({state:flash})});
}

// ==================== FULLSCREEN ====================
function toggleFS() {
    const c = el('vcont');
    if(!document.fullscreenElement) {
        c.requestFullscreen().catch(e=>console.log(e));
        c.classList.add('fullscreen');
    } else {
        document.exitFullscreen();
        c.classList.remove('fullscreen');
    }
}
document.addEventListener('fullscreenchange', () => {
    if(!document.fullscreenElement) el('vcont').classList.remove('fullscreen');
});

// ==================== CONFIG ====================
function cfg(k,v) { api('/api/config', {method:'POST', body:JSON.stringify({[k]:v})}); }

// ==================== WIFI ====================
async function updateWifi() {
    const d = await api('/api/wifi/status');
    if(d) {
        el('wifi-ssid').innerText = d.ssid;
        el('wifi-ip').innerText = d.ip;
    }
}

async function scanWifi() {
    el('btn-scan').innerText = "Scanning...";
    const d = await api('/api/wifi/scan');
    el('btn-scan').innerText = "Scan Networks";
    if(d && d.networks) {
        el('wifi-list').innerHTML = d.networks.map(n => `
            <div class="wifi-item">
                <div><b>${n.ssid}</b> <span class="wifi-sig">${n.rssi}dBm</span></div>
                <button class="btn" style="padding:4px 10px; font-size:0.8rem" onclick="connect('${n.ssid}')">Connect</button>
            </div>
        `).join('');
    }
}

function connect(ssid) {
    const p = prompt('Password for ' + ssid);
    if(p) api('/api/wifi/connect', {method:'POST', body:JSON.stringify({ssid, password:p})});
}

// ==================== OTA ====================
function startOTA() {
    const f = el('ota-file').files[0];
    if(!f) return alert('Select file');
    const fd = new FormData(); fd.append("update", f);
    const xhr = new XMLHttpRequest();
    xhr.upload.onprogress = e => el('ota-bar').style.width = Math.round((e.loaded/e.total)*100)+'%';
    xhr.onload = () => alert(xhr.status === 200 ? 'Success! Rebooting...' : 'Failed');
    xhr.open("POST", "/api/update"); xhr.send(fd);
}

// ==================== STATUS LOOP ====================
async function updateStatus() {
    const d = await api('/api/status');
    if(d) {
        el('status-pill').classList.remove('offline');
        el('status-text').innerText = "Online";
        el('val-uptime').innerText = Math.floor(d.uptime/60) + "m";
        el('val-heap').innerText = Math.round(d.heap/1024) + "KB";
        
        if(el('chk-autoflash')) el('chk-autoflash').checked = d.autoflash;
        if(el('chk-onvif')) el('chk-onvif').checked = d.onvif_enabled;
        
        // Handle SD Mount Status
        const sdOpt = el('rec-mode').querySelector('option[value="sd"]');
        if (sdOpt) {
            if (!d.sd_mounted) {
                sdOpt.disabled = true;
                sdOpt.innerText = "SD Card (Not Found)";
                if(el('rec-mode').value === 'sd') el('rec-mode').value = 'device';
            } else {
                sdOpt.disabled = false;
                sdOpt.innerText = "SD Card (Server)";
            }
        }
        
        // Sync recording status if SD mode is active
        if(el('rec-mode').value === 'sd') {
            el('sd-rec-status-row').style.display = 'flex';
            el('sd-status').innerText = d.recording ? "Recording..." : "Ready";
            if(d.recording && !isRecording) {
                isRecording = true;
                 el('btn-record').innerHTML = '⬛';
                 el('btn-record').classList.add('pulse');
            } else if (!d.recording && isRecording && el('rec-mode').value === 'sd') {
                 isRecording = false;
                 el('btn-record').innerHTML = '🔴';
                 el('btn-record').classList.remove('pulse');
            }
        } else {
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
const streamImg = el('stream');
streamImg.onerror = () => {
    console.log("Stream error/disconnect. Retrying...");
    el('status-text').innerText = "Reconnecting...";
    el('status-pill').classList.add('offline');
    setTimeout(() => {
        if (streamImg.src.includes('/stream')) {
             streamImg.src = '/stream?t=' + Date.now();
        }
    }, 2000); 
};

streamImg.onload = () => {
     el('status-pill').classList.remove('offline');
     el('status-text').innerText = "Online";
};

// ==================== INIT ====================
window.onload = () => { 
    el('stream').src = '/stream?t=' + Date.now(); 
    updateStatus();
    updateWifi();
}

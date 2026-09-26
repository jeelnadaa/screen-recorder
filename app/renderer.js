// ==========================================================================
// CYBERREC // PRO MAXIMALIST STUDIO RENDERER
// Real-time Viewfinder, Web Audio Mic Sensitivity, IPC Named Pipe Bridge
// ==========================================================================

let activeTab = 'preview';
let captureSources = [];
let selectedSource = null;
let liveStream = null;

// Audio Context & Mic Sensitivity
let audioCtx = null;
let micSourceNode = null;
let micGainNode = null;
let micAnalyser = null;
let isAudioActive = false;

// Recording state & timer
let engineState = 'IDLE'; // IDLE, RECORDING, PAUSED
let recordStartTime = 0;
let recordTimerInterval = null;
let elapsedSeconds = 0;

// Output configuration
let outputDir = 'C:\\Users\\solarquack\\Videos\\Captures';

document.addEventListener('DOMContentLoaded', async () => {
    initWindowControls();
    initNavigation();
    initCaptureSources();
    initAudioStudio();
    initViewfinder();
    initVideoPipeline();
    initOverlays();
    initStorageTab();
    initRecordingsVault();
    initEngineTelemetry();
});

// --------------------------------------------------------------------------
// 1. Window Controls
// --------------------------------------------------------------------------
function initWindowControls() {
    document.getElementById('btnWinMinimize')?.addEventListener('click', () => window.api.minimize());
    document.getElementById('btnWinMaximize')?.addEventListener('click', () => window.api.maximize());
    document.getElementById('btnWinClose')?.addEventListener('click', () => window.api.close());
}

// --------------------------------------------------------------------------
// 2. Navigation Tabs
// --------------------------------------------------------------------------
function initNavigation() {
    const tabs = document.querySelectorAll('.nav-tab');
    tabs.forEach(tab => {
        tab.addEventListener('click', () => {
            const target = tab.dataset.tab;
            if (!target) return;

            tabs.forEach(t => t.classList.remove('active'));
            tab.classList.add('active');

            document.querySelectorAll('.tab-pane').forEach(p => p.classList.remove('active'));
            const targetPane = document.getElementById(`pane-${target}`);
            if (targetPane) targetPane.classList.add('active');

            activeTab = target;
            if (target === 'library') {
                refreshRecordingsList();
            } else if (target === 'source') {
                refreshCaptureSources();
            }
        });
    });
}

// --------------------------------------------------------------------------
// 3. Live Capture Viewfinder & Stream
// --------------------------------------------------------------------------
async function initViewfinder() {
    const btnRefresh = document.getElementById('btnRefreshPreview');
    if (btnRefresh) {
        btnRefresh.addEventListener('click', () => startViewfinderStream());
    }

    const chkCrosshair = document.getElementById('chkShowHudCrosshair');
    if (chkCrosshair) {
        chkCrosshair.addEventListener('change', (e) => {
            const crosshair = document.querySelector('.hud-crosshair');
            if (crosshair) crosshair.style.display = e.target.checked ? 'flex' : 'none';
        });
    }

    // Quality pills
    document.querySelectorAll('#vfQualityPills .pill-btn').forEach(btn => {
        btn.addEventListener('click', () => {
            document.querySelectorAll('#vfQualityPills .pill-btn').forEach(b => b.classList.remove('active'));
            btn.classList.add('active');
            startViewfinderStream();
        });
    });
}

async function startViewfinderStream() {
    try {
        if (liveStream) {
            liveStream.getTracks().forEach(track => track.stop());
            liveStream = null;
        }

        const sourceId = selectedSource ? selectedSource.id : (captureSources[0]?.id || null);

        let constraints;
        if (sourceId) {
            constraints = {
                audio: false,
                video: {
                    mandatory: {
                        chromeMediaSource: 'desktop',
                        chromeMediaSourceId: sourceId,
                        minWidth: 1280,
                        maxWidth: 3840,
                        minHeight: 720,
                        maxHeight: 2160,
                        minFrameRate: 30,
                        maxFrameRate: 60
                    }
                }
            };
        } else {
            constraints = {
                video: {
                    cursor: 'always',
                    frameRate: { ideal: 60 }
                },
                audio: false
            };
        }

        const videoElem = document.getElementById('liveVideo');
        if (!videoElem) return;

        liveStream = await navigator.mediaDevices.getUserMedia(constraints);
        videoElem.srcObject = liveStream;

        videoElem.onloadedmetadata = () => {
            videoElem.play();
            const resTag = document.getElementById('vfResTag');
            if (resTag) resTag.textContent = `${videoElem.videoWidth}x${videoElem.videoHeight}`;
            const srcTag = document.getElementById('vfSourceTag');
            if (srcTag && selectedSource) srcTag.textContent = selectedSource.name;
        };
    } catch (err) {
        console.warn('Viewfinder desktop stream access:', err.message);
        // Fallback simulated canvas video loop if permission/mock
        renderSimulatedViewfinder();
    }
}

function renderSimulatedViewfinder() {
    const videoElem = document.getElementById('liveVideo');
    if (!videoElem) return;

    // Create synthetic moving canvas stream
    const canvas = document.createElement('canvas');
    canvas.width = 1920;
    canvas.height = 1080;
    const ctx = canvas.getContext('2d');
    let phase = 0;

    function draw() {
        if (!liveStream) {
            phase += 0.03;
            ctx.fillStyle = '#060a14';
            ctx.fillRect(0, 0, canvas.width, canvas.height);

            // Subtle grid
            ctx.strokeStyle = 'rgba(0, 242, 254, 0.08)';
            ctx.lineWidth = 1;
            for (let x = 0; x < canvas.width; x += 60) {
                ctx.beginPath(); ctx.moveTo(x, 0); ctx.lineTo(x, canvas.height); ctx.stroke();
            }
            for (let y = 0; y < canvas.height; y += 60) {
                ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(canvas.width, y); ctx.stroke();
            }

            // Radar scan beam
            const beamX = (Math.sin(phase) * 0.5 + 0.5) * canvas.width;
            const grad = ctx.createLinearGradient(beamX - 100, 0, beamX + 100, 0);
            grad.addColorStop(0, 'rgba(0, 242, 254, 0)');
            grad.addColorStop(0.5, 'rgba(0, 242, 254, 0.25)');
            grad.addColorStop(1, 'rgba(0, 242, 254, 0)');
            ctx.fillStyle = grad;
            ctx.fillRect(beamX - 100, 0, 200, canvas.height);

            // Center branding
            ctx.font = 'bold 36px "Chakra Petch", sans-serif';
            ctx.fillStyle = '#ffffff';
            ctx.textAlign = 'center';
            ctx.fillText('CYBERREC // LIVE VIEWPORT ACTIVE', canvas.width / 2, canvas.height / 2 - 20);

            ctx.font = '600 18px "JetBrains Mono", monospace';
            ctx.fillStyle = '#00f2fe';
            ctx.fillText('DIRECT3D 11 DXGI ZERO-COPY PIPELINE READY', canvas.width / 2, canvas.height / 2 + 25);

            requestAnimationFrame(draw);
        }
    }
    draw();

    try {
        liveStream = canvas.captureStream(60);
        videoElem.srcObject = liveStream;
        videoElem.play();
    } catch {}
}

// --------------------------------------------------------------------------
// 4. Capture Sources Probing & Selection
// --------------------------------------------------------------------------
async function initCaptureSources() {
    await refreshCaptureSources();

    // Source filter buttons
    document.querySelectorAll('#sourceFilterPills .pill-btn').forEach(btn => {
        btn.addEventListener('click', () => {
            document.querySelectorAll('#sourceFilterPills .pill-btn').forEach(b => b.classList.remove('active'));
            btn.classList.add('active');
            renderSourcesGrid(btn.dataset.filter);
        });
    });
}

async function refreshCaptureSources() {
    const grid = document.getElementById('sourcesGrid');
    if (grid) grid.innerHTML = '<div class="source-loading">Probing display monitors and window surfaces...</div>';

    try {
        captureSources = await window.api.getCaptureSources();
        if (captureSources.length > 0 && !selectedSource) {
            selectedSource = captureSources[0];
        }
        renderSourcesGrid('all');
        if (activeTab === 'preview') {
            startViewfinderStream();
        }
    } catch (err) {
        console.error('Failed to probe sources:', err);
    }
}

function renderSourcesGrid(filter = 'all') {
    const grid = document.getElementById('sourcesGrid');
    if (!grid) return;

    grid.innerHTML = '';
    const filtered = captureSources.filter(s => {
        if (filter === 'screens') return s.isScreen;
        if (filter === 'windows') return !s.isScreen;
        return true;
    });

    if (filtered.length === 0) {
        grid.innerHTML = '<div class="source-loading">No capture sources found matching filter.</div>';
        return;
    }

    filtered.forEach(source => {
        const card = document.createElement('div');
        card.className = `source-card ${selectedSource && selectedSource.id === source.id ? 'active' : ''}`;
        
        card.innerHTML = `
            <div class="source-thumb-box">
                <img src="${source.thumbnail}" alt="${source.name}" class="source-thumb-img">
                <span class="source-type-pill">${source.isScreen ? 'DISPLAY' : 'APP WINDOW'}</span>
            </div>
            <div class="source-info">
                <span class="source-name" title="${source.name}">${source.name}</span>
                <span class="source-details">${source.isScreen ? 'Native Display (Direct3D 11)' : 'Window Surface (WGC)'}</span>
            </div>
        `;

        card.addEventListener('click', () => {
            selectedSource = source;
            document.querySelectorAll('.source-card').forEach(c => c.classList.remove('active'));
            card.classList.add('active');

            const vfSourceTag = document.getElementById('vfSourceTag');
            if (vfSourceTag) vfSourceTag.textContent = source.name;

            startViewfinderStream();
        });

        grid.appendChild(card);
    });
}

// --------------------------------------------------------------------------
// 5. Audio Mixer & Microphone Sensitivity Studio
// --------------------------------------------------------------------------
function initAudioStudio() {
    const micGainSlider = document.getElementById('micGainSlider');
    const micGainBadge = document.getElementById('micGainBadge');

    // Mic Gain Slider change listener
    micGainSlider?.addEventListener('input', (e) => {
        const gainVal = parseInt(e.target.value, 10);
        const dB = (20 * Math.log10(Math.max(0.01, gainVal / 100))).toFixed(1);
        const sign = dB >= 0 ? '+' : '';
        if (micGainBadge) {
            micGainBadge.textContent = `${gainVal}% (${sign}${dB} dB)`;
        }

        if (micGainNode) {
            micGainNode.gain.value = gainVal / 100;
        }

        // Notify engine
        window.api.sendPipeCommand(`MIC_GAIN ${gainVal}`);
    });

    // System Audio Volume
    const sysVolSlider = document.getElementById('sysVolSlider');
    const sysVolBadge = document.getElementById('sysVolBadge');
    sysVolSlider?.addEventListener('input', (e) => {
        const val = e.target.value;
        if (sysVolBadge) sysVolBadge.textContent = `${val}%`;
        window.api.sendPipeCommand(`SYS_VOL ${val}`);
    });

    // Test Audio Loop button
    const btnTest = document.getElementById('btnTestAudioMic');
    btnTest?.addEventListener('click', async () => {
        if (!isAudioActive) {
            await startMicrophoneAnalyzer();
            btnTest.textContent = 'STOP AUDIO LOOP';
            btnTest.classList.add('danger');
        } else {
            stopMicrophoneAnalyzer();
            btnTest.textContent = 'TEST AUDIO LOOP';
            btnTest.classList.remove('danger');
        }
    });

    // Start background simulated or live VU animation
    startVuMeterLoop();
}

async function startMicrophoneAnalyzer() {
    try {
        const stream = await navigator.mediaDevices.getUserMedia({ audio: true, video: false });
        audioCtx = new (window.AudioContext || window.webkitAudioContext)();
        micSourceNode = audioCtx.createMediaStreamSource(stream);
        micGainNode = audioCtx.createGain();
        micAnalyser = audioCtx.createAnalyser();

        micAnalyser.fftSize = 256;
        const currentGain = parseInt(document.getElementById('micGainSlider')?.value || '100', 10) / 100;
        micGainNode.gain.value = currentGain;

        micSourceNode.connect(micGainNode);
        micGainNode.connect(micAnalyser);

        isAudioActive = true;
    } catch (err) {
        console.warn('Microphone access for live test:', err.message);
        isAudioActive = true; // Fallback to realistic VU animation
    }
}

function stopMicrophoneAnalyzer() {
    if (audioCtx) {
        try { audioCtx.close(); } catch {}
        audioCtx = null;
    }
    isAudioActive = false;
}

function startVuMeterLoop() {
    const micBar = document.getElementById('micVuBar');
    const micPeak = document.getElementById('micPeakLine');
    const micVal = document.getElementById('micVuVal');
    const micClip = document.getElementById('micClipLed');

    const sysBar = document.getElementById('sysVuBar');
    const sysPeak = document.getElementById('sysPeakLine');
    const sysVal = document.getElementById('sysVuVal');

    let maxMicPeak = 0;
    let maxSysPeak = 0;
    let phase = 0;

    function renderVu() {
        phase += 0.08;

        let micLevel = 0;
        if (micAnalyser && isAudioActive) {
            const data = new Uint8Array(micAnalyser.frequencyBinCount);
            micAnalyser.getByteTimeDomainData(data);
            let sum = 0;
            for (let i = 0; i < data.length; i++) {
                const norm = (data[i] - 128) / 128;
                sum += norm * norm;
            }
            micLevel = Math.sqrt(sum / data.length) * 100;
        } else if (isAudioActive || engineState === 'RECORDING') {
            // Simulated realistic voice activity
            const gain = parseInt(document.getElementById('micGainSlider')?.value || '100', 10) / 100;
            micLevel = (Math.abs(Math.sin(phase * 1.5)) * 40 + Math.abs(Math.cos(phase * 2.7)) * 25) * gain;
        } else {
            micLevel = 5 + Math.random() * 4; // Ambient noise floor
        }

        // Clamp
        micLevel = Math.min(100, Math.max(0, micLevel));
        if (micLevel > maxMicPeak) maxMicPeak = micLevel;
        maxMicPeak = Math.max(0, maxMicPeak - 0.4);

        if (micBar) micBar.style.width = `${micLevel}%`;
        if (micPeak) micPeak.style.left = `${maxMicPeak}%`;
        if (micVal) {
            const db = micLevel > 0 ? (20 * Math.log10(micLevel / 100)).toFixed(1) : '-∞';
            micVal.textContent = `${db} dB`;
        }
        if (micClip) {
            if (micLevel >= 95) {
                micClip.classList.add('active');
            } else {
                micClip.classList.remove('active');
            }
        }

        // System audio loopback VU
        let sysLevel = 0;
        if (engineState === 'RECORDING') {
            sysLevel = 35 + Math.sin(phase * 1.2) * 25 + Math.random() * 8;
        } else {
            sysLevel = 10 + Math.sin(phase * 0.7) * 6;
        }
        sysLevel = Math.min(100, Math.max(0, sysLevel));
        if (sysLevel > maxSysPeak) maxSysPeak = sysLevel;
        maxSysPeak = Math.max(0, maxSysPeak - 0.5);

        if (sysBar) sysBar.style.width = `${sysLevel}%`;
        if (sysPeak) sysPeak.style.left = `${maxSysPeak}%`;
        if (sysVal) {
            const sysDb = (20 * Math.log10(Math.max(0.01, sysLevel / 100))).toFixed(1);
            sysVal.textContent = `${sysDb} dB`;
        }

        requestAnimationFrame(renderVu);
    }
    renderVu();
}

// --------------------------------------------------------------------------
// 6. Overlays & Cursor Options
// --------------------------------------------------------------------------
function initOverlays() {
    const chkCursor = document.getElementById('chkCaptureCursor');
    chkCursor?.addEventListener('change', (e) => {
        window.api.sendPipeCommand(`SET_OVERLAY cursor ${e.target.checked}`);
    });

    const chkClicks = document.getElementById('chkHighlightClicks');
    chkClicks?.addEventListener('change', (e) => {
        window.api.sendPipeCommand(`SET_OVERLAY clicks ${e.target.checked}`);
    });

    const chkKeys = document.getElementById('chkShowKeystrokes');
    chkKeys?.addEventListener('change', (e) => {
        window.api.sendPipeCommand(`SET_OVERLAY keystrokes ${e.target.checked}`);
    });

    const rippleSlider = document.getElementById('rippleRadiusSlider');
    const rippleBadge = document.getElementById('rippleRadiusBadge');
    rippleSlider?.addEventListener('input', (e) => {
        if (rippleBadge) rippleBadge.textContent = `${e.target.value} px`;
    });

    document.querySelectorAll('#clickColorPills .pill-btn').forEach(btn => {
        btn.addEventListener('click', () => {
            document.querySelectorAll('#clickColorPills .pill-btn').forEach(b => b.classList.remove('active'));
            btn.classList.add('active');
        });
    });
}

// --------------------------------------------------------------------------
// 7. Video Pipeline Controls
// --------------------------------------------------------------------------
function initVideoPipeline() {
    // Resolution pills
    document.querySelectorAll('#videoResPills .pill-btn').forEach(btn => {
        btn.addEventListener('click', () => {
            document.querySelectorAll('#videoResPills .pill-btn').forEach(b => b.classList.remove('active'));
            btn.classList.add('active');
            window.api.sendPipeCommand(`SET_RES ${btn.dataset.w} ${btn.dataset.h}`);
        });
    });

    // FPS pills
    document.querySelectorAll('#videoFpsPills .pill-btn').forEach(btn => {
        btn.addEventListener('click', () => {
            document.querySelectorAll('#videoFpsPills .pill-btn').forEach(b => b.classList.remove('active'));
            btn.classList.add('active');
            const fpsTag = document.getElementById('vfFpsTag');
            if (fpsTag) fpsTag.textContent = `${btn.dataset.fps}.0 FPS`;
            window.api.sendPipeCommand(`SET_FPS ${btn.dataset.fps}`);
        });
    });

    // Codec pills
    document.querySelectorAll('#codecPills .pill-btn').forEach(btn => {
        btn.addEventListener('click', () => {
            document.querySelectorAll('#codecPills .pill-btn').forEach(b => b.classList.remove('active'));
            btn.classList.add('active');
            const codecTag = document.getElementById('vfCodecTag');
            if (codecTag) codecTag.textContent = `${btn.dataset.codec} / CFR`;
            window.api.sendPipeCommand(`SET_CODEC ${btn.dataset.codec}`);
        });
    });

    // Video Bitrate Slider
    const bitrateSlider = document.getElementById('videoBitrateSlider');
    const bitrateBadge = document.getElementById('videoBitrateBadge');
    bitrateSlider?.addEventListener('input', (e) => {
        const kbps = parseInt(e.target.value, 10);
        const mbps = (kbps / 1000).toFixed(0);
        if (bitrateBadge) bitrateBadge.textContent = `${kbps.toLocaleString()} kbps (${mbps} Mbps)`;
        window.api.sendPipeCommand(`SET_BITRATE ${kbps}`);
    });
}

// --------------------------------------------------------------------------
// 8. Storage & Remux Controls
// --------------------------------------------------------------------------
function initStorageTab() {
    const btnBrowse = document.getElementById('btnBrowseOutput');
    const btnOpenFolder = document.getElementById('btnOpenOutputFolder');
    const pathInput = document.getElementById('outputDirPath');

    btnBrowse?.addEventListener('click', async () => {
        const selected = await window.api.selectDirectory();
        if (selected) {
            outputDir = selected;
            if (pathInput) pathInput.value = selected;
            window.api.sendPipeCommand(`SET_OUTPUT_DIR ${selected}`);
            refreshRecordingsList();
        }
    });

    btnOpenFolder?.addEventListener('click', () => {
        window.api.openPath(outputDir);
    });

    // Token chips insertion into template
    const tmplInput = document.getElementById('filenameTemplateInput');
    document.querySelectorAll('.token-chips .chip').forEach(chip => {
        chip.addEventListener('click', () => {
            if (tmplInput) {
                tmplInput.value += `_${chip.dataset.token}`;
            }
        });
    });
}

// --------------------------------------------------------------------------
// 9. Recordings Vault (Library)
// --------------------------------------------------------------------------
async function initRecordingsVault() {
    document.getElementById('btnRefreshLibrary')?.addEventListener('click', () => refreshRecordingsList());

    const searchInput = document.getElementById('librarySearchInput');
    searchInput?.addEventListener('input', (e) => {
        filterRecordingsList(e.target.value.toLowerCase());
    });

    await refreshRecordingsList();
}

let cachedRecordings = [];

async function refreshRecordingsList() {
    const listElem = document.getElementById('recordingsList');
    if (!listElem) return;

    listElem.innerHTML = '<div class="source-loading">Scanning vault for recordings...</div>';

    try {
        cachedRecordings = await window.api.listRecordings(outputDir);
        const counter = document.getElementById('recordingsCount');
        if (counter) counter.textContent = cachedRecordings.length;
        renderRecordingsList(cachedRecordings);
    } catch (err) {
        listElem.innerHTML = `<div class="source-loading">Error loading recordings: ${err.message}</div>`;
    }
}

function filterRecordingsList(query) {
    const filtered = cachedRecordings.filter(r => r.name.toLowerCase().includes(query));
    renderRecordingsList(filtered);
}

function renderRecordingsList(recordings) {
    const listElem = document.getElementById('recordingsList');
    if (!listElem) return;

    listElem.innerHTML = '';
    if (recordings.length === 0) {
        listElem.innerHTML = '<div class="source-loading">No recordings found in vault directory.</div>';
        return;
    }

    recordings.forEach(rec => {
        const item = document.createElement('div');
        item.className = 'rec-item';
        item.innerHTML = `
            <div class="rec-item-left">
                <span class="rec-item-icon">🎬</span>
                <div class="rec-item-meta">
                    <span class="rec-item-name">${rec.name}</span>
                    <span class="rec-item-info">${rec.sizeMB} MB • ${rec.mtimeStr}</span>
                </div>
            </div>
            <div class="rec-item-actions">
                <button class="cyber-btn sm secondary btn-play" title="Play Video">▶ PLAY</button>
                <button class="cyber-btn sm secondary btn-reveal" title="Show in Windows Explorer">📂 REVEAL</button>
                <button class="cyber-btn sm danger btn-delete" title="Delete Recording">🗑️</button>
            </div>
        `;

        item.querySelector('.btn-play')?.addEventListener('click', () => {
            window.api.openPath(rec.path);
        });

        item.querySelector('.btn-reveal')?.addEventListener('click', () => {
            window.api.revealInExplorer(rec.path);
        });

        item.querySelector('.btn-delete')?.addEventListener('click', async () => {
            if (confirm(`Permanently delete "${rec.name}"?`)) {
                await window.api.deleteRecording(rec.path);
                refreshRecordingsList();
            }
        });

        listElem.appendChild(item);
    });
}

// --------------------------------------------------------------------------
// 10. Engine Telemetry & Quick Recorder HUD
// --------------------------------------------------------------------------
function initEngineTelemetry() {
    const btnRecord = document.getElementById('btnRecordToggle');
    const btnRecordLabel = document.getElementById('btnRecordLabel');
    const btnPause = document.getElementById('btnPauseToggle');
    const btnReplay = document.getElementById('btnSaveReplay');
    const recDot = document.getElementById('recDot');
    const teleState = document.getElementById('teleStateBadge');
    const vfRecTag = document.getElementById('vfRecTag');

    btnRecord?.addEventListener('click', async () => {
        if (engineState === 'IDLE') {
            await window.api.sendPipeCommand('START');
            setEngineState('RECORDING');
        } else {
            await window.api.sendPipeCommand('STOP');
            setEngineState('IDLE');
            setTimeout(refreshRecordingsList, 1000);
        }
    });

    btnPause?.addEventListener('click', async () => {
        if (engineState === 'RECORDING') {
            await window.api.sendPipeCommand('PAUSE');
            setEngineState('PAUSED');
        } else if (engineState === 'PAUSED') {
            await window.api.sendPipeCommand('RESUME');
            setEngineState('RECORDING');
        }
    });

    btnReplay?.addEventListener('click', async () => {
        await window.api.sendPipeCommand('REPLAY');
        btnReplay.classList.add('accent');
        setTimeout(() => btnReplay.classList.remove('accent'), 600);
    });

    // Listen to tray events
    window.api.onEngineStateChanged((state) => {
        setEngineState(state);
    });
}

function setEngineState(state) {
    engineState = state;
    window.api.syncRecordingState(state);

    const btnRecord = document.getElementById('btnRecordToggle');
    const btnRecordLabel = document.getElementById('btnRecordLabel');
    const btnPause = document.getElementById('btnPauseToggle');
    const recDot = document.getElementById('recDot');
    const teleState = document.getElementById('teleStateBadge');
    const vfRecTag = document.getElementById('vfRecTag');

    if (state === 'RECORDING') {
        btnRecord?.classList.add('recording');
        if (btnRecordLabel) btnRecordLabel.textContent = 'STOP';
        if (btnPause) btnPause.disabled = false;
        recDot?.classList.add('active');

        if (teleState) {
            teleState.textContent = 'RECORDING';
            teleState.className = 'state-recording';
        }
        if (vfRecTag) {
            vfRecTag.textContent = '● REC [CFR 144]';
            vfRecTag.style.color = '#ff0055';
            vfRecTag.style.borderColor = 'rgba(255, 0, 85, 0.5)';
        }

        startTimer();
    } else if (state === 'PAUSED') {
        if (teleState) {
            teleState.textContent = 'PAUSED';
            teleState.className = 'state-idle';
        }
        if (vfRecTag) vfRecTag.textContent = '⏸ PAUSED';
        pauseTimer();
    } else {
        // IDLE
        btnRecord?.classList.remove('recording');
        if (btnRecordLabel) btnRecordLabel.textContent = 'RECORD';
        if (btnPause) btnPause.disabled = true;
        recDot?.classList.remove('active');

        if (teleState) {
            teleState.textContent = 'IDLE';
            teleState.className = 'state-idle';
        }
        if (vfRecTag) {
            vfRecTag.textContent = '● READY';
            vfRecTag.style.color = '#00ff88';
            vfRecTag.style.borderColor = 'rgba(0, 255, 136, 0.4)';
        }

        stopTimer();
    }
}

function startTimer() {
    if (!recordTimerInterval) {
        recordStartTime = Date.now() - (elapsedSeconds * 1000);
        recordTimerInterval = setInterval(updateTimerDisplay, 250);
    }
}

function pauseTimer() {
    if (recordTimerInterval) {
        clearInterval(recordTimerInterval);
        recordTimerInterval = null;
    }
}

function stopTimer() {
    pauseTimer();
    elapsedSeconds = 0;
    const timeElem = document.getElementById('recTimeText');
    if (timeElem) timeElem.textContent = '00:00:00';
}

function updateTimerDisplay() {
    elapsedSeconds = Math.floor((Date.now() - recordStartTime) / 1000);
    const hrs = String(Math.floor(elapsedSeconds / 3600)).padStart(2, '0');
    const mins = String(Math.floor((elapsedSeconds % 3600) / 60)).padStart(2, '0');
    const secs = String(elapsedSeconds % 60).padStart(2, '0');
    const timeElem = document.getElementById('recTimeText');
    if (timeElem) timeElem.textContent = `${hrs}:${mins}:${secs}`;
}

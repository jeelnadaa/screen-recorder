const { app, BrowserWindow, Tray, Menu, ipcMain, dialog, shell, desktopCapturer } = require('electron');
const path = require('path');
const net = require('net');
const fs = require('fs');
const { spawn } = require('child_process');

let mainWindow = null;
let tray = null;
let backendProcess = null;
let pipeSocket = null;
const PIPE_NAME = '\\\\.\\pipe\\ScreenRecorderCmd';
const LOGO_ICO = path.join(__dirname, '..', 'assets', 'app_logo.ico');
const LOGO_PNG = path.join(__dirname, '..', 'assets', 'app_logo.png');

// State tracking
let recordingState = 'IDLE'; // IDLE, RECORDING, PAUSED
let isMicMuted = false;

function ensureBackendRunning() {
    // Attempt connecting to the named pipe
    const probe = net.createConnection(PIPE_NAME, () => {
        console.log('[IPC] Connected to C++ ScreenRecorder engine.');
        probe.end();
    });

    probe.on('error', () => {
        console.log('[IPC] C++ ScreenRecorder engine not detected on pipe. Launching in background...');
        const exePath = path.join(__dirname, '..', 'build', 'bin', 'Release', 'ScreenRecorder.exe');
        if (fs.existsSync(exePath)) {
            try {
                backendProcess = spawn(exePath, ['--minimized'], {
                    detached: true,
                    stdio: 'ignore'
                });
                backendProcess.unref();
                console.log('[IPC] Launched ScreenRecorder.exe in background.');
            } catch (err) {
                console.error('[IPC] Failed to auto-launch ScreenRecorder.exe:', err);
            }
        }
    });
}

function sendPipeCommand(cmd) {
    return new Promise((resolve) => {
        const client = net.createConnection(PIPE_NAME, () => {
            client.write(cmd);
        });

        client.on('data', (data) => {
            try {
                const response = JSON.parse(data.toString());
                resolve(response);
            } catch {
                resolve({ status: 'OK', raw: data.toString().trim() });
            }
            client.end();
        });

        client.on('error', (err) => {
            resolve({ status: 'OFFLINE', error: err.message });
        });

        setTimeout(() => {
            try { client.destroy(); } catch {}
            resolve({ status: 'TIMEOUT' });
        }, 1500);
    });
}

function createTray() {
    if (tray) return;

    const trayIconPath = fs.existsSync(LOGO_ICO) ? LOGO_ICO : (fs.existsSync(LOGO_PNG) ? LOGO_PNG : null);
    if (!trayIconPath) {
        console.warn('Tray icon file not found');
        return;
    }

    tray = new Tray(trayIconPath);
    tray.setToolTip('CYBERREC // Pro Screen Recorder');

    updateTrayMenu();

    tray.on('click', () => {
        if (mainWindow) {
            if (mainWindow.isVisible()) {
                if (mainWindow.isMinimized()) mainWindow.restore();
                mainWindow.focus();
            } else {
                mainWindow.show();
                mainWindow.focus();
            }
        }
    });
}

function updateTrayMenu() {
    if (!tray) return;

    const isRec = (recordingState === 'RECORDING');
    const isPaused = (recordingState === 'PAUSED');

    const contextMenu = Menu.buildFromTemplate([
        {
            label: isRec ? '⏹️  Stop Recording' : '🔴  Start Recording',
            click: async () => {
                if (isRec) {
                    await sendPipeCommand('STOP');
                    recordingState = 'IDLE';
                } else {
                    await sendPipeCommand('START');
                    recordingState = 'RECORDING';
                }
                updateTrayMenu();
                if (mainWindow && !mainWindow.isDestroyed()) {
                    mainWindow.webContents.send('engine-state-changed', recordingState);
                }
            }
        },
        {
            label: isPaused ? '▶️  Resume Recording' : '⏸️  Pause Recording',
            enabled: isRec || isPaused,
            click: async () => {
                if (isPaused) {
                    await sendPipeCommand('RESUME');
                    recordingState = 'RECORDING';
                } else {
                    await sendPipeCommand('PAUSE');
                    recordingState = 'PAUSED';
                }
                updateTrayMenu();
                if (mainWindow && !mainWindow.isDestroyed()) {
                    mainWindow.webContents.send('engine-state-changed', recordingState);
                }
            }
        },
        {
            label: '⚡  Save Instant Replay (Ctrl+Shift+S)',
            click: async () => {
                await sendPipeCommand('REPLAY');
                if (tray) {
                    tray.displayBalloon({
                        title: 'CyberRec Replay',
                        content: 'Instant replay buffer saved successfully!'
                    });
                }
            }
        },
        { type: 'separator' },
        {
            label: isMicMuted ? '🎙️  Unmute Microphone' : '🔇  Mute Microphone',
            click: async () => {
                isMicMuted = !isMicMuted;
                await sendPipeCommand(isMicMuted ? 'MIC_MUTE' : 'MIC_UNMUTE');
                updateTrayMenu();
                if (mainWindow && !mainWindow.isDestroyed()) {
                    mainWindow.webContents.send('mic-mute-changed', isMicMuted);
                }
            }
        },
        {
            label: '📂  Open Recordings Folder',
            click: () => {
                const capturesDir = path.join(app.getPath('videos'), 'Captures');
                if (fs.existsSync(capturesDir)) {
                    shell.openPath(capturesDir);
                } else {
                    shell.openPath(app.getPath('videos'));
                }
            }
        },
        { type: 'separator' },
        {
            label: '⚙️  Open Studio Dashboard',
            click: () => {
                if (mainWindow) {
                    mainWindow.show();
                    mainWindow.focus();
                }
            }
        },
        {
            label: '❌  Exit CyberRec',
            click: () => {
                app.isQuitting = true;
                app.quit();
            }
        }
    ]);

    tray.setContextMenu(contextMenu);
}

function createWindow() {
    mainWindow = new BrowserWindow({
        width: 1240,
        height: 820,
        minWidth: 1040,
        minHeight: 680,
        frame: false, // Custom frameless maximalist titlebar
        backgroundColor: '#07090e',
        icon: fs.existsSync(LOGO_ICO) ? LOGO_ICO : LOGO_PNG,
        webPreferences: {
            preload: path.join(__dirname, 'preload.js'),
            nodeIntegration: false,
            contextIsolation: true,
            sandbox: false
        }
    });

    mainWindow.loadFile(path.join(__dirname, 'index.html'));

    mainWindow.on('close', (event) => {
        if (!app.isQuitting) {
            event.preventDefault();
            mainWindow.hide();
            if (tray) {
                tray.displayBalloon({
                    title: 'CyberRec Pro',
                    content: 'App minimized to system tray. Right-click the icon to control recording.'
                });
            }
        }
    });
}

// App lifecycle
app.whenReady().then(() => {
    ensureBackendRunning();
    createWindow();
    createTray();

    app.on('activate', () => {
        if (BrowserWindow.getAllWindows().length === 0) createWindow();
    });
});

app.on('window-all-closed', () => {
    if (process.platform !== 'darwin') {
        // Keep running in tray unless explicit quit
    }
});

// IPC handlers for UI interactions
ipcMain.handle('get-capture-sources', async () => {
    try {
        const sources = await desktopCapturer.getSources({
            types: ['screen', 'window'],
            thumbnailSize: { width: 360, height: 202 },
            fetchWindowIcons: true
        });

        return sources.map(s => ({
            id: s.id,
            name: s.name,
            thumbnail: s.thumbnail.toDataURL(),
            appIcon: s.appIcon ? s.appIcon.toDataURL() : null,
            isScreen: s.id.startsWith('screen:')
        }));
    } catch (err) {
        console.error('Failed to get capture sources:', err);
        return [];
    }
});

ipcMain.handle('send-pipe-command', async (event, cmd) => {
    return await sendPipeCommand(cmd);
});

ipcMain.handle('select-directory', async () => {
    const res = await dialog.showOpenDialog(mainWindow, {
        properties: ['openDirectory', 'createDirectory'],
        title: 'Select Destination Directory'
    });
    if (!res.canceled && res.filePaths.length > 0) {
        return res.filePaths[0];
    }
    return null;
});

ipcMain.handle('open-path', async (event, targetPath) => {
    if (targetPath) {
        shell.openPath(targetPath);
    }
});

ipcMain.handle('reveal-in-explorer', async (event, filePath) => {
    if (filePath && fs.existsSync(filePath)) {
        shell.showItemInFolder(filePath);
    }
});

ipcMain.handle('list-recordings', async (event, dirPath) => {
    const targetDir = dirPath || path.join(app.getPath('videos'), 'Captures');
    if (!fs.existsSync(targetDir)) return [];

    try {
        const files = fs.readdirSync(targetDir);
        const videoFiles = files
            .filter(f => f.endsWith('.mp4') || f.endsWith('.mkv'))
            .map(f => {
                const fullPath = path.join(targetDir, f);
                const stats = fs.statSync(fullPath);
                return {
                    name: f,
                    path: fullPath,
                    sizeBytes: stats.size,
                    sizeMB: (stats.size / (1024 * 1024)).toFixed(1),
                    mtime: stats.mtime,
                    mtimeStr: stats.mtime.toLocaleString()
                };
            })
            .sort((a, b) => b.mtime - a.mtime);

        return videoFiles;
    } catch (err) {
        console.error('Failed to list recordings:', err);
        return [];
    }
});

ipcMain.handle('delete-recording', async (event, filePath) => {
    try {
        if (fs.existsSync(filePath)) {
            fs.unlinkSync(filePath);
            return { success: true };
        }
        return { success: false, error: 'File does not exist' };
    } catch (err) {
        return { success: false, error: err.message };
    }
});

ipcMain.on('window-minimize', () => {
    if (mainWindow) mainWindow.minimize();
});

ipcMain.on('window-maximize', () => {
    if (mainWindow) {
        if (mainWindow.isMaximized()) {
            mainWindow.unmaximize();
        } else {
            mainWindow.maximize();
        }
    }
});

ipcMain.on('window-close', () => {
    if (mainWindow) mainWindow.close();
});

ipcMain.on('sync-recording-state', (event, state) => {
    recordingState = state;
    updateTrayMenu();
});

app.on('before-quit', () => {
    app.isQuitting = true;
    if (backendProcess) {
        try { backendProcess.kill(); } catch {}
    }
});

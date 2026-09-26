const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('api', {
    getCaptureSources: () => ipcRenderer.invoke('get-capture-sources'),
    sendPipeCommand: (cmd) => ipcRenderer.invoke('send-pipe-command', cmd),
    selectDirectory: () => ipcRenderer.invoke('select-directory'),
    openPath: (targetPath) => ipcRenderer.invoke('open-path', targetPath),
    revealInExplorer: (filePath) => ipcRenderer.invoke('reveal-in-explorer', filePath),
    listRecordings: (dirPath) => ipcRenderer.invoke('list-recordings', dirPath),
    deleteRecording: (filePath) => ipcRenderer.invoke('delete-recording', filePath),
    minimize: () => ipcRenderer.send('window-minimize'),
    maximize: () => ipcRenderer.send('window-maximize'),
    close: () => ipcRenderer.send('window-close'),
    syncRecordingState: (state) => ipcRenderer.send('sync-recording-state', state),
    onEngineStateChanged: (callback) => {
        ipcRenderer.on('engine-state-changed', (event, state) => callback(state));
    },
    onMicMuteChanged: (callback) => {
        ipcRenderer.on('mic-mute-changed', (event, isMuted) => callback(isMuted));
    }
});

console.log('fsmanager.js loaded successfully');

let currentFolder = '/';

document.addEventListener('DOMContentLoaded', function() {
    console.log('DOMContentLoaded event fired');
    
    // Wait for displayManager's WebSocket to be established
    const checkWsInterval = setInterval(() => {
        if (window.ws && ws.readyState === WebSocket.OPEN) {
            console.log('Using existing WebSocket connection');
            clearInterval(checkWsInterval);
            
            // Add message handler for file system operations
            const originalOnMessage = ws.onmessage;
            ws.onmessage = function(event) {
                const data = JSON.parse(event.data);
                if (data.type === 'triggerUpload') {
                    console.log('Triggering file input click');
                    document.getElementById('fileInput').click();
                } else if (data.type === 'createFolder') {
                    console.log('Triggering create folder');
                    createFolder();
                } else if (data.type === 'reboot') {
                    console.log('Triggering reboot');
                    reboot();
                } else if (data.type === 'triggerFileList') {
                    console.log('Triggering file list refresh');
                    loadFileList();
                }
                // Call original message handler
                if (originalOnMessage) {
                    originalOnMessage(event);
                }
            };
        }
    }, 100);
});

function uploadFile(file) {
    if (!file) {
        console.log('No file selected');
        return;
    }
    
    console.log('Starting upload for file:', file.name);
    const formData = new FormData();
    formData.append('file', file);
    
    const xhr = new XMLHttpRequest();
    xhr.open('POST', '/fsm/upload', true);
    
    xhr.upload.onprogress = function(e) {
        if (e.lengthComputable) {
            const percentComplete = (e.loaded / e.total) * 100;
            console.log('Upload progress: ' + percentComplete.toFixed(2) + '%');
        }
    };
    
    xhr.onload = function() {
        if (xhr.status === 200) {
            console.log('Upload completed successfully');
            loadFileList();
        } else {
            console.error('Upload failed with status:', xhr.status);
            console.error('Response:', xhr.responseText);
        }
    };
    
    xhr.onerror = function() {
        console.error('Upload failed due to network error');
    };
    
    console.log('Sending upload request...');
    xhr.send(formData);
}

function loadFileList() {
    console.log('Loading file list for folder:', currentFolder);
    var xhr = new XMLHttpRequest();
    xhr.open('GET', '/fsm/filelist?folder=' + currentFolder, true);
    
    xhr.onload = function() {
        if (xhr.status === 200) {
            console.log('File list received successfully');
            var data = JSON.parse(xhr.responseText);
            
            var fileListElement = document.getElementById('fileList');
            if (!fileListElement) {
                console.error('fileList element not found in DOM');
                return;
            }
            fileListElement.innerHTML = '';

            // Add folder name header
            var headerItem = document.createElement('div');
            headerItem.classList.add('dM_file-list-header');
            headerItem.textContent = currentFolder === '/' ? 'Root' : currentFolder.slice(0, -1).split('/').pop();
            fileListElement.appendChild(headerItem);
            
            // Create arrays for folders and files
            var folders = data.files.filter(function(file) { return file.isDir; });
            var files = data.files.filter(function(file) { return !file.isDir; });

            console.log('Found folders:', folders.length, 'files:', files.length);

            // Sort folders and files alphabetically
            folders.sort(function(a, b) { return a.name.localeCompare(b.name); });
            files.sort(function(a, b) { return a.name.localeCompare(b.name); });

            var itemCount = 0;

            if (currentFolder !== '/') {
                itemCount++;
                var backItem = document.createElement('li');
                backItem.classList.add('dM_file-item');
                backItem.innerHTML = '<span style="cursor: pointer" onclick="navigateUp()"><span class="dM_folder-icon">[F]</span>..</span><span class="dM_size"></span><span></span><span></span>';
                backItem.style.backgroundColor = itemCount % 2 === 0 ? '#f5f5f5' : '#fafafa';
                fileListElement.appendChild(backItem);
            }

            // Add folders
            for (var i = 0; i < folders.length; i++) {
                var folder = folders[i];
                itemCount++;
                var fileItem = document.createElement('li');
                fileItem.classList.add('dM_file-item');
                
                // Create closure to preserve folder name
                (function(folder, fileItem, itemCount) {
                    // Check if folder is empty
                    var checkXhr = new XMLHttpRequest();
                    checkXhr.open('GET', '/fsm/filelist?folder=' + currentFolder + folder.name + '/', true);
                    
                    checkXhr.onload = function() {
                        var isEmpty = checkXhr.status === 200 && 
                                   (!JSON.parse(checkXhr.responseText).files || 
                                    JSON.parse(checkXhr.responseText).files.length === 0);
                        
                        fileItem.innerHTML = '<span style="cursor: pointer" onclick="openFolder(\'' + folder.name + '\')"><span class="dM_folder-icon">[F]</span>' + folder.name + '</span><span class="dM_size"></span><span></span><button class="dM_delete" onclick="deleteFolder(\'' + folder.name + '\')" ' + (!isEmpty ? 'disabled' : '') + '>Delete</button>';
                        fileItem.style.backgroundColor = itemCount % 2 === 0 ? '#f5f5f5' : '#fafafa';
                        fileListElement.appendChild(fileItem);
                    };
                    
                    checkXhr.send();
                })(folder, fileItem, itemCount);
            }

            // Add files
            for (var i = 0; i < files.length; i++) {
                var file = files[i];
                itemCount++;
                var fileItem = document.createElement('li');
                fileItem.classList.add('dM_file-item');
                fileItem.innerHTML = '<span>[D] ' + file.name + '</span><span class="dM_size">' + formatSize(file.size) + '</span><button onclick="downloadFile(\'' + file.name + '\')">Download</button><button class="dM_delete" onclick="deleteFile(\'' + file.name + '\')">Delete</button>';
                fileItem.style.backgroundColor = itemCount % 2 === 0 ? '#f5f5f5' : '#fafafa';
                fileListElement.appendChild(fileItem);
            }

            // Update space information
            var spaceInfo = document.getElementById('spaceInfo');
            if (spaceInfo) {
                var availableSpace = data.totalSpace - data.usedSpace;
                spaceInfo.textContent = 'FileSystem uses ' + formatSize(data.usedSpace) + ' of ' + formatSize(data.totalSpace) + ' (' + formatSize(availableSpace) + ' available)';
            } else {
                console.error('spaceInfo element not found in DOM');
            }
        } else {
            console.error('Failed to load file list, status:', xhr.status);
        }
    };
    
    xhr.onerror = function() {
        console.error('Failed to load file list');
    };
    
    xhr.send();
}

function navigateUp() {
    console.log('Navigating up from:', currentFolder);
    var oldFolder = currentFolder;
    currentFolder = currentFolder.split('/').slice(0, -2).join('/') + '/';
    if (currentFolder === '') currentFolder = '/';
    console.log('New folder:', currentFolder);
    loadFileList();
}

function openFolder(folderName) {
    console.log('Opening folder:', folderName);
    var oldFolder = currentFolder;
    if (currentFolder === '/') {
        currentFolder = folderName;
    } else {
        var base = currentFolder.endsWith('/') ? currentFolder.slice(0, -1) : currentFolder;
        currentFolder = base + '/' + folderName;
    }
    if (!currentFolder.startsWith('/')) currentFolder = '/' + currentFolder;
    if (!currentFolder.endsWith('/')) currentFolder += '/';
    console.log('New folder path:', currentFolder);
    loadFileList();
}

function deleteFolder(folderName) {
    console.log('Attempting to delete folder:', folderName);
    // Check if folder is empty
    var checkXhr = new XMLHttpRequest();
    checkXhr.open('GET', '/fsm/filelist?folder=' + currentFolder + folderName + '/', true);
    
    checkXhr.onload = function() {
        if (checkXhr.status === 200) {
            var data = JSON.parse(checkXhr.responseText);
            
            if (data.files && data.files.length > 0) {
                console.error('Cannot delete folder: Folder is not empty');
                return;
            }

            if (!confirm('Are you sure you want to delete the folder "' + folderName + '"?')) return;

            console.log('Sending delete folder request');
            var deleteXhr = new XMLHttpRequest();
            deleteXhr.open('POST', '/fsm/deleteFolder', true);
            deleteXhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
            
            deleteXhr.onload = function() {
                if (deleteXhr.status === 200) {
                    console.log('Folder deleted successfully');
                    loadFileList();
                } else {
                    console.error('Failed to delete folder, status:', deleteXhr.status);
                }
            };
            
            deleteXhr.onerror = function() {
                console.error('Failed to delete folder');
            };
            
            deleteXhr.send('folder=' + encodeURIComponent(folderName));
        }
    };
    
    checkXhr.onerror = function() {
        console.error('Failed to check folder');
    };
    
    checkXhr.send();
}

function downloadFile(fileName) {
    console.log('Downloading file:', fileName);
    var xhr = new XMLHttpRequest();
    xhr.open('GET', '/fsm/download?file=' + encodeURIComponent(currentFolder + fileName), true);
    xhr.responseType = 'blob';
    
    xhr.onload = function() {
        if (xhr.status === 200) {
            console.log('File downloaded successfully');
            var url = window.URL.createObjectURL(xhr.response);
            var a = document.createElement('a');
            a.href = url;
            a.download = fileName;
            document.body.appendChild(a);
            a.click();
            window.URL.revokeObjectURL(url);
            document.body.removeChild(a);
        } else {
            console.error('Failed to download file, status:', xhr.status);
        }
    };
    
    xhr.onerror = function() {
        console.error('Failed to download file');
    };
    
    xhr.send();
}

function deleteFile(fileName) {
    console.log('Attempting to delete file:', fileName);
    if (!confirm('Are you sure you want to delete "' + fileName + '"?')) return;

    var xhr = new XMLHttpRequest();
    xhr.open('POST', '/fsm/delete', true);
    xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
    
    xhr.onload = function() {
        if (xhr.status === 200) {
            console.log('File deleted successfully');
            loadFileList();
        } else {
            console.error('Failed to delete file, status:', xhr.status);
        }
    };
    
    xhr.onerror = function() {
        console.error('Failed to delete file');
    };
    
    console.log('Sending delete file request');
    xhr.send('file=' + encodeURIComponent(currentFolder + fileName));
}

function formatSize(size) {
    if (size >= 1048576) {
        return (size / 1048576).toFixed(2) + ' MB';
    } else if (size >= 1024) {
        return (size / 1024).toFixed(2) + ' KB';
    } else {
        return size + ' B';
    }
}

function createFolder() {
    console.log('Creating new folder');
    const folderName = prompt('Enter folder name:');
    if (!folderName) return;

    const xhr = new XMLHttpRequest();
    xhr.open('POST', '/fsm/createFolder', true);
    xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
    
    xhr.onload = function() {
        if (xhr.status === 200) {
            console.log('Folder created successfully');
            loadFileList();
        } else {
            console.error('Failed to create folder, status:', xhr.status);
        }
    };
    
    xhr.onerror = function() {
        console.error('Failed to create folder');
    };
    
    console.log('Sending create folder request');
    xhr.send('name=' + encodeURIComponent(folderName));
}

function reboot() {
    if (!confirm('Are you sure you want to reboot the device?')) return;

    console.log('Rebooting device...');
    const xhr = new XMLHttpRequest();
    xhr.open('POST', '/fsm/reboot', true);
    
    xhr.onload = function() {
        if (xhr.status === 200) {
            console.log('Reboot command sent successfully');
            alert('Device is rebooting...');
            // Reload page after delay to reconnect
            setTimeout(() => window.location.reload(), 5000);
        } else {
            console.error('Failed to reboot, status:', xhr.status);
        }
    };
    
    xhr.onerror = function() {
        console.error('Failed to reboot');
    };
    
    xhr.send();
}

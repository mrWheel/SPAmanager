//----- espTicker32.js -----

let isRequestingRssfeedSettings = false;
let isRequestingParolaSettings = false;
let isRequestingNeopixelsSettings = false;
let isRequestingWeerliveSettings = false;
let isRequestingDeviceSettings = false;
let renderDebounceTimer = null;
let lastReceivedData = null;
let deviceSettings = null;
let parolaSettings = null;
let neopixelsSettings = null;
let weerliveSettings = null;
let mediasstackSettings = null;

// Array to store input field values
let LocalMessages = [];

// Function to check if the page is ready for input fields
function isPageReadyForLocalMessages() 
{
  // Check if the inputTableBody element exists
  const tableBody = document.getElementById('inputTableBody');
  return !!tableBody;
} // isPageReadyForLocalMessages()


// Function to initialize the input fields from JSON
function initializeLocalMessages(jsonString) 
{
  console.log('initializeLocalMessages called with:', jsonString);
  try {
    LocalMessages = JSON.parse(jsonString) || [];
    renderLocalMessages();
  } catch (e) {
    console.error('Error parsing JSON:', e);
    LocalMessages = [];
  }
}

// Function to render the input fields in the table
function renderLocalMessages() 
{
  console.log('renderLocalMessages called');
  
  // Check if the page is ready
  if (!isPageReadyForLocalMessages()) {
    console.error('Input table body not found in DOM, page not ready yet');
    return;
  }
  
  const tableBody = document.getElementById('inputTableBody');
  tableBody.innerHTML = '';
  
  LocalMessages.forEach((value, index) => {
    const row = document.createElement('tr');
    const cell = document.createElement('td');
    cell.style.padding = '8px';
    
    // Create a container for the dropdown, input and buttons
    const container = document.createElement('div');
    container.style.display = 'flex';
    container.style.alignItems = 'center';
    container.style.width = '100%';
    
    // Create the key dropdown selector
    const keySelector = document.createElement('select');
    keySelector.style.marginRight = '8px';
    keySelector.dataset.index = index;
    
    // Add options A through J
    for (let charCode = 65; charCode <= 74; charCode++) {
      const option = document.createElement('option');
      const keyChar = String.fromCharCode(charCode);
      option.value = keyChar;
      option.textContent = keyChar;
      keySelector.appendChild(option);
    }
    
    // Set the current value or default to 'A'
    const messageObj = typeof value === 'object' ? value : { key: 'A', content: value };
    keySelector.value = messageObj.key || 'A';
    
    // Add event listener for key changes
    keySelector.addEventListener('change', (e) => {
      const index = e.target.dataset.index;
      // Ensure LocalMessages[index] is an object
      if (typeof LocalMessages[index] !== 'object') {
        LocalMessages[index] = { key: 'A', content: LocalMessages[index] || '' };
      }
      LocalMessages[index].key = e.target.value;
    });
    
    // Add the key selector to the container
    container.appendChild(keySelector);
    
    // Create the input field for content
    const input = document.createElement('input');
    input.type = 'text';
    input.value = messageObj.content || messageObj;
    input.style.width = '100ch'; // Set to exactly 100ch wide
    input.style.maxWidth = '100ch';
    input.style.fontFamily = "'Courier New', Courier, monospace"; // Set monospace font
    input.maxLength = 150;
    input.dataset.index = index;
    input.addEventListener('input', (e) => {
      const index = e.target.dataset.index;
      // Ensure LocalMessages[index] is an object
      if (typeof LocalMessages[index] !== 'object') {
        LocalMessages[index] = { key: 'A', content: '' };
      }
      LocalMessages[index].content = e.target.value;
    });
    
    // Add the input to the container
    container.appendChild(input);
    
    // Create a button container for better alignment
    const buttonContainer = document.createElement('div');
    buttonContainer.style.display = 'flex';
    buttonContainer.style.marginLeft = '8px';
    
    // Create the "+" button
    const addButton = document.createElement('button');
    addButton.textContent = '+';
    addButton.title = 'Add empty message below';
    addButton.style.marginRight = '4px';
    addButton.addEventListener('click', () => {
      addMessageBelow(index);
    });
    
    // Create the "Up" button
    const upButton = document.createElement('button');
    upButton.textContent = 'Up';
    upButton.title = 'Move message up';
    upButton.style.marginRight = '4px';
    upButton.disabled = index === 0; // Disable if it's the first message
    upButton.addEventListener('click', () => {
      moveMessageUp(index);
    });
    
    // Create the "Down" button
    const downButton = document.createElement('button');
    downButton.textContent = 'Down';
    downButton.title = 'Move message down';
    downButton.style.marginRight = '4px';
    downButton.disabled = index === LocalMessages.length - 1; // Disable if it's the last message
    downButton.addEventListener('click', () => {
      moveMessageDown(index);
    });
    
    // Create the "-" button
    const removeButton = document.createElement('button');
    removeButton.textContent = '-';
    removeButton.title = 'Remove message';
    removeButton.addEventListener('click', () => {
      removeMessage(index);
    });
    
    // Add the buttons to the button container
    buttonContainer.appendChild(addButton);
    buttonContainer.appendChild(upButton);
    buttonContainer.appendChild(downButton);
    buttonContainer.appendChild(removeButton);
    
    // Add the button container to the main container
    container.appendChild(buttonContainer);
    
    // Add the container to the cell
    cell.appendChild(container);
    row.appendChild(cell);
    tableBody.appendChild(row);
  });

} // renderLocalMessages()


// Function to add an empty message below the specified index
function addMessageBelow(index) 
{
  console.log(`Adding empty message below index ${index}`);
  
  // Get the key from the message above
  let keyToUse = 'A'; // Default to 'A' if we can't determine the key
  
  if (index >= 0 && index < LocalMessages.length) {
    const message = LocalMessages[index];
    if (typeof message === 'object' && message !== null && message.key) {
      keyToUse = message.key;
    }
  }
  
  // Add a new message with the same key and "<empty>" content
  LocalMessages.splice(index + 1, 0, { key: keyToUse, content: "<empty>" });
  renderLocalMessages();
}




// Function to move a message up one position
function moveMessageUp(index) 
{
  console.log(`Moving message at index ${index} up`);
  if (index > 0) {
    // Swap with the message above
    const temp = LocalMessages[index];
    LocalMessages[index] = LocalMessages[index - 1];
    LocalMessages[index - 1] = temp;
    renderLocalMessages();
  }
}

// Function to move a message down one position
function moveMessageDown(index) 
{
  console.log(`Moving message at index ${index} down`);
  if (index < LocalMessages.length - 1) {
    // Swap with the message below
    const temp = LocalMessages[index];
    LocalMessages[index] = LocalMessages[index + 1];
    LocalMessages[index + 1] = temp;
    renderLocalMessages();
  }
}

// Function to remove a message
function removeMessage(index) 
{
  console.log(`Removing message at index ${index}`);
  LocalMessages.splice(index, 1);
  renderLocalMessages();
}
// Function to add a new input field
function addInputField() 
{
  console.log('addInputField called');
  LocalMessages.push('');
  renderLocalMessages();
}

// Function to save input fields via WebSocket
function saveLocalMessages() 
{
  console.log('saveLocalMessages called');
  if (window.ws && window.ws.readyState === WebSocket.OPEN) 
  {
    window.ws.send(JSON.stringify({
      type: 'process',
      processType: 'saveLocalMessages',
      inputValues: { 'LocalMessagesData': JSON.stringify(LocalMessages) }
    }));
  } else {
    console.error('WebSocket is not connected');
  }
}

// This function is called by SPAmanager to check if the script is loaded
function isEspTicker32Loaded() 
{
  console.log("isEspTicker32Loaded called");
  
  // Check if we already have a WebSocket connection
  if (!window.ws || window.ws.readyState !== WebSocket.OPEN) 
  {
    console.log("WebSocket not available or not open, checking global ws variable");
    
    // Try to use the global ws variable from SPAmanager.js
    if (typeof ws !== 'undefined' && ws.readyState === WebSocket.OPEN) {
      console.log("Using global ws variable from SPAmanager");
      window.ws = ws;
    } else {
      console.log("WebSocket not available yet, waiting...");
      setTimeout(isEspTicker32Loaded, 100);
      return true;
    }
  }
  
  console.log("WebSocket is available, setting up message handler");
  
  // Set up WebSocket message handler
  window.ws.addEventListener('message', (event) => {
    try {
      console.log("WebSocket message received:", event.data.substring(0, 50) + "...");
      
      // Try to parse the message as JSON
      const data = JSON.parse(event.data);
      
      // Check if this is our custom LocalMessagesData message
      if (data.type === 'custom' && data.action === 'LocalMessagesData') {
        console.log('Received input fields data');
        
        // Initialize with the data
        initializeLocalMessages(data.data);
      }
      // Check if this is our custom deviceSettingsData message
      else if (data.type === 'custom' && data.action === 'deviceSettingsData') {
        console.log('Received device settings data');
        
        // Reset the request flag
        isRequestingDeviceSettings = false;
        
        // Store the data and debounce the rendering
        lastReceivedData = {
          type: 'deviceSettings',
          data: data.data
        };
        
        // Debounce the rendering
        clearTimeout(renderDebounceTimer);
        renderDebounceTimer = setTimeout(() => {
          // Initialize with the data
          initializeDeviceSettings(lastReceivedData.data);
        }, 100);
      }
      // Check if this is our custom parolaSettingsData message
      else if (data.type === 'custom' && data.action === 'parolaSettingsData') {
        console.log('Received parola settings data');
        
        // Reset the request flag
        isRequestingParolaSettings = false;
        
        // Store the data and debounce the rendering
        lastReceivedData = {
          type: 'parolaSettings',
          data: data.data
        };
        
        // Debounce the rendering
        clearTimeout(renderDebounceTimer);
        renderDebounceTimer = setTimeout(() => {
          // Initialize with the data
          initializeParolaSettings(lastReceivedData.data);
        }, 100);
      }
      // Check if this is our custom neopixelsSettingsData message
      else if (data.type === 'custom' && data.action === 'neopixelsSettingsData') {
        console.log('Received neopixels settings data');
        
        // Reset the request flag
        isRequestingNeopixelsSettings = false;
        
        // Store the data and debounce the rendering
        lastReceivedData = {
          type: 'neopixelsSettings',
          data: data.data
        };
        
        // Debounce the rendering
        clearTimeout(renderDebounceTimer);
        renderDebounceTimer = setTimeout(() => {
          // Initialize with the data
          initializeNeopixelsSettings(lastReceivedData.data);
        }, 100);
      }
      // Check if this is our custom weerliveSettingsData message
      else if (data.type === 'custom' && data.action === 'weerliveSettingsData') {
        console.log('Received weerlive settings data');
        
        // Reset the request flag
        isRequestingWeerliveSettings = false;
        
        // Store the data and debounce the rendering
        lastReceivedData = {
          type: 'weerliveSettings',
          data: data.data
        };
        
        // Debounce the rendering
        clearTimeout(renderDebounceTimer);
        renderDebounceTimer = setTimeout(() => {
          // Initialize with the data
          initializeWeerliveSettings(lastReceivedData.data);
        }, 100);
      }
      else if (data.type === 'custom' && data.action === 'rssfeedSettingsData') 
      {
          console.log('Received rssfeed settings data');
          
          // Reset the request flag
          isRequestingRssfeedSettings = false;
          
          // Store the data and debounce the rendering
          lastReceivedData = {
            type: 'rssfeedSettings',
            data: data.data
          };
          
          // Debounce the rendering
          clearTimeout(renderDebounceTimer);
          renderDebounceTimer = setTimeout(() => {
            // Initialize with the data
            initializeRssfeedSettings(lastReceivedData.data);
          }, 100);
      }
      // Also check for direct JSON arrays for backward compatibility
      else if (event.data.startsWith('[') && event.data.endsWith(']')) {
        console.log('Received direct input fields data');
        // Try to initialize immediately
        initializeLocalMessages(event.data);
      }
    } catch (e) {
      console.error('Error handling WebSocket message:', e);
      
      // If parsing as JSON fails, check if it's a direct JSON array
      if (event.data.startsWith('[') && event.data.endsWith(']')) {
        console.log('Received direct input fields data');
        
        // Try to initialize immediately
        initializeLocalMessages(event.data);
      }
    }
  });
  
  // Get the current page title to determine which page we're on
  const pageTitle = document.getElementById('title').textContent;
  console.log("Current page title:", pageTitle);
  
  // Check if this is the main page with the scrolling monitor
  if (pageTitle.includes("Main") && isPageReadyForScrollingMonitor()) 
  {
    console.log("Main page is ready with scrolling monitor");
    // If there are any queued messages, process them
    if (messageQueue.length > 0 && !isDisplayingMessage) {
      processNextMessage();
    }
  }

  // Check if the page is ready for input fields (only for Messages page)
  if (pageTitle.includes("Messages") && isPageReadyForLocalMessages()) 
  {
    console.log("Page is ready for input fields, requesting data from server");
    requestLocalMessages();
  }
  
  // Check if the page is ready for device settings (only for Device Settings page)
  if (pageTitle.includes("Device Settings") && isPageReadyForDeviceSettings() && !isRequestingDeviceSettings) 
  {
    console.log("Page is ready for device settings, requesting data from server");
    isRequestingDeviceSettings = true;
    requestDeviceSettings();
  }

  // Check if the page is ready for parola settings (only for Parola Settings page)
  if (pageTitle.includes("Parola Settings") && isPageReadyForParolaSettings() && !isRequestingParolaSettings) 
  {
    console.log("Page is ready for parola settings, requesting data from server");
    isRequestingParolaSettings = true;
    requestParolaSettings();
  }

  // Check if the page is ready for neopixels settings (only for Neopixels Settings page)
  if (pageTitle.includes("Neopixels Settings") && isPageReadyForNeopixelsSettings() && !isRequestingNeopixelsSettings) 
    {
      console.log("Page is ready for neopixels settings, requesting data from server");
      isRequestingNeopixelsSettings = true;
      requestNeopixelsSettings();
    }
    
  // Check if the page is ready for weerlive settings (only for Weerlive Settings page)
  if (pageTitle.includes("Weerlive Settings") && isPageReadyForWeerliveSettings() && !isRequestingWeerliveSettings) 
  {
    console.log("Page is ready for weerlive settings, requesting data from server");
    isRequestingWeerliveSettings = true;
    requestWeerliveSettings();
  }
  // Check if the page is ready for rssfeed settings (only for RSSfeed Settings page)
  if (pageTitle.includes("RSSfeed Settings") && isPageReadyForRssfeedSettings() && !isRequestingRssfeedSettings) 
    {
      console.log("Page is ready for rssfeed settings, requesting data from server");
      isRequestingRssfeedSettings = true;
      requestRssfeedSettings();
    }  
    return true;

} // isEspTicker32Loaded()


// Function to request input fields data from the server
function requestLocalMessages() 
{
  console.log("Requesting input fields data from server");
  window.ws.send(JSON.stringify({
    type: 'requestLocalMessages'
  }));

} // requestLocalMessages()

// Function to check if the page is ready for weerlive settings
function isPageReadyForWeerliveSettings() 
{
  // Check if the settingsTableBody element exists
  const tableBody = document.getElementById('settingsTableBody');
  return !!tableBody;

} //  isPageReadyForWeerliveSettings()

// Function to initialize the weerlive settings from JSON
function initializeWeerliveSettings(jsonString) 
{
  console.log('initializeWeerliveSettings called with:', jsonString);
  try {
    weerliveSettings = JSON.parse(jsonString) || { fields: [] };
    renderWeerliveSettings();
  } catch (e) {
    console.error('Error parsing JSON:', e);
    weerliveSettings = { fields: [] };
  }
} // initializeWeerliveSettings()

// Function to render the weerlive settings in the table
function renderWeerliveSettings() 
{
  console.log('renderWeerliveSettings called');
  
  // Check if the page is ready
  if (!isPageReadyForWeerliveSettings()) {
    console.error('Weerlive settings table body not found in DOM, page not ready yet');
    return;
  }
  
  const tableBody = document.getElementById('settingsTableBody');
  tableBody.innerHTML = '';
  
  if (weerliveSettings && weerliveSettings.fields) {
    weerliveSettings.fields.forEach((field) => {
      const row = document.createElement('tr');
      
      // Field prompt cell
      const promptCell = document.createElement('td');
      promptCell.style.padding = '8px';
      promptCell.style.textAlign = 'right'; 
      promptCell.textContent = field.fieldPrompt;
      
      // Field value cell
      const valueCell = document.createElement('td');
      valueCell.style.padding = '8px';
      
      // Create input element based on field type
      const input = document.createElement('input');
      if (field.fieldType === 's') {
        // String input
        input.type = 'text';
        input.value = field.fieldValue;
        input.maxLength = field.fieldLen;
      } else if (field.fieldType === 'n') {
        // Numeric input
        input.type = 'number';
        input.value = field.fieldValue;
        input.min = field.fieldMin;
        input.max = field.fieldMax;
        input.step = field.fieldStep;
        
        input.dataset.fieldName = field.fieldName;  // ADD THIS LINE
        input.dataset.fieldType = field.fieldType;  // ADD THIS LINE
        input.addEventListener('input', updateWeerliveSettings);  // ADD THIS LINE

        // Create a container for the input and range text
        const container = document.createElement('div');
        container.style.display = 'flex';
        container.style.alignItems = 'center';
        
        // Add the input to the container
        container.appendChild(input);
        
        // Add the range text
        const rangeText = document.createElement('span');
        rangeText.textContent = ` (${field.fieldMin} .. ${field.fieldMax})`;
        rangeText.style.marginLeft = '8px';
        rangeText.style.fontSize = '0.9em';
        rangeText.style.color = '#666';
        container.appendChild(rangeText);
        
        // Add the container to the cell instead of just the input
        valueCell.appendChild(container);
        row.appendChild(promptCell);
        row.appendChild(valueCell);
        tableBody.appendChild(row);
        
        // Skip the rest of this iteration since we've already added everything
        return;
      }
      
      input.style.width = '100%';
      input.dataset.fieldName = field.fieldName;
      input.dataset.fieldType = field.fieldType;
      input.addEventListener('input', updateWeerliveSettings);
      
      valueCell.appendChild(input);
      row.appendChild(promptCell);
      row.appendChild(valueCell);
      tableBody.appendChild(row);
    });
  }
  
  // Update the settings name
  const settingsNameElement = document.getElementById('settingsName');
  if (settingsNameElement) {
    settingsNameElement.textContent = 'Weerlive Settings';
  }

} // renderWeerliveSettings()


// Function to update a weerlive setting
function updateWeerliveSettings(event) 
{
  const input = event.target || this;
  const fieldName = input.dataset.fieldName;
  const fieldType = input.dataset.fieldType;
  const value = fieldType === 'n' ? parseFloat(input.value) : input.value;
  
  console.log(`Updating weerlive setting: ${fieldName} = ${value}`);
  
  // Find and update the field in the weerliveSettings object
  if (weerliveSettings && weerliveSettings.fields) {
    const field = weerliveSettings.fields.find(f => f.fieldName === fieldName);
    if (field) {
      field.fieldValue = value;
    }
  }

} // updateWeerliveSettings()


// Function to request weerlive settings data from the server
function requestWeerliveSettings() 
{
  console.log("Requesting weerlive settings data from server");
  window.ws.send(JSON.stringify({
    type: 'requestWeerliveSettings'
  }));

} // requestWeerliveSettings()


//===========[Rssfeed]==================================================================

// Function to check if the page is ready for rssfeed settings
function isPageReadyForRssfeedSettings() 
{
  // Check if the settingsTableBody element exists
  const tableBody = document.getElementById('settingsTableBody');
  return !!tableBody;
}

// Function to initialize the rssfeed settings from JSON
function initializeRssfeedSettings(jsonString) 
{
  console.log('initializeRssfeedSettings called with:', jsonString);
  try {
    rssfeedSettings = JSON.parse(jsonString) || { fields: [] };
    renderRssfeedSettings();
  } catch (e) {
    console.error('Error parsing JSON:', e);
    rssfeedSettings = { fields: [] };
  }
}

// Function to render the rssfeed settings in the table
function renderRssfeedSettings() 
{
  console.log('renderRssfeedSettings called');
  
  // Check if the page is ready
  if (!isPageReadyForRssfeedSettings()) {
    console.error('Rssfeed settings table body not found in DOM, page not ready yet');
    return;
  }
  
  const tableBody = document.getElementById('settingsTableBody');
  tableBody.innerHTML = '';
  
  if (rssfeedSettings && rssfeedSettings.fields) {
    rssfeedSettings.fields.forEach((field) => {
      const row = document.createElement('tr');
      
      // Field prompt cell
      const promptCell = document.createElement('td');
      promptCell.style.padding = '8px';
      promptCell.style.textAlign = 'right'; 
      promptCell.textContent = field.fieldPrompt;
      
      // Field value cell
      const valueCell = document.createElement('td');
      valueCell.style.padding = '8px';
      
      // Create input element based on field type
      const input = document.createElement('input');
      if (field.fieldType === 's') {
        // String input
        input.type = 'text';
        input.value = field.fieldValue;
        input.maxLength = field.fieldLen;
      } else if (field.fieldType === 'n') {
        // Numeric input
        input.type = 'number';
        input.value = field.fieldValue;
        input.min = field.fieldMin;
        input.max = field.fieldMax;
        input.step = field.fieldStep;
        
        input.dataset.fieldName = field.fieldName;
        input.dataset.fieldType = field.fieldType;
        input.addEventListener('input', updateRssfeedSettings);

        // Create a container for the input and range text
        const container = document.createElement('div');
        container.style.display = 'flex';
        container.style.alignItems = 'center';
        
        // Add the input to the container
        container.appendChild(input);
        
        // Add the range text
        const rangeText = document.createElement('span');
        rangeText.textContent = ` (${field.fieldMin} .. ${field.fieldMax})`;
        rangeText.style.marginLeft = '8px';
        rangeText.style.fontSize = '0.9em';
        rangeText.style.color = '#666';
        container.appendChild(rangeText);
        
        // Add the container to the cell instead of just the input
        valueCell.appendChild(container);
        row.appendChild(promptCell);
        row.appendChild(valueCell);
        tableBody.appendChild(row);
        
        // Skip the rest of this iteration since we've already added everything
        return;
      }
      
      input.style.width = '100%';
      input.dataset.fieldName = field.fieldName;
      input.dataset.fieldType = field.fieldType;
      input.addEventListener('input', updateRssfeedSettings);
      
      valueCell.appendChild(input);
      row.appendChild(promptCell);
      row.appendChild(valueCell);
      tableBody.appendChild(row);
    });
  }
  
  // Update the settings name
  const settingsNameElement = document.getElementById('settingsName');
  if (settingsNameElement) {
    settingsNameElement.textContent = 'RSSfeed Settings';
  }
}

// Function to update a rssfeed setting
function updateRssfeedSettings(event) 
{
  const input = event.target || this;
  const fieldName = input.dataset.fieldName;
  const fieldType = input.dataset.fieldType;
  const value = fieldType === 'n' ? parseFloat(input.value) : input.value;
  
  console.log(`Updating rssfeed setting: ${fieldName} = ${value}`);
  
  // Find and update the field in the rssfeedSettings object
  if (rssfeedSettings && rssfeedSettings.fields) {
    const field = rssfeedSettings.fields.find(f => f.fieldName === fieldName);
    if (field) {
      field.fieldValue = value;
    }
  }
}

// Function to request rssfeed settings data from the server
function requestRssfeedSettings() 
{
  console.log("Requesting rssfeed settings data from server");
  window.ws.send(JSON.stringify({
    type: 'requestRssfeedSettings'
  }));
}


//===========[Parola]==================================================================
// Function to check if the page is ready for parola settings
function isPageReadyForParolaSettings() 
{
  // Check if the settingsTableBody element exists
  const tableBody = document.getElementById('settingsTableBody');
  return !!tableBody;

} // isPageReadyForParolaSettings()

// Function to initialize the parola settings from JSON
function initializeParolaSettings(jsonString) 
{
  console.log('initializeParolaSettings called with:', jsonString);
  try {
    parolaSettings = JSON.parse(jsonString) || { fields: [] };
    renderParolaSettings();
  } catch (e) {
    console.error('Error parsing JSON:', e);
    parolaSettings = { fields: [] };
  }

} // initializeParolaSettings()


// Function to render the parola settings in the table
function renderParolaSettings() 
{
  console.log('renderParolaSettings called');
  
  // Check if the page is ready
  if (!isPageReadyForParolaSettings()) {
    console.error('parola settings table body not found in DOM, page not ready yet');
    return;
  }
  
  const tableBody = document.getElementById('settingsTableBody');
  
  // Clear the table body
  tableBody.innerHTML = '';
  
  if (parolaSettings && parolaSettings.fields) {
    parolaSettings.fields.forEach((field) => {
      const row = document.createElement('tr');
      
      // Field prompt cell
      const promptCell = document.createElement('td');
      promptCell.style.padding = '8px';
      promptCell.style.textAlign = 'right'; 
      promptCell.textContent = field.fieldPrompt;
      
      // Field value cell
      const valueCell = document.createElement('td');
      valueCell.style.padding = '8px';
      
      // Create input element based on field type
      const input = document.createElement('input');
      if (field.fieldType === 's') {
        // String input
        input.type = 'text';
        input.value = field.fieldValue;
        input.maxLength = field.fieldLen;
      } else if (field.fieldType === 'n') {
        // Numeric input
        input.type = 'number';
        input.value = field.fieldValue;
        input.min = field.fieldMin;
        input.max = field.fieldMax;
        input.step = field.fieldStep;
        
        input.dataset.fieldName = field.fieldName;
        input.dataset.fieldType = field.fieldType;
        input.addEventListener('input', updateParolaSettings);

        // Create a container for the input and range text
        const container = document.createElement('div');
        container.style.display = 'flex';
        container.style.alignItems = 'center';
        
        // Add the input to the container
        container.appendChild(input);
        
        // Add the range text
        const rangeText = document.createElement('span');
        rangeText.textContent = ` (${field.fieldMin} .. ${field.fieldMax})`;
        rangeText.style.marginLeft = '8px';
        rangeText.style.fontSize = '0.9em';
        rangeText.style.color = '#666';
        container.appendChild(rangeText);
        
        // Add the container to the cell instead of just the input
        valueCell.appendChild(container);
        row.appendChild(promptCell);
        row.appendChild(valueCell);
        tableBody.appendChild(row);
        
        // Skip the rest of this iteration since we've already added everything
        return;
      }
      
      input.style.width = '100%';
      input.dataset.fieldName = field.fieldName;
      input.dataset.fieldType = field.fieldType;
      input.addEventListener('input', updateParolaSettings);
      
      valueCell.appendChild(input);
      row.appendChild(promptCell);
      row.appendChild(valueCell);
      tableBody.appendChild(row);
    });
  }
  
  // Update the settings name
  const settingsNameElement = document.getElementById('settingsName');
  if (settingsNameElement) {
    settingsNameElement.textContent = 'Parola Settings';
  }
} // renderParolaSettings()

// Function to update a parola setting
function updateParolaSettings(event) 
{
  const input = event.target || this;
  const fieldName = input.dataset.fieldName;
  const fieldType = input.dataset.fieldType;
  const value = fieldType === 'n' ? parseFloat(input.value) : input.value;
  
  console.log(`Updating parola setting: ${fieldName} = ${value}`);
  
  // Find and update the field in the parolaSettings object
  if (parolaSettings && parolaSettings.fields) {
    const field = parolaSettings.fields.find(f => f.fieldName === fieldName);
    if (field) {
      field.fieldValue = value;
    }
  }
} // updateParolaSettings()


// Function to request parola settings data from the server
function requestParolaSettings() 
{
  console.log("Requesting parola settings data from server");
  window.ws.send(JSON.stringify({
    type: 'requestParolaSettings'
  }));

} // requestParolaSettings()


//===========[Neopixels]==================================================================
// Function to check if the page is ready for neopixels settings
function isPageReadyForNeopixelsSettings() 
{
  // Check if the settingsTableBody element exists
  const tableBody = document.getElementById('settingsTableBody');
  return !!tableBody;

} // isPageReadyForNeopixelsSettings()

// Function to initialize the neopixels settings from JSON
function initializeNeopixelsSettings(jsonString) 
{
  console.log('initializeNeopixelsSettings called with:', jsonString);
  try {
    neopixelsSettings = JSON.parse(jsonString) || { fields: [] };
    renderNeopixelsSettings();
  } catch (e) {
    console.error('Error parsing JSON:', e);
    neopixelsSettings = { fields: [] };
  }

} // initializeNeopixelsSettings()


// Function to render the neopixels settings in the table
function renderNeopixelsSettings() 
{
  console.log('renderNeopixelsSettings called');
  
  // Check if the page is ready
  if (!isPageReadyForNeopixelsSettings()) {
    console.error('neopixels settings table body not found in DOM, page not ready yet');
    return;
  }
  
  const tableBody = document.getElementById('settingsTableBody');
  
  // Clear the table body
  tableBody.innerHTML = '';
  
  if (neopixelsSettings && neopixelsSettings.fields) {
    neopixelsSettings.fields.forEach((field) => {
      const row = document.createElement('tr');
      
      // Field prompt cell
      const promptCell = document.createElement('td');
      promptCell.style.padding = '8px';
      promptCell.style.textAlign = 'right'; 
      promptCell.textContent = field.fieldPrompt;
      
      // Field value cell
      const valueCell = document.createElement('td');
      valueCell.style.padding = '8px';
      
      // Create input element based on field type
      if (field.fieldType === 's') {
        // String input
        const input = document.createElement('input');
        input.type = 'text';
        input.value = field.fieldValue;
        input.maxLength = field.fieldLen;
        input.style.width = '100%';
        input.dataset.fieldName = field.fieldName;
        input.dataset.fieldType = field.fieldType;
        input.addEventListener('input', updateNeopixelsSettings);
        
        valueCell.appendChild(input);

      } else if (field.fieldType === 'n') {
        // Numeric input
        const input = document.createElement('input');
        input.type = 'number';
        input.value = field.fieldValue;
        input.min = field.fieldMin;
        input.max = field.fieldMax;
        input.step = field.fieldStep;
        
        input.dataset.fieldName = field.fieldName;
        input.dataset.fieldType = field.fieldType;
        input.addEventListener('input', updateNeopixelsSettings);

        // Create a container for the input and range text
        const container = document.createElement('div');
        container.style.display = 'flex';
        container.style.alignItems = 'center';
        
        // Add the input to the container
        container.appendChild(input);
        
        // Add the range text
        const rangeText = document.createElement('span');
        rangeText.textContent = ` (${field.fieldMin} .. ${field.fieldMax})`;
        rangeText.style.marginLeft = '8px';
        rangeText.style.fontSize = '0.9em';
        rangeText.style.color = '#666';
        container.appendChild(rangeText);
        
        // Add the container to the cell
        valueCell.appendChild(container);

      } else if (field.fieldType === 'b') {
        // Boolean input (checkbox)
        const container = document.createElement('div');
        container.style.display = 'flex';
        container.style.alignItems = 'center';
        
        // Create checkbox input
        const input = document.createElement('input');
        input.type = 'checkbox';
        input.checked = field.fieldValue;
        console.log("Checkbox value:", field.fieldValue);
        input.dataset.fieldName = field.fieldName;
        input.dataset.fieldType = field.fieldType;
        input.addEventListener('change', updateNeopixelsSettings);
        
        // Add the checkbox to the container
        container.appendChild(input);
        
        // Add a label to explain the boolean values
        const label = document.createElement('span');
        label.textContent = field.fieldValue ? ' true' : ' false';
        label.style.marginLeft = '8px';
        container.appendChild(label);
        
        // Update the label when the checkbox changes
        input.addEventListener('change', function() {
          label.textContent = this.checked ? ' true' : ' false';
        });
        
        // Add the container to the cell
        valueCell.appendChild(container);
      }
      
      row.appendChild(promptCell);
      row.appendChild(valueCell);
      tableBody.appendChild(row);
    });
  }
  
  // Update the settings name
  const settingsNameElement = document.getElementById('settingsName');
  if (settingsNameElement) {
    settingsNameElement.textContent = 'Neopixels Settings';
  }
} // renderNeopixelsSettings()


// Function to update a neopixels setting
function updateNeopixelsSettings(event) 
{
  const input = event.target || this;
  const fieldName = input.dataset.fieldName;
  const fieldType = input.dataset.fieldType;
  
  // Use different properties based on field type
  let value;
  if (fieldType === 'n') {
    value = parseFloat(input.value);
  } else if (fieldType === 'b') {
    value = input.checked; // Use checked property for boolean fields
    console.log("Checkbox value:", value);
  } else {
    value = input.value;
  }
  
  console.log(`Updating neopixels setting: ${fieldName} = ${value}`);
  
  // Find and update the field in the neopixelsSettings object
  if (neopixelsSettings && neopixelsSettings.fields) {
    const field = neopixelsSettings.fields.find(f => f.fieldName === fieldName);
    if (field) {
      field.fieldValue = value;
    }
  }
} // updateNeopixelsSettings()


// Function to request neopixels settings data from the server
function requestNeopixelsSettings() 
{
  console.log("Requesting neopixels settings data from server");
  window.ws.send(JSON.stringify({
    type: 'requestNeopixelsSettings'
  }));

} // requestNeopixelsSettings()



//===========[Device Settings]=========================================================
// Function to check if the page is ready for device settings
function isPageReadyForDeviceSettings() 
{
  // Check if the settingsTableBody element exists
  const tableBody = document.getElementById('settingsTableBody');
  return !!tableBody;
} // isPageReadyForDeviceSettings()

// Function to initialize the device settings from JSON
function initializeDeviceSettings(jsonString) 
{
  console.log('initializeDeviceSettings called with:', jsonString);
  try {
    deviceSettings = JSON.parse(jsonString) || { fields: [] };
    renderDeviceSettings();
  } catch (e) {
    console.error('Error parsing JSON:', e);
    deviceSettings = { fields: [] };
  }

} //  initializeDeviceSettings()

// Function to render the device settings in the table
function renderDeviceSettings() 
{
  console.log('renderDeviceSettings called');
  
  // Check if the page is ready
  if (!isPageReadyForDeviceSettings()) {
    console.error('Device settings table body not found in DOM, page not ready yet');
    return;
  }
  
  const tableBody = document.getElementById('settingsTableBody');
  tableBody.innerHTML = '';
  
  if (deviceSettings && deviceSettings.fields) {
    deviceSettings.fields.forEach((field) => {
      const row = document.createElement('tr');
      
      // Field prompt cell
      const promptCell = document.createElement('td');
      promptCell.style.padding = '8px';
      promptCell.style.textAlign = 'right'; 
      promptCell.textContent = field.fieldPrompt;
      
      // Field value cell
      const valueCell = document.createElement('td');
      valueCell.style.padding = '8px';
      
      // Create input element based on field type
      if (field.fieldType === 's') {
        // String input
        const input = document.createElement('input');
        input.type = 'text';
        input.value = field.fieldValue;
        input.maxLength = field.fieldLen;
        input.style.width = '100%';
        input.dataset.fieldName = field.fieldName;
        input.dataset.fieldType = field.fieldType;
        input.addEventListener('input', updateDeviceSetting);
        
        valueCell.appendChild(input);

      } else if (field.fieldType === 'n') {
        // Numeric input
        const input = document.createElement('input');
        input.type = 'number';
        input.value = field.fieldValue;
        input.min = field.fieldMin;
        input.max = field.fieldMax;
        input.step = field.fieldStep;
        
        input.dataset.fieldName = field.fieldName;
        input.dataset.fieldType = field.fieldType;
        input.addEventListener('input', updateDeviceSetting);

        // Create a container for the input and range text
        const container = document.createElement('div');
        container.style.display = 'flex';
        container.style.alignItems = 'center';
        
        // Add the input to the container
        container.appendChild(input);
        
        // Add the range text
        const rangeText = document.createElement('span');
        rangeText.textContent = ` (${field.fieldMin} .. ${field.fieldMax})`;
        rangeText.style.marginLeft = '8px';
        rangeText.style.fontSize = '0.9em';
        rangeText.style.color = '#666';
        container.appendChild(rangeText);
        
        // Add the container to the cell
        valueCell.appendChild(container);

      } else if (field.fieldType === 'b') {
        // Boolean input (checkbox)
        const container = document.createElement('div');
        container.style.display = 'flex';
        container.style.alignItems = 'center';
        
        // Create checkbox input
        const input = document.createElement('input');
        input.type = 'checkbox';
        input.checked = field.fieldValue;
        console.log("Checkbox value:", field.fieldValue);
        input.dataset.fieldName = field.fieldName;
        input.dataset.fieldType = field.fieldType;
        input.addEventListener('change', updateDeviceSetting);
        
        // Add the checkbox to the container
        container.appendChild(input);
        
        // Add a label to explain the boolean values
        const label = document.createElement('span');
        label.textContent = field.fieldValue ? ' true' : ' false';
        label.style.marginLeft = '8px';
        container.appendChild(label);
        
        // Update the label when the checkbox changes
        input.addEventListener('change', function() {
          label.textContent = this.checked ? ' true' : ' false';
        });
        
        // Add the container to the cell
        valueCell.appendChild(container);
      }
      
      row.appendChild(promptCell);
      row.appendChild(valueCell);
      tableBody.appendChild(row);
    });
  }
  // Update the settings name
  const settingsNameElement = document.getElementById('settingsName');
  if (settingsNameElement) {
    settingsNameElement.textContent = 'Device Settings';
  }
} // renderDeviceSettings()

// Function to update a device setting
function updateDeviceSetting(event) 
{
  const input = event.target || this;
  const fieldName = input.dataset.fieldName;
  const fieldType = input.dataset.fieldType;
  
  // Use different properties based on field type
  let value;
  if (fieldType === 'n') {
    value = parseFloat(input.value);
  } else if (fieldType === 'b') {
    value = input.checked; // Use checked property for boolean fields
    console.log("Checkbox value:", value);
  } else {
    value = input.value;
  }
  
  console.log(`Updating device setting: ${fieldName} = ${value}`);
  
  // Find and update the field in the deviceSettings object
  if (deviceSettings && deviceSettings.fields) {
    const field = deviceSettings.fields.find(f => f.fieldName === fieldName);
    if (field) {
      field.fieldValue = value;
    }
  }
} //  updateDeviceSetting()

// Function to request device settings data from the server
function requestDeviceSettings() 
{
  console.log("Requesting device settings data from server");
  window.ws.send(JSON.stringify({
    type: 'requestDeviceSettings'
  }));
} //  requestDeviceSettings()

// Function to set the correct save function based on the settings page
function saveSettings() 
{
  const settingsName = document.getElementById('settingsName').textContent;
  
  console.log('saveSettings called for:', settingsName);
  
  // Determine which settings object to use based on the settingsName
  let settingsObj = null;
  let processType = '';
  let dataKey = '';
  
  if (settingsName === 'Device Settings') {
    settingsObj = deviceSettings;
    processType = 'saveDeviceSettings';
    dataKey = 'deviceSettingsData';
  } else if (settingsName === 'Parola Settings') {
    settingsObj = parolaSettings;
    processType = 'saveParolaSettings';
    dataKey = 'parolaSettingsData';
  } else if (settingsName === 'Neopixels Settings') {
    settingsObj = neopixelsSettings;
    processType = 'saveNeopixelsSettings';
    dataKey = 'neopixelsSettingsData';
  } else if (settingsName === 'Weerlive Settings') {
    settingsObj = weerliveSettings;
    processType = 'saveWeerliveSettings';
    dataKey = 'weerliveSettingsData';
  } else if (settingsName === 'RSSfeed Settings') {
    settingsObj = rssfeedSettings;
    processType = 'saveRssfeedSettings';
    dataKey = 'rssfeedSettingsData';
  } else {
    console.error('Unknown settings type:', settingsName);
    return;
  }
  
  if (window.ws && window.ws.readyState === WebSocket.OPEN && settingsObj) {
    // Create a copy of the settings object with the correct structure
    const formattedSettings = {
      fields: settingsObj.fields.map(field => {
        let value = field.fieldValue;
        
        // Convert boolean values to "on" or "off" strings
        if (field.fieldType === 'b') {
          value = value ? "true" : "false";
        }
        
        return {
          fieldName: field.fieldName,
          value: value
        };
      })
    };
    
    const inputValues = {};
    inputValues[dataKey] = JSON.stringify(formattedSettings);
    
    window.ws.send(JSON.stringify({
      type: 'process',
      processType: processType,
      inputValues: inputValues
    }));
  } else {
    console.error('WebSocket is not connected or settings object is null');
  }

} // saveSettings()

// Variable to track if a message is currently being displayed
let isDisplayingMessage = false;
// Queue to store pending messages
let messageQueue = [];

// Function to check if the page is ready for scrolling monitor
function isPageReadyForScrollingMonitor() 
{
  // Check if the scrollingMonitor element exists
  const monitor = document.getElementById('scrollingMonitor');
  return !!monitor;

} // isPageReadyForScrollingMonitor()

function queueMessageToMonitor(text) 
{
  // Check if text is undefined, null, or empty
  if (!text) 
  {
    console.log("queueMessageToMonitor called with empty or undefined text");
    return; // Exit the function early
  }

  // Check if the page is ready for the scrolling monitor
  if (!isPageReadyForScrollingMonitor()) 
  {
    console.error('Scrolling monitor not found in DOM, page not ready yet');
    return;
  }
  
  // Add the message to the queue
  messageQueue.push(text);
  
  // If we're not already displaying a message, start displaying
  if (!isDisplayingMessage) {
    processNextMessage();
  }
} // queueMessageToMonitor()

// Function to process the next message in the queue
async function processNextMessage() 
{
  // If the queue is empty, we're done
  if (messageQueue.length === 0) 
  {
    isDisplayingMessage = false;
    return;
  }
  
  // Check if the page is ready for the scrolling monitor
  if (!isPageReadyForScrollingMonitor()) 
  {
    console.error('Scrolling monitor not found in DOM, page not ready yet');
    isDisplayingMessage = false;
    return;
  }

  // Mark that we're displaying a message
  isDisplayingMessage = true;
  
  const monitor = document.getElementById("scrollingMonitor");
  const maxWidth = 80; // 80 characters per line
  const delay = 30; // delay between each character in ms
  
  // Calculate visible lines based on CSS properties
  // height: 18em, line-height: 1.3em, padding: 0.5em (top) + 0.5em (bottom)
  // Available height = 18em - 1em (padding) = 17em
  // Number of lines = 17em / 1.3em = ~13 lines
  const visibleLines = 13; // Based on the CSS properties
  
  // Get the next message from the queue
  const text = messageQueue.shift();
  
  // Split text into words, keeping spaces with the preceding word
  const words = text.split(/\s+|\r|\n/).map(w => w + ' ').filter(Boolean);
  let buffer = [];
  let currentLine = "";
  
  // Word wrapping logic
  for (let i = 0; i < words.length; i++) {
    const word = words[i];
    
    // Handle newlines (if a word ends with \n or \r)
    if (word.endsWith("\n ") || word.endsWith("\r ")) {
      // Add the word without the newline and space
      currentLine += word.slice(0, -2);
      buffer.push(currentLine);
      currentLine = "";
      continue;
    }
    
    // If the word fits on the current line, add it
    if (currentLine.length + word.length <= maxWidth) {
      currentLine += word;
    } 
    // If the word doesn't fit but the line is not empty, start a new line
    else if (currentLine.length > 0) {
      buffer.push(currentLine);
      currentLine = word;
    } 
    // If the word doesn't fit and the line is empty, we have to split the word
    else {
      // Handle words longer than maxWidth
      let remainingWord = word;
      while (remainingWord.length > maxWidth) {
        buffer.push(remainingWord.slice(0, maxWidth));
        remainingWord = remainingWord.slice(maxWidth);
      }
      currentLine = remainingWord;
    }
    
    // If line is exactly maxWidth, flush it
    if (currentLine.length >= maxWidth) {
      buffer.push(currentLine);
      currentLine = "";
    }
  }
  
  // Flush any remaining content
  if (currentLine) buffer.push(currentLine);
  
  // Initialize the monitor if it's empty
  if (!monitor.textContent.trim()) {
    // Fill with empty lines to position text at the bottom
    monitor.textContent = Array(visibleLines).fill("").join("\n");
  }
  
  // Get current content and split into lines
  let contentLines = monitor.textContent.split("\n");
  
  // If we have more lines than we need, trim the excess
  if (contentLines.length > visibleLines) {
    contentLines = contentLines.slice(contentLines.length - visibleLines);
  }
  
  // If we have fewer lines than we need, add empty lines at the top
  while (contentLines.length < visibleLines) {
    contentLines.unshift("");
  }
  
  // Process each line from our buffer
  for (let i = 0; i < buffer.length; i++) {
    const line = buffer[i];
    
    // Check if there are more messages in the queue and this is the last line
    const skipAnimation = messageQueue.length > 0 && i === buffer.length - 1;
    
    // Shift all lines up by one
    for (let j = 0; j < contentLines.length - 1; j++) {
      contentLines[j] = contentLines[j + 1];
    }
    
    // Clear the bottom line for new content
    contentLines[contentLines.length - 1] = "";
    
    if (skipAnimation) {
      // If we need to skip animation, just set the line immediately
      contentLines[contentLines.length - 1] = line;
      monitor.textContent = contentLines.join("\n");
    } else {
      // Otherwise animate character by character
      let displayedLine = "";
      
      // Type each character with animation
      for (let char of line) {
        displayedLine += char;
        
        // Update the bottom line of the content
        contentLines[contentLines.length - 1] = displayedLine;
        
        // Join back and update the monitor
        monitor.textContent = contentLines.join("\n");
        
        // Check if a new message has arrived
        if (messageQueue.length > 0) {
          // If so, finish this line immediately
          contentLines[contentLines.length - 1] = line;
          monitor.textContent = contentLines.join("\n");
          break;
        }
        
        // Wait for the animation delay
        await new Promise(r => setTimeout(r, delay));
      }
    }
  }
  
  // Process the next message if there is one
  processNextMessage();
  
} // processNextMessage()


// Update the isEspTicker32Loaded function to handle device settings
function isEspTicker32Loaded() 
{
  console.log("isEspTicker32Loaded called");
  
  // Check if we already have a WebSocket connection
  if (!window.ws || window.ws.readyState !== WebSocket.OPEN) 
    {
    console.log("WebSocket not available or not open, checking global ws variable");
    
    // Try to use the global ws variable from SPAmanager.js
    if (typeof ws !== 'undefined' && ws.readyState === WebSocket.OPEN) 
      {
      console.log("Using global ws variable from SPAmanager");
      window.ws = ws;
    } else {
      console.log("WebSocket not available yet, waiting...");
      setTimeout(isEspTicker32Loaded, 100);
      return true;
    }
  }
  
  console.log("WebSocket is available, setting up message handler");
  
  // Set up WebSocket message handler
  window.ws.addEventListener('message', (event) => {
    try {
      console.log("WebSocket message received:", event.data.substring(0, 50) + "...");
      
      // Try to parse the message as JSON
      const data = JSON.parse(event.data);
      
      // Check if this is our custom LocalMessagesData message
      if (data.type === 'custom' && data.action === 'LocalMessagesData') {
        console.log('Received input fields data');
        
        // Initialize with the data
        initializeLocalMessages(data.data);
      }
      // Check if this is our custom deviceSettingsData message
      else if (data.type === 'custom' && data.action === 'deviceSettingsData') {
        console.log('Received device settings data');
        
        // Initialize with the data
        initializeDeviceSettings(data.data);
      }
      // Check if this is our custom parolaSettingsData message
      else if (data.type === 'custom' && data.action === 'parolaSettingsData') {
        console.log('Received parola settings data');
        // Initialize with the data
        initializeParolaSettings(data.data);
      }
      // Check if this is our custom neopixelsSettingsData message
      else if (data.type === 'custom' && data.action === 'neopixelsSettingsData') {
        console.log('Received neopixels settings data');
        // Initialize with the data
        initializeNeopixelsSettings(data.data);
      }
      // Check if this is our custom weerliveSettingsData message
      else if (data.type === 'custom' && data.action === 'weerliveSettingsData') {
        console.log('Received weerlive settings data');
        // Initialize with the data
        initializeWeerliveSettings(data.data);
      }
      else if (data.type === 'custom' && data.action === 'rssfeedSettingsData') {
        console.log('Received rssfeed settings data');
        // Initialize with the data
        initializeRssfeedSettings(data.data);
      }
      // Also check for direct JSON arrays for backward compatibility
      else if (event.data.startsWith('[') && event.data.endsWith(']')) {
        console.log('Received direct input fields data');
        // Try to initialize immediately
        initializeLocalMessages(event.data);
      }
    } catch (e) {
      console.error('Error handling WebSocket message:', e);
      
      // If parsing as JSON fails, check if it's a direct JSON array
      if (event.data.startsWith('[') && event.data.endsWith(']')) {
        console.log('Received direct input fields data');
        
        // Try to initialize immediately
        initializeLocalMessages(event.data);
      }
    }
  });
  
  // Get the current page title to determine which page we're on
  const pageTitle = document.getElementById('title').textContent;
  console.log("Current page title:", pageTitle);
  
  // Check if this is the main page with the scrolling monitor
  if (pageTitle.includes("Main") && isPageReadyForScrollingMonitor()) 
  {
    console.log("Main page is ready with scrolling monitor");
    // If there are any queued messages, process them
    if (messageQueue.length > 0 && !isDisplayingMessage) {
      processNextMessage();
    }
  }

  // Check if the page is ready for input fields (only for Messages page)
  if (pageTitle.includes("Messages") && isPageReadyForLocalMessages()) 
  {
    console.log("Page is ready for input fields, requesting data from server");
    requestLocalMessages();
  }
  
  // Check if the page is ready for device settings (only for Device Settings page)
  if (pageTitle.includes("Device Settings") && isPageReadyForDeviceSettings()) 
  {
    console.log("Page is ready for device settings, requesting data from server");
    requestDeviceSettings();
  }

  // Check if the page is ready for parola settings (only for Parola Settings page)
  if (pageTitle.includes("Parola Settings") && isPageReadyForParolaSettings()) 
  {
    console.log("Page is ready for parola settings, requesting data from server");
    requestParolaSettings();
  }

  // Check if the page is ready for parola settings (only for Neopixels Settings page)
  if (pageTitle.includes("Neopixels Settings") && isPageReadyForNeopixelsSettings()) 
    {
      console.log("Page is ready for neopixels settings, requesting data from server");
      requestNeopixelsSettings();
    }
    
  // Check if the page is ready for weerlive settings (only for Weerlive Settings page)
  if (pageTitle.includes("Weerlive Settings") && isPageReadyForWeerliveSettings()) 
  {
    console.log("Page is ready for weerlive settings, requesting data from server");
    requestWeerliveSettings();
  }
  // Check if the page is ready for rssfeed settings (only for RSSfeed Settings page)
  if (pageTitle.includes("RSSfeed Settings") && isPageReadyForRssfeedSettings()) 
    {
      console.log("Page is ready for rssfeed settings, requesting data from server");
      requestRssfeedSettings();
    }  
    return true;

} // isEspTicker32Loaded()

// Log that the script has loaded
console.log("espTicker32.js has loaded");

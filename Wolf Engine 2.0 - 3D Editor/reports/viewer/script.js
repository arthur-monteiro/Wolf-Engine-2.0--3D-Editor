document.getElementById('jsonInput').addEventListener('change', function(e) {
    const reader = new FileReader();
    reader.onload = function(event) {
        try {
            const data = JSON.parse(event.target.result);
            renderData(data);
        } catch (err) {
            console.error("Error parsing JSON:", err);
            alert("Failed to load JSON file. Check console for details.");
        }
    };
    reader.readAsText(e.target.files[0]);
});

window.addEventListener('DOMContentLoaded', () => {
    setStandbyMode();
});

const systemColors = {};
let colorIndex = 0;

function getSystemColor(systemName) {
    if (systemName === "Unknown system") {
        return "hsl(0, 100%, 50%)";
    }

    if (systemColors[systemName]) return systemColors[systemName];

    const goldenRatioConjugate = 0.618033988749895;
    
    let hash = 0;
    for (let i = 0; i < systemName.length; i++) {
        hash = systemName.charCodeAt(i) + ((hash << 5) - hash);
    }
    
    colorIndex += goldenRatioConjugate;
    colorIndex %= 1; 

    const h = Math.floor(colorIndex * 360);
    const finalHue = (h < 15 || h > 345) ? (h + 30) % 360 : h;

    const s = 75 + (Math.abs(hash) % 15);
    const l = 50 + (Math.abs(hash) % 10); 
    
    const color = `hsl(${finalHue}, ${s}%, ${l}%)`;
    systemColors[systemName] = color;
    return color;
}

function extractSystem(name) {
    const regex = /\(([^:]+)::.*\)/;
    const match = name.match(regex);
    
    if (match && match[1]) {
        return formatSystemName(match[1]);
    }
    
    return "Unknown system"; 
}

function formatSystemName(name) {
    const spaced = name.replace(/([a-z])([A-Z])/g, '$1 $2').toLowerCase();
    return spaced.charAt(0).toUpperCase() + spaced.slice(1);
}

let currentData = null;

document.getElementById('clearSelection').addEventListener('click', (e) => {
    e.stopPropagation();
    
    const selectedRows = document.querySelectorAll('#resourceTable tbody tr.selected');
    selectedRows.forEach(row => row.classList.remove('selected'));
    
    updateSelectionStats();
});

function updateSelectionStats() {
    const selectedRows = document.querySelectorAll('#resourceTable tbody tr.selected');
    const bubble = document.getElementById('selectionBubble');
    const totalDisplay = document.getElementById('selectedTotal');
    const countDisplay = document.getElementById('selectedCount');

    if (selectedRows.length > 0) {
        let totalAllocated = 0;
        selectedRows.forEach(row => {
            totalAllocated += parseFloat(row.dataset.allocated);
        });

        totalDisplay.textContent = (totalAllocated / 1024 / 1024).toFixed(2) + " MB";
        countDisplay.textContent = `${selectedRows.length} item${selectedRows.length > 1 ? 's' : ''} selected`;
        
        bubble.classList.remove('hidden');
    } else {
        bubble.classList.add('hidden');
    }
}


let pendingResource = null;

const modal = document.getElementById('modalOverlay');
const btnSelectOne = document.getElementById('btnSelectOne');
const btnSelectSystem = document.getElementById('btnSelectSystem');
const btnCancel = document.getElementById('btnCancel');

const closeModal = () => modal.classList.add('hidden');
btnCancel.onclick = closeModal;

btnSelectOne.onclick = () => {
    const rows = document.querySelectorAll('#resourceTable tbody tr');
    rows.forEach(row => {
        if (row.dataset.name === pendingResource.name) {
            row.classList.add('selected');
            row.scrollIntoView({ behavior: 'smooth', block: 'center' });
        }
    });
    updateSelectionStats();
    closeModal();
};

btnSelectSystem.onclick = () => {
    const systemToSelect = extractSystem(pendingResource.name);
    const rows = document.querySelectorAll('#resourceTable tbody tr');
    rows.forEach(row => {
        const rowSystem = row.querySelector('td:nth-child(2)').textContent;
        if (rowSystem === systemToSelect) {
            row.classList.add('selected');
        }
    });
    updateSelectionStats();
    closeModal();
};

function setStandbyMode() {
    document.body.setAttribute('data-mode', 'none');
    document.getElementById('identityTitle').textContent = "STANDBY - WAITING FOR DATA";

    document.getElementById('totalAllocated').textContent = "0.00 MB";
    document.getElementById('totalRequested').textContent = "0.00 MB";
    document.getElementById('efficiency').textContent = "0%";
}

function renderData(data) {
    if (!data) return;

    const isCPU = data.type === "CPU"; 
    const mode = isCPU ? "cpu" : "gpu";
    
    document.body.setAttribute('data-mode', mode);
    document.getElementById('identityTitle').textContent = isCPU ? "SYSTEM RAM PROFILER" : "GPU VRAM PROFILER";

    currentData = data;
    
    document.getElementById('totalAllocated').textContent = (data.totalAllocated / 1024 / 1024).toFixed(2) + " MB";
    document.getElementById('totalRequested').textContent = (data.totalRequested / 1024 / 1024).toFixed(2) + " MB";
    const efficiency = data.totalAllocated > 0 ? (data.totalRequested / data.totalAllocated) * 100 : 0;
    document.getElementById('efficiency').textContent = efficiency.toFixed(1) + "%";

    const map = document.getElementById('memoryMap');
    const tbody = document.querySelector('#resourceTable tbody');
    map.innerHTML = '';
    tbody.innerHTML = '';

    data.resources.sort((a, b) => b.allocatedBytes - a.allocatedBytes);

    data.resources.forEach(res => {
        const system = extractSystem(res.name);
        const color = getSystemColor(system);

        const block = document.createElement('div');
        block.className = 'mem-block';
        block.style.width = `${Math.max(20, Math.sqrt(res.allocatedBytes) / 10)}px`;
        block.style.backgroundColor = color;
        
        block.onclick = () => {
            pendingResource = res;
            document.getElementById('modalTitle').textContent = res.name;
            btnSelectSystem.textContent = `Select All in "${system}"`;
            modal.classList.remove('hidden');
        };
        map.appendChild(block);

        const row = document.createElement('tr');
        row.dataset.allocated = res.allocatedBytes;
        row.dataset.requested = res.requestedBytes;
        row.dataset.name = res.name;
        
        row.innerHTML = `
            <td>${res.name}</td>
            <td style="color:${color}; font-weight:bold;">${system}</td>
        `;
        if (mode == "gpu")
            row.innerHTML += `<td>${res.type}</td>`;
        row.innerHTML += `<td>${(res.allocatedBytes / 1024).toFixed(1)}</td>`
        if (mode == "gpu")
            row.innerHTML += `<td>${(res.requestedBytes / 1024).toFixed(1)}</td>`;
        row.innerHTML += `<td>${res.usagePercent}%</td>`;

        row.onclick = () => {
            row.classList.toggle('selected');
            updateSelectionStats();
        };

        tbody.appendChild(row);
    });

    updateSelectionStats();
}
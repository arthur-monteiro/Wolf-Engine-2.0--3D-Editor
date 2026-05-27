
const AssetBrowser = {
    currentSearch: "",
    currentTypeFilter: ["all"],
    currentPage: 0,
    maxCountPerPage: 40,

    init() {
        const searchInput = document.getElementById('assetSearch');
        if (searchInput) {
            searchInput.addEventListener('input', this.debounce((e) => {
                this.currentSearch = e.target.value.toLowerCase();
                this.resetAndRequest();
            }, 50));
        }
    },

    requestAssetsFromEngine(append = false) {
        const queryParams = {
            search: this.currentSearch,
            type: this.currentTypeFilter,
            limit: this.maxCountPerPage,
            offset: this.currentPage * this.maxCountPerPage
        };

        this.populateGrid(requestAssetPayload(JSON.stringify(queryParams)), append);
    },

    resetAndRequest() {
        this.currentPage = 0;
        this.requestAssetsFromEngine();
    },

    nextPage() {
        this.currentPage++;
        this.requestAssetsFromEngine(true);
    },

    populateGrid(assetsJSON, append = false) {
        const grid = document.getElementById('assetGrid');
        if (!grid) return;

        if (!append) {
            grid.innerHTML = "";
        }

        const data = typeof assetsJSON === "string" ? JSON.parse(assetsJSON) : assetsJSON;
        if (!data || !data.assets || data.assets.length === 0) {
            if(!append) grid.innerHTML = `<div class="asset-browser-empty">No assets found</div>`;
            return;
        }

        const fragment = document.createDocumentFragment();

        data.assets.forEach(asset => {
            const item = document.createElement('div');
            item.className = 'assetItem';
            item.id = `asset_${asset.id}`;
            item.setAttribute('draggable', 'true');
            item.setAttribute('data-asset-type', asset.type);

            item.addEventListener('mousedown', (e) => {
                onMouseDownAsset(e, asset.id);
            });

            item.addEventListener('pointerdown', (e) => {
                if (event.button === 0) 
                    startAssetDrag(asset.name)
            });

            const iconDiv = document.createElement('div');
            iconDiv.className = 'assetIcon';
            const img = document.createElement('img');
            img.src = asset.iconPath;
            iconDiv.appendChild(img);

            const nameDiv = document.createElement('div');
            nameDiv.className = 'assetName';
            nameDiv.textContent = asset.name;

            item.appendChild(iconDiv);
            item.appendChild(nameDiv);
            fragment.appendChild(item);
        });

        grid.appendChild(fragment);
    },

    debounce(func, delay) {
        let timeout;
        return function(...args) {
            clearTimeout(timeout);
            timeout = setTimeout(() => func.apply(this, args), delay);
        };
    }
};

window.addEventListener('DOMContentLoaded', () => {
    AssetBrowser.init();
});
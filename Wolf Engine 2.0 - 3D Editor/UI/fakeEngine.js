function addBuilding() {
    addModel("", "", "building");
    selectModelByName("building0");
}

function getFrameRate() {
    return "FPS: 144";
}

function getRenderHeight() {
    return 600;
}

function getRenderOffsetLeft() {
    return 500;
}

function getRenderWidth() {
    return 1120;
}

let modelAddCounter = 0;
function addModel(filepath, materialPath, type) {
    addModelToList(type + modelAddCounter);
}

function pickFile(inputOption) {
    return "F:\\Code\\Wolf Engine 2.0 - 3D Editor\\Wolf Engine 2.0 - 3D Editor\\UI\\UI.html";
}

function selectModelByName(name) {
    updateSelectedModelInfo(name, 1, 1, 1, 0, 0, 0, 0, 0, 0);

    if (name.includes("building")) {
        updateSelectedBuildingInfo(10, 20, 8, 2, "F:\\Code\\Wolf Engine 2.0 - 3D Editor\\Wolf Engine 2.0 - 3D Editor\\UI\\UI.html");
    }
}

function changeBuildingSizeX(value) { console.log("changeBuildingSizeX " + value); }
function changeBuildingSizeZ(value) {}
function changeBuildingFloorCount(value) {}
function changeBuildingWindowSideSize(value) {}
function changeBuildingWindowMesh(meshIdx, filepath, materialFolder) {} 
function setRenderOffsetLeft(value) { console.log("setRenderOffsetLeft " + value); }
function setRenderOffsetRight(value) { console.log("setRenderOffsetRight " + value); }
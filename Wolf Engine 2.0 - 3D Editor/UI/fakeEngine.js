function addBuilding() {
    addModel("", "", "building");
    selectModelByName("building0");
}

function getFrameRate() {
    return "FPS: 144";
}

function getVRAMRequested() {
    return "0 MB";
}

function getVRAMUsed() {
	return "0 MB";
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

function getRenderOffsetRight() {
	return 300;
}

let modelAddCounter = 0;
function addEntity(filepath, materialPath, type) {
    addEntityToList(type + modelAddCounter);
}

function pickFile(inputOption) {
    return "F:\\Code\\Wolf Engine 2.0 - 3D Editor\\Wolf Engine 2.0 - 3D Editor\\UI\\UI.html";
}

function selectEntityByName(name) {
    let json = "{ \
        \"params\": [ \
            { \
                \"name\" : \"Scale\", \
                \"tab\" : \"Model\", \
                \"category\" : \"Transform\", \
                \"type\" : \"Vector3\", \
                \"valueX\" : 1.000000, \
                \"valueY\" : 1.000000, \
                \"valueZ\" : 1.000000, \
                \"min\" : -1.0, \
                \"max\" : 1.0 \
            }, \
            { \
                \"name\" : \"Translation\", \
                \"tab\" : \"Model\", \
                \"category\" : \"Transform\", \
                \"type\" : \"Vector3\", \
                \"valueX\" : 0.000000, \
                \"valueY\" : 0.000000, \
                \"valueZ\" : 0.000000, \
                \"min\" : -10.0, \
                \"max\" : 10.0 \
            }, \
            { \
                \"name\" : \"Rotation\", \
                \"tab\" : \"Model\", \
                \"category\" : \"Transform\", \
                \"type\" : \"Vector3\", \
                \"valueX\" : 0.000000, \
                \"valueY\" : -0.000000, \
                \"valueZ\" : 0.000000, \
                \"min\" : 0.0, \
                \"max\" : 6.29 \
            }, \
            { \
                \"name\" : \"Name\", \
                \"tab\" : \"Model\", \
                \"category\" : \"General\", \
                \"type\" : \"String\", \
                \"value\" : \"cube.obj\" \
            } \
        ] \
    }"

    setNewParams(json);

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
function changeBuildingWallMesh(idx, filename, folder) { console.log("changeBuildingWallMesh " + idx + " " + filename + " " + folder); }
function displayTypeSelectChanged(value) { console.log("displayTypeSelectChanged " + value); }
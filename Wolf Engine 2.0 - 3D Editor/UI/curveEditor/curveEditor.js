/// Returns a bitset:
let LMB = 1
let RMB = 2
let MMB = 4
function getMouseButtons(e) {
    //if (getUltralightVersion()) {
        switch (e.which) { // old webkit in ultralight...
            case 1: // LMB
                return 1;
            case 2: // middle
                return 4;
            case 3: // RMB
                return 2;
            default:
                return 0;
        }
    //} else {
    //    return e.buttons;
    //}
}

let hsl = (h, s, l) => `hsl(${h},${s}%,${l}%)`

class Line {
    constructor(beginX, beginY, endX, endY, color, idxInCurveEditor, curveEditor) {
        this.#beginX = beginX;
        this.#beginY = beginY;
        this.#endX = endX;
        this.#endY = endY;
        this.#color = color;

        this.#idxInCurveEditor = idxInCurveEditor;
        this.#curveEditor = curveEditor;
    }

    draw(ctx) {
        ctx.strokeStyle = this.#color;

        ctx.beginPath();
        ctx.moveTo(this.#beginX * this.#curveEditor.getWidthMultiplier(), this.#beginY * this.#curveEditor.getHeightMultiplier());
        ctx.lineTo(this.#endX * this.#curveEditor.getWidthMultiplier(), this.#endY * this.#curveEditor.getHeightMultiplier());
        ctx.stroke();
    }

    setBegin(posX, posY) {
        this.#beginX = posX;
        this.#beginY = posY;
    }

    getBeginX() {
        return this.#beginX;
    }

    getBeginY() {
        return this.#beginY;
    }

    setEnd(posX, posY) {
        this.#endX = posX;
        this.#endY = posY;
    }

    getEndX() {
        return this.#endX;
    }

    getEndY() {
        return this.#endY;
    }

    getIdxInCurveEditor() {
        return this.#idxInCurveEditor;
    }

    increaseIdxInCurveEditor() {
        this.#idxInCurveEditor++;
    }

    decreaseIdxInCurveEditor() {
        this.#idxInCurveEditor--;
    }

    #beginX;
    #beginY;
    #endX;
    #endY;
    #color;

    #idxInCurveEditor
    #curveEditor
}

class Circle {
    constructor(posX, posY, radius, color, curveEditor) {
        this.#posX = posX;
        this.#posY = posY;
        this.#radius = radius;
        this.#color = color;
        this.#isSelected = false;

        this.#curveEditor = curveEditor;
    }

    addLineBeginDependency(line) {
        this.#lineBeginDependency = line;
        line.setBegin(this.#posX, this.#posY);
    }

    addLineEndDependency(line) {
        this.#lineEndDependency = line;
        line.setEnd(this.#posX, this.#posY);
    }

    draw(ctx) {
        ctx.fillStyle = this.#color;
        ctx.strokeStyle = this.#color;

        ctx.beginPath();
        ctx.arc(this.#posX, this.#posY, this.#radius, 0, 2.0 * Math.PI);
        ctx.fill();
        ctx.stroke();
        
        if (this.#isSelected) {
            ctx.lineWidth = 3;
            ctx.strokeStyle = this.#color.replace(/,\d\d%\)/, str => str.replace(/\d\d/, str.match(/\d\d/)[0] * 0.6));

            ctx.beginPath();
            ctx.arc(this.#posX, this.#posY, this.#radius + ctx.lineWidth, 0, 2.0 * Math.PI);
            ctx.stroke();
        }
    }

    moveToPosition(posX, posY) {
        let minX = 0;
        if (this.#lineBeginDependency) {
            minX = this.#curveEditor.getMinX(this.#lineBeginDependency);
        }
        let maxX = this.#curveEditor.getWidth();
        if (this.#lineEndDependency) {
            maxX = this.#curveEditor.getMaxX(this.#lineEndDependency);
        }

        this.#posX = posX - this.#offsetX;
        if (this.#posX < minX)
            this.#posX = minX;
        if (this.#posX > maxX)
            this.#posX = maxX;

        this.#posY = posY - this.#offsetY;

        if (this.#lineBeginDependency) {
            this.#lineBeginDependency.setBegin(this.#posX, this.#posY);
            if (this.#posX > 0 && this.#lineBeginDependency.getIdxInCurveEditor() == 0) {
                this.#curveEditor.increaseAllIndicesInCurveEditor();
                this.#lineEndDependency = this.#curveEditor.addLine(0, this.#curveEditor.getHeight(), this.#posX, this.#posY, null, this);
            }
        }
        if (this.#lineEndDependency) {
            this.#lineEndDependency.setEnd(this.#posX, this.#posY);
            if (this.#posX < this.#curveEditor.getWidth() && this.#lineEndDependency.getIdxInCurveEditor() == this.#curveEditor.getLineCount() - 1) {
                this.#lineBeginDependency = this.#curveEditor.addLine(this.#posX, this.#posY, this.#curveEditor.getWidth(), 0, this, null, this.#curveEditor.getLineCount());
            }
        }
    }

    setSelected(posX, posY) {
        this.#isSelected = true;
        this.#offsetX = posX - this.#posX;
        this.#offsetY = posY - this.#posY;
    }

    setUnselected() {
        this.#isSelected = false;
        this.#offsetX = 0;
        this.#offsetY = 0;
    }

    isPointInside(coordX, coordY) {
        let xCircleSpace = coordX - this.#posX;
        let yCircleSpace = coordY - this.#posY;
    
        return xCircleSpace * xCircleSpace + yCircleSpace * yCircleSpace < this.#radius * this.#radius;
    }

    isSelected() {
        return this.#isSelected;
    }

    getPosX() {
        return this.#posX;
    }

    getPosY() {
        return this.#posY;
    }

    getLineBeginDependency() {
        return this.#lineBeginDependency;
    }

    getLineEndDependency() {
        return this.#lineEndDependency;
    }

    #posX;
    #posY;
    #radius;
    #color;
    #isSelected;
    #offsetX;
    #offsetY;

    #curveEditor

    /* Line dependency */
    #lineBeginDependency
    #lineEndDependency
}

class ContextMenu {
    constructor(pointSelected, curveEditor, inputForYValueId) {
        this.#topLeftX = pointSelected.getPosX();
        this.#topLeftY = pointSelected.getPosY();
        this.#deleteForbidden = this.#topLeftX == 0 || this.#topLeftX == curveEditor.getWidth(); 
        this.#width = 100;
        this.#height = this.#deleteForbidden ? 30 : 60;
        this.#color = "hsl(0, 0%, 0%)";
        this.#textColor = "hsl(0, 0%, 72%)";
        this.#inputForYValueId = inputForYValueId;

        this.#fixPosIfOutside(curveEditor);

        // Clear all event listeners
        var old_element = document.getElementById(this.#inputForYValueId);
        var new_element = old_element.cloneNode(true);
        old_element.parentNode.replaceChild(new_element, old_element);

        document.getElementById(this.#inputForYValueId).value = 1.0 - pointSelected.getPosY() / curveEditor.getHeight();

        document.getElementById(this.#inputForYValueId).style.display = "block";
        document.getElementById(this.#inputForYValueId).style.position = "absolute";
        this.#updateInputPos(curveEditor);
        document.getElementById(this.#inputForYValueId).style.background = this.#color;
        document.getElementById(this.#inputForYValueId).style.outline = "none";
        document.getElementById(this.#inputForYValueId).style.webkitBoxShadow = "none";
        document.getElementById(this.#inputForYValueId).style.boxShadow = "none";
        document.getElementById(this.#inputForYValueId).style. border = "none";
        document.getElementById(this.#inputForYValueId).style.borderColor = "inherit";
        document.getElementById(this.#inputForYValueId).style.color = this.#textColor;

        document.getElementById(this.#inputForYValueId).addEventListener("input", e => {
            let val = curveEditor.getHeight() - parseFloat(document.getElementById(this.#inputForYValueId).value) * curveEditor.getHeight();
            pointSelected.moveToPosition(pointSelected.getPosX(), val);

            this.#topLeftY = val;
            this.#fixPosIfOutside(curveEditor);
            this.#updateInputPos(curveEditor);
        });
    }

    #fixPosIfOutside(curveEditor) {
        const borderOffset = 15;
        if (this.#topLeftY + this.#height + borderOffset > curveEditor.getHeight()) {
            this.#topLeftY = curveEditor.getHeight() - this.#height - borderOffset;
        }
        if (this.#topLeftY - borderOffset < 0) {
            this.#topLeftY = borderOffset;
        }
        
        if (this.#topLeftX - borderOffset < 0) {
            this.#topLeftX = borderOffset;
        }
        if (this.#topLeftX + this.#width + borderOffset > curveEditor.getWidth()) {
            this.#topLeftX = curveEditor.getWidth() - this.#width - borderOffset;
        }
    }

    #updateInputPos(curveEditor) {
        document.getElementById(this.#inputForYValueId).style.left = "calc(50% - " + (curveEditor.getWidth() / 2) + "px + " + (this.#topLeftX + 37) + "px)";
        document.getElementById(this.#inputForYValueId).style.top = "calc(50% - " + (curveEditor.getHeight() / 2) + "px + " + (this.#topLeftY + (this.#deleteForbidden ? 2 : 32)) + "px)";
        document.getElementById(this.#inputForYValueId).style.width = "60px";
        document.getElementById(this.#inputForYValueId).style.height = "28px";
    }

    draw(ctx) {
        ctx.fillStyle = this.#color;
        ctx.fillRect(this.#topLeftX, this.#topLeftY, this.#width, this.#height);

        ctx.fillStyle = this.#textColor;
        ctx.font = "20px serif";
        if (!this.#deleteForbidden) {
            ctx.fillText("Delete", this.#topLeftX + 26, this.#topLeftY + 21);
        }

        ctx.fillText("y = ", this.#topLeftX + 5, this.#topLeftY + (this.#deleteForbidden ? 21 : 51));
        ctx.fillText("y = ", this.#topLeftX + 5, this.#topLeftY + (this.#deleteForbidden ? 21 : 51));

        if (!this.#deleteForbidden) {
            ctx.strokeStyle = this.#textColor;
            ctx.beginPath();
            ctx.moveTo(this.#topLeftX, this.#topLeftY + 30);
            ctx.lineTo(this.#topLeftX + this.#width, this.#topLeftY + 30);
            ctx.stroke();
        }
    }

    isPointInside(posX, posY) {
        if (this.#deleteForbidden)
            return false;

        let xLine = posX > this.#topLeftX && posX < this.#topLeftX + this.#width;
        let yLine = posY > this.#topLeftY && posY < this.#topLeftY + this.#height / 2;

        return xLine && yLine 
    }
    
    #topLeftX;
    #topLeftY;
    #width;
    #height;
    #color;
    #textColor;

    #inputForYValueId;

    #deleteForbidden;
}

class Background {
    constructor(width, height) {
        this.#width = width;
        this.#height = height;
    }

    draw(ctx) {
        ctx.fillStyle = "white";
        ctx.fillRect(0, 0, this.#width, this.#height);
    }

    #width;
    #height;
}

class Image {
    constructor(sizeX, sizeY, srcId, posX, posY) {
        this.#image = document.getElementById(srcId);
        this.#sizeX = sizeX;
        this.#sizeY = sizeY;
        this.#srcId = srcId;
        this.#topLeftX = posX;
        this.#topLeftY = posY;
    }

    draw(ctx) {
        if (!this.#image)
        {
            this.#image = document.getElementById(this.#srcId);
            return;
        }

        ctx.drawImage(this.#image, this.#topLeftX, this.#topLeftY, this.#sizeX, this.#sizeY);
    }

    isPointInside(posX, posY) {
        let xLine = posX > this.#topLeftX && posX < this.#topLeftX + this.#sizeX;
        let yLine = posY > this.#topLeftY && posY < this.#topLeftY + this.#sizeY;

        return xLine && yLine 
    }

    #image;
    #srcId;
    #sizeX;
    #sizeY;
    #topLeftX;
    #topLeftY;
}

class CurveEditor {
    constructor(previewSizeX, previewSizeY, percentOfPageX, percentOfPageY, divId, closeButtonId, inputForYValueId, callbackCurveChanged) {
        let canvasId = divId + "Canvas";
        document.getElementById(divId).innerHTML = "<div><canvas id=" + canvasId + "></canvas></div>"

        this.#canvas = document.getElementById(canvasId);
        this.#ctx = this.#canvas.getContext("2d");

        this.#isInPreview = true;
        this.#canvas.width = previewSizeX;
        this.#canvas.height = previewSizeY;
        this.#canvas.style.backgroundColor = 'transparent';

        this.#closeButtonId = closeButtonId;
        this.#divId = divId;
        this.#inputForYValueId = inputForYValueId;
        this.#defaultDivStyle = document.getElementById(divId).style;

        this.#callbackCurveChanged = callbackCurveChanged;

        this.#previewSizeX = previewSizeX;
        this.#previewSizeY = previewSizeY;
        this.#sizeX = percentOfPageX * window.innerWidth * 0.01;
        this.#sizeY = percentOfPageY * window.innerHeight * 0.01;

        this.#ctx.lineWidth = 2;
        this.#ctx.textAlign = 'center';
        this.#ctx.textBaseline = 'middle';
        this.#ctx.font = '10px Arial';

        this.#pointList = new Array();
        this.#lineList = new Array();
        this.#contextMenu = new Array();

        this.#editImage = new Image(50, 50, "editCurveIcon", previewSizeX - 50, previewSizeY - 50);
        this.#background = new Background(this.#sizeX, this.#sizeY);

        this.#canvas.addEventListener('mousemove', e => {
            if (this.#isInPreview)
                return;

            let mouse = this.getMouseCoords(e)
        
            this.#pointList.forEach(e => {
                if (e.isSelected()) {
                    e.moveToPosition(mouse.x, mouse.y);
                }
            })
        })
        
        this.#canvas.addEventListener('mousedown', e => {
            let mouse = this.getMouseCoords(e)

            let isRightClick = getMouseButtons(e) == RMB;

            if (isRightClick) {
                if (this.#isInPreview)
                    return;
    
                e.preventDefault();
    
                this.#pointSelectedForDelete = null;
    
                let mouse = this.getMouseCoords(e)
                this.#pointList.forEach(e => {
                    if (e.isPointInside(mouse.x, mouse.y)) {
                        this.#pointSelectedForDelete = e;
                    }
                })
    
                if (this.#pointSelectedForDelete) {
                    this.#clearContextMenu();
                    this.#contextMenu.push(new ContextMenu(this.#pointSelectedForDelete, this, this.#inputForYValueId));
                }
            }
            else {
                if (this.#isInPreview && this.#editImage.isPointInside(mouse.x, mouse.y)) {
                    this.#isInPreview = false;
    
                    this.#canvas.width = this.#sizeX;
                    this.#canvas.height = this.#sizeY;
    
                    document.getElementById(divId).style.position = "fixed";
                    document.getElementById(divId).style.border = "3px solid #73AD21";
                    document.getElementById(divId).style.top = "50%";
                    document.getElementById(divId).style.left = "50%";
                    document.getElementById(divId).style.marginRight = "-50%";
                    document.getElementById(divId).style.transform = "translate(-50%, -50%)";
                    document.getElementById(divId).style.width = this.#sizeX;
                    document.getElementById(divId).style.height = this.#sizeY;
    
                    document.getElementById(closeButtonId).style.display = 'block';
                    document.getElementById(closeButtonId).style.top = (48 - percentOfPageY / 2) + "%";
                    document.getElementById(closeButtonId).style.left = (52 + percentOfPageX / 2) + "%";
                    document.getElementById(closeButtonId).style.marginRight = "-50%";
                    document.getElementById(closeButtonId).style.transform = "translate(-50%, -50%)";
    
                    openModalWindow();
                    return;
                }
    
                this.#contextMenu.forEach(e => {
                    if (e.isPointInside(mouse.x, mouse.y)) {
                        this.#pointList.splice(this.#pointList.indexOf(this.#pointSelectedForDelete), 1);
    
                        // Attach begin point to begin of the next line
                        for (let i = 0; i < this.#pointList.length; ++i) {
                            if (this.#pointList[i].getLineBeginDependency() == this.#pointSelectedForDelete.getLineEndDependency()) {
                                this.#pointList[i].addLineBeginDependency(this.#pointSelectedForDelete.getLineBeginDependency());
                            }
                        }
    
                        // Adjust indices
                        this.#lineList.forEach(e => {
                            if (e.getIdxInCurveEditor() > this.#pointSelectedForDelete.getLineEndDependency().getIdxInCurveEditor())
                                e.decreaseIdxInCurveEditor();
                        })
    
                        // Remove previous line
                        this.#lineList.splice(this.#lineList.indexOf(this.#pointSelectedForDelete.getLineEndDependency()), 1);
                    }
                });
    
                this.#pointList.forEach(e => {
                    if (e.isPointInside(mouse.x, mouse.y)) {
                        e.setSelected(mouse.x, mouse.y);
                    } else {
                        e.setUnselected();
                    }
                });
    
                this.#clearContextMenu();
            }
        })
        
        this.#canvas.addEventListener('mouseup', e => {
            this.#pointList.forEach(e => e.setUnselected());
        })

        this.step();
    }

    #clearContextMenu() {
        document.getElementById(this.#inputForYValueId).style.display = "none";
        this.#contextMenu = [];
    }

    addPoint(posX, posY) {
        this.#pointList.push(new Circle(posX, posY, 10, "hsl(0, 0%, 50%)", this))
    }

    addLine(beginX, beginY, endX, endY, pointBegin, pointEnd, idxInCurveEditor = 0) {
        this.#lineList.push(new Line(beginX, beginY, endX, endY, "hsl(0, 0%, 0%)", idxInCurveEditor, this))

        if (!pointBegin) {
            this.addPoint(beginX, beginY);
            this.#pointList.at(-1).addLineBeginDependency(this.#lineList.at(-1));
        }
        if (!pointEnd) {
            this.addPoint(endX, endY);
            this.#pointList.at(-1).addLineEndDependency(this.#lineList.at(-1));
        }

        return this.#lineList.at(-1);
    }

    addLineExternal(beginX, beginY, endX, endY, idxInCurveEditor) {
        let startPoint = undefined;

        if (idxInCurveEditor > 0) {
            startPoint = this.#pointList.at(-1);
        }

        this.addLine(beginX, beginY, endX, endY, startPoint, undefined, idxInCurveEditor);

        if (idxInCurveEditor > 0) {
            startPoint.addLineBeginDependency(this.#lineList.at(-1));
        }
    }

    setToPreview() {
        this.#isInPreview = true;
        this.#clearContextMenu();

        this.#canvas.width = this.#previewSizeX;
        this.#canvas.height = this.#previewSizeY;

        document.getElementById(this.#closeButtonId).style.display = 'none';
        document.getElementById(this.#divId).style = this.#defaultDivStyle;

        closeModalWindow();

        // Send changes to engine
        let curveState = new Object();
        curveState.lines = new Array(this.#lineList.length);

        for (let lineIdx = 0; lineIdx < this.#lineList.length; ++lineIdx) {
            for (let i = 0; i < this.#lineList.length; ++i) {
                if (this.#lineList[i].getIdxInCurveEditor() == lineIdx) {
                    curveState.lines[lineIdx] = new Object();
                    curveState.lines[lineIdx].startPointX = this.#lineList[i].getBeginX() / this.getWidth();
                    curveState.lines[lineIdx].startPointY = 1.0 - (this.#lineList[i].getBeginY() / this.getHeight());
                }
            }
        }

        this.#lineList.forEach(e => {
            if (e.getEndX() == this.getWidth())
                curveState.endPointY = 1.0 - (e.getEndY() / this.getHeight()) ;
        });

        this.#callbackCurveChanged(JSON.stringify(curveState));
    }

    getMouseCoords(event) {
        let canvasCoords = this.#canvas.getBoundingClientRect()
        return {
            x: event.pageX - canvasCoords.left,
            y: event.pageY - canvasCoords.top
        }
    }

    step() {
        this.#ctx.clearRect(0, 0, this.#ctx.canvas.width, this.#ctx.canvas.height);
        this.#ctx.fillStyle = 'white';

        this.#background.draw(this.#ctx);

        this.#lineList.forEach(e => {
            e.draw(this.#ctx);
        })

        if (!this.#isInPreview) {
            this.#pointList.forEach(e => {
                e.draw(this.#ctx);
            })
            this.#contextMenu.forEach(e => {
                e.draw(this.#ctx);
            })
        }
        else {
            this.#editImage.draw(this.#ctx);
        }

        // Don't enable this in ultralight
        //window.requestAnimationFrame(this.step.bind(this));
    }

    increaseAllIndicesInCurveEditor() {
        this.#lineList.forEach(e => {
            e.increaseIdxInCurveEditor();
        })
    }

    getWidth() {
        return this.#sizeX;
    }

    getHeight() {
        return this.#sizeY;
    }

    getWidthMultiplier() {
        if (!this.#isInPreview)
            return 1.0;
        return this.#previewSizeX / this.#sizeX;
    }

    getHeightMultiplier() {
        if (!this.#isInPreview)
            return 1.0;
        return this.#previewSizeY / this.#sizeY;
    }

    getLineCount() {
        return this.#lineList.length;
    }

    getMinX(line) {
        if (line.getIdxInCurveEditor() == 0)
            return 0;
        else {
            for (let i = 0; i < this.#lineList.length; ++i) {
                if (this.#lineList.at(i).getIdxInCurveEditor() == line.getIdxInCurveEditor() - 1) {
                    return this.#lineList.at(i).getBeginX();
                }
            }
        }
    }

    getMaxX(line) {
        if (line.getIdxInCurveEditor() == this.#lineList.length - 1)
            return this.getWidth();
        else {
            for (let i = 0; i < this.#lineList.length; ++i) {
                if (this.#lineList.at(i).getIdxInCurveEditor() == line.getIdxInCurveEditor() + 1) {
                    return this.#lineList.at(i).getEndX();
                }
            }
        }
    }

    #isInPreview;
    #canvas;
    #ctx;

    #closeButtonId;
    #divId;
    #inputForYValueId;
    #defaultDivStyle;
    #callbackCurveChanged;

    #previewSizeX;
    #previewSizeY;
    #sizeX;
    #sizeY;

    #pointList;
    #lineList;
    #contextMenu;
    #editImage;
    #background;

    #pointSelectedForDelete
};
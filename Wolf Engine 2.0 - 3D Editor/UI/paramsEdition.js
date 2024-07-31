function formatStringForFunctionName(input) {
    let out = "";
    let nextCharIsUpper = false;

    for (let i = 0; i < input.length; ++i) {
        let character = input[i];

        if (character == ' ' || character == '(' || character == ')') {
            nextCharIsUpper = true;
        }
        else if (character == '/') {
            out += "_";
        }
        else {
            out += nextCharIsUpper ? character.toUpperCase() : character;
            nextCharIsUpper = false;
        }
    }

    return out;
}

function setNewParams(inputJSON) {
    // Hide all tab links
    let tabLinks = document.getElementsByClassName("tabLink");
    for (var i = 0; i < tabLinks.length; ++i) {
        var tabLink = tabLinks[i];  
        tabLink.classList.remove("tabSelected");
        tabLink.style.display = "none";
    }
    document.getElementById("tabLinkEntity").classList.add("tabSelected");
    document.getElementById("entityInfos").style.display = "block";

    const jsonObject = JSON.parse(inputJSON);

    // 1 - Fill initial data (tab count, categories count, ...)
    let tabs = []
    jsonObject.params.forEach(function(param) {
        let tabIdx = -1;
        for (let i = 0; i < tabs.length; ++i){
            if (tabs[i][0] == param.tab) {
                tabIdx = i;
                break;
            }
        }

        if (tabIdx == -1) {
            tabs.push([param.tab, []]);
            tabIdx = tabs.length - 1;
        }
        let tab = tabs[tabIdx];
        let categories = tab[1];

        let categoryIdx = -1;
        for (let i = 0; i < categories.length; ++i){
            if (categories[i][0] == param.category) {
                categoryIdx = i;
                break;
            }
        }

        if (categoryIdx == -1) {
            let initialHTML = "<div class='blockParameters'>";
            initialHTML += "<div class='blockParametersTitle' id='" + formatStringForFunctionName(param.category) + "'>" + param.category + "</div>";
		    initialHTML += "<div class='blockParametersContent'>";
            categories.push([param.category, 0, initialHTML]);
            categoryIdx = categories.length - 1;
        }
    
        categories[categoryIdx][1]++;
    });

    // 2 - Fill content
    tabs.forEach(function(tab) {
        let currentCategoryCounter = [];
        let categories = tab[1];
        for (let i = 0; i < categories.length; ++i)
            currentCategoryCounter.push(0);

        for (let i = 0; i < jsonObject.params.length; ++i) {
            let param = jsonObject.params[i];
            if (param.tab != tab[0])
                continue;
            let categoryIdx = -1;
            for (let j = 0; j < categories.length; ++j){
                if (categories[j][0] == param.category) {
                    categoryIdx = j;
                    break;
                }
            }
            currentCategoryCounter[categoryIdx]++;
            categories[categoryIdx][2] += computeInput(param, categories[categoryIdx][1] == currentCategoryCounter[categoryIdx]);
        }
    });


    // 3 - Push to DOM
    tabs.forEach(function(tab) {
        let htmlToAdd = "";

        let categories = tab[1];
        for (let i = 0; i < categories.length; ++i) {
            categories[i][2] += "</div></div>";
            htmlToAdd += categories[i][2];        
        }

        let tabId = tab[0].charAt(0).toLowerCase() + tab[0].slice(1) + "Infos";
        if (!document.getElementById(tabId)) {
            var newTabDiv = document.createElement('div');
            newTabDiv.id = tabId;
            newTabDiv.className = 'info';
            newTabDiv.style = "display: none; overflow-x: scroll;";
            document.getElementById("staticElements").appendChild(newTabDiv);

            var newTabLinkButton = document.createElement('button');
            newTabLinkButton.className = 'tabLink';
            newTabLinkButton.id = "tabLink" + tab[0];
            newTabLinkButton.addEventListener('click', function(){
                eval("selectTabInfo('" + newTabLinkButton.id + "', '" + tabId + "')")
            });
            
            newTabLinkButton.innerHTML = tab[0];
            document.getElementById("infoTabs").appendChild(newTabLinkButton);
        }

        document.getElementById("tabLink" + tab[0]).style.display = "block";
        document.getElementById(tabId).innerHTML = htmlToAdd;

        Array.from(document.getElementById(tabId).querySelectorAll("script"))
            .forEach( oldScriptEl => {
                const newScriptEl = document.createElement("script");
      
                Array.from(oldScriptEl.attributes).forEach( attr => {
                    newScriptEl.setAttribute(attr.name, attr.value) 
                });
                
                const scriptText = document.createTextNode(oldScriptEl.innerHTML);
                newScriptEl.appendChild(scriptText);
                
                oldScriptEl.parentNode.replaceChild(newScriptEl, oldScriptEl);
            });

    });
}

function computeInput(param, isLast) {
    let addBottomBorder = !(isLast || (param.type != "Vector2" && param.type != "Vector3"));
    let nameForCallback = formatStringForFunctionName(param.tab) + formatStringForFunctionName(param.name) + formatStringForFunctionName(param.category);
    let htmlToAdd = "<div style='width: 100%; overflow: auto;" + (!addBottomBorder && !isLast ? "padding-bottom: 5px;" : "") + "'>";

    let classForElements = "inputClass" + nameForCallback;

    if (param.isActivable) {
        htmlToAdd += "<input id='checkBox" + nameForCallback + "' type='checkbox' style='margin-top: 11px' onchange='(function() { "
            + "let elements = document.getElementsByClassName(\"" + classForElements + "\");"
            + "for (let i = 0; i < elements.length; ++i)"
            + "  elements[i].toggleDisabled();"
            + "if (!document.getElementById(\"checkBox" + nameForCallback + "\").checked)"
            + "  disable" + nameForCallback + "();"
            + "else"
            + "  enable" + nameForCallback + "();"
            + "})()'/>"
    }

    if (param.type == "String") {
        htmlToAdd += param.name + ": <input type=\"text\" id=\"nameInput" + nameForCallback + "\" name=\"name\" value=\"" + param.value + "\" oninput=\"(function() { "
            + "let value = document.getElementById('nameInput" + nameForCallback + "').value;"
            + "change" + nameForCallback + "(value); " 
            + (param.drivesCategoryName ? "document.getElementById('" + formatStringForFunctionName(param.category) + "').innerHTML = value;" : "")
            + "})()\"/>";
    }
    else if (param.type == "Vector2" || param.type == "Vector3" || param.type == "UInt" || param.type == "Float") {
        htmlToAdd += addBottomBorder ? "<div style='padding-bottom: 5px; margin-bottom: 5px; border-bottom:1px solid white;'>" : "" ;
        htmlToAdd += "<div style='display: inline-block; float: left; padding: 5px; width: 25%'>" + param.name + " :</div>";
        if (param.isActivable)
            htmlToAdd += "<div style='display: inline-block; width: 65%; float: right'>";
        else
            htmlToAdd += "<div style='display: inline-block; width: 70%'>";
        if (param.type == "UInt")
            htmlToAdd += "<wolf-slider id='uintSlider" + nameForCallback + "' class='" + classForElements + "' max='" + param.max + "' min='" + param.min + "' step='1' oninput=\"change" + nameForCallback + "\" value=\"" + param.value + "\"" + 
                (param.isActivable || param.isReadOnly ? "disabled='true'" : "") + "></wolf-slider>";
        else if (param.type == "Float")
            htmlToAdd += "<wolf-slider max='" + param.max + "' min='" + param.min + "' step='0.01' oninput=\"change" + nameForCallback + "\" value=\"" + param.value + "\"></wolf-slider>";
        else {
            htmlToAdd += "<wolf-slider max='" + param.max + "' min='" + param.min + "' step='0.01' oninput=\"change" + nameForCallback + "X\" value=\"" + param.valueX + "\"></wolf-slider>";
            htmlToAdd += "<wolf-slider max='" + param.max + "' min='" + param.min + "' step='0.01' oninput=\"change" + nameForCallback + "Y\" value=\"" + param.valueY + "\"></wolf-slider>";
            if (param.type == "Vector3")
                htmlToAdd += "<wolf-slider max='" + param.max + "' min='" + param.min + "' step='0.01' oninput=\"change" + nameForCallback + "Z\" value=\"" + param.valueZ + "\"></wolf-slider>";
        }
        htmlToAdd += "</div>";
        if (addBottomBorder) htmlToAdd += "</div>";
    }
    else if (param.type == "File") {       
        htmlToAdd += "<div style='display: inline-block; float: left; padding: 5px; width: 25%'>" + param.name + " :</div>";
        htmlToAdd += "<table style='width: 70%; border-collapse: collapse; border-radius: 5px'>"

        let id = param.tab + formatStringForFunctionName(param.name) + formatStringForFunctionName(param.category);
        htmlToAdd += "<tr><td><div id='" + id + "'>" + (param.value ? param.value : "Default") + "</div></td><td><button onclick=\"pickFileAndSetValue('" + id + "', 'open', '" + param.fileFilter + "', change" + nameForCallback + ")\">Select file</button></td></tr>";
        htmlToAdd += "</table>";
    }
    else if (param.type == "Array") {
        htmlToAdd += "<span style='display: inline-block; float: left; padding-top: 2px;'>" + param.name + ": " + param.count + "</span>";
        if (!param.isReadOnly) 
            htmlToAdd += "<div class='addButton' onclick='addTo" + nameForCallback + "()'></div>";
    }
    else if (param.type == "Entity") {
        var entityDivs = document.getElementById('entityList').getElementsByTagName('div');
        htmlToAdd += param.name + ": <select name='entity' id='entitySelect" + nameForCallback + "' onchange='change" + nameForCallback + "(this.value)'><option value=''>No entity selected</option>";
        for (let i = 0; i < entityDivs.length; i++) {
            var entityDiv = entityDivs[i];
            htmlToAdd += "<option value='" + entityDiv.id + "'" + (param.value == entityDiv.id ? "selected" : "") + ">" + entityDiv.innerHTML + "</option>";
        }
        htmlToAdd += "</select>";
    }
    else if (param.type == "Bool") {
        htmlToAdd += "<div style='display: inline-block; float: left; padding: 5px; width: 25%'>" + param.name + " :</div>";
        htmlToAdd += "<div style='display: inline-block; width: 70%'><input id='checkBox" + nameForCallback + "' type='checkbox' style='margin-top: 9px' onchange='(function() { "
            + "change" + nameForCallback + "(document.getElementById(\"checkBox" + nameForCallback + "\").checked) })()'/></div>";
    }
    else if (param.type == "Enum") {
        htmlToAdd += param.name + ": <select name='enum' id='enumSelect" + nameForCallback + "' onchange='change" + nameForCallback + "(this.value)'>";
        for (let i = 0; i < param.options.length; ++i) {
            htmlToAdd += "<option value='" + param.options[i] + "'" + (param.value == i ? "selected" : "") + ">" + param.options[i] + "</option>";
        }
		htmlToAdd += "</select>";
    }
    else if (param.type == "Curve") {
        htmlToAdd += param.name + ": <div id='curveEditorPlaceholder'></div>";

        htmlToAdd += "<div id='closeButton' onclick='setToPreview()' style='cursor: pointer'><div style='display: block; transform: translate(-1px, -5px); font: message-box;'>X</div></div>";

        // Add script to create the curve editor here to be sure it's executed when div is created
        htmlToAdd += "<script>";
        htmlToAdd += "var curveEditor = new CurveEditor(256, 256, 50, 50, 'curveEditorPlaceholder', 'closeButton', 'curveEditorCustomYInput', change" + nameForCallback + ");";
        for (let i = 0; i < param.lines.length; ++i) {
            let isLast = i == param.lines.length - 1;

            htmlToAdd += "curveEditor.addLineExternal((" + param.lines[i].startPointX + " * curveEditor.getWidth()), (curveEditor.getHeight() - (" + param.lines[i].startPointY + " * curveEditor.getHeight())), " 
                + (isLast ? ("curveEditor.getWidth(), " + "(curveEditor.getHeight() - (" + param.endPointY + "* curveEditor.getHeight()))") 
                    : "(" + (param.lines[i + 1].startPointX + " * curveEditor.getWidth()), (curveEditor.getHeight() - " + param.lines[i + 1].startPointY + " * curveEditor.getHeight())")) + ", " + i + ");";
        }
        htmlToAdd += "function setToPreview() { curveEditor.setToPreview(); }\n";
        htmlToAdd += "animationsInstances.push(curveEditor);";
        htmlToAdd += "</script>";        
    }

    htmlToAdd += "</div>"
    return htmlToAdd;
}
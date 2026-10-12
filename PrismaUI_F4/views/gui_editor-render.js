function renderRuleTreeRow(item, childMap, container, indent, visited, parentId, parentMode, rootMap) {
    if (!item || !container) return 0;
    if (!visited) visited = {};
    var id = safe(item.id, "");
    if (!id) return 0;
    var source = item.source || "json";
    var hasChildren = !!(childMap[id] && childMap[id].length);
    var filtering = treeFilterIsActive();
    var folded = !!treeFoldedNodes[id] && !filtering;
    var rendered = 1;

    var row = document.createElement("div");
    row.className = "rule-tree-row" + ((selectedRuleId === id && selectedRuleSource === source) ? " selected" : "") + (treeParentHighlight[id] ? " parent-chain" : "");
    row.dataset.ruleId = id;
    row.dataset.source = source;
    row.dataset.parentId = parentId || "BOTTOM";
    row.dataset.parentMode = parentMode || "after";
    row.draggable = true;
    row.onclick = (function (rid, ruleSource, rowHasChildren) {
        return function (event) {
            if (event && event.target && event.target.classList && event.target.classList.contains("tree-caret")) {
                treeFoldedNodes[rid] ? delete treeFoldedNodes[rid] : treeFoldedNodes[rid] = true;
                refreshRuleTree();
                return;
            }
            if (event && event.ctrlKey && rowHasChildren) {
                setTreeFoldRecursive(rid, !treeFoldedNodes[rid], childMap, rootMap);
                refreshRuleTree();
                return;
            }
            requestRuleDetails(rid, ruleSource);
        };
    })(id, source, hasChildren);
    row.oncontextmenu = (function (rid, rowHasChildren) {
        return function (event) {
            if (!rowHasChildren) return;
            event.preventDefault();
            setTreeFoldRecursive(rid, !treeFoldedNodes[rid], childMap, rootMap);
            refreshRuleTree();
        };
    })(id, hasChildren);
    row.ondragstart = (function (rid, ruleSource) {
        return function (event) {
            treeDragPayload = { id: rid, source: ruleSource };
            row.classList.add("dragging");
            if (event.dataTransfer) {
                event.dataTransfer.effectAllowed = "move";
                event.dataTransfer.setData("text/plain", rid);
            }
        };
    })(id, source);
    row.ondragend = function () {
        treeDragPayload = null;
        row.classList.remove("dragging");
        var targets = document.querySelectorAll(".tree-drop-target,.tree-drop-child");
        for (var i = 0; i < targets.length; ++i) targets[i].classList.remove("tree-drop-target", "tree-drop-child");
    };
    row.ondragover = function (event) {
        if (!treeDragPayload || treeDragPayload.id === id) return;
        event.preventDefault();
        row.classList.add("tree-drop-target");
        row.classList.toggle("tree-drop-child", !!event.shiftKey);
        if (event.dataTransfer) event.dataTransfer.dropEffect = "move";
    };
    row.ondragleave = function () {
        row.classList.remove("tree-drop-target", "tree-drop-child");
    };
    row.ondrop = function (event) {
        if (!treeDragPayload || treeDragPayload.id === id) return;
        event.preventDefault();
        row.classList.remove("tree-drop-target", "tree-drop-child");
        applyRuleTreeDrop(treeDragPayload.id, treeDragPayload.source, id, row.dataset.parentId, row.dataset.parentMode, !!event.shiftKey);
    };

    var sourceCell = document.createElement("span");
    sourceCell.className = "source";
    sourceCell.textContent = source.toUpperCase();
    var idCell = document.createElement("span");
    idCell.className = "id";
    idCell.style.paddingLeft = (indent * 14) + "px";
    var caret = document.createElement("span");
    caret.className = hasChildren ? "tree-caret" : "tree-caret-placeholder";
    caret.textContent = hasChildren ? (folded ? "\u25B8" : "\u25BE") : "";
    idCell.appendChild(caret);
    idCell.appendChild(document.createTextNode(id || "-"));
    var anchorCell = document.createElement("span");
    anchorCell.className = "anchor";
    anchorCell.textContent = resolveAnchorText(item);

    row.appendChild(sourceCell);
    row.appendChild(idCell);
    row.appendChild(anchorCell);
    container.appendChild(row);

    if (visited[id]) return rendered;
    visited[id] = true;
    if (!hasChildren || folded) return rendered;

    for (var i = 0; i < childMap[id].length; ++i) {
        rendered += renderRuleTreeRow(childMap[id][i], childMap, container, indent + 1, visited, id, "after", rootMap);
    }
    return rendered;
}
function refreshRuleTree() {
    var tree = document.getElementById("ruleTree");
    if (!tree) return;

    var data = buildTreeNodes();
    buildTreeParentHighlight(data);
    tree.innerHTML = "";

    var sceneForDisplay = safe((document.getElementById("treeSceneFilter") || {}).value, "all");
    var vanillaList = getTreeDisplayAnchors(sceneForDisplay);
    var count = 0;
    var seen = {};
    var filtering = treeFilterIsActive();

    var makeRootTitle = function (anchorName, group, isVanillaAnchor) {
        var title = document.createElement("div");
        var folded = !!treeFoldedNodes[anchorName] && !filtering;
        title.className = (isVanillaAnchor ? "rule-tree-row rule-tree-root vanilla-root" : "rule-tree-root") + (treeParentHighlight[anchorName] ? " parent-chain" : "") + (selectedRuleId === anchorName && selectedRuleSource === "vanilla" ? " selected" : "");
        title.dataset.rootId = anchorName;
        title.dataset.parentId = anchorName;
        title.dataset.parentMode = "after";
        title.dataset.source = isVanillaAnchor ? "vanilla" : "root";
        if (isVanillaAnchor) {
            var sourceCell = document.createElement("span");
            sourceCell.className = "source";
            sourceCell.textContent = "GAME";
            var idCell = document.createElement("span");
            idCell.className = "id";
            var caret = document.createElement("span");
            caret.className = "tree-caret";
            caret.textContent = folded ? "\u25B8" : "\u25BE";
            idCell.appendChild(caret);
            idCell.appendChild(document.createTextNode(anchorName));
            var anchorCell = document.createElement("span");
            anchorCell.className = "anchor";
            anchorCell.textContent = getTreeAnchorLabel(anchorName);
            title.appendChild(sourceCell);
            title.appendChild(idCell);
            title.appendChild(anchorCell);
        } else {
            title.innerHTML = '<span class="tree-caret">' + (folded ? "\u25B8" : "\u25BE") + '</span>' + anchorName + ' group';
        }
        title.onclick = function (event) {
            if (isVanillaAnchor && event && event.target && !(event.target.classList && event.target.classList.contains("tree-caret")) && !event.ctrlKey) {
                showVanillaAnchorDetails(anchorName);
                return;
            }
            if (event && event.ctrlKey) setTreeFoldRecursive(anchorName, !treeFoldedNodes[anchorName], data.childMap, data.rootMap);
            else treeFoldedNodes[anchorName] ? delete treeFoldedNodes[anchorName] : treeFoldedNodes[anchorName] = true;
            refreshRuleTree();
        };
        title.oncontextmenu = function (event) {
            event.preventDefault();
            setTreeFoldRecursive(anchorName, !treeFoldedNodes[anchorName], data.childMap, data.rootMap);
            refreshRuleTree();
        };
        title.ondragover = function (event) {
            if (!treeDragPayload) return;
            event.preventDefault();
            title.classList.add("tree-drop-target");
            if (event.dataTransfer) event.dataTransfer.dropEffect = "move";
        };
        title.ondragleave = function () {
            title.classList.remove("tree-drop-target", "tree-drop-child");
        };
        title.ondrop = function (event) {
            if (!treeDragPayload) return;
            event.preventDefault();
            title.classList.remove("tree-drop-target", "tree-drop-child");
            var sourceRule = getRuleByIdAndSource(treeDragPayload.id, treeDragPayload.source);
            if (!sourceRule) return;
            setRuleAnchorsForTree(sourceRule, [{ target: anchorName, mode: "after" }]);
            selectedRuleId = sourceRule.id;
            selectedRuleSource = sourceRule.source || treeDragPayload.source || "json";
            setAnchorRows(sourceRule);
            persistTreeRulePosition(sourceRule);
            refreshRuleTree();
            updateLastAction("tree moved: " + sourceRule.id + " to " + anchorName);
            logLine("tree moved " + sourceRule.id + " -> " + anchorName + " after");
        };
        tree.appendChild(title);
        return folded;
    };

    var emitGroup = function (anchorName, group, isVanillaAnchor) {
        if (!group) group = { before: [], replace: [], after: [] };
        var total = (group.before ? group.before.length : 0) + (group.replace ? group.replace.length : 0) + (group.after ? group.after.length : 0);
        if (!total && !isVanillaAnchor) return;
        if (!total && isVanillaAnchor && filtering) return;
        var folded = makeRootTitle(anchorName, group, isVanillaAnchor);
        if (isVanillaAnchor) count += 1;
        if (folded) return;
        for (var i = 0; i < group.before.length; ++i) {
            count += renderRuleTreeRow(group.before[i], data.childMap, tree, 1, seen, anchorName, "before", data.rootMap);
            seen[group.before[i].id] = true;
        }
        for (var j = 0; j < group.replace.length; ++j) {
            count += renderRuleTreeRow(group.replace[j], data.childMap, tree, 1, seen, anchorName, "replace", data.rootMap);
            seen[group.replace[j].id] = true;
        }
        for (var k = 0; k < group.after.length; ++k) {
            count += renderRuleTreeRow(group.after[k], data.childMap, tree, 1, seen, anchorName, "after", data.rootMap);
            seen[group.after[k].id] = true;
        }
    };

    for (var va = 0; va < vanillaList.length; ++va) {
        if (isPrimaryVanillaAnchor(vanillaList[va])) emitGroup(vanillaList[va], data.rootMap[vanillaList[va]], true);
    }
    emitGroup("TOP", data.rootMap.TOP, false);
    for (var vb = 0; vb < vanillaList.length; ++vb) {
        if (!isPrimaryVanillaAnchor(vanillaList[vb])) emitGroup(vanillaList[vb], data.rootMap[vanillaList[vb]], true);
    }
    emitGroup("BOTTOM", data.rootMap.BOTTOM, false);

    var orphanRoot = [];
    for (var i = 0; i < data.visibleRules.length; ++i) {
        var item = data.visibleRules[i];
        if (!item || !item.id) continue;
        if (!data.reachable[item.id] && !seen[item.id]) orphanRoot.push(item);
    }
    if (orphanRoot.length > 0) {
        orphanRoot.sort(function (a, b) { return safe(a.id, "").localeCompare(safe(b.id, "")); });
        var title = document.createElement("div");
        title.className = "rule-tree-root";
        title.textContent = "Unlinked Nodes";
        tree.appendChild(title);
        for (var oi = 0; oi < orphanRoot.length; ++oi) {
            count += renderRuleTreeRow(orphanRoot[oi], data.childMap, tree, 1, seen, "BOTTOM", "after", data.rootMap);
        }
    }

    if (count === 0) {
        tree.innerHTML = "No nodes matched filters.";
    }

    var countLabel = document.getElementById("ruleTreeCount");
    if (countLabel) countLabel.textContent = count + " nodes";
}

function refreshAnchorTargetHints() {
    anchorTargetHints = ["TOP", "BOTTOM"];
    for (var i = 0; i < rules.length; ++i) {
        if (rules[i] && rules[i].id) anchorTargetHints.push(rules[i].id);
    }
}

function getRuleById(id) {
    for (var i = 0; i < rules.length; ++i) {
        if (safe(rules[i].id, "") === id) return rules[i];
    }
    return null;
}

function normalizeAnchorMode(mode) {
    if (mode === "before" || mode === "replace" || mode === "after") return mode;
    return "after";
}

function readAnchorsFromRule(rule) {
    var arr = [];
    if (!rule) return arr;
    if (Array.isArray(rule.anchors)) {
        for (var i = 0; i < rule.anchors.length; ++i) {
            var item = rule.anchors[i];
            var target = safe(item.target, "");
            var mode = safe(item.mode, "after");
            arr.push({ target: target, mode: normalizeAnchorMode(mode) });
        }
        return arr;
    }
    if (Array.isArray(rule.anchorTargets)) {
        for (var i = 0; i < rule.anchorTargets.length; ++i) {
            var mode = safe((rule.anchorModes && rule.anchorModes[i]) ? rule.anchorModes[i] : "after", "after");
            arr.push({ target: safe(rule.anchorTargets[i], ""), mode: normalizeAnchorMode(mode) });
        }
        return arr;
    }
    if (rule.anchorTarget) {
        arr.push({ target: safe(rule.anchorTarget, ""), mode: normalizeAnchorMode(safe(rule.anchorMode, "after")) });
    }
    return arr;
}

function buildAnchorPreviewText(anchors) {
    if (!selectedRuleId) {
        return "No rule selected.";
    }
    if (!anchors || !anchors.length) {
        return "No custom anchors.\nRule will use default insertion position.";
    }
    var lines = ["Current anchor steps:"];
    for (var i = 0; i < anchors.length; ++i) {
        var target = safe(anchors[i].target, "");
        var mode = normalizeAnchorMode(safe(anchors[i].mode, "after"));
        var targetState = "";
        if (target !== "TOP" && target !== "BOTTOM" && !getRuleById(target)) {
            targetState = " [missing target]";
        }
        lines.push((i + 1) + ". " + selectedRuleId + " " + mode + " " + target + targetState);
    }
    return lines.join("\n");
}

function refreshAnchorPreview() {
    var panel = document.getElementById("anchorPreview");
    if (!panel) return;
    panel.textContent = buildAnchorPreviewText(getCurrentAnchors());
}

function addAnchorRow(target, mode) {
    var targetSelect = document.createElement("select");
    targetSelect.className = "anchor-target";
    refreshAnchorTargetHints();
    var targets = anchorTargetHints.slice(0);
    for (var i = 0; i < targets.length; ++i) {
        var opt = document.createElement("option");
        opt.value = targets[i];
        opt.textContent = targets[i];
        targetSelect.appendChild(opt);
    }
    if (target && targets.indexOf(target) >= 0) targetSelect.value = target;
    else if (target && target.indexOf("$") === 0) {
        var opt = document.createElement("option");
        opt.value = target;
        opt.textContent = target;
        targetSelect.appendChild(opt);
        targetSelect.value = target;
    }

    var modeSelect = document.createElement("select");
    modeSelect.className = "anchor-mode";
    [["after", "after"], ["before", "before"], ["replace", "replace"]].forEach(function (opt) {
        var item = document.createElement("option");
        item.value = opt[0];
        item.textContent = opt[1];
        modeSelect.appendChild(item);
    });
    modeSelect.value = normalizeAnchorMode(mode);

    var btn = document.createElement("button");
    btn.textContent = "Remove";
    btn.onclick = function () { removeAnchorRow(btn.parentNode); };
    targetSelect.onchange = function () { refreshAnchorPreview(); };
    modeSelect.onchange = function () { refreshAnchorPreview(); };

    var row = document.createElement("div");
    row.className = "anchor-row";
    row.appendChild(targetSelect);
    row.appendChild(modeSelect);
    row.appendChild(btn);

    var container = document.getElementById("anchorRows");
    if (container) container.appendChild(row);
    refreshAnchorPreview();
}

function removeAnchorRow(row) {
    var container = document.getElementById("anchorRows");
    if (container && row && row.parentNode === container) {
        container.removeChild(row);
    }
    if (container && container.children.length === 0) addAnchorRow("BOTTOM", "after");
    refreshAnchorPreview();
}

function getCurrentAnchors() {
    var arr = [];
    var container = document.getElementById("anchorRows");
    if (!container) return arr;
    var rows = container.children;
    for (var i = 0; i < rows.length; ++i) {
        var target = rows[i].querySelector(".anchor-target");
        var mode = rows[i].querySelector(".anchor-mode");
        if (!target || !mode) continue;
        var targetValue = target.value || "";
        if (!targetValue) continue;
        arr.push({ target: targetValue, mode: normalizeAnchorMode(mode.value) });
    }
    return arr;
}

function setAnchorRows(rule) {
    var arr = [];
    arr = readAnchorsFromRule(rule);
    var container = document.getElementById("anchorRows");
    if (!container) return;
    container.innerHTML = "";
    if (!arr.length) {
        addAnchorRow("BOTTOM", "after");
        refreshAnchorPreview();
        return;
    }
    for (var i = 0; i < arr.length; ++i) {
        addAnchorRow(arr[i].target || "BOTTOM", arr[i].mode || "after");
    }
    refreshAnchorPreview();
}

function setBoxFields(prefix, box) {
    document.getElementById(prefix + "Tag").value = safe(box.tag, "");
    document.getElementById(prefix + "Value").value = safe(box.value, "");
    document.getElementById(prefix + "IsIcon").checked = safe(box.isIcon, false);
    document.getElementById(prefix + "State").value = safe(box.state, "normal");
    document.getElementById(prefix + "Align").value = safe(box.align, "");
    document.getElementById(prefix + "Active").checked = safe(box.active, true);
}

function collectBox(prefix) {
    return {
        tag: safe(document.getElementById(prefix + "Tag").value, ""),
        value: safe(document.getElementById(prefix + "Value").value, ""),
        isIcon: !!document.getElementById(prefix + "IsIcon").checked,
        state: safe(document.getElementById(prefix + "State").value, "normal"),
        align: safe(document.getElementById(prefix + "Align").value, ""),
        active: !!document.getElementById(prefix + "Active").checked
    };
}

function createRule() {
    var id = safe(document.getElementById("newRuleId").value, "").trim();
    if (!id) {
        logLine("create rule failed: rule id is empty");
        return;
    }
    if (!window.uiRequestRuleCreate) {
        logLine("create rule API not ready");
        return;
    }
    var payload = {
        id: id,
        titleText: safe(document.getElementById("newRuleTitle").value, id),
        priority: toIntDefault(document.getElementById("newRulePriority").value, 800),
        displayType: toIntDefault(document.getElementById("newRuleType").value, 0),
        originPath: safe(document.getElementById("newRuleFile").value, "gui_editor_rules.json")
    };
    window.uiRequestRuleCreate(JSON.stringify(payload));
    updateLastAction("create requested: " + id);
}

function deleteCurrentRule() {
    if (selectedRuleSource === "cpp") {
        logLine("cannot delete cpp override entry");
        return;
    }
    if (!selectedRuleId) {
        logLine("delete rule failed: no selected rule");
        return;
    }
    if (!window.uiRequestRuleDelete) {
        logLine("delete rule API not ready");
        return;
    }
    if (typeof window.confirm === "function" && !window.confirm("Confirm delete rule: " + selectedRuleId + "?")) {
        updateLastAction("delete cancelled: " + selectedRuleId);
        return;
    }
    window.uiRequestRuleDelete(JSON.stringify({ id: selectedRuleId }));
    updateLastAction("delete requested: " + selectedRuleId);
}

function saveRuleChanges() {
    if (!selectedRuleId) {
        logLine("no rule selected");
        return;
    }
    if (selectedRuleSource === "cpp") {
        var payload = {
            id: selectedRuleId,
            source: "cpp",
            priority: toIntDefault(document.getElementById("rulePriority").value, 800),
            anchors: getCurrentAnchors()
        };
        if (typeof window.uiRequestCppOverrideUpdate === "function") {
            window.uiRequestCppOverrideUpdate(JSON.stringify(payload));
        }
        updateLastAction("cpp override update requested: " + selectedRuleId);
        return;
    }
    var fillPct = toFloatDefault(document.getElementById("ruleFillPct").value, -1.0);
    var shieldPct = toFloatDefault(document.getElementById("ruleShieldPct").value, 0.0);
    var payload = {
        id: selectedRuleId,
        titleText: safe(document.getElementById("ruleTitle").value, ""),
        priority: toIntDefault(document.getElementById("rulePriority").value, 800),
        displayType: toIntDefault(document.getElementById("ruleDisplayType").value, 2),
        state: safe(document.getElementById("ruleState").value, "normal"),
        highlightLabel: !!document.getElementById("ruleHighlight").checked,
        hasBackground: !!document.getElementById("ruleBackground").checked,
        valueText: safe(document.getElementById("ruleValueText").value, ""),
        valueAlign: safe(document.getElementById("ruleValueAlign").value, ""),
        valueStandard: !!document.getElementById("ruleValueStandard").checked,
        hideDifference: !!document.getElementById("ruleHideDiff").checked,
        invertDiffColor: !!document.getElementById("ruleInvertDiff").checked,
        showBar: !!document.getElementById("ruleShowBar").checked,
        showValue: !!document.getElementById("ruleShowValue").checked,
        fillPct: fillPct,
        shieldPct: shieldPct,
        globalLeftBox: collectBox("left"),
        globalRightBox: collectBox("right"),
        anchors: getCurrentAnchors()
    };
    var fc = toHexValue(document.getElementById("ruleFillColor").value, null);
    if (fc !== null) payload.fillColor = fc;
    var vc = toHexValue(document.getElementById("ruleValueColor").value, null);
    if (vc !== null) payload.valueColor = vc;
    var bc = toHexValue(document.getElementById("ruleBackgroundColor").value, null);
    if (bc !== null) payload.backgroundColor = bc;

    if (typeof window.uiRequestRuleUpdate === "function") {
        window.uiRequestRuleUpdate(JSON.stringify(payload));
    }
    updateLastAction("update requested: " + selectedRuleId);
}

function resetCurrentCppOverride() {
    if (!selectedRuleId) {
        logLine("no rule selected");
        return;
    }
    if (selectedRuleSource !== "cpp") {
        logLine("reset only applies to C++ override rules");
        return;
    }
    if (typeof window.uiRequestCppOverrideReset !== "function") {
        logLine("cpp reset API not ready");
        return;
    }
    window.uiRequestCppOverrideReset(JSON.stringify({ id: selectedRuleId }));
    updateLastAction("cpp override reset requested: " + selectedRuleId);
}

function onRuleSummary(payload) {
    logLine("received rule summary payload");
    try {
        var data = decodePayload(payload, null);
        if (!data) {
            throw new Error("invalid payload");
        }
        if (!data.rules) {
            if (Array.isArray(data)) {
                data = { rules: data, selected: { id: "", source: "json" } };
            }
            if (!data.rules) data.rules = [];
        }
        rules = data.rules || [];
        if (!Array.isArray(rules)) {
            rules = [];
        }
        _rulesVersion++;
        _cachedRuleIndex = null;
        ruleSummaryLoaded = true;
        ruleSummaryRetry = 0;
        ruleSummaryRequestStarted = 0;
        var parsedSel = parseSelectedPayload(data.selected);
        if (parsedSel.id) {
            selectedRuleId = parsedSel.id;
            selectedRuleSource = parsedSel.source || "json";
        } else if (!selectedRuleId) {
            selectedRuleSource = "json";
        }
        refreshAnchorTargetHints();
        var sourceFilter = safe((document.getElementById("sourceFilter") || {}).value, "all");
        var sceneFilter = safe((document.getElementById("sceneFilter") || {}).value, "all");
        var filterType = (document.getElementById("displayTypeFilter") || {}).value || "-1";

        var list = document.getElementById("ruleList");
        if (!list) {
            ruleSummaryRequestInFlight = false;
            setTimeout(function () {
                onRuleSummary(payload);
            }, 120);
            return;
        }
        var q = (document.getElementById("ruleSearch") || {}).value || "";
        q = q.toLowerCase();
        var filtered = rules.filter(function (item) {
            var title = (item.title || "").toLowerCase();
            var id = (item.id || "").toLowerCase();
            var okText = id.indexOf(q) >= 0 || title.indexOf(q) >= 0;
            var okType = filterType === "-1" || String(item.displayType) === String(filterType);
            var itemSource = item.source || "json";
            var okSource = sourceFilter === "all" ||
                (sourceFilter === "json" && itemSource === "json") ||
                (sourceFilter === "cpp" && itemSource === "cpp") ||
                (sourceFilter === "overridden" && item.isOverridden === true);
            var okScene = sceneFilter === "all" || (item.scene || "custom") === sceneFilter;
            return okText && okType && okSource && okScene;
        });

        list.innerHTML = "";
        for (var i = 0; i < filtered.length; ++i) {
            var item = filtered[i];
            var itemSource = item.source || "json";
            var itemScene = item.scene || "custom";
            var row = document.createElement("div");
            row.className = "rule-item" + ((selectedRuleId === item.id && selectedRuleSource === itemSource) ? " selected" : "");
            row.onclick = (function (id, source) {
                return function () { requestRuleDetails(id, source); };
            })(item.id, itemSource);

            var sourceCell = document.createElement("span");
            sourceCell.className = "source";
            sourceCell.textContent = itemSource.toUpperCase();
            var idCell = document.createElement("span");
            idCell.className = "id";
            idCell.textContent = item.id || "-";
            var titleCell = document.createElement("span");
            titleCell.className = "title";
            titleCell.textContent = item.title || "(no title)";
            var prioCell = document.createElement("span");
            prioCell.className = "num";
            prioCell.textContent = "p=" + safe(item.priority, 0);
            var sceneCell = document.createElement("span");
            sceneCell.className = "scene-tag";
            sceneCell.textContent = itemScene;

            row.appendChild(sourceCell);
            row.appendChild(idCell);
            row.appendChild(titleCell);
            row.appendChild(prioCell);
            row.appendChild(sceneCell);
            list.appendChild(row);
        }

        var count = document.getElementById("ruleCount");
        if (count) count.textContent = filtered.length + " / " + rules.length + " rules";
        if (!selectedRuleId) {
            if (filtered.length > 0) {
                requestRuleDetails(filtered[0].id, filtered[0].source || "json");
            }
            ruleSummaryRequestInFlight = false;
            ruleSummaryRetry = 0;
            return;
        }

        var selectedStillExists = false;
        for (var i = 0; i < filtered.length; ++i) {
            if (filtered[i].id === selectedRuleId && (filtered[i].source || "json") === selectedRuleSource) {
                selectedStillExists = true;
                break;
            }
        }
        if (!selectedStillExists) {
            selectedRuleId = "";
            selectedRuleSource = "json";
            if (filtered.length > 0) {
                requestRuleDetails(filtered[0].id, filtered[0].source || "json");
            }
            ruleSummaryRequestInFlight = false;
            ruleSummaryRetry = 0;
            return;
        }

        requestRuleDetails(selectedRuleId, selectedRuleSource);
        logLine("rule summary loaded: " + rules.length);
        ruleSummaryRequestInFlight = false;
        ruleSummaryRetry = 0;
    } catch (e) {
        ruleSummaryRequestInFlight = false;
        ruleSummaryLoaded = false;
        logLine("parse rule summary failed: " + e.message);
    }
}

function onRuleDetails(payload) {
    var ruleDisplay = document.getElementById("selectedRuleDisplay");
    var pathLabel = document.getElementById("selectedRulePath");
    var sourceLabel = document.getElementById("selectedRuleSource");
    try {
        if (!payload || payload === "{}") {
            selectedRuleId = "";
            if (ruleDisplay) ruleDisplay.textContent = "-";
            if (pathLabel) pathLabel.textContent = "";
            if (sourceLabel) sourceLabel.textContent = "source: json";
            selectedRuleSource = "json";
            showCppMeta(false);
            setInputDisabledByIds(detailFormIds, false);
            document.getElementById("ruleTitle").value = "";
            document.getElementById("rulePriority").value = "";
            document.getElementById("ruleDisplayType").value = "0";
            document.getElementById("ruleState").value = "normal";
            document.getElementById("ruleHighlight").checked = false;
            document.getElementById("ruleBackground").checked = false;
            document.getElementById("ruleHideDiff").checked = false;
            document.getElementById("ruleInvertDiff").checked = false;
            document.getElementById("ruleValueText").value = "";
            document.getElementById("ruleValueAlign").value = "";
            document.getElementById("ruleValueStandard").checked = false;
            document.getElementById("ruleShowBar").checked = false;
            document.getElementById("ruleShowValue").checked = false;
            document.getElementById("ruleFillPct").value = "";
            document.getElementById("ruleShieldPct").value = "";
            document.getElementById("ruleFillColor").value = "";
            document.getElementById("ruleValueColor").value = "";
            document.getElementById("ruleBackgroundColor").value = "";
            document.getElementById("leftTag").value = "";
            document.getElementById("leftValue").value = "";
            document.getElementById("leftIsIcon").checked = false;
            document.getElementById("leftState").value = "";
            document.getElementById("leftAlign").value = "";
            document.getElementById("leftActive").checked = true;
            document.getElementById("rightTag").value = "";
            document.getElementById("rightValue").value = "";
            document.getElementById("rightIsIcon").checked = false;
            document.getElementById("rightState").value = "";
            document.getElementById("rightAlign").value = "";
            document.getElementById("rightActive").checked = true;
            document.getElementById("cppDefaultPriority").value = "";
            document.getElementById("cppIsRegistered").value = "";
            document.getElementById("cppDefaultAnchorTargets").value = "";
            document.getElementById("cppDefaultAnchorModes").value = "";
            var anchorContainer = document.getElementById("anchorRows");
            if (anchorContainer) anchorContainer.innerHTML = "";
            refreshAnchorPreview();
            return;
        }
        var rule = decodePayload(payload, null);
        if (!rule) {
            throw new Error("invalid payload");
        }
        selectedRuleId = rule.id || "";
        selectedRuleSource = safe(rule.source, "json");

        if (sourceLabel) sourceLabel.textContent = "source: " + selectedRuleSource;
        document.getElementById("ruleId").value = rule.id || "";
        document.getElementById("ruleTitle").value = safe(rule.titleText, "");
        document.getElementById("rulePriority").value = safe(rule.priority, 0);
        document.getElementById("ruleDisplayType").value = String(safe(rule.displayType, 0));
        document.getElementById("ruleState").value = safe(rule.state, "normal");
        document.getElementById("ruleHighlight").checked = !!rule.highlightLabel;
        document.getElementById("ruleBackground").checked = !!rule.hasBackground;
        document.getElementById("ruleHideDiff").checked = (rule.hideDifference === 1 || rule.hideDifference === true);
        document.getElementById("ruleInvertDiff").checked = (rule.invertDiffColor === 1 || rule.invertDiffColor === true);
        document.getElementById("ruleValueText").value = safe(rule.valueText, "");
        document.getElementById("ruleValueAlign").value = safe(rule.valueAlign, "");
        document.getElementById("ruleValueStandard").checked = !!rule.valueStandard;
        document.getElementById("ruleShowBar").checked = !!rule.showBar;
        document.getElementById("ruleShowValue").checked = !!rule.showValue;
        document.getElementById("ruleFillPct").value = safe(rule.fillPct, "");
        document.getElementById("ruleShieldPct").value = safe(rule.shieldPct, "");
        document.getElementById("ruleFillColor").value = toHexString(rule.fillColor || 0);
        document.getElementById("ruleValueColor").value = toHexString(rule.valueColor || 0);
        document.getElementById("ruleBackgroundColor").value = toHexString(rule.backgroundColor || 0);
        setBoxFields("left", safe(rule.globalLeftBox, {}));
        setBoxFields("right", safe(rule.globalRightBox, {}));
        setAnchorRows(rule);

        var metaPanel = document.getElementById("cppMeta");
        var isCpp = selectedRuleSource === "cpp";
        showCppMeta(isCpp);
        if (metaPanel) {
            if (isCpp) {
                document.getElementById("cppDefaultPriority").value = safe(rule.defaultPriority, safe(rule.priority, 0));
                document.getElementById("cppIsRegistered").value = (rule.isRegistered ? "true" : "false");
                document.getElementById("cppDefaultAnchorTargets").value = (safe(rule.defaultAnchorTargets, []) || []).join("\n");
                document.getElementById("cppDefaultAnchorModes").value = (safe(rule.defaultAnchorModes, []) || []).join("\n");
            } else {
                document.getElementById("cppDefaultPriority").value = "";
                document.getElementById("cppIsRegistered").value = "";
                document.getElementById("cppDefaultAnchorTargets").value = "";
                document.getElementById("cppDefaultAnchorModes").value = "";
            }
        }

        setInputDisabledByIds(detailFormIds, isCpp);
        if (ruleDisplay) ruleDisplay.textContent = selectedRuleId ? (selectedRuleSource === "cpp" ? (selectedRuleId + " [CPP]") : selectedRuleId) : "-";
        if (pathLabel) pathLabel.textContent = rule.originPath || "";
        refreshAnchorPreview();
        refreshRuleTree();

        logLine("loaded details for " + (selectedRuleId || "unknown"));
    } catch (e) {
        if (sourceLabel) sourceLabel.textContent = "source: json";
        selectedRuleSource = "json";
        selectedRuleId = "";
        setInputDisabledByIds(detailFormIds, false);
        showCppMeta(false);
        document.getElementById("cppDefaultPriority").value = "";
        document.getElementById("cppIsRegistered").value = "";
        document.getElementById("cppDefaultAnchorTargets").value = "";
        document.getElementById("cppDefaultAnchorModes").value = "";
        if (ruleDisplay) ruleDisplay.textContent = "-";
        if (pathLabel) pathLabel.textContent = "";
        refreshAnchorPreview();
        refreshRuleTree();
        logLine("parse rule details failed: " + e.message);
    }
}

function setShortcutState(active) {
    var hint = document.getElementById("captureHint");
    if (!hint) return;
    hint.textContent = active ? "active" : "not active";
}

function setShortcutText(info) {
    var data = decodePayload(info, null);
    if (data && data.label) {
        document.getElementById("hotkeyText").textContent = data.label;
        return;
    }
    document.getElementById("hotkeyText").textContent = String(info || "Shift+F11");
}

function onShortcutUpdated(payload) {
    if (typeof payload === "object" && payload !== null && payload.label) {
        document.getElementById("hotkeyText").textContent = payload.label || "";
        return;
    }
    var data = decodePayload(payload, null);
    if (data && data.label) {
        document.getElementById("hotkeyText").textContent = data.label;
        return;
    }
    document.getElementById("hotkeyText").textContent = payload || "";
}

function onShortcutCaptured(payload) {
    onShortcutUpdated(payload);
    if (typeof window.onShortcutCaptureStateChanged === "function") {
        window.onShortcutCaptureStateChanged("false");
    }
    logLine("shortcut captured");
}

function onSaveResult(payload) {
    logLine("save result: " + payload);
}

window.onMenuStateChanged = function (stateText) {
    setMenuState(stateText === "true" ? true : false);
};
window.onShortcutUpdated = onShortcutUpdated;
window.onShortcutCaptureStateChanged = function (payload) {
    setShortcutState(payload === "true" || payload === true);
    logLine(payload === "true" ? "capture started" : "capture ended");
};
window.onShortcutCaptured = onShortcutCaptured;
window.onSaveResult = onSaveResult;
window.onRuleSummary = onRuleSummary;
window.onRuleDetails = onRuleDetails;
window.__iifBridgePush = function (name, payload) {
    if (!name) return;
    queueBridgeCall([name], payload);
};
window.__iifBridgeDrain = flushBridgeQueue;
initRuleDetailSections();
window.__iifBridgeDrain();
    

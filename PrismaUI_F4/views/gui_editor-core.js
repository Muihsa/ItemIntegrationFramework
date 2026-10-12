var rules = [];
var selectedRuleId = "";
var selectedRuleSource = "json";
var ruleSummaryRetry = 0;
var ruleSummaryLoaded = false;
var ruleSummaryMaxRetry = 16;
var ruleSummaryRequestStarted = 0;
var ruleSummaryRequestInFlight = false;
var __bridgeNotReadyLogged = {};
var __bridgeQueue = [];
var __bridgeQueueFlushing = false;
var anchorTargetHints = ["TOP", "BOTTOM"];
var detailPanelState = {};
var saveButtonText = "Save This Rule";
var treeVanillaByScene = {
    guns: ["$dmg", "$ammo", "$fireRate", "$rng", "$acc", "$wt", "$val"],
    melee: ["$Melee", "$speed", "$wt", "$val"],
    armor: ["$Armor", "$wt", "$val"],
    aid: ["$wt", "$val"],
    custom: []
};
var treeVanillaLabels = {
    "$dmg": "Damage",
    "$ammo": "Ammo",
    "$fireRate": "Fire Rate",
    "$rng": "Range",
    "$acc": "Accuracy",
    "$wt": "Weight",
    "$val": "Value",
    "$Melee": "Melee Damage",
    "$speed": "Speed",
    "$Armor": "Armor"
};
var detailFormIds = [
    "ruleTitle",
    "ruleDisplayType",
    "ruleState",
    "ruleHighlight",
    "ruleBackground",
    "ruleHideDiff",
    "ruleInvertDiff",
    "ruleValueText",
    "ruleValueAlign",
    "ruleValueStandard",
    "ruleShowBar",
    "ruleShowValue",
    "ruleFillPct",
    "ruleShieldPct",
    "ruleFillColor",
    "ruleValueColor",
    "ruleBackgroundColor",
    "leftTag",
    "leftValue",
    "leftIsIcon",
    "leftState",
    "leftAlign",
    "leftActive",
    "rightTag",
    "rightValue",
    "rightIsIcon",
    "rightState",
    "rightAlign",
    "rightActive"
];

function normalizeBridgeNames(primaryName, fallbackNames) {
    var names = [primaryName];
    if (fallbackNames) {
        if (Array.isArray(fallbackNames)) {
            for (var i = 0; i < fallbackNames.length; ++i) {
                if (fallbackNames[i]) names.push(fallbackNames[i]);
            }
        } else {
            names.push(fallbackNames);
        }
    }
    return names;
}

function queueBridgeCall(names, payload) {
    __bridgeQueue.push({ names: names, payload: payload });
    if (!__bridgeQueueFlushing) {
        __bridgeQueueFlushing = true;
        setTimeout(function () {
            __bridgeQueueFlushing = false;
            flushBridgeQueue();
        }, 30);
    }
}

function callBridgeFunction(primaryName, payload, fallbackNames, notReadyMessage) {
    var names = normalizeBridgeNames(primaryName, fallbackNames);
    for (var i = 0; i < names.length; ++i) {
        var fnName = names[i];
        var fn = window[fnName];
        if (typeof fn === "function") {
            try {
                if (typeof payload === "undefined") {
                    fn();
                } else {
                    fn(payload);
                }
            } catch (error) {
                if (typeof console !== "undefined" && console.error) {
                    console.error("[GUIEditor bridge] callback failed: " + String(error));
                }
                queueBridgeCall(names, payload);
            }
            return fnName;
        }
    }

    if (notReadyMessage && !__bridgeNotReadyLogged[primaryName]) {
        __bridgeNotReadyLogged[primaryName] = true;
        logLine(notReadyMessage);
    }
    queueBridgeCall(names, payload);
    return primaryName;
}

function flushBridgeQueue() {
    if (!__bridgeQueue.length) {
        return;
    }
    var remaining = [];
    for (var i = 0; i < __bridgeQueue.length; i++) {
        var entry = __bridgeQueue[i];
        if (!entry) continue;
        var names = entry.names;
        if (!Array.isArray(names) || !names.length) {
            names = [entry.name];
        }

        var delivered = false;
        for (var j = 0; j < names.length; ++j) {
            var fnName = names[j];
            var fn = window[fnName];
            if (typeof fn !== "function") continue;
            try {
                if (typeof entry.payload === "undefined") {
                    fn();
                } else {
                    fn(entry.payload);
                }
                delivered = true;
                break;
            } catch (error) {
                delivered = true;
                if (typeof console !== "undefined" && console.error) {
                    console.error("[GUIEditor bridge] queued callback failed: " + String(error));
                }
                break;
            }
        }
        if (!delivered) {
            remaining.push(entry);
        }
    }

    __bridgeQueue = remaining;
    if (__bridgeQueue.length) {
        if (!__bridgeQueueFlushing) {
            __bridgeQueueFlushing = true;
            setTimeout(function () {
                __bridgeQueueFlushing = false;
                flushBridgeQueue();
            }, 120);
        }
    } else {
        __bridgeQueueFlushing = false;
    }
}

function safe(value, fallback) { return value === undefined || value === null ? fallback : value; }
function debounce(fn, delay) {
    var t = 0;
    return function () { var self = this, args = arguments; clearTimeout(t); t = setTimeout(function () { fn.apply(self, args); }, delay); };
}
function decodePayload(value, fallback) {
    if (!value) return fallback;
    if (typeof value === "object") return value;
    if (typeof value === "string") {
        try { return JSON.parse(value); } catch (e) { return fallback; }
    }
    return fallback;
}

function toIntDefault(v, fallback) { var n = parseInt(v, 10); return isNaN(n) ? fallback : n; }
function toFloatDefault(v, fallback) { var n = parseFloat(v); return isNaN(n) ? fallback : n; }
function toHexValue(v, fallback) {
    if (v === undefined || v === null) return fallback;
    if (typeof v === "number") return v;
    if (typeof v !== "string") return fallback;
    var s = v.trim();
    if (!s) return fallback;
    if (s.indexOf("0x") === 0 || s.indexOf("0X") === 0) s = s.substring(2);
    if (s.indexOf("#") === 0) s = s.substring(1);
    var n = parseInt(s, 16);
    return isNaN(n) ? fallback : n;
}

function toHexString(v) {
    if (typeof v === "number" && !isNaN(v)) {
        var hex = v.toString(16);
        while (hex.length < 6) hex = "0" + hex;
        return "0x" + hex.toUpperCase();
    }
    return "";
}

function logLine(message) {
    var log = document.getElementById("runtimeLog");
    if (!log) return;
    var now = new Date();
    var stamp = now.toTimeString().slice(0, 8);
    log.textContent = "[" + stamp + "] " + message + "\n" + log.textContent;
    if (typeof console !== "undefined" && console.log) {
        console.log("[GUIEditor] " + message);
    }
}

window.onerror = function (message, source, line, col, error) {
    var detail = "js error: " + String(message) + " (" + String(source || "unknown") + ":" + (line || 0) + ":" + (col || 0) + ")";
    if (error && error.stack) detail += " | " + String(error.stack);
    logLine(detail);
    return false;
};

window.addEventListener("unhandledrejection", function (event) {
    var reason = (event && event.reason) ? String(event.reason) : "unknown";
    logLine("unhandledrejection: " + reason);
});

function parseSelectedPayload(value) {
    if (!value) return { id: "", source: "json" };
    if (typeof value === "string") return { id: value, source: "json" };
    if (typeof value === "object") {
        return {
            id: safe(value.id, ""),
            source: safe(value.source, "json")
        };
    }
    return { id: "", source: "json" };
}

function uniqueAnchorList(list) {
    var out = [];
    var seen = {};
    for (var i = 0; i < list.length; ++i) {
        var id = list[i];
        if (!id || seen[id]) continue;
        seen[id] = true;
        out.push(id);
    }
    return out;
}

function getTreeSceneAnchors(scene) {
    if (!scene || scene === "all") {
        return uniqueAnchorList([].concat(treeVanillaByScene.guns, treeVanillaByScene.melee, treeVanillaByScene.armor, treeVanillaByScene.aid));
    }
    if (scene === "custom") return treeVanillaByScene.custom.slice(0);
    return (treeVanillaByScene[scene] || []).slice(0);
}

function getTreeDisplayAnchors(scene) {
    return getTreeSceneAnchors(scene || "all");
}

function getTreeAnchorLabel(id) {
    if (id === "TOP") return "Top forced zone";
    if (id === "BOTTOM") return "Bottom fallback zone";
    return treeVanillaLabels[id] || id;
}

function isPrimaryVanillaAnchor(id) {
    return id === "$dmg" || id === "$Melee" || id === "$Armor";
}

var _rulesVersion = 0;
var _cachedRuleIndex = null, _cachedRuleIndexVer = -1;
var _cachedVanillaRoots = null;

function getTreeVanillaRoots() {
    if (_cachedVanillaRoots) return _cachedVanillaRoots;
    var all = {};
    for (var k in treeVanillaByScene) {
        if (!Object.prototype.hasOwnProperty.call(treeVanillaByScene, k)) continue;
        var list = treeVanillaByScene[k];
        for (var i = 0; i < list.length; ++i) all[list[i]] = true;
    }
    _cachedVanillaRoots = all;
    return all;
}

function normalizeRuleAnchorTarget(target) {
    return safe(target, "").trim();
}

function getRuleIndex(list) {
    if (list === rules) {
        if (_cachedRuleIndex && _cachedRuleIndexVer === _rulesVersion) return _cachedRuleIndex;
        var map = {};
        for (var i = 0; i < list.length; ++i) {
            var item = list[i];
            if (!item || !item.id) continue;
            map[item.id] = item;
        }
        _cachedRuleIndex = map;
        _cachedRuleIndexVer = _rulesVersion;
        return map;
    }
    var map = {};
    if (!Array.isArray(list)) return map;
    for (var i = 0; i < list.length; ++i) {
        var item = list[i];
        if (!item || !item.id) continue;
        map[item.id] = item;
    }
    return map;
}

function resolvePrimaryAnchor(rule, allRulesMap, vanillaRoots) {
    if (!rule) return { anchor: "BOTTOM", mode: "after" };
    var anchors = readAnchorsFromRule(rule);
    if (!anchors.length) return { anchor: "BOTTOM", mode: "after" };
    for (var i = 0; i < anchors.length; ++i) {
        var target = normalizeRuleAnchorTarget(anchors[i].target);
        var mode = normalizeAnchorMode(anchors[i].mode || "after");
        if (!target) continue;
        if (target === "TOP" || target === "BOTTOM") return { anchor: target, mode: mode };
        if (vanillaRoots[target]) return { anchor: target, mode: mode };
        if (allRulesMap[target]) return { anchor: target, mode: mode };
    }
    return { anchor: "BOTTOM", mode: "after" };
}

function selectedRuleKey(id, source) {
    return (source || "json") + "::" + (id || "");
}

function currentRuleKey() {
    return selectedRuleKey(selectedRuleId, selectedRuleSource);
}

function setInputDisabledByIds(ids, disabled) {
    for (var i = 0; i < ids.length; ++i) {
        var el = document.getElementById(ids[i]);
        if (!el) continue;
        el.disabled = disabled;
    }
}

function showCppMeta(show) {
    var panel = document.getElementById("cppMeta");
    var wrapper = document.getElementById("cppMetaSection");
    if (!panel) return;
    panel.style.display = show ? "block" : "none";
    if (wrapper) wrapper.style.display = show ? "block" : "none";
    var btn = document.getElementById("resetCppOverrideBtn");
    var saveBtn = document.getElementById("saveRuleBtn");
    var delBtn = document.getElementById("deleteRuleBtn");
    if (btn) btn.style.display = show ? "inline-block" : "none";
    if (saveBtn) saveBtn.textContent = show ? "Save CPP Override" : saveButtonText;
    if (delBtn) delBtn.disabled = show;
}

function setRuleDetailSectionState(sectionId, isOpen, fromClick) {
    var body = document.getElementById(sectionId);
    if (!body) return;
    var header = document.querySelector('[data-toggle-target="' + sectionId + '"]');
    if (!header) return;
    var caret = header.querySelector(".detail-caret");
    var open = !!isOpen;
    body.classList.toggle("collapsed", !open);
    detailPanelState[sectionId] = open;
    if (caret) caret.textContent = open ? "\u25BC" : "\u25B8";
    if (fromClick && open) {
        body.scrollIntoView({ behavior: "smooth", block: "nearest" });
    }
}

function isSectionOpen(sectionId) {
    var body = document.getElementById(sectionId);
    if (body) {
        return !body.classList.contains("collapsed");
    }
    var state = detailPanelState[sectionId];
    return state === undefined ? true : !!state;
}

function toggleRuleDetailSection(sectionId) {
    if (!sectionId) return;
    setRuleDetailSectionState(sectionId, !isSectionOpen(sectionId), true);
}

function initRuleDetailSections() {
    var headers = document.querySelectorAll(".section-title-collapsible[data-toggle-target]");
    for (var i = 0; i < headers.length; ++i) {
        var header = headers[i];
        var sectionId = header.getAttribute("data-toggle-target");
        if (!sectionId) continue;
        var body = document.getElementById(sectionId);
        if (!body) continue;

        if (detailPanelState[sectionId] === undefined) {
            detailPanelState[sectionId] = true;
        }
        setRuleDetailSectionState(sectionId, detailPanelState[sectionId], false);
        header.onclick = null;
        header.addEventListener("click", function () {
            var target = this.getAttribute("data-toggle-target");
            toggleRuleDetailSection(target);
        });
    }
}

function updateLastAction(action) {
    var span = document.getElementById("lastAction");
    if (span) span.textContent = action;
}

function setMenuState(opened) {
    var state = opened ? "open" : "closed";
    var badge = document.getElementById("stateBadge");
    var stateSpan = document.getElementById("menuState");
    if (stateSpan) stateSpan.textContent = state;
    if (badge) badge.textContent = state.toUpperCase();
    logLine("menu state changed: " + state);
    if (opened && !ruleSummaryLoaded && !ruleSummaryRequestInFlight) {
        ruleSummaryRetry = 0;
        ensureRuleSummary();
    }
}

function requestClose() {
    if (callBridgeFunction("uiRequestClose")) {
        updateLastAction("close requested");
        return;
    }
    updateLastAction("close API not ready");
}

function requestReload() {
    if (callBridgeFunction("uiRequestReloadRules")) {
        updateLastAction("reload requested");
        return;
    }
    updateLastAction("reload API not ready");
}

function requestSave() {
    if (callBridgeFunction("uiRequestSaveNow")) {
        updateLastAction("save requested");
        return;
    }
    updateLastAction("save API not ready");
}

function startCapture() {
    if (callBridgeFunction("uiRequestCaptureShortcut")) {
        updateLastAction("capture started");
        return;
    }
    updateLastAction("capture API not ready");
}

function refreshRuleList() {
    if (!rules) rules = [];
    ruleSummaryLoaded = false;
    ruleSummaryRequestInFlight = true;
    ruleSummaryRequestStarted = Date.now();
    var called = callBridgeFunction("uiRequestRuleList", undefined, "requestRuleList", "uiRequestRuleList is not ready");
    if (called) {
        updateLastAction("refresh rule list requested" + (called === "requestRuleList" ? " (compat)" : ""));
        return;
    }
    updateLastAction("refresh rule list failed");
    ruleSummaryRequestInFlight = false;
}

function ensureRuleSummary() {
    if (ruleSummaryLoaded) return;
    if (!ruleSummaryRequestStarted) ruleSummaryRequestStarted = Date.now();
    if (Date.now() - ruleSummaryRequestStarted > ruleSummaryMaxRetry * 500) {
        var list = document.getElementById("ruleList");
        if (list) list.innerHTML = 'Loading timed out. <span class="refresh-inline-link" onclick="refreshRuleList()">Click Refresh Rule List.</span>';
        var tree = document.getElementById("ruleTree");
        if (tree) tree.textContent = "Loading timed out.";
        ruleSummaryRequestStarted = 0;
        ruleSummaryRequestInFlight = false;
        logLine("rule summary still not loaded after retries");
        return;
    }
    if (ruleSummaryRequestInFlight) {
        setTimeout(ensureRuleSummary, 500);
        return;
    }
    ruleSummaryRetry += 1;
    logLine("retry rule summary request #" + ruleSummaryRetry);
    refreshRuleList();
    setTimeout(ensureRuleSummary, 500);
}

function repaintRuleListFromCachedData() {
    if (!ruleSummaryLoaded || !rules || rules.length === 0) {
        return;
    }
    onRuleSummary(JSON.stringify({ rules: rules, selected: { id: selectedRuleId, source: selectedRuleSource } }));
}

function requestRuleDetails(ruleId, source) {
    selectedRuleId = ruleId;
    selectedRuleSource = source || "json";
    if (!ruleId) return;
    if (!callBridgeFunction("uiRequestRuleDetails", JSON.stringify({ id: ruleId, source: selectedRuleSource }, null, 0), "requestRuleDetails", "uiRequestRuleDetails is not ready")) {
        logLine("uiRequestRuleDetails is not ready");
    }
    refreshRuleTree();
}

function getRuleSummaryFiltersForTree() {
    var sourceFilter = safe((document.getElementById("treeSourceFilter") || {}).value, "all");
    var sceneFilter = safe((document.getElementById("treeSceneFilter") || {}).value, "all");
    var q = safe((document.getElementById("treeSearch") || {}).value, "").trim().toLowerCase();
    return rules.filter(function (item) {
        if (!item) return false;
        var title = safe(item.title, "").toLowerCase();
        var id = safe(item.id, "").toLowerCase();
        var okSearch = !q || id.indexOf(q) >= 0 || title.indexOf(q) >= 0;
        var itemSource = item.source || "json";
        var okSource =
            sourceFilter === "all" ||
            (sourceFilter === "json" && itemSource === "json") ||
            (sourceFilter === "cpp" && itemSource === "cpp");
        var okScene = sceneFilter === "all" || (item.scene || "custom") === sceneFilter;
        return okSearch && okSource && okScene;
    });
}

function buildTreeNodes() {
    var visibleRules = getRuleSummaryFiltersForTree();
    var allRules = getRuleIndex(rules);
    var vanillaRoots = getTreeVanillaRoots();
    var rootMap = {
        TOP: { before: [], replace: [], after: [] },
        BOTTOM: { before: [], replace: [], after: [] }
    };
    var childMap = {};
    var reachable = {};
    var modeSort = function (a, b) { return safe(a.priority, 0) - safe(b.priority, 0); };

    for (var i = 0; i < visibleRules.length; ++i) {
        var item = visibleRules[i];
        var p = resolvePrimaryAnchor(item, allRules, vanillaRoots);
        var anchorId = p.anchor;
        var mode = p.mode;
        if (anchorId === "TOP" || anchorId === "BOTTOM" || vanillaRoots[anchorId]) {
            if (!rootMap[anchorId]) rootMap[anchorId] = { before: [], replace: [], after: [] };
            if (mode === "before") rootMap[anchorId].before.push(item);
            else if (mode === "replace") rootMap[anchorId].replace.push(item);
            else rootMap[anchorId].after.push(item);
        } else {
            if (!childMap[anchorId]) childMap[anchorId] = [];
            childMap[anchorId].push(item);
        }
    }

    var markReachable = function (id, visited) {
        if (!id || visited[id]) return;
        visited[id] = true;
        if (rootMap[id]) {
            var root = rootMap[id];
            var groups = [].concat(root.before || [], root.replace || [], root.after || []);
            for (var r = 0; r < groups.length; ++r) {
                if (groups[r] && groups[r].id) markReachable(groups[r].id, visited);
            }
        }
        if (childMap[id]) {
            for (var i = 0; i < childMap[id].length; ++i) {
                markReachable(childMap[id][i].id, visited);
            }
        }
    };

    for (var rootKey in rootMap) {
        if (!Object.prototype.hasOwnProperty.call(rootMap, rootKey)) continue;
        rootMap[rootKey].before.sort(modeSort);
        rootMap[rootKey].replace.sort(modeSort);
        rootMap[rootKey].after.sort(modeSort);
    }
    for (var key in childMap) {
        if (!Object.prototype.hasOwnProperty.call(childMap, key)) continue;
        childMap[key].sort(modeSort);
    }

    markReachable("TOP", reachable);
    markReachable("BOTTOM", reachable);

    var displayScene = safe((document.getElementById("treeSceneFilter") || {}).value, "all");
    var displayAnchors = getTreeDisplayAnchors(displayScene);
    for (var vi = 0; vi < displayAnchors.length; ++vi) {
        markReachable(displayAnchors[vi], reachable);
    }

    return { visibleRules: visibleRules, allRules: allRules, rootMap: rootMap, childMap: childMap, reachable: reachable };
}

function resolveAnchorText(item) {
    if (!item) return "";
    var a = resolvePrimaryAnchor(item, getRuleIndex(rules), getTreeVanillaRoots());
    return a.anchor + " " + a.mode;
}
var treeFoldedNodes = {};
var treeDragPayload = null;
var treeParentHighlight = {};

function getRuleByIdAndSource(id, source) {
    var fallback = null;
    var wantedSource = source || "";
    for (var i = 0; i < rules.length; ++i) {
        var item = rules[i];
        if (!item || safe(item.id, "") !== id) continue;
        if (!fallback) fallback = item;
        if (!wantedSource || (item.source || "json") === wantedSource) return item;
    }
    return fallback;
}

function setRuleAnchorsForTree(rule, anchors) {
    if (!rule) return;
    rule.anchors = [];
    rule.anchorTargets = [];
    rule.anchorModes = [];
    for (var i = 0; i < anchors.length; ++i) {
        var target = safe(anchors[i].target, "");
        var mode = normalizeAnchorMode(safe(anchors[i].mode, "after"));
        if (!target) continue;
        rule.anchors.push({ target: target, mode: mode });
        rule.anchorTargets.push(target);
        rule.anchorModes.push(mode);
    }
}

function getDirectTreeChildren(id, childMap, rootMap) {
    var arr = [];
    if (rootMap && rootMap[id]) {
        var root = rootMap[id];
        arr = arr.concat(root.before || [], root.replace || [], root.after || []);
    }
    if (childMap && childMap[id]) {
        arr = arr.concat(childMap[id]);
    }
    return arr;
}

function setTreeFoldRecursive(id, foldState, childMap, rootMap, visited) {
    if (!id) return;
    if (!visited) visited = {};
    if (visited[id]) return;
    visited[id] = true;
    if (foldState) treeFoldedNodes[id] = true;
    else delete treeFoldedNodes[id];
    var children = getDirectTreeChildren(id, childMap, rootMap);
    for (var i = 0; i < children.length; ++i) {
        if (children[i] && children[i].id) setTreeFoldRecursive(children[i].id, foldState, childMap, rootMap, visited);
    }
}

function treeFilterIsActive() {
    var sourceFilter = safe((document.getElementById("treeSourceFilter") || {}).value, "all");
    var q = safe((document.getElementById("treeSearch") || {}).value, "").trim();
    return !!q || sourceFilter !== "all";
}

function buildTreeParentHighlight(data) {
    treeParentHighlight = {};
    if (!selectedRuleId || !data) return;
    var parent = {};
    var markRoot = function (rootId, group) {
        if (!group) return;
        var all = [].concat(group.before || [], group.replace || [], group.after || []);
        for (var i = 0; i < all.length; ++i) {
            if (all[i] && all[i].id && !parent[all[i].id]) parent[all[i].id] = rootId;
        }
    };
    for (var rootId in data.rootMap) {
        if (Object.prototype.hasOwnProperty.call(data.rootMap, rootId)) markRoot(rootId, data.rootMap[rootId]);
    }
    for (var key in data.childMap) {
        if (!Object.prototype.hasOwnProperty.call(data.childMap, key)) continue;
        var children = data.childMap[key];
        for (var i = 0; i < children.length; ++i) {
            if (children[i] && children[i].id) parent[children[i].id] = key;
        }
    }
    var guard = 0;
    var cur = parent[selectedRuleId];
    while (cur && guard++ < 128) {
        treeParentHighlight[cur] = true;
        cur = parent[cur];
    }
}

function persistTreeRulePosition(rule) {
    if (!rule || !rule.id) return;
    var payload = {
        id: rule.id,
        source: rule.source || "json",
        priority: toIntDefault(rule.priority, 800),
        anchors: readAnchorsFromRule(rule)
    };
    if ((rule.source || "json") === "cpp") {
        if (typeof window.uiRequestCppOverrideUpdate === "function") {
            window.uiRequestCppOverrideUpdate(JSON.stringify(payload));
        }
        return;
    }
    if (typeof window.uiRequestRuleUpdate === "function") {
        window.uiRequestRuleUpdate(JSON.stringify(payload));
    }
}

function applyRuleTreeDrop(sourceId, sourceSource, targetId, parentId, parentMode, makeChild) {
    if (!sourceId || !targetId || sourceId === targetId) return;
    var sourceRule = getRuleByIdAndSource(sourceId, sourceSource);
    if (!sourceRule) {
        logLine("tree drop failed: missing source " + sourceId);
        return;
    }
    var touched = [sourceRule];
    var newAnchor = makeChild ? targetId : (parentId || "BOTTOM");
    var newMode = makeChild ? "after" : normalizeAnchorMode(parentMode || "after");
    setRuleAnchorsForTree(sourceRule, [{ target: newAnchor, mode: newMode }]);

    if (!makeChild) {
        var targetRule = getRuleByIdAndSource(targetId, "");
        var targetPriority = targetRule ? toIntDefault(targetRule.priority, 800) : 800;
        var newPriority = targetPriority + 1;
        sourceRule.priority = newPriority;
        for (var i = 0; i < rules.length; ++i) {
            var other = rules[i];
            if (!other || !other.id || other.id === sourceId || other.id === targetId) continue;
            var primary = resolvePrimaryAnchor(other, getRuleIndex(rules), getTreeVanillaRoots());
            if (primary.anchor === newAnchor && normalizeAnchorMode(primary.mode) === newMode && toIntDefault(other.priority, 800) >= newPriority) {
                other.priority = toIntDefault(other.priority, 800) + 1;
                touched.push(other);
            }
        }
    }

    selectedRuleId = sourceRule.id;
    selectedRuleSource = sourceRule.source || sourceSource || "json";
    var priorityInput = document.getElementById("rulePriority");
    if (priorityInput) priorityInput.value = toIntDefault(sourceRule.priority, 800);
    setAnchorRows(sourceRule);
    for (var t = 0; t < touched.length; ++t) persistTreeRulePosition(touched[t]);
    refreshRuleTree();
    if (typeof refreshLivePreview === "function") refreshLivePreview();
    updateLastAction("tree moved: " + sourceId + " " + (makeChild ? "under " : "near ") + targetId);
    logLine("tree moved " + sourceId + " -> " + newAnchor + " " + newMode);
}


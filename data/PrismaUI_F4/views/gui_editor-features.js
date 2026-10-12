// ImGui migration: live card preview.
(function initLivePreviewMigration() {
    var livePreviewLastDetails = null;
    var livePreviewHooked = false;

    function previewLogError(err) {
        try {
            if (typeof logLine === "function") {
                logLine("live preview error: " + (err && err.message ? err.message : err));
            }
        } catch (_) {}
    }

    function ensureLivePreviewPanel() {
        if (document.getElementById("livePreview")) {
            return;
        }

        var anchorSection = document.getElementById("ruleAnchorPreviewSection");
        var anchorPreview = document.getElementById("anchorPreview");
        var insertAfter = anchorSection || (anchorPreview ? anchorPreview.parentNode : null);
        var addAnchorBtn = document.getElementById("addAnchorBtn");
        var fallbackParent = addAnchorBtn && addAnchorBtn.parentNode ? addAnchorBtn.parentNode.parentNode : null;
        var parent = insertAfter && insertAfter.parentNode ? insertAfter.parentNode : fallbackParent;
        if (!parent) {
            return;
        }

        var title = document.createElement("div");
        title.className = "section-title section-title-collapsible";
        title.setAttribute("data-toggle-target", "ruleLivePreviewSection");

        var caret = document.createElement("span");
        caret.className = "detail-caret";
        caret.textContent = "\u25BC";
        title.appendChild(caret);

        var titleText = document.createElement("span");
        titleText.textContent = "LIVE PREVIEW";
        title.appendChild(titleText);

        var body = document.createElement("div");
        body.id = "ruleLivePreviewSection";
        body.className = "detail-collapsible-body";

        var preview = document.createElement("div");
        preview.id = "livePreview";
        preview.className = "live-preview";
        preview.textContent = "No rule selected.";
        body.appendChild(preview);

        title.addEventListener("click", function () {
            var hidden = body.style.display === "none";
            body.style.display = hidden ? "" : "none";
            caret.textContent = hidden ? "\u25BC" : "\u25B6";
        });

        if (insertAfter && insertAfter.parentNode === parent) {
            parent.insertBefore(title, insertAfter.nextSibling);
            parent.insertBefore(body, title.nextSibling);
        } else if (addAnchorBtn && addAnchorBtn.parentNode) {
            parent.insertBefore(title, addAnchorBtn.parentNode);
            parent.insertBefore(body, addAnchorBtn.parentNode);
        } else {
            parent.appendChild(title);
            parent.appendChild(body);
        }
    }

    function el(id) {
        return document.getElementById(id);
    }

    function firstDefined(values, fallback) {
        for (var i = 0; i < values.length; ++i) {
            if (values[i] !== undefined && values[i] !== null && values[i] !== "") {
                return values[i];
            }
        }
        return fallback;
    }

    function getObjValue(obj, names) {
        if (!obj) {
            return undefined;
        }
        for (var i = 0; i < names.length; ++i) {
            if (Object.prototype.hasOwnProperty.call(obj, names[i])) {
                return obj[names[i]];
            }
        }
        return undefined;
    }

    function getFieldValue(names, fallback) {
        var detail = livePreviewLastDetails || {};
        var value = getObjValue(detail, names);
        if (value !== undefined && value !== null && value !== "") {
            return value;
        }

        for (var i = 0; i < names.length; ++i) {
            var name = names[i];
            var candidates = [
                name,
                "rule" + name.charAt(0).toUpperCase() + name.slice(1),
                "detail" + name.charAt(0).toUpperCase() + name.slice(1),
                "field" + name.charAt(0).toUpperCase() + name.slice(1)
            ];
            for (var j = 0; j < candidates.length; ++j) {
                var node = el(candidates[j]);
                if (!node) {
                    continue;
                }
                if (node.type === "checkbox") {
                    return node.checked;
                }
                if (node.value !== undefined && node.value !== null && node.value !== "") {
                    return node.value;
                }
            }
        }
        return fallback;
    }

    function normalizeSource(source) {
        source = String(source || "json").toLowerCase();
        if (source.indexOf("cpp") !== -1) {
            return "cpp";
        }
        if (source.indexOf("vanilla") !== -1 || source.indexOf("game") !== -1) {
            return "vanilla";
        }
        return "json";
    }

    function getRuleByKey(id, source) {
        if (!id || !Array.isArray(rules)) {
            return null;
        }
        var wantedSource = source ? normalizeSource(source) : "";
        var fallback = null;
        for (var i = 0; i < rules.length; ++i) {
            var rule = rules[i];
            if (!rule || String(rule.id || "") !== String(id)) {
                continue;
            }
            if (!fallback) {
                fallback = rule;
            }
            if (!wantedSource || normalizeSource(rule.source || rule.Source || rule.sourceType) === wantedSource) {
                return rule;
            }
        }
        return fallback;
    }

    function makePreviewRuleFromSelection() {
        if (!selectedRuleId) {
            return null;
        }

        var source = normalizeSource(selectedRuleSource || (livePreviewLastDetails && livePreviewLastDetails.source) || "json");
        var base = getRuleByKey(selectedRuleId, source) || {};
        var detail = livePreviewLastDetails || {};
        var merged = {};
        var key;
        for (key in base) {
            if (Object.prototype.hasOwnProperty.call(base, key)) {
                merged[key] = base[key];
            }
        }
        for (key in detail) {
            if (Object.prototype.hasOwnProperty.call(detail, key)) {
                merged[key] = detail[key];
            }
        }

        merged.id = selectedRuleId;
        merged.source = source;
        merged.titleText = getFieldValue(["titleText", "title", "defaultTitle"], firstDefined([merged.titleText, merged.title, merged.defaultTitle], selectedRuleId));
        merged.displayType = Number(getFieldValue(["displayType", "type"], firstDefined([merged.displayType, merged.type], 0)) || 0);
        merged.state = getFieldValue(["state", "valueState"], firstDefined([merged.state, merged.valueState], "normal"));
        merged.valueText = getFieldValue(["valueText", "value"], firstDefined([merged.valueText, merged.value], ""));
        merged.valueColor = getFieldValue(["valueColor", "color"], firstDefined([merged.valueColor, merged.color], "0xFFFFFF"));
        merged.valueStandard = getFieldValue(["valueStandard", "standard"], firstDefined([merged.valueStandard, merged.standard], ""));
        merged.hasBackground = getFieldValue(["hasBackground", "background"], firstDefined([merged.hasBackground, merged.background], false));
        merged.backgroundColor = getFieldValue(["backgroundColor", "bgColor"], firstDefined([merged.backgroundColor, merged.bgColor], "0x000000"));
        merged.showBar = getFieldValue(["showBar"], firstDefined([merged.showBar], true));
        merged.showValue = getFieldValue(["showValue"], firstDefined([merged.showValue], true));
        merged.fillPct = Number(getFieldValue(["fillPct", "fillPercent"], firstDefined([merged.fillPct, merged.fillPercent], 0.65)) || 0);
        merged.shieldPct = Number(getFieldValue(["shieldPct", "shieldPercent"], firstDefined([merged.shieldPct, merged.shieldPercent], 0)) || 0);
        merged.fillColor = getFieldValue(["fillColor", "barColor"], firstDefined([merged.fillColor, merged.barColor], "0xA9D08E"));
        merged.leftTag = getFieldValue(["leftTag"], firstDefined([merged.leftTag], ""));
        merged.leftValue = getFieldValue(["leftValue"], firstDefined([merged.leftValue], ""));
        merged.leftIsIcon = getFieldValue(["leftIsIcon"], firstDefined([merged.leftIsIcon], false));
        merged.leftActive = getFieldValue(["leftActive"], firstDefined([merged.leftActive], true));
        merged.rightTag = getFieldValue(["rightTag"], firstDefined([merged.rightTag], ""));
        merged.rightValue = getFieldValue(["rightValue"], firstDefined([merged.rightValue], ""));
        merged.rightIsIcon = getFieldValue(["rightIsIcon"], firstDefined([merged.rightIsIcon], false));
        merged.rightActive = getFieldValue(["rightActive"], firstDefined([merged.rightActive], true));
        return merged;
    }

    function boolish(value) {
        if (typeof value === "boolean") {
            return value;
        }
        if (typeof value === "number") {
            return value !== 0;
        }
        value = String(value || "").toLowerCase();
        return value === "true" || value === "1" || value === "yes" || value === "on";
    }

    function clamp01(value) {
        value = Number(value);
        if (!isFinite(value)) {
            return 0;
        }
        return Math.max(0, Math.min(1, value));
    }

    function colorToCss(value, fallback) {
        if (value === undefined || value === null || value === "") {
            return fallback || "#ffffff";
        }
        if (typeof value === "number") {
            value = value.toString(16);
        }
        value = String(value).trim();
        if (value.indexOf("#") === 0) {
            return value;
        }
        if (value.indexOf("0x") === 0 || value.indexOf("0X") === 0) {
            value = value.slice(2);
        }
        value = value.replace(/[^0-9a-fA-F]/g, "");
        if (value.length > 6) {
            value = value.slice(value.length - 6);
        }
        while (value.length < 6) {
            value = "0" + value;
        }
        return "#" + value;
    }

    function valueColor(rule) {
        var explicit = colorToCss(rule.valueColor, "");
        if (explicit && explicit !== "#000000") {
            return explicit;
        }
        var state = String(rule.state || "normal").toLowerCase();
        if (state.indexOf("good") !== -1) {
            return "#9fe59f";
        }
        if (state.indexOf("bad") !== -1) {
            return "#ff9a8a";
        }
        if (state.indexOf("star") !== -1) {
            return "#ffd76a";
        }
        return "#ffffff";
    }

    function sampleValueForId(id, fallback) {
        id = String(id || "").toLowerCase();
        if (id.indexOf("ammo") !== -1) {
            return "037/000";
        }
        if (id.indexOf("dmg") !== -1 || id.indexOf("damage") !== -1) {
            return "124";
        }
        if (fallback !== undefined && fallback !== null && fallback !== "") {
            return String(fallback);
        }
        return "---";
    }

    function appendText(parent, className, text) {
        var node = document.createElement("div");
        node.className = className;
        node.textContent = text;
        parent.appendChild(node);
        return node;
    }

    function renderBoxPreview(parent, tag, value, isIcon, active) {
        var box = document.createElement("div");
        box.className = "preview-box";
        if (!boolish(active)) {
            box.style.opacity = "0.45";
        }
        appendText(box, "preview-box-tag", tag || "tag");
        var valueNode = appendText(box, "preview-box-value", "");
        if (boolish(isIcon)) {
            var swatch = document.createElement("span");
            swatch.className = "preview-icon-swatch";
            valueNode.appendChild(swatch);
            valueNode.appendChild(document.createTextNode(value || "icon"));
        } else {
            valueNode.textContent = sampleValueForId(tag, value);
        }
        parent.appendChild(box);
    }

    function getAnchorsForPreview(rule) {
        if (!rule) {
            return [];
        }
        if (String(rule.id || "") === String(selectedRuleId || "") && normalizeSource(rule.source) === normalizeSource(selectedRuleSource || rule.source)) {
            try {
                if (typeof getCurrentAnchors === "function") {
                    var currentAnchors = getCurrentAnchors();
                    if (Array.isArray(currentAnchors)) {
                        return currentAnchors;
                    }
                }
            } catch (_) {}
        }
        try {
            if (typeof readAnchorsFromRule === "function") {
                var anchors = readAnchorsFromRule(rule);
                if (Array.isArray(anchors)) {
                    return anchors;
                }
            }
        } catch (_) {}
        return rule.anchors || rule.AnchorRules || rule.anchorRules || [];
    }

    function getAnchorTarget(anchor) {
        if (!anchor) {
            return "";
        }
        return String(anchor.target || anchor.targetId || anchor.targetID || anchor.parent || anchor.parentId || anchor.parentID || anchor.anchor || anchor.anchorId || anchor.id || "");
    }

    function buildPreviewChildrenMap(currentRule) {
        var map = {};
        var list = Array.isArray(rules) ? rules.slice() : [];
        var replaced = false;
        if (currentRule && currentRule.id) {
            for (var i = 0; i < list.length; ++i) {
                if (String(list[i].id || "") === String(currentRule.id) && normalizeSource(list[i].source) === normalizeSource(currentRule.source)) {
                    list[i] = currentRule;
                    replaced = true;
                    break;
                }
            }
            if (!replaced) {
                list.push(currentRule);
            }
        }

        for (var r = 0; r < list.length; ++r) {
            var rule = list[r];
            if (!rule || !rule.id) {
                continue;
            }
            var anchors = getAnchorsForPreview(rule);
            for (var a = 0; a < anchors.length; ++a) {
                var target = getAnchorTarget(anchors[a]);
                if (!target) {
                    continue;
                }
                if (!map[target]) {
                    map[target] = [];
                }
                map[target].push(rule);
            }
        }

        Object.keys(map).forEach(function (key) {
            map[key].sort(function (a, b) {
                return Number(b.priority || 0) - Number(a.priority || 0);
            });
        });
        return map;
    }

    function createPreviewCard(rule, depth, childrenMap, visited) {
        var id = String(rule && rule.id ? rule.id : selectedRuleId || "");
        var source = normalizeSource(rule && rule.source ? rule.source : selectedRuleSource || "json");
        var card = document.createElement("div");
        card.className = "preview-card" + (depth > 0 ? " preview-child" : "") + (source === "cpp" ? " preview-cpp" : "") + (source === "vanilla" ? " preview-vanilla" : "");
        if (depth > 0) {
            card.style.marginLeft = Math.min(64, depth * 16) + "px";
        }

        var type = Number(rule && rule.displayType !== undefined ? rule.displayType : 0);
        var title = firstDefined([rule && rule.titleText, rule && rule.title, rule && rule.defaultTitle, id], id || "rule");
        var row = document.createElement("div");
        row.className = "preview-row";
        appendText(row, "preview-title", source === "cpp" ? "[CPP] " + title : (source === "vanilla" ? "[GAME] " + title : title));

        if (source !== "json") {
            var externalValue = appendText(row, "preview-value", sampleValueForId(id, rule && rule.valueText));
            externalValue.style.color = source === "cpp" ? "#bcd3ff" : "#ffd76a";
            card.appendChild(row);
            appendText(card, "preview-subtle", id);
        } else if (type === 1) {
            var barValue = boolish(rule.showValue) ? sampleValueForId(id, rule.valueText) : "";
            var barValueNode = appendText(row, "preview-value", barValue);
            barValueNode.style.color = valueColor(rule);
            card.appendChild(row);
            if (boolish(rule.showBar)) {
                var barWrap = document.createElement("div");
                barWrap.className = "preview-bar-wrap";
                var fill = document.createElement("div");
                fill.className = "preview-bar-fill";
                fill.style.width = (clamp01(rule.fillPct) * 100).toFixed(0) + "%";
                fill.style.background = colorToCss(rule.fillColor, "#a9d08e");
                var shield = document.createElement("div");
                shield.className = "preview-bar-shield";
                shield.style.width = (clamp01(rule.shieldPct) * 100).toFixed(0) + "%";
                barWrap.appendChild(fill);
                barWrap.appendChild(shield);
                card.appendChild(barWrap);
            }
            appendText(card, "preview-subtle", id + " | bar");
        } else if (type === 2) {
            card.appendChild(row);
            var boxes = document.createElement("div");
            boxes.className = "preview-boxes";
            renderBoxPreview(boxes, rule.leftTag || "left", rule.leftValue, rule.leftIsIcon, firstDefined([rule.leftActive], true));
            renderBoxPreview(boxes, rule.rightTag || "right", rule.rightValue, rule.rightIsIcon, firstDefined([rule.rightActive], true));
            card.appendChild(boxes);
            appendText(card, "preview-subtle", id + " | boxes");
        } else {
            var valueNode = appendText(row, "preview-value", sampleValueForId(id, rule && rule.valueText));
            valueNode.style.color = valueColor(rule || {});
            if (boolish(rule && rule.hasBackground)) {
                card.style.background = colorToCss(rule.backgroundColor, "#000000");
            }
            card.appendChild(row);
            appendText(card, "preview-subtle", id + " | value");
        }

        if (visited[id]) {
            appendText(card, "preview-subtle", "Cycle stopped.");
            return card;
        }
        visited[id] = true;

        var children = childrenMap[id] || [];
        for (var i = 0; i < children.length; ++i) {
            if (!children[i] || String(children[i].id || "") === id) {
                continue;
            }
            card.appendChild(createPreviewCard(children[i], depth + 1, childrenMap, Object.assign({}, visited)));
        }
        return card;
    }

    function refreshLivePreview() {
        try {
            ensureLivePreviewPanel();
            var host = document.getElementById("livePreview");
            if (!host) {
                return;
            }
            host.textContent = "";

            var rule = makePreviewRuleFromSelection();
            if (!rule || !rule.id) {
                appendText(host, "preview-empty", "No rule selected.");
                return;
            }

            var stack = document.createElement("div");
            stack.className = "preview-stack";
            var childMap = buildPreviewChildrenMap(rule);
            stack.appendChild(createPreviewCard(rule, 0, childMap, {}));
            host.appendChild(stack);
        } catch (err) {
            previewLogError(err);
        }
    }

    function bindLivePreviewInputs() {
        if (livePreviewHooked) {
            return;
        }
        livePreviewHooked = true;
        var debouncedRefreshLivePreview = debounce(refreshLivePreview, 150);
        var inputs = document.querySelectorAll("input, select, textarea");
        for (var i = 0; i < inputs.length; ++i) {
            inputs[i].addEventListener("input", debouncedRefreshLivePreview);
            inputs[i].addEventListener("change", debouncedRefreshLivePreview);
        }
    }

    function hookLivePreviewCallbacks() {
        if (typeof refreshAnchorPreview === "function") {
            var originalRefreshAnchorPreview = refreshAnchorPreview;
            refreshAnchorPreview = function () {
                var result = originalRefreshAnchorPreview.apply(this, arguments);
                refreshLivePreview();
                return result;
            };
        }

        if (typeof onRuleDetails === "function") {
            var originalOnRuleDetails = onRuleDetails;
            onRuleDetails = function (payload) {
                livePreviewLastDetails = payload && (payload.rule || payload.details || payload);
                var result = originalOnRuleDetails.apply(this, arguments);
                refreshLivePreview();
                return result;
            };
            window.onRuleDetails = onRuleDetails;
        }

        if (typeof onRuleSummary === "function") {
            var originalOnRuleSummary = onRuleSummary;
            onRuleSummary = function (payload) {
                var result = originalOnRuleSummary.apply(this, arguments);
                refreshLivePreview();
                return result;
            };
            window.onRuleSummary = onRuleSummary;
        }
    }

    ensureLivePreviewPanel();
    bindLivePreviewInputs();
    hookLivePreviewCallbacks();
    refreshLivePreview();
})();

// ImGui migration: localized labels and richer numeric/color controls.
(function initEditorUxMigration() {
    if (window.__iifEditorUxMigrationReady) return;
    window.__iifEditorUxMigrationReady = true;

    var translations = {
        en_US: {
            "ItemIntegrationFramework Editor": "ItemIntegrationFramework Editor",
            "QUICK TOOLS": "QUICK TOOLS",
            "Close": "Close",
            "Set shortcut": "Set shortcut",
            "Reload JSON rules": "Reload JSON rules",
            "Save JSON now": "Save JSON now",
            "Refresh Rule List": "Refresh Rule List",
            "CREATE NEW RULE": "CREATE NEW RULE",
            "Rule ID": "Rule ID",
            "Default Title": "Default Title",
            "Priority": "Priority",
            "Display Type": "Display Type",
            "File": "File",
            "Create Rule": "Create Rule",
            "RULE LIST": "RULE LIST",
            "RULE RELATIONSHIP TREE": "RULE RELATIONSHIP TREE",
            "RULE DETAILS": "RULE DETAILS",
            "SOURCE: JSON": "SOURCE: JSON",
            "State": "State",
            "Highlight Label": "Highlight Label",
            "Has Background": "Has Background",
            "Hide Difference": "Hide Difference",
            "Invert Diff Color": "Invert Diff Color",
            "Value Text": "Value Text",
            "Value Align": "Value Align",
            "Value Standard": "Value Standard",
            "Value Color (Hex)": "Value Color (Hex)",
            "Background Color (Hex)": "Background Color (Hex)",
            "Show Bar": "Show Bar",
            "Show Value": "Show Value",
            "Fill % (0~1)": "Fill % (0~1)",
            "Shield % (0~1)": "Shield % (0~1)",
            "Fill Color (Hex)": "Fill Color (Hex)",
            "FLEX BOXES": "FLEX BOXES",
            "CPP METADATA": "CPP METADATA",
            "ANCHORS": "ANCHORS",
            "ANCHOR PREVIEW": "ANCHOR PREVIEW",
            "LIVE PREVIEW": "LIVE PREVIEW",
            "Add Anchor": "Add Anchor",
            "Save This Rule": "Save This Rule",
            "Delete Rule": "Delete Rule",
            "RUNTIME LOG": "RUNTIME LOG",
            "Menu:": "Menu:",
            "Shortcut:": "Shortcut:",
            "Capture:": "Capture:",
            "Selected rule:": "Selected rule:",
            "Last action:": "Last action:"
        },
        zh_CN: {
            "ItemIntegrationFramework Editor": "ItemIntegrationFramework 编辑器",
            "QUICK TOOLS": "快捷工具",
            "Close": "关闭",
            "Set shortcut": "设置快捷键",
            "Reload JSON rules": "重载 JSON 规则",
            "Save JSON now": "立即保存 JSON",
            "Refresh Rule List": "刷新规则列表",
            "CREATE NEW RULE": "创建新规则",
            "Rule ID": "规则 ID",
            "Default Title": "默认标题",
            "Priority": "优先级",
            "Display Type": "显示类型",
            "File": "文件",
            "Create Rule": "创建规则",
            "RULE LIST": "规则列表",
            "RULE RELATIONSHIP TREE": "规则关系树",
            "RULE DETAILS": "规则详情",
            "SOURCE: JSON": "来源：JSON",
            "State": "状态",
            "Highlight Label": "高亮标题",
            "Has Background": "启用背景",
            "Hide Difference": "隐藏差值",
            "Invert Diff Color": "反转差值颜色",
            "Value Text": "数值文本",
            "Value Align": "数值对齐",
            "Value Standard": "标准值判定",
            "Value Color (Hex)": "数值颜色 Hex",
            "Background Color (Hex)": "背景颜色 Hex",
            "Show Bar": "显示进度条",
            "Show Value": "显示数值",
            "Fill % (0~1)": "填充比例 (0~1)",
            "Shield % (0~1)": "护盾比例 (0~1)",
            "Fill Color (Hex)": "填充颜色 Hex",
            "FLEX BOXES": "左右信息框",
            "CPP METADATA": "C++ 元数据",
            "ANCHORS": "锚点",
            "ANCHOR PREVIEW": "锚点预览",
            "LIVE PREVIEW": "实时预览",
            "Add Anchor": "添加锚点",
            "Save This Rule": "保存此规则",
            "Delete Rule": "删除规则",
            "RUNTIME LOG": "运行日志",
            "Menu:": "菜单：",
            "Shortcut:": "快捷键：",
            "Capture:": "录入：",
            "Selected rule:": "选中规则：",
            "Last action:": "上次操作："
        }
    };

    function normText(text) {
        return String(text || "").replace(/\s+/g, " ").trim();
    }

    function currentLang() {
        return localStorage.getItem("iif.guiEditor.lang") || "zh_CN";
    }

    function setNodeText(node, text) {
        if (!node) return;
        node.textContent = text;
    }

    function translateExactTextNodes(root, lang) {
        var dict = translations[lang] || translations.en_US;
        var walker = document.createTreeWalker(root || document.body, NodeFilter.SHOW_TEXT, null);
        var nodes = [];
        while (walker.nextNode()) nodes.push(walker.currentNode);
        for (var i = 0; i < nodes.length; ++i) {
            var node = nodes[i];
            var key = normText(node.nodeValue);
            if (!key || !dict[key]) continue;
            node.nodeValue = node.nodeValue.replace(key, dict[key]);
        }
    }

    function applyPlaceholders(lang) {
        var zh = lang === "zh_CN";
        var map = {
            treeSearch: zh ? "搜索树节点 ID ..." : "Search tree ids ...",
            ruleSearch: zh ? "搜索规则 ID 或标题..." : "Search rule id or title...",
            newRuleId: zh ? "Stats_MyNewRule" : "Stats_MyNewRule",
            newRuleTitle: zh ? "显示标题" : "display title"
        };
        Object.keys(map).forEach(function (id) {
            var node = document.getElementById(id);
            if (node) node.setAttribute("placeholder", map[id]);
        });
    }

    function applyLanguage(lang) {
        localStorage.setItem("iif.guiEditor.lang", lang);
        translateExactTextNodes(document.body, lang);
        applyPlaceholders(lang);
        var select = document.getElementById("iifLanguageSelect");
        if (select) select.value = lang;
        try {
            if (typeof updateLastAction === "function") updateLastAction(lang === "zh_CN" ? "语言已切换" : "language changed");
        } catch (_) {}
    }

    function ensureLanguageSelect() {
        if (document.getElementById("iifLanguageSelect")) return;
        var firstToolsPanel = document.querySelector("button");
        if (!firstToolsPanel || !firstToolsPanel.parentNode) return;
        var select = document.createElement("select");
        select.id = "iifLanguageSelect";
        select.className = "iif-lang-select";
        select.innerHTML = '<option value="zh_CN">中文</option><option value="en_US">English</option>';
        select.value = currentLang();
        select.onchange = function () { applyLanguage(select.value); };
        firstToolsPanel.parentNode.insertBefore(select, firstToolsPanel.parentNode.firstChild);
    }

    function cssColorFromHexText(value, fallback) {
        value = String(value || "").trim();
        if (!value) return fallback || "#ffffff";
        if (value.indexOf("#") === 0) return value;
        if (value.indexOf("0x") === 0 || value.indexOf("0X") === 0) value = value.slice(2);
        value = value.replace(/[^0-9a-fA-F]/g, "");
        if (value.length > 6) value = value.slice(value.length - 6);
        while (value.length < 6) value = "0" + value;
        return "#" + value;
    }

    function hexTextFromCssColor(value) {
        value = String(value || "#ffffff").replace("#", "").toUpperCase();
        while (value.length < 6) value = "0" + value;
        return "0x" + value.slice(value.length - 6);
    }

    function enhanceRangeField(id, min, max, step) {
        var input = document.getElementById(id);
        if (!input || input.dataset.iifEnhanced === "1") return;
        input.dataset.iifEnhanced = "1";
        var wrap = document.createElement("div");
        wrap.className = "iif-enhanced-control";
        var range = document.createElement("input");
        range.type = "range";
        range.min = String(min);
        range.max = String(max);
        range.step = String(step);
        var hint = document.createElement("span");
        hint.className = "iif-control-hint";
        function syncFromText() {
            var value = Number(input.value);
            if (!isFinite(value)) value = min;
            value = Math.max(min, Math.min(max, value));
            range.value = String(value);
            hint.textContent = value.toFixed(2);
        }
        function syncFromRange() {
            input.value = range.value;
            hint.textContent = Number(range.value).toFixed(2);
            input.dispatchEvent(new Event("input", { bubbles: true }));
            input.dispatchEvent(new Event("change", { bubbles: true }));
        }
        range.oninput = syncFromRange;
        input.addEventListener("input", syncFromText);
        input.addEventListener("change", syncFromText);
        syncFromText();
        wrap.appendChild(range);
        wrap.appendChild(hint);
        input.insertAdjacentElement("afterend", wrap);
    }

    function enhanceColorField(id, fallback) {
        var input = document.getElementById(id);
        if (!input || input.dataset.iifColorEnhanced === "1") return;
        input.dataset.iifColorEnhanced = "1";
        var wrap = document.createElement("div");
        wrap.className = "iif-enhanced-control";
        var color = document.createElement("input");
        color.type = "color";
        var hint = document.createElement("span");
        hint.className = "iif-control-hint";
        function syncFromText() {
            color.value = cssColorFromHexText(input.value, fallback || "#ffffff");
            hint.textContent = color.value.toUpperCase();
        }
        function syncFromColor() {
            input.value = hexTextFromCssColor(color.value);
            hint.textContent = color.value.toUpperCase();
            input.dispatchEvent(new Event("input", { bubbles: true }));
            input.dispatchEvent(new Event("change", { bubbles: true }));
        }
        color.oninput = syncFromColor;
        input.addEventListener("input", syncFromText);
        input.addEventListener("change", syncFromText);
        syncFromText();
        wrap.appendChild(color);
        wrap.appendChild(hint);
        input.insertAdjacentElement("afterend", wrap);
    }

    function enhanceControls() {
        enhanceRangeField("ruleFillPct", 0, 1, 0.01);
        enhanceRangeField("ruleShieldPct", 0, 1, 0.01);
        enhanceColorField("ruleFillColor", "#A9D08E");
        enhanceColorField("ruleValueColor", "#FFFFFF");
        enhanceColorField("ruleBackgroundColor", "#000000");
    }

    window.iifApplyLanguage = applyLanguage;
    window.iifCurrentLang = currentLang;

    function run() {
        ensureLanguageSelect();
        enhanceControls();
        applyLanguage(currentLang());
    }

    var originalOnRuleDetailsForUx = window.onRuleDetails;
    if (typeof originalOnRuleDetailsForUx === "function") {
        window.onRuleDetails = function () {
            var result = originalOnRuleDetailsForUx.apply(this, arguments);
            setTimeout(function () {
                enhanceControls();
                applyLanguage(currentLang());
            }, 0);
            return result;
        };
    }

    var originalOnRuleSummaryForUx = window.onRuleSummary;
    if (typeof originalOnRuleSummaryForUx === "function") {
        window.onRuleSummary = function () {
            var result = originalOnRuleSummaryForUx.apply(this, arguments);
            setTimeout(function () { applyLanguage(currentLang()); }, 0);
            return result;
        };
    }

    if (document.readyState === "loading") {
        document.addEventListener("DOMContentLoaded", run);
    } else {
        run();
    }
})();

// ImGui migration: mouse-based fallback for PrismaUI where HTML5 draggable may not fire.
(function initTreeMouseDragFallback() {
    if (window.__iifTreeMouseDragFallbackReady) return;
    window.__iifTreeMouseDragFallbackReady = true;

    var state = {
        pending: false,
        dragging: false,
        startX: 0,
        startY: 0,
        sourceId: "",
        sourceSource: "json",
        sourceRow: null,
        ghost: null,
        dropTarget: null
    };
    var toastTimer = 0;

    function showEditorToast(message, kind) {
        var toast = document.getElementById("iifEditorToast");
        if (!toast) {
            toast = document.createElement("div");
            toast.id = "iifEditorToast";
            toast.className = "iif-editor-toast";
            document.body.appendChild(toast);
        }
        toast.textContent = message;
        toast.className = "iif-editor-toast visible" + (kind ? " " + kind : "");
        if (toastTimer) clearTimeout(toastTimer);
        toastTimer = setTimeout(function () {
            toast.classList.remove("visible");
        }, 2600);
    }

    function closestClass(node, classNames) {
        while (node && node !== document) {
            if (node.classList) {
                for (var i = 0; i < classNames.length; ++i) {
                    if (node.classList.contains(classNames[i])) return node;
                }
            }
            node = node.parentNode;
        }
        return null;
    }

    function isInteractiveNode(node) {
        while (node && node !== document) {
            var tag = (node.tagName || "").toLowerCase();
            if (tag === "input" || tag === "select" || tag === "textarea" || tag === "button") return true;
            if (node.classList && node.classList.contains("tree-caret")) return true;
            if (node.classList && node.classList.contains("rule-tree-row")) return false;
            node = node.parentNode;
        }
        return false;
    }

    function clearDropTarget() {
        if (!state.dropTarget) return;
        state.dropTarget.classList.remove("tree-mouse-drop-target", "tree-mouse-drop-child");
        state.dropTarget = null;
    }

    function setDropTarget(target, shiftKey) {
        if (target === state.dropTarget) {
            if (target) target.classList.toggle("tree-mouse-drop-child", !!shiftKey && target.classList.contains("rule-tree-row"));
            return;
        }
        clearDropTarget();
        state.dropTarget = target;
        if (!target) return;
        target.classList.add("tree-mouse-drop-target");
        target.classList.toggle("tree-mouse-drop-child", !!shiftKey && target.classList.contains("rule-tree-row"));
    }

    function makeGhost() {
        var ghost = document.createElement("div");
        ghost.className = "iif-tree-drag-ghost";
        ghost.textContent = "Moving: " + state.sourceId;
        document.body.appendChild(ghost);
        state.ghost = ghost;
    }

    function moveGhost(event) {
        if (!state.ghost) return;
        state.ghost.style.transform = 'translate(' + (event.clientX + 12) + 'px,' + (event.clientY + 12) + 'px)';
        state.ghost.textContent = "Moving: " + state.sourceId + (event.shiftKey ? " | attach as child" : " | reorder");
    }

    function beginDrag(event) {
        state.dragging = true;
        if (state.sourceRow) state.sourceRow.classList.add("dragging");
        makeGhost();
        moveGhost(event);
        try { if (typeof logLine === "function") logLine("tree mouse drag started: " + state.sourceId); } catch (_) {}
    }

    function resetDragState() {
        clearDropTarget();
        if (state.sourceRow) state.sourceRow.classList.remove("dragging");
        if (state.ghost && state.ghost.parentNode) state.ghost.parentNode.removeChild(state.ghost);
        state.pending = false;
        state.dragging = false;
        state.sourceId = "";
        state.sourceSource = "json";
        state.sourceRow = null;
        state.ghost = null;
    }

    function applyRootDrop(rootId) {
        var sourceRule = getRuleByIdAndSource(state.sourceId, state.sourceSource);
        if (!sourceRule) {
            showEditorToast("Move failed: source rule not found.", "warn");
            return false;
        }
        setRuleAnchorsForTree(sourceRule, [{ target: rootId, mode: "after" }]);
        selectedRuleId = sourceRule.id;
        selectedRuleSource = sourceRule.source || state.sourceSource || "json";
        setAnchorRows(sourceRule);
        persistTreeRulePosition(sourceRule);
        refreshRuleTree();
        updateLastAction("tree moved: " + sourceRule.id + " to " + rootId);
        logLine("tree mouse moved " + sourceRule.id + " -> " + rootId + " after");
        showEditorToast("Moved " + sourceRule.id + " to " + rootId + ". Save requested.", "ok");
        return true;
    }

    function applyDrop(event) {
        var target = state.dropTarget;
        if (!target) {
            showEditorToast("Move cancelled: release over a rule or TOP/BOTTOM group.", "warn");
            logLine("tree mouse drop cancelled: no valid target");
            return false;
        }

        if (target.classList.contains("rule-tree-row")) {
            var targetId = target.dataset.ruleId || "";
            if (!targetId || targetId === state.sourceId) {
                showEditorToast("Move cancelled: invalid target.", "warn");
                return false;
            }
            applyRuleTreeDrop(
                state.sourceId,
                state.sourceSource,
                targetId,
                target.dataset.parentId || "BOTTOM",
                target.dataset.parentMode || "after",
                !!event.shiftKey
            );
            logLine("tree mouse drop completed: " + state.sourceId + " -> " + targetId);
            showEditorToast(
                "Moved " + state.sourceId + (event.shiftKey ? " under " : " near ") + targetId + ". Save requested.",
                "ok"
            );
            return true;
        }

        if (target.classList.contains("rule-tree-root")) {
            var rootId = target.dataset.rootId || "";
            if (rootId === "TOP" || rootId === "BOTTOM") return applyRootDrop(rootId);
        }
        showEditorToast("Move cancelled: invalid drop target.", "warn");
        return false;
    }

    function install() {
        var tree = document.getElementById("ruleTree");
        if (!tree || tree.dataset.mouseDragFallback === "1") return;
        tree.dataset.mouseDragFallback = "1";

        tree.addEventListener("mousedown", function (event) {
            if (event.button !== 0) return;
            if (isInteractiveNode(event.target)) return;
            var row = closestClass(event.target, ["rule-tree-row"]);
            if (!row || !row.dataset || !row.dataset.ruleId) return;
            state.pending = true;
            state.dragging = false;
            state.startX = event.clientX;
            state.startY = event.clientY;
            state.sourceId = row.dataset.ruleId;
            state.sourceSource = row.dataset.source || "json";
            state.sourceRow = row;
        });
    }

    var _dragRafPending = false;
    document.addEventListener("mousemove", function (event) {
        if (!state.pending && !state.dragging) return;
        var dx = event.clientX - state.startX;
        var dy = event.clientY - state.startY;
        if (!state.dragging && Math.sqrt(dx * dx + dy * dy) >= 5) beginDrag(event);
        if (!state.dragging) return;
        event.preventDefault();
        if (_dragRafPending) return;
        _dragRafPending = true;
        var ex = event.clientX, ey = event.clientY, shift = event.shiftKey;
        requestAnimationFrame(function () {
            _dragRafPending = false;
            moveGhost({ clientX: ex, clientY: ey, shiftKey: shift });
            var target = closestClass(document.elementFromPoint(ex, ey), ["rule-tree-row", "rule-tree-root"]);
            if (target && target.classList.contains("rule-tree-row") && target.dataset.ruleId === state.sourceId) target = null;
            if (target && target.classList.contains("rule-tree-root") && !target.dataset.rootId) target = null;
            setDropTarget(target, shift);
        });
    }, true);

    document.addEventListener("mouseup", function (event) {
        if (!state.pending && !state.dragging) return;
        var wasDragging = state.dragging;
        if (wasDragging) {
            event.preventDefault();
            applyDrop(event);
        }
        resetDragState();
    }, true);

    var originalRefreshRuleTreeForMouseDrag = window.refreshRuleTree || (typeof refreshRuleTree === "function" ? refreshRuleTree : null);
    if (typeof originalRefreshRuleTreeForMouseDrag === "function") {
        window.refreshRuleTree = refreshRuleTree = function () {
            var result = originalRefreshRuleTreeForMouseDrag.apply(this, arguments);
            setTimeout(install, 0);
            return result;
        };
    }

    if (document.readyState === "loading") {
        document.addEventListener("DOMContentLoaded", install);
    } else {
        install();
    }
})();

// ImGui migration: create/delete validation and confirmation.
(function initRuleActionConfirmationMigration() {
    if (window.__iifRuleActionConfirmationReady) return;
    window.__iifRuleActionConfirmationReady = true;

    function textOf(node) {
        return String((node && node.textContent) || "").replace(/\s+/g, " ").trim();
    }

    function val(id, fallback) {
        var node = document.getElementById(id);
        if (!node) return fallback || "";
        return String(node.value || fallback || "").trim();
    }

    function toast(message, kind) {
        var toastNode = document.getElementById("iifEditorToast");
        if (!toastNode) {
            toastNode = document.createElement("div");
            toastNode.id = "iifEditorToast";
            toastNode.className = "iif-editor-toast";
            document.body.appendChild(toastNode);
        }
        toastNode.textContent = message;
        toastNode.className = "iif-editor-toast visible" + (kind ? " " + kind : "");
        setTimeout(function () { toastNode.classList.remove("visible"); }, 2600);
        try { if (typeof logLine === "function") logLine(message); } catch (_) {}
    }

    function findButtonByText(labels) {
        var buttons = document.querySelectorAll("button");
        for (var i = 0; i < buttons.length; ++i) {
            var txt = textOf(buttons[i]).toLowerCase();
            for (var j = 0; j < labels.length; ++j) {
                if (txt === labels[j].toLowerCase()) return buttons[i];
            }
        }
        return null;
    }

    function findCreateButton() {
        return document.getElementById("createRuleBtn") || findButtonByText(["Create Rule", "创建规则"]);
    }

    function findDeleteButton() {
        return document.getElementById("deleteRuleBtn") || findButtonByText(["Delete Rule", "删除规则"]);
    }

    function hasRuleId(id) {
        if (!id || !Array.isArray(rules)) return false;
        for (var i = 0; i < rules.length; ++i) {
            if (rules[i] && String(rules[i].id || "") === id) return true;
        }
        return false;
    }

    function validateNewRule() {
        var id = val("newRuleId", "");
        if (!id) {
            toast("Create failed: Rule ID is empty.", "warn");
            return false;
        }
        if (id === "TOP" || id === "BOTTOM") {
            toast("Create failed: TOP and BOTTOM are reserved anchor IDs.", "warn");
            return false;
        }
        if (/\s/.test(id)) {
            toast("Create failed: Rule ID cannot contain spaces.", "warn");
            return false;
        }
        if (hasRuleId(id)) {
            toast("Create failed: duplicate Rule ID: " + id, "warn");
            return false;
        }
        return true;
    }

    function showConfirm(title, body, onConfirm) {
        var old = document.getElementById("iifConfirmModal");
        if (old && old.parentNode) old.parentNode.removeChild(old);

        var backdrop = document.createElement("div");
        backdrop.id = "iifConfirmModal";
        backdrop.className = "iif-modal-backdrop";

        var modal = document.createElement("div");
        modal.className = "iif-modal";
        var titleNode = document.createElement("div");
        titleNode.className = "iif-modal-title";
        titleNode.textContent = title;
        var bodyNode = document.createElement("div");
        bodyNode.className = "iif-modal-body";
        bodyNode.textContent = body;
        var actions = document.createElement("div");
        actions.className = "iif-modal-actions";
        var cancel = document.createElement("button");
        cancel.textContent = "Cancel";
        var ok = document.createElement("button");
        ok.textContent = "Confirm";

        function close() {
            if (backdrop.parentNode) backdrop.parentNode.removeChild(backdrop);
        }
        cancel.onclick = function () { close(); toast("Action cancelled."); };
        ok.onclick = function () {
            close();
            if (typeof onConfirm === "function") onConfirm();
        };
        backdrop.onclick = function (event) {
            if (event.target === backdrop) close();
        };

        actions.appendChild(cancel);
        actions.appendChild(ok);
        modal.appendChild(titleNode);
        modal.appendChild(bodyNode);
        modal.appendChild(actions);
        backdrop.appendChild(modal);
        document.body.appendChild(backdrop);
        ok.focus();
    }

    function requestDeleteSelectedWithModal() {
        if (!selectedRuleId) {
            toast("Delete failed: no rule selected.", "warn");
            return;
        }
        if (selectedRuleSource === "cpp") {
            toast("Delete failed: C++ rule overrides cannot be deleted here.", "warn");
            return;
        }
        if (typeof window.uiRequestRuleDelete !== "function") {
            toast("Delete failed: delete API is not ready.", "warn");
            return;
        }
        var id = selectedRuleId;
        showConfirm("Delete rule", "Delete JSON rule " + id + "?\n\nThis updates the editor rule data and requests a save.", function () {
            window.uiRequestRuleDelete(JSON.stringify({ id: id }));
            if (typeof updateLastAction === "function") updateLastAction("delete requested: " + id);
            toast("Delete requested: " + id, "ok");
        });
    }

    function interceptButtons() {
        var createBtn = findCreateButton();
        if (createBtn && createBtn.dataset.iifCreateValidation !== "1") {
            createBtn.dataset.iifCreateValidation = "1";
            createBtn.addEventListener("click", function (event) {
                if (!validateNewRule()) {
                    event.preventDefault();
                    event.stopImmediatePropagation();
                }
            }, true);
        }

        var deleteBtn = findDeleteButton();
        if (deleteBtn && deleteBtn.dataset.iifDeleteValidation !== "1") {
            deleteBtn.dataset.iifDeleteValidation = "1";
            deleteBtn.addEventListener("click", function (event) {
                event.preventDefault();
                event.stopImmediatePropagation();
                requestDeleteSelectedWithModal();
            }, true);
        }
    }

    var originalCreateNames = ["createRule", "createNewRule", "requestCreateRule"];
    for (var i = 0; i < originalCreateNames.length; ++i) {
        var name = originalCreateNames[i];
        if (typeof window[name] === "function" && !window[name].__iifWrapped) {
            (function (fnName, original) {
                var wrapped = function () {
                    if (!validateNewRule()) return;
                    return original.apply(this, arguments);
                };
                wrapped.__iifWrapped = true;
                window[fnName] = wrapped;
            })(name, window[name]);
        }
    }

    if (typeof window.deleteCurrentRule === "function" && !window.deleteCurrentRule.__iifWrapped) {
        var wrappedDelete = function () { requestDeleteSelectedWithModal(); };
        wrappedDelete.__iifWrapped = true;
        window.deleteCurrentRule = wrappedDelete;
    }

    if (document.readyState === "loading") {
        document.addEventListener("DOMContentLoaded", interceptButtons);
    } else {
        interceptButtons();
    }
    setTimeout(interceptButtons, 0);
    setTimeout(interceptButtons, 800);
})();

// ImGui migration: duplicate selected JSON rule.
(function initRuleDuplicateMigration() {
    if (window.__iifRuleDuplicateReady) return;
    window.__iifRuleDuplicateReady = true;

    function toast(message, kind) {
        var toastNode = document.getElementById("iifEditorToast");
        if (!toastNode) {
            toastNode = document.createElement("div");
            toastNode.id = "iifEditorToast";
            toastNode.className = "iif-editor-toast";
            document.body.appendChild(toastNode);
        }
        toastNode.textContent = message;
        toastNode.className = "iif-editor-toast visible" + (kind ? " " + kind : "");
        setTimeout(function () { toastNode.classList.remove("visible"); }, 2600);
        try { if (typeof logLine === "function") logLine(message); } catch (_) {}
    }

    function existingRuleId(id) {
        if (!id || !Array.isArray(rules)) return false;
        for (var i = 0; i < rules.length; ++i) {
            if (rules[i] && String(rules[i].id || "") === id) return true;
        }
        return false;
    }

    function getCurrentRuleSummaryForDuplicate() {
        if (!selectedRuleId || !Array.isArray(rules)) return null;
        for (var i = 0; i < rules.length; ++i) {
            if (!rules[i]) continue;
            if (String(rules[i].id || "") === selectedRuleId && String(rules[i].source || "json") === String(selectedRuleSource || "json")) {
                return rules[i];
            }
        }
        return null;
    }

    function makeCopyId(baseId) {
        var base = String(baseId || "Rule").replace(/\s+/g, "_");
        var candidate = base + "_Copy";
        var index = 2;
        while (existingRuleId(candidate)) {
            candidate = base + "_Copy" + index;
            index += 1;
        }
        return candidate;
    }

    function closeModal(backdrop) {
        if (backdrop && backdrop.parentNode) backdrop.parentNode.removeChild(backdrop);
    }

    function showDuplicateDialog() {
        if (!selectedRuleId) {
            toast("Duplicate failed: no rule selected.", "warn");
            return;
        }
        if ((selectedRuleSource || "json") !== "json") {
            toast("Duplicate failed: only JSON rules can be duplicated.", "warn");
            return;
        }
        if (typeof window.uiRequestRuleCreate !== "function") {
            toast("Duplicate failed: create API is not ready.", "warn");
            return;
        }

        var source = getCurrentRuleSummaryForDuplicate();
        var old = document.getElementById("iifDuplicateModal");
        if (old && old.parentNode) old.parentNode.removeChild(old);

        var backdrop = document.createElement("div");
        backdrop.id = "iifDuplicateModal";
        backdrop.className = "iif-modal-backdrop";
        var modal = document.createElement("div");
        modal.className = "iif-modal";
        var title = document.createElement("div");
        title.className = "iif-modal-title";
        title.textContent = "Duplicate rule";
        var body = document.createElement("div");
        body.className = "iif-modal-body";
        body.appendChild(document.createTextNode("Create a full JSON copy from " + selectedRuleId + "."));

        var idLabel = document.createElement("div");
        idLabel.className = "iif-modal-field-label";
        idLabel.textContent = "New Rule ID";
        var idInput = document.createElement("input");
        idInput.className = "iif-modal-input";
        idInput.value = makeCopyId(selectedRuleId);
        body.appendChild(idLabel);
        body.appendChild(idInput);

        var titleLabel = document.createElement("div");
        titleLabel.className = "iif-modal-field-label";
        titleLabel.textContent = "New Title";
        var titleInput = document.createElement("input");
        titleInput.className = "iif-modal-input";
        titleInput.value = ((source && (source.title || source.titleText)) || selectedRuleId) + " Copy";
        body.appendChild(titleLabel);
        body.appendChild(titleInput);

        var fileLabel = document.createElement("div");
        fileLabel.className = "iif-modal-field-label";
        fileLabel.textContent = "File";
        var fileInput = document.createElement("input");
        fileInput.className = "iif-modal-input";
        var createFile = document.getElementById("newRuleFile");
        fileInput.value = (createFile && createFile.value) || (source && source.originPath) || "gui_editor_rules.json";
        body.appendChild(fileLabel);
        body.appendChild(fileInput);

        var actions = document.createElement("div");
        actions.className = "iif-modal-actions";
        var cancel = document.createElement("button");
        cancel.textContent = "Cancel";
        var ok = document.createElement("button");
        ok.textContent = "Duplicate";
        cancel.onclick = function () { closeModal(backdrop); toast("Duplicate cancelled."); };
        ok.onclick = function () {
            var newId = String(idInput.value || "").trim();
            if (!newId) { toast("Duplicate failed: new Rule ID is empty.", "warn"); return; }
            if (newId === "TOP" || newId === "BOTTOM") { toast("Duplicate failed: reserved Rule ID.", "warn"); return; }
            if (/\s/.test(newId)) { toast("Duplicate failed: Rule ID cannot contain spaces.", "warn"); return; }
            if (existingRuleId(newId)) { toast("Duplicate failed: duplicate Rule ID: " + newId, "warn"); return; }

            var payload = {
                id: newId,
                cloneFrom: selectedRuleId,
                titleText: String(titleInput.value || newId),
                originPath: String(fileInput.value || "gui_editor_rules.json"),
                priority: source && source.priority !== undefined ? Number(source.priority) + 1 : 801,
                anchors: source && source.anchors ? source.anchors : []
            };
            window.uiRequestRuleCreate(JSON.stringify(payload));
            closeModal(backdrop);
            toast("Duplicate requested: " + selectedRuleId + " -> " + newId, "ok");
            if (typeof updateLastAction === "function") updateLastAction("duplicate requested: " + newId);
        };
        backdrop.onclick = function (event) {
            if (event.target === backdrop) closeModal(backdrop);
        };
        actions.appendChild(cancel);
        actions.appendChild(ok);
        modal.appendChild(title);
        modal.appendChild(body);
        modal.appendChild(actions);
        backdrop.appendChild(modal);
        document.body.appendChild(backdrop);
        idInput.focus();
        idInput.select();
    }

    function installDuplicateButton() {
        if (document.getElementById("duplicateRuleBtn")) return;
        var saveBtn = document.getElementById("saveRuleBtn");
        var deleteBtn = document.getElementById("deleteRuleBtn");
        var anchor = deleteBtn || saveBtn;
        if (!anchor || !anchor.parentNode) return;
        var btn = document.createElement("button");
        btn.id = "duplicateRuleBtn";
        btn.textContent = "Duplicate Rule";
        btn.onclick = showDuplicateDialog;
        anchor.parentNode.insertBefore(btn, deleteBtn || anchor.nextSibling);
    }

    if (document.readyState === "loading") {
        document.addEventListener("DOMContentLoaded", installDuplicateButton);
    } else {
        installDuplicateButton();
    }
    setTimeout(installDuplicateButton, 0);
    setTimeout(installDuplicateButton, 800);
})();

// ImGui migration: rename selected JSON rule and update anchor references in C++.
(function initRuleRenameMigration() {
    if (window.__iifRuleRenameReady) return;
    window.__iifRuleRenameReady = true;

    function toast(message, kind) {
        var toastNode = document.getElementById("iifEditorToast");
        if (!toastNode) {
            toastNode = document.createElement("div");
            toastNode.id = "iifEditorToast";
            toastNode.className = "iif-editor-toast";
            document.body.appendChild(toastNode);
        }
        toastNode.textContent = message;
        toastNode.className = "iif-editor-toast visible" + (kind ? " " + kind : "");
        setTimeout(function () { toastNode.classList.remove("visible"); }, 2600);
        try { if (typeof logLine === "function") logLine(message); } catch (_) {}
    }

    function existingRuleId(id) {
        if (!id || !Array.isArray(rules)) return false;
        for (var i = 0; i < rules.length; ++i) {
            if (rules[i] && String(rules[i].id || "") === id) return true;
        }
        return false;
    }

    function getSelectedRuleSummary() {
        if (!selectedRuleId || !Array.isArray(rules)) return null;
        for (var i = 0; i < rules.length; ++i) {
            if (!rules[i]) continue;
            if (String(rules[i].id || "") === selectedRuleId && String(rules[i].source || "json") === String(selectedRuleSource || "json")) {
                return rules[i];
            }
        }
        return null;
    }

    function closeModal(backdrop) {
        if (backdrop && backdrop.parentNode) backdrop.parentNode.removeChild(backdrop);
    }

    function validateNewId(newId) {
        if (!newId) return "Rename failed: new Rule ID is empty.";
        if (newId === "TOP" || newId === "BOTTOM") return "Rename failed: TOP and BOTTOM are reserved anchor IDs.";
        if (/\s/.test(newId)) return "Rename failed: Rule ID cannot contain spaces.";
        if (newId !== selectedRuleId && existingRuleId(newId)) return "Rename failed: duplicate Rule ID: " + newId;
        return "";
    }

    function showRenameDialog() {
        if (!selectedRuleId) {
            toast("Rename failed: no rule selected.", "warn");
            return;
        }
        if ((selectedRuleSource || "json") !== "json") {
            toast("Rename failed: only JSON rules can be renamed.", "warn");
            return;
        }
        if (typeof window.uiRequestRuleRename !== "function") {
            toast("Rename failed: rename API is not ready.", "warn");
            return;
        }

        var summary = getSelectedRuleSummary();
        var old = document.getElementById("iifRenameModal");
        if (old && old.parentNode) old.parentNode.removeChild(old);

        var backdrop = document.createElement("div");
        backdrop.id = "iifRenameModal";
        backdrop.className = "iif-modal-backdrop";
        var modal = document.createElement("div");
        modal.className = "iif-modal";
        var title = document.createElement("div");
        title.className = "iif-modal-title";
        title.textContent = "Rename rule";
        var body = document.createElement("div");
        body.className = "iif-modal-body";
        body.appendChild(document.createTextNode("Rename JSON rule " + selectedRuleId + ". Anchor references will be updated."));

        var idLabel = document.createElement("div");
        idLabel.className = "iif-modal-field-label";
        idLabel.textContent = "New Rule ID";
        var idInput = document.createElement("input");
        idInput.className = "iif-modal-input";
        idInput.value = selectedRuleId;
        body.appendChild(idLabel);
        body.appendChild(idInput);

        var titleLabel = document.createElement("div");
        titleLabel.className = "iif-modal-field-label";
        titleLabel.textContent = "Title";
        var titleInput = document.createElement("input");
        titleInput.className = "iif-modal-input";
        var titleField = document.getElementById("ruleTitle");
        titleInput.value = (titleField && titleField.value) || (summary && (summary.title || summary.titleText)) || selectedRuleId;
        body.appendChild(titleLabel);
        body.appendChild(titleInput);

        var actions = document.createElement("div");
        actions.className = "iif-modal-actions";
        var cancel = document.createElement("button");
        cancel.textContent = "Cancel";
        var ok = document.createElement("button");
        ok.textContent = "Rename";
        cancel.onclick = function () { closeModal(backdrop); toast("Rename cancelled."); };
        ok.onclick = function () {
            var newId = String(idInput.value || "").trim();
            var error = validateNewId(newId);
            if (error) { toast(error, "warn"); return; }
            var oldId = selectedRuleId;
            window.uiRequestRuleRename(JSON.stringify({
                oldId: oldId,
                newId: newId,
                titleText: String(titleInput.value || newId),
                updateAnchors: true
            }));
            closeModal(backdrop);
            toast("Rename requested: " + oldId + " -> " + newId, "ok");
            if (typeof updateLastAction === "function") updateLastAction("rename requested: " + oldId + " -> " + newId);
        };
        backdrop.onclick = function (event) {
            if (event.target === backdrop) closeModal(backdrop);
        };
        actions.appendChild(cancel);
        actions.appendChild(ok);
        modal.appendChild(title);
        modal.appendChild(body);
        modal.appendChild(actions);
        backdrop.appendChild(modal);
        document.body.appendChild(backdrop);
        idInput.focus();
        idInput.select();
    }

    function installRenameButton() {
        if (document.getElementById("renameRuleBtn")) return;
        var saveBtn = document.getElementById("saveRuleBtn");
        var duplicateBtn = document.getElementById("duplicateRuleBtn");
        var anchor = duplicateBtn || saveBtn;
        if (!anchor || !anchor.parentNode) return;
        var btn = document.createElement("button");
        btn.id = "renameRuleBtn";
        btn.textContent = "Rename Rule";
        btn.onclick = showRenameDialog;
        anchor.parentNode.insertBefore(btn, anchor);
    }

    if (document.readyState === "loading") {
        document.addEventListener("DOMContentLoaded", installRenameButton);
    } else {
        installRenameButton();
    }
    setTimeout(installRenameButton, 0);
    setTimeout(installRenameButton, 800);
})();

// ImGui migration: vanilla game anchor nodes.
(function initVanillaAnchorNodeMigration() {
    if (window.__iifVanillaAnchorNodeReady) return;
    window.__iifVanillaAnchorNodeReady = true;

    function anchorLabel(id) {
        return typeof getTreeAnchorLabel === "function" ? getTreeAnchorLabel(id) : id;
    }

    function allVanillaAnchorsForCurrentScene() {
        var scene = "all";
        var sceneNode = document.getElementById("treeSceneFilter");
        if (sceneNode) scene = sceneNode.value || "all";
        if (typeof getTreeDisplayAnchors === "function") return getTreeDisplayAnchors(scene).slice(0);
        return [];
    }

    function selectedIsVanillaAnchor() {
        if (!selectedRuleId) return false;
        if (selectedRuleSource === "vanilla") return true;
        var anchors = allVanillaAnchorsForCurrentScene();
        return anchors.indexOf(selectedRuleId) >= 0 || selectedRuleId === "TOP" || selectedRuleId === "BOTTOM";
    }

    function showVanillaAnchorDetails(id) {
        selectedRuleId = id;
        selectedRuleSource = "vanilla";
        var sourceLabel = document.getElementById("ruleSourceLabel") || document.getElementById("sourceLabel");
        if (sourceLabel) sourceLabel.textContent = "SOURCE: GAME";
        var selectedLabel = document.getElementById("selectedRuleLabel") || document.getElementById("selectedRule");
        if (selectedLabel) selectedLabel.textContent = id;
        var anchorPreview = document.getElementById("anchorPreview");
        if (anchorPreview) {
            anchorPreview.textContent =
                "Game anchor selected: " + id + "\n" +
                "Label: " + anchorLabel(id) + "\n\n" +
                "This is a vanilla/FallUI insertion point. It is not editable as a JSON rule.\n" +
                "Drag JSON or C++ rules onto this row to attach them after this anchor.";
        }
        var livePreview = document.getElementById("livePreview");
        if (livePreview) {
            livePreview.innerHTML = "";
            var card = document.createElement("div");
            card.className = "preview-card preview-vanilla";
            var row = document.createElement("div");
            row.className = "preview-row";
            var title = document.createElement("div");
            title.className = "preview-title";
            title.textContent = "[GAME] " + anchorLabel(id);
            var value = document.createElement("div");
            value.className = "preview-value";
            value.textContent = id.indexOf("ammo") >= 0 ? "037/000" : (id.indexOf("dmg") >= 0 ? "124" : "---");
            row.appendChild(title);
            row.appendChild(value);
            card.appendChild(row);
            var subtle = document.createElement("div");
            subtle.className = "preview-subtle";
            subtle.textContent = id + " | anchor only";
            card.appendChild(subtle);
            livePreview.appendChild(card);
        }
        if (typeof refreshRuleTree === "function") refreshRuleTree();
        try { if (typeof logLine === "function") logLine("selected game anchor: " + id); } catch (_) {}
    }
    window.showVanillaAnchorDetails = showVanillaAnchorDetails;

    function renderVanillaAnchorRows() {
        return;
    }

    var originalRefreshRuleTreeForVanillaAnchors = window.refreshRuleTree || (typeof refreshRuleTree === "function" ? refreshRuleTree : null);
    if (typeof originalRefreshRuleTreeForVanillaAnchors === "function") {
        window.refreshRuleTree = refreshRuleTree = function () {
            var result = originalRefreshRuleTreeForVanillaAnchors.apply(this, arguments);
            var tree = document.getElementById("ruleTree");
            if (tree) tree.dataset.vanillaAnchorsRendered = "0";
            renderVanillaAnchorRows();
            return result;
        };
    }

    var originalRefreshAnchorTargetHintsForVanillaAnchors = window.refreshAnchorTargetHints || (typeof refreshAnchorTargetHints === "function" ? refreshAnchorTargetHints : null);
    if (typeof originalRefreshAnchorTargetHintsForVanillaAnchors === "function") {
        window.refreshAnchorTargetHints = refreshAnchorTargetHints = function () {
            originalRefreshAnchorTargetHintsForVanillaAnchors.apply(this, arguments);
            var anchors = allVanillaAnchorsForCurrentScene();
            for (var i = 0; i < anchors.length; ++i) {
                if (anchorTargetHints.indexOf(anchors[i]) < 0) anchorTargetHints.push(anchors[i]);
            }
        };
    }

    if (document.readyState === "loading") {
        document.addEventListener("DOMContentLoaded", renderVanillaAnchorRows);
    } else {
        renderVanillaAnchorRows();
    }
    setTimeout(renderVanillaAnchorRows, 0);
    setTimeout(renderVanillaAnchorRows, 800);
})();

// ImGui migration: advanced JSON block editing for condition/result/valuesMapping.
(function initAdvancedRuleJsonMigration() {
    if (window.__iifAdvancedRuleJsonReady) return;
    window.__iifAdvancedRuleJsonReady = true;

    function toast(message, kind) {
        var toastNode = document.getElementById("iifEditorToast");
        if (!toastNode) {
            toastNode = document.createElement("div");
            toastNode.id = "iifEditorToast";
            toastNode.className = "iif-editor-toast";
            document.body.appendChild(toastNode);
        }
        toastNode.textContent = message;
        toastNode.className = "iif-editor-toast visible" + (kind ? " " + kind : "");
        setTimeout(function () { toastNode.classList.remove("visible"); }, 2800);
        try { if (typeof logLine === "function") logLine(message); } catch (_) {}
    }

    function pretty(value) {
        return JSON.stringify(value === undefined ? null : value, null, 2);
    }

    function parseEditor(id, fallback) {
        var node = document.getElementById(id);
        if (!node) return fallback;
        var raw = String(node.value || "").trim();
        if (!raw) return fallback;
        return JSON.parse(raw);
    }

    function ensureAdvancedSection() {
        if (document.getElementById("ruleAdvancedJsonSection")) return;
        var anchorSection = document.getElementById("ruleAnchorsSection") || document.getElementById("cppMetaSection") || document.getElementById("ruleAnchorPreviewSection");
        var parent = anchorSection && anchorSection.parentNode ? anchorSection.parentNode : null;
        if (!parent) {
            var saveBtn = document.getElementById("saveRuleBtn");
            parent = saveBtn && saveBtn.parentNode ? saveBtn.parentNode.parentNode : null;
        }
        if (!parent) return;

        var title = document.createElement("div");
        title.className = "section-title section-title-collapsible";
        title.setAttribute("data-toggle-target", "ruleAdvancedJsonSection");
        var caret = document.createElement("span");
        caret.className = "detail-caret";
        caret.textContent = "\u25B6";
        var label = document.createElement("span");
        label.textContent = "ADVANCED JSON";
        title.appendChild(caret);
        title.appendChild(label);

        var body = document.createElement("div");
        body.id = "ruleAdvancedJsonSection";
        body.className = "detail-collapsible-body collapsed";

        var toolbar = document.createElement("div");
        toolbar.className = "iif-advanced-toolbar";
        var formatBtn = document.createElement("button");
        formatBtn.textContent = "Format JSON";
        formatBtn.onclick = function () {
            try {
                ["advancedConditionJson", "advancedResultJson", "advancedValuesMappingJson"].forEach(function (id) {
                    var node = document.getElementById(id);
                    if (node) node.value = pretty(JSON.parse(node.value || "{}"));
                });
                toast("Advanced JSON formatted.", "ok");
            } catch (err) {
                toast("Format failed: " + err.message, "warn");
            }
        };
        toolbar.appendChild(formatBtn);
        body.appendChild(toolbar);

        var wrap = document.createElement("div");
        wrap.className = "iif-advanced-json-wrap";
        function addEditor(id, titleText, rows) {
            var l = document.createElement("div");
            l.className = "iif-advanced-json-label";
            l.textContent = titleText;
            var t = document.createElement("textarea");
            t.id = id;
            t.className = "iif-advanced-json-editor";
            t.rows = rows;
            wrap.appendChild(l);
            wrap.appendChild(t);
        }
        addEditor("advancedConditionJson", "Condition / Match", 5);
        addEditor("advancedResultJson", "Result Block", 8);
        addEditor("advancedValuesMappingJson", "Values Mapping", 10);
        body.appendChild(wrap);

        title.onclick = function () {
            var collapsed = body.classList.toggle("collapsed");
            caret.textContent = collapsed ? "\u25B6" : "\u25BC";
        };

        if (anchorSection && anchorSection.parentNode === parent) {
            parent.insertBefore(title, anchorSection);
            parent.insertBefore(body, anchorSection);
        } else {
            parent.appendChild(title);
            parent.appendChild(body);
        }
    }

    function fillAdvancedEditors(rule) {
        ensureAdvancedSection();
        var conditionNode = document.getElementById("advancedConditionJson");
        var resultNode = document.getElementById("advancedResultJson");
        var mappingNode = document.getElementById("advancedValuesMappingJson");
        if (!conditionNode || !resultNode || !mappingNode) return;

        if (!rule || String(rule.source || "json") === "cpp") {
            conditionNode.value = "{}";
            resultNode.value = "{}";
            mappingNode.value = "{}";
            conditionNode.disabled = true;
            resultNode.disabled = true;
            mappingNode.disabled = true;
            return;
        }

        conditionNode.disabled = false;
        resultNode.disabled = false;
        mappingNode.disabled = false;
        conditionNode.value = pretty({
            conditionType: rule.conditionType || "FormType",
            matchType: rule.matchType || "OR",
            conditionIDs: Array.isArray(rule.conditionIDs) ? rule.conditionIDs : [],
            sortValueFrom: rule.sortValueFrom || ""
        });
        resultNode.value = pretty(rule.result || {});
        mappingNode.value = pretty(rule.valuesMapping || {});
    }

    function mergeAdvancedPayload(payload) {
        if (!payload || (payload.source || "json") === "cpp") return payload;
        if (payload.displayType === undefined && payload.titleText === undefined) return payload;
        if (!document.getElementById("advancedConditionJson")) return payload;

        try {
            var condition = parseEditor("advancedConditionJson", {});
            var result = parseEditor("advancedResultJson", {});
            var mapping = parseEditor("advancedValuesMappingJson", {});
            if (condition && typeof condition === "object") {
                if (condition.conditionType !== undefined) payload.conditionType = String(condition.conditionType || "");
                if (condition.matchType !== undefined) payload.matchType = String(condition.matchType || "");
                if (Array.isArray(condition.conditionIDs)) payload.conditionIDs = condition.conditionIDs.map(String);
                if (condition.sortValueFrom !== undefined) payload.sortValueFrom = String(condition.sortValueFrom || "");
            }
            if (result && typeof result === "object" && !Array.isArray(result)) payload.result = result;
            else throw new Error("Result Block must be a JSON object.");
            if (mapping && typeof mapping === "object" && !Array.isArray(mapping)) payload.valuesMapping = mapping;
            else throw new Error("Values Mapping must be a JSON object.");
            return payload;
        } catch (err) {
            toast("Advanced JSON save blocked: " + err.message, "warn");
            return null;
        }
    }

    function hookRuleUpdateFunction() {
        if (typeof window.uiRequestRuleUpdate !== "function" || window.uiRequestRuleUpdate.__iifAdvancedWrapped) return false;
        var original = window.uiRequestRuleUpdate;
        var wrapped = function (payloadText) {
            var payload = payloadText;
            try {
                if (typeof payloadText === "string") payload = JSON.parse(payloadText || "{}");
                payload = mergeAdvancedPayload(payload);
                if (!payload) return;
                return original.call(this, JSON.stringify(payload));
            } catch (err) {
                toast("Rule update blocked: " + err.message, "warn");
            }
        };
        wrapped.__iifAdvancedWrapped = true;
        window.uiRequestRuleUpdate = wrapped;
        return true;
    }

    var originalOnRuleDetailsForAdvanced = window.onRuleDetails || (typeof onRuleDetails === "function" ? onRuleDetails : null);
    if (typeof originalOnRuleDetailsForAdvanced === "function") {
        window.onRuleDetails = onRuleDetails = function (payload) {
            var result = originalOnRuleDetailsForAdvanced.apply(this, arguments);
            try {
                var data = typeof decodePayload === "function" ? decodePayload(payload, null) : (typeof payload === "string" ? JSON.parse(payload || "{}") : payload);
                fillAdvancedEditors(data && (data.rule || data.details || data));
            } catch (_) {}
            return result;
        };
    }

    ensureAdvancedSection();
    hookRuleUpdateFunction();
    var attempts = 0;
    var timer = setInterval(function () {
        attempts += 1;
        if (hookRuleUpdateFunction() || attempts > 40) clearInterval(timer);
    }, 250);
})();

// ImGui migration: edit current rule JSON file ownership.
(function initRuleFileOwnershipMigration() {
    if (window.__iifRuleFileOwnershipReady) return;
    window.__iifRuleFileOwnershipReady = true;

    function toast(message, kind) {
        var toastNode = document.getElementById("iifEditorToast");
        if (!toastNode) {
            toastNode = document.createElement("div");
            toastNode.id = "iifEditorToast";
            toastNode.className = "iif-editor-toast";
            document.body.appendChild(toastNode);
        }
        toastNode.textContent = message;
        toastNode.className = "iif-editor-toast visible" + (kind ? " " + kind : "");
        setTimeout(function () { toastNode.classList.remove("visible"); }, 2600);
        try { if (typeof logLine === "function") logLine(message); } catch (_) {}
    }

    function ensureRuleFileSection() {
        if (document.getElementById("ruleOriginPath")) return;
        var parent = null;
        var advanced = document.getElementById("ruleAdvancedJsonSection");
        if (advanced && advanced.parentNode) parent = advanced.parentNode;
        if (!parent) {
            var anchors = document.getElementById("ruleAnchorsSection") || document.getElementById("cppMetaSection") || document.getElementById("ruleAnchorPreviewSection");
            if (anchors && anchors.parentNode) parent = anchors.parentNode;
        }
        if (!parent) return;

        var title = document.createElement("div");
        title.className = "section-title section-title-collapsible";
        title.setAttribute("data-toggle-target", "ruleFileSection");
        var caret = document.createElement("span");
        caret.className = "detail-caret";
        caret.textContent = "\u25BC";
        var label = document.createElement("span");
        label.textContent = "RULE FILE";
        title.appendChild(caret);
        title.appendChild(label);

        var body = document.createElement("div");
        body.id = "ruleFileSection";
        body.className = "detail-collapsible-body";
        var grid = document.createElement("div");
        grid.className = "iif-rule-file-body";
        var fieldLabel = document.createElement("label");
        fieldLabel.textContent = "File";
        fieldLabel.setAttribute("for", "ruleOriginPath");
        var input = document.createElement("input");
        input.id = "ruleOriginPath";
        input.placeholder = "Data\\F4SE\\Plugins\\ItemIntegrationFramework\\gui_editor_rules.json";
        var note = document.createElement("div");
        note.className = "iif-rule-file-note";
        note.textContent = "Changing this value moves where SaveConfigs writes this JSON rule. Use a relative plugin path or the existing filename.";
        grid.appendChild(fieldLabel);
        grid.appendChild(input);
        grid.appendChild(note);
        body.appendChild(grid);

        title.onclick = function () {
            var collapsed = body.classList.toggle("collapsed");
            caret.textContent = collapsed ? "\u25B6" : "\u25BC";
        };

        var advancedTitle = document.querySelector('[data-toggle-target="ruleAdvancedJsonSection"]');
        if (advancedTitle && advancedTitle.parentNode === parent) {
            parent.insertBefore(title, advancedTitle);
            parent.insertBefore(body, advancedTitle);
        } else {
            parent.appendChild(title);
            parent.appendChild(body);
        }
    }

    function fillRuleFile(rule) {
        ensureRuleFileSection();
        var input = document.getElementById("ruleOriginPath");
        if (!input) return;
        if (!rule || String(rule.source || "json") !== "json") {
            input.value = "";
            input.disabled = true;
            return;
        }
        input.disabled = false;
        input.value = String(rule.originPath || "Data\\F4SE\\Plugins\\ItemIntegrationFramework\\gui_editor_rules.json");
    }

    function mergeRuleFilePayload(payload) {
        if (!payload || String(payload.source || "json") === "cpp") return payload;
        if (payload.displayType === undefined && payload.titleText === undefined && payload.priority === undefined) return payload;
        var input = document.getElementById("ruleOriginPath");
        if (!input || input.disabled) return payload;
        var value = String(input.value || "").trim();
        if (!value) {
            toast("Save blocked: rule file path is empty.", "warn");
            return null;
        }
        payload.originPath = value;
        return payload;
    }

    function hookRuleUpdateFunction() {
        if (typeof window.uiRequestRuleUpdate !== "function" || window.uiRequestRuleUpdate.__iifFileWrapped) return false;
        var original = window.uiRequestRuleUpdate;
        var wrapped = function (payloadText) {
            try {
                var payload = typeof payloadText === "string" ? JSON.parse(payloadText || "{}") : payloadText;
                payload = mergeRuleFilePayload(payload);
                if (!payload) return;
                return original.call(this, JSON.stringify(payload));
            } catch (err) {
                toast("Rule file save blocked: " + err.message, "warn");
            }
        };
        wrapped.__iifFileWrapped = true;
        window.uiRequestRuleUpdate = wrapped;
        return true;
    }

    var originalOnRuleDetailsForFile = window.onRuleDetails || (typeof onRuleDetails === "function" ? onRuleDetails : null);
    if (typeof originalOnRuleDetailsForFile === "function") {
        window.onRuleDetails = onRuleDetails = function (payload) {
            var result = originalOnRuleDetailsForFile.apply(this, arguments);
            try {
                var data = typeof decodePayload === "function" ? decodePayload(payload, null) : (typeof payload === "string" ? JSON.parse(payload || "{}") : payload);
                fillRuleFile(data && (data.rule || data.details || data));
            } catch (_) {}
            return result;
        };
    }

    ensureRuleFileSection();
    hookRuleUpdateFunction();
    var attempts = 0;
    var timer = setInterval(function () {
        attempts += 1;
        if (hookRuleUpdateFunction() || attempts > 40) clearInterval(timer);
    }, 250);
})();

// ImGui migration: form editor for result and valuesMapping result blocks.
(function initResultBlockFormMigration() {
    if (window.__iifResultBlockFormReady) return;
    window.__iifResultBlockFormReady = true;

    function toast(message, kind) {
        var toastNode = document.getElementById("iifEditorToast");
        if (!toastNode) {
            toastNode = document.createElement("div");
            toastNode.id = "iifEditorToast";
            toastNode.className = "iif-editor-toast";
            document.body.appendChild(toastNode);
        }
        toastNode.textContent = message;
        toastNode.className = "iif-editor-toast visible" + (kind ? " " + kind : "");
        setTimeout(function () { toastNode.classList.remove("visible"); }, 2600);
        try { if (typeof logLine === "function") logLine(message); } catch (_) {}
    }

    function parseJson(id, fallback) {
        var node = document.getElementById(id);
        if (!node) return fallback;
        var raw = String(node.value || "").trim();
        if (!raw) return fallback;
        return JSON.parse(raw);
    }

    function setJson(id, value) {
        var node = document.getElementById(id);
        if (!node) return;
        node.value = JSON.stringify(value || {}, null, 2);
        node.dispatchEvent(new Event("input", { bubbles: true }));
        node.dispatchEvent(new Event("change", { bubbles: true }));
    }

    function blockDefault() {
        return {
            value: "",
            tag: "",
            state: "normal",
            align: "",
            hideDifference: -1,
            invertDiffColor: -1,
            isIcon: false,
            dataSource: {
                active: false,
                type: "",
                id: "",
                required: 0,
                offset: 0,
                multiplier: 1,
                suffix: ""
            },
            fillPct: -1,
            shieldPct: 0,
            fillColor: 0,
            showBar: -1,
            showValue: -1,
            valueText: "",
            valueAlign: "",
            valueColor: 0,
            valueStandard: -1,
            hasContent: true
        };
    }

    function mergeDefaults(block) {
        var out = blockDefault();
        block = block || {};
        Object.keys(block).forEach(function (key) { out[key] = block[key]; });
        out.dataSource = Object.assign({}, blockDefault().dataSource, block.dataSource || block.DataSource || {});
        return out;
    }

    function val(id) {
        var node = document.getElementById(id);
        if (!node) return "";
        if (node.type === "checkbox") return !!node.checked;
        return node.value;
    }

    function setVal(id, value) {
        var node = document.getElementById(id);
        if (!node) return;
        if (node.type === "checkbox") node.checked = !!value;
        else node.value = value === undefined || value === null ? "" : String(value);
    }

    function numberVal(id, fallback) {
        var n = Number(val(id));
        return isFinite(n) ? n : fallback;
    }

    function triVal(id) {
        var raw = String(val(id));
        if (raw === "true") return 1;
        if (raw === "false") return 0;
        return -1;
    }

    function triText(value) {
        value = Number(value);
        if (value > 0) return "true";
        if (value === 0) return "false";
        return "default";
    }

    function currentTarget() {
        return val("blockFormTarget") || "result";
    }

    function currentMappingKey() {
        return val("blockFormMappingKey") || "";
    }

    function refreshMappingKeys(selected) {
        var select = document.getElementById("blockFormMappingKey");
        if (!select) return;
        var mapping = {};
        try { mapping = parseJson("advancedValuesMappingJson", {}); } catch (_) {}
        var keys = Object.keys(mapping).sort();
        select.innerHTML = "";
        if (!keys.length) {
            var empty = document.createElement("option");
            empty.value = "";
            empty.textContent = "(no mapping)";
            select.appendChild(empty);
            return;
        }
        for (var i = 0; i < keys.length; ++i) {
            var opt = document.createElement("option");
            opt.value = keys[i];
            opt.textContent = keys[i];
            select.appendChild(opt);
        }
        if (selected && keys.indexOf(selected) >= 0) select.value = selected;
    }

    function getCurrentBlock() {
        if (currentTarget() === "mapping") {
            var mapping = parseJson("advancedValuesMappingJson", {});
            return mergeDefaults(mapping[currentMappingKey()] || {});
        }
        return mergeDefaults(parseJson("advancedResultJson", {}));
    }

    function fillFormFromBlock(block) {
        block = mergeDefaults(block);
        setVal("blockValue", block.value);
        setVal("blockTag", block.tag);
        setVal("blockState", block.state || "normal");
        setVal("blockAlign", block.align || "");
        setVal("blockIsIcon", !!block.isIcon);
        setVal("blockHideDifference", triText(block.hideDifference));
        setVal("blockInvertDiffColor", triText(block.invertDiffColor));
        setVal("blockValueText", block.valueText || "");
        setVal("blockValueAlign", block.valueAlign || "");
        setVal("blockValueColor", block.valueColor === undefined ? 0 : block.valueColor);
        setVal("blockValueStandard", triText(block.valueStandard));
        setVal("blockFillPct", block.fillPct === undefined ? -1 : block.fillPct);
        setVal("blockShieldPct", block.shieldPct === undefined ? 0 : block.shieldPct);
        setVal("blockFillColor", block.fillColor === undefined ? 0 : block.fillColor);
        setVal("blockShowBar", triText(block.showBar));
        setVal("blockShowValue", triText(block.showValue));
        var ds = block.dataSource || {};
        setVal("blockDsActive", !!ds.active);
        setVal("blockDsType", ds.type || ds.Type || "");
        setVal("blockDsId", ds.id || ds.ID || "");
        setVal("blockDsRequired", ds.required === undefined ? (ds.Required || 0) : ds.required);
        setVal("blockDsOffset", ds.offset === undefined ? (ds.Offset || 0) : ds.offset);
        setVal("blockDsMultiplier", ds.multiplier === undefined ? (ds.Multiplier || 1) : ds.multiplier);
        setVal("blockDsSuffix", ds.suffix || ds.Suffix || "");
    }

    function readBlockFromForm(base) {
        var block = mergeDefaults(base || {});
        block.value = String(val("blockValue") || "");
        block.tag = String(val("blockTag") || "");
        block.state = String(val("blockState") || "normal");
        block.align = String(val("blockAlign") || "");
        block.isIcon = !!val("blockIsIcon");
        block.hideDifference = triVal("blockHideDifference");
        block.invertDiffColor = triVal("blockInvertDiffColor");
        block.valueText = String(val("blockValueText") || "");
        block.valueAlign = String(val("blockValueAlign") || "");
        block.valueColor = numberVal("blockValueColor", 0);
        block.valueStandard = triVal("blockValueStandard");
        block.fillPct = numberVal("blockFillPct", -1);
        block.shieldPct = numberVal("blockShieldPct", 0);
        block.fillColor = numberVal("blockFillColor", 0);
        block.showBar = triVal("blockShowBar");
        block.showValue = triVal("blockShowValue");
        block.dataSource = {
            active: !!val("blockDsActive"),
            type: String(val("blockDsType") || ""),
            id: String(val("blockDsId") || ""),
            required: numberVal("blockDsRequired", 0),
            offset: numberVal("blockDsOffset", 0),
            multiplier: numberVal("blockDsMultiplier", 1),
            suffix: String(val("blockDsSuffix") || "")
        };
        block.hasContent = true;
        return block;
    }

    function applyFormToJson() {
        try {
            if (currentTarget() === "mapping") {
                var key = currentMappingKey();
                if (!key) { toast("Apply failed: no valuesMapping key selected.", "warn"); return; }
                var mapping = parseJson("advancedValuesMappingJson", {});
                mapping[key] = readBlockFromForm(mapping[key] || {});
                setJson("advancedValuesMappingJson", mapping);
                toast("valuesMapping updated: " + key, "ok");
            } else {
                var result = readBlockFromForm(parseJson("advancedResultJson", {}));
                setJson("advancedResultJson", result);
                toast("result block updated.", "ok");
            }
            if (typeof refreshLivePreview === "function") refreshLivePreview();
        } catch (err) {
            toast("Apply failed: " + err.message, "warn");
        }
    }

    function addMappingKey() {
        try {
            var input = document.getElementById("blockFormNewMappingKey");
            var key = input ? String(input.value || "").trim() : "";
            if (!key) { toast("Add mapping failed: key is empty.", "warn"); return; }
            var mapping = parseJson("advancedValuesMappingJson", {});
            if (mapping[key]) { toast("Add mapping failed: key already exists.", "warn"); return; }
            mapping[key] = readBlockFromForm({});
            setJson("advancedValuesMappingJson", mapping);
            refreshMappingKeys(key);
            setVal("blockFormTarget", "mapping");
            fillFormFromBlock(mapping[key]);
            toast("valuesMapping added: " + key, "ok");
        } catch (err) {
            toast("Add mapping failed: " + err.message, "warn");
        }
    }

    function deleteMappingKey() {
        try {
            var key = currentMappingKey();
            if (!key) { toast("Delete mapping failed: no key selected.", "warn"); return; }
            var mapping = parseJson("advancedValuesMappingJson", {});
            delete mapping[key];
            setJson("advancedValuesMappingJson", mapping);
            refreshMappingKeys("");
            fillFormFromBlock(getCurrentBlock());
            toast("valuesMapping deleted: " + key, "ok");
        } catch (err) {
            toast("Delete mapping failed: " + err.message, "warn");
        }
    }

    function reloadFormFromJson() {
        try {
            refreshMappingKeys(currentMappingKey());
            fillFormFromBlock(getCurrentBlock());
            toast("block form loaded from JSON.", "ok");
        } catch (err) {
            toast("Load failed: " + err.message, "warn");
        }
    }

    function addField(parent, labelText, id, type, extra) {
        var label = document.createElement("label");
        label.textContent = labelText;
        label.setAttribute("for", id);
        var input;
        if (type === "select") {
            input = document.createElement("select");
            var options = (extra && extra.options) || [];
            for (var i = 0; i < options.length; ++i) {
                var opt = document.createElement("option");
                opt.value = options[i][0];
                opt.textContent = options[i][1];
                input.appendChild(opt);
            }
        } else {
            input = document.createElement("input");
            input.type = type || "text";
            if (extra && extra.step) input.step = extra.step;
        }
        input.id = id;
        if (extra && extra.wide) input.className = "wide";
        parent.appendChild(label);
        parent.appendChild(input);
        return input;
    }

    function ensureBlockFormSection() {
        if (document.getElementById("resultBlockFormSection")) return;
        var advancedBody = document.getElementById("ruleAdvancedJsonSection");
        var advancedTitle = document.querySelector('[data-toggle-target="ruleAdvancedJsonSection"]');
        var parent = advancedBody && advancedBody.parentNode ? advancedBody.parentNode : null;
        if (!parent) return;

        var title = document.createElement("div");
        title.className = "section-title section-title-collapsible";
        title.setAttribute("data-toggle-target", "resultBlockFormSection");
        var caret = document.createElement("span");
        caret.className = "detail-caret";
        caret.textContent = "\u25B6";
        var titleText = document.createElement("span");
        titleText.textContent = "RESULT BLOCK FORM";
        title.appendChild(caret);
        title.appendChild(titleText);

        var body = document.createElement("div");
        body.id = "resultBlockFormSection";
        body.className = "detail-collapsible-body collapsed";
        var toolbar = document.createElement("div");
        toolbar.className = "iif-block-toolbar";
        function button(text, fn) { var b = document.createElement("button"); b.textContent = text; b.onclick = fn; toolbar.appendChild(b); }
        button("Load From JSON", reloadFormFromJson);
        button("Apply To JSON", applyFormToJson);
        button("Add Mapping", addMappingKey);
        button("Delete Mapping", deleteMappingKey);
        body.appendChild(toolbar);

        var form = document.createElement("div");
        form.className = "iif-block-form";
        addField(form, "Target", "blockFormTarget", "select", { options: [["result", "Result Block"], ["mapping", "Values Mapping"]] });
        addField(form, "Mapping Key", "blockFormMappingKey", "select", { options: [["", "(no mapping)"]] });
        addField(form, "New Key", "blockFormNewMappingKey", "text");
        var subtitle1 = document.createElement("div"); subtitle1.className = "iif-block-subtitle"; subtitle1.textContent = "Display Fields"; form.appendChild(subtitle1);
        addField(form, "Value", "blockValue", "text");
        addField(form, "Tag", "blockTag", "text");
        addField(form, "State", "blockState", "select", { options: [["normal", "normal"], ["good", "good"], ["bad", "bad"], ["star", "star"]] });
        addField(form, "Align", "blockAlign", "select", { options: [["", "default"], ["left", "left"], ["right", "right"], ["center", "center"]] });
        addField(form, "Is Icon", "blockIsIcon", "checkbox");
        addField(form, "Value Text", "blockValueText", "text");
        addField(form, "Value Align", "blockValueAlign", "select", { options: [["", "default"], ["left", "left"], ["right", "right"], ["center", "center"]] });
        addField(form, "Value Color", "blockValueColor", "number");
        addField(form, "Value Std", "blockValueStandard", "select", { options: [["default", "default"], ["true", "true"], ["false", "false"]] });
        addField(form, "Hide Diff", "blockHideDifference", "select", { options: [["default", "default"], ["true", "true"], ["false", "false"]] });
        addField(form, "Invert Diff", "blockInvertDiffColor", "select", { options: [["default", "default"], ["true", "true"], ["false", "false"]] });
        var subtitle2 = document.createElement("div"); subtitle2.className = "iif-block-subtitle"; subtitle2.textContent = "Bar Fields"; form.appendChild(subtitle2);
        addField(form, "Fill Pct", "blockFillPct", "number", { step: "0.01" });
        addField(form, "Shield Pct", "blockShieldPct", "number", { step: "0.01" });
        addField(form, "Fill Color", "blockFillColor", "number");
        addField(form, "Show Bar", "blockShowBar", "select", { options: [["default", "default"], ["true", "true"], ["false", "false"]] });
        addField(form, "Show Value", "blockShowValue", "select", { options: [["default", "default"], ["true", "true"], ["false", "false"]] });
        var subtitle3 = document.createElement("div"); subtitle3.className = "iif-block-subtitle"; subtitle3.textContent = "Data Source"; form.appendChild(subtitle3);
        addField(form, "DS Active", "blockDsActive", "checkbox");
        addField(form, "DS Type", "blockDsType", "text");
        addField(form, "DS ID", "blockDsId", "text");
        addField(form, "Required", "blockDsRequired", "number");
        addField(form, "Offset", "blockDsOffset", "number", { step: "0.01" });
        addField(form, "Multiplier", "blockDsMultiplier", "number", { step: "0.01" });
        addField(form, "Suffix", "blockDsSuffix", "text");
        body.appendChild(form);

        title.onclick = function () {
            var collapsed = body.classList.toggle("collapsed");
            caret.textContent = collapsed ? "\u25B6" : "\u25BC";
        };
        if (advancedTitle && advancedTitle.parentNode === parent) {
            parent.insertBefore(title, advancedTitle);
            parent.insertBefore(body, advancedTitle);
        } else {
            parent.appendChild(title);
            parent.appendChild(body);
        }
        document.getElementById("blockFormTarget").onchange = reloadFormFromJson;
        document.getElementById("blockFormMappingKey").onchange = reloadFormFromJson;
        reloadFormFromJson();
    }

    var originalOnRuleDetailsForBlockForm = window.onRuleDetails || (typeof onRuleDetails === "function" ? onRuleDetails : null);
    if (typeof originalOnRuleDetailsForBlockForm === "function") {
        window.onRuleDetails = onRuleDetails = function () {
            var result = originalOnRuleDetailsForBlockForm.apply(this, arguments);
            setTimeout(function () { ensureBlockFormSection(); reloadFormFromJson(); }, 0);
            return result;
        };
    }

    var attempts = 0;
    var timer = setInterval(function () {
        attempts += 1;
        ensureBlockFormSection();
        if (document.getElementById("resultBlockFormSection") || attempts > 40) clearInterval(timer);
    }, 250);
})();

// ImGui migration: condition form, inner left/right box form, TOP/BOTTOM details, overridden C++ filter.
(function initRemainingImGuiEditorMigration() {
    if (window.__iifRemainingImGuiEditorReady) return;
    window.__iifRemainingImGuiEditorReady = true;

    function toast(message, kind) {
        var toastNode = document.getElementById("iifEditorToast");
        if (!toastNode) {
            toastNode = document.createElement("div");
            toastNode.id = "iifEditorToast";
            toastNode.className = "iif-editor-toast";
            document.body.appendChild(toastNode);
        }
        toastNode.textContent = message;
        toastNode.className = "iif-editor-toast visible" + (kind ? " " + kind : "");
        setTimeout(function () { toastNode.classList.remove("visible"); }, 2600);
        try { if (typeof logLine === "function") logLine(message); } catch (_) {}
    }

    function parseJson(id, fallback) {
        var node = document.getElementById(id);
        if (!node) return fallback;
        var raw = String(node.value || "").trim();
        if (!raw) return fallback;
        return JSON.parse(raw);
    }

    function setJson(id, value) {
        var node = document.getElementById(id);
        if (!node) return;
        node.value = JSON.stringify(value || {}, null, 2);
        node.dispatchEvent(new Event("input", { bubbles: true }));
        node.dispatchEvent(new Event("change", { bubbles: true }));
    }

    function val(id) {
        var node = document.getElementById(id);
        if (!node) return "";
        if (node.type === "checkbox") return !!node.checked;
        return node.value;
    }

    function setVal(id, value) {
        var node = document.getElementById(id);
        if (!node) return;
        if (node.type === "checkbox") node.checked = !!value;
        else node.value = value === undefined || value === null ? "" : String(value);
    }

    function numberVal(id, fallback) {
        var n = Number(val(id));
        return isFinite(n) ? n : fallback;
    }

    function appendSelect(parent, id, options) {
        var select = document.createElement("select");
        select.id = id;
        for (var i = 0; i < options.length; ++i) {
            var opt = document.createElement("option");
            opt.value = options[i][0];
            opt.textContent = options[i][1];
            select.appendChild(opt);
        }
        parent.appendChild(select);
        return select;
    }

    function appendInput(parent, id, type) {
        var input = document.createElement("input");
        input.id = id;
        input.type = type || "text";
        parent.appendChild(input);
        return input;
    }

    function addLabel(parent, text, id) {
        var label = document.createElement("label");
        label.textContent = text;
        if (id) label.setAttribute("for", id);
        parent.appendChild(label);
        return label;
    }

    function insertBeforeAdvanced(title, body) {
        var advancedTitle = document.querySelector('[data-toggle-target="ruleAdvancedJsonSection"]');
        var parent = advancedTitle && advancedTitle.parentNode ? advancedTitle.parentNode : null;
        if (!parent) {
            var saveBtn = document.getElementById("saveRuleBtn");
            parent = saveBtn && saveBtn.parentNode ? saveBtn.parentNode.parentNode : null;
        }
        if (!parent) return false;
        if (advancedTitle && advancedTitle.parentNode === parent) {
            parent.insertBefore(title, advancedTitle);
            parent.insertBefore(body, advancedTitle);
        } else {
            parent.appendChild(title);
            parent.appendChild(body);
        }
        return true;
    }

    function makeSection(titleText, bodyId) {
        var title = document.createElement("div");
        title.className = "section-title section-title-collapsible";
        title.setAttribute("data-toggle-target", bodyId);
        var caret = document.createElement("span");
        caret.className = "detail-caret";
        caret.textContent = "\u25B6";
        var label = document.createElement("span");
        label.textContent = titleText;
        title.appendChild(caret);
        title.appendChild(label);
        var body = document.createElement("div");
        body.id = bodyId;
        body.className = "detail-collapsible-body collapsed";
        title.onclick = function () {
            var collapsed = body.classList.toggle("collapsed");
            caret.textContent = collapsed ? "\u25B6" : "\u25BC";
        };
        return { title: title, body: body };
    }

    function ensureConditionForm() {
        if (document.getElementById("conditionFormSection")) return;
        var sec = makeSection("CONDITION FORM", "conditionFormSection");
        var toolbar = document.createElement("div");
        toolbar.className = "iif-form-toolbar";
        var load = document.createElement("button"); load.textContent = "Load From JSON"; load.onclick = loadConditionForm;
        var apply = document.createElement("button"); apply.textContent = "Apply To JSON"; apply.onclick = applyConditionForm;
        toolbar.appendChild(load); toolbar.appendChild(apply);
        sec.body.appendChild(toolbar);
        var form = document.createElement("div");
        form.className = "iif-condition-form";
        addLabel(form, "Condition Type", "conditionTypeField");
        appendSelect(form, "conditionTypeField", [["FormType", "FormType"], ["Keyword", "Keyword"], ["FormID", "FormID"], ["EditorID", "EditorID"], ["Name", "Name"], ["Custom", "Custom"]]);
        addLabel(form, "Match Type", "matchTypeField");
        appendSelect(form, "matchTypeField", [["OR", "OR"], ["AND", "AND"], ["NOT", "NOT"]]);
        addLabel(form, "Sort Value From", "sortValueFromField");
        appendInput(form, "sortValueFromField", "text");
        var spacer = document.createElement("div"); form.appendChild(spacer);
        var spacer2 = document.createElement("div"); form.appendChild(spacer2);
        addLabel(form, "Condition IDs", "conditionIDsField");
        var ids = document.createElement("textarea");
        ids.id = "conditionIDsField";
        ids.placeholder = "One ID per line, e.g. Weapon";
        form.appendChild(ids);
        sec.body.appendChild(form);
        insertBeforeAdvanced(sec.title, sec.body);
        setTimeout(loadConditionForm, 0);
    }

    function loadConditionForm() {
        try {
            var condition = parseJson("advancedConditionJson", {});
            setVal("conditionTypeField", condition.conditionType || "FormType");
            setVal("matchTypeField", condition.matchType || "OR");
            setVal("sortValueFromField", condition.sortValueFrom || "");
            setVal("conditionIDsField", Array.isArray(condition.conditionIDs) ? condition.conditionIDs.join("\n") : "");
        } catch (err) {
            toast("Condition load failed: " + err.message, "warn");
        }
    }

    function applyConditionForm() {
        try {
            var ids = String(val("conditionIDsField") || "").split(/\r?\n|,/).map(function (x) { return x.trim(); }).filter(Boolean);
            setJson("advancedConditionJson", {
                conditionType: String(val("conditionTypeField") || "FormType"),
                matchType: String(val("matchTypeField") || "OR"),
                conditionIDs: ids,
                sortValueFrom: String(val("sortValueFromField") || "")
            });
            toast("Condition JSON updated.", "ok");
        } catch (err) {
            toast("Condition apply failed: " + err.message, "warn");
        }
    }

    function blockDefault() {
        return { leftBox: {}, rightBox: {} };
    }

    function currentBlockTarget() {
        var targetNode = document.getElementById("boxFormTarget");
        return targetNode ? targetNode.value : "result";
    }

    function currentBoxSide() {
        var sideNode = document.getElementById("boxFormSide");
        return sideNode ? sideNode.value : "leftBox";
    }

    function currentMappingKeyForBox() {
        var keyNode = document.getElementById("boxFormMappingKey");
        return keyNode ? keyNode.value : "";
    }

    function refreshBoxMappingKeys(selected) {
        var select = document.getElementById("boxFormMappingKey");
        if (!select) return;
        var mapping = {};
        try { mapping = parseJson("advancedValuesMappingJson", {}); } catch (_) {}
        var keys = Object.keys(mapping).sort();
        select.innerHTML = "";
        if (!keys.length) {
            var empty = document.createElement("option");
            empty.value = "";
            empty.textContent = "(no mapping)";
            select.appendChild(empty);
            return;
        }
        for (var i = 0; i < keys.length; ++i) {
            var opt = document.createElement("option");
            opt.value = keys[i];
            opt.textContent = keys[i];
            select.appendChild(opt);
        }
        if (selected && keys.indexOf(selected) >= 0) select.value = selected;
    }

    function getCurrentBlockForBox() {
        if (currentBlockTarget() === "mapping") {
            var mapping = parseJson("advancedValuesMappingJson", {});
            return mapping[currentMappingKeyForBox()] || blockDefault();
        }
        return parseJson("advancedResultJson", blockDefault());
    }

    function getBoxFromBlock(block) {
        var side = currentBoxSide();
        return Object.assign({ active: false, tag: "", value: "", isIcon: false, state: "normal", align: "", dataSource: {} }, block && block[side] ? block[side] : {});
    }

    function ensureInnerBoxForm() {
        if (document.getElementById("innerBoxFormSection")) return;
        var sec = makeSection("INNER BOX FORM", "innerBoxFormSection");
        var toolbar = document.createElement("div");
        toolbar.className = "iif-form-toolbar";
        var load = document.createElement("button"); load.textContent = "Load From JSON"; load.onclick = loadInnerBoxForm;
        var apply = document.createElement("button"); apply.textContent = "Apply To JSON"; apply.onclick = applyInnerBoxForm;
        toolbar.appendChild(load); toolbar.appendChild(apply);
        sec.body.appendChild(toolbar);
        var form = document.createElement("div");
        form.className = "iif-inner-box-form";
        addLabel(form, "Target", "boxFormTarget"); appendSelect(form, "boxFormTarget", [["result", "Result Block"], ["mapping", "Values Mapping"]]);
        addLabel(form, "Mapping Key", "boxFormMappingKey"); appendSelect(form, "boxFormMappingKey", [["", "(no mapping)"]]);
        addLabel(form, "Box Side", "boxFormSide"); appendSelect(form, "boxFormSide", [["leftBox", "Left Box"], ["rightBox", "Right Box"]]);
        var sub = document.createElement("div"); sub.className = "iif-box-form-subtitle"; sub.textContent = "Box Fields"; form.appendChild(sub);
        addLabel(form, "Active", "innerBoxActive"); appendInput(form, "innerBoxActive", "checkbox");
        addLabel(form, "Is Icon", "innerBoxIsIcon"); appendInput(form, "innerBoxIsIcon", "checkbox");
        addLabel(form, "Tag", "innerBoxTag"); appendInput(form, "innerBoxTag", "text");
        addLabel(form, "Value", "innerBoxValue"); appendInput(form, "innerBoxValue", "text");
        addLabel(form, "State", "innerBoxState"); appendSelect(form, "innerBoxState", [["normal", "normal"], ["good", "good"], ["bad", "bad"], ["star", "star"]]);
        addLabel(form, "Align", "innerBoxAlign"); appendSelect(form, "innerBoxAlign", [["", "default"], ["left", "left"], ["right", "right"], ["center", "center"]]);
        var sub2 = document.createElement("div"); sub2.className = "iif-box-form-subtitle"; sub2.textContent = "Box Data Source"; form.appendChild(sub2);
        addLabel(form, "DS Active", "innerBoxDsActive"); appendInput(form, "innerBoxDsActive", "checkbox");
        addLabel(form, "DS Type", "innerBoxDsType"); appendInput(form, "innerBoxDsType", "text");
        addLabel(form, "DS ID", "innerBoxDsId"); appendInput(form, "innerBoxDsId", "text");
        addLabel(form, "Required", "innerBoxDsRequired"); appendInput(form, "innerBoxDsRequired", "number");
        addLabel(form, "Offset", "innerBoxDsOffset"); appendInput(form, "innerBoxDsOffset", "number");
        addLabel(form, "Multiplier", "innerBoxDsMultiplier"); appendInput(form, "innerBoxDsMultiplier", "number");
        addLabel(form, "Suffix", "innerBoxDsSuffix"); appendInput(form, "innerBoxDsSuffix", "text");
        sec.body.appendChild(form);
        insertBeforeAdvanced(sec.title, sec.body);
        document.getElementById("boxFormTarget").onchange = loadInnerBoxForm;
        document.getElementById("boxFormMappingKey").onchange = loadInnerBoxForm;
        document.getElementById("boxFormSide").onchange = loadInnerBoxForm;
        setTimeout(loadInnerBoxForm, 0);
    }

    function loadInnerBoxForm() {
        try {
            refreshBoxMappingKeys(currentMappingKeyForBox());
            var box = getBoxFromBlock(getCurrentBlockForBox());
            var ds = box.dataSource || box.DataSource || {};
            setVal("innerBoxActive", !!box.active);
            setVal("innerBoxIsIcon", !!box.isIcon);
            setVal("innerBoxTag", box.tag || "");
            setVal("innerBoxValue", box.value || "");
            setVal("innerBoxState", box.state || "normal");
            setVal("innerBoxAlign", box.align || "");
            setVal("innerBoxDsActive", !!ds.active);
            setVal("innerBoxDsType", ds.type || ds.Type || "");
            setVal("innerBoxDsId", ds.id || ds.ID || "");
            setVal("innerBoxDsRequired", ds.required === undefined ? (ds.Required || 0) : ds.required);
            setVal("innerBoxDsOffset", ds.offset === undefined ? (ds.Offset || 0) : ds.offset);
            setVal("innerBoxDsMultiplier", ds.multiplier === undefined ? (ds.Multiplier || 1) : ds.multiplier);
            setVal("innerBoxDsSuffix", ds.suffix || ds.Suffix || "");
        } catch (err) {
            toast("Inner box load failed: " + err.message, "warn");
        }
    }

    function readInnerBoxForm(existing) {
        return Object.assign({}, existing || {}, {
            active: !!val("innerBoxActive"),
            isIcon: !!val("innerBoxIsIcon"),
            tag: String(val("innerBoxTag") || ""),
            value: String(val("innerBoxValue") || ""),
            state: String(val("innerBoxState") || "normal"),
            align: String(val("innerBoxAlign") || ""),
            dataSource: {
                active: !!val("innerBoxDsActive"),
                type: String(val("innerBoxDsType") || ""),
                id: String(val("innerBoxDsId") || ""),
                required: numberVal("innerBoxDsRequired", 0),
                offset: numberVal("innerBoxDsOffset", 0),
                multiplier: numberVal("innerBoxDsMultiplier", 1),
                suffix: String(val("innerBoxDsSuffix") || "")
            }
        });
    }

    function applyInnerBoxForm() {
        try {
            var side = currentBoxSide();
            if (currentBlockTarget() === "mapping") {
                var key = currentMappingKeyForBox();
                if (!key) { toast("Apply failed: no valuesMapping key selected.", "warn"); return; }
                var mapping = parseJson("advancedValuesMappingJson", {});
                var block = mapping[key] || {};
                block[side] = readInnerBoxForm(block[side] || {});
                mapping[key] = block;
                setJson("advancedValuesMappingJson", mapping);
                toast("Inner box updated: " + key + " / " + side, "ok");
            } else {
                var result = parseJson("advancedResultJson", {});
                result[side] = readInnerBoxForm(result[side] || {});
                setJson("advancedResultJson", result);
                toast("Inner box updated: result / " + side, "ok");
            }
        } catch (err) {
            toast("Inner box apply failed: " + err.message, "warn");
        }
    }

    function installRootAnchorDetails() {
        var tree = document.getElementById("ruleTree");
        if (!tree || tree.dataset.rootAnchorDetails === "1") return;
        tree.dataset.rootAnchorDetails = "1";
        tree.addEventListener("click", function (event) {
            var node = event.target;
            while (node && node !== tree) {
                if (node.classList && node.classList.contains("rule-tree-root") && node.dataset && (node.dataset.rootId === "TOP" || node.dataset.rootId === "BOTTOM")) {
                    var rootId = node.dataset.rootId;
                    selectedRuleId = rootId;
                    selectedRuleSource = "vanilla";
                    var anchorPreview = document.getElementById("anchorPreview");
                    if (anchorPreview) {
                        anchorPreview.textContent = rootId === "TOP"
                            ? "Root anchor selected: TOP\n\nForced top insertion zone. Drag rules here when they must appear before normal vanilla/FallUI cards."
                            : "Root anchor selected: BOTTOM\n\nFallback bottom insertion zone. Rules without a custom anchor generally end up here.";
                    }
                    var livePreview = document.getElementById("livePreview");
                    if (livePreview) {
                        livePreview.innerHTML = '<div class="preview-card preview-vanilla"><div class="preview-row"><div class="preview-title">[ROOT] ' + rootId + '</div><div class="preview-value">anchor</div></div><div class="preview-subtle">drop target only</div></div>';
                    }
                    toast("Selected root anchor: " + rootId, "ok");
                    return;
                }
                node = node.parentNode;
            }
        }, false);
    }

    function installOverriddenCppFilter() {
        var select = document.getElementById("treeSourceFilter");
        if (!select || select.dataset.overriddenCppAdded === "1") return;
        select.dataset.overriddenCppAdded = "1";
        var opt = document.createElement("option");
        opt.value = "cpp-overridden";
        opt.textContent = "Overridden C++";
        select.appendChild(opt);

        if (typeof getRuleSummaryFiltersForTree === "function" && !getRuleSummaryFiltersForTree.__iifOverriddenWrapped) {
            var original = getRuleSummaryFiltersForTree;
            var wrapped = function () {
                var sourceFilter = String((document.getElementById("treeSourceFilter") || {}).value || "all");
                if (sourceFilter !== "cpp-overridden") return original.apply(this, arguments);
                var sceneFilter = String((document.getElementById("treeSceneFilter") || {}).value || "all");
                var q = String((document.getElementById("treeSearch") || {}).value || "").trim().toLowerCase();
                return rules.filter(function (item) {
                    if (!item) return false;
                    var title = String(item.title || "").toLowerCase();
                    var id = String(item.id || "").toLowerCase();
                    var okSearch = !q || id.indexOf(q) >= 0 || title.indexOf(q) >= 0;
                    var okScene = sceneFilter === "all" || (item.scene || "custom") === sceneFilter;
                    return okSearch && okScene && String(item.source || "json") === "cpp" && !!item.isOverridden;
                });
            };
            wrapped.__iifOverriddenWrapped = true;
            window.getRuleSummaryFiltersForTree = getRuleSummaryFiltersForTree = wrapped;
        }
    }

    function init() {
        ensureConditionForm();
        ensureInnerBoxForm();
        installRootAnchorDetails();
        installOverriddenCppFilter();
    }

    if (document.readyState === "loading") document.addEventListener("DOMContentLoaded", init);
    else init();
    var attempts = 0;
    var timer = setInterval(function () {
        attempts += 1;
        init();
        if ((document.getElementById("conditionFormSection") && document.getElementById("innerBoxFormSection")) || attempts > 40) clearInterval(timer);
    }, 250);
})();

// ImGui migration: drag diff preview and file move warnings.
(function initEditorDiffAndFileMoveHints() {
    if (window.__iifEditorDiffAndFileMoveHintsReady) return;
    window.__iifEditorDiffAndFileMoveHintsReady = true;

    var lastOriginPathByRule = {};

    function toast(message, kind) {
        var toastNode = document.getElementById("iifEditorToast");
        if (!toastNode) {
            toastNode = document.createElement("div");
            toastNode.id = "iifEditorToast";
            toastNode.className = "iif-editor-toast";
            document.body.appendChild(toastNode);
        }
        toastNode.textContent = message;
        toastNode.className = "iif-editor-toast visible" + (kind ? " " + kind : "");
        setTimeout(function () { toastNode.classList.remove("visible"); }, 4200);
        try { if (typeof logLine === "function") logLine(message); } catch (_) {}
    }

    function snapshotRules() {
        var snap = {};
        if (!Array.isArray(rules)) return snap;
        for (var i = 0; i < rules.length; ++i) {
            var rule = rules[i];
            if (!rule || !rule.id) continue;
            var key = String(rule.source || "json") + "::" + String(rule.id);
            var anchors = [];
            try { anchors = typeof readAnchorsFromRule === "function" ? readAnchorsFromRule(rule) : (rule.anchors || []); } catch (_) {}
            snap[key] = {
                id: String(rule.id),
                source: String(rule.source || "json"),
                priority: Number(rule.priority || 0),
                anchors: JSON.stringify(anchors || [])
            };
        }
        return snap;
    }

    function describeDiff(before, after) {
        var lines = [];
        Object.keys(after).sort().forEach(function (key) {
            var a = after[key];
            var b = before[key];
            if (!a || !b) return;
            var changes = [];
            if (a.priority !== b.priority) changes.push("p " + b.priority + " -> " + a.priority);
            if (a.anchors !== b.anchors) changes.push("anchor changed");
            if (changes.length) lines.push(a.id + ": " + changes.join(", "));
        });
        return lines;
    }

    if (typeof applyRuleTreeDrop === "function" && !applyRuleTreeDrop.__iifDiffWrapped) {
        var originalApplyRuleTreeDrop = applyRuleTreeDrop;
        applyRuleTreeDrop = window.applyRuleTreeDrop = function () {
            var before = snapshotRules();
            var result = originalApplyRuleTreeDrop.apply(this, arguments);
            var after = snapshotRules();
            var lines = describeDiff(before, after);
            if (lines.length) {
                toast("Drag diff:\n" + lines.slice(0, 6).join("\n") + (lines.length > 6 ? "\n..." : ""), "ok");
            }
            return result;
        };
        applyRuleTreeDrop.__iifDiffWrapped = true;
    }

    var originalOnRuleDetailsForFileMove = window.onRuleDetails || (typeof onRuleDetails === "function" ? onRuleDetails : null);
    if (typeof originalOnRuleDetailsForFileMove === "function") {
        window.onRuleDetails = onRuleDetails = function (payload) {
            var result = originalOnRuleDetailsForFileMove.apply(this, arguments);
            try {
                var data = typeof decodePayload === "function" ? decodePayload(payload, null) : (typeof payload === "string" ? JSON.parse(payload || "{}") : payload);
                var rule = data && (data.rule || data.details || data);
                if (rule && rule.id && String(rule.source || "json") === "json") {
                    lastOriginPathByRule[String(rule.id)] = String(rule.originPath || "");
                }
            } catch (_) {}
            return result;
        };
    }

    function hookRuleUpdateForFileMoveHint() {
        if (typeof window.uiRequestRuleUpdate !== "function" || window.uiRequestRuleUpdate.__iifFileMoveHintWrapped) return false;
        var original = window.uiRequestRuleUpdate;
        var wrapped = function (payloadText) {
            try {
                var payload = typeof payloadText === "string" ? JSON.parse(payloadText || "{}") : payloadText;
                if (payload && payload.id && payload.originPath !== undefined) {
                    var oldPath = lastOriginPathByRule[String(payload.id)] || "";
                    var newPath = String(payload.originPath || "");
                    if (oldPath && newPath && oldPath !== newPath) {
                        toast("Rule file changed:\n" + oldPath + "\n-> " + newPath + "\nOld files are not deleted automatically.", "warn");
                    }
                }
            } catch (_) {}
            return original.apply(this, arguments);
        };
        wrapped.__iifFileMoveHintWrapped = true;
        window.uiRequestRuleUpdate = wrapped;
        return true;
    }

    hookRuleUpdateForFileMoveHint();
    var attempts = 0;
    var timer = setInterval(function () {
        attempts += 1;
        if (hookRuleUpdateForFileMoveHint() || attempts > 40) clearInterval(timer);
    }, 250);
})();

// Editor window mouse resize/drag. Keeps layout changes local to this HTML view.
(function initEditorWindowResize() {
    if (window.__iifEditorWindowResizeReady) return;
    window.__iifEditorWindowResizeReady = true;

    var storageKey = "iif.guiEditor.windowRect";
    var resizeState = null;
    var dragState = null;
    var workspaceSyncPending = false;
    var dragPaintIntervalMs = 80;

    function clamp(value, min, max) {
        return Math.max(min, Math.min(max, value));
    }

    function shouldPaintDragFrame(state, force) {
        if (!state) return false;
        if (force) {
            state.lastPaintMs = Date.now();
            return true;
        }
        var now = Date.now();
        if (!state.lastPaintMs || now - state.lastPaintMs >= dragPaintIntervalMs) {
            state.lastPaintMs = now;
            return true;
        }
        return false;
    }

    function viewportMaxWidth() {
        return Math.max(760, window.innerWidth - 20);
    }

    function viewportMaxHeight() {
        return Math.max(560, window.innerHeight - 20);
    }

    function panelMinWidth(panel) {
        return Number(panel.dataset.minWidth || 980);
    }

    function panelMinHeight(panel) {
        return Number(panel.dataset.minHeight || 640);
    }

    function savePanelRect(panel) {
        try {
            var rect = panel.getBoundingClientRect();
            localStorage.setItem(storageKey, JSON.stringify({
                left: Math.round(rect.left),
                top: Math.round(rect.top),
                width: Math.round(rect.width),
                height: Math.round(rect.height)
            }));
        } catch (_) {}
    }

    function applyPanelRect(panel, rect) {
        if (!rect) return;
        var maxW = viewportMaxWidth();
        var maxH = viewportMaxHeight();
        var width = clamp(Number(rect.width || 0), panelMinWidth(panel), maxW);
        var height = clamp(Number(rect.height || 0), panelMinHeight(panel), maxH);
        var left = clamp(Number(rect.left || panel.getBoundingClientRect().left), 4, Math.max(4, window.innerWidth - width - 4));
        var top = clamp(Number(rect.top || panel.getBoundingClientRect().top), 4, Math.max(4, window.innerHeight - height - 4));
        panel.style.width = width + "px";
        panel.style.height = height + "px";
        panel.style.left = left + "px";
        panel.style.top = top + "px";
    }

    function restorePanelRect(panel) {
        try {
            var raw = localStorage.getItem(storageKey);
            if (!raw) return;
            applyPanelRect(panel, JSON.parse(raw));
        } catch (_) {}
    }

    function startResize(event, mode, panel) {
        event.preventDefault();
        event.stopPropagation();
        var rect = panel.getBoundingClientRect();
        resizeState = {
            panel: panel,
            mode: mode,
            startX: event.clientX,
            startY: event.clientY,
            lastX: event.clientX,
            lastY: event.clientY,
            lastPaintMs: 0,
            left: rect.left,
            top: rect.top,
            width: rect.width,
            height: rect.height
        };
        panel.classList.add("iif-resizing");
    }

    function startDrag(event, panel) {
        if (event.button !== 0) return;
        var target = event.target;
        if (target && target.closest && target.closest("button, input, select, textarea, a, .iif-resize-handle")) return;
        event.preventDefault();
        event.stopPropagation();
        var rect = panel.getBoundingClientRect();
        dragState = {
            panel: panel,
            startX: event.clientX,
            startY: event.clientY,
            lastX: event.clientX,
            lastY: event.clientY,
            lastPaintMs: 0,
            left: rect.left,
            top: rect.top,
            width: rect.width,
            height: rect.height
        };
        panel.classList.add("iif-dragging");
    }

    function scheduleWorkspaceSync() {
        if (workspaceSyncPending) return;
        workspaceSyncPending = true;
        requestAnimationFrame(function () {
            workspaceSyncPending = false;
            if (typeof window.__iifSyncWorkspaceLayout === "function") window.__iifSyncWorkspaceLayout();
        });
    }

    function applyResizeFrame(force) {
        if (!resizeState || !shouldPaintDragFrame(resizeState, force)) return;
        var panel = resizeState.panel;
        var dx = resizeState.lastX - resizeState.startX;
        var dy = resizeState.lastY - resizeState.startY;
        var width = resizeState.width;
        var height = resizeState.height;
        if (resizeState.mode === "right" || resizeState.mode === "corner") {
            width = resizeState.width + dx;
        }
        if (resizeState.mode === "bottom" || resizeState.mode === "corner") {
            height = resizeState.height + dy;
        }
        width = clamp(width, panelMinWidth(panel), viewportMaxWidth() - resizeState.left);
        height = clamp(height, panelMinHeight(panel), viewportMaxHeight() - resizeState.top);
        panel.style.width = Math.round(width) + "px";
        panel.style.height = Math.round(height) + "px";
        scheduleWorkspaceSync();
    }

    function applyDragFrame(force) {
        if (!dragState || !shouldPaintDragFrame(dragState, force)) return;
        var dragPanel = dragState.panel;
        var left = clamp(dragState.left + dragState.lastX - dragState.startX, 4, Math.max(4, window.innerWidth - dragState.width - 4));
        var top = clamp(dragState.top + dragState.lastY - dragState.startY, 4, Math.max(4, window.innerHeight - dragState.height - 4));
        dragPanel.style.left = Math.round(left) + "px";
        dragPanel.style.top = Math.round(top) + "px";
    }

    function onMouseMove(event) {
        if (resizeState) {
            event.preventDefault();
            resizeState.lastX = event.clientX;
            resizeState.lastY = event.clientY;
            applyResizeFrame(false);
            return;
        }
        if (dragState) {
            event.preventDefault();
            dragState.lastX = event.clientX;
            dragState.lastY = event.clientY;
            applyDragFrame(false);
        }
    }

    function onMouseUp() {
        if (resizeState) {
            applyResizeFrame(true);
            var panel = resizeState.panel;
            panel.classList.remove("iif-resizing");
            if (typeof window.__iifSyncWorkspaceLayout === "function") window.__iifSyncWorkspaceLayout();
            savePanelRect(panel);
            resizeState = null;
        }
        if (dragState) {
            applyDragFrame(true);
            dragState.panel.classList.remove("iif-dragging");
            savePanelRect(dragState.panel);
            dragState = null;
        }
    }

    function addHandle(panel, mode) {
        var handle = document.createElement("div");
        handle.className = "iif-resize-handle " + mode;
        handle.title = mode === "corner" ? "Resize editor" : "Resize editor " + mode;
        handle.addEventListener("mousedown", function (event) { startResize(event, mode, panel); }, true);
        panel.appendChild(handle);
    }

    function install() {
        var panel = document.querySelector(".editor-panel");
        if (!panel || panel.dataset.resizeInstalled === "1") return;
        panel.dataset.resizeInstalled = "1";
        panel.dataset.minWidth = "980";
        panel.dataset.minHeight = "640";
        restorePanelRect(panel);
        addHandle(panel, "right");
        addHandle(panel, "bottom");
        addHandle(panel, "corner");
        var header = panel.querySelector(":scope > .header");
        if (header && header.dataset.dragInstalled !== "1") {
            header.dataset.dragInstalled = "1";
            header.addEventListener("mousedown", function (event) { startDrag(event, panel); }, true);
        }
    }

    document.addEventListener("mousemove", onMouseMove, true);
    document.addEventListener("mouseup", onMouseUp, true);
    window.addEventListener("resize", function () {
        var panel = document.querySelector(".editor-panel");
        if (!panel) return;
        var rect = panel.getBoundingClientRect();
        applyPanelRect(panel, rect);
        if (typeof window.__iifSyncWorkspaceLayout === "function") window.__iifSyncWorkspaceLayout();
        savePanelRect(panel);
    });

    if (document.readyState === "loading") document.addEventListener("DOMContentLoaded", install);
    else install();
    setTimeout(install, 0);
})();

// Layout migration: dedicate the full left pane to Rule Relationship Tree.
(function initTreeFocusedLayout() {
    if (window.__iifTreeFocusedLayoutReady) return;
    window.__iifTreeFocusedLayoutReady = true;
    return; // Superseded by initRedesignedEditorLayout; keep disabled to avoid double DOM reflow in PrismaUI.

    function directSections(parent) {
        var out = [];
        if (!parent) return out;
        for (var i = 0; i < parent.children.length; ++i) {
            var child = parent.children[i];
            if (child.classList && child.classList.contains("section")) out.push(child);
        }
        return out;
    }

    function titleText(section) {
        if (!section) return "";
        var title = section.querySelector(".section-title");
        return String(title ? title.textContent : "").replace(/\s+/g, " ").trim().toLowerCase();
    }

    function findSection(sections, needles) {
        for (var i = 0; i < sections.length; ++i) {
            var text = titleText(sections[i]);
            for (var n = 0; n < needles.length; ++n) {
                if (text.indexOf(needles[n]) >= 0) return sections[i];
            }
        }
        return null;
    }

    function makeDrawer() {
        var drawer = document.createElement("div");
        drawer.id = "iifToolsDrawer";
        drawer.className = "section iif-tools-drawer";
        var title = document.createElement("div");
        title.className = "section-title iif-tools-drawer-title";
        title.innerHTML = '<span><span class="iif-tools-drawer-caret">\u25B6</span>TOOLS / CREATE / RULE LIST</span><span class="small-note">collapsed</span>';
        var body = document.createElement("div");
        body.className = "iif-tools-drawer-body";
        title.onclick = function () {
            var open = !drawer.classList.contains("open");
            drawer.classList.toggle("open", open);
            var caret = drawer.querySelector(".iif-tools-drawer-caret");
            var note = drawer.querySelector(".small-note");
            if (caret) caret.textContent = open ? "\u25BC" : "\u25B6";
            if (note) note.textContent = open ? "open" : "collapsed";
        };
        drawer.appendChild(title);
        drawer.appendChild(body);
        return drawer;
    }

    function install() {
        var content = document.querySelector(".content");
        var left = document.querySelector(".left-col");
        var right = document.querySelector(".right-col");
        if (!content || !left || !right || left.dataset.treeFocusedLayout === "1") return;

        var sections = directSections(left);
        var quickTools = findSection(sections, ["quick tools", "快捷工具"]);
        var createRule = findSection(sections, ["create new rule", "创建新规则"]);
        var ruleList = findSection(sections, ["rule list", "规则列表"]);
        var ruleTree = findSection(sections, ["rule relationship tree", "规则关系树"]);
        if (!ruleTree) return;

        var drawer = makeDrawer();
        var body = drawer.querySelector(".iif-tools-drawer-body");
        if (quickTools) body.appendChild(quickTools);
        if (createRule) body.appendChild(createRule);
        if (ruleList) body.appendChild(ruleList);

        right.insertBefore(drawer, right.firstChild);
        left.appendChild(ruleTree);
        left.classList.add("iif-tree-focused");
        content.classList.add("iif-tree-focused-layout");
        left.dataset.treeFocusedLayout = "1";

        try {
            if (typeof refreshRuleTree === "function") refreshRuleTree();
            if (typeof repaintRuleListFromCachedData === "function") repaintRuleListFromCachedData();
            if (typeof logLine === "function") logLine("layout changed: left pane dedicated to relationship tree");
        } catch (_) {}
    }

    if (document.readyState === "loading") document.addEventListener("DOMContentLoaded", install);
    else install();
    setTimeout(install, 0);
    setTimeout(install, 800);
})();

// Redesigned layout: top toolbar, left create/tree, center list/log, right details.
(function initRedesignedEditorLayout() {
    if (window.__iifRedesignedEditorLayoutReady) return;
    window.__iifRedesignedEditorLayoutReady = true;

    function directSections(parent) {
        var out = [];
        if (!parent) return out;
        for (var i = 0; i < parent.children.length; ++i) {
            var child = parent.children[i];
            if (child.classList && child.classList.contains("section")) out.push(child);
        }
        return out;
    }

    function sectionTitle(section) {
        var title = section ? section.querySelector(".section-title") : null;
        return String(title ? title.textContent : "").replace(/\s+/g, " ").trim().toLowerCase();
    }

    function matchSection(section, keys) {
        var t = sectionTitle(section);
        for (var i = 0; i < keys.length; ++i) {
            if (t.indexOf(keys[i]) >= 0) return true;
        }
        return false;
    }

    function collectSection(root, keys) {
        if (!root) return null;
        var sections = root.querySelectorAll(".section");
        for (var i = 0; i < sections.length; ++i) {
            if (matchSection(sections[i], keys)) return sections[i];
        }
        return null;
    }

    function ensureColumn(className) {
        var col = document.querySelector("." + className);
        if (col) return col;
        col = document.createElement("div");
        col.className = className;
        return col;
    }

    function unwrapToolsDrawer(toolsCol) {
        var drawer = document.getElementById("iifToolsDrawer");
        if (!drawer) return;
        var body = drawer.querySelector(".iif-tools-drawer-body");
        if (!body) return;
        var moving = [];
        for (var i = 0; i < body.children.length; ++i) moving.push(body.children[i]);
        for (var j = 0; j < moving.length; ++j) toolsCol.appendChild(moving[j]);
        if (drawer.parentNode) drawer.parentNode.removeChild(drawer);
    }

    function install() {
        var panel = document.querySelector(".editor-panel");
        var content = document.querySelector(".content");
        var left = document.querySelector(".left-col");
        var existingRight = document.querySelector(".right-col");
        if (!panel || !content || !left || !existingRight || panel.dataset.redesignedLayout === "1") return;

        panel.dataset.redesignedLayout = "1";
        panel.classList.add("iif-redesigned-layout");
        content.classList.add("iif-redesigned-content");
        content.classList.remove("iif-tree-focused-layout");
        left.classList.remove("iif-tree-focused");
        delete left.dataset.treeFocusedLayout;

        var center = ensureColumn("center-col");
        var tools = ensureColumn("tools-col");
        unwrapToolsDrawer(tools);

        var tree = collectSection(content, ["rule relationship tree", "规则关系树"]);
        var details = collectSection(content, ["rule details", "规则详情"]);
        var runtime = collectSection(content, ["runtime log", "运行日志"]);
        var quick = collectSection(content, ["quick tools", "快捷工具"]) || collectSection(tools, ["quick tools", "快捷工具"]);
        var create = collectSection(content, ["create new rule", "创建新规则"]) || collectSection(tools, ["create new rule", "创建新规则"]);
        var list = collectSection(content, ["rule list", "规则列表"]) || collectSection(tools, ["rule list", "规则列表"]);

        if (create) {
            create.classList.add("iif-create-card");
            left.appendChild(create);
        }
        if (tree) {
            tree.classList.add("iif-tree-card");
            left.appendChild(tree);
        }
        if (list) {
            list.classList.add("iif-rule-list-card");
            center.appendChild(list);
        }
        if (runtime) {
            runtime.classList.add("iif-runtime-card");
            center.appendChild(runtime);
        }
        if (details) {
            details.classList.add("iif-rule-details-card");
            tools.appendChild(details);
        }
        if (quick) {
            quick.classList.add("iif-top-toolbar");
            panel.insertBefore(quick, content);
        }

        content.innerHTML = "";
        content.appendChild(left);
        content.appendChild(center);
        content.appendChild(tools);

        try {
            if (typeof refreshRuleTree === "function") refreshRuleTree();
            if (typeof repaintRuleListFromCachedData === "function") repaintRuleListFromCachedData();
            if (typeof logLine === "function") logLine("layout changed: create/tree/list/details columns");
        } catch (_) {}
    }

    if (document.readyState === "loading") document.addEventListener("DOMContentLoaded", install);
    else install();
    setTimeout(install, 0);
    setTimeout(install, 800);
})();

// Workspace-style free docking and four-side panel resizing.
(function initEditorPaneWorkspace() {
    if (window.__iifEditorPaneWorkspaceV2Ready) return;
    window.__iifEditorPaneWorkspaceV2Ready = true;

    var widthStorageKey = "iif.guiEditor.columnWidthsV3";
    var floatingZ = 100080;
    var panelDragState = null;
    var paneResizeState = null;
    var dockPlaceholder = null;
    var lastDockHintColumn = null;
    var lastDockHintRef = null;
    var lastDockHintHeight = 0;
    var dragPaintIntervalMs = 80;

    function clamp(value, min, max) {
        return Math.max(min, Math.min(max, value));
    }

    function shouldPaintDragFrame(state, force) {
        if (!state) return false;
        if (force) {
            state.lastPaintMs = Date.now();
            return true;
        }
        var now = Date.now();
        if (!state.lastPaintMs || now - state.lastPaintMs >= dragPaintIntervalMs) {
            state.lastPaintMs = now;
            return true;
        }
        return false;
    }

    function getColumns(content) {
        if (!content) return null;
        var left = content.querySelector(":scope > .left-col");
        var center = content.querySelector(":scope > .center-col");
        var tools = content.querySelector(":scope > .tools-col");
        if (!left || !center || !tools) return null;
        return [left, center, tools];
    }

    function columnWidths(content) {
        var cols = getColumns(content);
        if (!cols) return null;
        return [
            cols[0].getBoundingClientRect().width,
            cols[1].getBoundingClientRect().width,
            cols[2].getBoundingClientRect().width
        ];
    }

    function saveColumnWidths(content) {
        try {
            var widths = columnWidths(content);
            if (!widths) return;
            localStorage.setItem(widthStorageKey, JSON.stringify(widths.map(function (value) {
                return Math.round(value);
            })));
        } catch (_) {}
    }

    function loadColumnWidths() {
        try {
            var raw = JSON.parse(localStorage.getItem(widthStorageKey) || "null");
            if (!raw || raw.length !== 3) return null;
            var out = [Number(raw[0]), Number(raw[1]), Number(raw[2])];
            if (!isFinite(out[0]) || !isFinite(out[1]) || !isFinite(out[2])) return null;
            return out;
        } catch (_) {
            return null;
        }
    }

    function applyColumnWidths(content, widths) {
        if (!content || !widths) return;
        var quickTools = document.querySelector(".editor-panel.iif-redesigned-layout > .iif-top-toolbar");
        var basisWidth = quickTools ? quickTools.getBoundingClientRect().width : content.clientWidth;
        if (basisWidth > 0) content.style.width = Math.round(basisWidth) + "px";
        var gapText = window.getComputedStyle(content).columnGap || window.getComputedStyle(content).gap || "0";
        var gap = parseFloat(gapText) || 0;
        var available = Math.max(760, basisWidth - gap * 2);
        var mins = [280, 280, 220];
        widths = [
            Math.max(mins[0], Number(widths[0]) || mins[0]),
            Math.max(mins[1], Number(widths[1]) || mins[1]),
            Math.max(mins[2], Number(widths[2]) || mins[2])
        ];
        var total = widths[0] + widths[1] + widths[2];
        if (total > 0) {
            var scale = available / total;
            widths = widths.map(function (value, index) {
                return Math.max(mins[index], value * scale);
            });
            total = widths[0] + widths[1] + widths[2];
            if (total > available) {
                var overflowScale = available / total;
                widths = widths.map(function (value, index) {
                    return Math.max(mins[index], value * overflowScale);
                });
            }
        }
        content.style.gridTemplateColumns = Math.round(widths[0]) + "px " + Math.round(widths[1]) + "px " + Math.round(widths[2]) + "px";
    }

    function syncContentToQuickTools(content) {
        if (!content) return;
        var widths = loadColumnWidths() || columnWidths(content);
        if (widths) applyColumnWidths(content, widths);
    }

    function stretchDockedColumnHeights(content) {
        var cols = getColumns(content);
        if (!cols) return;
        for (var c = 0; c < cols.length; ++c) {
            var sections = cols[c].querySelectorAll(":scope > .section:not(.iif-floating-section)");
            if (!sections.length) continue;
            for (var i = 0; i < sections.length; ++i) {
                normalizeDockedSection(sections[i]);
            }
        }
        var ruleList = content.querySelector(":scope .section.iif-rule-list-card:not(.iif-floating-section)");
        if (ruleList) {
            ruleList.style.flex = "1 1 auto";
            ruleList.style.height = "";
        }
    }

    function normalizeDockedSection(section) {
        if (!section || section.classList.contains("iif-floating-section")) return;
        if (section.classList.contains("iif-rule-details-card")) unwrapRuleDetailsScrollBody(section);
        section.style.left = "";
        section.style.top = "";
        section.style.width = "";
        section.style.maxWidth = "";
        section.style.zIndex = "";
        section.style.boxSizing = "border-box";
        var fills = section.querySelectorAll(".runtime-log, .log, .rule-list, .rule-tree, .anchor-preview, .preview-area");
        var hasScrollRegion = fills.length > 0 || !!section.querySelector(":scope > .section-scroll-body");
        section.classList.toggle("iif-scroll-card", hasScrollRegion);
        for (var i = 0; i < fills.length; ++i) {
            fills[i].style.maxHeight = "";
            fills[i].style.maxWidth = "100%";
            fills[i].style.overflow = "auto";
        }
        var runtime = section.querySelector(".runtime-log, .log");
        if (runtime) {
            runtime.style.height = "";
            runtime.style.flex = "1 1 auto";
            runtime.style.minHeight = "80px";
        }
        if (section.classList.contains("iif-runtime-card")) {
            section.style.display = "flex";
            section.style.flexDirection = "column";
            section.style.overflow = "hidden";
        }
        if (section.classList.contains("iif-rule-details-card")) {
            section.style.maxHeight = "";
            section.style.maxWidth = "100%";
            section.style.height = "";
            section.style.display = "flex";
            section.style.flexDirection = "column";
            section.style.overflow = "hidden";
            var scrollBody = section.querySelector(":scope > .section-scroll-body");
            if (scrollBody) {
                scrollBody.style.display = "";
                scrollBody.style.flex = "1 1 auto";
                scrollBody.style.minHeight = "0";
                scrollBody.style.height = "";
                scrollBody.style.overflowY = "auto";
                scrollBody.style.overflowX = "hidden";
                scrollBody.style.maxHeight = "";
            }
            var fields = section.querySelectorAll("input, select, textarea, pre");
            for (var f = 0; f < fields.length; ++f) {
                fields[f].style.maxWidth = "100%";
                fields[f].style.boxSizing = "border-box";
            }
            var wideBlocks = section.querySelectorAll(".anchor-row, .iif-block-form, .iif-condition-form, .iif-box-form, .toolbar");
            for (var w = 0; w < wideBlocks.length; ++w) {
                wideBlocks[w].style.maxWidth = "100%";
                wideBlocks[w].style.overflowX = "hidden";
            }
        }
    }

    function ensureRuleDetailsScrollBody(section) {
        unwrapRuleDetailsScrollBody(section);
    }

    function unwrapRuleDetailsScrollBody(section) {
        if (!section) return;
        var body = section.querySelector(":scope > .iif-rule-details-body");
        if (body) {
            while (body.firstChild) {
                section.insertBefore(body.firstChild, body);
            }
            if (body.parentNode) body.parentNode.removeChild(body);
        }
        delete section.dataset.ruleDetailsBodyReady;
    }

    function snapDockedLayoutToOuterFrame(content) {
        if (!content) return;
        var panel = document.querySelector(".editor-panel.iif-redesigned-layout");
        var quickTools = document.querySelector(".editor-panel.iif-redesigned-layout > .iif-top-toolbar");
        var cols = getColumns(content);
        if (!panel || !quickTools || !cols) return;

        var snapPx = 36;
        var panelRect = panel.getBoundingClientRect();
        var quickRect = quickTools.getBoundingClientRect();
        var contentRect = content.getBoundingClientRect();
        var rightColRect = cols[2].getBoundingClientRect();
        var leftColRect = cols[0].getBoundingClientRect();
        var visualRight = panelRect.right - 12;
        var visualLeft = panelRect.left + 12;
        var visualBottom = panelRect.bottom - 12;

        if (Math.abs(quickRect.right - visualRight) <= snapPx || Math.abs(contentRect.right - visualRight) <= snapPx || Math.abs(rightColRect.right - visualRight) <= snapPx) {
            quickTools.style.marginRight = "12px";
            content.style.marginRight = "12px";
            content.style.maxWidth = "calc(100% - 24px)";
            content.style.width = Math.max(760, Math.round(panelRect.width - 24)) + "px";
            var widths = columnWidths(content);
            if (widths) applyColumnWidths(content, widths);
        }

        if (Math.abs(quickRect.left - visualLeft) <= snapPx || Math.abs(contentRect.left - visualLeft) <= snapPx || Math.abs(leftColRect.left - visualLeft) <= snapPx) {
            quickTools.style.marginLeft = "12px";
            content.style.marginLeft = "12px";
        }

        var dockedSections = content.querySelectorAll(":scope .section:not(.iif-floating-section)");
        for (var i = 0; i < dockedSections.length; ++i) {
            var section = dockedSections[i];
            var rect = section.getBoundingClientRect();
            if (Math.abs(rect.right - visualRight) <= snapPx) {
                section.style.width = "";
                section.style.maxWidth = "";
            }
            if (Math.abs(rect.bottom - visualBottom) <= snapPx) {
                section.style.maxHeight = "";
                if (section.classList.contains("iif-rule-list-card")) {
                    section.style.height = "";
                    section.style.flex = "1 1 auto";
                }
            }
        }
    }

    function syncWorkspaceLayout() {
        var content = document.querySelector(".editor-panel.iif-redesigned-layout .content");
        if (!content) return;
        syncContentToQuickTools(content);
        stretchDockedColumnHeights(content);
        snapDockedLayoutToOuterFrame(content);
    }

    window.__iifSyncWorkspaceLayout = syncWorkspaceLayout;

    function removeOldSplitters(content) {
        var nodes = content ? content.querySelectorAll(".iif-column-splitter") : [];
        for (var i = 0; i < nodes.length; ++i) {
            if (nodes[i].parentNode) nodes[i].parentNode.removeChild(nodes[i]);
        }
    }

    function ensureSectionMarker(section) {
        if (section.__iifDockMarker) return;
        var marker = document.createComment("iif dock marker");
        section.parentNode.insertBefore(marker, section);
        section.__iifDockMarker = marker;
    }

    function labelFor(section, floating) {
        var button = section.querySelector(":scope > .section-title .iif-dock-toggle");
        if (!button) return;
        button.textContent = floating ? "停靠" : "浮动";
        button.title = floating ? "Dock this panel into the pointed column" : "Float this panel";
    }

    function detachSection(section, rect) {
        if (!section || section.classList.contains("iif-floating-section")) return;
        ensureSectionMarker(section);
        var box = rect || section.getBoundingClientRect();
        section.classList.add("iif-floating-section");
        section.style.left = Math.round(box.left) + "px";
        section.style.top = Math.round(box.top) + "px";
        section.style.width = Math.max(300, Math.round(box.width)) + "px";
        section.style.height = Math.max(180, Math.round(box.height)) + "px";
        section.style.zIndex = String(++floatingZ);
        section.style.flex = "";
        document.body.appendChild(section);
        labelFor(section, true);
    }

    function placeDockHint(column, ref, draggedSection) {
        if (!column) return;
        if (!dockPlaceholder) {
            dockPlaceholder = document.createElement("div");
            dockPlaceholder.className = "iif-dock-placeholder";
        }
        var height = draggedSection ? ((panelDragState && panelDragState.section === draggedSection) ? panelDragState.height : draggedSection.getBoundingClientRect().height) : 140;
        var roundedHeight = Math.round(clamp(height, 120, Math.max(160, window.innerHeight - 160)));
        if (lastDockHintColumn === column && lastDockHintRef === ref && lastDockHintHeight === roundedHeight && dockPlaceholder.parentNode === column) return;
        clearDockHints();
        lastDockHintColumn = column;
        lastDockHintRef = ref || null;
        lastDockHintHeight = roundedHeight;
        dockPlaceholder.style.height = roundedHeight + "px";
        column.classList.add("iif-dock-target-hint");
        column.insertBefore(dockPlaceholder, ref || null);
    }

    function clearDockHints() {
        var cols = document.querySelectorAll(".iif-dock-target-hint");
        for (var i = 0; i < cols.length; ++i) cols[i].classList.remove("iif-dock-target-hint");
        if (dockPlaceholder && dockPlaceholder.parentNode) dockPlaceholder.parentNode.removeChild(dockPlaceholder);
        lastDockHintColumn = null;
        lastDockHintRef = null;
        lastDockHintHeight = 0;
    }

    function dockSection(section, targetColumn, ref) {
        if (!section) return;
        ensureSectionMarker(section);
        targetColumn = targetColumn || (section.__iifDockMarker && section.__iifDockMarker.parentNode);
        if (!targetColumn) return;
        var wasFloating = section.classList.contains("iif-floating-section");
        ref = ref || (dockPlaceholder && dockPlaceholder.parentNode === targetColumn ? dockPlaceholder : null);
        targetColumn.insertBefore(section.__iifDockMarker, ref || null);
        targetColumn.insertBefore(section, ref || null);
        section.classList.remove("iif-floating-section");
        section.style.left = "";
        section.style.top = "";
        section.style.width = "";
        section.style.zIndex = "";
        if (wasFloating) {
            section.style.height = "";
            section.style.flex = "";
        }
        labelFor(section, false);
        normalizeDockedSection(section);
        clearDockHints();
    }

    function pointerInside(node, event) {
        if (!node) return false;
        var r = node.getBoundingClientRect();
        return event.clientX >= r.left && event.clientX <= r.right && event.clientY >= r.top && event.clientY <= r.bottom;
    }

    function findDockColumn(event) {
        var content = document.querySelector(".editor-panel.iif-redesigned-layout .content");
        var cols = getColumns(content);
        if (!content || !cols) return null;
        var quickTools = document.querySelector(".editor-panel.iif-redesigned-layout > .iif-top-toolbar");
        var basisRect = quickTools ? quickTools.getBoundingClientRect() : content.getBoundingClientRect();
        if (basisRect && event.clientX >= basisRect.left && event.clientX <= basisRect.right) {
            var widths = columnWidths(content);
            if (widths) {
                var total = widths[0] + widths[1] + widths[2];
                if (total > 0) {
                    var x = clamp(event.clientX - basisRect.left, 0, basisRect.width);
                    var scaled = x / Math.max(1, basisRect.width) * total;
                    if (scaled <= widths[0]) return cols[0];
                    if (scaled <= widths[0] + widths[1]) return cols[1];
                    return cols[2];
                }
            }
        }
        for (var i = 0; i < cols.length; ++i) {
            if (pointerInside(cols[i], event)) return cols[i];
        }
        if (!pointerInside(content, event)) return null;
        var best = null;
        var bestDistance = Infinity;
        for (var j = 0; j < cols.length; ++j) {
            var rect = cols[j].getBoundingClientRect();
            var center = rect.left + rect.width * 0.5;
            var distance = Math.abs(event.clientX - center);
            if (distance < bestDistance) {
                best = cols[j];
                bestDistance = distance;
            }
        }
        return best;
    }

    function dockRefFor(column, section, event) {
        if (!column) return null;
        var children = column.children;
        for (var i = 0; i < children.length; ++i) {
            var child = children[i];
            if (!child.classList || !child.classList.contains("section")) continue;
            if (child === section || child.classList.contains("iif-floating-section")) continue;
            var rect = child.getBoundingClientRect();
            if (event.clientY < rect.top + rect.height * 0.5) return child;
        }
        return null;
    }

    function columnIndexOf(column) {
        var content = document.querySelector(".editor-panel.iif-redesigned-layout .content");
        var cols = getColumns(content);
        if (!cols) return -1;
        for (var i = 0; i < cols.length; ++i) {
            if (cols[i] === column) return i;
        }
        return -1;
    }

    function sectionColumn(section) {
        var node = section ? section.parentNode : null;
        while (node) {
            if (node.classList && (node.classList.contains("left-col") || node.classList.contains("center-col") || node.classList.contains("tools-col"))) return node;
            node = node.parentNode;
        }
        return null;
    }

    function beginPanelDrag(section, event) {
        if (event.button !== 0) return;
        var target = event.target;
        if (target && target.closest && target.closest(".iif-pane-resize-handle")) return;
        if (target && target.closest && target.closest("button, input, select, textarea, a")) return;
        var rect = section.getBoundingClientRect();
        panelDragState = {
            section: section,
            startX: event.clientX,
            startY: event.clientY,
            offsetX: event.clientX - rect.left,
            offsetY: event.clientY - rect.top,
            rect: rect,
            width: rect.width,
            height: rect.height,
            detachedAtStart: section.classList.contains("iif-floating-section"),
            active: false,
            lastX: event.clientX,
            lastY: event.clientY,
            lastPaintMs: 0
        };
    }

    function applyPanelDragFrame(force) {
        if (!panelDragState) return;
        if (!shouldPaintDragFrame(panelDragState, force)) return;
        var section = panelDragState.section;
        var dx = panelDragState.lastX - panelDragState.startX;
        var dy = panelDragState.lastY - panelDragState.startY;
        if (!panelDragState.active && Math.abs(dx) + Math.abs(dy) < 8) return;
        if (!panelDragState.active) {
            panelDragState.active = true;
            if (!panelDragState.detachedAtStart) detachSection(section, panelDragState.rect);
            section.style.zIndex = String(++floatingZ);
            document.body.classList.add("iif-floating-dragging");
        }
        var width = panelDragState.width;
        var height = panelDragState.height;
        var left = clamp(panelDragState.lastX - panelDragState.offsetX, 8, window.innerWidth - Math.min(width, window.innerWidth - 16));
        var top = clamp(panelDragState.lastY - panelDragState.offsetY, 8, window.innerHeight - Math.min(height, window.innerHeight - 16));
        section.style.left = Math.round(left) + "px";
        section.style.top = Math.round(top) + "px";
        var dragEvent = { clientX: panelDragState.lastX, clientY: panelDragState.lastY };
        var targetColumn = findDockColumn(dragEvent);
        var ref = dockRefFor(targetColumn, section, dragEvent);
        placeDockHint(targetColumn, ref, section);
    }

    function onPanelDragMove(event) {
        if (!panelDragState) return;
        panelDragState.lastX = event.clientX;
        panelDragState.lastY = event.clientY;
        event.preventDefault();
        applyPanelDragFrame(false);
    }

    function onPanelDragUp(event) {
        if (!panelDragState) return;
        panelDragState.lastX = event.clientX;
        panelDragState.lastY = event.clientY;
        applyPanelDragFrame(true);
        var section = panelDragState.section;
        if (panelDragState.active && section.classList.contains("iif-floating-section")) {
            var targetColumn = findDockColumn(event);
            if (targetColumn) dockSection(section, targetColumn, dockPlaceholder && dockPlaceholder.parentNode === targetColumn ? dockPlaceholder : dockRefFor(targetColumn, section, event));
        }
        if (!targetColumn) clearDockHints();
        document.body.classList.remove("iif-floating-dragging");
        panelDragState = null;
    }

    function resizeModeHas(mode, part) {
        return mode.indexOf(part) !== -1 || (part === "left" && (mode === "tl" || mode === "bl")) || (part === "right" && (mode === "tr" || mode === "br")) || (part === "top" && (mode === "tl" || mode === "tr")) || (part === "bottom" && (mode === "bl" || mode === "br"));
    }

    function beginPaneResize(section, mode, event) {
        event.preventDefault();
        event.stopPropagation();
        var rect = section.getBoundingClientRect();
        var content = document.querySelector(".editor-panel.iif-redesigned-layout .content");
        var column = sectionColumn(section);
        paneResizeState = {
            section: section,
            mode: mode,
            startX: event.clientX,
            startY: event.clientY,
            rect: rect,
            floating: section.classList.contains("iif-floating-section"),
            content: content,
            column: column,
            columnIndex: columnIndexOf(column),
            widths: columnWidths(content),
            lastX: event.clientX,
            lastY: event.clientY,
            lastPaintMs: 0
        };
        if (content && paneResizeState.widths) applyColumnWidths(content, paneResizeState.widths);
        section.classList.add("iif-resizing");
        document.body.classList.add("iif-pane-resizing");
    }

    function resizeDockedColumns(state, dx) {
        if (!state.content || !state.widths || state.columnIndex < 0) return;
        var widths = state.widths.slice();
        var mins = [280, 280, 220];
        var i = state.columnIndex;
        if (resizeModeHas(state.mode, "left") && i > 0) {
            var pairLeft = state.widths[i - 1] + state.widths[i];
            widths[i] = clamp(state.widths[i] - dx, mins[i], pairLeft - mins[i - 1]);
            widths[i - 1] = pairLeft - widths[i];
        }
        if (resizeModeHas(state.mode, "right") && i < 2) {
            var pairRight = state.widths[i] + state.widths[i + 1];
            widths[i] = clamp(state.widths[i] + dx, mins[i], pairRight - mins[i + 1]);
            widths[i + 1] = pairRight - widths[i];
        }
        applyColumnWidths(state.content, widths);
    }

    function resizeDockedHeight(state, dy) {
        if (!resizeModeHas(state.mode, "top") && !resizeModeHas(state.mode, "bottom")) return;
        var height = state.rect.height;
        if (resizeModeHas(state.mode, "top")) height = state.rect.height - dy;
        if (resizeModeHas(state.mode, "bottom")) height = state.rect.height + dy;
        height = clamp(height, 150, Math.max(180, window.innerHeight - 90));
        state.section.style.flex = "0 0 " + Math.round(height) + "px";
        state.section.style.height = Math.round(height) + "px";
    }

    function resizeFloatingPanel(state, dx, dy) {
        var minW = 300;
        var minH = 180;
        var left = state.rect.left;
        var top = state.rect.top;
        var width = state.rect.width;
        var height = state.rect.height;
        if (resizeModeHas(state.mode, "left")) {
            left = clamp(state.rect.left + dx, 8, state.rect.right - minW);
            width = state.rect.right - left;
        }
        if (resizeModeHas(state.mode, "right")) {
            width = clamp(state.rect.width + dx, minW, window.innerWidth - state.rect.left - 8);
        }
        if (resizeModeHas(state.mode, "top")) {
            top = clamp(state.rect.top + dy, 8, state.rect.bottom - minH);
            height = state.rect.bottom - top;
        }
        if (resizeModeHas(state.mode, "bottom")) {
            height = clamp(state.rect.height + dy, minH, window.innerHeight - state.rect.top - 8);
        }
        state.section.style.left = Math.round(left) + "px";
        state.section.style.top = Math.round(top) + "px";
        state.section.style.width = Math.round(width) + "px";
        state.section.style.height = Math.round(height) + "px";
    }

    function applyPaneResizeFrame(force) {
        if (!paneResizeState) return;
        if (!shouldPaintDragFrame(paneResizeState, force)) return;
        var dx = paneResizeState.lastX - paneResizeState.startX;
        var dy = paneResizeState.lastY - paneResizeState.startY;
        if (paneResizeState.floating) {
            resizeFloatingPanel(paneResizeState, dx, dy);
        } else {
            resizeDockedColumns(paneResizeState, dx);
            resizeDockedHeight(paneResizeState, dy);
        }
    }

    function onPaneResizeMove(event) {
        if (!paneResizeState) return;
        paneResizeState.lastX = event.clientX;
        paneResizeState.lastY = event.clientY;
        event.preventDefault();
        applyPaneResizeFrame(false);
    }

    function onPaneResizeUp() {
        if (!paneResizeState) return;
        applyPaneResizeFrame(true);
        paneResizeState.section.classList.remove("iif-resizing");
        if (paneResizeState.content) saveColumnWidths(paneResizeState.content);
        document.body.classList.remove("iif-pane-resizing");
        paneResizeState = null;
    }

    function addPaneResizeHandles(section) {
        var modes = ["left", "right", "top", "bottom", "tl", "tr", "bl", "br"];
        for (var i = 0; i < modes.length; ++i) {
            var mode = modes[i];
            if (section.querySelector(":scope > .iif-pane-resize-handle." + mode)) continue;
            var handle = document.createElement("div");
            handle.className = "iif-pane-resize-handle " + mode;
            handle.title = "Resize panel";
            handle.addEventListener("mousedown", (function (resizeMode) {
                return function (event) { beginPaneResize(section, resizeMode, event); };
            })(mode), true);
            section.appendChild(handle);
        }
    }

    function installDetachable(section) {
        if (!section || section.dataset.detachableInstalled === "1") return;
        var title = section.querySelector(":scope > .section-title");
        if (!title) return;
        section.dataset.detachableInstalled = "1";
        section.classList.add("iif-detachable");
        ensureSectionMarker(section);
        var button = document.createElement("button");
        button.type = "button";
        button.className = "iif-dock-toggle";
        button.textContent = "浮动";
        button.addEventListener("click", function (event) {
            event.preventDefault();
            event.stopPropagation();
            if (section.classList.contains("iif-floating-section")) dockSection(section);
            else detachSection(section);
        }, true);
        title.appendChild(button);
        title.addEventListener("mousedown", function (event) { beginPanelDrag(section, event); }, true);
        addPaneResizeHandles(section);
    }

    function install() {
        var panel = document.querySelector(".editor-panel.iif-redesigned-layout");
        var content = panel ? panel.querySelector(".content") : null;
        if (!panel || !content || content.dataset.workspaceInstalled === "2") return;
        if (!getColumns(content)) return;
        content.dataset.workspaceInstalled = "2";
        content.classList.add("iif-split-active");
        removeOldSplitters(content);
        requestAnimationFrame(function () {
            syncWorkspaceLayout();
        });
        var sections = content.querySelectorAll(":scope > .left-col > .section, :scope > .center-col > .section, :scope > .tools-col > .section");
        for (var i = 0; i < sections.length; ++i) {
            installDetachable(sections[i]);
            normalizeDockedSection(sections[i]);
        }
    }

    document.addEventListener("mousemove", function (event) {
        onPanelDragMove(event);
        onPaneResizeMove(event);
    }, true);
    document.addEventListener("mouseup", function (event) {
        onPanelDragUp(event);
        onPaneResizeUp();
    }, true);
    window.addEventListener("resize", function () {
        syncWorkspaceLayout();
    });

    if (document.readyState === "loading") document.addEventListener("DOMContentLoaded", install);
    else install();
    setTimeout(install, 0);
    setTimeout(install, 900);
})();

// PrismaUI/Ultralight can be conservative with nested wheel targets.
// Route wheel input to the nearest scrollable editor pane so tree/list/detail panes scroll independently.
(function initEditorNestedWheelScroll() {
    if (window.__iifNestedWheelScrollReady) return;
    window.__iifNestedWheelScrollReady = true;

    function nearestScroller(target) {
        var node = target && target.closest ? target.closest(".rule-tree, .rule-list, .runtime-log, .log, .section-scroll-body, .anchor-preview, .preview-area") : null;
        while (node) {
            if ((node.scrollHeight > node.clientHeight + 1) || (node.scrollWidth > node.clientWidth + 1)) return node;
            node = node.parentElement && node.parentElement.closest ? node.parentElement.closest(".rule-tree, .rule-list, .runtime-log, .log, .section-scroll-body, .anchor-preview, .preview-area") : null;
        }
        return null;
    }

    document.addEventListener("wheel", function (event) {
        if (event.defaultPrevented) return;
        var scroller = nearestScroller(event.target);
        if (!scroller) return;
        var beforeTop = scroller.scrollTop;
        var beforeLeft = scroller.scrollLeft;
        scroller.scrollTop += event.deltaY || 0;
        scroller.scrollLeft += event.deltaX || 0;
        if (scroller.scrollTop !== beforeTop || scroller.scrollLeft !== beforeLeft) {
            event.preventDefault();
            event.stopPropagation();
        }
    }, { capture: true, passive: false });
})();

// Performance: debounce the fully-composed refreshRuleTree so rapid cascading calls
// (click → requestRuleDetails → refreshRuleTree, plus the direct call, plus the
// initTreeMouseDragFallback wrap's setTimeout(install)) all collapse into one render.
// All IIFEs above have run, so window.refreshRuleTree is the final composed version.
(function applyPerfDebounce() {
    if (typeof refreshRuleTree === "function" && !refreshRuleTree.__perfDebounced) {
        var _core = refreshRuleTree;
        var _t = 0;
        var _debounced = function () {
            clearTimeout(_t);
            _t = setTimeout(function () { _core(); }, 20);
        };
        _debounced.__perfDebounced = true;
        window.refreshRuleTree = refreshRuleTree = _debounced;
    }
})();

// Re-apply language after all IIFEs have settled so dynamically-added text nodes
// (from workspace layout, redesigned layout, condition forms, etc.) get translated.
// Also update the lang toggle button text to match.
(function applyLangAfterIIFEs() {
    function syncLangBtn(lang) {
        var btn = document.getElementById("langToggleBtn");
        if (btn) btn.textContent = lang === "zh_CN" ? "EN" : "中文";
    }
    function run() {
        if (typeof iifApplyLanguage === "function" && typeof iifCurrentLang === "function") {
            var lang = iifCurrentLang();
            iifApplyLanguage(lang);
            syncLangBtn(lang);
        }
    }
    // Two passes: once after DOM settle, once after layout IIFEs finish painting
    setTimeout(run, 80);
    setTimeout(run, 400);
})();

const MODES = Object.freeze({
    YEAR: 0,
    MONTH: 1,
    WEEK: 2,
    DAY: 3,
    DDAY: 4,
    QUARTER: 5,
    COUNTER: 10,
    TIMER: 11,
    POMODORO: 12
});

const PAYLOAD = Object.freeze({
    NONE: "none",
    DDAY: "dday",
    COUNTER: "counter",
    TIMER: "timer",
    POMODORO: "pomodoro"
});

function isInteractiveMode(mode) {
    return mode >= MODES.COUNTER;
}

let config = makeDefaultConfig();
let curDDayIdx = -1;
let fwUploadBusy = false;

window.onload = async () => {
    try {
        const res = await fetch("/get-config");
        const data = await res.json();
        config = normalizeConfig(data);
    } catch (e) {
        config = makeDefaultConfig();
    }
    renderUI();
    initLogWS();
    renderFirmwareProgress(0);
    refreshFirmwareInfo();
};

function initLogWS() {
    const protocol = window.location.protocol === "https:" ? "wss:" : "ws:";
    const wsUrl = `${protocol}//${window.location.host}/ws/log`;
    const ws = new WebSocket(wsUrl);
    const logContainer = document.getElementById("logContainer");

    ws.onmessage = (event) => {
        const line = document.createElement("div");
        line.className = "log-line";
        line.innerText = `[${new Date().toLocaleTimeString()}] ${event.data}`;
        logContainer.appendChild(line);
        logContainer.scrollTop = logContainer.scrollHeight;

        while (logContainer.childNodes.length > 100) {
            logContainer.removeChild(logContainer.firstChild);
        }
    };

    ws.onclose = () => {
        setTimeout(initLogWS, 2000);
    };
}

function clearLog() {
    document.getElementById("logContainer").innerHTML = "";
}

function setFirmwareUploadBusy(isBusy) {
    fwUploadBusy = isBusy;
    const btn = document.getElementById("fwUploadBtn");
    const fwInput = document.getElementById("fwFile");
    const fsInput = document.getElementById("fsFile");
    if (btn) btn.disabled = isBusy;
    if (fwInput) fwInput.disabled = isBusy;
    if (fsInput) fsInput.disabled = isBusy;
}

function setFirmwareStatus(message, state = "idle") {
    const statusEl = document.getElementById("fwUploadStatus");
    if (!statusEl) return;
    statusEl.innerText = message;
    statusEl.classList.remove("ok", "error", "running");
    if (state === "ok") statusEl.classList.add("ok");
    if (state === "error") statusEl.classList.add("error");
    if (state === "running") statusEl.classList.add("running");
}

function renderFirmwareProgress(percent) {
    const safe = Math.max(0, Math.min(100, Math.round(Number(percent) || 0)));
    const bar = document.getElementById("fwProgressFill");
    const text = document.getElementById("fwProgressText");
    if (bar) bar.style.width = `${safe}%`;
    if (text) text.innerText = `${safe}%`;
}

async function refreshFirmwareInfo() {
    const infoEl = document.getElementById("fwInfoText");
    if (!infoEl) return;

    try {
        const res = await fetch("/fw-info");
        const data = await res.json();
        if (!res.ok) throw new Error("fw_info_request_failed");

        const model = data.chipModel || "unknown";
        const rev = toInt(data.chipRev, 0);
        const free = toInt(data.freeSketchSpace, 0);
        const freeKb = Math.round(free / 1024);
        const busy = !!data.uploadInProgress ? "busy" : "idle";
        const fsSupport = data.fsUploadSupported === false ? "fs ota off" : "fs ota on";
        const version = data.version ? `, ver ${data.version}` : "";
        infoEl.innerText = `${model} rev${rev}, free ${freeKb}KB, ${busy}, ${fsSupport}${version}`;
    } catch (e) {
        infoEl.innerText = "Device info unavailable";
    }
}

function isBinFile(file) {
    return !!file && /\.bin$/i.test(file.name || "");
}

function uploadOtaPart(endpoint, fieldName, file, progressStart, progressEnd) {
    return new Promise((resolve, reject) => {
        const formData = new FormData();
        formData.append(fieldName, file, file.name);

        const xhr = new XMLHttpRequest();
        xhr.open("POST", endpoint, true);

        xhr.upload.onprogress = (event) => {
            if (!event.lengthComputable) return;
            const ratio = event.total > 0 ? (event.loaded / event.total) : 0;
            const percent = progressStart + ((progressEnd - progressStart) * ratio);
            renderFirmwareProgress(percent);
        };

        xhr.onload = () => {
            let body = {};
            try {
                body = JSON.parse(xhr.responseText || "{}");
            } catch (e) {
                body = {};
            }

            if (xhr.status >= 200 && xhr.status < 300 && body.status === "ok") {
                resolve(body);
                return;
            }

            const err = new Error(body.reason || `http_${xhr.status}`);
            err.reason = body.reason || `http_${xhr.status}`;
            err.detail = body.detail || "";
            reject(err);
        };

        xhr.onerror = () => {
            const err = new Error("network_error");
            err.reason = "network_error";
            err.detail = "connection lost during upload";
            reject(err);
        };

        xhr.onabort = () => {
            const err = new Error("aborted");
            err.reason = "aborted";
            err.detail = "upload aborted";
            reject(err);
        };

        xhr.send(formData);
    });
}

async function uploadFirmwareBin() {
    if (fwUploadBusy) return;

    const fwInput = document.getElementById("fwFile");
    const fsInput = document.getElementById("fsFile");

    if (!fwInput || !fwInput.files || fwInput.files.length === 0) {
        setFirmwareStatus("Select firmware .bin first.", "error");
        return;
    }

    const firmwareFile = fwInput.files[0];
    if (!isBinFile(firmwareFile)) {
        setFirmwareStatus("Firmware must be a .bin file.", "error");
        return;
    }

    let filesystemFile = null;
    if (fsInput && fsInput.files && fsInput.files.length > 0) {
        filesystemFile = fsInput.files[0];
        if (!isBinFile(filesystemFile)) {
            setFirmwareStatus("Filesystem image must be a .bin file.", "error");
            return;
        }
    }

    setFirmwareUploadBusy(true);
    renderFirmwareProgress(0);

    try {
        if (filesystemFile) {
            setFirmwareStatus("1/2 Uploading filesystem...", "running");
            await uploadOtaPart("/fs-upload", "filesystem", filesystemFile, 0, 50);
            renderFirmwareProgress(50);
        }

        const fwStart = filesystemFile ? 50 : 0;
        const fwPhaseText = filesystemFile ? "2/2 Uploading firmware..." : "Uploading firmware...";
        setFirmwareStatus(fwPhaseText, "running");
        const fwResult = await uploadOtaPart("/fw-upload", "firmware", firmwareFile, fwStart, 100);

        renderFirmwareProgress(100);
        if (fwResult.rebooting) {
            setFirmwareStatus("Update complete. Rebooting device...", "ok");
        } else {
            setFirmwareStatus("Update complete.", "ok");
        }
        setTimeout(() => refreshFirmwareInfo(), 2500);
    } catch (e) {
        const reason = e?.reason || e?.message || "upload_failed";
        const detail = e?.detail ? ` (${e.detail})` : "";
        setFirmwareStatus(`Upload failed: ${reason}${detail}`, "error");
        refreshFirmwareInfo();
    } finally {
        setFirmwareUploadBusy(false);
    }
}

function makeDefaultRing(fill1, fill2) {
    return {
        mode: MODES.YEAR,
        colorMode: 0,
        colorFill: fill1,
        colorFill2: fill2,
        colorEmpty: 0,
        payload: { kind: PAYLOAD.NONE }
    };
}

function makeDefaultPreset() {
    return {
        inner: makeDefaultRing(0xFF0000, 0x00FF00),
        outer: makeDefaultRing(0x0000FF, 0xFFFF00),
        segment: {
            mode: 1,
            payload: { kind: PAYLOAD.NONE }
        }
    };
}

function makeDefaultConfig() {
    return {
        version: 2,
        curIdx: 0,
        presets: [makeDefaultPreset()],
        ddays: [{ n: "새해", s: "2025-01-01", t: "2026-01-01" }],
        bri: 50,
        nEn: false,
        nS: 22,
        nE: 7,
        nB: 10
    };
}

function normalizeConfig(raw) {
    const base = makeDefaultConfig();
    if (!raw || typeof raw !== "object") return base;
    if (toInt(raw.version, 0) !== 2) return base;

    const presetsRaw = Array.isArray(raw.presets) ? raw.presets : [];
    const ddaysRaw = Array.isArray(raw.ddays) ? raw.ddays : [];

    const normalized = {
        version: 2,
        curIdx: toInt(raw.curIdx, 0),
        presets: presetsRaw.map((p) => normalizePreset(p)),
        ddays: ddaysRaw.map((d) => ({
            n: String(d?.n ?? ""),
            s: String(d?.s ?? ""),
            t: String(d?.t ?? "")
        })),
        bri: toInt(raw.bri, 50),
        nEn: !!raw.nEn,
        nS: toInt(raw.nS, 22),
        nE: toInt(raw.nE, 7),
        nB: toInt(raw.nB, 10)
    };

    if (normalized.presets.length === 0) normalized.presets = [makeDefaultPreset()];
    if (normalized.ddays.length === 0) normalized.ddays = [{ n: "새 일정", s: "2025-01-01", t: "2025-12-31" }];
    if (normalized.curIdx < 0 || normalized.curIdx >= normalized.presets.length) normalized.curIdx = 0;

    clampDDayIndices(normalized);
    return normalized;
}

function normalizePreset(raw) {
    const preset = makeDefaultPreset();
    if (!raw || typeof raw !== "object") return preset;

    if (!raw.inner || !raw.outer || !raw.segment) {
        return preset;
    }

    preset.inner = normalizeRing(raw.inner, preset.inner);
    preset.outer = normalizeRing(raw.outer, preset.outer);
    preset.segment = normalizeSegment(raw.segment, preset.segment);
    enforcePresetRules(preset, "");
    return preset;
}

function normalizeRing(rawRing, fallback) {
    const ring = {
        mode: toInt(rawRing?.mode, fallback.mode),
        colorMode: toInt(rawRing?.colorMode, fallback.colorMode),
        colorFill: toInt(rawRing?.colorFill, fallback.colorFill),
        colorFill2: toInt(rawRing?.colorFill2, fallback.colorFill2),
        colorEmpty: toInt(rawRing?.colorEmpty, fallback.colorEmpty),
        payload: normalizePayload(rawRing?.payload, PAYLOAD.NONE)
    };

    ring.payload = normalizePayloadForMode(ring.mode, ring.payload);
    return ring;
}

function normalizeSegment(rawSegment, fallback) {
    const segment = {
        mode: toInt(rawSegment?.mode, fallback.mode),
        payload: normalizePayload(rawSegment?.payload, PAYLOAD.NONE)
    };

    if (segment.mode === 5) {
        segment.payload = normalizePayload(segment.payload, PAYLOAD.DDAY);
        segment.payload.ddayIndex = toInt(segment.payload.ddayIndex, 0);
    } else {
        segment.payload = { kind: PAYLOAD.NONE };
    }

    return segment;
}

function normalizePayload(payload, fallbackKind) {
    const kind = String(payload?.kind ?? fallbackKind ?? PAYLOAD.NONE);
    const out = { kind };
    if (kind === PAYLOAD.DDAY) out.ddayIndex = toInt(payload?.ddayIndex, 0);
    if (kind === PAYLOAD.COUNTER) out.counterTarget = toInt(payload?.counterTarget, 0);
    if (kind === PAYLOAD.TIMER) {
        out.timerSeconds = toInt(payload?.timerSeconds, 0);
        out.displaySeconds = !!payload?.displaySeconds;
    }
    if (kind === PAYLOAD.POMODORO) {
        out.workMinutes = toInt(payload?.workMinutes, 0);
        out.restMinutes = toInt(payload?.restMinutes, 0);
        out.displaySeconds = !!payload?.displaySeconds;
    }
    return out;
}

function normalizePayloadForMode(mode, payload) {
    if (mode === MODES.DDAY) return { kind: PAYLOAD.DDAY, ddayIndex: toInt(payload?.ddayIndex, 0) };
    if (mode === MODES.COUNTER) return { kind: PAYLOAD.COUNTER, counterTarget: toInt(payload?.counterTarget, 0) };
    if (mode === MODES.TIMER) return {
        kind: PAYLOAD.TIMER,
        timerSeconds: toInt(payload?.timerSeconds, 0),
        displaySeconds: !!payload?.displaySeconds
    };
    if (mode === MODES.POMODORO) {
        return {
            kind: PAYLOAD.POMODORO,
            workMinutes: toInt(payload?.workMinutes, 0),
            restMinutes: toInt(payload?.restMinutes, 0),
            displaySeconds: !!payload?.displaySeconds
        };
    }
    return { kind: PAYLOAD.NONE };
}

function setTab(idx) {
    document.querySelectorAll(".tab").forEach((t, i) => t.classList.toggle("active", i === idx));
    document.querySelectorAll(".content").forEach((c, i) => c.classList.toggle("active", i === idx));
}

function renderUI() {
    clampDDayIndices(config);

    const pTabs = document.getElementById("presetTabs");
    pTabs.innerHTML = "";
    config.presets.forEach((p, i) => {
        const pill = document.createElement("div");
        pill.className = `preset-pill ${i === config.curIdx ? "active" : ""}`;
        pill.innerText = i;
        pill.onclick = () => {
            config.curIdx = i;
            renderUI();
        };
        pTabs.appendChild(pill);
    });

    const addBtn = document.createElement("div");
    addBtn.className = "preset-pill add-btn";
    addBtn.innerText = "+ 추가";
    addBtn.onclick = addPreset;
    pTabs.appendChild(addBtn);

    loadPresetUI();

    const dList = document.getElementById("ddayListContainer");
    dList.innerHTML = "";
    config.ddays.forEach((d, i) => {
        const div = document.createElement("div");
        div.className = "list-item";
        div.innerHTML = `<span>${d.n}</span> <small>${d.s} ~ ${d.t}</small>`;
        div.onclick = () => {
            openDDayEdit(i);
            renderUI();
        };
        dList.appendChild(div);
    });

    document.getElementById("bri").value = config.bri;
    document.getElementById("briVal").innerText = config.bri;
    document.getElementById("nEn").checked = config.nEn;
    document.getElementById("nS").value = config.nS;
    document.getElementById("nE").value = config.nE;
    document.getElementById("nB").value = config.nB;
    document.getElementById("nBVal").innerText = config.nB;
    toggleNightBox();
}

function toggleNightBox() {
    document.getElementById("nightBox").style.display = document.getElementById("nEn").checked ? "block" : "none";
}

function loadPresetUI() {
    if (config.presets.length === 0) return;
    const p = config.presets[config.curIdx];

    document.getElementById("im").value = p.inner.mode;
    document.getElementById("icm").value = p.inner.colorMode;
    document.getElementById("icf").value = intToHex(p.inner.colorFill);
    document.getElementById("icf2").value = intToHex(p.inner.colorFill2);
    document.getElementById("ice").value = intToHex(p.inner.colorEmpty);
    fillDDaySelect("id_dd", getDDayIndex(p.inner.payload));

    document.getElementById("om").value = p.outer.mode;
    document.getElementById("ocm").value = p.outer.colorMode;
    document.getElementById("ocf").value = intToHex(p.outer.colorFill);
    document.getElementById("ocf2").value = intToHex(p.outer.colorFill2);
    document.getElementById("oce").value = intToHex(p.outer.colorEmpty);
    fillDDaySelect("od", getDDayIndex(p.outer.payload));

    document.getElementById("sm").value = p.segment.mode;
    fillDDaySelect("sd", getDDayIndex(p.segment.payload));

    setSpecialInputsFromPayload("im", p.inner.payload);
    setSpecialInputsFromPayload("om", p.outer.payload);
    updateDynamicUI();
}

function setSpecialInputsFromPayload(prefix, payload) {
    const sv = document.getElementById(`${prefix}_sv`);
    const sv2 = document.getElementById(`${prefix}_sv2`);
    if (payload.kind === PAYLOAD.COUNTER) {
        sv.value = toInt(payload.counterTarget, 0);
        sv2.value = 0;
        setUnitDisplayMode(prefix, false);
    } else if (payload.kind === PAYLOAD.TIMER) {
        sv.value = toInt(payload.timerSeconds, 0);
        sv2.value = 0;
        setUnitDisplayMode(prefix, !!payload.displaySeconds);
    } else if (payload.kind === PAYLOAD.POMODORO) {
        sv.value = toInt(payload.workMinutes, 0);
        sv2.value = toInt(payload.restMinutes, 0);
        setUnitDisplayMode(prefix, !!payload.displaySeconds);
    } else {
        sv.value = 0;
        sv2.value = 0;
        setUnitDisplayMode(prefix, false);
    }
}

function setUnitDisplayMode(prefix, displaySeconds) {
    const secRadio = document.getElementById(`${prefix}_unit_sec`);
    const minRadio = document.getElementById(`${prefix}_unit_min`);
    if (!secRadio || !minRadio) return;
    secRadio.checked = !!displaySeconds;
    minRadio.checked = !displaySeconds;
}

function getUnitDisplaySeconds(prefix) {
    const secRadio = document.getElementById(`${prefix}_unit_sec`);
    if (!secRadio) return false;
    return !!secRadio.checked;
}

function updateDynamicUI() {
    const p = config.presets[config.curIdx];
    const imVal = toInt(document.getElementById("im").value, 0);
    const omVal = toInt(document.getElementById("om").value, 0);
    const smVal = toInt(document.getElementById("sm").value, 1);

    document.getElementById("im_dd_box").style.display = (imVal === MODES.DDAY) ? "block" : "none";
    document.getElementById("om_dd_box").style.display = (omVal === MODES.DDAY) ? "block" : "none";
    document.getElementById("sm_dd_box").style.display = (smVal === 5) ? "block" : "none";

    setSpecialModeUI("im", imVal);
    setSpecialModeUI("om", omVal);

    updateColorModeUI("i");
    updateColorModeUI("o");
    syncRingInteractiveOptions(imVal, omVal);
    syncSegmentInteractiveOptions(p, smVal);
}

function setSpecialModeUI(prefix, mode) {
    const box = document.getElementById(`${prefix}_sv_box`);
    const label = document.getElementById(`${prefix}_sv_label`);
    const sv2Wrap = document.getElementById(`${prefix}_sv2_container`);
    const secWrap = document.getElementById(`${prefix}_sec_container`);

    if (mode >= MODES.COUNTER) {
        box.style.display = "block";
        if (mode === MODES.COUNTER) label.innerText = "카운터 목표값";
        else if (mode === MODES.TIMER) label.innerText = "타이머 시간 (초)";
        else if (mode === MODES.POMODORO) label.innerText = "집중 시간 (분)";
        else label.innerText = "설정값";
        sv2Wrap.style.display = (mode === MODES.POMODORO) ? "block" : "none";
        secWrap.style.display = (mode === MODES.TIMER || mode === MODES.POMODORO) ? "flex" : "none";
    } else {
        box.style.display = "none";
        secWrap.style.display = "none";
    }
}

function updateColorModeUI(prefix) {
    const cm = document.getElementById(`${prefix}cm`).value;
    const cfBox = document.getElementById(`${prefix}cf_box`);
    const cfLabel = document.getElementById(`${prefix}cf_label`);
    const cf2Box = document.getElementById(`${prefix}cf2_box`);

    if (cm === "1") {
        cfBox.style.display = "none";
        cf2Box.style.display = "none";
    } else if (cm === "0") {
        cfBox.style.display = "flex";
        cfLabel.innerText = "채움 색";
        cf2Box.style.display = "none";
    } else {
        cfBox.style.display = "flex";
        cfLabel.innerText = "시작 색";
        cf2Box.style.display = "flex";
    }
}

function updatePresetData(source = "") {
    const p = config.presets[config.curIdx];
    p.inner = readRingFromUI("i", "id_dd", "im");
    p.outer = readRingFromUI("o", "od", "om");
    p.segment = readSegmentFromUI();
    enforcePresetRules(p, source);
    loadPresetUI();
}

function readRingFromUI(prefix, ddaySelectId, modeSelectId) {
    const mode = toInt(document.getElementById(modeSelectId).value, 0);
    const ring = {
        mode,
        colorMode: toInt(document.getElementById(`${prefix}cm`).value, 0),
        colorFill: hexToInt(document.getElementById(`${prefix}cf`).value),
        colorFill2: hexToInt(document.getElementById(`${prefix}cf2`).value),
        colorEmpty: hexToInt(document.getElementById(`${prefix}ce`).value),
        payload: { kind: PAYLOAD.NONE }
    };

    if (mode === MODES.DDAY) {
        ring.payload = {
            kind: PAYLOAD.DDAY,
            ddayIndex: toInt(document.getElementById(ddaySelectId).value, 0)
        };
    } else if (mode === MODES.COUNTER) {
        ring.payload = {
            kind: PAYLOAD.COUNTER,
            counterTarget: toInt(document.getElementById(`${modeSelectId}_sv`).value, 0)
        };
    } else if (mode === MODES.TIMER) {
        ring.payload = {
            kind: PAYLOAD.TIMER,
            timerSeconds: toInt(document.getElementById(`${modeSelectId}_sv`).value, 0),
            displaySeconds: getUnitDisplaySeconds(modeSelectId)
        };
    } else if (mode === MODES.POMODORO) {
        ring.payload = {
            kind: PAYLOAD.POMODORO,
            workMinutes: toInt(document.getElementById(`${modeSelectId}_sv`).value, 0),
            restMinutes: toInt(document.getElementById(`${modeSelectId}_sv2`).value, 0),
            displaySeconds: getUnitDisplaySeconds(modeSelectId)
        };
    }

    return ring;
}

function readSegmentFromUI() {
    const mode = toInt(document.getElementById("sm").value, 1);
    if (mode === 5) {
        return {
            mode,
            payload: {
                kind: PAYLOAD.DDAY,
                ddayIndex: toInt(document.getElementById("sd").value, 0)
            }
        };
    }
    return { mode, payload: { kind: PAYLOAD.NONE } };
}

function getActiveInteractiveMode(preset) {
    if (isInteractiveMode(preset.inner.mode)) return preset.inner.mode;
    if (isInteractiveMode(preset.outer.mode)) return preset.outer.mode;
    return null;
}

function resetRingToNonInteractive(ring) {
    ring.mode = MODES.YEAR;
    ring.payload = { kind: PAYLOAD.NONE };
}

function enforcePresetRules(preset, source) {
    const innerInteractive = isInteractiveMode(preset.inner.mode);
    const outerInteractive = isInteractiveMode(preset.outer.mode);

    // Rule 1: inner/outer 동시에 interactive 선택 금지
    if (innerInteractive && outerInteractive) {
        if (source === "om") resetRingToNonInteractive(preset.inner);
        else resetRingToNonInteractive(preset.outer);
    }

    const activeInteractiveMode = getActiveInteractiveMode(preset);

    // Rule 2: 7세그 interactive 선택은 실제 active interactive mode와 동일할 때만 허용
    if (isInteractiveMode(preset.segment.mode) && preset.segment.mode !== activeInteractiveMode) {
        preset.segment.mode = 0;
        preset.segment.payload = { kind: PAYLOAD.NONE };
    }
}

function syncRingInteractiveOptions(imVal, omVal) {
    const imSelect = document.getElementById("im");
    const omSelect = document.getElementById("om");
    const interactiveModes = [MODES.COUNTER, MODES.TIMER, MODES.POMODORO];

    for (const mode of interactiveModes) {
        const imOpt = imSelect.querySelector(`option[value="${mode}"]`);
        const omOpt = omSelect.querySelector(`option[value="${mode}"]`);
        if (imOpt) imOpt.disabled = isInteractiveMode(omVal) && imVal !== mode;
        if (omOpt) omOpt.disabled = isInteractiveMode(imVal) && omVal !== mode;
    }
}

function syncSegmentInteractiveOptions(preset, currentSegMode) {
    const activeInteractiveMode = getActiveInteractiveMode(preset);
    const smSelect = document.getElementById("sm");
    const interactiveModes = [MODES.COUNTER, MODES.TIMER, MODES.POMODORO];

    for (const mode of interactiveModes) {
        const opt = smSelect.querySelector(`option[value="${mode}"]`);
        if (opt) {
            opt.disabled = activeInteractiveMode !== mode;
        }
    }

    if (isInteractiveMode(currentSegMode) && currentSegMode !== activeInteractiveMode) {
        smSelect.value = "0";
        preset.segment.mode = 0;
        preset.segment.payload = { kind: PAYLOAD.NONE };
    }
}

function fillDDaySelect(elId, selectedVal) {
    const sel = document.getElementById(elId);
    sel.innerHTML = "";

    if (config.ddays.length === 0) {
        const opt = document.createElement("option");
        opt.value = 0;
        opt.text = "(디데이 없음)";
        sel.add(opt);
        sel.value = "0";
        return;
    }

    config.ddays.forEach((d, i) => {
        const opt = document.createElement("option");
        opt.value = i;
        opt.text = d.n;
        sel.add(opt);
    });

    const safeIndex = Math.max(0, Math.min(toInt(selectedVal, 0), config.ddays.length - 1));
    sel.value = String(safeIndex);
}

function addPreset() {
    config.presets.push(makeDefaultPreset());
    config.curIdx = config.presets.length - 1;
    renderUI();
}

function movePreset(dir) {
    const newIdx = config.curIdx + dir;
    if (newIdx < 0 || newIdx >= config.presets.length) return;
    const temp = config.presets[config.curIdx];
    config.presets[config.curIdx] = config.presets[newIdx];
    config.presets[newIdx] = temp;
    config.curIdx = newIdx;
    renderUI();
}

function delPreset() {
    if (config.presets.length <= 1) {
        alert("최소 1개는 있어야 합니다.");
        return;
    }
    if (!confirm("삭제?")) return;
    config.presets.splice(config.curIdx, 1);
    config.curIdx = 0;
    renderUI();
}

function openDDayEdit(idx) {
    curDDayIdx = idx;
    const d = config.ddays[idx];
    document.getElementById("dName").value = d.n;
    document.getElementById("dStart").value = d.s;
    document.getElementById("dTarget").value = d.t;
    document.getElementById("ddayEditor").style.display = "block";
}

function saveDDay() {
    if (curDDayIdx < 0) return;
    const d = config.ddays[curDDayIdx];
    d.n = document.getElementById("dName").value;
    d.s = document.getElementById("dStart").value;
    d.t = document.getElementById("dTarget").value;
    document.getElementById("ddayEditor").style.display = "none";
    renderUI();
}

function addDDay() {
    config.ddays.push({ n: "새 일정", s: "2025-01-01", t: "2025-12-31" });
    openDDayEdit(config.ddays.length - 1);
}

function delDDay() {
    if (curDDayIdx < 0) return;
    if (!confirm("삭제?")) return;
    config.ddays.splice(curDDayIdx, 1);
    curDDayIdx = -1;
    document.getElementById("ddayEditor").style.display = "none";
    if (config.ddays.length === 0) {
        config.ddays.push({ n: "새 일정", s: "2025-01-01", t: "2025-12-31" });
    }
    clampDDayIndices(config);
    renderUI();
}

function clampDDayIndices(cfg) {
    const maxIdx = Math.max(0, cfg.ddays.length - 1);
    cfg.presets.forEach((p) => {
        clampPayloadDDay(p.inner.payload, maxIdx);
        clampPayloadDDay(p.outer.payload, maxIdx);
        clampPayloadDDay(p.segment.payload, maxIdx);
    });
}

function clampPayloadDDay(payload, maxIdx) {
    if (payload?.kind !== PAYLOAD.DDAY) return;
    payload.ddayIndex = Math.max(0, Math.min(toInt(payload.ddayIndex, 0), maxIdx));
}

async function uploadConfig() {
    const btn = document.getElementById("saveBtn");
    const loader = document.getElementById("saveLoader");
    const txt = document.getElementById("saveText");

    btn.disabled = true;
    loader.style.display = "inline-block";
    txt.innerText = "저장 중...";

    updatePresetData();
    config.bri = toInt(document.getElementById("bri").value, 50);
    config.nEn = document.getElementById("nEn").checked;
    config.nS = toInt(document.getElementById("nS").value, 22);
    config.nE = toInt(document.getElementById("nE").value, 7);
    config.nB = toInt(document.getElementById("nB").value, 10);

    try {
        const res = await fetch("/set-config", {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify(config)
        });
        const data = await res.json();
        if (!res.ok || data.status !== "ok") {
            throw new Error(data.reason || "unknown");
        }
        txt.innerText = "저장 완료!";
    } catch (e) {
        alert(`실패: ${e.message}`);
        txt.innerText = "저장 실패";
    } finally {
        loader.style.display = "none";
        setTimeout(() => {
            btn.disabled = false;
            txt.innerText = "설정 저장하기";
        }, 1000);
    }
}

function getDDayIndex(payload) {
    if (payload?.kind === PAYLOAD.DDAY) return toInt(payload.ddayIndex, 0);
    return 0;
}

function toInt(value, fallback = 0) {
    const parsed = parseInt(value, 10);
    return Number.isFinite(parsed) ? parsed : fallback;
}

function intToHex(num) {
    const safe = Number.isFinite(num) ? num : 0;
    let hex = safe.toString(16);
    while (hex.length < 6) hex = "0" + hex;
    return "#" + hex.slice(-6);
}

function hexToInt(hex) {
    const value = parseInt(String(hex || "#000000").replace("#", ""), 16);
    return Number.isFinite(value) ? value : 0;
}

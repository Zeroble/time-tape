let config = { curIdx: 0, presets: [], ddays: [], bri: 50, nEn: false, nS: 22, nE: 7, nB: 10 };
let curDDayIdx = -1;

window.onload = async () => {
    try {
        let res = await fetch('/get-config');
        let data = await res.json();
        if (data.presets) config = data; else initDefault();
    } catch (e) { initDefault(); }
    renderUI();
    initLogWS();
};

function initLogWS() {
    const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
    const wsUrl = `${protocol}//${window.location.host}/ws/log`;
    const ws = new WebSocket(wsUrl);
    const logContainer = document.getElementById('logContainer');

    ws.onmessage = (event) => {
        const line = document.createElement('div');
        line.className = 'log-line';
        line.innerText = `[${new Date().toLocaleTimeString()}] ${event.data}`;
        logContainer.appendChild(line);
        logContainer.scrollTop = logContainer.scrollHeight;

        // 로그가 너무 많아지면 오래된 것 삭제 (최근 100개 유지)
        while (logContainer.childNodes.length > 100) {
            logContainer.removeChild(logContainer.firstChild);
        }
    };

    ws.onclose = () => {
        setTimeout(initLogWS, 2000); // 2초 후 재연결 시도
    };
}

function clearLog() {
    document.getElementById('logContainer').innerHTML = '';
}

function initDefault() {
    config.presets = [{
        im: 0, id_dd: 0, icm: 0, icf: 0xFF0000, icf2: 0x00FF00, ice: 0,
        om: 0, od: 0, ocm: 0, ocf: 0x0000FF, ocf2: 0xFFFF00, oce: 0,
        sm: 1, sd: 0, sv: 0, sv2: 0
    }];
    config.ddays = [{ n: "새해", s: "2025-01-01", t: "2026-01-01" }];
}

function setTab(idx) {
    document.querySelectorAll('.tab').forEach((t, i) => t.classList.toggle('active', i === idx));
    document.querySelectorAll('.content').forEach((c, i) => c.classList.toggle('active', i === idx));
}

function renderUI() {
    const pTabs = document.getElementById('presetTabs');
    pTabs.innerHTML = '';
    config.presets.forEach((p, i) => {
        let pill = document.createElement('div');
        pill.className = `preset-pill ${i === config.curIdx ? 'active' : ''}`;
        pill.innerText = i; // 숫자만 표시
        pill.onclick = () => { config.curIdx = i; renderUI(); };
        pTabs.appendChild(pill);
    });
    let addBtn = document.createElement('div');
    addBtn.className = 'preset-pill add-btn';
    addBtn.innerText = '+ 추가';
    addBtn.onclick = addPreset;
    pTabs.appendChild(addBtn);

    loadPresetUI();

    const dList = document.getElementById('ddayListContainer');
    dList.innerHTML = '';
    config.ddays.forEach((d, i) => {
        let div = document.createElement('div');
        div.className = 'list-item';
        div.innerHTML = `<span>${d.n}</span> <small>${d.s} ~ ${d.t}</small>`;
        div.onclick = () => { openDDayEdit(i); renderUI(); }; // openDDayEdit sets curDDayIdx
        dList.appendChild(div);
    });

    document.getElementById('bri').value = config.bri;
    document.getElementById('briVal').innerText = config.bri;
    document.getElementById('nEn').checked = config.nEn;
    document.getElementById('nS').value = config.nS;
    document.getElementById('nE').value = config.nE;
    document.getElementById('nB').value = config.nB;
    document.getElementById('nBVal').innerText = config.nB;
    toggleNightBox();
}

function toggleNightBox() {
    document.getElementById('nightBox').style.display = document.getElementById('nEn').checked ? 'block' : 'none';
}

function loadPresetUI() {
    if (config.presets.length === 0) return;
    const p = config.presets[config.curIdx];

    document.getElementById('im').value = p.im;
    document.getElementById('icm').value = p.icm || 0;
    document.getElementById('icf').value = intToHex(p.icf);
    document.getElementById('icf2').value = intToHex(p.icf2 || 0x00FF00);
    document.getElementById('ice').value = intToHex(p.ice);
    fillDDaySelect('id_dd', p.id_dd);

    document.getElementById('om').value = p.om;
    document.getElementById('ocm').value = p.ocm || 0;
    document.getElementById('ocf').value = intToHex(p.ocf);
    document.getElementById('ocf2').value = intToHex(p.ocf2 || 0xFFFF00);
    document.getElementById('oce').value = intToHex(p.oce);
    fillDDaySelect('od', p.od);

    let smVal = p.sm; if (smVal === 0) smVal = 1;
    document.getElementById('sm').value = smVal;
    fillDDaySelect('sd', p.sd);
    
    // Sync both inputs to same value
    let sv = p.sv || 0;
    document.getElementById('im_sv').value = sv;
    document.getElementById('om_sv').value = sv;

    let sv2 = p.sv2 || 0;
    document.getElementById('im_sv2').value = sv2;
    document.getElementById('om_sv2').value = sv2;

    updateDynamicUI();
}

function updateDynamicUI() {
    let imVal = parseInt(document.getElementById('im').value);
    let omVal = parseInt(document.getElementById('om').value);
    let smVal = parseInt(document.getElementById('sm').value);
    
    document.getElementById('im_dd_box').style.display = (imVal == 4) ? 'block' : 'none';
    document.getElementById('om_dd_box').style.display = (omVal == 4) ? 'block' : 'none';
    document.getElementById('sm_dd_box').style.display = (smVal == 5) ? 'block' : 'none';

    // Helper to get label
    const getLabel = (mode) => {
        if (mode == 10) return "카운터 목표값";
        if (mode == 11) return "타이머 시간 (초)";
        if (mode == 12) return "집중 시간 (분)";
        return "설정값";
    };

    // Inner Ring Special Value Box
    if (imVal >= 10) {
        document.getElementById('im_sv_box').style.display = 'block';
        document.getElementById('im_sv_label').innerText = getLabel(imVal);
        document.getElementById('im_sv2_container').style.display = (imVal == 12) ? 'block' : 'none';
    } else {
        document.getElementById('im_sv_box').style.display = 'none';
    }

    // Outer Ring Special Value Box
    if (omVal >= 10) {
        document.getElementById('om_sv_box').style.display = 'block';
        document.getElementById('om_sv_label').innerText = getLabel(omVal);
        document.getElementById('om_sv2_container').style.display = (omVal == 12) ? 'block' : 'none';
    } else {
        document.getElementById('om_sv_box').style.display = 'none';
    }

    let im = document.getElementById('icm').value;
    if (im == 1) { 
        document.getElementById('icf_box').style.display = 'none';
        document.getElementById('icf2_box').style.display = 'none';
    } else if (im == 0) { 
        document.getElementById('icf_box').style.display = 'flex';
        document.getElementById('icf_label').innerText = '채움 색';
        document.getElementById('icf2_box').style.display = 'none';
    } else { 
        document.getElementById('icf_box').style.display = 'flex';
        document.getElementById('icf_label').innerText = '시작 색';
        document.getElementById('icf2_box').style.display = 'flex';
    }

    let om = document.getElementById('ocm').value;
    if (om == 1) { 
        document.getElementById('ocf_box').style.display = 'none';
        document.getElementById('ocf2_box').style.display = 'none';
    } else if (om == 0) { 
        document.getElementById('ocf_box').style.display = 'flex';
        document.getElementById('ocf_label').innerText = '채움 색';
        document.getElementById('ocf2_box').style.display = 'none';
    } else { 
        document.getElementById('ocf_box').style.display = 'flex';
        document.getElementById('ocf_label').innerText = '시작 색';
        document.getElementById('ocf2_box').style.display = 'flex';
    }
}

function updatePresetData(source) {
    const p = config.presets[config.curIdx];
    p.im = parseInt(document.getElementById('im').value);
    p.id_dd = parseInt(document.getElementById('id_dd').value);
    p.icm = parseInt(document.getElementById('icm').value);
    p.icf = hexToInt(document.getElementById('icf').value);
    p.icf2 = hexToInt(document.getElementById('icf2').value);
    p.ice = hexToInt(document.getElementById('ice').value);
    p.om = parseInt(document.getElementById('om').value);
    p.od = parseInt(document.getElementById('od').value);
    p.ocm = parseInt(document.getElementById('ocm').value);
    p.ocf = hexToInt(document.getElementById('ocf').value);
    p.ocf2 = hexToInt(document.getElementById('ocf2').value);
    p.oce = hexToInt(document.getElementById('oce').value);
    p.sm = parseInt(document.getElementById('sm').value);
    p.sd = parseInt(document.getElementById('sd').value);
    
    // Sync sv inputs if changed
    if (source === 'im') {
        p.sv = parseInt(document.getElementById('im_sv').value);
        document.getElementById('om_sv').value = p.sv;
        p.sv2 = parseInt(document.getElementById('im_sv2').value);
        document.getElementById('om_sv2').value = p.sv2;
    } else if (source === 'om') {
        p.sv = parseInt(document.getElementById('om_sv').value);
        document.getElementById('im_sv').value = p.sv;
        p.sv2 = parseInt(document.getElementById('om_sv2').value);
        document.getElementById('im_sv2').value = p.sv2;
    } else {
        p.sv = parseInt(document.getElementById('im_sv').value);
        p.sv2 = parseInt(document.getElementById('im_sv2').value);
    }
    
    updateDynamicUI();
}

function fillDDaySelect(elId, selectedVal) {
    const sel = document.getElementById(elId);
    sel.innerHTML = '';
    config.ddays.forEach((d, i) => {
        let opt = document.createElement('option');
        opt.value = i; opt.text = d.n;
        sel.add(opt);
    });
    sel.value = selectedVal >= config.ddays.length ? 0 : selectedVal;
}

function addPreset() {
    config.presets.push({
        im: 0, id_dd: 0, icm: 0, icf: 0xFF0000, icf2: 0x00FF00, ice: 0,
        om: 0, od: 0, ocm: 0, ocf: 0x0000FF, ocf2: 0xFFFF00, oce: 0,
        sm: 1, sd: 0, sv: 0, sv2: 0
    });
    config.curIdx = config.presets.length - 1;
    renderUI();
}

function movePreset(dir) {
    let newIdx = config.curIdx + dir;
    if (newIdx < 0 || newIdx >= config.presets.length) return;
    let temp = config.presets[config.curIdx];
    config.presets[config.curIdx] = config.presets[newIdx];
    config.presets[newIdx] = temp;
    config.curIdx = newIdx;
    renderUI();
}

function delPreset() {
    if (config.presets.length <= 1) return alert("최소 1개는 있어야 합니다.");
    if (confirm("삭제?")) { config.presets.splice(config.curIdx, 1); config.curIdx = 0; renderUI(); }
}

function openDDayEdit(idx) {
    curDDayIdx = idx; const d = config.ddays[idx];
    document.getElementById('dName').value = d.n;
    document.getElementById('dStart').value = d.s;
    document.getElementById('dTarget').value = d.t;
    document.getElementById('ddayEditor').style.display = 'block';
}

function saveDDay() {
    if (curDDayIdx < 0) return;
    const d = config.ddays[curDDayIdx];
    d.n = document.getElementById('dName').value;
    d.s = document.getElementById('dStart').value;
    d.t = document.getElementById('dTarget').value;
    document.getElementById('ddayEditor').style.display = 'none';
    renderUI();
}

function addDDay() {
    config.ddays.push({ n: "새 일정", s: "2025-01-01", t: "2025-12-31" });
    openDDayEdit(config.ddays.length - 1);
}

function delDDay() {
    if (curDDayIdx < 0) return;
    if (confirm("삭제?")) { config.ddays.splice(curDDayIdx, 1); document.getElementById('ddayEditor').style.display = 'none'; renderUI(); }
}

function uploadConfig() {
    const btn = document.getElementById('saveBtn');
    const loader = document.getElementById('saveLoader');
    const txt = document.getElementById('saveText');
    btn.disabled = true; loader.style.display = 'inline-block'; txt.innerText = '저장 중...';
    config.bri = parseInt(document.getElementById('bri').value);
    config.nEn = document.getElementById('nEn').checked;
    config.nS = parseInt(document.getElementById('nS').value);
    config.nE = parseInt(document.getElementById('nE').value);
    config.nB = parseInt(document.getElementById('nB').value);
    fetch('/set-config', {
        method: 'POST', headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(config)
    }).then(r => r.json()).then(d => {
        txt.innerText = '저장 완료!'; loader.style.display = 'none';
        setTimeout(() => { btn.disabled = false; txt.innerText = '설정 저장하기'; }, 1000);
    }).catch(e => {
        alert('실패'); btn.disabled = false; txt.innerText = '설정 저장하기'; loader.style.display = 'none';
    });
}

function intToHex(num) {
    let hex = num.toString(16);
    while (hex.length < 6) hex = "0" + hex;
    return "#" + hex;
}

function hexToInt(hex) { return parseInt(hex.replace('#', ''), 16); }

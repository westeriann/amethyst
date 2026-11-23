/* Amethyst — keyboard-driven MilkDrop visuals.
   Rendering: Butterchurn (WebGL MilkDrop engine). Import: .milk files are converted
   to Butterchurn presets in a Web Worker and cached in IndexedDB. */
(() => {
  'use strict';

  // ---------------------------------------------------------------- helpers
  const $ = (s) => document.querySelector(s);
  const clamp = (v, a, b) => Math.min(b, Math.max(a, v));
  const now = () => performance.now();
  const esc = (s) => String(s).replace(/[&<>"']/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c]));
  const errMsg = (e) => String((e && e.message) || e || 'error').split('\n')[0].slice(0, 140);
  const unwrap = (m) => (m && m.default) || m;
  const stepIn = (steps, value, dir) => {
    let i = 0, best = Infinity;
    steps.forEach((s, j) => { const d = Math.abs(s - value); if (d < best) { best = d; i = j; } });
    return steps[clamp(i + dir, 0, steps.length - 1)];
  };
  const fmtNum = (n) => (Math.round(n * 100) / 100).toString();
  const idle = () => new Promise((r) => (window.requestIdleCallback ? requestIdleCallback(() => r(), { timeout: 200 }) : setTimeout(r, 16)));

  // ---------------------------------------------------------------- settings
  const SETTINGS_KEY = 'amethyst.settings.v1';
  const DEFAULTS = {
    blendTime: 2, autoInterval: 20, auto: false, beatSync: true, shuffle: true,
    sensitivity: 1, speed: 1, quality: 2, pool: 'all', cues: Array(10).fill(null),
    last: null, clean: false, micId: null,
  };
  const settings = (() => {
    let saved = {};
    try { saved = JSON.parse(localStorage.getItem(SETTINGS_KEY)) || {}; } catch (_) { /* private mode */ }
    const s = Object.assign({}, DEFAULTS, saved);
    if (!Array.isArray(s.cues) || s.cues.length !== 10) s.cues = Array(10).fill(null);
    return s;
  })();
  let saveTimer = 0;
  function save() {
    clearTimeout(saveTimer);
    saveTimer = setTimeout(() => { try { localStorage.setItem(SETTINGS_KEY, JSON.stringify(settings)); } catch (_) { /* ignore */ } }, 250);
  }

  const BLEND_STEPS = [0, 0.25, 0.5, 1, 1.5, 2, 3, 4, 6, 8, 12];
  const INTERVAL_STEPS = [4, 6, 8, 10, 15, 20, 30, 45, 60, 90, 120, 180, 300];
  const SPEED_STEPS = [0.1, 0.25, 0.5, 0.75, 1, 1.25, 1.5, 2, 3, 4];
  const QUALITY = [
    { label: 'Low', max: 960 }, { label: 'Medium', max: 1440 },
    { label: 'High', max: 1920 }, { label: 'Ultra', max: 3840 },
  ];
  const CONV_VERSION = 2; // bump when the .milk conversion pipeline changes
  const POOLS = ['all', 'user', 'builtin'];
  const POOL_LABEL = { all: 'All', user: 'Imported', builtin: 'Built-in' };

  // ---------------------------------------------------------------- storage (IndexedDB w/ memory fallback)
  const DB = (() => {
    let dbp = null;
    const mem = { meta: new Map(), data: new Map(), textures: new Map() };
    const keyOf = { meta: 'key', data: 'key', textures: 'name' };
    const req = (r) => new Promise((res, rej) => { r.onsuccess = () => res(r.result); r.onerror = () => rej(r.error); });
    function open() {
      if (!dbp) {
        dbp = new Promise((res) => {
          try {
            const r = indexedDB.open('amethyst', 1);
            r.onupgradeneeded = () => {
              const d = r.result;
              d.createObjectStore('meta', { keyPath: 'key' });
              d.createObjectStore('data', { keyPath: 'key' });
              d.createObjectStore('textures', { keyPath: 'name' });
            };
            r.onsuccess = () => res(r.result);
            r.onerror = () => res(null);
            r.onblocked = () => res(null);
          } catch (_) { res(null); }
        });
      }
      return dbp;
    }
    return {
      async getAll(store) {
        const d = await open();
        if (!d) return [...mem[store].values()];
        return req(d.transaction(store).objectStore(store).getAll());
      },
      async get(store, key) {
        const d = await open();
        if (!d) return mem[store].get(key);
        return req(d.transaction(store).objectStore(store).get(key));
      },
      async putMany(store, items) {
        const d = await open();
        if (!d) { items.forEach((it) => mem[store].set(it[keyOf[store]], it)); return; }
        await new Promise((res, rej) => {
          const t = d.transaction(store, 'readwrite');
          const s = t.objectStore(store);
          items.forEach((it) => s.put(it));
          t.oncomplete = () => res();
          t.onerror = () => rej(t.error);
          t.onabort = () => rej(t.error);
        });
      },
      put(store, item) { return this.putMany(store, [item]); },
      async clear(store) {
        const d = await open();
        if (!d) { mem[store].clear(); return; }
        await new Promise((res, rej) => {
          const t = d.transaction(store, 'readwrite');
          t.objectStore(store).clear();
          t.oncomplete = () => res();
          t.onerror = () => rej(t.error);
        });
      },
    };
  })();

  // ---------------------------------------------------------------- .milk converter (worker, with main-thread fallback)
  let mainConverterP = null;
  function mainThreadConvert(text) {
    if (!mainConverterP) {
      mainConverterP = new Promise((res, rej) => {
        if (window.milkdropPresetConverter) return res(window.milkdropPresetConverter);
        const s = document.createElement('script');
        s.src = 'vendor/milkdrop-preset-converter.min.js';
        s.onload = () => res(window.milkdropPresetConverter);
        s.onerror = () => rej(new Error('Could not load the preset converter'));
        document.head.appendChild(s);
      });
    }
    return mainConverterP.then((c) => {
      const cv = unwrap(c);
      const fixer = window.AmethystShaderFix;
      if (!fixer) return cv.convertPreset(text);
      return cv.convertPreset(fixer.fixMilk(text)).catch(() => cv.convertPreset(text));
    });
  }

  const conv = {
    worker: null, healthy: false, fallback: false, queue: [], busy: null, seq: 0,
    start() {
      if (this.fallback) return;
      try {
        this.worker = new Worker('converter-worker.js');
        this.worker.onmessage = (e) => { this.healthy = true; this.done(e.data); };
        this.worker.onerror = (e) => { if (e.preventDefault) e.preventDefault(); this.crash(new Error(e.message || 'Converter crashed')); };
      } catch (_) {
        this.worker = null; this.fallback = true; // e.g. opened from file://
      }
    },
    convert(key, text, urgent) {
      return new Promise((resolve, reject) => {
        const job = { key, text, resolve, reject };
        if (urgent) this.queue.unshift(job); else this.queue.push(job);
        this.pump();
      });
    },
    bump(key) {
      const i = this.queue.findIndex((j) => j.key === key);
      if (i > 0) this.queue.unshift(this.queue.splice(i, 1)[0]);
    },
    pump() {
      if (this.busy || !this.queue.length) return;
      const job = (this.busy = this.queue.shift());
      job.id = ++this.seq;
      if (this.fallback || !this.worker) {
        mainThreadConvert(job.text).then((p) => this.finish(job, null, p), (err) => this.finish(job, err));
        return;
      }
      job.timer = setTimeout(() => this.crash(new Error('Conversion timed out')), 20000);
      this.worker.postMessage({ id: job.id, text: job.text });
    },
    done({ id, preset, error }) {
      const job = this.busy;
      if (!job || job.id !== id) return;
      this.finish(job, error ? new Error(error) : null, preset);
    },
    finish(job, err, preset) {
      clearTimeout(job.timer);
      this.busy = null;
      if (err) job.reject(err); else job.resolve(preset);
      this.pump();
    },
    crash(err) {
      const job = this.busy;
      if (this.worker) this.worker.terminate();
      this.worker = null;
      if (!this.healthy) {
        // Worker never worked (e.g. blocked) → convert on the main thread instead.
        this.fallback = true;
        if (job) { clearTimeout(job.timer); this.busy = null; this.queue.unshift(job); }
        this.pump();
        return;
      }
      this.start();
      if (job) this.finish(job, err);
    },
  };

  // ---------------------------------------------------------------- preset library
  const lib = { all: [], byKey: new Map() };
  const cache = new Map(); // LRU of converted user presets
  let view = [];
  let query = '';
  let current = null;
  let pending = null;
  let hist = [];
  let upcoming = null;
  let lastSwitch = now();
  let locked = false;
  let loadSeq = 0;

  function addEntry(e) {
    e.lname = e.name.toLowerCase();
    const old = lib.byKey.get(e.key);
    if (old) { Object.assign(old, e); return old; }
    lib.all.push(e);
    lib.byKey.set(e.key, e);
    return e;
  }

  function loadBuiltins() {
    const packs = [window.butterchurnPresets, window.butterchurnPresetsExtra, window.butterchurnPresetsExtra2];
    for (const pack of packs) {
      const p = unwrap(pack);
      if (!p || typeof p.getPresets !== 'function') continue;
      let map;
      try { map = p.getPresets(); } catch (_) { continue; }
      for (const name of Object.keys(map)) {
        const key = 'builtin:' + name;
        if (!lib.byKey.has(key)) addEntry({ key, name, source: 'builtin', preset: map[name] });
      }
    }
  }

  const collator = new Intl.Collator(undefined, { numeric: true, sensitivity: 'base' });
  function sortLib() {
    lib.all.sort((a, b) => (a.source === b.source ? 0 : a.source === 'user' ? -1 : 1) || collator.compare(a.name, b.name));
  }

  function rebuildView() {
    const terms = query.toLowerCase().split(/\s+/).filter(Boolean);
    view = lib.all.filter((e) => (settings.pool === 'all' || e.source === settings.pool) && terms.every((t) => e.lname.includes(t)));
    browser.sel = Math.max(0, current ? view.indexOf(current) : 0);
    renderCounts();
    renderList(true);
    refreshNow();
  }

  function remember(key, preset) {
    cache.delete(key);
    cache.set(key, preset);
    while (cache.size > 40) cache.delete(cache.keys().next().value);
  }

  async function getPreset(entry, urgent, keep = true) {
    if (entry.preset) return entry.preset;
    if (cache.has(entry.key)) { const p = cache.get(entry.key); remember(entry.key, p); return p; }
    if (entry.inflight) { if (urgent) conv.bump(entry.key); return entry.inflight; }
    entry.inflight = (async () => {
      const rec = await DB.get('data', entry.key);
      if (!rec) throw new Error('preset data missing — re-import it');
      // Re-convert cached results made by an older converter pipeline.
      let preset = rec.json && (rec.cv === CONV_VERSION || !rec.raw) ? rec.json : null;
      if (!preset) {
        preset = await conv.convert(entry.key, rec.raw, urgent);
        rec.json = preset;
        rec.cv = CONV_VERSION;
        DB.put('data', rec).catch(() => {});
      }
      if (!entry.converted) { entry.converted = true; saveMeta(entry); }
      if (keep) remember(entry.key, preset);
      return preset;
    })();
    try { return await entry.inflight; } finally { entry.inflight = null; }
  }

  function saveMeta(e) {
    if (e.source !== 'user') return;
    DB.put('meta', { key: e.key, name: e.name, source: 'user', converted: !!e.converted, broken: !!e.broken, error: e.error || null, added: e.added || 0, cv: CONV_VERSION }).catch(() => {});
  }

  function markBroken(entry, err) {
    entry.broken = true;
    entry.error = errMsg(err);
    console.warn('[Amethyst] preset unusable:', entry.name, err);
    saveMeta(entry);
    renderList();
  }

  // ---------------------------------------------------------------- navigation
  function stepFrom(dir, base) {
    if (!view.length) return null;
    let i = base ? view.indexOf(base) : -1;
    if (i === -1) i = dir > 0 ? -1 : view.length;
    for (let n = 0; n < view.length; n++) {
      i = (i + dir + view.length) % view.length;
      if (!view[i].broken) return view[i];
    }
    return null;
  }

  function pickRandom(exclude) {
    const ok = (e) => e && !e.broken && e !== exclude && e !== current;
    for (let n = 0; n < 12 && view.length; n++) {
      const e = view[(Math.random() * view.length) | 0];
      if (ok(e)) return e;
    }
    const c = view.filter(ok);
    return c.length ? c[(Math.random() * c.length) | 0] : null;
  }

  function prefetch() {
    const n = stepFrom(1, current);
    if (n && n.source === 'user') getPreset(n, false).catch(() => {});
    if (!upcoming || upcoming.broken || upcoming === current || !view.includes(upcoming)) upcoming = pickRandom();
    if (upcoming && upcoming.source === 'user') getPreset(upcoming, false).catch(() => {});
  }

  async function loadEntry(entry, opts = {}) {
    if (!entry || !engine.viz) return false;
    const seq = ++loadSeq;
    pending = entry;
    let preset;
    try {
      preset = await getPreset(entry, true);
    } catch (err) {
      if (seq === loadSeq) pending = null;
      markBroken(entry, err);
      toast(`Skipped “${entry.name}” — ${errMsg(err)}`, true);
      return false;
    }
    if (seq !== loadSeq) return false; // a newer request won
    pending = null;
    try {
      engine.viz.loadPreset(preset, opts.hard ? 0 : settings.blendTime);
    } catch (err) {
      markBroken(entry, err);
      toast(`Skipped “${entry.name}” — ${errMsg(err)}`, true);
      return false;
    }
    if (!verifyShaders(engine.viz, entry)) toast(`“${entry.name}”: shader not supported — using MilkDrop default shader`, true);
    if (current && current !== entry && opts.record !== false) {
      hist.push(current.key);
      if (hist.length > 300) hist.shift();
    }
    current = entry;
    lastSwitch = now();
    renderErr.count = 0;
    settings.last = entry.key;
    save();
    refreshNow();
    refreshCues();
    renderList();
    prefetch();
    return true;
  }

  // Butterchurn doesn't notice shaders that fail to compile (the screen just breaks),
  // so check the link status and fall back to the default warp/comp shader.
  function verifyShaders(viz, entry) {
    const r = viz && viz.renderer;
    const gl = r && r.gl;
    if (!gl) return true;
    let ok = true;
    for (const name of ['warpShader', 'compShader']) {
      const sh = r[name];
      if (!sh || !sh.shaderProgram || gl.getProgramParameter(sh.shaderProgram, gl.LINK_STATUS)) continue;
      const log = (gl.getAttachedShaders(sh.shaderProgram) || []).map((s) => gl.getShaderInfoLog(s)).filter(Boolean).join(' ');
      console.warn(`[Amethyst] ${name} failed for “${entry.name}”, using default.`, log);
      try { sh.updateShader(''); } catch (_) { /* */ }
      ok = false;
    }
    return ok;
  }

  async function go(kind, hard) {
    if (!view.length) { toast(query ? 'No presets match the search' : `No presets in “${POOL_LABEL[settings.pool]}”`, true); return; }
    for (let tries = 0; tries < 10; tries++) {
      let e;
      if (kind === 'random') {
        e = upcoming && !upcoming.broken && upcoming !== current && view.includes(upcoming) ? upcoming : pickRandom();
        upcoming = null;
      } else {
        e = stepFrom(kind === 'next' ? 1 : -1, pending || current);
      }
      if (!e) return;
      const r = await loadEntry(e, { hard });
      if (r || !e.broken) return; // success, or superseded by a newer key press
    }
  }

  async function back(hard) {
    while (hist.length) {
      const e = lib.byKey.get(hist.pop());
      if (e && !e.broken && e !== current) {
        if (await loadEntry(e, { hard, record: false })) return;
        if (!e.broken) return;
      }
    }
    go('prev', hard);
  }

  // ---------------------------------------------------------------- audio
  const player = $('#player');
  const audio = {
    ctx: null, input: null, analyser: null, freq: null,
    kind: 'none', label: 'No audio', node: null, stream: null, mediaNode: null, deviceId: null, objUrl: null,
    level: 0, bassAvg: 0, lastBeat: 0,
    ensure() {
      if (!this.ctx) {
        const AC = window.AudioContext || window.webkitAudioContext;
        this.ctx = new AC({ latencyHint: 'interactive' });
        this.input = this.ctx.createGain();
        this.input.gain.value = settings.sensitivity;
        this.analyser = this.ctx.createAnalyser();
        this.analyser.fftSize = 1024;
        this.analyser.smoothingTimeConstant = 0.3;
        this.input.connect(this.analyser);
        this.freq = new Uint8Array(this.analyser.frequencyBinCount);
      }
      if (this.ctx.state !== 'running') this.ctx.resume().catch(() => {});
    },
    stop() {
      if (this.kind === 'file') { player.pause(); try { this.mediaNode.disconnect(); } catch (_) { /* */ } }
      else if (this.node) { try { this.node.disconnect(); } catch (_) { /* */ } }
      if (this.stream) this.stream.getTracks().forEach((t) => t.stop());
      this.node = null; this.stream = null; this.kind = 'none'; this.label = 'No audio';
    },
    attachStream(stream, kind, label) {
      this.stop();
      this.stream = stream;
      this.node = this.ctx.createMediaStreamSource(stream);
      this.node.connect(this.input);
      this.kind = kind;
      this.label = label;
      const track = stream.getAudioTracks()[0];
      if (track) track.addEventListener('ended', () => {
        if (this.stream === stream) { this.stop(); refreshAudio(); toast('Audio input ended', true); }
      });
      refreshAudio();
      toast(`Audio: ${label}`);
    },
    async mic(deviceId) {
      this.ensure();
      const constraints = { echoCancellation: false, noiseSuppression: false, autoGainControl: false };
      if (deviceId) constraints.deviceId = { exact: deviceId };
      let stream;
      try {
        stream = await navigator.mediaDevices.getUserMedia({ audio: constraints });
      } catch (err) {
        if (deviceId && err.name === 'OverconstrainedError') return this.mic(null);
        throw new Error(err.name === 'NotAllowedError' ? 'Microphone permission was denied' : errMsg(err));
      }
      const track = stream.getAudioTracks()[0];
      this.deviceId = (track.getSettings && track.getSettings().deviceId) || deviceId || null;
      settings.micId = deviceId || null; save();
      this.attachStream(stream, 'mic', '🎤 ' + (track.label || 'Microphone'));
    },
    async system() {
      this.ensure();
      if (!navigator.mediaDevices.getDisplayMedia) throw new Error('This browser cannot capture system audio');
      const stream = await navigator.mediaDevices.getDisplayMedia({
        video: true,
        audio: { echoCancellation: false, noiseSuppression: false, autoGainControl: false },
        systemAudio: 'include',
      });
      if (!stream.getAudioTracks().length) {
        stream.getTracks().forEach((t) => t.stop());
        throw new Error('No audio was shared — tick “Share system audio” (or “Share tab audio”)');
      }
      stream.getVideoTracks().forEach((t) => { t.enabled = false; });
      this.attachStream(stream, 'system', '🖥 System / tab audio');
    },
    file(f) {
      this.ensure();
      this.stop();
      if (this.objUrl) URL.revokeObjectURL(this.objUrl);
      this.objUrl = URL.createObjectURL(f);
      player.src = this.objUrl;
      player.loop = true;
      if (!this.mediaNode) this.mediaNode = this.ctx.createMediaElementSource(player);
      this.mediaNode.connect(this.input);
      this.mediaNode.connect(this.ctx.destination);
      this.kind = 'file';
      this.label = '🎵 ' + f.name;
      player.play().catch((err) => toast('Playback failed: ' + errMsg(err), true));
      refreshAudio();
      toast(`Playing ${f.name}  ·  Shift+M pauses`);
    },
    none() { this.stop(); refreshAudio(); toast('Audio off'); },
    setGain(v) {
      settings.sensitivity = clamp(v, 0.1, 10);
      save();
      if (this.input) this.input.gain.setTargetAtTime(settings.sensitivity, this.ctx.currentTime, 0.02);
      refreshPills();
      toast(`Sensitivity ${fmtNum(settings.sensitivity)}×`);
    },
    analyse(t) {
      if (!this.analyser || this.kind === 'none') { this.level *= 0.85; return false; }
      this.analyser.getByteFrequencyData(this.freq);
      const f = this.freq;
      let bass = 0;
      for (let i = 1; i <= 6; i++) bass += f[i];
      bass /= 6 * 255;
      let sum = 0;
      const n = Math.min(256, f.length);
      for (let i = 0; i < n; i++) sum += f[i];
      this.level = sum / (n * 255);
      const beat = bass > 0.2 && bass > this.bassAvg * 1.25 + 0.03 && t - this.lastBeat > 300;
      this.bassAvg += (bass - this.bassAvg) * 0.06;
      if (beat) this.lastBeat = t;
      return beat;
    },
  };

  // ---------------------------------------------------------------- engine (Butterchurn) & output window
  const BC = unwrap(window.butterchurn);
  const mainCanvas = $('#viz');
  const preview = $('#preview');
  const engine = { viz: null, canvas: null, win: window, gen: 0 };
  let mainViz = null, outViz = null, outWin = null;
  let textures = {};
  const fx = { blackout: false, frozen: false };

  function dims(win) {
    const w = Math.max(1, win.innerWidth), h = Math.max(1, win.innerHeight);
    const pr = Math.max(0.25, Math.min(win.devicePixelRatio || 1, QUALITY[settings.quality].max / w));
    return { w, h, pr };
  }

  function makeViz(canvas, win) {
    const { w, h, pr } = dims(win);
    canvas.width = Math.round(w * pr);
    canvas.height = Math.round(h * pr);
    const viz = BC.createVisualizer(audio.ctx, canvas, { width: w, height: h, pixelRatio: pr, textureRatio: 1 });
    const extra = unwrap(window.butterchurnExtraImages);
    if (extra && extra.getImages) viz.loadExtraImages(extra.getImages());
    if (Object.keys(textures).length) viz.loadExtraImages(textures);
    viz.connectAudio(audio.input);
    return viz;
  }

  function resize() {
    if (!engine.viz) return;
    const { w, h, pr } = dims(engine.win);
    engine.canvas.width = Math.round(w * pr);
    engine.canvas.height = Math.round(h * pr);
    engine.viz.setRendererSize(w, h, { pixelRatio: pr });
  }

  function useViz(viz, canvas, win) {
    engine.viz = viz; engine.canvas = canvas; engine.win = win;
    resize();
    startLoop();
    if (current) loadEntry(current, { hard: true, record: false });
  }

  const renderErr = { count: 0, since: 0 };
  function onRenderError(err) {
    const t = now();
    if (t - renderErr.since > 2000) { renderErr.since = t; renderErr.count = 0; }
    if (++renderErr.count === 5 && current) {
      const bad = current;
      markBroken(bad, err);
      toast(`“${bad.name}” crashed — skipping`, true);
      go('next', true);
    }
  }

  function startLoop() {
    const gen = ++engine.gen;
    const win = engine.win;
    let last = 0;
    const frame = (t) => {
      if (gen !== engine.gen) return;
      win.requestAnimationFrame(frame);
      const dt = last ? clamp((t - last) / 1000, 0.001, 0.25) : 1 / 60;
      last = t;
      tick();
      if (!fx.frozen) {
        try { engine.viz.render({ elapsedTime: dt * settings.speed }); } catch (err) { onRenderError(err); }
      }
    };
    win.requestAnimationFrame(frame);
  }

  let lastHudUpdate = 0;
  const meterFill = $('#meter-fill');
  const beatDot = $('#beat-dot');
  function tick() {
    const t = now();
    const beat = audio.analyse(t);
    if (beat) { beatDot.classList.add('hit'); setTimeout(() => beatDot.classList.remove('hit'), 60); }
    meterFill.style.width = Math.round(clamp(audio.level * 160, 0, 100)) + '%';
    // Auto-pilot
    if (settings.auto && !locked && !pending) {
      const el = (t - lastSwitch) / 1000;
      if (el >= settings.autoInterval) {
        const wantsBeat = settings.beatSync && audio.kind !== 'none';
        if (!wantsBeat || beat || el >= settings.autoInterval + 4) {
          lastSwitch = t;
          go(settings.shuffle ? 'random' : 'next', false);
        }
      }
    }
    if (t - lastHudUpdate > 250) { lastHudUpdate = t; if (settings.auto) refreshPills(); }
  }

  function toggleOutput() {
    if (outWin && !outWin.closed) { outWin.close(); detachOutput(); return; }
    const w = window.open('output.html', 'amethyst-output', 'popup=yes,width=1280,height=720');
    if (!w) { toast('Pop-up blocked — allow pop-ups for this page, then press O again', true); return; }
    outWin = w;
    toast('Opening output window…');
  }

  function outputReady(win) {
    if (!started) return;
    if (outViz) { try { outViz.disconnectAudio(audio.input); } catch (_) { /* */ } }
    outWin = win;
    const canvas = win.document.getElementById('viz');
    try { outViz = makeViz(canvas, win); } catch (err) { toast('Output failed: ' + errMsg(err), true); return; }
    useViz(outViz, canvas, win);
    win.addEventListener('resize', () => { if (engine.win === win) resize(); });
    win.addEventListener('pagehide', () => setTimeout(() => { if (win.closed && outWin === win) detachOutput(); }, 60));
    document.body.classList.add('output-live');
    try { preview.srcObject = canvas.captureStream(30); preview.play().catch(() => {}); } catch (_) { /* preview is optional */ }
    applyFx();
    refreshPills(); refreshTopbar();
    toast('Output live — move it to the projector and press F there');
  }

  function detachOutput() {
    if (!outViz) { outWin = null; return; }
    try { outViz.disconnectAudio(audio.input); } catch (_) { /* */ }
    outViz = null; outWin = null;
    preview.srcObject = null;
    document.body.classList.remove('output-live');
    useViz(mainViz, mainCanvas, window);
    refreshPills(); refreshTopbar();
    toast('Output window closed');
  }
  setInterval(() => { if (outViz && (!outWin || outWin.closed)) detachOutput(); }, 500);

  // ---------------------------------------------------------------- FX
  function stageDocs() {
    const d = [document];
    if (outWin && !outWin.closed) { try { d.push(outWin.document); } catch (_) { /* */ } }
    return d;
  }
  function applyFx() {
    stageDocs().forEach((d) => d.documentElement.classList.toggle('blackout', fx.blackout));
    refreshPills();
  }
  function flash() {
    const docs = stageDocs();
    docs.forEach((d) => d.documentElement.classList.add('flash'));
    setTimeout(() => docs.forEach((d) => d.documentElement.classList.remove('flash')), 70);
  }
  function setFrozen(on) {
    if (fx.frozen === on) return;
    fx.frozen = on;
    refreshPills();
  }

  // ---------------------------------------------------------------- import
  const AUDIO_EXT = ['mp3', 'wav', 'ogg', 'flac', 'm4a', 'aac', 'opus', 'webm', 'mp4'];
  const IMG_EXT = ['jpg', 'jpeg', 'png', 'bmp', 'gif', 'webp', 'tga'];
  const extOf = (n) => (n.split('.').pop() || '').toLowerCase();

  const bg = {
    keys: [], running: false, total: 0, done: 0,
    add(keys) { this.keys.push(...keys); this.total += keys.length; this.run(); },
    async run() {
      if (this.running) return;
      this.running = true;
      while (this.keys.length) {
        const e = lib.byKey.get(this.keys.shift());
        if (e && !e.broken && !e.converted) {
          try { await getPreset(e, false, false); } catch (err) { markBroken(e, err); }
        }
        this.done++;
        if (this.done % 10 === 0 || !this.keys.length) renderBgStatus();
        await idle();
      }
      this.running = false;
      this.total = 0; this.done = 0;
      renderBgStatus();
      renderList();
    },
  };

  async function importFiles(files) {
    const presets = [], images = [], audios = [];
    for (const f of files) {
      const x = extOf(f.name);
      if (x === 'milk' || x === 'json') presets.push(f);
      else if (IMG_EXT.includes(x)) images.push(f);
      else if (AUDIO_EXT.includes(x) || (f.type && f.type.startsWith('audio/'))) audios.push(f);
    }
    if (!presets.length && !images.length && !audios.length) { toast('Nothing usable there — drop .milk presets, textures or audio', true); return; }
    if (images.length) await importTextures(images);
    if (presets.length) await importPresets(presets);
    if (audios.length) {
      if (started) audio.file(audios[0]);
      else toast('Pick an audio option to start, then drop the audio file again', true);
    }
  }

  async function importPresets(files) {
    let ok = 0, bad = 0, firstEntry = null;
    const keys = [];
    let metas = [], datas = [];
    const flush = async () => {
      if (!metas.length) return;
      await DB.putMany('data', datas);
      await DB.putMany('meta', metas);
      for (const m of metas) {
        cache.delete(m.key);
        const e = addEntry(Object.assign({}, m, { inflight: null }));
        if (!firstEntry) firstEntry = e;
      }
      metas = []; datas = [];
    };
    for (let i = 0; i < files.length; i++) {
      const f = files[i];
      try {
        const text = await f.text();
        const name = f.name.replace(/\.(milk|json)$/i, '').trim() || 'Untitled';
        const key = 'user:' + name;
        let rec;
        if (extOf(f.name) === 'json') {
          const j = JSON.parse(text);
          if (!j || typeof j.baseVals !== 'object') throw new Error('not a Butterchurn preset');
          rec = { key, raw: null, json: j };
        } else {
          if (!/=/.test(text)) throw new Error('not a MilkDrop preset');
          rec = { key, raw: text, json: null };
        }
        datas.push(rec);
        metas.push({ key, name, source: 'user', converted: !!rec.json, broken: false, error: null, added: Date.now(), cv: CONV_VERSION });
        keys.push(key);
        ok++;
      } catch (err) {
        bad++;
        console.warn('[Amethyst] skipped file', f.name, err);
      }
      if (metas.length >= 250) { await flush(); toast(`Importing… ${i + 1} / ${files.length}`); }
    }
    await flush();
    if (!ok) { toast(`No presets imported${bad ? ` (${bad} unreadable)` : ''}`, true); return; }
    if (settings.pool === 'builtin') settings.pool = 'all';
    sortLib();
    rebuildView();
    toast(`Imported ${ok} preset${ok === 1 ? '' : 's'}${bad ? ` · ${bad} skipped` : ''}${ok > 1 ? '  ·  G switches pool' : ''}`);
    bg.add(keys);
    if (started && firstEntry) loadEntry(firstEntry);
  }

  function readDataURL(f) {
    return new Promise((res, rej) => { const r = new FileReader(); r.onload = () => res(r.result); r.onerror = () => rej(r.error); r.readAsDataURL(f); });
  }
  function imageSize(src) {
    return new Promise((res, rej) => { const i = new Image(); i.onload = () => res({ width: i.naturalWidth, height: i.naturalHeight }); i.onerror = () => rej(new Error('bad image')); i.src = src; });
  }
  async function importTextures(files) {
    const recs = [];
    for (const f of files) {
      try {
        const data = await readDataURL(f);
        const { width, height } = await imageSize(data);
        recs.push({ name: f.name.replace(/\.[^.]+$/, ''), data, width, height });
      } catch (_) { /* skip */ }
    }
    if (!recs.length) return;
    await DB.putMany('textures', recs).catch(() => {});
    applyTextures(recs);
    toast(`Loaded ${recs.length} texture${recs.length === 1 ? '' : 's'}`);
  }
  function applyTextures(recs) {
    const map = {};
    for (const r of recs) { map[r.name] = r; map[r.name.toLowerCase()] = r; }
    Object.assign(textures, map);
    [mainViz, outViz].forEach((v) => { if (v) v.loadExtraImages(map); });
  }

  async function clearImported() {
    const n = lib.all.filter((e) => e.source === 'user').length;
    if (!n) { toast('No imported presets to clear'); return; }
    if (!window.confirm(`Remove all ${n} imported presets from Amethyst? (Your .milk files on disk are not touched.)`)) return;
    await DB.clear('meta'); await DB.clear('data');
    bg.keys = [];
    lib.all = lib.all.filter((e) => e.source !== 'user');
    lib.byKey = new Map(lib.all.map((e) => [e.key, e]));
    cache.clear();
    hist = hist.filter((k) => lib.byKey.has(k));
    settings.cues = settings.cues.map((k) => (k && lib.byKey.has(k) ? k : null));
    save();
    rebuildView(); refreshCues();
    toast(`Cleared ${n} imported presets`);
  }

  async function filesFromDrop(dt) {
    const entries = [...dt.items].filter((i) => i.kind === 'file').map((i) => i.webkitGetAsEntry && i.webkitGetAsEntry()).filter(Boolean);
    const plain = [...dt.files];
    if (!entries.length) return plain;
    const out = [];
    const walk = async (ent) => {
      if (ent.isFile) {
        try { out.push(await new Promise((res, rej) => ent.file(res, rej))); } catch (_) { /* */ }
      } else if (ent.isDirectory) {
        const reader = ent.createReader();
        for (;;) {
          const batch = await new Promise((res) => reader.readEntries(res, () => res([])));
          if (!batch.length) break;
          for (const b of batch) await walk(b);
        }
      }
    };
    for (const e of entries) await walk(e);
    return out;
  }

  // ---------------------------------------------------------------- UI: toast, HUD, cues, browser, menu
  const toastEl = $('#toast');
  let toastTimer = 0;
  function toast(msg, isErr) {
    toastEl.textContent = msg;
    toastEl.classList.toggle('err', !!isErr);
    toastEl.classList.add('show');
    clearTimeout(toastTimer);
    toastTimer = setTimeout(() => toastEl.classList.remove('show'), isErr ? 3200 : 1700);
  }

  function refreshNow() {
    $('#now-name').textContent = current ? current.name : '—';
    const parts = [];
    if (current) parts.push(current.source === 'user' ? 'Imported' : 'Built-in');
    const i = current ? view.indexOf(current) : -1;
    parts.push(i >= 0 ? `#${i + 1} of ${view.length}` : `${view.length} in pool`);
    if (settings.pool !== 'all') parts.push(`pool: ${POOL_LABEL[settings.pool]}`);
    if (query) parts.push(`search “${query}”  (Esc clears)`);
    $('#now-sub').textContent = parts.join('  ·  ');
  }

  const pillsEl = $('#pills');
  function pill(html, state, key) {
    const cls = state === true ? 'on' : state || '';
    return `<span class="pill ${cls}">${html}${key ? `<kbd>${key}</kbd>` : ''}</span>`;
  }
  let lastPills = '';
  function refreshPills() {
    const p = [];
    if (settings.auto) {
      const left = Math.max(0, Math.ceil(settings.autoInterval - (now() - lastSwitch) / 1000));
      const mode = (settings.shuffle ? 'shuffle' : 'in order') + (settings.beatSync ? ' · on beat' : '');
      p.push(pill(`AUTO <b>${locked ? '—' : left + 's'}</b> · ${mode}`, true, 'A'));
    } else {
      p.push(pill('AUTO off', false, 'A'));
    }
    if (locked) p.push(pill('LOCKED', 'warn', 'L'));
    p.push(pill(`BLEND <b>${fmtNum(settings.blendTime)}s</b>`, false, '[ ]'));
    p.push(pill(`EVERY <b>${settings.autoInterval}s</b>`, false, ', .'));
    p.push(pill(`SHUFFLE <b>${settings.shuffle ? 'on' : 'off'}</b>`, false, 'S'));
    p.push(pill(`BEAT <b>${settings.beatSync ? 'on' : 'off'}</b>`, false, 'K'));
    p.push(pill(`SPEED <b>${fmtNum(settings.speed)}×</b>`, settings.speed !== 1 ? 'warn' : false, '- ='));
    p.push(pill(`GAIN <b>${fmtNum(settings.sensitivity)}×</b>`, false, '↑ ↓'));
    p.push(pill(`QUALITY <b>${QUALITY[settings.quality].label}</b>`, false, 'Q'));
    if (outViz) p.push(pill('OUTPUT LIVE', true, 'O'));
    if (fx.blackout) p.push(pill('BLACKOUT', 'hot', 'B'));
    if (fx.frozen) p.push(pill('FROZEN', 'warn', 'Z'));
    const html = p.join('');
    if (html !== lastPills) { pillsEl.innerHTML = html; lastPills = html; }
  }

  function refreshAudio() {
    $('#audio-label').textContent = audio.label;
  }

  function refreshTopbar() {
    document.querySelectorAll('[data-act="browser"]').forEach((b) => b.classList.toggle('on', !browser.el.hidden));
    document.querySelectorAll('[data-act="output"]').forEach((b) => b.classList.toggle('on', !!outViz));
  }

  const cuesEl = $('#cues');
  function refreshCues() {
    let html = '';
    for (let i = 0; i < 10; i++) {
      const key = settings.cues[i];
      const e = key && lib.byKey.get(key);
      const label = e ? e.name : key ? '(missing)' : 'empty';
      const cls = ['cue', e ? 'filled' : '', e && e === current ? 'active' : ''].join(' ');
      html += `<div class="${cls}" data-cue="${i}" data-key="${(i + 1) % 10}" title="${esc(e ? e.name : 'Shift+' + ((i + 1) % 10) + ' stores the current preset here')}">${esc(label)}</div>`;
    }
    cuesEl.innerHTML = html;
  }
  function fireCue(slot) {
    const key = settings.cues[slot];
    const label = (slot + 1) % 10;
    if (!key) { toast(`Cue ${label} is empty — Shift+${label} stores the current preset`); return; }
    const e = lib.byKey.get(key);
    if (!e) { toast(`Cue ${label}: preset no longer in library`, true); return; }
    const el = cuesEl.children[slot];
    if (el) { el.classList.remove('fired'); void el.offsetWidth; el.classList.add('fired'); }
    loadEntry(e);
  }
  function storeCue(slot) {
    if (!current) return;
    settings.cues[slot] = current.key; save(); refreshCues(); renderList();
    toast(`Stored in cue ${(slot + 1) % 10}: ${current.name}`);
  }
  function clearCue(slot) {
    settings.cues[slot] = null; save(); refreshCues(); renderList();
    toast(`Cleared cue ${(slot + 1) % 10}`);
  }

  // Preset browser (virtualised list)
  const ROW = 30;
  const browser = { el: $('#browser'), list: $('#list'), spacer: $('#list-spacer'), search: $('#search'), sel: 0 };
  function renderCounts() {
    const u = lib.all.filter((e) => e.source === 'user').length;
    const counts = { all: lib.all.length, user: u, builtin: lib.all.length - u };
    document.querySelectorAll('#pools button').forEach((b) => {
      b.classList.toggle('on', b.dataset.pool === settings.pool);
      b.querySelector('span').textContent = counts[b.dataset.pool];
    });
  }
  let listRaf = 0;
  function renderList(scrollToSel) {
    if (browser.el.hidden) return;
    if (scrollToSel) ensureVisible();
    cancelAnimationFrame(listRaf);
    listRaf = requestAnimationFrame(() => {
      const { list, spacer } = browser;
      if (!view.length) {
        spacer.style.height = 'auto';
        spacer.innerHTML = `<div class="empty">${lib.all.length ? 'No presets match.' : 'No presets yet.'}<br><br>Drop .milk files or a whole folder here,<br>or press <kbd>Ctrl</kbd>+<kbd>O</kbd>.</div>`;
        return;
      }
      spacer.style.height = view.length * ROW + 'px';
      const top = list.scrollTop, h = list.clientHeight || 400;
      const first = Math.max(0, Math.floor(top / ROW) - 6);
      const last = Math.min(view.length, Math.ceil((top + h) / ROW) + 6);
      let html = '';
      for (let i = first; i < last; i++) {
        const e = view[i];
        const cue = settings.cues.indexOf(e.key);
        const cls = ['row', i === browser.sel ? 'sel' : '', e === current ? 'cur' : '', e.broken ? 'broken' : ''].join(' ');
        html += `<div class="${cls}" data-i="${i}" style="top:${i * ROW}px" title="${esc(e.broken ? 'Unsupported: ' + (e.error || '') : e.name)}">`
          + `<span class="nm">${esc(e.name)}</span>`
          + (cue >= 0 ? `<span class="tag cue">${(cue + 1) % 10}</span>` : '')
          + (e.source === 'user' ? '<span class="tag user">milk</span>' : '')
          + '</div>';
      }
      spacer.innerHTML = html;
    });
  }
  function ensureVisible() {
    const { list } = browser;
    const y = browser.sel * ROW, h = list.clientHeight || 400;
    if (y < list.scrollTop) list.scrollTop = y - ROW * 2;
    else if (y + ROW > list.scrollTop + h) list.scrollTop = y + ROW * 3 - h;
  }
  function moveSel(d) {
    if (!view.length) return;
    browser.sel = clamp(browser.sel + d, 0, view.length - 1);
    renderList(true);
  }
  function showBrowser(on, focusSearch) {
    browser.el.hidden = !on;
    if (on) {
      browser.sel = Math.max(0, current ? view.indexOf(current) : 0);
      renderCounts();
      renderList(true);
      renderBgStatus();
      if (focusSearch) { browser.search.focus(); browser.search.select(); }
    } else if (document.activeElement === browser.search) {
      browser.search.blur();
    }
    refreshTopbar();
  }
  function renderBgStatus() {
    const el = $('#bg-status');
    const broken = lib.all.filter((e) => e.source === 'user' && e.broken).length;
    if (bg.running && bg.total) el.textContent = `Preparing imported presets… ${bg.done} / ${bg.total}`;
    else el.textContent = broken ? `${broken} imported preset${broken === 1 ? '' : 's'} could not be converted (shown struck through)` : '';
  }
  function setPool(p) {
    settings.pool = p; save();
    rebuildView();
    toast(`Pool: ${POOL_LABEL[p]} (${view.length})`);
  }

  // Audio menu
  const menu = { el: $('#menu'), list: $('#menu-list'), items: [], sel: 0 };
  async function openMenu() {
    let devices = [];
    try { devices = (await navigator.mediaDevices.enumerateDevices()).filter((d) => d.kind === 'audioinput'); } catch (_) { /* */ }
    const items = [];
    if (devices.length && devices.some((d) => d.label)) {
      devices.forEach((d, i) => items.push({
        icon: '🎤', label: d.label || `Input ${i + 1}`,
        active: audio.kind === 'mic' && audio.deviceId === d.deviceId,
        run: () => audio.mic(d.deviceId),
      }));
    } else {
      items.push({ icon: '🎤', label: 'Microphone / line-in', active: audio.kind === 'mic', run: () => audio.mic(settings.micId) });
    }
    items.push({ sep: true });
    items.push({ icon: '🖥', label: 'System or tab audio…', active: audio.kind === 'system', run: () => audio.system() });
    items.push({ icon: '🎵', label: 'Audio file…', active: audio.kind === 'file', run: () => $('#file-audio').click() });
    items.push({ icon: '⏹', label: 'No audio', active: audio.kind === 'none', run: () => audio.none() });
    menu.items = items;
    menu.sel = Math.max(0, items.findIndex((i) => i.active));
    if (menu.items[menu.sel].sep) menu.sel = 0;
    renderMenu();
    menu.el.hidden = false;
  }
  function renderMenu() {
    let n = 0;
    menu.list.innerHTML = menu.items.map((it, i) => (it.sep ? '<li class="sep"></li>'
      : `<li data-i="${i}" class="${i === menu.sel ? 'sel' : ''}"><span class="ck">${it.active ? '●' : ''}</span>${it.icon} ${esc(it.label)}<kbd style="margin-left:auto">${++n}</kbd></li>`)).join('');
  }
  function closeMenu() { menu.el.hidden = true; }
  async function runMenuItem(i) {
    const it = menu.items[i];
    if (!it || it.sep) return;
    closeMenu();
    try { await it.run(); } catch (err) { toast('Audio: ' + errMsg(err), true); }
  }
  function menuKey(e) {
    const k = e.key;
    const move = (d) => {
      let i = menu.sel;
      do { i = (i + d + menu.items.length) % menu.items.length; } while (menu.items[i].sep);
      menu.sel = i; renderMenu();
    };
    if (k === 'ArrowDown') move(1);
    else if (k === 'ArrowUp') move(-1);
    else if (k === 'Enter' || k === ' ') runMenuItem(menu.sel);
    else if (k === 'Escape' || keyId(e) === 'KeyI') closeMenu();
    else if (/^[1-9]$/.test(k)) {
      const real = menu.items.map((it, i) => (it.sep ? -1 : i)).filter((i) => i >= 0);
      const idx = real[Number(k) - 1];
      if (idx !== undefined) runMenuItem(idx);
    } else return;
    e.preventDefault();
  }

  // ---------------------------------------------------------------- keyboard
  const DIGITS = {};
  for (let i = 0; i < 10; i++) { DIGITS['Digit' + ((i + 1) % 10)] = i; DIGITS['Numpad' + ((i + 1) % 10)] = i; }

  // Normalise a key event to a code-like id. Letters and punctuation follow the printed
  // character (so shortcuts work on any keyboard layout); the number row keeps its
  // physical position so Shift+1…0 still means "store cue".
  const PUNCT = {
    ' ': 'Space', '[': 'BracketLeft', '{': 'BracketLeft', ']': 'BracketRight', '}': 'BracketRight',
    ',': 'Comma', '<': 'Comma', '.': 'Period', '>': 'Period', '-': 'Minus', '_': 'Minus',
    '=': 'Equal', '+': 'Equal', '/': 'Slash', '?': 'Slash', '\\': 'Backslash',
  };
  const SHIFT_DIGITS = { '!': 1, '@': 2, '#': 3, '$': 4, '%': 5, '^': 6, '&': 7, '*': 8, '(': 9, ')': 0 };
  function keyId(e) {
    if (/^(Digit|Numpad)\d$/.test(e.code)) return e.code;
    const k = e.key || '';
    if (k.length === 1) {
      if (/[a-z]/i.test(k)) return 'Key' + k.toUpperCase();
      if (/\d/.test(k)) return 'Digit' + k;
      if (!e.code && SHIFT_DIGITS[k] !== undefined) return 'Digit' + SHIFT_DIGITS[k];
      if (PUNCT[k]) return PUNCT[k];
    }
    return e.code || k;
  }
  const help = $('#help');
  let started = false;

  function toggleFullscreen(win) {
    const d = (win || window).document;
    if (d.fullscreenElement) d.exitFullscreen().catch(() => {});
    else d.documentElement.requestFullscreen().catch((err) => toast('Fullscreen blocked: ' + errMsg(err), true));
  }

  function searchKey(e) {
    switch (e.key) {
      case 'Escape': showBrowser(false); break; // keeps the search as a filter; Esc again clears it
      case 'ArrowDown': moveSel(1); break;
      case 'ArrowUp': moveSel(-1); break;
      case 'PageDown': moveSel(10); break;
      case 'PageUp': moveSel(-10); break;
      case 'Enter': if (view[browser.sel]) loadEntry(view[browser.sel], { hard: e.shiftKey }); break;
      case 'Tab': browser.search.blur(); break;
      default: return; // let the user type
    }
    e.preventDefault();
  }

  function splashKey(e) {
    const map = { Enter: 'mic', Digit1: 'mic', Digit2: 'system', Digit3: 'file', Digit4: 'none', Numpad1: 'mic', Numpad2: 'system', Numpad3: 'file', Numpad4: 'none' };
    const id = keyId(e);
    const kind = map[id] || map[e.key];
    if (kind) { e.preventDefault(); start(kind); }
    else if (id === 'KeyH' || e.key === '?') { e.preventDefault(); help.hidden = !help.hidden; }
  }

  function handleKey(e) {
    const win = e.view || window;
    if (e.defaultPrevented) return;
    if (e.target && e.target.id !== 'search' && /^(INPUT|TEXTAREA|SELECT)$/.test(e.target.tagName)) return;
    if (!started) return splashKey(e);
    if (!menu.el.hidden) return menuKey(e);
    if (e.target === browser.search) return searchKey(e);

    if (e.key === 'Escape') {
      if (!help.hidden) help.hidden = true;
      else if (!browser.el.hidden) showBrowser(false);
      else if (query) { query = ''; browser.search.value = ''; rebuildView(); toast('Search cleared'); }
      return;
    }
    const code = keyId(e), sh = e.shiftKey, rep = e.repeat;
    if (e.ctrlKey || e.metaKey) {
      if (code === 'KeyO') { e.preventDefault(); (e.shiftKey ? $('#folder-milk') : $('#file-milk')).click(); }
      return; // leave other browser shortcuts alone
    }
    if (code in DIGITS) {
      e.preventDefault();
      if (rep) return;
      const slot = DIGITS[code];
      if (e.altKey) clearCue(slot); else if (sh) storeCue(slot); else fireCue(slot);
      return;
    }
    if (e.altKey) return;

    let handled = true;
    switch (code) {
      // presets
      case 'Space': case 'ArrowRight': if (!rep) go('next', sh); break;
      case 'ArrowLeft': if (!rep) back(sh); break;
      case 'KeyR': if (!rep) go('random', sh); break;
      case 'BracketLeft': settings.blendTime = stepIn(BLEND_STEPS, settings.blendTime, -1); save(); refreshPills(); toast(`Blend ${fmtNum(settings.blendTime)}s`); break;
      case 'BracketRight': settings.blendTime = stepIn(BLEND_STEPS, settings.blendTime, 1); save(); refreshPills(); toast(`Blend ${fmtNum(settings.blendTime)}s`); break;
      case 'KeyL': if (!rep) { locked = !locked; refreshPills(); toast(locked ? 'Preset locked' : 'Preset unlocked'); } break;
      case 'KeyT': if (!rep && current && engine.viz) engine.viz.launchSongTitleAnim(current.name); break;
      // auto-pilot
      case 'KeyA': if (!rep) { settings.auto = !settings.auto; lastSwitch = now(); save(); refreshPills(); toast(settings.auto ? `Auto-pilot on — every ${settings.autoInterval}s` : 'Auto-pilot off'); } break;
      case 'Comma': settings.autoInterval = stepIn(INTERVAL_STEPS, settings.autoInterval, -1); save(); refreshPills(); toast(`Auto interval ${settings.autoInterval}s`); break;
      case 'Period': settings.autoInterval = stepIn(INTERVAL_STEPS, settings.autoInterval, 1); save(); refreshPills(); toast(`Auto interval ${settings.autoInterval}s`); break;
      case 'KeyK': if (!rep) { settings.beatSync = !settings.beatSync; save(); refreshPills(); toast(settings.beatSync ? 'Auto switches land on the beat' : 'Beat-sync off'); } break;
      case 'KeyS': if (!rep) { settings.shuffle = !settings.shuffle; save(); refreshPills(); toast(settings.shuffle ? 'Auto-pilot: shuffle' : 'Auto-pilot: in order'); } break;
      // performance
      case 'KeyB': if (!rep) { fx.blackout = !fx.blackout; applyFx(); } break;
      case 'KeyX': flash(); break;
      case 'KeyZ': setFrozen(true); break;
      case 'Minus': case 'NumpadSubtract': settings.speed = stepIn(SPEED_STEPS, settings.speed, -1); save(); refreshPills(); toast(`Speed ${fmtNum(settings.speed)}×`); break;
      case 'Equal': case 'NumpadAdd': settings.speed = stepIn(SPEED_STEPS, settings.speed, 1); save(); refreshPills(); toast(`Speed ${fmtNum(settings.speed)}×`); break;
      case 'Backspace': settings.speed = 1; save(); refreshPills(); toast('Speed 1×'); break;
      case 'ArrowUp': audio.setGain(settings.sensitivity * 1.2); break;
      case 'ArrowDown': audio.setGain(settings.sensitivity / 1.2); break;
      case 'KeyQ': if (!rep) { settings.quality = (settings.quality + 1) % QUALITY.length; save(); resize(); refreshPills(); toast(`Quality: ${QUALITY[settings.quality].label}`); } break;
      // view
      case 'KeyF': if (!rep) toggleFullscreen(win); break;
      case 'KeyO': if (!rep) toggleOutput(); break;
      case 'KeyU': if (!rep) { settings.clean = !settings.clean; save(); document.body.classList.toggle('clean', settings.clean); } break;
      case 'KeyP': case 'Tab': if (!rep) showBrowser(browser.el.hidden); break;
      case 'Slash':
        if (sh) help.hidden = !help.hidden;
        else { if (settings.clean) { settings.clean = false; document.body.classList.remove('clean'); } showBrowser(true, true); }
        break;
      case 'KeyH': if (!rep) help.hidden = !help.hidden; break;
      case 'KeyG': if (!rep) setPool(POOLS[(POOLS.indexOf(settings.pool) + 1) % POOLS.length]); break;
      // audio
      case 'KeyI': if (!rep) openMenu(); break;
      case 'KeyM':
        if (rep) break;
        if (sh) {
          if (audio.kind !== 'file') toast('No audio file playing — press M to choose one');
          else if (player.paused) { player.play(); toast('Resumed'); } else { player.pause(); toast('Paused'); }
        } else $('#file-audio').click();
        break;
      default: handled = false;
    }
    if (handled) e.preventDefault();
  }
  function handleKeyUp(e) { if (keyId(e) === 'KeyZ') setFrozen(false); }
  function handleBlur() { setFrozen(false); }

  window.addEventListener('keydown', handleKey);
  window.addEventListener('keyup', handleKeyUp);
  window.addEventListener('blur', handleBlur);
  window.VJ = {
    handleKey, handleKeyUp, handleBlur, outputReady,
    // Diagnostics for the browser console: VJ.stats()
    stats: () => ({
      frames: engine.viz && engine.viz.renderer ? engine.viz.renderer.frameNum : 0,
      fps: engine.viz && engine.viz.renderer ? Math.round(engine.viz.renderer.fps) : 0,
      target: engine.win === window ? 'control window' : 'output window',
      preset: current && current.name,
      audio: audio.kind,
      library: { total: lib.all.length, imported: lib.all.filter((e) => e.source === 'user').length, unsupported: lib.all.filter((e) => e.broken).length },
    }),
  };

  // ---------------------------------------------------------------- mouse / DOM wiring
  document.addEventListener('click', (e) => {
    const btn = e.target.closest('[data-act]');
    if (btn) {
      const act = btn.dataset.act;
      if (act === 'browser') showBrowser(browser.el.hidden);
      else if (act === 'import') $('#file-milk').click();
      else if (act === 'import-folder') $('#folder-milk').click();
      else if (act === 'audio') { if (started) openMenu(); }
      else if (act === 'output') { if (started) toggleOutput(); }
      else if (act === 'fullscreen') toggleFullscreen(window);
      else if (act === 'help') help.hidden = !help.hidden;
      else if (act === 'clear-user') clearImported();
      btn.blur();
      return;
    }
    const poolBtn = e.target.closest('[data-pool]');
    if (poolBtn) { setPool(poolBtn.dataset.pool); poolBtn.blur(); return; }
    const row = e.target.closest('.row');
    if (row) { browser.sel = Number(row.dataset.i); renderList(); loadEntry(view[browser.sel], { hard: e.shiftKey }); return; }
    const cue = e.target.closest('[data-cue]');
    if (cue) { const s = Number(cue.dataset.cue); if (e.shiftKey) storeCue(s); else if (e.altKey) clearCue(s); else fireCue(s); return; }
    const mi = e.target.closest('#menu-list li[data-i]');
    if (mi) { runMenuItem(Number(mi.dataset.i)); return; }
    const sb = e.target.closest('[data-start]');
    if (sb) { start(sb.dataset.start); return; }
    if (e.target === help) help.hidden = true;
  });
  cuesEl.addEventListener('contextmenu', (e) => {
    const cue = e.target.closest('[data-cue]');
    if (cue) { e.preventDefault(); clearCue(Number(cue.dataset.cue)); }
  });
  browser.list.addEventListener('scroll', () => renderList());
  browser.search.addEventListener('input', () => { query = browser.search.value.trim(); rebuildView(); });
  window.addEventListener('resize', () => { if (engine.win === window) resize(); renderList(); });

  $('#file-milk').addEventListener('change', (e) => { const f = [...e.target.files]; e.target.value = ''; if (f.length) importFiles(f); });
  $('#folder-milk').addEventListener('change', (e) => { const f = [...e.target.files]; e.target.value = ''; if (f.length) importFiles(f); });
  $('#file-audio').addEventListener('change', (e) => {
    const f = e.target.files[0]; e.target.value = '';
    if (f) { try { audio.file(f); } catch (err) { toast('Audio: ' + errMsg(err), true); } }
  });

  // Drag & drop
  let dragDepth = 0;
  window.addEventListener('dragenter', (e) => { if (e.dataTransfer && [...e.dataTransfer.types].includes('Files')) { dragDepth++; document.body.classList.add('dragging'); } });
  window.addEventListener('dragleave', () => { dragDepth = Math.max(0, dragDepth - 1); if (!dragDepth) document.body.classList.remove('dragging'); });
  window.addEventListener('dragover', (e) => { e.preventDefault(); });
  window.addEventListener('drop', (e) => {
    e.preventDefault();
    dragDepth = 0; document.body.classList.remove('dragging');
    if (!e.dataTransfer) return;
    filesFromDrop(e.dataTransfer).then((files) => importFiles(files)).catch((err) => toast('Import failed: ' + errMsg(err), true));
  });

  // Cursor hiding & fullscreen state
  let idleTimer = 0;
  window.addEventListener('mousemove', () => {
    document.body.classList.remove('cursor-idle');
    clearTimeout(idleTimer);
    idleTimer = setTimeout(() => document.body.classList.add('cursor-idle'), 2000);
  });
  document.addEventListener('fullscreenchange', () => document.body.classList.toggle('fs', !!document.fullscreenElement));

  navigator.mediaDevices && navigator.mediaDevices.addEventListener && navigator.mediaDevices.addEventListener('devicechange', () => { if (!menu.el.hidden) openMenu(); });

  // ---------------------------------------------------------------- startup
  const ready = (async () => {
    loadBuiltins();
    try {
      const metas = await DB.getAll('meta');
      metas.forEach((m) => {
        // Presets judged by an older converter get a fresh chance.
        if (m.cv !== CONV_VERSION) Object.assign(m, { converted: false, broken: false, error: null });
        addEntry(Object.assign({}, m));
      });
      const tex = await DB.getAll('textures');
      if (tex.length) applyTextures(tex);
      const todo = metas.filter((m) => !m.converted && !m.broken).map((m) => m.key);
      if (todo.length) bg.add(todo);
    } catch (err) { console.warn('[Amethyst] library load failed', err); }
    sortLib();
    rebuildView();
    refreshCues();
    refreshPills();
    refreshAudio();
  })();
  conv.start();
  document.body.classList.toggle('clean', !!settings.clean);

  async function start(kind) {
    if (started) return;
    const errEl = $('#splash-err');
    if (!BC || !BC.createVisualizer) { errEl.textContent = 'Visualizer library failed to load.'; return; }
    try { audio.ensure(); } catch (err) { errEl.textContent = 'Audio is unavailable: ' + errMsg(err); return; }
    try {
      mainViz = makeViz(mainCanvas, window);
    } catch (err) {
      errEl.textContent = 'WebGL 2 is required — try an up-to-date Chrome or Edge. (' + errMsg(err) + ')';
      return;
    }
    started = true;
    $('#splash').hidden = true;
    help.hidden = true;
    await ready;
    current = (settings.last && lib.byKey.get(settings.last)) || null;
    if (current && current.broken) current = null;
    if (!current) current = pickRandom() || lib.all[0] || null;
    useViz(mainViz, mainCanvas, window);
    refreshPills(); refreshTopbar();
    try {
      if (kind === 'mic') await audio.mic(settings.micId);
      else if (kind === 'system') await audio.system();
      else if (kind === 'file') $('#file-audio').click();
      else toast('Visuals only — press I to pick an audio input any time');
    } catch (err) {
      toast('Audio: ' + errMsg(err) + ' — press I to pick another input', true);
    }
  }
})();

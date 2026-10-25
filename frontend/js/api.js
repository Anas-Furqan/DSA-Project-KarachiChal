/*
 * api.js — every call to the C++ backend goes through API.request().
 *  - wraps fetch in try/catch, never throws raw network errors at callers
 *  - success: returns `data`, forwards `trace` to the DS inspector log
 *  - failure: throws ApiError with the server's message + code
 *  - network failure: shows the "Server not running" banner
 * Also holds small shared UI helpers (toasts, number tweening, escaping).
 */
class ApiError extends Error {
  constructor(message, code, status) {
    super(message);
    this.code = code || "ERROR";
    this.status = status || 0;
  }
}

const API = {
  onTrace: null,          // set by Inspector: (entries, truncated) => void
  onServerState: null,    // set by App: (isUp) => void

  async request(method, path, body) {
    let res;
    try {
      res = await fetch(path, {
        method,
        headers: body ? { "Content-Type": "application/json" } : undefined,
        body: body ? JSON.stringify(body) : undefined,
      });
    } catch (e) {
      if (this.onServerState) this.onServerState(false);
      throw new ApiError("Server not running", "NETWORK", 0);
    }
    if (this.onServerState) this.onServerState(true);

    let json;
    try {
      json = await res.json();
    } catch (e) {
      throw new ApiError(`Unexpected response (${res.status})`, "BAD_RESPONSE", res.status);
    }
    if (!json || json.ok !== true) {
      throw new ApiError((json && json.error) || `Request failed (${res.status})`, json && json.code, res.status);
    }
    if (this.onTrace && Array.isArray(json.trace) && json.trace.length) this.onTrace(json.trace, json.truncated);
    return json.data;
  },

  get(path) { return this.request("GET", path); },
  post(path, body) { return this.request("POST", path, body || {}); },
};

/* ------------------------------------------------------------- UI helpers */
const UI = {
  escape(s) {
    return String(s == null ? "" : s).replace(/[&<>"']/g, (c) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" }[c]));
  },

  toast(title, message, kind = "success", ms = 3800) {
    const box = document.getElementById("toasts");
    if (!box) return;
    const el = document.createElement("div");
    el.className = `toast ${kind}`;
    el.innerHTML = `<span class="t-bar"></span><div><div class="t-title">${UI.escape(title)}</div>${
      message ? `<div class="t-msg">${UI.escape(message)}</div>` : ""}</div>`;
    box.appendChild(el);
    while (box.children.length > 5) box.firstElementChild.remove();
    setTimeout(() => {
      el.classList.add("out");
      el.addEventListener("animationend", () => el.remove(), { once: true });
    }, ms);
  },

  error(err, fallback = "Something went wrong") {
    if (err && err.code === "NETWORK") return;   // the banner already says it
    UI.toast(fallback, err && err.message ? err.message : String(err), "error", 5000);
  },

  // tween a number in an element over ~300ms (no layout shift: tabular nums)
  animateNumber(el, to, decimals = 0) {
    if (!el) return;
    const from = parseFloat(el.dataset.value || el.textContent) || 0;
    el.dataset.value = to;
    if (from === to) { el.textContent = to.toFixed(decimals); return; }
    const start = performance.now();
    const dur = 320;
    const step = (now) => {
      const t = Math.min(1, (now - start) / dur);
      const e = 1 - Math.pow(1 - t, 3);
      el.textContent = (from + (to - from) * e).toFixed(decimals);
      if (t < 1) requestAnimationFrame(step);
    };
    requestAnimationFrame(step);
  },

  setBusy(btn, busy) {
    if (!btn) return;
    btn.disabled = busy;
    const sp = btn.querySelector(".spinner");
    if (sp) sp.hidden = !busy;
  },

  store: {
    get(key) { try { return localStorage.getItem(key); } catch (e) { return null; } },
    set(key, v) { try { localStorage.setItem(key, v); } catch (e) { /* storage blocked */ } },
    del(key) { try { localStorage.removeItem(key); } catch (e) { /* storage blocked */ } },
  },

  // sim minutes since start -> "HH:MM" (sim starts 08:00)
  simClock(minutes) {
    const total = Math.floor(8 * 60 + minutes) % (24 * 60);
    return `${String(Math.floor(total / 60)).padStart(2, "0")}:${String(total % 60).padStart(2, "0")}`;
  },
};

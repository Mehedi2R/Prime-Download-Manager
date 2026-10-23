// Prime Download Manager - Browser Extension Popup Controller
const PRIME_BASE_URL = "http://127.0.0.1:41245";

function logPopupError(tag, message, details = null) {
  const errorObj = {
    source: "EXTENSION-POPUP",
    tag: String(tag),
    level: "ERROR",
    message: String(message),
    details: details ? (typeof details === "object" ? (details.stack || JSON.stringify(details)) : String(details)) : null
  };
  console.error(`[PDM-POPUP:${tag}]`, message, details);
  try {
    fetch(`${PRIME_BASE_URL}/log_error`, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(errorObj)
    }).catch(() => {});
  } catch (_) {}
}

window.addEventListener("error", (e) => {
  logPopupError("WINDOW-ERROR", e.message, `${e.filename}:${e.lineno}`);
});
window.addEventListener("unhandledrejection", (e) => {
  logPopupError("UNHANDLED-PROMISE", e.reason?.message || e.reason, e.reason?.stack);
});

// UI Elements
const inputFilename = document.getElementById("input-filename");
const fileIcon = document.getElementById("file-icon");
const fileBadge = document.getElementById("file-badge");
const selectCategory = document.getElementById("select-category");
const cardRight = document.getElementById("card-right");
const lblRightCard = document.getElementById("lbl-right-card");
const lblFilesize = document.getElementById("lbl-filesize");
const selectQuality = document.getElementById("select-quality");
const inputUrl = document.getElementById("input-url");
const inputSavePath = document.getElementById("input-save-path");
const referrerText = document.getElementById("referrer-text");
const btnStart = document.getElementById("btn-start");
const btnLater = document.getElementById("btn-later");
const btnCancel = document.getElementById("btn-cancel");
const btnClose = document.getElementById("btn-close");
const notificationBanner = document.getElementById("notification-banner");

let isAppOnline = false;
let isVideoMode = false;
let currentFileSize = 0;
let probeDebounceTimer = null;

function showNotification(msg, type = "success") {
  if (!notificationBanner) return;
  notificationBanner.className = `notification ${type}`;
  notificationBanner.innerText = msg;
}

function formatBytes(bytes) {
  if (!bytes || bytes <= 0) return "Calculating size...";
  const k = 1024;
  const sizes = ["Bytes", "KB", "MB", "GB", "TB"];
  const i = Math.floor(Math.log(bytes) / Math.log(k));
  return `${parseFloat((bytes / Math.pow(k, i)).toFixed(2))} ${sizes[i]}`;
}

function getFormatInfo(filename, url) {
  const target = ((filename || url || "").split("?")[0].split("#")[0]).toLowerCase();
  const ext = target.split(".").pop();

  if (["exe", "msi", "bat", "cmd", "apk", "appx", "appimage", "bin"].includes(ext)) {
    return { icon: "⚙", cat: "Programs", ext: ext.toUpperCase() };
  }
  if (["zip", "rar", "7z", "tar", "gz", "tgz", "bz2", "xz", "iso", "img", "dmg"].includes(ext)) {
    return { icon: "📦", cat: "Compressed", ext: ext.toUpperCase() };
  }
  if (["pdf", "doc", "docx", "xls", "xlsx", "ppt", "pptx", "txt", "csv", "epub", "rtf", "odt"].includes(ext)) {
    return { icon: "📄", cat: "Documents", ext: ext.toUpperCase() };
  }
  if (["mp4", "mkv", "webm", "avi", "mov", "flv", "wmv", "ts", "m4v"].includes(ext)) {
    return { icon: "🎬", cat: "Videos", ext: ext.toUpperCase() };
  }
  if (["mp3", "m4a", "wav", "flac", "aac", "ogg", "wma", "opus"].includes(ext)) {
    return { icon: "🎵", cat: "Music", ext: ext.toUpperCase() };
  }
  if (["jpg", "jpeg", "png", "webp", "gif", "svg", "bmp", "ico", "avif"].includes(ext)) {
    return { icon: "🖼", cat: "Images", ext: ext.toUpperCase() };
  }
  return { icon: "📁", cat: "Downloads", ext: (ext && ext.length <= 5) ? ext.toUpperCase() : "FILE" };
}

function isVideoPortal(url) {
  const u = (url || "").toLowerCase();
  return u.includes("youtube.com") || 
         u.includes("youtu.be") || 
         u.includes("facebook.com") || 
         u.includes("fb.watch") || 
         u.includes("instagram.com") || 
         u.includes("tiktok.com") || 
         u.includes("vimeo.com") || 
         u.includes("twitter.com") || 
         u.includes("x.com");
}

function setUiMode(isVideo) {
  isVideoMode = isVideo;
  if (isVideo) {
    lblRightCard.innerText = "Resolution / Quality";
    lblFilesize.style.display = "none";
    selectQuality.style.display = "block";
    selectCategory.value = "Videos";
  } else {
    lblRightCard.innerText = "File Size";
    lblFilesize.style.display = "block";
    selectQuality.style.display = "none";
  }
}

// 1. Check connection with Desktop Engine
async function checkDesktopConnection() {
  try {
    const controller = new AbortController();
    const timeout = setTimeout(() => controller.abort(), 1200);
    const res = await fetch(`${PRIME_BASE_URL}/ping`, { method: "GET", signal: controller.signal });
    clearTimeout(timeout);
    if (res.ok) {
      isAppOnline = true;
      return true;
    }
  } catch (_) {}
  isAppOnline = false;
  return false;
}

// 2. Probe URL information
async function probeTargetUrl(url, initialFilename = "", passedSize = 0) {
  if (!url || !url.startsWith("http")) return;

  const isVid = isVideoPortal(url) || /\.(mp4|mkv|webm|avi|mov|flv|wmv|ts|m4v)(\?|$)/i.test(url);
  setUiMode(isVid);

  const fmt = getFormatInfo(initialFilename, url);
  if (fileIcon) fileIcon.innerText = fmt.icon;
  if (fileBadge) fileBadge.innerText = fmt.ext;
  if (selectCategory) selectCategory.value = fmt.cat;

  // Extract clean filename if empty
  if (inputFilename && !inputFilename.value) {
    if (initialFilename) {
      inputFilename.value = initialFilename;
    } else {
      try {
        const u = new URL(url);
        const seg = u.pathname.split("/").filter(Boolean).pop();
        inputFilename.value = seg ? decodeURIComponent(seg) : "downloaded_file";
      } catch (_) {
        inputFilename.value = "downloaded_file";
      }
    }
  }

  // Update extension badge on filename input
  const curFmt = getFormatInfo(inputFilename.value, url);
  if (fileBadge) fileBadge.innerText = curFmt.ext;
  if (!isVid && selectCategory) selectCategory.value = curFmt.cat;

  // File size handling
  if (passedSize > 0) {
    currentFileSize = passedSize;
    if (lblFilesize) lblFilesize.innerText = formatBytes(passedSize);
  } else if (!isVid) {
    if (lblFilesize) lblFilesize.innerText = "Calculating size...";
  }

  if (!isAppOnline) return;

  try {
    const res = await fetch(`${PRIME_BASE_URL}/probe`, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ url, quality: "720p" })
    });

    if (res.ok) {
      const meta = await res.json();
      if (meta.file_name && !inputFilename.value) {
        inputFilename.value = meta.file_name;
        const updFmt = getFormatInfo(meta.file_name, url);
        if (fileBadge) fileBadge.innerText = updFmt.ext;
        if (!isVid && selectCategory) selectCategory.value = updFmt.cat;
      }

      if (isVid) {
        const qualities = meta?.available_qualities || meta?.data?.available_qualities || [];
        if (qualities.length > 0) {
          selectQuality.innerHTML = "";
          qualities.forEach(q => {
            const opt = document.createElement("option");
            opt.value = q.value;
            opt.innerText = q.label;
            if (q.is_default) opt.selected = true;
            selectQuality.appendChild(opt);
          });
        }
      } else {
        if (meta.total_size > 0) {
          currentFileSize = meta.total_size;
          if (lblFilesize) lblFilesize.innerText = formatBytes(meta.total_size);
        }
      }
    }
  } catch (_) {
    if (!isVid && currentFileSize <= 0) {
      // Fallback fast HEAD probe
      try {
        fetch(url, { method: "HEAD" }).then(r => {
          const cl = r.headers.get("Content-Length");
          if (cl) {
            const sz = parseInt(cl, 10);
            if (sz > 0) {
              currentFileSize = sz;
              if (lblFilesize) lblFilesize.innerText = formatBytes(sz);
              return;
            }
          }
          if (lblFilesize) lblFilesize.innerText = "Unknown Size";
        }).catch(() => {
          if (lblFilesize) lblFilesize.innerText = "Unknown Size";
        });
      } catch (_) {}
    }
  }
}

// 3. Initialize popup
async function initPopup() {
  await checkDesktopConnection();

  let initialUrl = "";
  let initialFilename = "";
  let initialReferrer = "";
  let initialSize = 0;

  try {
    const params = new URLSearchParams(window.location.search);
    initialUrl = params.get("url") || "";
    initialFilename = params.get("filename") || "";
    initialReferrer = params.get("referrer") || "";
    initialSize = parseInt(params.get("size") || "0", 10);
  } catch (_) {}

  if (initialUrl) {
    inputUrl.value = initialUrl;
    if (initialFilename) inputFilename.value = initialFilename;
    if (initialReferrer) {
      try {
        const host = new URL(initialReferrer).hostname;
        referrerText.innerText = `Initiated from: ${host}`;
      } catch (_) {
        referrerText.innerText = `Initiated from: ${initialReferrer}`;
      }
    }
    probeTargetUrl(initialUrl, initialFilename, initialSize);
    return;
  }

  // Check pending intercepted download in storage
  if (typeof chrome !== "undefined" && chrome.storage && chrome.storage.local) {
    chrome.storage.local.get(["pending_download_url", "pending_download_filename", "pending_download_time"], (result) => {
      const now = Date.now();
      if (result && result.pending_download_url && (now - (result.pending_download_time || 0) < 60000)) {
        inputUrl.value = result.pending_download_url;
        inputFilename.value = result.pending_download_filename || "";
        probeTargetUrl(result.pending_download_url, result.pending_download_filename, 0);
        chrome.storage.local.remove(["pending_download_url", "pending_download_filename", "pending_download_time"]);
        return;
      }
      queryActiveTab();
    });
  } else {
    queryActiveTab();
  }
}

function queryActiveTab() {
  if (typeof chrome !== "undefined" && chrome.tabs && chrome.tabs.query) {
    chrome.tabs.query({ active: true, currentWindow: true }, (tabs) => {
      if (tabs && tabs[0] && tabs[0].url) {
        const activeUrl = tabs[0].url;
        if (activeUrl.startsWith("http://") || activeUrl.startsWith("https://")) {
          inputUrl.value = activeUrl;
          try {
            referrerText.innerText = `Initiated from: ${new URL(activeUrl).hostname}`;
          } catch (_) {}
          probeTargetUrl(activeUrl);
        }
      }
    });
  }
}

// Event Listeners
inputUrl.addEventListener("input", (e) => {
  clearTimeout(probeDebounceTimer);
  const val = e.target.value.trim();
  probeDebounceTimer = setTimeout(() => {
    probeTargetUrl(val, inputFilename.value);
  }, 400);
});

inputFilename.addEventListener("input", (e) => {
  const fmt = getFormatInfo(e.target.value, inputUrl.value);
  if (fileIcon) fileIcon.innerText = fmt.icon;
  if (fileBadge) fileBadge.innerText = fmt.ext;
  if (!isVideoMode && selectCategory) selectCategory.value = fmt.cat;
});

btnCancel.addEventListener("click", () => window.close());
btnClose.addEventListener("click", () => window.close());

async function triggerDownload(startNow) {
  const url = inputUrl.value.trim();
  if (!url || !url.startsWith("http")) {
    showNotification("Please enter a valid HTTP/HTTPS download link!", "error");
    inputUrl.focus();
    return;
  }

  const quality = isVideoMode ? selectQuality.value : "best";
  const threads = 8;
  const savePath = inputSavePath.value.trim();
  const title = inputFilename.value.trim();

  btnStart.disabled = true;
  btnLater.disabled = true;

  try {
    const payload = {
      url,
      save_path: savePath || undefined,
      threads,
      quality,
      title: title || undefined,
      start_now: startNow
    };

    const res = await fetch(`${PRIME_BASE_URL}/download`, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(payload)
    });

    if (res.ok) {
      const data = await res.json();
      showNotification(startNow ? "🚀 Download Started!" : "📋 Added to Download Queue!", "success");
      setTimeout(() => {
        window.close();
      }, 900);
      return;
    } else {
      throw new Error(`Server returned ${res.status}`);
    }
  } catch (err) {
    logPopupError("TRIGGER-DOWNLOAD", `Failed to send download to desktop: ${err.message}`, { url });
    try {
      await navigator.clipboard.writeText(url);
      showNotification("📋 Copied URL! Open Prime Download Manager.", "success");
    } catch (_) {
      showNotification("Desktop application offline. Please launch Prime Download Manager.", "error");
    }
    btnStart.disabled = false;
    btnLater.disabled = false;
  }
}

btnStart.addEventListener("click", () => triggerDownload(true));
btnLater.addEventListener("click", () => triggerDownload(false));

// Run init
initPopup();

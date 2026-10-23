const PRIME_DESKTOP_ENDPOINT = "http://127.0.0.1:41245/download";
const PRIME_LOG_ENDPOINT = "http://127.0.0.1:41245/log_error";

// Centralized Extension Error Logging
function logExtensionError(tag, message, details = null) {
  const errorObj = {
    source: "EXTENSION",
    tag: String(tag),
    level: "ERROR",
    message: String(message),
    details: details ? (typeof details === "object" ? (details.stack || JSON.stringify(details)) : String(details)) : null
  };

  // 1. Extension Service Worker Console
  console.error(`[PDM-EXT:${tag}]`, message, details);

  // 2. Buffer in chrome.storage.local (stores last 100 error logs)
  try {
    chrome.storage.local.get(["pdm_extension_errors"], (res) => {
      const list = res.pdm_extension_errors || [];
      list.unshift({ ...errorObj, timestamp: new Date().toISOString() });
      if (list.length > 100) list.length = 100;
      chrome.storage.local.set({ pdm_extension_errors: list });
    });
  } catch (_) {}

  // 3. Post to Desktop Backend Error Log File
  try {
    fetch(PRIME_LOG_ENDPOINT, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(errorObj)
    }).catch(() => {});
  } catch (_) {}
}

// Global Exception Handlers for Extension Background Service Worker
self.addEventListener("unhandledrejection", (event) => {
  logExtensionError("UNHANDLED-PROMISE", event.reason?.message || event.reason, event.reason?.stack);
});
self.addEventListener("error", (event) => {
  logExtensionError("UNCAUGHT-ERROR", event.message, `${event.filename}:${event.lineno}:${event.colno}`);
});

// 1. GENERAL FILE & LINK CONTEXT MENU (NON-VIDEO FORMATS)
// Video downloads are handled exclusively via the floating PDM grabber overlay button.
// The browser right-click context menu is dedicated strictly to non-video formats:
// direct file links (zip, exe, rar, 7z, pdf, doc, apk, etc.), images, audio, documents, and page links.
function safeCreateMenu(options, callback) {
  try {
    chrome.contextMenus.create(options, () => {
      if (chrome.runtime.lastError) {}
      if (typeof callback === "function") callback();
    });
  } catch (_) {
    if (typeof callback === "function") callback();
  }
}

function registerGeneralContextMenus() {
  if (!chrome.contextMenus) return;
  chrome.contextMenus.removeAll(() => {
    if (chrome.runtime.lastError) {}

    // Exactly 1 top-level menu item: completely prevents subfolder nesting & duplication
    safeCreateMenu({
      id: "pdm_download_this",
      title: "Download this with PDM",
      contexts: ["all"]
    });
  });
}

chrome.runtime.onInstalled.addListener(registerGeneralContextMenus);
chrome.runtime.onStartup.addListener(registerGeneralContextMenus);

// Handle Non-Video Context Menu Clicks with Accurate Server URL Analysis
if (chrome.contextMenus && chrome.contextMenus.onClicked) {
  chrome.contextMenus.onClicked.addListener((info, tab) => {
    const pageUrl = tab?.url || info.pageUrl || "";
    let targetUrl = "";

    const isDirectFileLink = (u) => {
      if (!u) return false;
      const clean = u.split('?')[0].split('#')[0].toLowerCase();
      return /\.(zip|rar|7z|tar|gz|bz2|xz|exe|msi|iso|apk|dmg|pkg|deb|rpm|pdf|doc|docx|xls|xlsx|ppt|pptx|txt|csv|torrent|mp3|wav|flac|aac|m4a|ogg|mp4|mkv|webm|avi|mov|jpg|jpeg|png|webp|gif|svg|bmp)$/i.test(clean);
    };

    // 1. Right-clicked on an image, audio, or media element
    if (info.srcUrl && !info.srcUrl.startsWith("blob:") && !info.srcUrl.startsWith("data:")) {
      // If the media element is wrapped in a link that points directly to a downloadable file (e.g. <a href="installer.exe"><img src="icon.png"></a>)
      if (info.linkUrl && isDirectFileLink(info.linkUrl)) {
        targetUrl = info.linkUrl;
      } else {
        // Otherwise, user right-clicked an image/media element: download the real server media file!
        targetUrl = info.srcUrl;
      }
    }
    // 2. Right-clicked on a link
    else if (info.linkUrl && !info.linkUrl.startsWith("javascript:") && !info.linkUrl.startsWith("#")) {
      targetUrl = info.linkUrl;
    }
    // 3. Highlighted text containing a URL
    else if (info.selectionText) {
      const trimmed = info.selectionText.trim();
      if (trimmed.startsWith("http://") || trimmed.startsWith("https://")) {
        targetUrl = trimmed;
      }
    }
    // 4. Page URL fallback
    else if (pageUrl && (pageUrl.startsWith("http://") || pageUrl.startsWith("https://"))) {
      targetUrl = pageUrl;
    }

    if (!targetUrl) return;

    // Send directly to PDM Desktop for server URL probing and formatted file download
    sendDownloadToPrime(targetUrl, "", 0, pageUrl, null, "best");
  });
}

// 2. BACKGROUND KEEP-ALIVE: Prevents background service worker from going to sleep while streaming
try {
  chrome.alarms.create("prime_keep_alive", { periodInMinutes: 0.5 });
  chrome.alarms.onAlarm.addListener((alarm) => {
    if (alarm.name === "prime_keep_alive") {
      // Periodic keep-alive ping
    }
  });
} catch (_) {}

// 3. REAL-TIME NETWORK MEDIA STREAM SNIFFER (Captures true .m3u8, .mpd, .mp4, and CDN video endpoints)
const tabCapturedMedia = new Map();

if (chrome.tabs) {
  try {
    if (chrome.tabs.onUpdated) {
      chrome.tabs.onUpdated.addListener((tabId, changeInfo) => {
        if (changeInfo.url && tabCapturedMedia.has(tabId)) {
          tabCapturedMedia.set(tabId, []);
        }
      });
    }
    if (chrome.tabs.onRemoved) {
      chrome.tabs.onRemoved.addListener((tabId) => {
        tabCapturedMedia.delete(tabId);
      });
    }
  } catch (_) {}
}

function isMediaStreamUrl(url, type, mime) {
  if (!url || typeof url !== "string") return false;
  const u = url.toLowerCase();
  
  if (u.startsWith("blob:") || u.startsWith("data:") || u.startsWith("javascript:")) return false;
  if (u.includes("favicon") || u.includes("analytics") || u.includes("doubleclick") || u.includes(".svg")) return false;

  // STRICT REJECTION: Reject all web images, avatars, static web assets
  if (type === "image" || type === "stylesheet" || type === "script" || type === "font") return false;
  if (mime) {
    const m = mime.toLowerCase();
    if (m.startsWith("image/") || m.includes("text/css") || m.includes("javascript")) return false;
  }
  if (u.includes(".image") || u.includes(".jpg") || u.includes(".jpeg") || u.includes(".png") ||
      u.includes(".webp") || u.includes(".gif") || u.includes(".ico") || u.includes(".bmp") ||
      u.includes(".avif") || u.includes(".css") || u.includes(".js") || u.includes(".woff") ||
      u.includes(".ttf") || u.includes(".map")) {
    return false;
  }

  // 1. Manifests / Playlists (Top priority - master playlist containing all resolutions)
  if (u.includes(".m3u8") || u.includes(".mpd") || u.includes("manifest.mpd") || u.includes("playlist.m3u8") || u.includes("/manifest(") || u.includes("master.m3u8")) {
    return { kind: "manifest", ext: u.includes(".mpd") ? "mpd" : "m3u8" };
  }

  // 2. Direct Video / Audio streams
  if (u.includes(".mp4") || u.includes(".webm") || u.includes(".m4v") || u.includes(".m4s") || u.includes(".ts") || u.includes(".m4a")) {
    return { kind: "direct", ext: "mp4" };
  }

  // 3. Known CDN video streaming paths (LinkedIn, Reddit, Meta, Dailymotion, Twitter)
  if (u.includes("dms.licdn.com") || u.includes("v.redd.it") || u.includes("video.twimg.com") || 
      u.includes("cdninstagram.com") || u.includes("fbcdn.net") || u.includes("dmcdn.net") || 
      u.includes("googlevideo.com/videoplayback")) {
    return { kind: "cdn", ext: "mp4" };
  }

  // TikTok video CDN: strictly match genuine video streams only (v16, v19, v77, /video/, /play/)
  if (u.includes("tiktokcdn.com") || u.includes("akamaized.net") || u.includes("byteoversea.com") || u.includes("ibytedtos.com")) {
    if (u.includes("/video/") || u.includes("/play/") || u.includes("v16") || u.includes("v19") || u.includes("v77") || (mime && mime.toLowerCase().includes("video/"))) {
      return { kind: "cdn", ext: "mp4" };
    }
    return false;
  }

  // 4. Response MIME type checks
  if (mime) {
    const m = mime.toLowerCase();
    if (m.includes("video/") || m.includes("application/x-mpegurl") || m.includes("application/vnd.apple.mpegurl") || m.includes("application/dash+xml")) {
      return { kind: "mime", ext: m.includes("mpegurl") ? "m3u8" : (m.includes("dash") ? "mpd" : "mp4") };
    }
  }

  return false;
}

function cleanMetaUrl(url) {
  if (!url || typeof url !== "string") return url;
  if (url.includes("cdninstagram.com") || url.includes("fbcdn.net") || url.includes("scontent")) {
    try {
      const u = new URL(url);
      let changed = false;
      if (u.searchParams.has("bytestart")) {
        u.searchParams.delete("bytestart");
        changed = true;
      }
      if (u.searchParams.has("byteend")) {
        u.searchParams.delete("byteend");
        changed = true;
      }
      if (changed) return u.toString();
    } catch (_) {}
    return url.replace(/[?&]bytestart=\d+/g, '').replace(/[?&]byteend=\d+/g, '').replace(/\?&/, '?');
  }
  return url;
}

function decodeBase64Safe(str) {
  if (!str || typeof str !== "string") return "";
  try {
    let b64 = decodeURIComponent(str).replace(/-/g, '+').replace(/_/g, '/');
    while (b64.length % 4) b64 += '=';
    return atob(b64);
  } catch (_) {
    return "";
  }
}

function isMetaAudioUrl(u, mime = "") {
  if (!u || typeof u !== "string") return false;
  if (mime && (mime.startsWith("audio/") || mime.includes("audio"))) return true;
  const lower = u.toLowerCase();
  if (lower.includes("_audio") || lower.includes("dash_audio") || lower.includes("dash_ln_heaac") ||
      lower.includes("heaac") || lower.includes("dash_aac") || lower.includes("dash_base_aac") ||
      lower.includes("audio_representation") || lower.includes("/audio/") || lower.endsWith(".m4a") ||
      lower.includes(".m4a?") || lower.includes("audio_dashinit")) {
    return true;
  }
  try {
    const m = u.match(/[?&]efg=([^&]+)/);
    if (m && m[1]) {
      const decoded = decodeBase64Safe(m[1]).toLowerCase();
      if (decoded.includes("audio") || decoded.includes("heaac") || decoded.includes("aac")) {
        return true;
      }
    }
  } catch (_) {}
  return false;
}

function recordCapturedMedia(tabId, url, info, size = 0, mime = "") {
  if (!url || typeof url !== "string") return;
  url = cleanMetaUrl(url);
  if (!tabCapturedMedia.has(tabId)) {
    tabCapturedMedia.set(tabId, []);
  }
  const list = tabCapturedMedia.get(tabId);
  const existing = list.find(item => item.url === url);
  if (existing) {
    if (size > 0 && !existing.size) existing.size = size;
    if (mime && !existing.mime) existing.mime = mime;
    return;
  }
  const isSegment = url.includes(".ts") || url.includes(".m4s");
  const isAudio = isMetaAudioUrl(url, mime);
  const entry = {
    url,
    kind: info.kind,
    ext: info.ext,
    isSegment,
    isAudio,
    isVideo: !isAudio,
    mime: mime || "",
    size,
    timestamp: Date.now()
  };

  if (info.kind === "manifest") {
    list.unshift(entry);
  } else if (!isSegment) {
    const firstSegmentIdx = list.findIndex(i => i.isSegment);
    if (firstSegmentIdx >= 0) {
      list.splice(firstSegmentIdx, 0, entry);
    } else {
      list.push(entry);
    }
  } else {
    list.push(entry);
  }

  if (list.length > 30) {
    list.pop();
  }
}

// Non-blocking network sniffer listeners (Manifest V3 compliant, 0 overhead)
if (chrome.webRequest) {
  try {
    chrome.webRequest.onBeforeRequest.addListener(
      (details) => {
        if (details.tabId < 0) return;
        const mediaInfo = isMediaStreamUrl(details.url, details.type);
        if (mediaInfo) {
          recordCapturedMedia(details.tabId, details.url, mediaInfo);
        }
      },
      { urls: ["<all_urls>"] }
    );

    if (chrome.webRequest.onResponseStarted) {
      chrome.webRequest.onResponseStarted.addListener(
        (details) => {
          if (details.tabId < 0) return;
          let mime = "";
          let contentLength = 0;
          if (details.responseHeaders) {
            for (const h of details.responseHeaders) {
              const name = h.name.toLowerCase();
              if (name === "content-type") mime = h.value;
              else if (name === "content-range") {
                const m = (h.value || "").match(/\/(\d+)/);
                if (m && m[1]) contentLength = Math.max(contentLength, parseInt(m[1], 10) || 0);
              } else if (name === "content-length") {
                const len = parseInt(h.value, 10) || 0;
                if (!contentLength) contentLength = len;
              }
            }
          }
          const mediaInfo = isMediaStreamUrl(details.url, details.type, mime);
          if (mediaInfo) {
            recordCapturedMedia(details.tabId, details.url, mediaInfo, contentLength, mime);
          }
        },
        { urls: ["<all_urls>"] },
      );
    }
  } catch (_) {}
}

const urlRequestHeaders = new Map();
const interceptedDownloadIds = new Set();
const recentlyHandledUrls = new Map();
const bypassedDownloadUrls = new Map();

if (chrome.webRequest && chrome.webRequest.onBeforeSendHeaders) {
  const saveRequestHeaders = (details) => {
    if (!details.url) return;
    const headers = {};
    if (details.requestHeaders) {
      for (const h of details.requestHeaders) {
        if (h.name && h.value) {
          headers[h.name.toLowerCase()] = h.value;
        }
      }
    }
    urlRequestHeaders.set(details.url, {
      headers,
      referrer: details.initiator || headers["referer"] || "",
      tabId: details.tabId,
      timestamp: Date.now()
    });

    if (urlRequestHeaders.size > 250) {
      const oldestKey = urlRequestHeaders.keys().next().value;
      urlRequestHeaders.delete(oldestKey);
    }
  };

  try {
    chrome.webRequest.onBeforeSendHeaders.addListener(
      saveRequestHeaders,
      { urls: ["<all_urls>"] },
      ["requestHeaders", "extraHeaders"]
    );
  } catch (_) {
    try {
      chrome.webRequest.onBeforeSendHeaders.addListener(
        saveRequestHeaders,
        { urls: ["<all_urls>"] },
        ["requestHeaders"]
      );
    } catch (_) {}
  }
}

// Helper to convert MIME type to file extension
function mimeToExt(mime) {
  if (!mime || typeof mime !== "string") return "";
  const m = mime.toLowerCase();
  if (m.includes("x-msdownload") || m.includes("portable-executable") || m.includes("x-msdos-program") || m.includes("x-executable")) return "exe";
  if (m.includes("x-msi") || m.includes("ms-installer")) return "msi";
  if (m.includes("zip")) return "zip";
  if (m.includes("x-7z")) return "7z";
  if (m.includes("x-rar") || m.includes("vnd.rar")) return "rar";
  if (m.includes("x-tar")) return "tar";
  if (m.includes("gzip") || m.includes("x-gzip")) return "gz";
  if (m.includes("pdf")) return "pdf";
  if (m.includes("msword")) return "doc";
  if (m.includes("wordprocessingml")) return "docx";
  if (m.includes("vnd.ms-excel")) return "xls";
  if (m.includes("spreadsheetml")) return "xlsx";
  if (m.includes("presentationml") || m.includes("powerpoint")) return "pptx";
  if (m.includes("text/plain")) return "txt";
  if (m.includes("text/csv")) return "csv";
  if (m.includes("video/mp4")) return "mp4";
  if (m.includes("video/webm")) return "webm";
  if (m.includes("video/x-matroska")) return "mkv";
  if (m.includes("video/quicktime")) return "mov";
  if (m.includes("audio/mpeg") || m.includes("audio/mp3")) return "mp3";
  if (m.includes("audio/mp4") || m.includes("audio/aac")) return "m4a";
  if (m.includes("audio/wav")) return "wav";
  if (m.includes("audio/flac")) return "flac";
  if (m.includes("image/jpeg")) return "jpg";
  if (m.includes("image/png")) return "png";
  if (m.includes("image/webp")) return "webp";
  if (m.includes("image/gif")) return "gif";
  if (m.includes("image/svg")) return "svg";
  if (m.includes("iso9660")) return "iso";
  if (m.includes("android.package-archive")) return "apk";
  return "";
}

const BLOCKED_SITES = [
  "*.update.microsoft.com",
  "download.windowsupdate.com",
  "*.download.windowsupdate.com",
  "siteseal.thawte.com",
  "ecom.cimetz.com",
  "*.voice2page.com"
];

const ADDRESS_EXCEPTIONS = [
  "http://*.akamaihd.net/*.mp3",
  "http://*.akamaihd.net/*.zip",
  "http://*.appspot.com/*/audio/*.mp3",
  "http://*.appspot.com/*/Sounds/*",
  "http://*.ask.com/*toolbar/*config*.zip",
  "http://*.cloudfront.net/game/*/res/*.bin",
  "http://*.cloudfront.net/game/*/res/*.zip",
  "http://*.cox.com/*.gz",
  "http://*.download.windowsupdate.com/*",
  "http://*.farmville.com/*.gz",
  "http://*.farmville.com/*/sounds/*.mp3",
  "http://*.gstatic.com/*/sounds/*.mp3",
  "http://*.internetdownloadmanager.com/updates/*/idmupdt.exe*",
  "http://*.meebo.com/*skin/*/sound/*.mp3",
  "http://*.skyrock.com/*.img",
  "http://*.skyrock.net/*.img",
  "http://*.teletalk.com.bd/admitcard/card.php",
  "http://*.vk.com/*.zip",
  "http://*.vkontakte.ru/*.zip",
  "http://*.wikifortio.com/node-fs/upload/*",
  "http://*.zgncdn.com/*.gz",
  "http://*.zgncdn.com/*.mp3",
  "http://*.zgncdn.com/*.z",
  "http://*.zynga.com/*.mp3",
  "http://6rbmasr.egyptany.net/*.ram",
  "http://aa.voice2page.com/*",
  "http://ad.*.com/st?ad_type=*",
  "http://ad.*.net/st?ad_type=*",
  "http://ad.yieldmanager.com/st?*",
  "http://ak.imgfarm.com/images/download/spokesperson/html5/audio/*.mp3",
  "http://api.browser.ya.ru/configs/all_zip?*",
  "http://assets.*.zynga.com/*.zip",
  "http://ccbar.ask.com/images/*.bin",
  "http://cdmdownload.adobe.com/firefox/*primetime*.zip",
  "http://cdn.engine.*/pu.pdf",
  "http://ciscobinary.openh264.org/*.zip",
  "http://counters.gigya.com/wildfire/counters/*=.tif",
  "http://dar.youknowbest.com/Resources/*.img",
  "http://get*.opera.com/pub/opera/*/autoupdate/*.exe",
  "http://get*.opera.com/pub/opera/*/autoupdate/*.msi",
  "http://get.geo.opera.com/res/servicefiles/*.zip",
  "http://images.apple.com/*/*.mov",
  "http://img.imgsmail.ru/*/message.bin",
  "http://img.tfd.com/hm/mp3/*.mp3",
  "http://imgfarm.com/images/*.bin",
  "http://mail.yimg.com/*/i/us/pim/*.mp3",
  "http://movies.apple.com/*/*.mov",
  "http://msedge.b.tlu.dl.delivery.mp.microsoft.com/filestreamingservice/files/*",
  "http://o.aolcdn.com/cdn.webmail.aol.com/*/aol/*/sounds/*.mp3",
  "http://quickaccess-dl.maxthon.com/quickaccess_*.dat*",
  "http://static.ak.fbcdn.net/*.mp3",
  "http://static.ak.fbcdn.net/*.ogg",
  "http://static.playfish.com/game/*.bin",
  "http://statics.verycd.com/javascripts/*.js.r*",
  "http://toolbar.live.com/static/sm/*.bin",
  "http://village*.amazonaws.com/static/sound/*.mp3",
  "http://widget*.meebo.com/*.mp3",
  "http://www.6rb.com/*.ram",
  "http://www.6rbtop.com/library/resources/*.ram",
  "http://www.cbox.ws/box/click*.wav",
  "http://www.nseries.com/nseries/*.mp3",
  "http://www.slimjetbrowser.com/release/*_console.exe",
  "http://www.sonyericsson.com/origin/images/content/*.exe",
  "http://x.myspacecdn.com/modules/webim/static/audio/*.mp3",
  "https://*.akamaihd.net/*.gz",
  "https://*.akamaihd.net/*.mp3",
  "https://*.gstatic.com/*/sounds/*.mp3",
  "https://*.gvt1.com/edgedl/widevine-cdm/*win*.zip?*",
  "https://*.internetdownloadmanager.com/updates/*/idmupdt.exe*",
  "https://*.nic.in/*/*/*.aspx",
  "https://*.nic.in/*/*/*.php",
  "https://ak.imgfarm.com/images/download/spokesperson/html5/audio/*.mp3",
  "https://cdmdownload.adobe.com/firefox/*primetime*.zip",
  "https://cdn.thegameawards.com/frontend/video/tga*.mp4",
  "https://compiles.overleaf.com/project/*/output/output.pdf?*",
  "https://fpdownload.macromedia.com/get/*.z",
  "https://i3j3u3u9.ssl.hwcdn.net/common/sounds/interval.mp3",
  "https://img6.wsimg.com/serpimg/img/solutionsimg/*.mp4",
  "https://lookaside.fbsbx.com/file/*.pdf?token=*",
  "https://pc.xuexi.cn/points/study-login/asset/*.mp3",
  "https://redirector.gvt1.com/edgedl/widevine-cdm/*.zip",
  "https://sso-f.orange.fr/omoi_erb/facture/v1.0/pdf?*",
  "https://swx.cdn.skype.com/assets/*/audio/*.m4a",
  "https://swx.cdn.skype.com/assets/*/audio/*.ogg",
  "https://web.whatsapp.com/res/*.mp3",
  "https://www.bing.com/api/shopping/v1/ux/images/autoApply/*.mp4",
  "https://www.syssel.net/hoefs/software_count.php",
  "https://www.youtube.com/s/search/audio/*.mp3"
];

function isBlockedUrlPattern(pattern, url) {
  if (!pattern || !url) return false;
  const p = pattern.trim();
  const u = url.trim();

  // If pattern has no scheme (e.g. *.update.microsoft.com)
  if (!p.includes("://")) {
    try {
      const parsed = new URL(u);
      const host = parsed.hostname;
      const rxHost = new RegExp("^" + p.replace(/[-[\]{}()+?.,\\^$|#\s]/g, "\\$&").replace(/\*/g, ".*") + "$", "i");
      if (rxHost.test(host)) return true;
    } catch (_) {}
  }

  // Full URL match with wildcard
  let rxStr = "^" + p.replace(/[-[\]{}()+.,\\^$|#\s]/g, "\\$&").replace(/\*/g, ".*").replace(/\?/g, "\\?");
  if (!p.endsWith("*")) {
    rxStr += "(\\?.*)?";
  }
  rxStr += "$";
  try {
    const rx = new RegExp(rxStr, "i");
    return rx.test(u);
  } catch (_) {
    return false;
  }
}

function isUrlBlocked(url) {
  if (!url) return false;
  for (const s of BLOCKED_SITES) {
    if (isBlockedUrlPattern(s, url)) return true;
  }
  for (const a of ADDRESS_EXCEPTIONS) {
    if (isBlockedUrlPattern(a, url)) return true;
  }
  return false;
}

// Helper to check if a download should be intercepted by Prime Download Manager (Programs, Compressed, Docs, Media, Others, etc.)
function isInterceptableDownload(item) {
  if (!item || !item.url) return false;
  if (item.byExtensionId && item.byExtensionId === chrome.runtime.id) return false;
  if (isUrlBlocked(item.url)) return false;

  const urlLower = item.url.toLowerCase();
  const mimeLower = (item.mime || "").toLowerCase();
  const filenameLower = (item.filename || "").toLowerCase();

  // Exclude internal or browser protocols that desktop app cannot fetch directly
  if (urlLower.startsWith("blob:") || urlLower.startsWith("data:") || urlLower.startsWith("chrome:") || 
      urlLower.startsWith("edge:") || urlLower.startsWith("javascript:") || urlLower.startsWith("chrome-extension:") ||
      urlLower.startsWith("moz-extension:") || urlLower.startsWith("about:")) {
    return false;
  }

  // Exclude streaming playlist manifest files if they trigger download (handled by floating grabber)
  if (urlLower.includes(".m3u8") || urlLower.includes(".mpd")) {
    return false;
  }

  // Exclude web application assets, PWA manifests, and page resources
  if (urlLower.endsWith(".webmanifest") || urlLower.endsWith(".manifest") || 
      urlLower.endsWith(".js") || urlLower.endsWith(".mjs") || urlLower.endsWith(".css") || 
      urlLower.endsWith(".map") || urlLower.endsWith(".ico") ||
      urlLower.includes("/manifest.webmanifest") || urlLower.includes("/site.webmanifest") ||
      urlLower.includes("/favicon.") || urlLower.includes("/sw.js") ||
      urlLower.includes("analytics") || urlLower.includes("telemetry")) {
    return false;
  }

  if (mimeLower === "application/manifest+json" || mimeLower === "text/javascript" || 
      mimeLower === "application/javascript" || mimeLower === "text/css") {
    return false;
  }

  // Exclude ordinary webpage HTML navigations unless saved as a file download
  if (mimeLower === "text/html" && !filenameLower.endsWith(".html") && !filenameLower.endsWith(".htm")) {
    const hasDownloadExt = /\.(exe|msi|msix|zip|rar|7z|tar|gz|bz2|xz|pdf|doc|docx|xls|xlsx|ppt|pptx|apk|iso|img|bin|dmg|mp4|mkv|webm|mp3|m4a|wav)(\?|$)/i.test(urlLower) ||
                           /\.(exe|msi|msix|zip|rar|7z|tar|gz|bz2|xz|pdf|doc|docx|xls|xlsx|ppt|pptx|apk|iso|img|bin|dmg|mp4|mkv|webm|mp3|m4a|wav)$/i.test(filenameLower);
    if (!hasDownloadExt) {
      return false;
    }
  }

  return true;
}

async function probeUrlFileSize(url, headers = {}) {
  if (!url || typeof url !== "string") return 0;
  const cleanUrl = url.trim();
  if (cleanUrl.startsWith("blob:") || cleanUrl.startsWith("data:")) return 0;

  // 1. Try HEAD request
  try {
    const controller = new AbortController();
    const timeout = setTimeout(() => controller.abort(), 3500);
    const resp = await fetch(cleanUrl, {
      method: "HEAD",
      headers,
      signal: controller.signal
    });
    clearTimeout(timeout);
    const cl = resp.headers.get("Content-Length");
    if (cl) {
      const sz = parseInt(cl, 10);
      if (sz > 0) return sz;
    }
  } catch (_) {}

  // 2. Fallback to Range: bytes=0-0 GET request
  try {
    const controller = new AbortController();
    const timeout = setTimeout(() => controller.abort(), 3500);
    const resp = await fetch(cleanUrl, {
      method: "GET",
      headers: { ...headers, "Range": "bytes=0-0" },
      signal: controller.signal
    });
    clearTimeout(timeout);
    const cr = resp.headers.get("Content-Range");
    if (cr) {
      const m = cr.match(/\/(\d+)/);
      if (m && m[1]) {
        const sz = parseInt(m[1], 10);
        if (sz > 0) return sz;
      }
    }
    const cl = resp.headers.get("Content-Length");
    if (cl) {
      const sz = parseInt(cl, 10);
      if (sz > 0) return sz;
    }
  } catch (_) {}

  return 0;
}

async function routeDownloadToDesktop(item) {
  try {
    if (!item || !item.url) return;
    if (interceptedDownloadIds.has(item.id)) return;

    const now = Date.now();
    if (bypassedDownloadUrls.has(item.url) && (now - bypassedDownloadUrls.get(item.url) < 30000)) {
      bypassedDownloadUrls.delete(item.url);
      return; // Allow native browser download without intercepting
    }

    const lastTrigger = recentlyHandledUrls.get(item.url) || 0;
    if (now - lastTrigger < 3000) {
      chrome.downloads.cancel(item.id, () => {
        if (chrome.runtime.lastError) {}
        chrome.downloads.erase({ id: item.id }, () => {});
      });
      return;
    }

    if (!isInterceptableDownload(item)) {
      return;
    }

    interceptedDownloadIds.add(item.id);
    recentlyHandledUrls.set(item.url, now);
    if (recentlyHandledUrls.size > 100) {
      for (const [k, v] of recentlyHandledUrls.entries()) {
        if (now - v > 15000) recentlyHandledUrls.delete(k);
      }
    }

    // 1. Cancel Chrome native download immediately & erase from shelf
    chrome.downloads.cancel(item.id, () => {
      if (chrome.runtime.lastError) {}
      chrome.downloads.erase({ id: item.id }, () => {});
    });

    // 2. Resolve clean authentic filename and format extension
    const targetUrl = item.finalUrl || item.url;
    let filename = "";
    if (item.filename) {
      filename = item.filename.split(/[\\/]/).pop();
    }

    let urlFilename = "";
    try {
      const u = new URL(targetUrl);
      const seg = u.pathname.split("/").pop();
      if (seg) urlFilename = decodeURIComponent(seg);
    } catch (_) {}

    const getFileExt = (name) => {
      if (!name || typeof name !== "string") return "";
      const dot = name.lastIndexOf(".");
      return (dot > 0 && dot < name.length - 1) ? name.substring(dot + 1).toLowerCase() : "";
    };

    const hasAuthenticExt = (name) => {
      const ext = getFileExt(name);
      if (!ext) return false;
      const webExts = ["php", "asp", "aspx", "jsp", "cgi", "html", "htm", "action", "do", "part", "crdownload"];
      return !webExts.includes(ext);
    };

    if (!hasAuthenticExt(filename)) {
      if (hasAuthenticExt(urlFilename)) {
        if (!filename || filename === "download" || filename.length <= 5 || urlFilename.toLowerCase().includes(filename.toLowerCase())) {
          filename = urlFilename;
        } else {
          filename = filename + "." + getFileExt(urlFilename);
        }
      } else if (item.mime) {
        const mExt = mimeToExt(item.mime);
        if (mExt) {
          filename = (filename && filename !== "download") ? (filename + "." + mExt) : ("download_" + Date.now() + "." + mExt);
        }
      }
    }

    if (!filename || filename === "download") {
      filename = urlFilename || "downloaded_file";
    }

    // 3. Extract captured request headers
    const capturedInfo = urlRequestHeaders.get(item.url) || urlRequestHeaders.get(item.finalUrl);
    const headers = capturedInfo?.headers || {};
    const referrer = item.referrer || capturedInfo?.referrer || "";

    // 4. Fast cookie retrieval (capped at 100ms)
    let cookies = "";
    try {
      const cookiePromise = getNetscapeCookies(targetUrl);
      const timeoutPromise = new Promise(resolve => setTimeout(() => resolve(""), 100));
      cookies = await Promise.race([cookiePromise, timeoutPromise]);
    } catch (_) {
      cookies = "";
    }

    const totalSize = (item.totalBytes > 0) ? item.totalBytes : (item.fileSize > 0 ? item.fileSize : 0);

    // 5. Instantly route to Prime Download Manager desktop backend (no extension popup or in-page overlay)
    const payload = {
      url: targetUrl,
      webpage_url: referrer || targetUrl,
      title: filename,
      file_name: filename,
      total_size: totalSize,
      size: totalSize,
      headers: headers,
      cookies: cookies || undefined,
      threads: 32,
      start_now: false
    };

    try {
      const controller = new AbortController();
      const timeoutId = setTimeout(() => controller.abort(), 2500);
      const res = await fetch("http://127.0.0.1:41245/download", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(payload),
        signal: controller.signal
      });
      clearTimeout(timeoutId);
      if (!res.ok) {
        throw new Error(`Server returned status ${res.status}`);
      }
    } catch (err) {
      console.warn("[PDM Extension] Backend offline or unreachable, falling back to browser download:", err);
      bypassedDownloadUrls.set(item.url, Date.now());
      if (chrome.downloads && chrome.downloads.download) {
        chrome.downloads.download({
          url: item.url,
          filename: filename !== "downloaded_file" ? filename : undefined,
          saveAs: false
        });
      }
    }
  } catch (err) {
    logExtensionError("DOWNLOAD-INTERCEPT", `Error intercepting download: ${err.message}`, { id: item?.id, url: item?.url });
  }
}

// 4. CHROME.DOWNLOADS API INTERCEPTION (PDF, ZIP, EXE, ETC.) - DIRECT FAST ROUTE TO PDM DESKTOP
if (chrome.downloads && chrome.downloads.onCreated) {
  chrome.downloads.onCreated.addListener((item) => {
    routeDownloadToDesktop(item);
  });
}

if (chrome.downloads && chrome.downloads.onDeterminingFilename) {
  chrome.downloads.onDeterminingFilename.addListener((item, suggest) => {
    routeDownloadToDesktop(item);
  });
}

async function getNetscapeCookies(targetUrl) {
  if (!chrome.cookies) return "";
  try {
    let cookies = [];
    try {
      cookies = await chrome.cookies.getAll({ url: targetUrl });
    } catch (_) {}
    if (!cookies || cookies.length === 0) {
      let domain = new URL(targetUrl).hostname.toLowerCase();
      if (domain.includes("facebook.com") || domain.includes("fb.com") || domain.includes("fbcdn.net")) domain = ".facebook.com";
      else if (domain.includes("youtube.com") || domain.includes("youtu.be")) domain = ".youtube.com";
      else if (domain.includes("instagram.com") || domain.includes("cdninstagram.com")) domain = ".instagram.com";
      else if (domain.includes("reddit.com")) domain = ".reddit.com";
      else if (domain.includes("twitter.com") || domain.includes("x.com")) domain = ".x.com";
      else if (domain.includes("tiktok.com") || domain.includes("tiktokcdn.com") || domain.includes("byteoversea.com") || domain.includes("ibytedtos.com")) domain = ".tiktok.com";
      else if (domain.includes("linkedin.com")) domain = ".linkedin.com";
      else if (domain.includes("pinterest.com")) domain = ".pinterest.com";
      else if (domain.includes("threads.net") || domain.includes("threads.com")) domain = ".threads.net";
      else if (domain.includes("twitch.tv")) domain = ".twitch.tv";
      else if (domain.includes("tumblr.com")) domain = ".tumblr.com";
      else if (domain.includes("dailymotion.com") || domain.includes("dai.ly") || domain.includes("dmcdn.net")) domain = ".dailymotion.com";

      cookies = await chrome.cookies.getAll({ domain });

      // Extra sync for Reddit authentication session
      if (domain === ".reddit.com") {
        const extra = await chrome.cookies.getAll({ domain: "www.reddit.com" });
        if (extra && extra.length > 0) {
          cookies = [...(cookies || []), ...extra];
        }
      }
    }
    if (!cookies || cookies.length === 0) return "";
    let lines = ["# Netscape HTTP Cookie File"];
    for (const c of cookies) {
      const d = c.domain.startsWith(".") ? c.domain : "." + c.domain;
      const flag = "TRUE";
      const path = c.path || "/";
      const secure = c.secure ? "TRUE" : "FALSE";
      const exp = c.expirationDate ? Math.round(c.expirationDate) : Math.round(Date.now() / 1000 + 86400 * 30);
      lines.push(`${d}\t${flag}\t${path}\t${secure}\t${exp}\t${c.name}\t${c.value}`);
    }
    return lines.join("\n");
  } catch (_) {
    return "";
  }
}

const isFeedPage = (u) => {
  if (!u) return true;
  const l = u.toLowerCase().replace(/https?:\/\//, "").replace(/^www\./, "");
  return l === "tiktok.com" || l === "tiktok.com/" || l.startsWith("tiktok.com/foryou") || l.startsWith("tiktok.com/explore") ||
         l === "instagram.com" || l === "instagram.com/" ||
         l === "threads.net" || l === "threads.net/" ||
         l === "x.com" || l === "x.com/" || l.startsWith("x.com/home") ||
         l === "twitter.com" || l === "twitter.com/" || l.startsWith("twitter.com/home") ||
         l === "pinterest.com" || l === "pinterest.com/";
};

function resolveCanonicalMediaUrl(url, sender) {
  let targetUrl = cleanMetaUrl((url || "").trim());
  let targetTitle = "";

  const tabId = sender?.tab?.id;
  const tabUrl = (sender && sender.tab && sender.tab.url) ? sender.tab.url : "";
  const tabTitle = (sender && sender.tab && sender.tab.title) ? sender.tab.title : "";

  // 1. Direct Playable Media CDN Streams:
  // Threads, Instagram, TikTok, Pinterest, Tumblr, LinkedIn, Reddit direct streams (cdninstagram.com, fbcdn.net, tiktokcdn.com, pinimg.com, media.tumblr.com)
  // must NEVER be overwritten with tabUrl!
  const lowerTarget = targetUrl.toLowerCase();
  const isImageFile = lowerTarget.includes(".image") || lowerTarget.includes(".jpg") || lowerTarget.includes(".jpeg") ||
                      lowerTarget.includes(".png") || lowerTarget.includes(".webp") || lowerTarget.includes(".gif") ||
                      lowerTarget.includes(".ico") || lowerTarget.includes(".svg");

  const isInstagramSite = (tabUrl && tabUrl.includes("instagram.com")) || (targetUrl && targetUrl.includes("instagram.com"));
  const isThreadsSite = (tabUrl && (tabUrl.includes("threads.net") || tabUrl.includes("threads.com"))) ||
                        (targetUrl && (targetUrl.includes("threads.net") || targetUrl.includes("threads.com")));

  const isDirectCdnStream = !isImageFile && !isInstagramSite && !isThreadsSite && (
                            lowerTarget.includes("cdninstagram.com") ||
                            lowerTarget.includes("fbcdn.net") ||
                            lowerTarget.includes("twimg.com") ||
                            (lowerTarget.includes("tiktokcdn.com") && (lowerTarget.includes("/video/") || lowerTarget.includes("/play/") || lowerTarget.includes("v16") || lowerTarget.includes("v19") || lowerTarget.includes("v77"))) ||
                            lowerTarget.includes("media.tumblr.com") ||
                            lowerTarget.includes("vtt.tumblr.com") ||
                            lowerTarget.includes("dms.licdn.com") ||
                            lowerTarget.includes("v.redd.it") ||
                            (lowerTarget.includes(".mp4") && !lowerTarget.includes("googlevideo.com"))
  );

  if (isDirectCdnStream && !lowerTarget.startsWith("blob:") && !lowerTarget.startsWith("data:")) {
    return { targetUrl, targetTitle: tabTitle };
  }

  // 1.5. Instagram Priority:
  // Direct raw MP4 files from cdninstagram.com / fbcdn.net are muted DASH video-only streams.
  // Always prioritize the canonical Reel/Post URL (/reel/..., /reels/..., /p/...) so yt-dlp & ffmpeg download and merge BOTH video and audio!
  if (isInstagramSite) {
    if (tabUrl && (tabUrl.includes("/reel/") || tabUrl.includes("/reels/") || tabUrl.includes("/p/"))) {
      const m = tabUrl.match(/\/(reel|reels|p)\/([a-zA-Z0-9_-]+)/i);
      if (m && m[2]) {
        targetUrl = `https://www.instagram.com/reel/${m[2]}/`;
        return { targetUrl, targetTitle: tabTitle ? tabTitle.replace(/ • Instagram photos and videos$/i, "").replace(/ \| Instagram$/i, "").trim() : "" };
      }
    }
    if (targetUrl.includes("/reel/") || targetUrl.includes("/reels/") || targetUrl.includes("/p/")) {
      const m = targetUrl.match(/\/(reel|reels|p)\/([a-zA-Z0-9_-]+)/i);
      if (m && m[2]) {
        targetUrl = `https://www.instagram.com/reel/${m[2]}/`;
        return { targetUrl, targetTitle: tabTitle ? tabTitle.replace(/ • Instagram photos and videos$/i, "").replace(/ \| Instagram$/i, "").trim() : "" };
      }
    }
  }

  // 1.55. Threads Priority:
  // Threads is not supported by yt-dlp; prioritize authentic captured Meta CDN video + audio stream from tab!
  if (isThreadsSite) {
    let capturedAudio = "";
    if (tabId && tabCapturedMedia.has(tabId)) {
      const captured = tabCapturedMedia.get(tabId) || [];
      const aStreams = captured.filter(c => 
        (c.url.includes("cdninstagram.com") || c.url.includes("fbcdn.net") || c.url.includes("scontent")) && 
        !c.isSegment && (c.isAudio || isMetaAudioUrl(c.url, c.mime))
      );
      if (aStreams.length > 0) {
        capturedAudio = cleanMetaUrl(aStreams[0].url);
      }

      const vStreams = captured.filter(c => 
        (c.url.includes("cdninstagram.com") || c.url.includes("fbcdn.net") || c.url.includes("scontent")) && 
        !c.isSegment && !c.isAudio && !isMetaAudioUrl(c.url, c.mime) && (c.ext === "mp4" || c.url.includes(".mp4") || c.url.includes("/v/t64"))
      );
      if (vStreams.length > 0) {
        vStreams.sort((a, b) => (b.timestamp || 0) - (a.timestamp || 0));
        return { 
          targetUrl: cleanMetaUrl(vStreams[0].url), 
          targetTitle: tabTitle ? tabTitle.replace(/ • Threads$/i, "").replace(/ on Threads$/i, "").trim() : "",
          audioUrl: capturedAudio
        };
      }
    }
    if (targetUrl && (targetUrl.includes("cdninstagram.com") || targetUrl.includes("fbcdn.net"))) {
      return { 
        targetUrl: cleanMetaUrl(targetUrl), 
        targetTitle: tabTitle ? tabTitle.replace(/ • Threads$/i, "").replace(/ on Threads$/i, "").trim() : "",
        audioUrl: capturedAudio
      };
    }
  }

  // 1.6. Pinterest Priority:
  // Direct raw MP4 files from pinimg.com/videos/ are muted transcodes.
  // Always prioritize the canonical Pin URL (/pin/...) or HLS master playlist (.m3u8 with audio)
  const isPinterestSite = (tabUrl && (tabUrl.includes("pinterest.com") || tabUrl.includes("pin.it"))) ||
                          (targetUrl && (targetUrl.includes("pinterest.com") || targetUrl.includes("pin.it")));
  if (isPinterestSite) {
    if (tabUrl && (tabUrl.includes("/pin/") || tabUrl.includes("pin.it/"))) {
      const pinMatch = tabUrl.match(/\/pin\/(\d+)/) || tabUrl.match(/pin\.it\/([a-zA-Z0-9_-]+)/);
      if (pinMatch) {
        targetUrl = tabUrl.split("?")[0];
        return { targetUrl, targetTitle: tabTitle ? tabTitle.replace(/ \| Pinterest$/i, "").trim() : "" };
      }
    }
    // If targetUrl contains /pin/, preserve it as top priority
    if (targetUrl.includes("/pin/") || targetUrl.includes("pin.it/")) {
      return { targetUrl, targetTitle: tabTitle ? tabTitle.replace(/ \| Pinterest$/i, "").trim() : "" };
    }
    // If captured media has HLS manifest (.m3u8), prioritize it over muted mp4
    if (tabId && tabCapturedMedia.has(tabId)) {
      const captured = tabCapturedMedia.get(tabId) || [];
      const hls = captured.find(c => (c.url.includes("pinimg.com") || c.url.includes("pinterest")) && (c.kind === "manifest" || c.url.includes(".m3u8")) && !c.isSegment);
      if (hls && hls.url) {
        return { targetUrl: hls.url, targetTitle: tabTitle ? tabTitle.replace(/ \| Pinterest$/i, "").trim() : "" };
      }
    }
  }

  const isSpecificPortalPost = (u) => {
    if (!u) return false;
    const l = u.toLowerCase();
    return (l.includes("facebook.com") && (l.includes("/videos/") || l.includes("/reel/") || l.includes("/watch/") || l.includes("fb.watch") || l.includes("story.php"))) ||
           (l.includes("fb.watch")) ||
           (l.includes("youtube.com") && (l.includes("/watch") || l.includes("/shorts/"))) ||
           (l.includes("youtu.be/")) ||
           (l.includes("twitter.com") && l.includes("/status/")) ||
           (l.includes("x.com") && l.includes("/status/")) ||
           (l.includes("tiktok.com") && (l.includes("/video/") || l.includes("/v/"))) ||
           (l.includes("pinterest.com") && l.includes("/pin/")) ||
           (l.includes("pin.it/")) ||
           (l.includes("tumblr.com") && l.includes("/post/")) ||
           (l.includes("dailymotion.com") && l.includes("/video/")) ||
           (l.includes("dai.ly/")) ||
           (l.includes("threads.net") && l.includes("/post/")) ||
           (l.includes("instagram.com") && (l.includes("/p/") || l.includes("/reel/") || l.includes("/reels/"))) ||
           (l.includes("reddit.com") && (l.includes("/comments/") || l.includes("/r/")));
  };

  // If content.js already resolved the exact specific post link, PRESERVE IT!
  if (isSpecificPortalPost(targetUrl)) {
    return { targetUrl, targetTitle: tabTitle };
  }

  // 2. YouTube: If user is on a YouTube watch/shorts/embed page
  if (tabUrl && (tabUrl.includes("youtube.com") || tabUrl.includes("youtu.be"))) {
    const vMatch = tabUrl.match(/[?&]v=([^&]+)/) || tabUrl.match(/(?:embed\/|v\/|shorts\/)([a-zA-Z0-9_-]+)/);
    if (vMatch && vMatch[1]) {
      targetUrl = `https://www.youtube.com/watch?v=${vMatch[1]}`;
      if (tabTitle) {
        targetTitle = tabTitle.replace(/ - YouTube$/i, "").trim();
      }
      return { targetUrl, targetTitle };
    }
  }

  // 3. Dailymotion: If user is on a Dailymotion video page
  if (tabUrl) {
    const dmTabMatch = tabUrl.match(/(?:dailymotion\.com\/video\/|dai\.ly\/|[?&]video=)([a-zA-Z0-9]+)/i);
    if (dmTabMatch && dmTabMatch[1] && dmTabMatch[1] !== "fmp4") {
      targetUrl = `https://www.dailymotion.com/video/${dmTabMatch[1]}`;
      if (tabTitle) {
        targetTitle = tabTitle.replace(/ - Dailymotion$/i, "").replace(/ \| Dailymotion$/i, "").trim();
      }
      return { targetUrl, targetTitle };
    }
  }

  // 4. Other Major Video Portals: ONLY if tabUrl is a specific post, not a home feed
  if (tabUrl && isSpecificPortalPost(tabUrl)) {
    targetUrl = tabUrl;
    return { targetUrl, targetTitle: tabTitle };
  }

  const isDirectMediaEndpoint = (raw) => {
    if (!raw || typeof raw !== "string") return false;
    const l = raw.toLowerCase();
    if (l.startsWith("blob:") || l.startsWith("data:") || l.includes("googlevideo.com/videoplayback")) return false;
    if (l.includes(".image") || l.includes(".jpeg") || l.includes(".jpg") || l.includes(".webp") || l.includes(".png") || l.includes(".gif")) return false;
    return l.includes(".mp4") || l.includes(".m3u8") || l.includes(".mpd") || l.includes(".webm") ||
           l.includes("cdninstagram.com") || l.includes("fbcdn.net") || 
           (l.includes("tiktokcdn.com") && (l.includes("/video/") || l.includes("/play/") || l.includes("v16") || l.includes("v19") || l.includes("v77"))) ||
           l.includes("pinimg.com/videos/") || l.includes("media.tumblr.com") || l.includes("dms.licdn.com") ||
           l.includes("v.redd.it");
  };

  // 5. If targetUrl is an explicit direct media endpoint, prioritize it
  if (isDirectMediaEndpoint(targetUrl)) {
    return { targetUrl, targetTitle: tabTitle };
  }

  // 6. Check captured media sniffer for the tab BEFORE falling back to bare page URL (strictly newest first)
  if (tabId && tabCapturedMedia.has(tabId)) {
    const captured = tabCapturedMedia.get(tabId) || [];
    const best = [...captured].reverse().find(c => c.kind === "manifest") ||
                 [...captured].reverse().find(c => !c.isSegment && c.ext === "mp4") ||
                 [...captured].reverse().find(c => isDirectMediaEndpoint(c.url)) ||
                 captured[captured.length - 1];
    if (best && best.url) {
      targetUrl = best.url;
      return { targetUrl, targetTitle: tabTitle };
    }
  }

  // 7. Fallback for clean non-blob URL
  if (targetUrl && !targetUrl.startsWith("blob:") && !targetUrl.startsWith("data:") &&
      !targetUrl.includes("googlevideo.com/videoplayback") &&
      !targetUrl.endsWith("threads.net/") && !targetUrl.endsWith("threads.net")) {
    return { targetUrl, targetTitle: tabTitle };
  }

  if (tabUrl && !tabUrl.startsWith("blob:") && !isFeedPage(tabUrl)) {
    targetUrl = tabUrl;
  }

  return { targetUrl, targetTitle };
}

// 5. Listen for requests from floating video button in content scripts
chrome.runtime.onMessage.addListener((request, sender, sendResponse) => {
  if (request.action === "get_tab_info") {
    sendResponse({
      tabUrl: sender?.tab?.url || "",
      tabTitle: sender?.tab?.title || ""
    });
  } else if (request.action === "get_captured_media") {
    const tabId = sender?.tab?.id;
    const media = tabId ? (tabCapturedMedia.get(tabId) || []) : [];
    sendResponse({ media });
    return true;
  } else if (request.action === "url_changed") {
    const tabId = sender?.tab?.id;
    if (tabId && tabCapturedMedia.has(tabId)) {
      tabCapturedMedia.set(tabId, []);
    }
    sendResponse({ success: true });
    return true;
  } else if (request.action === "probe_url") {
    (async () => {
      try {
        const { targetUrl } = resolveCanonicalMediaUrl(request.url, sender);
        let urlToUse = targetUrl || request.url;
        const webpageUrl = request.webpage_url || sender?.tab?.url || "";
        if ((!urlToUse || urlToUse.startsWith("blob:") || urlToUse.startsWith("data:")) && webpageUrl) {
          if (!isFeedPage(webpageUrl)) {
            urlToUse = webpageUrl;
          }
        }
        if (!urlToUse || urlToUse.endsWith(".webmanifest") || urlToUse.endsWith(".manifest") || 
            urlToUse.includes("/manifest.webmanifest") || urlToUse.includes("/site.webmanifest") ||
            urlToUse.endsWith(".json") || urlToUse.endsWith(".ico") || isFeedPage(urlToUse)) {
          sendResponse({ success: false, error: "Not a probeable media URL" });
          return;
        }
        const cookies = await getNetscapeCookies(urlToUse);
        const controller = new AbortController();
        const timeoutId = setTimeout(() => controller.abort(), 35000);
        const res = await fetch("http://127.0.0.1:41245/probe", {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({ 
            url: urlToUse, 
            webpage_url: webpageUrl, 
            quality: request.quality || "best", 
            cookies: cookies || undefined, 
            fresh: request.fresh || false 
          }),
          signal: controller.signal
        });
        clearTimeout(timeoutId);
        const data = await res.json();
        sendResponse({ success: true, data });
      } catch (err) {
        if (err.name !== "AbortError" && !err.message?.includes("aborted")) {
          logExtensionError("PROBE-HANDLER", `Failed to probe url: ${err.message}`, { url: request.url });
        }
        sendResponse({ success: false, error: err.message });
      }
    })();
    return true;
  } else if (request.action === "send_download") {
    (async () => {
      try {
        const { targetUrl, targetTitle, audioUrl: canonicalAudio } = resolveCanonicalMediaUrl(request.url, sender);
        let urlToUse = (request.direct_server_url && request.direct_server_url.startsWith("http") && !request.direct_server_url.startsWith("blob:"))
          ? request.direct_server_url
          : (targetUrl || request.url);
        urlToUse = cleanMetaUrl(urlToUse);
        const webpageUrl = request.webpage_url || sender?.tab?.url || "";
        if ((!urlToUse || urlToUse.startsWith("blob:") || urlToUse.startsWith("data:")) && webpageUrl) {
          if (!isFeedPage(webpageUrl)) {
            urlToUse = webpageUrl;
          }
        }
        let audioToUse = request.audio_url || canonicalAudio || undefined;
        if (!audioToUse && sender?.tab?.id && tabCapturedMedia.has(sender.tab.id)) {
          const captured = tabCapturedMedia.get(sender.tab.id) || [];
          const aStream = [...captured].reverse().find(c => 
            (c.url.includes("cdninstagram.com") || c.url.includes("fbcdn.net") || c.url.includes("scontent")) && 
            !c.isSegment && (c.isAudio || isMetaAudioUrl(c.url, c.mime))
          );
          if (aStream) audioToUse = cleanMetaUrl(aStream.url);
        }

        const isInstagram = (webpageUrl && webpageUrl.includes("instagram.com")) || (targetUrl && targetUrl.includes("instagram.com")) || (request.url && request.url.includes("instagram.com"));
        if (isInstagram) {
          if (targetUrl && (targetUrl.includes("/reel/") || targetUrl.includes("/p/"))) {
            urlToUse = targetUrl;
          } else if (webpageUrl && (webpageUrl.includes("/reel/") || webpageUrl.includes("/p/"))) {
            urlToUse = webpageUrl;
          }
          audioToUse = undefined;
        }
        const titleToUse = ((request.title && !request.title.toLowerCase().includes("dailymotion video player") && !request.title.toLowerCase().includes("xtv3w"))
          ? request.title
          : (targetTitle || request.title || "")).replace(/\.unknown_video$/i, "");

        let cookies = "";
        try {
          const cookiePromise = getNetscapeCookies(urlToUse);
          const timeoutPromise = new Promise(resolve => setTimeout(() => resolve(""), 200));
          cookies = await Promise.race([cookiePromise, timeoutPromise]);
        } catch (_) {
          cookies = "";
        }
        const payload = {
          url: urlToUse,
          audio_url: audioToUse || undefined,
          webpage_url: webpageUrl,
          direct_server_url: request.direct_server_url || ((urlToUse && !urlToUse.startsWith("blob:") && urlToUse !== webpageUrl) ? urlToUse : undefined),
          threads: request.threads || 32,
          quality: request.quality || "best",
          title: titleToUse,
          duration: request.duration || 0,
          qualities: request.qualities || [],
          cookies: cookies || undefined
        };
        if (request.save_path) {
          payload.save_path = request.save_path;
        }
        const res = await fetch("http://127.0.0.1:41245/download", {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify(payload)
        });
        const data = await res.json();
        sendResponse({ success: true, data });
      } catch (err) {
        logExtensionError("DOWNLOAD-HANDLER", `Failed to send download to desktop: ${err.message}`, { url: request.url });
        sendResponse({ success: false, error: err.message });
      }
    })();
    return true;
  } else if (request.action === "open_download_dialog") {
    (async () => {
      try {
        const { targetUrl, targetTitle, audioUrl: canonicalAudio } = resolveCanonicalMediaUrl(request.url, sender);
        let target = (request.direct_server_url && request.direct_server_url.startsWith("http") && !request.direct_server_url.startsWith("blob:"))
          ? request.direct_server_url
          : (targetUrl || request.url);
        target = cleanMetaUrl(target);
        const webpageUrl = request.webpage_url || sender?.tab?.url || "";
        if ((!target || target.startsWith("blob:") || target.startsWith("data:")) && webpageUrl) {
          target = webpageUrl;
        }
        let audioToUse = request.audio_url || canonicalAudio || undefined;
        if (!audioToUse && sender?.tab?.id && tabCapturedMedia.has(sender.tab.id)) {
          const captured = tabCapturedMedia.get(sender.tab.id) || [];
          const aStream = [...captured].reverse().find(c => 
            (c.url.includes("cdninstagram.com") || c.url.includes("fbcdn.net") || c.url.includes("scontent")) && 
            !c.isSegment && (c.isAudio || isMetaAudioUrl(c.url, c.mime))
          );
          if (aStream) audioToUse = cleanMetaUrl(aStream.url);
        }

        const isInstagram = (webpageUrl && webpageUrl.includes("instagram.com")) || (targetUrl && targetUrl.includes("instagram.com")) || (request.url && request.url.includes("instagram.com"));
        if (isInstagram) {
          if (targetUrl && (targetUrl.includes("/reel/") || targetUrl.includes("/p/"))) {
            urlToUse = targetUrl;
          } else if (webpageUrl && (webpageUrl.includes("/reel/") || webpageUrl.includes("/p/"))) {
            urlToUse = webpageUrl;
          }
          audioToUse = undefined;
        }
        const titleToUse = request.title || targetTitle || "";
        await sendDownloadToPrime(target, titleToUse, request.duration || 0, webpageUrl, request.qualities || null, request.quality || "best", audioToUse);
        sendResponse({ success: true, status: "dialog_opened" });
      } catch (err) {
        logExtensionError("OPEN-DIALOG", `Failed to send download to desktop: ${err.message}`, { url: request.url });
        sendResponse({ success: false, error: err.message });
      }
    })();
    return true;
  } else if (request.action === "start_intercepted_download") {
    (async () => {
      try {
        const dl = request.downloadData;
        const targetUrl = dl.url || dl.finalUrl;
        const pageUrl = dl.referrer || dl.pageUrl || targetUrl;
        const cookies = await getNetscapeCookies(targetUrl);

        // Capture request headers and inject Referer if missing
        const reqHeaders = { ...(dl.headers || {}) };
        if (pageUrl && !reqHeaders["referer"]) {
          reqHeaders["referer"] = pageUrl;
        }

        const payload = {
          url: targetUrl,
          webpage_url: pageUrl,
          title: dl.filename || "",
          category: dl.category || "",
          save_path: dl.save_path || "",
          threads: 32,
          quality: dl.quality || "best",
          start_now: (dl.start_now !== false),
          headers: reqHeaders,
          cookies: cookies || undefined,
          user_agent: navigator.userAgent
        };

        const res = await fetch("http://127.0.0.1:41245/download", {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify(payload)
        });

        if (!res.ok) {
          throw new Error(`Server returned status ${res.status}`);
        }
        const data = await res.json();
        sendResponse({ success: true, data });
      } catch (err) {
        logExtensionError("START-INTERCEPTED-DL", `Failed to route download to backend: ${err.message}`, { url: request.downloadData?.url });
        const isOffline = !err || err.message === "Failed to fetch" || err.message.includes("NetworkError") || err.message.includes("Failed to connect") || err.message.includes("connection");
        const errMsg = isOffline
          ? "Prime Download Manager desktop app is not running. Please start the app or download via browser."
          : `Desktop connection error: ${err.message}`;
        sendResponse({ success: false, error: errMsg, isOffline });
      }
    })();
    return true;
  } else if (request.action === "download_via_browser") {
    const dl = request.downloadData;
    const targetUrl = dl?.url || dl?.finalUrl;
    if (targetUrl && chrome.downloads && chrome.downloads.download) {
      const now = Date.now();
      bypassedDownloadUrls.set(targetUrl, now);
      chrome.downloads.download({
        url: targetUrl,
        filename: dl.filename || undefined,
        saveAs: true
      }, (downloadId) => {
        if (chrome.runtime.lastError) {
          sendResponse({ success: false, error: chrome.runtime.lastError.message });
        } else {
          sendResponse({ success: true, downloadId });
        }
      });
      return true;
    }
    sendResponse({ success: false, error: "Download URL invalid" });
    return true;
  } else if (request.action === "probe_file_size") {
    (async () => {
      const sz = await probeUrlFileSize(request.url, request.headers || {});
      sendResponse({ size: sz });
    })();
    return true;
  }
  return true;
});

// 4. Core Handshake Function with Prime Desktop App
async function sendDownloadToPrime(url, filename = "", duration = 0, webpageUrl = "", qualities = null, quality = null, audioUrl = "") {
  try {
    let cleanUrl = url;
    if ((!cleanUrl || cleanUrl.startsWith("blob:") || cleanUrl.startsWith("data:")) && webpageUrl) {
      cleanUrl = webpageUrl;
    }

    let cookies = "";
    try {
      const cookiePromise = getNetscapeCookies(cleanUrl);
      const timeoutPromise = new Promise(resolve => setTimeout(() => resolve(""), 200));
      cookies = await Promise.race([cookiePromise, timeoutPromise]);
    } catch (_) {
      cookies = "";
    }
    const chosenQuality = quality || "best";

    const payload = {
      url: cleanUrl,
      audio_url: audioUrl || undefined,
      webpage_url: webpageUrl || cleanUrl,
      save_path: "",
      threads: 32,
      quality: chosenQuality,
      title: filename || "",
      duration: duration || 0,
      qualities: qualities || undefined,
      cookies: (cleanUrl && (cleanUrl.includes("youtube.com") || cleanUrl.includes("youtu.be"))) ? undefined : (cookies || undefined)
    };

    const res = await fetch(PRIME_DESKTOP_ENDPOINT, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(payload)
    });

    if (!res.ok) {
      throw new Error(`Server returned ${res.status}`);
    }
  } catch (err) {
    logExtensionError("SEND-DOWNLOAD", `Failed to send download to Prime Desktop: ${err.message}`, { url, filename });
    // Fallback: Copy to storage so Prime's popup or monitor catches it
    try {
      chrome.storage.local.set({ pending_download_url: url });
    } catch (_) {}
  }
}


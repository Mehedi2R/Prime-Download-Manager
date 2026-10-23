// Prime Download Manager Extension - High Precision IDM Floating Video Grabber
// Accurate Multi-Tier Available Qualities Inspector & Downloader Engine
(function() {
  // 1. STRICT SINGLETON: Only ONE container element ever exists on the page
  let grabberContainer = null;
  let activeVideo = null;
  let hoveredVideo = null;
  let probeRequestId = 0;
  let currentProbedUrl = null;
  let activeRenderQualityItems = null;

  // Track hovered video for instant grabber attachment on feeds (Twitter/X, Pinterest, Facebook, TikTok, Instagram)
  document.addEventListener("mouseover", (e) => {
    try {
      let v = null;
      if (e.target instanceof HTMLVideoElement) {
        v = e.target;
      } else if (e.target && e.target.closest) {
        v = e.target.closest("video");
        if (!v) {
          // Twitter / X.com specific video player containers (highest priority on X.com overlay clicks)
          const xPlayerBox = e.target.closest('[data-testid="videoComponent"], [data-testid="videoPlayer"], [data-testid="tweetPhoto"], div[aria-label="Embedded video"], [data-testid="expanded-media-root"]');
          if (xPlayerBox) {
            v = xPlayerBox.querySelector("video");
          }
        }
        if (!v) {
          const container = e.target.closest('[data-e2e*="video"], [data-e2e*="feed"], [data-e2e*="recommend"], [data-pressable-container="true"], [role="article"], div[style*="aspect-ratio"], div[class*="x1rg5ohu"], div[class*="x1ey2m1c"], [class*="DivVideo"], [class*="DivPlayer"], [class*="DivBasicPlayerWrapper"], [class*="ItemContainer"], [class*="xgwrapper"], [data-test-id*="pin"], [data-testid="tweet"], article');
          if (container) {
            const vFound = container.querySelector("video");
            if (vFound) {
              const box = vFound.closest('[data-testid="videoComponent"], [data-testid="videoPlayer"]') || vFound;
              const r = box.getBoundingClientRect();
              if (e.clientX >= r.left - 10 && e.clientX <= r.right + 10 && e.clientY >= r.top - 10 && e.clientY <= r.bottom + 10) {
                v = vFound;
              }
            }
          }
        }
      }
      if (v && isEligibleVideo(v)) {
        if (hoveredVideo !== v) {
          hoveredVideo = v;
          // Ensure dropdown remains strictly closed on hover
          if (grabberContainer) {
            const dd = grabberContainer.querySelector("#m2r-dropdown");
            const b = grabberContainer.querySelector("#m2r-btn");
            if (dd) {
              dd.classList.remove("show");
              dd.removeAttribute("data-state");
              dd.style.setProperty("display", "none", "important");
            }
            if (b) b.classList.remove("active");
          }
          updatePosition();
        }
      }
    } catch (_) {}
  }, { passive: true, capture: true });

  // Cache for accurate available qualities per video URL
  const detectedQualitiesMap = new Map();

  let cachedTabUrl = "";

  let cachedNetworkStreams = [];
  function refreshCapturedMedia() {
    try {
      chrome.runtime.sendMessage({ action: "get_captured_media" }, (res) => {
        if (res && Array.isArray(res.media)) {
          cachedNetworkStreams = res.media;
        }
      });
    } catch (_) {}
  }
  refreshCapturedMedia();
  setInterval(refreshCapturedMedia, 1500);
  document.addEventListener("play", refreshCapturedMedia, { passive: true, capture: true });
  document.addEventListener("playing", refreshCapturedMedia, { passive: true, capture: true });
  document.addEventListener("loadedmetadata", refreshCapturedMedia, { passive: true, capture: true });

  function fetchTabInfo() {
    try {
      chrome.runtime.sendMessage({ action: "get_tab_info" }, (res) => {
        if (res && res.tabUrl) {
          cachedTabUrl = res.tabUrl;
        }
      });
    } catch (_) {}
  }
  fetchTabInfo();
  setInterval(fetchTabInfo, 3000);

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

  function extractMetaStreams(videoElement) {
    let videoStream = "";
    let audioStream = "";

    // 1. Direct video element src
    if (videoElement) {
      const vSrc = videoElement.currentSrc || videoElement.src || "";
      if (vSrc && vSrc.startsWith("http") && !vSrc.startsWith("blob:") && !vSrc.startsWith("data:")) {
        if (isMetaAudioUrl(vSrc)) {
          audioStream = cleanMetaUrl(vSrc);
        } else {
          videoStream = cleanMetaUrl(vSrc);
        }
      }
      const source = videoElement.querySelector("source[src]");
      if (source && source.src && source.src.startsWith("http") && !source.src.startsWith("blob:")) {
        if (isMetaAudioUrl(source.src)) {
          if (!audioStream) audioStream = cleanMetaUrl(source.src);
        } else {
          if (!videoStream) videoStream = cleanMetaUrl(source.src);
        }
      }
    }

    // 2. Deep React Fiber / Props inspection for Threads
    if (videoElement) {
      try {
        let cur = videoElement;
        let depth = 0;
        while (cur && depth < 10) {
          const fiberKey = Object.keys(cur).find(k => k.startsWith("__reactFiber$") || k.startsWith("__reactProps$"));
          if (fiberKey) {
            let fiber = cur[fiberKey];
            let fDepth = 0;
            while (fiber && fDepth < 25) {
              const p = fiber.memoizedProps;
              if (p) {
                // Check if progressive video with audio exists
                const versions = p.video_versions || p.videoVersions || p.post?.video_versions || p.thread_item?.post?.video_versions;
                if (Array.isArray(versions) && versions.length > 0) {
                  const best = versions.find(v => v.url && !v.url.startsWith("blob:")) || versions[0];
                  if (best && best.url) {
                    return { videoStream: cleanMetaUrl(best.url), audioStream: "" };
                  }
                }
                // Check for dash_manifest XML
                const dashManifest = p.dashManifest || p.dash_manifest || p.post?.dash_manifest;
                if (dashManifest && typeof dashManifest === "string") {
                  const audioMatch = dashManifest.match(/<BaseURL[^>]*>(https:[^<]+(?:dash_ln_heaac|heaac|audio)[^<]*)<\/BaseURL>/i);
                  if (audioMatch && audioMatch[1]) {
                    audioStream = cleanMetaUrl(audioMatch[1].replace(/&amp;/g, '&'));
                  }
                  const videoMatch = dashManifest.match(/<BaseURL[^>]*>(https:[^<]+(?:dash_vp9|video)[^<]*)<\/BaseURL>/i);
                  if (videoMatch && videoMatch[1]) {
                    videoStream = cleanMetaUrl(videoMatch[1].replace(/&amp;/g, '&'));
                  }
                  if (videoStream && audioStream) {
                    return { videoStream, audioStream };
                  }
                }
              }
              fiber = fiber.return;
              fDepth++;
            }
          }
          cur = cur.parentElement;
          depth++;
        }
      } catch (_) {}
    }

    // 3. Real-time browser performance resource entries
    try {
      const perf = performance.getEntriesByType("resource");
      if (perf && perf.length > 0) {
        for (let i = perf.length - 1; i >= 0; i--) {
          const name = perf[i].name || "";
          if ((name.includes("cdninstagram.com") || name.includes("fbcdn.net") || name.includes("scontent")) &&
              !name.includes(".jpg") && !name.includes(".png") && !name.includes(".webp") && !name.includes(".ico")) {
            if (isMetaAudioUrl(name)) {
              if (!audioStream) audioStream = cleanMetaUrl(name);
            } else if (name.includes(".mp4") || name.includes("/v/t64") || name.includes("bytestart=")) {
              if (!videoStream) videoStream = cleanMetaUrl(name);
            }
            if (videoStream && audioStream) break;
          }
        }
      }
    } catch (_) {}

    // 4. Real-time network sniffer for captured cdninstagram/fbcdn mp4 stream (strictly newest first)
    refreshCapturedMedia();
    if (cachedNetworkStreams && cachedNetworkStreams.length > 0) {
      for (let i = cachedNetworkStreams.length - 1; i >= 0; i--) {
        const s = cachedNetworkStreams[i];
        const u = s.url || "";
        if ((u.includes("cdninstagram.com") || u.includes("fbcdn.net") || u.includes("scontent")) && !s.isSegment) {
          if (isMetaAudioUrl(u, s.mime)) {
            if (!audioStream) audioStream = cleanMetaUrl(u);
          } else {
            if (!videoStream) videoStream = cleanMetaUrl(u);
          }
          if (videoStream && audioStream) break;
        }
      }
    }

    return { videoStream, audioStream };
  }

  function extractDirectMetaCdnStream(videoElement) {
    // 1. Direct video element stream URL from Meta CDN (if not blob)
    if (videoElement) {
      const vSrc = videoElement.currentSrc || videoElement.src || "";
      if (vSrc && vSrc.startsWith("http") && !vSrc.startsWith("blob:") && !vSrc.startsWith("data:")) {
        return cleanMetaUrl(vSrc);
      }
      const source = videoElement.querySelector("source[src]");
      if (source && source.src && source.src.startsWith("http") && !source.src.startsWith("blob:")) {
        return cleanMetaUrl(source.src);
      }
    }

    // 1.5. Real-time browser resource inspection (100% reliable for active Meta media requests, newest first)
    try {
      const perf = performance.getEntriesByType("resource");
      if (perf && perf.length > 0) {
        for (let i = perf.length - 1; i >= 0; i--) {
          const name = perf[i].name || "";
          if ((name.includes("cdninstagram.com") || name.includes("fbcdn.net") || name.includes("scontent")) &&
              name.includes(".mp4") && !name.includes(".jpg") && !name.includes(".png") && !name.includes(".webp")) {
            return cleanMetaUrl(name);
          }
        }
      }
    } catch (_) {}

    // 2. Real-time network sniffer for captured cdninstagram/fbcdn mp4 stream (newest stream first)
    refreshCapturedMedia();
    if (cachedNetworkStreams && cachedNetworkStreams.length > 0) {
      const validStreams = cachedNetworkStreams.filter(s => 
        (s.url.includes("cdninstagram.com") || s.url.includes("fbcdn.net") || s.url.includes("scontent")) && 
        s.ext === "mp4" && !s.isSegment
      );
      if (validStreams.length > 0) {
        validStreams.sort((a, b) => (b.timestamp || 0) - (a.timestamp || 0));
        return cleanMetaUrl(validStreams[0].url);
      }
    }

    return "";
  }

  function getTikTokData(videoEl) {
    if (!videoEl) return null;

    // 1. React Fiber / Props Deep-Inspection (100% reliable data access)
    let cur = videoEl;
    let depth = 0;
    while (cur && depth < 8) {
      const fiberKey = Object.keys(cur).find(k => k.startsWith("__reactFiber$") || k.startsWith("__reactProps$"));
      if (fiberKey) {
        try {
          let fiber = cur[fiberKey];
          let fDepth = 0;
          while (fiber && fDepth < 20) {
            const p = fiber.memoizedProps;
            const item = p?.itemInfo || p?.item || p?.videoData || p?.aweme || p?.data;
            if (item && (item.id || item.aweme_id)) {
              const id = String(item.id || item.aweme_id);
              const author = item.author?.uniqueId || item.author?.id || item.author?.nickname || "";
              const desc = (item.desc || item.title || "").trim();
              const canonicalUrl = author ? `https://www.tiktok.com/@${author}/video/${id}` : `https://www.tiktok.com/video/${id}`;

              const qualities = [];
              if (Array.isArray(item.video?.bitrateInfo)) {
                for (const b of item.video.bitrateInfo) {
                  const h = b.Height || b.height || 0;
                  const label = b.gearName || (h ? `${h}p` : "HD");
                  const url = b.PlayAddr?.UrlList?.[0] || b.playAddr?.urlList?.[0];
                  if (url) {
                    qualities.push({
                      quality: label,
                      height: h,
                      sizeBytes: b.PlayAddr?.DataSize || b.playAddr?.dataSize || 0,
                      typeLabel: `MP4 (${label})`,
                      dialogLabel: label,
                      directUrl: url
                    });
                  }
                }
              }
              return { id, author, desc, canonicalUrl, qualities };
            }
            fiber = fiber.return;
            fDepth++;
          }
        } catch (_) {}
      }
      cur = cur.parentElement;
      depth++;
    }

    // 2. DOM inspection on container
    try {
      const container = videoEl.closest('[data-e2e="recommend-list-item-container"]')
                     || videoEl.closest('[data-e2e="feed-item"]')
                     || videoEl.closest('[data-e2e="feed-video"]')
                     || videoEl.closest('div[id*="xgwrapper"]')
                     || videoEl.closest('div[class*="DivItemContainer"]')
                     || videoEl.closest('div[class*="DivVideoWrapper"]')
                     || videoEl.closest('article')
                     || videoEl.parentElement;
      if (container) {
        // Direct anchor to /video/
        const postLink = container.querySelector('a[href*="/video/"], a[href*="/v/"]');
        if (postLink && postLink.href) {
          const m = postLink.href.match(/tiktok\.com\/@([^/]+)\/video\/(\d+)/i);
          const author = m ? m[1] : "";
          const id = m ? m[2] : "";
          const descEl = container.querySelector('[data-e2e="video-desc"], [data-e2e="browse-video-desc"], div[class*="DivTextWrapper"], span[class*="SpanText"]');
          const desc = descEl ? descEl.textContent.trim() : "";
          return { id, author, desc, canonicalUrl: postLink.href, qualities: [] };
        }

        // Author anchor + numeric video ID from container attributes
        const authorLink = container.querySelector('a[href^="/@"], [data-e2e="video-author-uniqueid"], [data-e2e="video-author-avatar"] a');
        let author = "";
        if (authorLink) {
          const rawHref = authorLink.getAttribute("href") || authorLink.innerText || "";
          const aMatch = rawHref.match(/@([a-zA-Z0-9_.-]+)/);
          if (aMatch) author = aMatch[1];
        }

        let videoId = "";
        const allNodes = [container, ...Array.from(container.querySelectorAll('*'))];
        for (const node of allNodes) {
          for (const attr of node.getAttributeNames()) {
            const val = node.getAttribute(attr);
            if (val && typeof val === "string") {
              const idMatch = val.match(/\b(7\d{18})\b/);
              if (idMatch) {
                videoId = idMatch[1];
                break;
              }
            }
          }
          if (videoId) break;
        }

        const descEl = container.querySelector('[data-e2e="video-desc"], [data-e2e="browse-video-desc"], div[class*="DivTextWrapper"], span[class*="SpanText"]');
        const desc = descEl ? descEl.textContent.trim() : "";

        if (videoId) {
          const canonicalUrl = author ? `https://www.tiktok.com/@${author}/video/${videoId}` : `https://www.tiktok.com/video/${videoId}`;
          return { id: videoId, author, desc, canonicalUrl, qualities: [] };
        }
      }
    } catch (_) {}

    return null;
  }

  function getCleanVideoUrl(videoElement) {
    let currentHref = window.location.href;
    try {
      if (window.top && window.top.location && window.top.location.href) {
        currentHref = window.top.location.href;
      }
    } catch (_) {}

    const host = window.location.hostname.toLowerCase();

    // YouTube handler (youtube.com & youtu.be)
    if (host.includes("youtube.com") || host.includes("youtu.be")) {
      const vMatch = currentHref.match(/[?&]v=([^&]+)/);
      if (vMatch) {
        return `https://www.youtube.com/watch?v=${vMatch[1]}`;
      }
      if (currentHref.includes("/shorts/")) {
        const shortMatch = currentHref.match(/\/shorts\/([a-zA-Z0-9_-]+)/);
        if (shortMatch) {
          return `https://www.youtube.com/shorts/${shortMatch[1]}`;
        }
      }
      if (videoElement) {
        try {
          const item = videoElement.closest('ytd-rich-item-renderer, ytd-video-renderer, ytd-compact-video-renderer, ytd-grid-video-renderer, ytd-reel-item-renderer, ytd-shorts, ytd-notification-renderer');
          if (item) {
            const link = item.querySelector('a#thumbnail[href*="/watch?v="], a[href*="/watch?v="], a[href*="/shorts/"]');
            if (link && link.href) {
              return link.href;
            }
          }
        } catch (_) {}
      }
      return currentHref;
    }

    // Dailymotion handler (including geo.dailymotion player iframes and dmcdn streams)
    if (host.includes("dailymotion.com") || host.includes("dai.ly") || host.includes("dmcdn.net")) {
      // 0. Priority: Check cached tab URL from background script
      if (cachedTabUrl) {
        const tabMatch = cachedTabUrl.match(/(?:dailymotion\.com\/video\/|dai\.ly\/|[?&]video=)([a-zA-Z0-9]+)/i);
        if (tabMatch && tabMatch[1] && tabMatch[1] !== "fmp4") {
          return `https://www.dailymotion.com/video/${tabMatch[1]}`;
        }
      }

      // 1. Check currentHref (top URL if accessible, or window.location.href)
      const hrefMatch = currentHref.match(/(?:dailymotion\.com\/video\/|dai\.ly\/|[?&]video=)([a-zA-Z0-9]+)/);
      if (hrefMatch && hrefMatch[1] && hrefMatch[1] !== "fmp4") {
        return `https://www.dailymotion.com/video/${hrefMatch[1]}`;
      }

      // 2. In iframe: check document.referrer (parent webpage that embeds the player)
      if (document.referrer) {
        const refMatch = document.referrer.match(/(?:dailymotion\.com\/video\/|dai\.ly\/|[?&]video=)([a-zA-Z0-9]+)/);
        if (refMatch && refMatch[1] && refMatch[1] !== "fmp4") {
          return `https://www.dailymotion.com/video/${refMatch[1]}`;
        }
      }

      // 3. Check window.location.search or window.location.href for video param
      const searchMatch = window.location.search.match(/[?&]video=([a-zA-Z0-9]+)/) ||
                          window.location.href.match(/[?&]video=([a-zA-Z0-9]+)/);
      if (searchMatch && searchMatch[1] && searchMatch[1] !== "fmp4") {
        return `https://www.dailymotion.com/video/${searchMatch[1]}`;
      }

      // 4. Check canonical or og:url meta tags in document
      try {
        const canon = document.querySelector('link[rel="canonical"]')?.href ||
                      document.querySelector('meta[property="og:url"]')?.content || "";
        const m = canon.match(/(?:dailymotion\.com\/video\/|dai\.ly\/)([a-zA-Z0-9]+)/);
        if (m && m[1] && m[1] !== "fmp4") {
          return `https://www.dailymotion.com/video/${m[1]}`;
        }
      } catch (_) {}

      // 5. Inspect global player configs or script tags
      try {
        const cfg = window.__PLAYER_CONFIG__ || window.playerConfig;
        const vid = cfg?.video?.id || cfg?.metadata?.id || cfg?.context?.video || cfg?.player?.videoId;
        if (vid && typeof vid === "string" && vid.length >= 4 && vid !== "fmp4") {
          return `https://www.dailymotion.com/video/${vid}`;
        }
      } catch (_) {}

      try {
        const scripts = document.querySelectorAll("script");
        for (const s of scripts) {
          const txt = s.textContent || "";
          if (txt.includes("videoId") || txt.includes("video_id") || txt.includes("/video/")) {
            const m = txt.match(/["'](?:videoId|video_id|xid)["']\s*:\s*["']([a-zA-Z0-9]+)["']/i) ||
                      txt.match(/"id"\s*:\s*"(x[a-zA-Z0-9]{5,})"/i) ||
                      txt.match(/(?:dailymotion\.com\/video\/|dai\.ly\/)([a-zA-Z0-9]+)/i);
            if (m && m[1] && m[1] !== "fmp4" && m[1] !== "html") {
              return `https://www.dailymotion.com/video/${m[1]}`;
            }
          }
        }
      } catch (_) {}

      // 6. Direct video element stream URL (e.g. vod*.cf.dmcdn.net/.../manifest.m3u8)
      if (videoElement) {
        const vSrc = videoElement.currentSrc || videoElement.src || "";
        if (vSrc && !vSrc.startsWith("blob:") && !vSrc.startsWith("data:")) {
          if (vSrc.includes("dmcdn.net") || vSrc.includes(".m3u8")) {
            return cleanMetaUrl(vSrc);
          }
        }
      }

      // 7. If still on geo.dailymotion player without video ID, try window.parent.location
      try {
        if (window.parent && window.parent.location && window.parent.location.href) {
          const pMatch = window.parent.location.href.match(/(?:dailymotion\.com\/video\/|dai\.ly\/)([a-zA-Z0-9]+)/);
          if (pMatch && pMatch[1]) {
            return `https://www.dailymotion.com/video/${pMatch[1]}`;
          }
        }
      } catch (_) {}

      // 8. If nothing matched and we are on geo.dailymotion player HTML:
      // NEVER return bare geo.dailymotion.com/player/xtv3w.html!
      if (currentHref.includes("geo.dailymotion.com/player/")) {
        const anyDm = document.documentElement.innerHTML.match(/https:\/\/[^"'\s]*dmcdn\.net[^"'\s]*\.m3u8/);
        if (anyDm && anyDm[0]) {
          return anyDm[0];
        }
      }

      return currentHref;
    }

    // Threads handler (threads.net & threads.com)
    if (host.includes("threads.net") || host.includes("threads.com")) {
      // 1. Direct stream from active video (Threads is not supported by yt-dlp, so direct stream muxing is used)
      const streams = extractMetaStreams(videoElement);
      if (streams && streams.videoStream) {
        return streams.videoStream;
      }

      // 2. Post permalink
      if (currentHref.includes("/post/")) {
        return currentHref.split("?")[0];
      }

      // 3. Ancestor search for post link
      if (videoElement) {
        try {
          const postContainer = videoElement.closest('article')
                             || videoElement.closest('div[data-pressable-container="true"]')
                             || videoElement.closest('div[dir="auto"]')
                             || videoElement.parentElement;
          if (postContainer) {
            const postLink = postContainer.querySelector('a[href*="/post/"]');
            if (postLink && postLink.href) {
              return postLink.href.split("?")[0];
            }
          }
        } catch (_) {}
      }

      // Never return bare feed url
      if (currentHref.endsWith("threads.net/") || currentHref.endsWith("threads.net") || currentHref.endsWith("threads.com/") || currentHref.endsWith("threads.com")) {
        return "";
      }
      return currentHref;
    }

    // Pinterest handler (pinterest.com & pin.it & pinimg.com)
    if (host.includes("pinterest.com") || host.includes("pin.it") || host.includes("pinimg.com")) {
      const cleanPinUrl = (raw) => {
        if (!raw) return "";
        try {
          const u = new URL(raw, window.location.origin);
          const m = u.pathname.match(/\/pin\/(\d+)/i);
          if (m && m[1]) {
            return `https://www.pinterest.com/pin/${m[1]}/`;
          }
          if (u.hostname.includes("pin.it")) {
            return raw.split("?")[0];
          }
        } catch (_) {}
        return "";
      };

      // 1. If currently on a Pin page or closeup overlay
      if (currentHref.includes("/pin/")) {
        const cleaned = cleanPinUrl(currentHref);
        if (cleaned) return cleaned;
      }

      // 2. Search nearest Pin card container for the pin link
      if (videoElement) {
        try {
          const pinContainer = videoElement.closest('[data-test-id="pin"]')
                            || videoElement.closest('[data-test-id="closeup-body"]')
                            || videoElement.closest('[data-test-id="pin-visual-wrapper"]')
                            || videoElement.closest('div[data-grid-item]')
                            || videoElement.closest('div[class*="Pin"]')
                            || videoElement.closest('article')
                            || videoElement.parentElement;
          if (pinContainer) {
            const pinLink = pinContainer.querySelector('a[href*="/pin/"]');
            if (pinLink && pinLink.href) {
              const cleaned = cleanPinUrl(pinLink.href);
              if (cleaned) return cleaned;
            }
          }
        } catch (_) {}
      }

      // 3. Search all pin cards in viewport containing videoElement
      try {
        const allPins = Array.from(document.querySelectorAll('[data-test-id="pin"], div[data-grid-item], article'));
        for (const p of allPins) {
          if (videoElement && p.contains(videoElement)) {
            const a = p.querySelector('a[href*="/pin/"]');
            if (a && a.href) {
              const cleaned = cleanPinUrl(a.href);
              if (cleaned) return cleaned;
            }
          }
        }
      } catch (_) {}

      // 4. Look for HLS master playlist in network streams or performance entries (HLS has AUDIO!)
      refreshCapturedMedia();
      if (cachedNetworkStreams && cachedNetworkStreams.length > 0) {
        const hlsStream = cachedNetworkStreams.find(s => (s.url.includes("pinimg.com") || s.url.includes("pinterest")) && (s.kind === "manifest" || s.url.includes(".m3u8")) && !s.isSegment);
        if (hlsStream && hlsStream.url) {
          return hlsStream.url;
        }
      }

      try {
        const perfEntries = performance.getEntriesByType("resource");
        const hlsEntry = perfEntries.find(r => r.name && r.name.includes("pinimg.com") && r.name.includes(".m3u8"));
        if (hlsEntry && hlsEntry.name) {
          return hlsEntry.name;
        }
      } catch (_) {}

      // 5. If only direct pinimg stream exists on video tag:
      if (videoElement) {
        const vSrc = videoElement.currentSrc || videoElement.src || "";
        if (vSrc && vSrc.startsWith("http") && !vSrc.startsWith("blob:")) {
          // Convert muted .../720p/...mp4 to .../hls/...m3u8 which contains the audio track
          if (vSrc.includes("/videos/mc/") && vSrc.includes(".mp4")) {
            const hlsCandidate = vSrc.replace(/\/videos\/mc\/(?:\d+p|exp\d+)\//, "/videos/mc/hls/").replace(/\.mp4(?:\?.*)?$/, ".m3u8");
            return hlsCandidate;
          }
          return cleanMetaUrl(vSrc);
        }
        const source = videoElement.querySelector("source[src]");
        if (source && source.src && source.src.startsWith("http") && !source.src.startsWith("blob:")) {
          if (source.src.includes("/videos/mc/") && source.src.includes(".mp4")) {
            return source.src.replace(/\/videos\/mc\/(?:\d+p|exp\d+)\//, "/videos/mc/hls/").replace(/\.mp4(?:\?.*)?$/, ".m3u8");
          }
          return cleanMetaUrl(source.src);
        }
      }

      // 6. Never return generic feed page
      if (currentHref.endsWith("pinterest.com/") || currentHref.includes("/ideas/") || currentHref.includes("/today/")) {
        return "";
      }
      return currentHref;
    }

    // Reddit handler
    if (host.includes("reddit.com") || host.includes("redd.it")) {
      try {
        const player = videoElement?.closest('shreddit-player') || document.querySelector('shreddit-player');
        if (player) {
          const pSrc = player.getAttribute('src');
          if (pSrc && pSrc.startsWith("http")) return pSrc;
          const streamUrl = player.getAttribute('stream-url');
          if (streamUrl && streamUrl.startsWith("http")) return streamUrl;
          const pkg = player.getAttribute('packaged-media-json');
          if (pkg) {
            const data = JSON.parse(pkg);
            if (data.playbackMp4Url) return data.playbackMp4Url;
            if (data.hlsUrl) return data.hlsUrl;
          }
        }
      } catch (_) {}
      if (videoElement) {
        const vSrc = videoElement.currentSrc || videoElement.src || "";
        if (vSrc && vSrc.startsWith("http") && !vSrc.startsWith("blob:")) {
          return cleanMetaUrl(vSrc);
        }
        const source = videoElement.querySelector("source[src]");
        if (source && source.src && source.src.startsWith("http") && !source.src.startsWith("blob:")) {
          return cleanMetaUrl(source.src);
        }
      }
      try {
        const html = document.documentElement.innerHTML;
        const vReddMatch = html.match(/https:\/\/v\.redd\.it\/[a-zA-Z0-9_-]+\/(?:HLSPlaylist\.m3u8|DASH_[0-9]+\.mp4)/);
        if (vReddMatch && vReddMatch[0]) {
          return vReddMatch[0];
        }
      } catch (_) {}
      return currentHref;
    }

    // TikTok handler
    if (host.includes("tiktok.com")) {
      // 1. Current page URL if already on a specific video page
      if (currentHref.includes("/video/") || currentHref.includes("/v/")) {
        return currentHref;
      }

      // 2. Resolve TikTok data (Fiber + DOM) for given videoElement or active feed video
      let ttEl = videoElement;
      if (!ttEl) {
        const videos = Array.from(document.querySelectorAll("video")).filter(v => {
          const r = v.getBoundingClientRect();
          return r.width > 80 && r.height > 80 && r.bottom > 50 && r.top < window.innerHeight - 50;
        });
        const playing = videos.find(v => !v.paused && !v.ended && v.currentTime > 0);
        ttEl = playing || videos[0] || null;
      }

      if (ttEl) {
        const ttData = getTikTokData(ttEl);
        if (ttData && ttData.canonicalUrl) {
          if (ttData.qualities && ttData.qualities.length > 0) {
            detectedQualitiesMap.set(ttData.canonicalUrl, ttData.qualities);
          }
          return ttData.canonicalUrl;
        }

        // Direct stream if non-blob
        const vSrc = ttEl.currentSrc || ttEl.src || "";
        if (vSrc && vSrc.startsWith("http") && !vSrc.startsWith("blob:") &&
            !vSrc.includes(".image") && !vSrc.includes(".jpeg") && !vSrc.includes(".jpg") && !vSrc.includes(".webp")) {
          return cleanMetaUrl(vSrc);
        }
      }

      // 3. Real-time network sniffer for captured tiktokcdn.com MP4 stream (strictly non-image, actual video)
      refreshCapturedMedia();
      if (cachedNetworkStreams && cachedNetworkStreams.length > 0) {
        const videoStreams = cachedNetworkStreams.filter(s => {
          const u = (s.url || "").toLowerCase();
          return (u.includes("tiktokcdn.com") || u.includes("tiktok.com") || u.includes("akamaized.net") || u.includes("byteoversea.com")) &&
                 !s.isSegment &&
                 !u.includes(".image") && !u.includes(".jpeg") && !u.includes(".jpg") && !u.includes(".webp") && !u.includes(".png") &&
                 (u.includes("/video/") || u.includes("/play/") || u.includes("v16") || u.includes("v19") || u.includes("v77") || s.ext === "mp4");
        });
        if (videoStreams.length > 0) {
          videoStreams.sort((a, b) => (b.size || 0) - (a.size || 0));
          return videoStreams[0].url;
        }
      }

      // NEVER return bare tiktok home feed (tiktok.com/ or /foryou) as a media download URL
      return (currentHref.includes("/video/") || currentHref.includes("/v/")) ? currentHref : "";
    }

    // LinkedIn handler
    if (host.includes("linkedin.com")) {
      if (currentHref.includes("/feed/update/") || currentHref.includes("/posts/")) {
        return currentHref;
      }
      if (videoElement) {
        try {
          const postContainer = videoElement.closest('[data-urn]') ||
                                videoElement.closest('[data-id]') ||
                                videoElement.closest('.feed-shared-update-v2') ||
                                videoElement.closest('div[class*="feed-shared-update"]');
          if (postContainer) {
            const urn = postContainer.getAttribute('data-urn') || postContainer.getAttribute('data-id');
            if (urn && urn.includes("activity")) {
              const actMatch = urn.match(/urn:li:activity:(\d+)/);
              if (actMatch) {
                return `https://www.linkedin.com/feed/update/urn:li:activity:${actMatch[1]}`;
              }
            }
            const permalink = postContainer.querySelector('a[href*="/feed/update/"]');
            if (permalink && permalink.href) {
              return permalink.href;
            }
          }
        } catch (_) {}
      }
      // Check if network sniffer captured the direct media stream (e.g. dms.licdn.com)
      if (cachedNetworkStreams && cachedNetworkStreams.length > 0) {
        const best = cachedNetworkStreams.find(s => s.kind === "manifest") ||
                     cachedNetworkStreams.find(s => !s.isSegment && s.ext === "mp4") ||
                     cachedNetworkStreams[0];
        if (best && best.url) {
          return best.url;
        }
      }
      return currentHref;
    }

    // Tumblr handler (tumblr.com & media.tumblr.com & vtt.tumblr.com)
    if (host.includes("tumblr.com") || host.includes("media.tumblr.com")) {
      // 1. Direct video element stream URL from Tumblr Media CDN (100% working direct progressive MP4)
      if (videoElement) {
        const vSrc = videoElement.currentSrc || videoElement.src || "";
        if (vSrc && vSrc.startsWith("http") && !vSrc.startsWith("blob:")) {
          return cleanMetaUrl(vSrc);
        }
        const source = videoElement.querySelector("source[src]");
        if (source && source.src && source.src.startsWith("http") && !source.src.startsWith("blob:")) {
          return cleanMetaUrl(source.src);
        }
      }

      // 2. Real-time network sniffer for captured media.tumblr.com / va.media.tumblr.com MP4 stream
      if (cachedNetworkStreams && cachedNetworkStreams.length > 0) {
        const tStream = cachedNetworkStreams.find(s => 
          (s.url.includes("media.tumblr.com") || s.url.includes("vtt.tumblr.com") || s.url.includes("tumblr.com")) && 
          s.ext === "mp4" && !s.isSegment
        );
        if (tStream && tStream.url) {
          return tStream.url;
        }
      }

      // 3. Search page HTML / DOM for direct va.media.tumblr.com MP4
      try {
        const anyTumblr = document.documentElement.innerHTML.match(/https:\/\/(?:va\.)?media\.tumblr\.com\/[a-zA-Z0-9_-]+\.mp4/);
        if (anyTumblr && anyTumblr[0]) {
          return anyTumblr[0];
        }
      } catch (_) {}

      // 4. Tumblr video iframe support
      if (currentHref.includes("tumblr.com/video/")) {
        if (videoElement && videoElement.currentSrc) {
          return videoElement.currentSrc;
        }
      }

      // 5. Single post fallback (only if user is viewing a single post)
      if (currentHref.includes("/post/")) {
        return currentHref;
      }
      if (videoElement) {
        try {
          const postArticle = videoElement.closest('article') || videoElement.closest('[data-testid="post"]');
          if (postArticle) {
            const postLink = postArticle.querySelector('a[href*="/post/"]');
            if (postLink && postLink.href) {
              return postLink.href;
            }
          }
        } catch (_) {}
      }

      return currentHref;
    }

    // Instagram handler (instagram.com)
    if (host.includes("instagram.com")) {
      // 1. Post or Reel permalink if on single reel/post view (/reel/, /reels/, /p/)
      const mHref = currentHref.match(/\/(reel|reels|p)\/([a-zA-Z0-9_-]+)/i);
      if (mHref && mHref[2]) {
        return `https://www.instagram.com/reel/${mHref[2]}/`;
      }

      // 2. React Fiber shortcode inspection on videoElement
      if (videoElement) {
        try {
          let cur = videoElement;
          let depth = 0;
          while (cur && depth < 8) {
            const fiberKey = Object.keys(cur).find(k => k.startsWith("__reactFiber$") || k.startsWith("__reactProps$"));
            if (fiberKey) {
              let fiber = cur[fiberKey];
              let fDepth = 0;
              while (fiber && fDepth < 20) {
                const p = fiber.memoizedProps;
                if (p) {
                  const code = p.code || p.shortcode || p.post?.code || p.post?.shortcode || p.item?.code || p.media?.code || p.items?.[0]?.code;
                  if (code && typeof code === "string" && /^[a-zA-Z0-9_-]{8,15}$/.test(code)) {
                    return `https://www.instagram.com/reel/${code}/`;
                  }
                }
                fiber = fiber.return;
                fDepth++;
              }
            }
            cur = cur.parentElement;
            depth++;
          }
        } catch (_) {}

        // 3. Search card container strictly (article, dialog)
        try {
          const container = videoElement.closest('article')
                         || videoElement.closest('div[role="article"]')
                         || videoElement.closest('div[role="dialog"]')
                         || videoElement.closest('div[class*="x1qjc9v5"]')
                         || videoElement.closest('div[class*="_ab6-"]')
                         || videoElement.closest('div[class*="_aa0-"]');
          if (container) {
            const timeLink = container.querySelector('time')?.closest('a');
            if (timeLink && timeLink.href) {
              const tm = timeLink.href.match(/\/(reel|reels|p)\/([a-zA-Z0-9_-]+)/i);
              if (tm && tm[2]) {
                return `https://www.instagram.com/reel/${tm[2]}/`;
              }
            }

            const link = container.querySelector('a[href*="/reel/"], a[href*="/reels/"], a[href*="/p/"]');
            if (link && link.href) {
              const lm = link.href.match(/\/(reel|reels|p)\/([a-zA-Z0-9_-]+)/i);
              if (lm && lm[2]) {
                return `https://www.instagram.com/reel/${lm[2]}/`;
              }
            }
          }
        } catch (_) {}
      }

      // Never return bare feed url
      if (currentHref.endsWith("instagram.com/") || currentHref.endsWith("instagram.com")) {
        return "";
      }
      return currentHref;
    }

    // Facebook handler (facebook.com & fb.watch)
    if (host.includes("facebook.com") || host.includes("fb.watch")) {
      if (currentHref.includes("/watch") || currentHref.includes("/videos/") || currentHref.includes("/reel/") || currentHref.includes("story.php")) {
        return currentHref;
      }
      if (videoElement) {
        try {
          const card = videoElement.closest('[role="article"], [data-pagelet*="FeedUnit"], div[class*="x1yztbdb"], div[class*="x1lliihq"], div[data-visualcompletion="ignore-dynamic-hydration"]');
          if (card) {
            const link = card.querySelector('a[href*="/videos/"], a[href*="/reel/"], a[href*="/watch/"], a[href*="fb.watch"], a[href*="story.php"], a[href*="/posts/"]');
            if (link && link.href) {
              return link.href;
            }
          }
        } catch (_) {}
        const vSrc = videoElement.currentSrc || videoElement.src || "";
        if (vSrc && vSrc.startsWith("http") && !vSrc.startsWith("blob:") && !vSrc.startsWith("data:")) {
          return cleanMetaUrl(vSrc);
        }
        const source = videoElement.querySelector("source[src]");
        if (source && source.src && source.src.startsWith("http") && !source.src.startsWith("blob:")) {
          return cleanMetaUrl(source.src);
        }
      }
      refreshCapturedMedia();
      if (cachedNetworkStreams && cachedNetworkStreams.length > 0) {
        const fbStream = cachedNetworkStreams.find(s => (s.url.includes("fbcdn.net") || s.url.includes("facebook.com")) && s.ext === "mp4" && !s.isSegment);
        if (fbStream && fbStream.url) {
          return fbStream.url;
        }
      }
      return currentHref;
    }

    // X (Twitter) handler (x.com & twitter.com)
    if (host.includes("x.com") || host.includes("twitter.com")) {
      const cleanTweetUrl = (raw) => {
        if (!raw) return "";
        try {
          const u = new URL(raw, window.location.origin);
          const m = u.pathname.match(/(?:\/[^\/]+)?\/status\/(\d+)/i);
          if (m && m[1]) {
            return `https://x.com/i/status/${m[1]}`;
          }
        } catch (_) {}
        return "";
      };

      if (currentHref.includes("/status/")) {
        const cleaned = cleanTweetUrl(currentHref);
        if (cleaned) return cleaned;
      }

      if (videoElement) {
        try {
          const tweet = videoElement.closest('article[data-testid="tweet"]')
                     || videoElement.closest('div[data-testid="cellInnerDiv"]')
                     || videoElement.closest('[data-testid="expanded-media-root"]')
                     || videoElement.closest('div[role="dialog"]')
                     || videoElement.closest('article');
          if (tweet) {
            const timeAnchor = tweet.querySelector('time')?.closest('a');
            if (timeAnchor && timeAnchor.href) {
              const cleaned = cleanTweetUrl(timeAnchor.href);
              if (cleaned) return cleaned;
            }
            const link = tweet.querySelector('a[href*="/status/"]');
            if (link && link.href) {
              const cleaned = cleanTweetUrl(link.href);
              if (cleaned) return cleaned;
            }
          }
        } catch (_) {}

        const vSrc = videoElement.currentSrc || videoElement.src || "";
        if (vSrc && vSrc.startsWith("http") && !vSrc.startsWith("blob:") && !vSrc.startsWith("data:")) {
          return cleanMetaUrl(vSrc);
        }
        const source = videoElement.querySelector("source[src]");
        if (source && source.src && source.src.startsWith("http") && !source.src.startsWith("blob:")) {
          return cleanMetaUrl(source.src);
        }
      }

      // Check all tweets in viewport containing videoElement
      try {
        const allTweets = Array.from(document.querySelectorAll('article[data-testid="tweet"], div[data-testid="cellInnerDiv"]'));
        for (const tw of allTweets) {
          if (videoElement && tw.contains(videoElement)) {
            const timeAnchor = tw.querySelector('time')?.closest('a') || tw.querySelector('a[href*="/status/"]');
            if (timeAnchor && timeAnchor.href) {
              const cleaned = cleanTweetUrl(timeAnchor.href);
              if (cleaned) return cleaned;
            }
          }
        }
      } catch (_) {}

      // Fallback: captured network streams (twimg master playlist or progressive mp4)
      refreshCapturedMedia();
      if (cachedNetworkStreams && cachedNetworkStreams.length > 0) {
        const xStream = cachedNetworkStreams.find(s => (s.url.includes("twimg.com") || s.url.includes("twitter.com") || s.url.includes("x.com")) && (s.ext === "mp4" || s.kind === "manifest" || s.url.includes(".m3u8")) && !s.isSegment);
        if (xStream && xStream.url) {
          return xStream.url;
        }
      }

      // Fallback: check performance resource entries
      try {
        const perfEntries = performance.getEntriesByType("resource");
        const twimgEntry = perfEntries.find(r => r.name && r.name.includes("video.twimg.com") && (r.name.includes(".m3u8") || r.name.includes(".mp4")));
        if (twimgEntry && twimgEntry.name) {
          return twimgEntry.name;
        }
      } catch (_) {}

      // Never return feed page as targetUrl
      if (currentHref.includes("/home") || currentHref.includes("/explore") || currentHref.includes("/notifications") || currentHref.endsWith("x.com/") || currentHref.endsWith("twitter.com/")) {
        return "";
      }
      return currentHref;
    }

    const isOtherPortal = host.includes("vimeo.com") || 
                          host.includes("rumble.com") || 
                          host.includes("bilibili.com") || 
                          host.includes("twitch.tv");
    if (isOtherPortal) {
      return currentHref;
    }

    // Direct video element stream URL fallback (non-blob)
    const src = videoElement?.currentSrc || videoElement?.src;
    if (src && !src.startsWith("blob:") && !src.startsWith("data:")) {
      return src;
    }

    // If stream is blob/MSE or empty, resolve from real-time network sniffer
    refreshCapturedMedia();
    if (cachedNetworkStreams && cachedNetworkStreams.length > 0) {
      const best = cachedNetworkStreams.find(s => s.kind === "manifest") ||
                   cachedNetworkStreams.find(s => !s.isSegment && s.ext === "mp4") ||
                   cachedNetworkStreams[0];
      if (best && best.url) {
        return best.url;
      }
    }

    return currentHref;
  }

  function isFacebookPageOrAuthorName(txt, container) {
    if (!txt) return true;
    const lower = txt.trim().toLowerCase();
    if (isGenericTitleText(lower)) return true;

    if (container) {
      // Find all author / page name links in the post header
      const authorLinks = container.querySelectorAll('h2 a, h3 a, h4 a, a[role="link"][href*="/user/"], a[role="link"][href*="profile.php"], a[role="link"][href*="/groups/"], a[role="link"][href*="/pages/"], strong, a[aria-hidden="true"] + span a');
      for (const al of authorLinks) {
        const aTxt = (al.textContent || "").trim().toLowerCase();
        if (aTxt && (lower === aTxt || lower.startsWith(aTxt + " -") || lower.startsWith(aTxt + " |"))) {
          return true;
        }
      }
    }
    return false;
  }

  function getFacebookVideoTitle(videoElement) {
    if (!videoElement) return "";

    const container = videoElement.closest('[role="article"]')
                   || videoElement.closest('[data-pagelet*="FeedUnit"]')
                   || videoElement.closest('div[class*="x1yztbdb"]')
                   || videoElement.closest('div[class*="x1lliihq"]')
                   || videoElement.closest('article')
                   || videoElement.parentElement;

    let candidateTitle = "";
    let candidateDesc = "";

    // 1. Check for dedicated video title (e.g. Facebook Watch / Show title, distinct from author name)
    if (container) {
      const headings = container.querySelectorAll('h1, span[class*="x193iq5w"], div[class*="xz9dl7a"]');
      for (const h of headings) {
        const txt = (h.textContent || "").trim();
        if (txt && txt.length > 2 && !isFacebookPageOrAuthorName(txt, container)) {
          if (!h.closest('a[role="link"]') && !h.closest('h2') && !h.closest('h3')) {
            candidateTitle = txt;
            break;
          }
        }
      }

      // 2. Look for post description / message text in container
      const msgSelectors = [
        '[data-ad-preview="message"]',
        '[data-ad-comet-preview="message"]',
        'div[dir="auto"][style*="text-align"]',
        'div[class*="x1iorvi4"]',
        'div[dir="auto"]',
        'span[dir="auto"]'
      ];
      for (const sel of msgSelectors) {
        const el = container.querySelector(sel);
        if (el && el.textContent) {
          const txt = el.textContent.trim();
          if (txt && txt.length > 2 && !isFacebookPageOrAuthorName(txt, container)) {
            candidateDesc = txt;
            break;
          }
        }
      }
    }

    // Fallback description from meta tags
    if (!candidateDesc) {
      const ogDesc = document.querySelector('meta[property="og:description"]')?.content
                  || document.querySelector('meta[name="description"]')?.content;
      if (ogDesc && ogDesc.trim() && !isFacebookPageOrAuthorName(ogDesc, container)) {
        candidateDesc = ogDesc.trim();
      }
    }

    // Fallback title from meta tags (ensure it's not the page name!)
    if (!candidateTitle) {
      const ogTitle = document.querySelector('meta[property="og:video:title"]')?.content
                   || document.querySelector('meta[property="og:title"]')?.content;
      if (ogTitle && ogTitle.trim()) {
        const cleanOg = ogTitle.replace(/ \| Facebook$/i, "").replace(/ - Facebook$/i, "").trim();
        if (cleanOg && !isFacebookPageOrAuthorName(cleanOg, container)) {
          candidateTitle = cleanOg;
        }
      }
    }

    // Rule 1: Use actual Video Title if found and not page name
    if (candidateTitle && !isFacebookPageOrAuthorName(candidateTitle, container)) {
      return candidateTitle.replace(/[\r\n\t]+/g, " ").trim();
    }

    // Rule 2: If Title not found or is Page Name, use first 30 characters of Description
    if (candidateDesc) {
      const cleanDesc = candidateDesc.replace(/[\r\n\t]+/g, " ").replace(/\s+/g, " ").trim();
      if (cleanDesc.length > 0) {
        return cleanDesc.substring(0, 30).trim();
      }
    }

    // Rule 3: Timelapse fallback (if neither title nor description found)
    const now = new Date();
    const pad = n => String(n).padStart(2, '0');
    return `Video_${now.getFullYear()}${pad(now.getMonth()+1)}${pad(now.getDate())}_${pad(now.getHours())}${pad(now.getMinutes())}${pad(now.getSeconds())}`;
  }

  function getXVideoTitle(videoElement) {
    let tweetEl = videoElement;
    if (!tweetEl) {
      const videos = Array.from(document.querySelectorAll("video")).filter(v => {
        const r = v.getBoundingClientRect();
        return r.width > 80 && r.height > 80 && r.bottom > 50 && r.top < window.innerHeight - 50;
      });
      tweetEl = videos.find(v => !v.paused && !v.ended && v.currentTime > 0) || videos[0] || null;
    }

    let container = null;
    if (tweetEl && tweetEl.closest) {
      container = tweetEl.closest('article[data-testid="tweet"]')
               || tweetEl.closest('div[data-testid="cellInnerDiv"]')
               || tweetEl.closest('[data-testid="expanded-media-root"]')
               || tweetEl.closest('div[role="dialog"]')
               || tweetEl.closest('article');
    }
    if (!container) {
      container = document.querySelector('div[role="dialog"] article[data-testid="tweet"]')
               || document.querySelector('article[data-testid="tweet"]');
    }

    let tweetText = "";
    let authorHandle = "";
    let authorName = "";

    if (container) {
      // 1. Tweet text caption
      const textEl = container.querySelector('[data-testid="tweetText"]');
      if (textEl && textEl.textContent) {
        const clean = textEl.textContent.trim().replace(/[\r\n\t]+/g, " ").replace(/\s+/g, " ");
        if (clean && !isGenericTitleText(clean)) {
          tweetText = clean;
        }
      }

      // 2. Author handle & name
      const userEl = container.querySelector('[data-testid="User-Name"]');
      if (userEl) {
        const links = userEl.querySelectorAll('a[href^="/"]');
        for (const a of links) {
          const txt = (a.textContent || "").trim();
          if (txt.startsWith("@")) {
            authorHandle = txt;
            break;
          }
        }
        if (!authorHandle) {
          const textNodes = userEl.innerText || userEl.textContent || "";
          const m = textNodes.match(/@([a-zA-Z0-9_]+)/);
          if (m) authorHandle = "@" + m[1];
        }
        const firstSpan = userEl.querySelector('span');
        if (firstSpan && firstSpan.textContent) {
          const n = firstSpan.textContent.trim();
          if (n && !n.startsWith("@")) authorName = n;
        }
      }
    }

    // If tweet text exists, use first 60 chars of tweet text
    if (tweetText) {
      if (tweetText.length > 60) {
        return tweetText.substring(0, 60).trim();
      }
      return tweetText;
    }

    // If author handle exists, use "Tweet by @handle"
    if (authorHandle) {
      return `Tweet by ${authorHandle}`;
    }
    if (authorName) {
      return `Tweet by ${authorName}`;
    }

    // Fallback to og:description / meta if non-generic
    const ogDesc = document.querySelector('meta[property="og:description"]')?.content
                || document.querySelector('meta[name="description"]')?.content;
    if (ogDesc && ogDesc.trim() && !isGenericTitleText(ogDesc)) {
      const clean = ogDesc.replace(/[\r\n\t]+/g, " ").trim();
      if (clean.length > 50) return clean.substring(0, 50).trim();
      return cleanMetaUrl(clean);
    }

    // Timelapse fallback
    const now = new Date();
    const pad = n => String(n).padStart(2, '0');
    return `Tweet_${now.getFullYear()}${pad(now.getMonth()+1)}${pad(now.getDate())}_${pad(now.getHours())}${pad(now.getMinutes())}${pad(now.getSeconds())}`;
  }

  function getPinterestVideoTitle(videoElement) {
    let pinEl = videoElement;
    if (!pinEl) {
      const videos = Array.from(document.querySelectorAll("video")).filter(v => {
        const r = v.getBoundingClientRect();
        return r.width > 80 && r.height > 80 && r.bottom > 50 && r.top < window.innerHeight - 50;
      });
      pinEl = videos.find(v => !v.paused && !v.ended && v.currentTime > 0) || videos[0] || null;
    }

    let container = null;
    if (pinEl && pinEl.closest) {
      container = pinEl.closest('[data-test-id="pin"]')
               || pinEl.closest('[data-test-id="closeup-body"]')
               || pinEl.closest('[data-test-id="pin-visual-wrapper"]')
               || pinEl.closest('div[data-grid-item]')
               || pinEl.closest('div[class*="Pin"]')
               || pinEl.closest('article');
    }
    if (!container) {
      container = document.querySelector('[data-test-id="closeup-body"]')
               || document.querySelector('[data-test-id="pin"]');
    }

    let candidateTitle = "";
    if (container) {
      const titleEl = container.querySelector('[data-test-id="pin-title"], [data-test-id="closeup-title"], h1, h2, h3');
      if (titleEl && titleEl.textContent) {
        const clean = titleEl.textContent.trim().replace(/[\r\n\t]+/g, " ").replace(/\s+/g, " ");
        if (clean && !isGenericTitleText(clean)) {
          candidateTitle = clean;
        }
      }
      if (!candidateTitle) {
        const descEl = container.querySelector('[data-test-id="pin-description"], [data-test-id="closeup-description"], [data-test-id="truncated-description"]');
        if (descEl && descEl.textContent) {
          const clean = descEl.textContent.trim().replace(/[\r\n\t]+/g, " ").replace(/\s+/g, " ");
          if (clean && !isGenericTitleText(clean)) {
            candidateTitle = clean.length > 50 ? clean.substring(0, 50).trim() : clean;
          }
        }
      }
    }

    if (!candidateTitle) {
      const ogTitle = document.querySelector('meta[property="og:title"]')?.content
                   || document.querySelector('meta[name="twitter:title"]')?.content
                   || document.title;
      if (ogTitle) {
        const clean = ogTitle.replace(/ \| Pinterest$/i, "").replace(/ - Pinterest$/i, "").trim();
        if (clean && !isGenericTitleText(clean)) {
          candidateTitle = clean;
        }
      }
    }

    if (candidateTitle) return candidateTitle;

    const now = new Date();
    const pad = n => String(n).padStart(2, '0');
    return `Pin_${now.getFullYear()}${pad(now.getMonth()+1)}${pad(now.getDate())}_${pad(now.getHours())}${pad(now.getMinutes())}${pad(now.getSeconds())}`;
  }

  function getInstagramVideoTitle(videoElement) {
    let vidEl = videoElement || activeVideo || findPrimaryVideo();
    let container = null;
    if (vidEl && vidEl.closest) {
      container = vidEl.closest('article')
               || vidEl.closest('div[role="article"]')
               || vidEl.closest('div[class*="x1qjc9v5"]')
               || vidEl.closest('div[role="dialog"]')
               || vidEl.closest('div[class*="_ab6-"]')
               || vidEl.closest('div[class*="_aa0-"]')
               || vidEl.closest('main')
               || vidEl.parentElement;
    }
    if (!container) {
      // Find the article intersecting the center of the viewport (the active reel on screen)
      const centerY = window.innerHeight / 2;
      const allArticles = Array.from(document.querySelectorAll('article, div[role="article"]'));
      container = allArticles.find(art => {
        const r = art.getBoundingClientRect();
        return r.top < centerY && r.bottom > centerY;
      }) || allArticles[0] || document.querySelector('div[role="dialog"]') || document.querySelector('main');
    }

    let captionText = "";
    let authorHandle = "";

    if (container) {
      // 1. Reel/Post caption in h1 or caption classes
      const captionEl = container.querySelector('h1')
                     || container.querySelector('div[class*="_a9zs"]')
                     || container.querySelector('span[class*="_aaco"]')
                     || container.querySelector('span[dir="auto"]')
                     || container.querySelector('div[dir="auto"]');
      if (captionEl && captionEl.textContent) {
        const clean = captionEl.textContent.trim().replace(/[\r\n\t]+/g, " ").replace(/\s+/g, " ");
        if (clean && !isGenericTitleText(clean)) {
          captionText = clean;
        }
      }

      // 2. Author handle
      const userEl = container.querySelector('header a[role="link"]')
                  || container.querySelector('header a')
                  || container.querySelector('a[href^="/"][role="link"]')
                  || container.querySelector('span[class*="_aauo"]');
      if (userEl && userEl.textContent) {
        const uText = userEl.textContent.trim().replace(/^@/, "");
        if (uText && !isGenericTitleText(uText)) {
          authorHandle = "@" + uText;
        }
      }
    }

    // 3. Fallback to parsing og:title or og:description
    if (!captionText) {
      const ogTitle = document.querySelector('meta[property="og:title"]')?.content || "";
      const m = ogTitle.match(/on Instagram:\s*["“']?(.*?)["”']?$/i);
      if (m && m[1] && m[1].trim() && !isGenericTitleText(m[1].trim())) {
        captionText = m[1].trim();
      }
      if (!authorHandle) {
        const mUser = ogTitle.match(/^([^:]+)\s+on Instagram/i);
        if (mUser && mUser[1]) authorHandle = "@" + mUser[1].trim();
      }
    }

    if (!captionText) {
      const ogDesc = document.querySelector('meta[property="og:description"]')?.content
                  || document.querySelector('meta[name="description"]')?.content;
      if (ogDesc && ogDesc.trim() && !isGenericTitleText(ogDesc.trim())) {
        const clean = ogDesc.replace(/[\r\n\t]+/g, " ").replace(/\s+/g, " ").trim();
        const afterColon = clean.includes(":") ? clean.substring(clean.indexOf(":") + 1).trim() : clean;
        if (afterColon && !isGenericTitleText(afterColon)) {
          captionText = afterColon;
        }
      }
    }

    if (captionText) {
      return captionText.length > 60 ? captionText.substring(0, 60).trim() : captionText;
    }

    if (authorHandle) {
      return `Instagram Reel by ${authorHandle}`;
    }

    const now = new Date();
    const pad = n => String(n).padStart(2, '0');
    return `Instagram_${now.getFullYear()}${pad(now.getMonth()+1)}${pad(now.getDate())}_${pad(now.getHours())}${pad(now.getMinutes())}${pad(now.getSeconds())}`;
  }

  function getThreadsVideoTitle(videoElement) {
    let vidEl = videoElement;
    if (!vidEl) {
      const videos = Array.from(document.querySelectorAll("video")).filter(v => {
        const r = v.getBoundingClientRect();
        return r.width > 80 && r.height > 80 && r.bottom > 50 && r.top < window.innerHeight - 50;
      });
      vidEl = videos.find(v => !v.paused && !v.ended && v.currentTime > 0) || videos[0] || null;
    }

    let container = null;
    if (vidEl && vidEl.closest) {
      container = vidEl.closest('article')
               || vidEl.closest('div[data-pressable-container="true"]')
               || vidEl.closest('div[dir="auto"]')
               || vidEl.parentElement;
    }
    if (!container) {
      container = document.querySelector('article')
               || document.querySelector('div[data-pressable-container="true"]');
    }

    let postText = "";
    let authorHandle = "";

    if (container) {
      // 1. Post text content
      const textEls = container.querySelectorAll('div[dir="auto"], span[dir="auto"]');
      for (const el of textEls) {
        const txt = el.textContent ? el.textContent.trim().replace(/[\r\n\t]+/g, " ").replace(/\s+/g, " ") : "";
        if (txt && txt.length > 2 && !txt.startsWith("@") && !isGenericTitleText(txt)) {
          postText = txt;
          break;
        }
      }

      // 2. Author handle
      const userEl = container.querySelector('a[href^="/@"]')
                  || container.querySelector('a[role="link"][href*="@"]');
      if (userEl) {
        const uHref = userEl.getAttribute("href") || "";
        const m = uHref.match(/@([a-zA-Z0-9_.]+)/);
        if (m && m[1]) {
          authorHandle = "@" + m[1];
        }
      }
    }

    // 3. Fallback to og:description / meta
    if (!postText) {
      const ogDesc = document.querySelector('meta[property="og:description"]')?.content
                  || document.querySelector('meta[name="description"]')?.content;
      if (ogDesc && ogDesc.trim() && !isGenericTitleText(ogDesc.trim())) {
        postText = ogDesc.replace(/[\r\n\t]+/g, " ").replace(/\s+/g, " ").trim();
      }
    }

    if (!authorHandle) {
      const ogTitle = document.querySelector('meta[property="og:title"]')?.content || "";
      const m = ogTitle.match(/@([a-zA-Z0-9_.]+)/);
      if (m && m[1]) authorHandle = "@" + m[1];
    }

    if (postText) {
      return postText.length > 60 ? postText.substring(0, 60).trim() : postText;
    }

    if (authorHandle) {
      return `Threads Post by ${authorHandle}`;
    }

    const now = new Date();
    const pad = n => String(n).padStart(2, '0');
    return `Threads_${now.getFullYear()}${pad(now.getMonth()+1)}${pad(now.getDate())}_${pad(now.getHours())}${pad(now.getMinutes())}${pad(now.getSeconds())}`;
  }

  function getVideoTitle(videoElement) {
    const hostname = window.location.hostname.toLowerCase();
    const host = hostname;
    const isFacebook = hostname.includes("facebook.com") || hostname.includes("fb.watch");

    // Facebook-specific title/description/timelapse resolver
    if (isFacebook) {
      return getFacebookVideoTitle(videoElement);
    }

    // X (Twitter) specific title resolver
    if (host.includes("x.com") || host.includes("twitter.com")) {
      return getXVideoTitle(videoElement);
    }

    // Pinterest specific title resolver
    if (host.includes("pinterest.com") || host.includes("pin.it") || host.includes("pinimg.com")) {
      return getPinterestVideoTitle(videoElement);
    }

    // Instagram specific title resolver
    if (host.includes("instagram.com")) {
      return getInstagramVideoTitle(videoElement);
    }

    // Threads specific title resolver
    if (host.includes("threads.net") || host.includes("threads.com")) {
      return getThreadsVideoTitle(videoElement);
    }

    let title = "";

    // 1. If on YouTube, always get current fresh YouTube video title from live DOM
    const isYouTube = hostname.includes("youtube.com") || hostname.includes("youtu.be");

    if (isYouTube) {
      try {
        const ytTitleEl = document.querySelector('h1.ytd-watch-metadata yt-formatted-string')
                       || document.querySelector('ytd-watch-metadata #title h1 yt-formatted-string')
                       || document.querySelector('ytd-watch-metadata #title h1')
                       || document.querySelector('#title h1 yt-formatted-string')
                       || document.querySelector('h1.title yt-formatted-string')
                       || document.querySelector('ytd-shorts [is-active] #overlay #title')
                       || document.querySelector('ytd-reel-video-renderer[is-active] #overlay #title');
        if (ytTitleEl && ytTitleEl.textContent) {
          title = ytTitleEl.textContent.trim();
        }
      } catch (_) {}

      // If live DOM hasn't rendered yet or title is empty, check authoritative in-page bridge
      if (!title || isGenericTitleText(title)) {
        try {
          const bridgeMeta = getYouTubeBridgeMetadata();
          if (bridgeMeta && bridgeMeta.title && !isGenericTitleText(bridgeMeta.title)) {
            title = bridgeMeta.title.trim();
          }
        } catch (_) {}
      }
    }

    // TikTok specific title extraction (Fiber + precise DOM caption)
    if (host.includes("tiktok.com")) {
      let ttEl = videoElement;
      if (!ttEl) {
        const videos = Array.from(document.querySelectorAll("video")).filter(v => {
          const r = v.getBoundingClientRect();
          return r.width > 80 && r.height > 80 && r.bottom > 50 && r.top < window.innerHeight - 50;
        });
        ttEl = videos.find(v => !v.paused && !v.ended && v.currentTime > 0) || videos[0] || null;
      }
      if (ttEl) {
        try {
          const ttData = getTikTokData(ttEl);
          if (ttData && ttData.desc && !isGenericTitleText(ttData.desc)) {
            title = ttData.desc;
          }
        } catch (_) {}

        if (!title || isGenericTitleText(title)) {
          const container = ttEl.closest('[data-e2e="recommend-list-item-container"]')
                         || ttEl.closest('[data-e2e="feed-item"]')
                         || ttEl.closest('[data-e2e="feed-video"]')
                         || ttEl.closest('div[class*="DivItemContainer"]')
                         || ttEl.closest('div[class*="DivVideoWrapper"]')
                         || ttEl.parentElement;
          if (container) {
            const descEl = container.querySelector('[data-e2e="video-desc"]')
                        || container.querySelector('[data-e2e="browse-video-desc"]')
                        || container.querySelector('h1[data-e2e="browse-video-desc"]')
                        || container.querySelector('div[class*="DivTextWrapper"]')
                        || container.querySelector('span[class*="SpanText"]');
            if (descEl && descEl.textContent && !isGenericTitleText(descEl.textContent)) {
              title = descEl.textContent.trim().replace(/\s+/g, " ");
            }
            if (!title || isGenericTitleText(title)) {
              const authorEl = container.querySelector('[data-e2e="video-author-uniqueid"]')
                            || container.querySelector('[data-e2e="video-author-avatar"]');
              if (authorEl && authorEl.textContent) {
                const authorText = authorEl.textContent.trim().replace(/^@/, "");
                if (authorText) title = "TikTok Video by @" + authorText;
              }
            }
          }
        }
      }
    }

    // 2. Specific per-video container check (for Pinterest, Instagram, Twitter/X, TikTok, Reddit)
    if (!title && videoElement && videoElement.closest) {
      try {
        const container = videoElement.closest('[data-test-id="pin"]')
                       || videoElement.closest('[data-test-id="closeup-body"]')
                       || videoElement.closest('[data-testid="tweet"]')
                       || videoElement.closest('article')
                       || videoElement.closest('[role="article"]')
                       || videoElement.closest('[role="dialog"]')
                       || videoElement.closest('.feed-shared-update-v2')
                       || videoElement.parentElement;
        if (container) {
          const cardTitle = container.querySelector('[data-test-id="pin-title"], [data-test-id="closeup-title"], h1, h2, h3, [data-testid="tweetText"]');
          if (cardTitle && cardTitle.textContent && !isGenericTitleText(cardTitle.textContent)) {
            title = cardTitle.textContent.trim();
          }
        }
      } catch (_) {}
    }

    // 3. Document Title / OG Title (fresh from DOM, NEVER sticky cachedTabTitle)
    if (!title) {
      try {
        title = document.querySelector('meta[property="og:title"]')?.content
             || document.querySelector('meta[name="twitter:title"]')?.content
             || (window.top && window.top.document ? window.top.document.title : document.title)
             || document.title
             || "";
      } catch (_) {
        title = document.title || "";
      }
    }

    // Strip common portal brand suffixes
    title = title.replace(/^\(\d+\)\s*/, "")
                 .replace(/ - YouTube$/i, "")
                 .replace(/ \| Facebook$/i, "")
                 .replace(/ \/ X$/i, "")
                 .replace(/ - Dailymotion$/i, "")
                 .replace(/ \| Dailymotion$/i, "")
                 .replace(/ : r\/[a-zA-Z0-9_]+$/i, "")
                 .replace(/ - Twitch$/i, "")
                 .replace(/ \| LinkedIn$/i, "")
                 .replace(/ \| Pinterest$/i, "")
                 .replace(/ \| Tumblr$/i, "")
                 .replace(/ - TikTok$/i, "")
                 .replace(/ \| Instagram$/i, "")
                 .replace(/ • Instagram photos and videos$/i, "")
                 .replace(/ • Threads$/i, "")
                 .replace(/ on Threads$/i, "")
                 .trim();

    // Check if title is valid and non-generic
    if (title && !isGenericTitleText(title)) {
      return title;
    }

    // 4. SECOND PRIORITY: DESCRIPTION (If no title or generic, use first 30 chars of video Description)
    let metaDesc = null;
    try {
      if (videoElement && videoElement.closest) {
        const container = videoElement.closest('[data-test-id="pin"]')
                       || videoElement.closest('[data-test-id="closeup-body"]')
                       || videoElement.closest('[data-testid="tweet"]')
                       || videoElement.closest('article')
                       || videoElement.closest('[role="article"]')
                       || videoElement.closest('[role="dialog"]')
                       || videoElement.parentElement;
        if (container) {
          const descEl = container.querySelector('[data-test-id="pin-description"], [data-testid="tweetText"], [data-e2e="browse-video-desc"], [data-e2e="video-desc"], [data-e2e="search-card-video-caption"], [data-ad-preview="message"], h1, p, span');
          if (descEl && descEl.textContent) {
            metaDesc = descEl.textContent;
          }
        }
      }

      if (!metaDesc || isGenericTitleText(metaDesc)) {
        metaDesc = document.querySelector('meta[property="og:description"]')?.content
                || document.querySelector('meta[name="description"]')?.content
                || document.querySelector('meta[name="twitter:description"]')?.content;
      }
    } catch (_) {}

    if (metaDesc && metaDesc.trim()) {
      const cleanDesc = metaDesc.replace(/[\r\n\t]+/g, " ").replace(/\s+/g, " ").trim();
      if (cleanDesc.length > 0 && !isGenericTitleText(cleanDesc)) {
        return cleanDesc.substring(0, 30).trim();
      }
    }

    // 5. THIRD PRIORITY: TIMELAPSE (If neither title nor description exists, use unique timelapse)
    const now = new Date();
    const pad = n => String(n).padStart(2, '0');
    const timelapse = `${now.getFullYear()}${pad(now.getMonth()+1)}${pad(now.getDate())}_${pad(now.getHours())}${pad(now.getMinutes())}${pad(now.getSeconds())}`;
    return `Video_${timelapse}`;
  }

  function isGenericTitleText(t) {
    if (!t) return true;
    const clean = t.trim();
    if (!clean || !/[a-zA-Z0-9]/.test(clean)) return true;
    const lower = clean.toLowerCase();
    return /^(home|home \/ x|home \/ twitter|feed|timeline|explore|notifications|video|watch|post|status|facebook|instagram|tiktok|reddit|tumblr|pinterest|threads|twitch|twitter|x|video stream|media_stream|download|dailymotion video player|dailymotion|manifest|make your day|tiktok - make your day|trending videos on tiktok)$/i.test(lower)
      || /^\d+$/.test(lower)
      || /^(post|video|tweet) by /i.test(lower);
  }


  // 2. MAIN VIDEO DETECTION: Completely filters out previews, sidebars, miniplayers, and ads
  function isEligibleVideo(v) {
    if (!v || !(v instanceof HTMLVideoElement)) return false;
    
    // Ignore YouTube thumbnail overlays, preview players, sidebar previews, and miniplayers
    if (window.location.hostname.includes("youtube.com")) {
      if (v.closest('ytd-thumbnail-overlay-inline-playback-renderer') ||
          v.closest('#inline-preview-player') ||
          v.closest('ytd-miniplayer') ||
          v.closest('.ytd-compact-video-renderer') ||
          v.closest('ytd-rich-grid-media') ||
          v.closest('[class*="preview"]') ||
          v.closest('[class*="thumbnail"]')) {
        return false;
      }
    }

    // Actively playing videos are always eligible
    if (!v.paused && !v.ended && v.currentTime > 0) {
      return true;
    }

    // Twitter / X.com video player elements are always eligible
    if (window.location.hostname.includes("x.com") || window.location.hostname.includes("twitter.com")) {
      if (v.closest('[data-testid="videoComponent"], [data-testid="videoPlayer"], [data-testid="tweetPhoto"], div[aria-label="Embedded video"]')) {
        return true;
      }
    }

    // Must be reasonably sized (relaxed to 120x80 to support Pinterest pins, vertical reels, and embedded players)
    const rect = v.getBoundingClientRect();
    if ((rect.width > 0 && rect.width < 120) || (rect.height > 0 && rect.height < 80)) {
      return false;
    }
    if (rect.width === 0 && rect.height === 0) {
      if (v.videoWidth > 0 && (v.videoWidth < 120 || v.videoHeight < 80)) {
        return false;
      }
    }

    return true;
  }

  function findPrimaryVideo() {
    // A. Specific Handler for YouTube: always lock to the main player video (not sidebar recommendation previews!)
    if (window.location.hostname.includes("youtube.com")) {
      if (window.location.pathname.includes("/shorts/")) {
        if (hoveredVideo && hoveredVideo.isConnected && isEligibleVideo(hoveredVideo)) {
          return hoveredVideo;
        }
        const activeShort = Array.from(document.querySelectorAll("ytd-reel-video-renderer[is-active] video, video"))
          .find(v => isEligibleVideo(v) && !v.paused);
        if (activeShort) return activeShort;
      }
      const mainYt = document.querySelector("#movie_player video.html5-main-video") ||
                     document.querySelector("#movie_player video") ||
                     document.querySelector(".html5-video-player video");
      if (mainYt && isEligibleVideo(mainYt)) {
        return mainYt;
      }
      return null;
    }

    // A2. Dedicated Instagram Feed & Reels Handler: strictly lock to the reel playing or centered in viewport
    if (window.location.hostname.includes("instagram.com")) {
      const igVideos = Array.from(document.querySelectorAll("video")).filter(v => {
        if (!isEligibleVideo(v)) return false;
        const r = v.getBoundingClientRect();
        return r.width >= 100 && r.height >= 100 && r.bottom > 50 && r.top < window.innerHeight - 50;
      });

      if (igVideos.length > 0) {
        // 1. If user is currently playing a video, that video is 100% the active reel
        const playing = igVideos.find(v => !v.paused && !v.ended && v.currentTime > 0);
        if (playing) return playing;

        // 2. If user specifically hovered over one of the visible Instagram videos:
        if (hoveredVideo && igVideos.includes(hoveredVideo)) {
          const hRect = hoveredVideo.getBoundingClientRect();
          const centerY = window.innerHeight / 2;
          if (Math.abs((hRect.top + hRect.bottom) / 2 - centerY) < window.innerHeight * 0.35) {
            return hoveredVideo;
          }
        }

        // 3. Otherwise pick the video closest to the vertical center of the screen
        const centerY = window.innerHeight / 2;
        igVideos.sort((a, b) => {
          const rA = a.getBoundingClientRect();
          const rB = b.getBoundingClientRect();
          return Math.abs((rA.top + rA.bottom) / 2 - centerY) - Math.abs((rB.top + rB.bottom) / 2 - centerY);
        });
        return igVideos[0];
      }
    }

    // 0. If user hovered over a specific video on other sites, use that video immediately (like IDM)
    if (hoveredVideo && hoveredVideo.isConnected && isEligibleVideo(hoveredVideo)) {
      const hBox = hoveredVideo.closest('[data-testid="videoComponent"], [data-testid="videoPlayer"]') || hoveredVideo;
      const hRect = hBox.getBoundingClientRect();
      const visibleH = Math.min(hRect.bottom, window.innerHeight) - Math.max(hRect.top, 0);
      if (visibleH > 80 && (visibleH / Math.max(1, hRect.height)) > 0.4) {
        return hoveredVideo;
      }
    }

    // B. Dailymotion Player Handler
    if (window.location.hostname.includes("dailymotion.com") || window.location.hostname.includes("dmcdn.net")) {
      const dmVideo = document.querySelector(".np_player video") ||
                      document.querySelector("[data-testid='player'] video") ||
                      document.querySelector(".dmp_player video") ||
                      document.querySelector("video");
      if (dmVideo && dmVideo instanceof HTMLVideoElement && isEligibleVideo(dmVideo)) {
        return dmVideo;
      }
    }

    // B2. TikTok Feed Handler: strictly pick visible playing video or centered video
    if (window.location.hostname.includes("tiktok.com")) {
      const ttVideos = Array.from(document.querySelectorAll("video")).filter(v => {
        const r = v.getBoundingClientRect();
        return r.height > 80 && r.width > 80 && r.bottom > 50 && r.top < window.innerHeight - 50;
      });
      if (ttVideos.length > 0) {
        const playing = ttVideos.find(v => !v.paused && !v.ended && v.currentTime > 0);
        if (playing) return playing;
        const centerY = window.innerHeight / 2;
        ttVideos.sort((a, b) => {
          const rA = a.getBoundingClientRect();
          const rB = b.getBoundingClientRect();
          return Math.abs((rA.top + rA.bottom) / 2 - centerY) - Math.abs((rB.top + rB.bottom) / 2 - centerY);
        });
        return ttVideos[0];
      }
    }

    // C. Generic handler for all other websites (Pinterest, Facebook, TikTok, Instagram, Threads, Vimeo, Twitter/X, etc.)
    const allVideos = Array.from(document.querySelectorAll("video")).filter(isEligibleVideo);
    if (allVideos.length === 0) return null;

    // 1. Filter to videos currently visible in the viewport
    const visibleVideos = allVideos.filter(v => {
      const box = v.closest('[data-testid="videoComponent"], [data-testid="videoPlayer"]') || v;
      const r = box.getBoundingClientRect();
      return r.bottom > 40 && r.top < window.innerHeight - 40 && (r.width >= 100 || v.videoWidth >= 100) && (r.height >= 70 || v.videoHeight >= 70);
    });

    const pool = visibleVideos.length > 0 ? visibleVideos : allVideos;

    // 2. Prioritize active playing video in visible pool
    const playing = pool.find(v => !v.paused && !v.ended && v.currentTime > 0);
    if (playing) return playing;

    // 3. Pick video closest to center of screen (especially for TikTok / Reels feeds / Twitter timeline!)
    const centerY = window.innerHeight / 2;
    pool.sort((a, b) => {
      const bA = a.closest('[data-testid="videoComponent"], [data-testid="videoPlayer"]') || a;
      const bB = b.closest('[data-testid="videoComponent"], [data-testid="videoPlayer"]') || b;
      const rA = bA.getBoundingClientRect();
      const rB = bB.getBoundingClientRect();
      const distA = Math.abs((rA.top + rA.bottom) / 2 - centerY);
      const distB = Math.abs((rB.top + rB.bottom) / 2 - centerY);
      return distA - distB;
    });
    return pool[0];
  }

  function getPlayerContainer(videoElement) {
    if (window.location.hostname.includes("youtube.com")) {
      return videoElement.closest('#movie_player') ||
             videoElement.closest('.html5-video-player') ||
             videoElement.parentElement;
    }

    // TikTok specific: return video's direct frame container
    if (window.location.hostname.includes("tiktok.com")) {
      return videoElement.closest('div[class*="DivVideoWrapper"]') ||
             videoElement.closest('div[id*="xgwrapper"]') ||
             videoElement.parentElement ||
             videoElement;
    }

    // Instagram specific
    if (window.location.hostname.includes("instagram.com")) {
      return videoElement.parentElement ||
             videoElement.closest('div[role="button"]') ||
             videoElement.closest('article') ||
             videoElement;
    }

    // Pinterest specific
    if (window.location.hostname.includes("pinterest.com") || window.location.hostname.includes("pin.it")) {
      return videoElement.parentElement ||
             videoElement.closest('[data-test-id="pin"]') ||
             videoElement.closest('[data-test-id="closeup-body"]') ||
             videoElement;
    }

    // Threads specific
    if (window.location.hostname.includes("threads.net") || window.location.hostname.includes("threads.com")) {
      return videoElement.parentElement ||
             videoElement.closest('article') ||
             videoElement;
    }

    // Tumblr specific
    if (window.location.hostname.includes("tumblr.com")) {
      return videoElement.parentElement ||
             videoElement.closest('article') ||
             videoElement.closest('[data-testid="post"]') ||
             videoElement;
    }

    // Facebook specific
    if (window.location.hostname.includes("facebook.com") || window.location.hostname.includes("fb.watch")) {
      return videoElement.closest('[role="article"]') ||
             videoElement.closest('[data-pagelet*="FeedUnit"]') ||
             videoElement.closest('div[class*="x1yztbdb"]') ||
             videoElement.parentElement ||
             videoElement;
    }

    // X / Twitter specific
    if (window.location.hostname.includes("x.com") || window.location.hostname.includes("twitter.com")) {
      return videoElement.closest('[data-testid="videoComponent"]') ||
             videoElement.closest('[data-testid="videoPlayer"]') ||
             videoElement.closest('[data-testid="tweetPhoto"]') ||
             videoElement.closest('div[aria-label="Embedded video"]') ||
             videoElement.closest('[data-testid="expanded-media-root"]') ||
             videoElement.parentElement ||
             videoElement;
    }

    return videoElement.closest('.np_player') ||
           videoElement.closest('[data-testid="player"]') ||
           videoElement.closest('.dmp_player') ||
           videoElement.closest('[class*="player"]') ||
           videoElement.closest('[class*="video-container"]') ||
           videoElement.parentElement ||
           videoElement;
  }

  function formatBytes(bytes) {
    if (!bytes || bytes <= 0) return "";
    const sizes = ['B', 'KB', 'MB', 'GB', 'TB'];
    const i = Math.floor(Math.log(bytes) / Math.log(1024));
    return (bytes / Math.pow(1024, i)).toFixed(1) + ' ' + sizes[i];
  }

  function calculateSizeBytes(quality, duration) {
    if (!duration || duration <= 0) return 0;
    let bitrate = 1500;
    if (quality === "2160p") bitrate = 15000;
    else if (quality === "1440p") bitrate = 6000;
    else if (quality === "1080p") bitrate = 3000;
    else if (quality === "720p") bitrate = 1500;
    else if (quality === "540p") bitrate = 1000;
    else if (quality === "480p") bitrate = 750;
    else if (quality === "360p") bitrate = 450;
    else if (quality === "240p") bitrate = 250;
    else if (quality === "144p") bitrate = 150;
    else if (quality === "audio" || quality === "mp3") bitrate = 128;
    
    return Math.round((bitrate * 1000 / 8) * duration);
  }

  // 3. DEEP YOUTUBE IN-PAGE PARSER & QUALITY ANALYZER
  function getYouTubeBridgeMetadata() {
    try {
      const raw = document.documentElement.getAttribute("data-prime-yt-metadata");
      if (raw) {
        return JSON.parse(raw);
      }
    } catch (_) {}
    return null;
  }

  function parseInlineYouTubeScripts() {
    try {
      const scripts = document.getElementsByTagName("script");
      for (let i = 0; i < scripts.length; i++) {
        const text = scripts[i].textContent;
        if (text && text.includes("ytInitialPlayerResponse")) {
          const match = text.match(/ytInitialPlayerResponse\s*=\s*({.+?});/);
          if (match && match[1]) {
            return JSON.parse(match[1]);
          }
        }
      }
    } catch (_) {}
    return null;
  }

  function analyzeYouTubeQualities(metadata, targetUrl, duration) {
    if (!metadata) return null;

    const qualityLevels = Array.isArray(metadata.qualityLevels) ? metadata.qualityLevels : [];
    const streams = Array.isArray(metadata.streams) ? metadata.streams : [];
    const bestAudioSize = Number(metadata.bestAudioSize) || 0;

    const availableHeights = new Set();
    const streamSizes = new Map();

    // 1. Process YouTube player API quality levels (minimum 240p)
    for (const lvl of qualityLevels) {
      const l = String(lvl).toLowerCase();
      if (l.includes("2160") || l.includes("4k") || l === "highres" || l === "hd2160") availableHeights.add(2160);
      else if (l.includes("1440") || l.includes("2k") || l === "hd1440") availableHeights.add(1440);
      else if (l.includes("1080") || l === "hd1080") availableHeights.add(1080);
      else if (l.includes("720") || l === "hd720") availableHeights.add(720);
      else if (l.includes("480") || l === "large") availableHeights.add(480);
      else if (l.includes("360") || l === "medium") availableHeights.add(360);
      else if (l.includes("240") || l === "small") availableHeights.add(240);
      // Below 240p (e.g. 144p / tiny) is strictly omitted per requirement
    }

    // 2. Process stream adaptive formats (minimum 240p)
    for (const s of streams) {
      const h = Number(s.height) || 0;
      const w = Number(s.width) || 0;
      let tier = 0;
      if (h >= 2160 || w >= 3840) tier = 2160;
      else if (h >= 1440 || w >= 2560) tier = 1440;
      else if (h >= 1080 || w >= 1920) tier = 1080;
      else if (h >= 720 || w >= 1280) tier = 720;
      else if (h >= 480 || w >= 854) tier = 480;
      else if (h >= 360 || w >= 640) tier = 360;
      else if (h >= 240 || w >= 426) tier = 240;
      // Drop any stream below 240p

      if (tier >= 240) {
        availableHeights.add(tier);
        const sz = (Number(s.contentLength) || 0) + bestAudioSize;
        const cur = streamSizes.get(tier) || 0;
        if (sz > cur) streamSizes.set(tier, sz);
      }
    }

    if (availableHeights.size === 0) return null;

    // Sort descending: 2160 -> 1440 -> 1080 -> 720 ... -> 240
    const sortedHeights = Array.from(availableHeights).sort((a, b) => b - a);
    const resultList = [];

    for (const h of sortedHeights) {
      if (h < 240) continue; // Minimum video quality is 240p
      const qKey = `${h}p`;
      let shortLabel = `${h}p`;
      let typeDesc = `MP4 file, quality ${h}p`;

      if (h >= 2160) {
        shortLabel = "4K Ultra HD (2160p)";
        typeDesc = "MP4 file, quality 2160p 4K Ultra HD";
      } else if (h >= 1440) {
        shortLabel = "2K Quad HD (1440p)";
        typeDesc = "MP4 file, quality 1440p 2K Quad HD";
      } else if (h >= 1080) {
        shortLabel = "Full HD (1080p)";
        typeDesc = "MP4 file, quality 1080p Full HD";
      } else if (h >= 720) {
        shortLabel = "HD (720p)";
        typeDesc = "MP4 file, quality 720p HD";
      } else if (h >= 480) {
        shortLabel = "Standard (480p)";
        typeDesc = "MP4 file, quality 480p Standard";
      } else if (h >= 360) {
        shortLabel = "Medium (360p)";
        typeDesc = "MP4 file, quality 360p Medium";
      } else if (h >= 240) {
        shortLabel = "Low (240p)";
        typeDesc = "MP4 file, quality 240p Low";
      }

      let bytes = streamSizes.get(h) || 0;
      if (bytes <= 0) {
        bytes = calculateSizeBytes(qKey, duration);
      }
      const szStr = formatBytes(bytes);

      resultList.push({
        quality: qKey,
        height: h,
        sizeBytes: bytes,
        typeLabel: szStr ? `${typeDesc} ~ ${szStr}` : typeDesc,
        dialogLabel: szStr ? `${shortLabel} ~ ${szStr}` : shortLabel
      });
    }

    // Audio stream
    let audioBytes = bestAudioSize;
    if (audioBytes <= 0) audioBytes = calculateSizeBytes("audio", duration);
    const audioSzStr = formatBytes(audioBytes);
    resultList.push({
      quality: "audio",
      height: 0,
      sizeBytes: audioBytes,
      typeLabel: audioSzStr ? `Audio file (MP3 / M4A) ~ ${audioSzStr}` : `Audio file (MP3 / M4A)`,
      dialogLabel: audioSzStr ? `Audio Only ~ ${audioSzStr}` : `Audio Only`
    });

    detectedQualitiesMap.set(targetUrl, resultList);
    return resultList;
  }

  // Listen for real-time postMessages from yt_page_bridge.js
  window.addEventListener("message", (event) => {
    if (event.data && event.data.type === "YT_STREAM_QUALITIES" && event.data.payload) {
      if (activeVideo) {
        const url = getCleanVideoUrl(activeVideo);
        const dur = (activeVideo.duration && !isNaN(activeVideo.duration)) ? activeVideo.duration : (event.data.payload.duration || 0);
        const analyzed = analyzeYouTubeQualities(event.data.payload, url, dur);
        if (analyzed && analyzed.length > 0 && grabberContainer) {
          const dd = grabberContainer.querySelector("#m2r-dropdown");
          if (dd && (dd.classList.contains("show") || dd.style.display === "flex")) {
            const title = getVideoTitle(activeVideo);
            if (typeof activeRenderQualityItems === "function") {
              activeRenderQualityItems(analyzed, title, dur, analyzed);
            }
          }
        }
      }
    }
  });

  // 3.5. INSTANT IN-PAGE STREAM DETECTOR (Multi-tier 240p to 4K resolution provider)
  function detectInstantVideoStreams(videoElement, targetUrl, duration) {
    const vw = (videoElement && videoElement.videoWidth) || 0;
    const vh = (videoElement && videoElement.videoHeight) || 0;
    const maxDim = Math.max(vw, vh);
    const minDim = Math.min(vw, vh);

    let maxSupportedHeight = 1080;
    if (minDim >= 2160 || maxDim >= 3840) {
      maxSupportedHeight = 2160;
    } else if (minDim >= 1440 || maxDim >= 2560) {
      maxSupportedHeight = 1440;
    } else if (minDim >= 1000 || maxDim >= 1800) {
      maxSupportedHeight = 1080;
    } else if (minDim >= 650 || maxDim >= 1100) {
      maxSupportedHeight = 720;
    } else if (minDim >= 450 || maxDim >= 750) {
      maxSupportedHeight = 480;
    } else if (minDim >= 300 || maxDim >= 450) {
      maxSupportedHeight = 360;
    } else if (minDim >= 240 || maxDim >= 360) {
      maxSupportedHeight = 240;
    }

    const allResolutionTiers = [
      { quality: "2160p", height: 2160, label: "4K Ultra HD (2160p)", byteRate: 1200000 },
      { quality: "1440p", height: 1440, label: "2K Quad HD (1440p)", byteRate: 650000 },
      { quality: "1080p", height: 1080, label: "Full HD (1080p)", byteRate: 320000 },
      { quality: "720p", height: 720, label: "HD (720p)", byteRate: 180000 },
      { quality: "480p", height: 480, label: "Standard (480p)", byteRate: 100000 },
      { quality: "360p", height: 360, label: "Medium (360p)", byteRate: 65000 },
      { quality: "240p", height: 240, label: "Low (240p)", byteRate: 40000 }
    ];

    const rawSrc = (videoElement && (videoElement.currentSrc || videoElement.src)) || "";
    let directCdnStream = "";
    const isMeta = window.location.hostname.includes("threads.") || window.location.hostname.includes("instagram.");
    if (isMeta && videoElement) {
      directCdnStream = extractDirectMetaCdnStream(videoElement) || "";
    }
    const effectiveStreamUrl = cleanMetaUrl((!rawSrc.startsWith("blob:") && !rawSrc.startsWith("data:")) ? rawSrc : directCdnStream);

    let exactSizeBytes = 0;
    if (cachedNetworkStreams && cachedNetworkStreams.length > 0) {
      const match = cachedNetworkStreams.find(s => 
        s.url === effectiveStreamUrl || s.url === rawSrc || 
        ((s.url.includes("cdninstagram.com") || s.url.includes("fbcdn.net")) && s.ext === "mp4" && !s.isSegment) || 
        (s.ext === "mp4" && !s.isSegment) || s.kind === "manifest"
      );
      if (match && match.size > 0) exactSizeBytes = match.size;
      else if (match && match.contentLength > 0) exactSizeBytes = match.contentLength;
    }

    const effectiveDur = (duration && !isNaN(duration) && duration > 0) ? duration : 30;
    const list = [];

    // Filter tiers: show up to maxSupportedHeight, minimum down to 240p
    const eligibleTiers = allResolutionTiers.filter(t => t.height <= Math.max(maxSupportedHeight, 1080));
    for (const t of eligibleTiers) {
      const estBytes = (t.height === maxSupportedHeight && exactSizeBytes > 0)
        ? exactSizeBytes
        : Math.round(effectiveDur * t.byteRate);
      const szStr = formatBytes(estBytes);
      list.push({
        quality: t.quality,
        height: t.height,
        sizeBytes: estBytes,
        directUrl: (!isMeta && !window.location.hostname.includes("pinterest.") && !window.location.hostname.includes("pin.it") && t.height === maxSupportedHeight && effectiveStreamUrl) ? effectiveStreamUrl : undefined,
        typeLabel: szStr ? `${t.label} ~ ${szStr}` : t.label,
        dialogLabel: szStr ? `${t.quality} ~ ${szStr}` : t.quality
      });
    }

    // Always include audio track
    const audioSz = Math.round(effectiveDur * 16000);
    const audioSzStr = formatBytes(audioSz);
    list.push({
      quality: "audio",
      height: 0,
      sizeBytes: audioSz,
      directUrl: (isMeta && effectiveStreamUrl) ? effectiveStreamUrl : undefined,
      typeLabel: audioSzStr ? `Audio file (MP3 / M4A) ~ ${audioSzStr}` : "Audio file (MP3 / M4A)",
      dialogLabel: audioSzStr ? `Audio Only (MP3) ~ ${audioSzStr}` : "Audio Only (MP3)"
    });

    return list;
  }

  function showGrabberToast() {
    // Completely removed: do not display any notification banner or text when downloading
    try {
      const toast = document.getElementById("m2r-grabber-toast");
      if (toast) toast.remove();
    } catch (_) {}
  }

  // 4. CREATE THE SINGLETON GRABBER UI (Only once)
  function initGrabber() {
    if (grabberContainer) return;

    grabberContainer = document.createElement("div");
    grabberContainer.id = "m2r-singleton-grabber";
    grabberContainer.className = "m2r-floating-container";
    grabberContainer.style.display = "none";

    grabberContainer.innerHTML = `
      <div class="m2r-floating-btn" id="m2r-btn" title="Download this video">
        <div class="m2r-btn-label-area" id="m2r-btn-label" style="display: inline-flex !important; align-items: center !important; gap: 8px !important; cursor: pointer !important;">
          <svg class="m2r-play-icon" viewBox="0 0 24 24">
            <polygon points="5 3 19 12 5 21 5 3"></polygon>
          </svg>
          <span>Download this video</span>
        </div>
        <div class="m2r-arrow-box" id="m2r-btn-arrow" title="Select quality" style="display: inline-flex !important; align-items: center !important; padding: 0 4px !important; margin-left: 2px !important; border-left: 1px solid rgba(255, 255, 255, 0.25) !important; cursor: pointer !important;">
          <svg class="m2r-arrow-icon" viewBox="0 0 20 20">
            <path fill-rule="evenodd" d="M5.293 7.293a1 1 0 011.414 0L10 10.586l3.293-3.293a1 1 0 111.414 1.414l-4 4a1 1 0 01-1.414 0l-4-4a1 1 0 010-1.414z" clip-rule="evenodd"/>
          </svg>
        </div>
      </div>

      <div class="m2r-dropdown-menu" id="m2r-dropdown" style="display: none !important;">
        <div class="m2r-dropdown-header">
          <div class="m2r-dropdown-header-title">
            <svg class="m2r-play-icon" viewBox="0 0 24 24" style="width: 10px; height: 10px;">
              <polygon points="5 3 19 12 5 21 5 3"></polygon>
            </svg>
            <span>Download all / Select Quality:</span>
          </div>
          <span class="m2r-dropdown-close" id="m2r-close">✕</span>
        </div>
        <ul class="m2r-quality-list" id="m2r-quality-items"></ul>
        <div class="m2r-dropdown-footer">
          <span>⚡ Turbo Download Engine</span>
          <span class="m2r-btn-custom-dialog" id="m2r-btn-custom">Advanced Options...</span>
        </div>
      </div>
    `;

    document.body.appendChild(grabberContainer);

    const btn = grabberContainer.querySelector("#m2r-btn");
    const btnLabel = grabberContainer.querySelector("#m2r-btn-label");
    const btnArrow = grabberContainer.querySelector("#m2r-btn-arrow");
    const dropdown = grabberContainer.querySelector("#m2r-dropdown");
    const closeBtn = grabberContainer.querySelector("#m2r-close");
    const qualityList = grabberContainer.querySelector("#m2r-quality-items");
    const customBtn = grabberContainer.querySelector("#m2r-btn-custom");

    function renderQualityLoading(message = "Detecting available stream qualities...") {
      if (!qualityList) return;
      qualityList.innerHTML = `
        <li class="m2r-quality-item loading" style="display: flex !important; align-items: center !important; justify-content: center !important; gap: 8px !important; padding: 12px 14px !important; cursor: default !important; font-size: 11.5px !important; color: #64748b !important; background: #ffffff !important;">
          <svg class="m2r-spin-icon" viewBox="0 0 24 24" style="width: 14px; height: 14px; animation: m2rSpin 0.8s linear infinite !important;">
            <circle cx="12" cy="12" r="10" stroke="#0284c7" stroke-width="3" fill="none" stroke-dasharray="31.4 31.4"></circle>
          </svg>
          <span>${message}</span>
        </li>
      `;
    }

    function renderQualityItems(qualities, currentTitle, duration = 0, fullQualitiesList = [], boundUrl = null) {
      try {
        if (!qualityList) return;
        qualityList.innerHTML = "";
        qualityList.style.cssText = "display: block !important; visibility: visible !important; opacity: 1 !important; list-style: none !important; margin: 0 !important; padding: 2px 0 !important; max-height: 460px !important; overflow-y: auto !important; width: 100% !important; box-sizing: border-box !important;";

        let filtered = (Array.isArray(qualities) ? qualities : []).filter(q => q.height === 0 || q.height >= 240);
        if (filtered.length === 0) {
          filtered = detectInstantVideoStreams(activeVideo, boundUrl, duration) || [];
        }
        if (filtered.length === 0) {
          renderQualityLoading("Detecting available stream qualities (240p - 4K)...");
          return;
        }

        const safeTitle = (typeof currentTitle === "string" && currentTitle.trim())
          ? currentTitle.trim()
          : (getVideoTitle() || "Video");
        const baseTitle = safeTitle.length > 48 ? safeTitle.substring(0, 48) + "..." : safeTitle;

        const targetUrlToUse = boundUrl || (activeVideo ? getCleanVideoUrl(activeVideo) : null) || currentProbedUrl || window.location.href;

        filtered.forEach((q, idx) => {
          const li = document.createElement("li");
          li.className = "m2r-quality-item";
          li.setAttribute("data-quality", q.quality);
          li.style.cssText = "display: flex !important; align-items: center !important; justify-content: space-between !important; padding: 6px 10px !important; gap: 8px !important; cursor: pointer !important; font-size: 11.5px !important; color: #0f172a !important; background: #ffffff !important; border-bottom: 1px solid #f1f5f9 !important; visibility: visible !important; opacity: 1 !important; height: auto !important; min-height: 26px !important; width: 100% !important; box-sizing: border-box !important; line-height: normal !important; user-select: none !important;";

          li.innerHTML = `
            <div class="m2r-item-left" style="display: flex !important; align-items: center !important; gap: 6px !important; min-width: 0 !important; flex: 1 1 auto !important; pointer-events: none !important;">
              <span class="m2r-item-num" style="font-weight: 700 !important; color: #475569 !important; font-size: 10.5px !important; min-width: 14px !important; pointer-events: none !important;">${idx + 1}.</span>
              <span class="m2r-item-title" style="white-space: nowrap !important; overflow: hidden !important; text-overflow: ellipsis !important; color: #1e293b !important; font-weight: 500 !important; font-size: 11.5px !important; pointer-events: none !important;">${baseTitle}</span>
            </div>
            <span class="m2r-item-spec" style="flex-shrink: 0 !important; font-size: 11px !important; font-weight: 600 !important; color: #0284c7 !important; margin-left: 6px !important; pointer-events: none !important;">${q.typeLabel || q.dialogLabel || q.quality}</span>
          `;

          const handleClick = (e) => {
            if (e) {
              try {
                e.stopPropagation();
                e.stopImmediatePropagation();
              } catch (_) {}
            }
            triggerDirectDownload(e, q.quality, targetUrlToUse, safeTitle, duration, filtered);
          };

          li._handleQualityClick = handleClick;
          li.addEventListener("click", handleClick, true);
          li.addEventListener("pointerdown", (e) => { e.stopPropagation(); }, true);
          li.addEventListener("mousedown", (e) => { e.stopPropagation(); }, true);
          qualityList.appendChild(li);
        });

        qualityList.onclick = (e) => {
          const item = e.target.closest(".m2r-quality-item");
          if (item && item._handleQualityClick) {
            item._handleQualityClick(e);
          }
        };
      } catch (err) {
        console.error("[PDM Content] renderQualityItems error:", err);
      }
    }
    activeRenderQualityItems = renderQualityItems;

    const closeDropdown = () => {
      if (!dropdown) return;
      dropdown.classList.remove("show");
      dropdown.removeAttribute("data-state");
      dropdown.style.setProperty("display", "none", "important");
      if (btn) btn.classList.remove("active");
    };

    // Toggle dropdown (STRICTLY MANUAL USER CLICK ONLY - REJECT SYNTHETIC SCRIPT HOVERS/CLICKS)
    const toggleDropdown = (e) => {
      if (!e) return;
      // Strictly manual user clicks: block synthetic events triggered by player scripts
      if (e.isTrusted === false) return;
      try {
        e.stopPropagation();
        e.stopImmediatePropagation();
      } catch (_) {}
      const isOpen = dropdown.classList.contains("show") || dropdown.getAttribute("data-state") === "open" || dropdown.style.display === "flex";
      if (isOpen) {
        closeDropdown();
        return;
      }

      dropdown.classList.add("show");
      dropdown.setAttribute("data-state", "open");
      dropdown.style.setProperty("display", "flex", "important");
      btn.classList.add("active");

      // Strictly re-evaluate active video on click to guarantee exact current reel/video
      activeVideo = findPrimaryVideo();

      try {
        // Adaptive dropdown positioning relative to grabberContainer
        dropdown.style.setProperty("position", "absolute", "important");
        dropdown.style.setProperty("right", "0px", "important");
        dropdown.style.setProperty("left", "auto", "important");
        dropdown.style.setProperty("z-index", "2147483647", "important");

        const btnRect = grabberContainer.getBoundingClientRect();
        if (btnRect.bottom + 340 > window.innerHeight && btnRect.top > 350) {
          dropdown.style.setProperty("top", "auto", "important");
          dropdown.style.setProperty("bottom", "calc(100% + 3px)", "important");
          dropdown.style.setProperty("margin-top", "0px", "important");
          dropdown.style.setProperty("margin-bottom", "0px", "important");
        } else {
          dropdown.style.setProperty("top", "calc(100% + 3px)", "important");
          dropdown.style.setProperty("bottom", "auto", "important");
          dropdown.style.setProperty("margin-top", "0px", "important");
          dropdown.style.setProperty("margin-bottom", "0px", "important");
        }

        const title = (activeVideo ? getVideoTitle(activeVideo) : null) || document.title || "Video";
        const duration = (activeVideo && activeVideo.duration && !isNaN(activeVideo.duration)) ? activeVideo.duration : 0;
        const targetUrl = (activeVideo ? getCleanVideoUrl(activeVideo) : null) || window.location.href;

        // 1. Immediately ensure dropdown is NEVER blank (use cache or instant 240p-4K tiers)
        let availableList = detectedQualitiesMap.get(targetUrl);
        if (!availableList || availableList.length === 0) {
          if (window.location.hostname.includes("youtube.com")) {
            const bridgeData = getYouTubeBridgeMetadata();
            if (bridgeData) {
              availableList = analyzeYouTubeQualities(bridgeData, targetUrl, duration);
            } else {
              const inline = parseInlineYouTubeScripts();
              if (inline && inline.streamingData) {
                const adaptive = inline.streamingData.adaptiveFormats || [];
                availableList = analyzeYouTubeQualities({
                  qualityLevels: [],
                  streams: adaptive.map(f => ({
                    height: f.height,
                    width: f.width,
                    qualityLabel: f.qualityLabel,
                    contentLength: f.contentLength,
                    bitrate: f.bitrate,
                    mimeType: f.mimeType
                  })),
                  bestAudioSize: 0
                }, targetUrl, duration);
              }
            }
          }
          if (!availableList || availableList.length === 0) {
            availableList = detectInstantVideoStreams(activeVideo, targetUrl, duration);
          }
          if (availableList && availableList.length > 0) {
            detectedQualitiesMap.set(targetUrl, availableList);
          }
        }

        if (availableList && availableList.length > 0) {
          renderQualityItems(availableList, title, duration, availableList, targetUrl);
        } else {
          renderQualityLoading("Detecting available stream qualities (240p - 4K)...");
        }

        // 3. Request authoritative probed qualities from backend (uses 5-min cache for instant response)
        const thisProbeId = ++probeRequestId;

        chrome.runtime.sendMessage({
          action: "probe_url",
          url: targetUrl,
          webpage_url: window.location.href,
          fresh: false
        }, (res) => {
          if (thisProbeId !== probeRequestId) return;
          const rawQualities = res?.data?.available_qualities || res?.available_qualities || res?.data?.data?.available_qualities;
          if (Array.isArray(rawQualities) && rawQualities.length > 0) {
            let probed = rawQualities
              .filter(item => item.value !== "best" && (item.height === 0 || item.height >= 240))
              .map(item => ({
                quality: item.value,
                height: item.height || 0,
                sizeBytes: item.size || 0,
                typeLabel: item.label || (item.value === "audio" ? "Audio file (MP3 / M4A)" : `MP4 (${item.value})`),
                dialogLabel: item.label || item.value
              }));

            if (probed.length === 0) {
              probed = detectInstantVideoStreams(activeVideo, targetUrl, duration) || [];
            }

            if (probed.length > 0) {
              probed.sort((a, b) => (b.height || 0) - (a.height || 0));
              detectedQualitiesMap.set(targetUrl, probed);
              if (dropdown.classList.contains("show") || dropdown.style.display === "flex") {
                renderQualityItems(probed, title, duration, probed, targetUrl);
              }
            }
          } else if (res && res.success && (res.data?.total_size || res.total_size)) {
            const sz = res.data?.total_size || res.total_size;
            let fullTiers = detectInstantVideoStreams(activeVideo, targetUrl, duration) || [];
            if (fullTiers.length === 0) {
              const szStr = formatBytes(sz);
              fullTiers = [{
                quality: "1080p", height: 1080, sizeBytes: sz, typeLabel: "Full HD (1080p)", dialogLabel: "1080p"
              }, {
                quality: "720p", height: 720, sizeBytes: Math.round(sz * 0.6), typeLabel: "HD (720p)", dialogLabel: "720p"
              }, {
                quality: "480p", height: 480, sizeBytes: Math.round(sz * 0.35), typeLabel: "Standard (480p)", dialogLabel: "480p"
              }, {
                quality: "audio", height: 0, sizeBytes: Math.round(sz * 0.1), typeLabel: "Audio file (MP3 / M4A)", dialogLabel: "Audio Only (MP3)"
              }];
            }
            detectedQualitiesMap.set(targetUrl, fullTiers);
            if (dropdown.classList.contains("show") || dropdown.style.display === "flex") {
              renderQualityItems(fullTiers, title, duration, fullTiers, targetUrl);
            }
          } else {
            // Only if probe completed with error and nothing was detected:
            const curList = detectedQualitiesMap.get(targetUrl);
            if (!curList || curList.length === 0) {
              const instantFallback = detectInstantVideoStreams(activeVideo, targetUrl, duration) || [
                { quality: "best", height: (activeVideo && activeVideo.videoHeight) || 0, sizeBytes: 0, typeLabel: "Original Stream (Best Available)", dialogLabel: "Best Available" },
                { quality: "audio", height: 0, sizeBytes: 0, typeLabel: "Audio file (MP3 / M4A)", dialogLabel: "Audio Only" }
              ];
              detectedQualitiesMap.set(targetUrl, instantFallback);
              if (dropdown.classList.contains("show") || dropdown.style.display === "flex") {
                renderQualityItems(instantFallback, title, duration, instantFallback, targetUrl);
              }
            }
          }
        });
      } catch (err) {
        console.error("[PDM Content] toggleDropdown error:", err);
      }
    };

    const triggerDirectDownload = (e, selectedQuality = null, explicitUrl = null, explicitTitle = null, explicitDuration = 0, explicitQualities = null) => {
      if (e) {
        try {
          e.stopPropagation();
          e.stopImmediatePropagation();
        } catch (_) {}
      }
      closeDropdown();

      let targetUrl = explicitUrl || (activeVideo ? getCleanVideoUrl(activeVideo) : null) || currentProbedUrl || window.location.href;
      if (!targetUrl || targetUrl.startsWith("blob:") || targetUrl.startsWith("data:")) {
        targetUrl = window.location.href;
      }

      let safeTitle = (explicitTitle || (activeVideo ? getVideoTitle(activeVideo) : null) || document.title || "Video").trim();
      safeTitle = safeTitle
        .replace(/\.unknown_video(?:\.mp4)?$/i, "")
        .replace(/\.image(?:\.mp4)?$/i, "")
        .trim();
      let duration = explicitDuration || (activeVideo && activeVideo.duration && !isNaN(activeVideo.duration) ? activeVideo.duration : 0);
      let directServerSrc = (activeVideo && activeVideo.currentSrc && !activeVideo.currentSrc.startsWith("blob:") && !activeVideo.currentSrc.startsWith("data:"))
        ? activeVideo.currentSrc
        : "";
      if (!directServerSrc && targetUrl && targetUrl.startsWith("http") && !targetUrl.startsWith("blob:") && targetUrl !== window.location.href) {
        directServerSrc = targetUrl;
      }
      if (!directServerSrc && window.location.hostname.includes("threads.")) {
        directServerSrc = extractDirectMetaCdnStream(activeVideo) || "";
      }
      if (directServerSrc && directServerSrc.startsWith("http") && (!targetUrl || targetUrl.startsWith("blob:") || targetUrl === window.location.href || targetUrl.includes("threads.net") || targetUrl.includes("threads.com"))) {
        targetUrl = directServerSrc;
      }

      if (directServerSrc) {
        directServerSrc = cleanMetaUrl(directServerSrc);
      }
      if (targetUrl) {
        targetUrl = cleanMetaUrl(targetUrl);
      }
      const rawList = (explicitQualities && explicitQualities.length > 0)
        ? explicitQualities
        : (detectedQualitiesMap.get(targetUrl) || detectedQualitiesMap.get(window.location.href) || []);

      const chosenQ = selectedQuality || (rawList[0]?.quality || "best");
      const chosenItem = rawList.find(q => q.quality === chosenQ);
      if (chosenItem && chosenItem.directUrl && chosenItem.directUrl.startsWith("http") && !chosenItem.directUrl.startsWith("blob:")) {
        directServerSrc = chosenItem.directUrl;
        targetUrl = chosenItem.directUrl;
      }

      const filteredList = rawList.filter(item => item.quality === "audio" || (item.height === 0 || item.height >= 240));

      let qualitiesPayload = filteredList.map(item => ({
        value: item.quality,
        label: item.dialogLabel || item.typeLabel || (item.height > 0 ? `${item.height}p` : item.quality),
        height: item.height || (item.quality === 'audio' ? 0 : 720),
        is_default: item.quality === chosenQ,
        size: item.sizeBytes || 0
      }));

      if (qualitiesPayload.length === 0) {
        qualitiesPayload = [{
          value: chosenQ,
          label: chosenQ === "audio" ? "Audio file (MP3 / M4A)" : "Original Stream (Best Available)",
          height: (activeVideo && activeVideo.videoHeight) || 0,
          is_default: true,
          size: 0
        }];
      } else if (!qualitiesPayload.some(q => q.is_default)) {
        qualitiesPayload[0].is_default = true;
      }

      const isThreads = window.location.hostname.includes("threads.");
      let audioStreamUrl = "";
      if (isThreads) {
        const streams = extractMetaStreams(activeVideo);
        if (streams.videoStream) {
          targetUrl = streams.videoStream;
          directServerSrc = streams.videoStream;
        }
        if (streams.audioStream) {
          audioStreamUrl = streams.audioStream;
        }
      }

      chrome.runtime.sendMessage({
        action: "send_download",
        url: targetUrl,
        audio_url: audioStreamUrl || undefined,
        webpage_url: window.location.href,
        direct_server_url: directServerSrc || undefined,
        quality: chosenQ,
        title: safeTitle,
        duration: duration,
        qualities: qualitiesPayload,
        threads: 8
      }, (resp) => {
        if (chrome.runtime.lastError) {
          console.warn("[PDM Content] send_download error/warning:", chrome.runtime.lastError.message);
        }
      });
    };

    let lastActionTimestamp = 0;

    const handleButtonClick = (e) => {
      if (!e) return;
      const now = Date.now();
      if (now - lastActionTimestamp < 300) return;
      lastActionTimestamp = now;
      try {
        e.stopPropagation();
        e.stopImmediatePropagation();
      } catch (_) {}

      // In IDM style: Clicking the download button opens the available qualities dropdown menu
      toggleDropdown(e);
    };

    if (btnLabel) {
      btnLabel.addEventListener("click", handleButtonClick, true);
      btnLabel.addEventListener("pointerdown", (e) => { e.stopPropagation(); }, true);
      btnLabel.addEventListener("mousedown", (e) => { e.stopPropagation(); }, true);
    }

    if (btnArrow) {
      btnArrow.addEventListener("click", handleButtonClick, true);
      btnArrow.addEventListener("pointerdown", (e) => { e.stopPropagation(); }, true);
      btnArrow.addEventListener("mousedown", (e) => { e.stopPropagation(); }, true);
    }

    btn.addEventListener("click", handleButtonClick, true);
    btn.addEventListener("pointerdown", (e) => { e.stopPropagation(); }, true);
    btn.addEventListener("mousedown", (e) => { e.stopPropagation(); }, true);
    btn.addEventListener("mouseup", (e) => { e.stopPropagation(); }, true);

    dropdown.addEventListener("pointerdown", (e) => { e.stopPropagation(); }, true);
    dropdown.addEventListener("mousedown", (e) => { e.stopPropagation(); }, true);

    closeBtn.addEventListener("click", (e) => {
      e.stopPropagation();
      e.stopImmediatePropagation();
      closeDropdown();
    }, true);
    closeBtn.addEventListener("mousedown", (e) => { e.stopPropagation(); e.stopImmediatePropagation(); }, true);

    customBtn.addEventListener("click", (e) => {
      e.stopPropagation();
      e.stopImmediatePropagation();
      closeDropdown();
      triggerDirectDownload(e);
    }, true);
    customBtn.addEventListener("mousedown", (e) => { e.stopPropagation(); e.stopImmediatePropagation(); }, true);

    // Close on click or pointerdown outside in CAPTURE phase (prevents video player stopPropagation from blocking close!)
    const handleOutsideInteraction = (e) => {
      if (grabberContainer && !grabberContainer.contains(e.target)) {
        closeDropdown();
      }
    };
    document.addEventListener("click", handleOutsideInteraction, true);
    document.addEventListener("pointerdown", handleOutsideInteraction, true);

    // Close on Escape key
    document.addEventListener("keydown", (e) => {
      if (e.key === "Escape") {
        closeDropdown();
      }
    }, true);
  }

  // 5. BACKGROUND PRE-PROBING: Prepares accurate stream list before user even clicks
  function preProbeCurrentVideo() {
    if (!activeVideo) return;
    const targetUrl = getCleanVideoUrl(activeVideo);
    if (!targetUrl || !targetUrl.startsWith("http")) return;

    if (window.location.hostname.includes("youtube.com")) {
      const bridgeData = getYouTubeBridgeMetadata();
      if (bridgeData) {
        const dur = (activeVideo.duration && !isNaN(activeVideo.duration)) ? activeVideo.duration : (bridgeData.duration || 0);
        analyzeYouTubeQualities(bridgeData, targetUrl, dur);
      } else {
        window.postMessage({ type: "PRIME_REQUEST_YT_QUALITIES" }, "*");
      }
    } else if (!detectedQualitiesMap.has(targetUrl)) {
      const dur = (activeVideo.duration && !isNaN(activeVideo.duration)) ? activeVideo.duration : 0;
      const instant = detectInstantVideoStreams(activeVideo, targetUrl, dur);
      if (instant && instant.length > 0) {
        detectedQualitiesMap.set(targetUrl, instant);
      }
    }

    if (currentProbedUrl === targetUrl || detectedQualitiesMap.has(targetUrl) || window.location.hostname.includes("youtube.com")) {
      return;
    }

    currentProbedUrl = targetUrl;
    const thisProbeId = ++probeRequestId;

    chrome.runtime.sendMessage({
      action: "probe_url",
      url: targetUrl,
      webpage_url: window.location.href,
      fresh: false
    }, (res) => {
      if (thisProbeId !== probeRequestId) return;
      const rawQualities = res?.data?.available_qualities || res?.available_qualities || res?.data?.data?.available_qualities;
      if (Array.isArray(rawQualities) && rawQualities.length > 0) {
        const probed = rawQualities
          .filter(item => item.value !== "best" && (item.height === 0 || item.height >= 240))
          .map(item => ({
            quality: item.value,
            height: item.height || 0,
            sizeBytes: item.size || 0,
            typeLabel: item.label || (item.value === "audio" ? "Audio file (MP3 / M4A)" : `MP4 (${item.value})`),
            dialogLabel: item.label || item.value
          }));
        probed.sort((a, b) => (b.height || 0) - (a.height || 0));
        detectedQualitiesMap.set(targetUrl, probed);

        // Live update if user opened the dropdown during background pre-probing
        if (grabberContainer) {
          const dd = grabberContainer.querySelector("#m2r-dropdown");
          if (dd && (dd.classList.contains("show") || dd.style.display === "flex")) {
            const title = getVideoTitle(activeVideo);
            const duration = (activeVideo && activeVideo.duration && !isNaN(activeVideo.duration)) ? activeVideo.duration : 0;
            if (typeof activeRenderQualityItems === "function") {
              activeRenderQualityItems(probed, title, duration, probed);
            }
          }
        }
      } else if (res && res.success && (res.data?.total_size || res.total_size)) {
        const sz = res.data?.total_size || res.total_size;
        const szStr = formatBytes(sz);
        const single = [{
          quality: "default",
          height: 0,
          sizeBytes: sz,
          typeLabel: szStr ? `Original Stream ~ ${szStr}` : "Original Stream",
          dialogLabel: szStr ? `Original Stream ~ ${szStr}` : "Original Stream"
        }];
        detectedQualitiesMap.set(targetUrl, single);

        if (grabberContainer) {
          const dd = grabberContainer.querySelector("#m2r-dropdown");
          if (dd && (dd.classList.contains("show") || dd.style.display === "flex")) {
            const title = getVideoTitle(activeVideo);
            const duration = (activeVideo && activeVideo.duration && !isNaN(activeVideo.duration)) ? activeVideo.duration : 0;
            if (typeof activeRenderQualityItems === "function") {
              activeRenderQualityItems(single, title, duration, single);
            }
          }
        }
      }
    });
  }

  let lastKnownHref = window.location.href;
  function handleUrlOrNavigationChange() {
    const currentHref = window.location.href;
    if (currentHref !== lastKnownHref) {
      lastKnownHref = currentHref;
      currentProbedUrl = null;
      activeVideo = null;
      hoveredVideo = null;
      lastAttachedVideo = null;
      cachedNetworkStreams = [];
      detectedQualitiesMap.clear();

      // Reset and close dropdown on page/video navigation so previous video title is never carried over
      if (grabberContainer) {
        const dropdown = grabberContainer.querySelector("#m2r-dropdown");
        const btn = grabberContainer.querySelector("#m2r-btn");
        if (dropdown) {
          dropdown.classList.remove("show");
          dropdown.removeAttribute("data-state");
          dropdown.style.setProperty("display", "none", "important");
        }
        if (btn) btn.classList.remove("active");
        const list = grabberContainer.querySelector("#m2r-quality-items");
        if (list) list.innerHTML = "";
      }

      // Notify background to clear tab media
      try {
        chrome.runtime.sendMessage({ action: "url_changed", url: currentHref }, () => {
          if (chrome.runtime.lastError) {}
        });
      } catch (_) {}
    }
  }

  // Modern Navigation API listener (instant SPA route detection in Chrome 102+)
  if (window.navigation) {
    try {
      window.navigation.addEventListener("navigate", () => {
        setTimeout(handleUrlOrNavigationChange, 0);
        setTimeout(updatePosition, 100);
      });
    } catch (_) {}
  }
  window.addEventListener("popstate", () => {
    handleUrlOrNavigationChange();
    setTimeout(updatePosition, 100);
  });
  window.addEventListener("hashchange", () => {
    handleUrlOrNavigationChange();
    setTimeout(updatePosition, 100);
  });
  window.addEventListener("yt-navigate-finish", () => {
    handleUrlOrNavigationChange();
    setTimeout(updatePosition, 200);
  });
  window.addEventListener("yt-page-data-updated", () => {
    handleUrlOrNavigationChange();
    setTimeout(updatePosition, 200);
  });

  // High-frequency lightweight URL watcher (guarantees detection within 100ms on all SPAs without reload)
  setInterval(() => {
    if (window.location.href !== lastKnownHref) {
      handleUrlOrNavigationChange();
      updatePosition();
    }
  }, 100);
  let lastAttachedVideo = null;

  // 6. POSITION SYNCHRONIZER: Pinned to top-right corner of the video player
  function updatePosition() {
    handleUrlOrNavigationChange();
    initGrabber();
    activeVideo = findPrimaryVideo();

    if (!activeVideo) {
      lastAttachedVideo = null;
      if (grabberContainer) {
        grabberContainer.style.setProperty("display", "none", "important");
        const dd = grabberContainer.querySelector("#m2r-dropdown");
        const b = grabberContainer.querySelector("#m2r-btn");
        if (dd) {
          dd.classList.remove("show");
          dd.removeAttribute("data-state");
          dd.style.setProperty("display", "none", "important");
        }
        if (b) b.classList.remove("active");
      }
      return;
    }

    if (activeVideo !== lastAttachedVideo) {
      lastAttachedVideo = activeVideo;
      if (grabberContainer) {
        const dd = grabberContainer.querySelector("#m2r-dropdown");
        const b = grabberContainer.querySelector("#m2r-btn");
        if (dd) {
          dd.classList.remove("show");
          dd.removeAttribute("data-state");
          dd.style.setProperty("display", "none", "important");
        }
        if (b) b.classList.remove("active");
      }
    }

    // Trigger pre-probing
    preProbeCurrentVideo();

    const player = getPlayerContainer(activeVideo);
    if (!player) return;

    const pRect = player.getBoundingClientRect();
    const vRect = activeVideo.getBoundingClientRect();

    // Select the best reference bounds for docking
    let rect = vRect;
    const isTikTok = window.location.hostname.includes("tiktok.com");
    const isInstagram = window.location.hostname.includes("instagram.com");
    if (!isTikTok && !isInstagram && pRect && pRect.width >= 100 && pRect.height >= 70 && pRect.width <= window.innerWidth * 1.05 && pRect.height <= window.innerHeight * 1.05) {
      rect = pRect;
    } else {
      rect = (vRect && vRect.width >= 100 && vRect.height >= 70) ? vRect : (pRect || vRect);
    }

    // Check visibility within viewport
    if (rect.width < 100 || rect.height < 60 || rect.bottom < 40 || rect.top > window.innerHeight - 30 || rect.right < 40 || rect.left > window.innerWidth - 30) {
      if (grabberContainer) {
        grabberContainer.style.setProperty("display", "none", "important");
      }
      return;
    }

    // Bulletproof attachment: dock firmly to document.documentElement (or fullscreenElement)
    // so it sits above all website overlays (Threads, Pinterest, Instagram, TikTok, Facebook, etc.)
    const isFullscreen = !!(document.fullscreenElement || document.webkitFullscreenElement);
    const fullscreenEl = document.fullscreenElement || document.webkitFullscreenElement;

    const targetParent = (isFullscreen && fullscreenEl) ? fullscreenEl : document.documentElement;

    if (grabberContainer.parentElement !== targetParent) {
      targetParent.appendChild(grabberContainer);
    }

    grabberContainer.style.setProperty("display", "block", "important");
    grabberContainer.style.setProperty("position", isFullscreen ? "absolute" : "fixed", "important");

    // Position fixed cleanly at top-right corner of video player
    let topPos = isFullscreen ? 12 : Math.round(rect.top + 8);
    let rightPos = isFullscreen ? 12 : Math.round(Math.max(10, window.innerWidth - rect.right + 8));

    if (!isFullscreen) {
      topPos = Math.max(10, Math.min(topPos, window.innerHeight - 45));
      rightPos = Math.max(10, Math.min(rightPos, window.innerWidth - 180));
    }

    grabberContainer.style.setProperty("top", `${topPos}px`, "important");
    grabberContainer.style.setProperty("right", `${rightPos}px`, "important");
    grabberContainer.style.setProperty("left", "auto", "important");
    grabberContainer.style.setProperty("bottom", "auto", "important");
    grabberContainer.style.setProperty("z-index", "2147483647", "important");
    grabberContainer.style.setProperty("pointer-events", "auto", "important");
  }

  window.addEventListener("scroll", updatePosition, { passive: true, capture: true });
  document.addEventListener("scroll", updatePosition, { passive: true, capture: true });
  window.addEventListener("wheel", updatePosition, { passive: true, capture: true });
  window.addEventListener("touchmove", updatePosition, { passive: true, capture: true });
  window.addEventListener("resize", updatePosition, { passive: true });
  document.addEventListener("fullscreenchange", updatePosition);
  document.addEventListener("webkitfullscreenchange", updatePosition);
  window.addEventListener("keydown", (e) => {
    if (e.key === "ArrowDown" || e.key === "ArrowUp" || e.key === "PageDown" || e.key === "PageUp") {
      setTimeout(updatePosition, 60);
      setTimeout(updatePosition, 200);
    }
  }, { passive: true });
  setInterval(updatePosition, 250);

  // Pre-detect stream qualities as soon as video loads metadata or starts playing
  document.addEventListener("loadedmetadata", (e) => {
    if (e.target && e.target.tagName === "VIDEO") {
      const v = e.target;
      const url = getCleanVideoUrl(v);
      if (url && !detectedQualitiesMap.has(url)) {
        const dur = (v.duration && !isNaN(v.duration)) ? v.duration : 0;
        const instant = detectInstantVideoStreams(v, url, dur);
        if (instant && instant.length > 0) {
          detectedQualitiesMap.set(url, instant);
        }
      }
    }
  }, { passive: true, capture: true });

  document.addEventListener("playing", (e) => {
    if (e.target && e.target.tagName === "VIDEO") {
      const v = e.target;
      const url = getCleanVideoUrl(v);
      if (url && !detectedQualitiesMap.has(url)) {
        const dur = (v.duration && !isNaN(v.duration)) ? v.duration : 0;
        const instant = detectInstantVideoStreams(v, url, dur);
        if (instant && instant.length > 0) {
          detectedQualitiesMap.set(url, instant);
        }
      }
    }
  }, { passive: true, capture: true });

  // =========================================================================
  // PAGE DOWNLOAD FILE DETECTION
  function detectPageDownloadFile() {
    // 1. Script-based redirects (e.g. OBS Studio welcome page, SourceForge, etc.)
    try {
      const scripts = document.querySelectorAll("script");
      for (const s of scripts) {
        const txt = s.textContent || "";
        if (txt.includes("window.location") || txt.includes("location.href") || txt.includes("location.replace") || txt.includes("location.assign")) {
          const m = txt.match(/https?:\/\/[^"'\s\\]+\.(?:exe|msi|msix|zip|rar|7z|tar\.gz|tgz|iso|dmg|pkg|deb|rpm|pdf|apk|bin|appimage)/i);
          if (m && m[0]) {
            const clean = m[0].replace(/\\\/|\/\\/g, '/');
            const fname = clean.split("/").pop().split("?")[0];
            return { downloadUrl: clean, filename: decodeURIComponent(fname) };
          }
        }
      }
    } catch (_) {}

    // 2. Meta refresh redirect
    try {
      const meta = document.querySelector('meta[http-equiv="refresh"]');
      if (meta && meta.content) {
        const m = meta.content.match(/url=(https?:\/\/[^"'\s;]+)/i);
        if (m && m[1]) {
          const fname = m[1].split("/").pop().split("?")[0];
          return { downloadUrl: m[1], filename: decodeURIComponent(fname) };
        }
      }
    } catch (_) {}

    // 3. Anchor download links on the page (matching downloadable files)
    try {
      const downloadExts = /\.(exe|msi|msix|zip|rar|7z|tar\.gz|tgz|iso|dmg|pkg|deb|rpm|pdf|apk|bin|appimage)(\?.*)?$/i;
      const links = Array.from(document.querySelectorAll('a[href]'));
      for (const a of links) {
        const h = a.href || "";
        if (downloadExts.test(h)) {
          const cleanName = a.getAttribute("download") || h.split("/").pop().split("?")[0] || "download";
          return { downloadUrl: h, filename: decodeURIComponent(cleanName) };
        }
      }
    } catch (_) {}

    return null;
  }

  // Listen for message from background service worker or popup
  try {
    chrome.runtime.onMessage.addListener((request, sender, sendResponse) => {
      
      if (request.action === "get_page_download_file") {
        const detected = detectPageDownloadFile();
        sendResponse(detected || { downloadUrl: null });
        return true;
      }
    });
  } catch (_) {}

  // Initialize
  updatePosition();
})();

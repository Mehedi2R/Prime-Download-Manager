// Prime Download Manager Extension - YouTube Page Bridge (Runs in MAIN World)
// Extracts authoritative player response, stream formats, and available quality levels
(function() {
  function getYouTubeMetadata() {
    try {
      const player = document.getElementById("movie_player") || document.querySelector(".html5-video-player");
      if (!player) return null;

      const videoData = (typeof player.getVideoData === "function") ? player.getVideoData() : null;
      const videoId = videoData?.video_id || "";

      let playerResponse = null;
      if (typeof player.getPlayerResponse === "function") {
        playerResponse = player.getPlayerResponse();
      } else if (window.ytInitialPlayerResponse) {
        playerResponse = window.ytInitialPlayerResponse;
      }

      const title = videoData?.title || playerResponse?.videoDetails?.title || document.title || "YouTube Video";
      const duration = (typeof player.getDuration === "function") ? player.getDuration() : (Number(playerResponse?.videoDetails?.lengthSeconds) || 0);

      // 1. Available Quality Levels from YouTube Player API (e.g. ['highres', 'hd2160', 'hd1440', 'hd1080', 'hd720', 'large', 'medium', 'small', 'tiny'])
      let qualityLevels = [];
      if (typeof player.getAvailableQualityLevels === "function") {
        qualityLevels = player.getAvailableQualityLevels() || [];
      }
      if (typeof player.getAvailableQualityData === "function") {
        const qData = player.getAvailableQualityData() || [];
        for (const d of qData) {
          if (d.quality && !qualityLevels.includes(d.quality)) qualityLevels.push(d.quality);
          if (d.qualityLabel && !qualityLevels.includes(d.qualityLabel)) qualityLevels.push(d.qualityLabel);
        }
      }

      // Check DOM quality menus if open or rendered
      try {
        const menuItems = document.querySelectorAll(".ytp-quality-menu .ytp-menuitem, .ytp-settings-menu .ytp-menuitem, .ytp-panel-menu .ytp-menuitem");
        for (const item of menuItems) {
          const txt = item.innerText || item.textContent || "";
          const match = txt.match(/(\d{3,4})p/);
          if (match && !qualityLevels.includes(`${match[1]}p`)) {
            qualityLevels.push(`${match[1]}p`);
          }
        }
      } catch (_) {}

      // 2. Stream Adaptive Formats from StreamingData
      const streamingData = playerResponse?.streamingData;
      const adaptiveFormats = streamingData?.adaptiveFormats || [];
      const progressiveFormats = streamingData?.formats || [];

      // Extract unique stream resolutions and sizes
      const streams = [];
      const allFormats = [...adaptiveFormats, ...progressiveFormats];

      // Find best audio size/bitrate
      let bestAudioSize = 0;
      let bestAudioBitrate = 128000;
      for (const fmt of allFormats) {
        const mime = fmt.mimeType || "";
        if (mime.includes("audio/")) {
          const sz = Number(fmt.contentLength) || 0;
          const br = Number(fmt.bitrate) || 0;
          if (sz > bestAudioSize) bestAudioSize = sz;
          if (br > bestAudioBitrate) bestAudioBitrate = br;
        }
      }

      for (const fmt of allFormats) {
        const mime = fmt.mimeType || "";
        if (mime.includes("video/")) {
          const h = Number(fmt.height) || 0;
          const w = Number(fmt.width) || 0;
          const sz = Number(fmt.contentLength) || 0;
          const br = Number(fmt.bitrate) || 0;
          const qLabel = fmt.qualityLabel || (h > 0 ? `${h}p` : "");
          if (h > 0 || w > 0) {
            streams.push({
              height: h,
              width: w,
              qualityLabel: qLabel,
              contentLength: sz,
              bitrate: br,
              mimeType: mime
            });
          }
        }
      }

      return {
        videoId,
        title,
        duration,
        qualityLevels,
        streams,
        bestAudioSize,
        bestAudioBitrate
      };
    } catch (_) {
      return null;
    }
  }

  function syncMetadata() {
    const meta = getYouTubeMetadata();
    if (!meta) return;

    try {
      const serialized = JSON.stringify(meta);
      document.documentElement.setAttribute("data-prime-yt-metadata", serialized);

      window.postMessage({
        source: "PRIME_DOWNLOAD_MANAGER",
        type: "YT_STREAM_QUALITIES",
        payload: meta
      }, "*");
    } catch (_) {}
  }

  // Hook navigation and player lifecycle events
  window.addEventListener("yt-navigate-finish", () => {
    syncMetadata();
    setTimeout(syncMetadata, 400);
    setTimeout(syncMetadata, 1200);
  });

  window.addEventListener("yt-page-data-updated", () => {
    syncMetadata();
    setTimeout(syncMetadata, 400);
  });

  window.addEventListener("message", (event) => {
    if (event.data && event.data.type === "PRIME_REQUEST_YT_QUALITIES") {
      syncMetadata();
    }
  });

  // Polling hook for player initialization
  let attempts = 0;
  const pollTimer = setInterval(() => {
    syncMetadata();
    attempts++;
    if (attempts > 15) {
      clearInterval(pollTimer);
    }
  }, 500);

  // Sync on state change or playback
  document.addEventListener("playing", syncMetadata, true);
  document.addEventListener("loadedmetadata", syncMetadata, true);

  syncMetadata();
})();

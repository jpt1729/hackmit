const CACHE_NAME = "routine-anchor-v65";
const APP_SHELL = [
  "./",
  "./index.html",
  "./css/style.css",
  "./vendor/leaflet/leaflet.css",
  "./vendor/leaflet/leaflet.js",
  "./js/app.js",
  "./js/alerts.js",
  "./js/api.js",
  "./js/checklist.js",
  "./js/routine-editor.js",
  "./js/clock.js",
  "./js/progress.js",
  "./js/location-map.js",
  "./js/messages.js",
  "./js/timeline.js",
  "./data/demo.json",
  "./manifest.json",
  "./icons/app-icon.svg",
  "./icons/brainbuddy.png"
];

self.addEventListener("install", (event) => {
  event.waitUntil(caches.open(CACHE_NAME).then((cache) => cache.addAll(APP_SHELL)));
  self.skipWaiting();
});

self.addEventListener("activate", (event) => {
  event.waitUntil(
    caches.keys().then((keys) => Promise.all(
      keys.filter((key) => key !== CACHE_NAME).map((key) => caches.delete(key))
    ))
  );
  self.clients.claim();
});

self.addEventListener("fetch", (event) => {
  const url = new URL(event.request.url);
  if (event.request.method !== "GET" || url.origin !== self.location.origin || url.pathname.startsWith("/api/")) {
    return;
  }

  event.respondWith(
    fetch(event.request, { cache: "no-cache" }).catch(async (error) => {
      const cached = await caches.match(event.request);
      if (cached) return cached;
      throw error;
    })
  );
});

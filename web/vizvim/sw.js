const CACHE = 'vizvim-v4';
const ASSETS = ['./', 'index.html', 'vizvim.js', 'vizvim.wasm', 'manifest.json'];

self.addEventListener('install', e => {
  e.waitUntil(caches.open(CACHE).then(c => c.addAll(ASSETS.map(asset => new Request(asset, { cache: 'reload' })))));
  self.skipWaiting();
});

self.addEventListener('activate', e => {
  e.waitUntil(
    caches.keys().then(keys =>
      Promise.all(keys.filter(k => k !== CACHE).map(k => caches.delete(k)))
    )
  );
  self.clients.claim();
});

self.addEventListener('fetch', event => {
  const request = event.request;
  if (request.cache === 'only-if-cached' && request.mode !== 'same-origin') return;

  event.respondWith((async () => {
    const url = new URL(request.url);
    const sameOrigin = url.origin === self.location.origin;
    const cache = await caches.open(CACHE);
    const wantsHtml = request.mode === 'navigate' || request.headers.get('accept')?.includes('text/html');

    if (!sameOrigin) {
      return fetch(request);
    }

    if (wantsHtml) {
      try {
        const fresh = await fetch(request, { cache: 'no-cache' });
        if (fresh && fresh.ok) {
          cache.put(request, fresh.clone()).catch(() => {});
        }
        return fresh;
      } catch (error) {
        const cachedHtml = await cache.match(request) || await cache.match('index.html') || await cache.match('./');
        if (cachedHtml) return cachedHtml;
        throw error;
      }
    }

    const cached = await cache.match(request);
    const response = cached || await fetch(request);

    if (request.method === 'GET' && response && response.ok) {
      cache.put(request, response.clone()).catch(() => {});
    }

    return response;
  })());
});

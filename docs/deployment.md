# Running and publishing Brain Buddy

Run all commands below from the repository root.

## Local website with voice messages

```sh
python3 tools/message_server.py
```

Open <http://127.0.0.1:8787/>. The server serves `app/` at `/` and handles `/api/voice/` requests. The SQLite database remains in `var/voice_messages.sqlite3`. Moving the website folder does not change browser URLs, saved schedules, or recordings.

To use another port:

```sh
VOICE_PORT=8788 python3 tools/message_server.py
```

For static files only:

```sh
python3 -m http.server 8000 --directory app
```

The static server does not provide voice messaging. JavaScript modules should be served over HTTP, not opened as local files.

## GitHub Pages

The workflow at [`.github/workflows/pages.yml`](../.github/workflows/pages.yml) uploads only `app/`, preserving the existing site URL and relative asset paths. No frontend build is required.

When merging this folder migration:

1. In the repository, open **Settings → Pages → Build and deployment**.
2. Set **Source** to **GitHub Actions**. The old branch-based `/docs` publishing source no longer contains the website.
3. Merge the migration into `main`; the workflow runs automatically. It can also be started from **Actions → Deploy website → Run workflow** on `main`.

The `github-pages` deployment environment must permit `main`. These are repository settings; adding the workflow locally does not change them. See [GitHub’s custom Pages workflow documentation](https://docs.github.com/en/pages/getting-started-with-github-pages/using-custom-workflows-with-github-pages).

Pages serves the static website and replay data. On `*.github.io`, messages use a browser-only demo: text and audio are saved in IndexedDB on the current device, and recordings remain playable after reload. No message API is needed. These messages are not delivered to another person or synchronized between devices. Clearing site data removes them.

For the same demo on another static host or localhost, add `?messages=local` (or `&messages=local` alongside other parameters). The local Python server continues to provide server-backed messaging when this option is absent.

Pages does not run `tools/message_server.py` or provide access to a bracelet on a private home network.

## Other hosting

Use `app/` as the static document root, preserving its relative paths. For voice messaging, run the Python server and route both the static site and `/api/voice/` to it on the same origin.

Microphone access requires localhost or HTTPS. To serve other devices directly, configure `VOICE_HOST`, `VOICE_TLS_CERT`, and `VOICE_TLS_KEY` as described in the [website guide](app.md). Street map tiles require internet access.

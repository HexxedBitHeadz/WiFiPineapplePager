#!/usr/bin/env python3
# ==============================================================================
# httpd.py — HeBi File Server
# Simple HTTP server with directory listing, download, and upload.
# Supports single files, multiple files, and full folder uploads with
# directory structure preserved.
# Stdlib only — no external dependencies.
#
# Usage: python3 httpd.py [port] [directory] [username] [password]
# ==============================================================================

import os
import sys
import io
import html
import base64
import zipfile
import urllib.parse
from http.server import SimpleHTTPRequestHandler, HTTPServer
from datetime import datetime

# Set by main() if credentials are provided
AUTH_TOKEN = None   # base64-encoded "user:pass" if auth is enabled


class FileServerHandler(SimpleHTTPRequestHandler):

    # ------------------------------------------------------------------
    # Auth check — called at the top of every request handler
    # ------------------------------------------------------------------
    def _check_auth(self):
        if AUTH_TOKEN is None:
            return True

        auth_header = self.headers.get('Authorization', '')
        if auth_header.startswith('Basic '):
            token = auth_header[6:].strip()
            if token == AUTH_TOKEN:
                return True

        self.send_response(401)
        self.send_header('WWW-Authenticate', 'Basic realm="HeBi File Server"')
        self.send_header('Content-Length', '0')
        self.end_headers()
        return False

    # ------------------------------------------------------------------
    # Auth gate for GET / HEAD
    # Intercepts ?zip requests before passing to SimpleHTTPRequestHandler
    # ------------------------------------------------------------------
    def do_GET(self):
        if not self._check_auth():
            return
        parsed = urllib.parse.urlparse(self.path)
        qs = urllib.parse.parse_qs(parsed.query)
        if 'zip' in qs or 'zip' in parsed.query.split('&'):
            self._send_zip(parsed.path)
            return
        super().do_GET()

    def do_HEAD(self):
        if not self._check_auth():
            return
        super().do_HEAD()

    # ------------------------------------------------------------------
    # Upload — POST multipart/form-data
    # Accepts optional 'relpath' field to preserve directory structure.
    # JS uploads send Accept: application/json → JSON response.
    # Form fallback sends no Accept → 303 redirect.
    # ------------------------------------------------------------------
    def do_POST(self):
        if not self._check_auth():
            return

        content_type = self.headers.get('Content-Type', '')
        if 'multipart/form-data' not in content_type:
            self._send_error(400, 'Expected multipart/form-data')
            return

        try:
            length = int(self.headers.get('Content-Length', 0))
            body = self.rfile.read(length)
        except Exception:
            self._send_error(400, 'Could not read request body')
            return

        fields = self._parse_multipart_fields(content_type, body)

        if 'file' not in fields:
            self._send_error(400, 'No file found in upload')
            return

        filename, data = fields['file']
        relpath = fields.get('relpath', ('', b''))[0].strip()

        base_dir = self.translate_path(self.path)

        if relpath:
            # Sanitize: reject .. traversal, strip leading slash
            parts = [p for p in relpath.replace('\\', '/').split('/') if p and p != '..']
            if not parts:
                self._send_error(400, 'Invalid path')
                return
            dest = os.path.join(base_dir, *parts)
        else:
            filename = os.path.basename(filename.strip())
            if not filename:
                self._send_error(400, 'Invalid filename')
                return
            dest = os.path.join(base_dir, filename)

        try:
            os.makedirs(os.path.dirname(dest), exist_ok=True)
            with open(dest, 'wb') as f:
                f.write(data)
        except OSError as e:
            self._send_error(500, f'Write failed: {e}')
            return

        # JS fetch expects JSON; form submit expects redirect
        if 'application/json' in self.headers.get('Accept', ''):
            resp = b'{"ok":true}'
            self.send_response(200)
            self.send_header('Content-Type', 'application/json')
            self.send_header('Content-Length', str(len(resp)))
            self.end_headers()
            self.wfile.write(resp)
        else:
            self.send_response(303)
            self.send_header('Location', self.path)
            self.end_headers()

    # ------------------------------------------------------------------
    # Directory listing
    # ------------------------------------------------------------------
    def list_directory(self, path):
        try:
            entries = os.listdir(path)
        except OSError:
            self._send_error(403, 'Permission denied')
            return None

        entries.sort(key=lambda n: (not os.path.isdir(os.path.join(path, n)), n.lower()))

        display_path = html.escape(urllib.parse.unquote(self.path))

        rows = []

        if self.path != '/':
            rows.append(
                '<tr><td class="name"><a href="../">..</a></td>'
                '<td class="size">-</td><td class="mtime">-</td></tr>'
            )

        for name in entries:
            fullpath = os.path.join(path, name)
            is_dir = os.path.isdir(fullpath)

            try:
                stat = os.stat(fullpath)
                mtime = datetime.fromtimestamp(stat.st_mtime).strftime('%Y-%m-%d %H:%M')
                size_str = '-' if is_dir else self._fmt_size(stat.st_size)
            except OSError:
                mtime = '-'
                size_str = '-'

            display = html.escape(name) + ('/' if is_dir else '')
            href = urllib.parse.quote(name) + ('/' if is_dir else '')
            cls = 'dir' if is_dir else 'file'
            zip_cell = (f'<td class="zip-cell"><a class="zip-link" '
                        f'href="{urllib.parse.quote(name)}/?zip" '
                        f'title="Download as zip">[zip]</a></td>') if is_dir else '<td></td>'

            rows.append(
                f'<tr class="{cls}">'
                f'<td class="name"><a href="{href}">{display}</a></td>'
                f'<td class="size">{size_str}</td>'
                f'<td class="mtime">{mtime}</td>'
                f'{zip_cell}'
                f'</tr>'
            )

        rows_html = '\n'.join(rows)

        page = f"""<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>File Server by Hexxed BitHeadz</title>
  <style>
    *, *::before, *::after {{ box-sizing: border-box; margin: 0; padding: 0; }}
    body {{
      font-family: 'Courier New', monospace;
      background: #0a0a0a;
      color: #00ff88;
      padding: 20px;
      font-size: 14px;
    }}
    header {{
      border-bottom: 1px solid #1a4a2a;
      padding-bottom: 12px;
      margin-bottom: 16px;
    }}
    h1 {{ font-size: 1em; color: #00ffcc; letter-spacing: 1px; }}
    .path {{ color: #666; margin-top: 4px; word-break: break-all; }}
    table {{ width: 100%; border-collapse: collapse; }}
    th {{
      text-align: left;
      color: #00ffcc;
      border-bottom: 1px solid #1a4a2a;
      padding: 6px 8px;
      font-size: 0.85em;
      letter-spacing: 1px;
    }}
    td {{ padding: 5px 8px; border-bottom: 1px solid #111; }}
    tr:hover td {{ background: #0f1f14; }}
    .name a {{ color: #00ff88; text-decoration: none; }}
    .name a:hover {{ color: #00ffcc; text-decoration: underline; }}
    .dir .name a {{ color: #00ccff; }}
    .size, .mtime {{ color: #558866; white-space: nowrap; }}
    .zip-cell {{ text-align: right; white-space: nowrap; width: 1%; }}
    .zip-link {{ color: #334433; font-size: 0.8em; text-decoration: none; }}
    .zip-link:hover {{ color: #00ffcc; }}

    /* Upload area */
    .upload-section {{
      margin-top: 24px;
      padding-top: 16px;
      border-top: 1px solid #1a4a2a;
    }}
    .upload-label {{
      color: #00ffcc;
      font-size: 0.85em;
      letter-spacing: 1px;
      margin-bottom: 10px;
    }}
    .drop-zone {{
      border: 1px dashed #1a4a2a;
      padding: 24px;
      text-align: center;
      color: #558866;
      cursor: pointer;
      transition: border-color 0.15s, color 0.15s;
      margin-bottom: 10px;
    }}
    .drop-zone.over {{
      border-color: #00ffcc;
      color: #00ffcc;
      background: #0a1a0f;
    }}
    .drop-zone .hint {{ font-size: 0.8em; margin-top: 6px; color: #334433; }}
    .btn-row {{
      display: flex;
      gap: 8px;
      flex-wrap: wrap;
      align-items: center;
    }}
    .btn {{
      background: #0f2a1a;
      color: #00ff88;
      border: 1px solid #00ff88;
      padding: 5px 14px;
      cursor: pointer;
      font-family: inherit;
      font-size: 0.9em;
      letter-spacing: 1px;
    }}
    .btn:hover {{ background: #1a4a2a; color: #00ffcc; border-color: #00ffcc; }}
    .status {{
      margin-top: 10px;
      font-size: 0.85em;
      color: #558866;
      min-height: 1.2em;
      word-break: break-all;
    }}
    .status.scanning {{ color: #00ffcc; }}
    .status.error    {{ color: #ff4444; }}
    .status.done     {{ color: #00ff88; }}
    .progress-wrap {{
      margin-top: 8px;
      background: #0f1f14;
      border: 1px solid #1a4a2a;
      height: 6px;
      display: none;
    }}
    .progress-wrap.visible {{ display: block; }}
    .progress-bar {{
      background: #00ff88;
      height: 100%;
      width: 0%;
      transition: width 0.15s ease;
    }}
    input[type="file"] {{ display: none; }}
  </style>
</head>
<body>
  <header>
    <h1>File Server by Hexxed BitHeadz</h1>
    <div class="path">{display_path}</div>
  </header>

  <table>
    <thead>
      <tr><th>NAME</th><th>SIZE</th><th>MODIFIED</th><th></th></tr>
    </thead>
    <tbody>
      {rows_html}
    </tbody>
  </table>

  <div class="upload-section">
    <div class="upload-label">UPLOAD</div>

    <div class="drop-zone" id="dropZone">
      Drop files or folders here
      <div class="hint">or use the buttons below</div>
    </div>

    <div class="btn-row">
      <button class="btn" onclick="document.getElementById('filePicker').click()">Files</button>
      <button class="btn" onclick="document.getElementById('folderPicker').click()">Folder</button>
    </div>

    <input type="file" id="filePicker" multiple>
    <input type="file" id="folderPicker" webkitdirectory multiple>

    <div class="progress-wrap" id="progressWrap">
      <div class="progress-bar" id="progressBar"></div>
    </div>
    <div class="status" id="status"></div>
  </div>

  <script>
    const dropZone    = document.getElementById('dropZone');
    const status      = document.getElementById('status');
    const progressWrap = document.getElementById('progressWrap');
    const progressBar  = document.getElementById('progressBar');
    const uploadUrl   = window.location.pathname;

    function setProgress(pct) {{
      progressWrap.classList.add('visible');
      progressBar.style.width = pct + '%';
    }}

    function resetProgress() {{
      progressWrap.classList.remove('visible');
      progressBar.style.width = '0%';
    }}

    // ----------------------------------------------------------------
    // Recursively collect files from a FileSystemEntry
    // Updates the status label as it finds files.
    // ----------------------------------------------------------------
    let _scanCount = 0;

    async function collectEntries(entry, basePath) {{
      const files = [];
      if (entry.isFile) {{
        const file = await new Promise(r => entry.file(r));
        files.push({{ file, relpath: basePath + entry.name }});
        _scanCount++;
        status.textContent = `Scanning... ${{_scanCount}} file${{_scanCount !== 1 ? 's' : ''}} found`;
      }} else if (entry.isDirectory) {{
        const reader = entry.createReader();
        let batch;
        do {{
          batch = await new Promise(r => reader.readEntries(r));
          for (const child of batch) {{
            const sub = await collectEntries(child, basePath + entry.name + '/');
            files.push(...sub);
          }}
        }} while (batch.length > 0);
      }}
      return files;
    }}

    // ----------------------------------------------------------------
    // Upload a list of {{ file, relpath }} objects one by one
    // ----------------------------------------------------------------
    async function uploadAll(items) {{
      const total = items.length;
      setProgress(0);
      status.className = 'status';
      status.textContent = `Uploading 0 / ${{total}}`;

      for (let i = 0; i < total; i++) {{
        const {{ file, relpath }} = items[i];
        const pct = Math.round(((i + 1) / total) * 100);
        status.textContent = `${{i + 1}} / ${{total}} (${{pct}}%) — ${{relpath}}`;
        setProgress(pct);

        const fd = new FormData();
        fd.append('file', file, file.name);
        fd.append('relpath', relpath);

        const resp = await fetch(uploadUrl, {{
          method: 'POST',
          headers: {{ 'Accept': 'application/json' }},
          body: fd
        }});

        if (!resp.ok) {{
          status.className = 'status error';
          status.textContent = `Error uploading ${{relpath}}`;
          resetProgress();
          return;
        }}
      }}

      setProgress(100);
      status.className = 'status done';
      status.textContent = `Done — ${{total}} file${{total !== 1 ? 's' : ''}} uploaded`;
      setTimeout(() => window.location.reload(), 1200);
    }}

    // ----------------------------------------------------------------
    // Drag and drop — handles both files and folders
    // ----------------------------------------------------------------
    dropZone.addEventListener('dragover', e => {{
      e.preventDefault();
      dropZone.classList.add('over');
    }});

    dropZone.addEventListener('dragleave', () => {{
      dropZone.classList.remove('over');
    }});

    dropZone.addEventListener('drop', async e => {{
      e.preventDefault();
      dropZone.classList.remove('over');

      // Collect ALL FileSystemEntry objects synchronously before any await —
      // dataTransfer.items goes stale after the first await.
      const entries = [];
      for (const item of e.dataTransfer.items) {{
        const entry = item.webkitGetAsEntry ? item.webkitGetAsEntry() : null;
        if (entry) entries.push(entry);
      }}

      if (!entries.length) return;

      _scanCount = 0;
      status.className = 'status scanning';
      status.textContent = 'Scanning...';

      const allFiles = [];
      for (const entry of entries) {{
        const collected = await collectEntries(entry, '');
        allFiles.push(...collected);
      }}

      if (allFiles.length) uploadAll(allFiles);
    }});

    // ----------------------------------------------------------------
    // File picker (flat, multiple)
    // ----------------------------------------------------------------
    document.getElementById('filePicker').addEventListener('change', function() {{
      _scanCount = 0;
      const items = Array.from(this.files).map(f => ({{
        file: f,
        relpath: f.name
      }}));
      if (items.length) uploadAll(items);
      this.value = '';
    }});

    // ----------------------------------------------------------------
    // Folder picker (webkitdirectory — preserves relative paths)
    // ----------------------------------------------------------------
    document.getElementById('folderPicker').addEventListener('change', function() {{
      _scanCount = 0;
      const items = Array.from(this.files).map(f => ({{
        file: f,
        relpath: f.webkitRelativePath || f.name
      }}));
      if (items.length) uploadAll(items);
      this.value = '';
    }});
  </script>
</body>
</html>"""

        encoded = page.encode('utf-8')
        self.send_response(200)
        self.send_header('Content-Type', 'text/html; charset=utf-8')
        self.send_header('Content-Length', str(len(encoded)))
        self.end_headers()
        return io.BytesIO(encoded)

    # ------------------------------------------------------------------
    # Helpers
    # ------------------------------------------------------------------
    def _parse_multipart_fields(self, content_type, body):
        """Parse all fields from multipart/form-data.
        Returns dict of field_name -> (str_value_or_filename, bytes)."""
        boundary = None
        for part in content_type.split(';'):
            part = part.strip()
            if part.startswith('boundary='):
                boundary = part[9:].strip('"')
                break

        if not boundary:
            return {}

        delimiter = ('--' + boundary).encode()
        chunks = body.split(delimiter)
        fields = {}

        for chunk in chunks[1:]:
            if chunk.startswith(b'--'):
                break
            if b'\r\n\r\n' not in chunk:
                continue

            headers_raw, data = chunk.split(b'\r\n\r\n', 1)
            data = data.rstrip(b'\r\n')
            headers = headers_raw.decode('utf-8', errors='replace')

            field_name = None
            filename = None

            for line in headers.split('\r\n'):
                if 'Content-Disposition' in line:
                    for item in line.split(';'):
                        item = item.strip()
                        if item.lower().startswith('name='):
                            field_name = item[5:].strip('"').strip("'")
                        elif item.lower().startswith('filename='):
                            filename = item[9:].strip('"').strip("'")

            if field_name is not None:
                value = filename if filename else data.decode('utf-8', errors='replace')
                fields[field_name] = (value, data)

        return fields

    # ------------------------------------------------------------------
    # Zip a directory and stream it as a download
    # ------------------------------------------------------------------
    def _send_zip(self, url_path):
        fs_path = self.translate_path(url_path)

        if not os.path.isdir(fs_path):
            self._send_error(400, 'Not a directory')
            return

        dirname = os.path.basename(fs_path.rstrip(os.sep)) or 'archive'

        buf = io.BytesIO()
        try:
            with zipfile.ZipFile(buf, 'w', zipfile.ZIP_DEFLATED) as zf:
                for root, dirs, files in os.walk(fs_path):
                    dirs.sort()
                    for fname in sorted(files):
                        full = os.path.join(root, fname)
                        arcname = os.path.relpath(full, fs_path)
                        zf.write(full, arcname)
        except OSError as e:
            self._send_error(500, f'Zip failed: {e}')
            return

        data = buf.getvalue()
        self.send_response(200)
        self.send_header('Content-Type', 'application/zip')
        self.send_header('Content-Disposition', f'attachment; filename="{dirname}.zip"')
        self.send_header('Content-Length', str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def _fmt_size(self, size):
        for unit in ('B', 'KB', 'MB', 'GB'):
            if size < 1024:
                return f'{size:.0f} {unit}'
            size /= 1024
        return f'{size:.1f} TB'

    def _send_error(self, code, message):
        body = message.encode()
        self.send_response(code)
        self.send_header('Content-Type', 'text/plain')
        self.send_header('Content-Length', str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, fmt, *args):
        pass


# ------------------------------------------------------------------
# Entry point
# ------------------------------------------------------------------
def main():
    global AUTH_TOKEN

    port      = int(sys.argv[1]) if len(sys.argv) > 1 else 8080
    directory = sys.argv[2]      if len(sys.argv) > 2 else '/root'
    username  = sys.argv[3]      if len(sys.argv) > 3 else None
    password  = sys.argv[4]      if len(sys.argv) > 4 else None

    if username and password:
        AUTH_TOKEN = base64.b64encode(f'{username}:{password}'.encode()).decode()
        print(f'[+] Auth enabled (user: {username})', flush=True)
    else:
        print('[!] Auth disabled — no password set', flush=True)

    os.makedirs(directory, exist_ok=True)
    os.chdir(directory)

    server = HTTPServer(('0.0.0.0', port), FileServerHandler)
    print(f'[+] Serving {directory} on http://0.0.0.0:{port}', flush=True)

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()
        print('[+] Server stopped.', flush=True)


if __name__ == '__main__':
    main()

#!/usr/bin/env python3
"""Minimal OCI registry stub for oras smoke tests."""

from __future__ import annotations

import argparse
import hashlib
import json
import sys
import threading
import urllib.parse
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer


FILES_ARTIFACT_TYPE = "application/vnd.riscos.fileset.v1"
MANIFEST_MEDIA_TYPE = "application/vnd.oci.image.manifest.v1+json"


def sha256_digest(data: bytes) -> str:
    return "sha256:" + hashlib.sha256(data).hexdigest()


def make_fixture_state() -> dict:
    blob = b"fixture pull data\n"
    blob_digest = sha256_digest(blob)
    manifest = {
        "schemaVersion": 2,
        "mediaType": MANIFEST_MEDIA_TYPE,
        "artifactType": FILES_ARTIFACT_TYPE,
        "annotations": {
            "org.riscos.artifact.format": "fileset-v1",
        },
        "layers": [
            {
                "mediaType": "application/octet-stream",
                "digest": blob_digest,
                "size": len(blob),
                "annotations": {
                    "org.opencontainers.image.title": "fixture,fff",
                    "org.riscos.filetype": "FFF",
                },
            }
        ],
    }
    manifest_bytes = json.dumps(manifest, separators=(",", ":")).encode("utf-8")
    manifest_digest = sha256_digest(manifest_bytes)
    return {
        "repos": {
            "demo/pull": {
                "tags": {"latest": manifest_digest},
                "manifests": {manifest_digest: manifest_bytes},
                "blobs": {blob_digest: blob},
            },
            "demo/push": {
                "tags": {},
                "manifests": {},
                "blobs": {},
            },
            "demo/basic": {
                "tags": {"latest": manifest_digest},
                "manifests": {manifest_digest: manifest_bytes},
                "blobs": {blob_digest: blob},
            },
            "demo/bearer": {"tags": {}, "manifests": {}, "blobs": {}},
        },
        "uploads": {},
        "next_upload": 1,
    }


class RegistryHandler(BaseHTTPRequestHandler):
    server_version = "oras-fake-registry/0.1"
    protocol_version = "HTTP/1.1"

    def log_message(self, fmt, *args):
        sys.stderr.write("%s - - [%s] %s\n" %
                         (self.address_string(), self.log_date_time_string(), fmt % args))

    @property
    def state(self):
        return self.server.state

    def _read_body(self) -> bytes:
        length = int(self.headers.get("Content-Length", "0"))
        return self.rfile.read(length) if length else b""

    def _send(self, status: int, body: bytes = b"", headers: dict | None = None):
        self.send_response(status)
        if headers:
            for key, value in headers.items():
                self.send_header(key, value)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        if body:
            self.wfile.write(body)

    def _split_v2_path(self):
        path = urllib.parse.urlparse(self.path).path
        if not path.startswith("/v2/"):
            return None
        remainder = path[4:]
        for marker in ("/manifests/", "/blobs/", "/tags/list", "/blobs/uploads/"):
            if marker in remainder:
                index = remainder.index(marker)
                repo = remainder[:index]
                action = remainder[index:]
                return repo, action
        return None

    def _authorised(self, repo: str) -> bool:
        if repo == "demo/basic":
            expected = "Basic dGVzdDpzZWNyZXQ="
            if self.headers.get("Authorization") != expected:
                self._send(401, headers={"WWW-Authenticate": 'Basic realm="fake"'})
                return False
        if repo == "demo/bearer":
            if self.headers.get("Authorization") != "Bearer test-bearer":
                realm = f"http://localhost:{self.server.server_port}/token"
                self._send(401, headers={"WWW-Authenticate":
                          f'Bearer realm="{realm}",service="fake",scope="repository:{repo}:pull,push"'})
                return False
        return True

    def do_GET(self):
        if urllib.parse.urlparse(self.path).path == "/token":
            if self.headers.get("Authorization") != "Basic dGVzdDpzZWNyZXQ=":
                self._send(401)
            else:
                self._send(200, b'{"access_token":"test-bearer"}', {"Content-Type": "application/json"})
            return
        split = self._split_v2_path()
        if split is None:
            self._send(404)
            return
        repo, action = split
        if not self._authorised(repo):
            return
        repo_state = self.state["repos"].get(repo)
        if repo_state is None:
            self._send(404)
            return
        if action == "/tags/list":
            payload = {
                "name": repo,
                "tags": sorted(repo_state["tags"].keys()),
            }
            self._send(200,
                       json.dumps(payload).encode("utf-8"),
                       {"Content-Type": "application/json"})
            return
        if action.startswith("/manifests/"):
            selector = action[len("/manifests/"):]
            digest = repo_state["tags"].get(selector, selector)
            manifest = repo_state["manifests"].get(digest)
            if manifest is None:
                self._send(404)
                return
            self._send(200, manifest, {"Content-Type": MANIFEST_MEDIA_TYPE})
            return
        if action.startswith("/blobs/"):
            digest = action[len("/blobs/"):]
            blob = repo_state["blobs"].get(digest)
            if blob is None:
                self._send(404)
                return
            self._send(200, blob, {"Content-Type": "application/octet-stream"})
            return
        self._send(404)

    def do_POST(self):
        split = self._split_v2_path()
        if split is None:
            self._send(404)
            return
        repo, action = split
        if not self._authorised(repo):
            return
        if action != "/blobs/uploads/":
            self._send(404)
            return
        if repo not in self.state["repos"]:
            self.state["repos"][repo] = {"tags": {}, "manifests": {}, "blobs": {}}
        upload_id = str(self.state["next_upload"])
        self.state["next_upload"] += 1
        self.state["uploads"][upload_id] = {"repo": repo}
        self._send(202, b"", {"Location": f"/upload/{upload_id}"})

    def do_PUT(self):
        parsed = urllib.parse.urlparse(self.path)
        if parsed.path.startswith("/upload/"):
            upload_id = parsed.path.split("/")[-1]
            upload = self.state["uploads"].get(upload_id)
            if upload is None:
                self._send(404)
                return
            if not self._authorised(upload["repo"]):
                return
            self.state["uploads"].pop(upload_id)
            query = urllib.parse.parse_qs(parsed.query)
            digest = query.get("digest", [None])[0]
            body = self._read_body()
            actual = sha256_digest(body)
            if digest != actual:
                self._send(400, b"digest mismatch")
                return
            repo_state = self.state["repos"][upload["repo"]]
            repo_state["blobs"][digest] = body
            self._send(201, b"", {"Docker-Content-Digest": digest})
            return

        split = self._split_v2_path()
        if split is None:
            self._send(404)
            return
        repo, action = split
        if not self._authorised(repo):
            return
        if not action.startswith("/manifests/"):
            self._send(404)
            return
        selector = action[len("/manifests/"):]
        body = self._read_body()
        digest = sha256_digest(body)
        if repo not in self.state["repos"]:
            self.state["repos"][repo] = {"tags": {}, "manifests": {}, "blobs": {}}
        repo_state = self.state["repos"][repo]
        repo_state["manifests"][digest] = body
        repo_state["tags"][selector] = digest
        self._send(201, b"", {"Docker-Content-Digest": digest})


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", type=int, required=True)
    args = parser.parse_args()

    state = make_fixture_state()
    httpd = ThreadingHTTPServer(("127.0.0.1", args.port), RegistryHandler)
    httpd.state = state

    thread = threading.Thread(target=httpd.serve_forever)
    thread.daemon = True
    thread.start()
    thread.join()


if __name__ == "__main__":
    main()

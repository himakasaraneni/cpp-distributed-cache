#!/usr/bin/env python3
"""Stop a key's primary container and verify replica failover."""
import subprocess, sys, time, urllib.request
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from client.cluster import DistributedCache
PORT_TO_SERVICE = {"8081": "cache-1", "8082": "cache-2",
                   "8083": "cache-3", "8084": "cache-4"}
def compose(*args):
    subprocess.run(["docker", "compose", *args], check=True)
def wait_for_nodes():
    for port in PORT_TO_SERVICE:
        for _ in range(30):
            try:
                urllib.request.urlopen(f"http://localhost:{port}/health", timeout=.5)
                break
            except OSError: time.sleep(1)
        else: raise RuntimeError(f"node on port {port} did not become healthy")
def main():
    # All four services use the same image. Build once to avoid parallel
    # exporters racing to write the same image tag.
    compose("build", "cache-1")
    compose("up", "-d", "--no-build", "--wait")
    stopped = None
    try:
        wait_for_nodes()
        cache = DistributedCache(timeout=.5)
        key, value = "failure-test-key", b"survives-primary-failure"
        cache.put(key, value)
        stopped = PORT_TO_SERVICE[cache.owners(key)[0].rsplit(":", 1)[1]]
        compose("stop", stopped)
        assert cache.get(key) == value
        print(f"PASS: replica served value after stopping {stopped}")
    finally:
        if stopped: compose("start", stopped)
if __name__ == "__main__": main()

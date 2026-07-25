"""Dependency-free distributed-cache client using rendezvous hashing."""
from __future__ import annotations
import hashlib
import json
import os
import urllib.error
import urllib.parse
import urllib.request
from dataclasses import dataclass

DEFAULT_NODES = tuple(value.strip() for value in os.getenv(
    "CACHE_NODES", "http://localhost:8081,http://localhost:8082,"
    "http://localhost:8083,http://localhost:8084").split(",") if value.strip())

@dataclass
class CacheError(RuntimeError):
    operation: str
    errors: list[str]
    def __str__(self):
        return f"{self.operation} failed: {'; '.join(self.errors)}"

class DistributedCache:
    def __init__(self, nodes=DEFAULT_NODES, replicas=2, timeout=1.0):
        self.nodes = tuple(node.rstrip("/") for node in nodes)
        if not self.nodes: raise ValueError("at least one node is required")
        if not 1 <= replicas <= len(self.nodes): raise ValueError("invalid replica count")
        self.replicas, self.timeout = replicas, timeout
    def owners(self, key):
        ranked = sorted(self.nodes, key=lambda node:
            hashlib.sha256(f"{key}\0{node}".encode()).digest(), reverse=True)
        return tuple(ranked[:self.replicas])
    def _request(self, node, key, method, value=None):
        safe_key = urllib.parse.quote(key, safe="")
        request = urllib.request.Request(f"{node}/cache/{safe_key}", data=value, method=method)
        return urllib.request.urlopen(request, timeout=self.timeout)
    def put(self, key, value):
        payload = value.encode() if isinstance(value, str) else value
        errors, successes = [], 0
        for node in self.owners(key):
            try:
                with self._request(node, key, "PUT", payload): successes += 1
            except (OSError, urllib.error.URLError) as error: errors.append(f"{node}: {error}")
        if successes == 0: raise CacheError("PUT", errors)
    def get(self, key):
        errors = []
        for node in self.owners(key):
            try:
                with self._request(node, key, "GET") as response: return response.read()
            except urllib.error.HTTPError as error:
                if error.code != 404: errors.append(f"{node}: HTTP {error.code}")
            except (OSError, urllib.error.URLError) as error: errors.append(f"{node}: {error}")
        if len(errors) == self.replicas: raise CacheError("GET", errors)
        return None
    def delete(self, key):
        errors = []
        for node in self.owners(key):
            try:
                with self._request(node, key, "DELETE"): pass
            except urllib.error.HTTPError as error:
                if error.code != 404: errors.append(f"{node}: HTTP {error.code}")
            except (OSError, urllib.error.URLError) as error: errors.append(f"{node}: {error}")
        if len(errors) == self.replicas: raise CacheError("DELETE", errors)
    def stats(self):
        result = []
        for node in self.nodes:
            try:
                with urllib.request.urlopen(f"{node}/stats", timeout=self.timeout) as response:
                    result.append(json.load(response))
            except (OSError, urllib.error.URLError):
                result.append({"node": node, "status": "unavailable"})
        return result

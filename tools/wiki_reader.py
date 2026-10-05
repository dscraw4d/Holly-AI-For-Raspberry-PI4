#!/usr/bin/env python3
"""Fetch one English Wikipedia summary over HTTPS and teach Holly via SSH."""
import argparse
import json
import re
import subprocess
import sys
import unicodedata
from urllib.error import HTTPError, URLError
from urllib.parse import quote, urlsplit
from urllib.request import Request, urlopen

API = "https://en.wikipedia.org/api/rest_v1/page/summary/"
SOURCE = "https://en.wikipedia.org/wiki/"
MAX_RESPONSE = 65536
MAX_EXCERPT = 175


def article_url(url):
    parsed = urlsplit(url)
    if parsed.scheme != "https" or parsed.netloc != "en.wikipedia.org" or not parsed.path.startswith("/wiki/"):
        raise ValueError("Wikipedia returned an unexpected article URL")
    if parsed.query or parsed.fragment or not parsed.path[6:]:
        raise ValueError("Wikipedia article URL cannot have a query or fragment")
    if not re.fullmatch(r"[A-Za-z0-9_.()~%/-]+", parsed.path[6:]) or "/" in parsed.path[6:]:
        raise ValueError("Article URL contains unsupported characters")
    if len(url) >= 97:
        raise ValueError("Article URL is too long for Holly's source field")
    return url


def excerpt(text):
    if not isinstance(text, str):
        raise ValueError("Wikipedia did not provide a text extract")
    plain = unicodedata.normalize("NFKD", text).encode("ascii", "ignore").decode("ascii")
    plain = " ".join(plain.split())
    first = re.split(r"(?<=[.!?])\s+", plain, maxsplit=1)[0]
    if not first or "|" in first or any(ord(c) < 32 for c in first):
        raise ValueError("The opening sentence is unavailable or contains unsupported characters")
    if len(first) > MAX_EXCERPT:
        shortened = first[:MAX_EXCERPT - 3].rsplit(" ", 1)[0]
        if not shortened:
            raise ValueError("The opening sentence has no usable words")
        first = shortened + "..."
    return first


def prepare(title, payload):
    if payload.get("type") == "disambiguation":
        raise ValueError("That is a disambiguation page; select a specific article")
    source = article_url(payload.get("content_urls", {}).get("desktop", {}).get("page", ""))
    fact = excerpt(payload.get("extract"))
    command = f"wiki import {fact} | {source}"
    if len(command) >= 320:
        raise ValueError("Excerpt and URL exceed Holly's SSH command limit")
    return command


def fetch(title, contact):
    if not title or len(title) > 80 or any(ord(c) < 32 for c in title):
        raise ValueError("Use an article title of at most 80 characters")
    if not re.fullmatch(r"https://[A-Za-z0-9.-]+(?:/[A-Za-z0-9_./~-]*)?", contact):
        raise ValueError("--contact must be an HTTPS project URL")
    url = API + quote(title.replace(" ", "_"), safe="_()")
    request = Request(url, headers={
        "User-Agent": f"HollyWikiReader/0.33 ({contact}; one user-requested article)",
        "Accept": "application/json",
    })
    with urlopen(request, timeout=15) as response:
        raw = response.read(MAX_RESPONSE + 1)
        if len(raw) > MAX_RESPONSE:
            raise ValueError("Wikipedia response exceeds the reader's limit")
        if "json" not in response.headers.get("Content-Type", ""):
            raise ValueError("Wikipedia response was not JSON")
    return json.loads(raw)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", required=True, help="Pi address from the router's DHCP list")
    parser.add_argument("--title", required=True, help="English Wikipedia article title")
    parser.add_argument("--contact", default="https://blitter.ca", help="HTTPS project URL in the Wikimedia User-Agent")
    parser.add_argument("--dry-run", action="store_true", help="Fetch and print the import command without sending it")
    args = parser.parse_args(argv)
    try:
        if not re.fullmatch(r"[A-Za-z0-9.:-]+", args.host) or args.host.startswith("-"):
            raise ValueError("--host must be a LAN IP address or hostname")
        command = prepare(args.title, fetch(args.title, args.contact))
        if args.dry_run:
            print(command)
            return 0
        completed = subprocess.run(["ssh", "-o", "RekeyLimit=4M", "holly@" + args.host, command], check=False)
        return completed.returncode
    except HTTPError as error:
        print(f"Wikipedia HTTP {error.code}; try later if rate limited. Retry-After: {error.headers.get('Retry-After', 'unspecified')}", file=sys.stderr)
    except (URLError, TimeoutError, ValueError, json.JSONDecodeError) as error:
        print(f"Wikipedia reader: {error}", file=sys.stderr)
    except FileNotFoundError:
        print("OpenSSH client not found; install or enable it on this computer", file=sys.stderr)
    return 1


if __name__ == "__main__":
    raise SystemExit(main())

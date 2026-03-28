#!/usr/bin/env python3

from __future__ import annotations

import argparse
import html
import json
import re
import sys
import time
import urllib.parse
import urllib.request
from html.parser import HTMLParser
from pathlib import Path


API_URL = "https://ck.uesp.net/w/api.php"
INDEX_TITLE = "Condition_Functions"
USER_AGENT = "SVSConditionDocScraper/1.0 (+https://github.com/PenguinToast/SkyrimVanitySystem)"


class HTMLTextExtractor(HTMLParser):
    def __init__(self) -> None:
        super().__init__()
        self.parts: list[str] = []

    def handle_data(self, data: str) -> None:
        if data:
            self.parts.append(data)

    def get_text(self) -> str:
        text = html.unescape(" ".join(self.parts))
        text = re.sub(r"\s+", " ", text)
        return text.strip()


def api_get(params: dict[str, str]) -> dict:
    query = urllib.parse.urlencode(params)
    request = urllib.request.Request(
        f"{API_URL}?{query}",
        headers={"User-Agent": USER_AGENT},
    )
    with urllib.request.urlopen(request, timeout=30) as response:
        return json.load(response)


def strip_html(html_text: str) -> str:
    extractor = HTMLTextExtractor()
    extractor.feed(html_text)
    return extractor.get_text()


def iter_condition_function_titles() -> list[str]:
    titles: list[str] = []
    continuation: dict[str, str] = {}

    while True:
        response = api_get(
            {
                "action": "query",
                "titles": INDEX_TITLE,
                "prop": "links",
                "pllimit": "max",
                "format": "json",
                "formatversion": "2",
                **continuation,
            }
        )

        page = response["query"]["pages"][0]
        for link in page.get("links", []):
            title = link.get("title", "").strip()
            if not title or title == "Conditions":
                continue
            titles.append(title)

        if "continue" not in response:
            break
        continuation = response["continue"]

    return titles


def fetch_function_doc(title: str) -> dict:
    response = api_get(
        {
            "action": "parse",
            "page": title,
            "prop": "text",
            "format": "json",
            "formatversion": "2",
        }
    )

    html_text = response["parse"]["text"]
    text = strip_html(html_text)
    lowered = text.lower()
    obsolete = (
        "obsolete" in lowered
        or "deprecated" in lowered
        or "not used in skyrim" in lowered
    )

    return {
        "title": title,
        "url": f"https://ck.uesp.net/wiki/{urllib.parse.quote(title.replace(' ', '_'))}",
        "obsolete": obsolete,
        "text": text,
    }


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Scrape live CK condition-function docs through the MediaWiki API."
    )
    parser.add_argument(
        "--output",
        type=Path,
        help="Write scraped JSON to this file instead of stdout.",
    )
    parser.add_argument(
        "--limit",
        type=int,
        default=0,
        help="Only fetch the first N function pages (0 = all).",
    )
    parser.add_argument(
        "--delay-ms",
        type=int,
        default=0,
        help="Optional delay between page fetches.",
    )
    args = parser.parse_args()

    titles = iter_condition_function_titles()
    if args.limit > 0:
        titles = titles[: args.limit]

    docs = []
    for index, title in enumerate(titles, start=1):
        docs.append(fetch_function_doc(title))
        print(f"[{index}/{len(titles)}] {title}", file=sys.stderr)
        if args.delay_ms > 0 and index < len(titles):
            time.sleep(args.delay_ms / 1000.0)

    payload = {
        "source": INDEX_TITLE,
        "count": len(docs),
        "functions": docs,
    }

    output_text = json.dumps(payload, indent=2, ensure_ascii=False)
    if args.output:
        args.output.write_text(output_text, encoding="utf-8")
    else:
        print(output_text)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())

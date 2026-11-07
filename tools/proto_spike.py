#!/usr/bin/env python3
"""HaikuNCM 协议探针 —— 用来确定 weapi/eapi 的线上真实行为。

跑通后，C++ 端只需机械照抄这里的字节序列。
依赖: pycryptodome
"""
import base64
import hashlib
import json
import random
import string
import sys
import urllib.parse
import urllib.request

from Crypto.Cipher import AES

MODULUS = (
    "00e0b509f6259df8642dbc35662901477df22677ec152b5ff68ace615bb7b725"
    "152b3ab17a876aea8a5aa76d2e417629ec4ee341f56135fccf695280104e0312"
    "ecbda92557c93870114af6c9d05c4f7f0c3685b7a46bee255932575cce10b424"
    "d813cfe4875d3e82047b97ddef52741d546b8e289dc6935b3ece0462db0a22b8e7"
)
PUBKEY = "010001"
NONCE = "0CoJUm6Qyw8W8jud"
IV = "0102030405060708"
EAPI_KEY = "e82ckenh8dichen8"
UA = (
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36"
)
COOKIE = "os=pc; appver=8.9.70; osver=Microsoft-Windows-10-Professional-build-19045-64bit; channel=netease"


def _pad(data: bytes) -> bytes:
    n = 16 - len(data) % 16
    return data + bytes([n]) * n


def aes_cbc_b64(text: str, key: str) -> str:
    raw = AES.new(key.encode(), AES.MODE_CBC, IV.encode()).encrypt(_pad(text.encode()))
    return base64.b64encode(raw).decode()


def rsa_encrypt(text: str) -> str:
    m = int.from_bytes(text.encode(), "big")
    c = pow(m, int(PUBKEY, 16), int(MODULUS, 16))
    return format(c, "x").rjust(256, "0")


def weapi(payload: dict) -> dict:
    text = json.dumps(payload, separators=(",", ":"))
    secret = "".join(random.choices(string.ascii_letters + string.digits, k=16))
    params = aes_cbc_b64(aes_cbc_b64(text, NONCE), secret)
    return {"params": params, "encSecKey": rsa_encrypt(secret[::-1])}


def eapi(path: str, payload: dict) -> dict:
    text = json.dumps(payload, separators=(",", ":"))
    digest = hashlib.md5(f"nobody{path}use{text}md5forencrypt".encode()).hexdigest()
    joined = f"{path}-36cd479b6b5-{text}-36cd479b6b5-{digest}"
    raw = AES.new(EAPI_KEY.encode(), AES.MODE_ECB).encrypt(_pad(joined.encode()))
    return {"params": raw.hex().upper()}


def post(url: str, form: dict, referer="https://music.163.com") -> dict:
    body = urllib.parse.urlencode(form).encode()
    req = urllib.request.Request(
        url,
        data=body,
        headers={
            "User-Agent": UA,
            "Referer": referer,
            "Cookie": COOKIE,
            "Content-Type": "application/x-www-form-urlencoded",
        },
    )
    with urllib.request.urlopen(req, timeout=20) as resp:
        return json.loads(resp.read().decode())


def main() -> int:
    keyword = sys.argv[1] if len(sys.argv) > 1 else "千本桜"
    print(f"[1] 搜索: {keyword}")
    # 注意: /weapi/cloudsearch/get/web 对匿名请求返回 50000005, 用旧的 search/get
    result = post(
        "https://music.163.com/weapi/search/get",
        weapi({"s": keyword, "type": 1, "limit": 5, "offset": 0}),
    )
    print("    code =", result.get("code"), " songs =", (result.get("result") or {}).get("songCount"))
    songs = ((result.get("result") or {}).get("songs")) or []
    if not songs:
        print("    !! 没有结果:", json.dumps(result)[:300])
        return 1
    for s in songs[:5]:
        artist = (s.get("ar") or [{}])[0].get("name") or (s.get("artists") or [{}])[0].get("name")
        ms = s.get("dt") or s.get("duration")
        print(f"    {s['id']:>12}  {s['name']}  -  {artist}  fee={s.get('fee')}  {ms}ms")

    song = songs[0]
    print(f"[2] 取流: {song['name']} (id={song['id']})")
    url_path = "/api/song/enhance/player/url/v1"
    res = post(
        "https://interface3.music.163.com/eapi/song/enhance/player/url/v1",
        eapi(url_path, {"ids": f"[{song['id']}]", "level": "standard", "encodeType": "flac"}),
        referer="https://music.163.com",
    )
    print("    code =", res.get("code"))
    items = res.get("data") or []
    if not items or not items[0].get("url"):
        print("    !! 没拿到直链:", json.dumps(res)[:300])
        return 2
    item = items[0]
    print(f"    url  = {item['url'][:110]}")
    print(f"    type = {item.get('type')}  br = {item.get('br')}  size = {item.get('size')}")

    print("[3] 下载")
    req = urllib.request.Request(item["url"], headers={"User-Agent": UA, "Cookie": COOKIE})
    with urllib.request.urlopen(req, timeout=30) as resp, open("probe_sample.mp3", "wb") as f:
        f.write(resp.read())
    import os

    print("    probe_sample.mp3 =", os.path.getsize("probe_sample.mp3"), "bytes")
    return 0


if __name__ == "__main__":
    sys.exit(main())

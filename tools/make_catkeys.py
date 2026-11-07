#!/usr/bin/env python3
"""从 C++ 源码里抽取 B_TRANSLATE() 字符串，生成 Haiku 的 .catkeys 模板。

为什么不用 collectcatkeys：beta6 上它把 B_TRANSLATE 展开成
B_CATKEY((ctx), (str)) 后一个键都认不出来（产出空 catalog）。
这个脚本直接扫源码，格式与 catkeys 一致，linkcatkeys 可以直接吃。

用法：make_catkeys.py <signature> <language> <file.cpp>...
"""
import re
import sys

CONTEXT_RE = re.compile(r'#define\s+B_TRANSLATION_CONTEXT\s+"([^"]*)"')
TRANSLATE_RE = re.compile(r'B_TRANSLATE\s*\(\s*"((?:[^"\\]|\\.)*)"\s*\)')


def unescape(text):
    return (text.replace('\\"', '"')
                .replace("\\n", "\n")
                .replace("\\t", "\t")
                .replace("\\\\", "\\"))


def main(argv):
    if len(argv) < 4:
        print(__doc__, file=sys.stderr)
        return 1

    signature, language = argv[1], argv[2]
    entries = []          # (context, string)，保持首次出现顺序
    seen = set()

    for path in argv[3:]:
        context = ""
        with open(path, encoding="utf-8") as handle:
            for line in handle:
                found = CONTEXT_RE.search(line)
                if found:
                    context = found.group(1)
                    continue
                for match in TRANSLATE_RE.finditer(line):
                    text = unescape(match.group(1))
                    key = (context, text)
                    if key in seen:
                        continue
                    seen.add(key)
                    entries.append(key)

    # 头行：版本 / 语言 / 签名 / 校验和（0 表示不校验）
    print("1\t%s\t%s\t0" % (language, signature))
    for context, text in entries:
        print("%s\t%s\t\t%s" % (context, text, context))

    print("extracted %d key(s) from %d file(s)"
          % (len(entries), len(argv) - 3), file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))

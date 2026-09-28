#!/usr/bin/env python3
"""校验模版引用的 CSS 类在生产构建产物中存在。

背景：Chirpy gem 内置裁剪版 Bootstrap（_sass/vendors/_bootstrap.scss），
本地开发模式走 CDN 全量 bootstrap@5，模版里用到的类可能本地有样式、
线上 bundle 缺失（如 nav 卡的 g-3 间距）。此脚本扫描模版 class 属性，
对照构建出的 CSS 逐个检查，缺类即非零退出，供 pre-commit 与
pages-deploy workflow 调用。

用法: python3 tools/check_css_classes.py <built.css>
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TEMPLATE_GLOBS = ["_includes/**/*.html", "_layouts/**/*.html", "_tabs/*.md", "index.html"]
ALLOWLIST = Path(__file__).resolve().parent / "css-class-allowlist.txt"

# Liquid 标签拆开后残留的控制词，不是 class
LIQUID_KEYWORDS = {
    "if", "elsif", "else", "endif", "unless", "endunless", "case", "when",
    "endcase", "for", "endfor", "in", "and", "or", "not", "contains",
    "true", "false", "nil", "empty", "assign", "capture", "endcapture",
    "include", "comment", "endcomment", "raw", "endraw",
}
# Font Awesome 图标类，由 CDN 提供，不在本站 CSS
FONTAWESOME = re.compile(r"^fa[srlbd]?$|^fa-")
# 纯数字/比较符等 Liquid 表达式残渣
JUNK = re.compile(r"^[0-9<>=|&+*/%.,:!?\[\]\"'-]+$")


def template_tokens():
    tokens = set()
    files = sorted(p for g in TEMPLATE_GLOBS for p in ROOT.glob(g))
    for path in files:
        text = path.read_text(encoding="utf-8")
        for group in re.findall(r'class="([^"]*)"|class=\'([^\']*)\'', text):
            value = group[0] or group[1]
            # 保留 Liquid 表达式内容（条件类如 {% if x %}mt-4{% endif %}），
            # 只剥掉定界符，控制词/变量残渣在下面过滤
            value = value.replace("{%", " ").replace("%}", " ")
            value = value.replace("{{", " ").replace("}}", " ")
            for token in value.split():
                if (
                    token in LIQUID_KEYWORDS
                    or FONTAWESOME.match(token)
                    or JUNK.match(token)
                    or "." in token  # Liquid 变量残渣，如 nav_sites.size / ov.icon
                    or "{" in token
                    or "}" in token
                    or "%" in token
                ):
                    continue
                tokens.add(token)
    return tokens, len(files)


def allowlisted():
    if not ALLOWLIST.exists():
        return set()
    return {
        line.strip()
        for line in ALLOWLIST.read_text(encoding="utf-8").splitlines()
        if line.strip() and not line.strip().startswith("#")
    }


def main():
    if len(sys.argv) != 2:
        sys.exit(f"用法: {sys.argv[0]} <built.css>")
    css_path = Path(sys.argv[1])
    css = css_path.read_text(encoding="utf-8")
    tokens, file_count = template_tokens()
    allowed = allowlisted()
    missing = sorted(
        t for t in tokens
        if t not in allowed
        and not re.search(r"\." + re.escape(t) + r"(?![\w-])", css)
    )
    if missing:
        print(f"CSS 类校验失败：{len(missing)} 个模版引用的类在 {css_path} 中不存在：")
        for t in missing:
            print(f"  - {t}")
        print("修复：Bootstrap 工具类补进 assets/css/jekyll-theme-chirpy.scss；"
              "站点自定义类加入 tools/css-class-allowlist.txt")
        sys.exit(1)
    print(f"CSS 类校验通过：{file_count} 个模版文件，{len(tokens)} 个类全部存在或已豁免。")


if __name__ == "__main__":
    main()

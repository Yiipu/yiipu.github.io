#!/usr/bin/env bash
# 本地预览固定使用生产环境：与线上相同走 bundle CSS，
# 避免 dev 模式的 CDN 全量 Bootstrap 掩盖裁剪版缺类问题
# 用法: tools/preview.sh
set -euo pipefail
cd "$(git rev-parse --show-toplevel)"
exec env JEKYLL_ENV=production bundle exec jekyll s

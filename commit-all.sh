#!/bin/bash
# 提交脚本 - 一次性提交主仓库和所有子模块

MSG="$1"

if [ -z "$MSG" ]; then
    echo "Usage: ./commit-all.sh \"commit message\""
    exit 1
fi

echo "=== Committing LumeRender ==="
cd LumeRender
git add -A
git commit -m "$MSG" -m "Ultraworked with [Sisyphus](https://github.com/code-yeongyu/oh-my-openagent)" -m "Co-authored-by: Sisyphus <clio-agent@sisyphuslabs.ai>"
git push origin win
cd ..

echo "=== Committing Lume3D ==="
cd Lume3D
git add -A
git commit -m "$MSG" -m "Ultraworked with [Sisyphus](https://github.com/code-yeongyu/oh-my-openagent)" -m "Co-authored-by: Sisyphus <clio-agent@sisyphuslabs.ai>"
git push origin win
cd ..

echo "=== Committing Main Repo ==="
git add -A
git commit -m "$MSG" -m "Ultraworked with [Sisyphus](https://github.com/code-yeongyu/oh-my-openagent)" -m "Co-authored-by: Sisyphus <clio-agent@sisyphuslabs.ai>"
git push origin win

echo "=== Done ==="

#!/usr/bin/env bash
# 增强版内存地址追踪解析脚本
# 功能：遍历 ./mem 目录下 .mem 文件，读取文件内容中“[+]<addr>”中的调用地址，
#       并调用 addr2line 将地址映射到源代码。忽略无符号(??)结果。
#       处理完后，询问是否删除 ./mem 中的 .mem 文件。
# 用法：
#   chmod +x addr2line_mem_script.sh
#   ./addr2line_mem_script.sh <可执行文件路径>

set -euo pipefail  # -e:遇错退出, -u:未声明变量报错, -o pipefail:管道失败也退出

# 帮助信息
show_help() {
    echo "Usage: $0 <path_to_executable>"
    echo "示例：$0 ./socketpool_test"
}

# 参数检查
[[ $# -ne 1 ]] && { show_help >&2; exit 1; }
target_exec="$1"

# 检查 addr2line
command -v addr2line &>/dev/null || { echo "Error: 请安装 binutils 以获得 addr2line" >&2; exit 1; }
# 检查可执行文件
[[ -x "$target_exec" ]] || { echo "Error: '$target_exec' 不可执行或不存在" >&2; exit 1; }
# 检查 ./mem 目录
mem_dir="./mem"
[[ -d "$mem_dir" ]] || { echo "Error: 目录 '$mem_dir' 不存在" >&2; exit 1; }

# 查找 .mem 文件
shopt -s nullglob
files=("$mem_dir"/*.mem)
if [[ ${#files[@]} -eq 0 ]]; then
    echo "提示: 未找到任何 .mem 文件于 '$mem_dir'" >&2
    exit 0
fi

# 处理每个 .mem 文件
for file in "${files[@]}"; do
    # 读取文件第一行
    if ! read -r line < "$file"; then
        echo "Warning: 无法读取 '$file'" >&2
        continue
    fi
    # 正则提取 [+] 后紧跟的 0x... 地址，直到逗号或空白结束
    if [[ $line =~ \[\+\]([[:space:]]*)(0x[0-9A-Fa-f]+) ]]; then
        addr="${BASH_REMATCH[2]}"
    else
        echo "Warning: '$file' 中未找到调用地址" >&2
        continue
    fi
    # 调用 addr2line 获取两行输出：函数名 + 源文件:行号
    mapfile -t info < <(addr2line -e "$target_exec" -f -C "$addr" 2>/dev/null)
    func="${info[0]:-}"
    loc="${info[1]:-}"
    # 忽略无效(entry: ?? and ??:0)
    [[ "$func" == "??" && "$loc" == "??:0" ]] && continue
    # 打印有效结果
    printf "%s -> %s at %s\n" "$addr" "$func" "$loc"
done
shopt -u nullglob

# 询问是否删除 .mem 文件
echo -n "是否删除 '$mem_dir' 目录下的 .mem 文件？ [y/N]: "
read -r answer
case "$answer" in
    [Yy]* )
        rm -v "$mem_dir"/*.mem
        echo "已删除所有 .mem 文件。"
        ;;
    * )
        echo "已跳过删除。"
        ;;
esac

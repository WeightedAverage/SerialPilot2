import os
import requests

# 获取脚本所在目录的父目录（即 STM32 工程根目录）
PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# 官方仓库原始文件地址
BASE_URL = "https://raw.githubusercontent.com/multica-ai/andrej-karpathy-skills/main"
OFFICIAL_FILES = {
    "CLAUDE.md": f"{BASE_URL}/CLAUDE.md",
    "EXAMPLES.md": f"{BASE_URL}/EXAMPLES.md",
    "README.md": f"{BASE_URL}/README.md"
}

# 通用核心文件内容前缀
UNIVERSAL_PREFIX = """# 通用 LLM 编码行为指南（Karpathy 风格）
本指南基于 Andrej Karpathy 对 LLM 编码陷阱的观察，适配所有主流 AI 编码助手：
- ✅ Cursor
- ✅ Trae
- ✅ GitHub Copilot / Codex
- ✅ Windsurf
- ✅ Claude Code
- ✅ 其他支持自定义指令的工具

---

"""

# 各工具规则文件映射（路径相对于工程根目录）
TOOL_RULES = {
    "Cursor": ".cursor/rules/karpathy-guidelines.mdc",
    "Trae": ".trae/rules/karpathy-guidelines.md",
    "GitHub Copilot": ".github/copilot-instructions.md",
    "Windsurf": ".windsurf/rules/karpathy-guidelines.md",
    "Claude Code": "CLAUDE.md"
}

def download_file(url, save_path):
    """下载官方文件"""
    try:
        response = requests.get(url, timeout=10)
        response.raise_for_status()
        with open(save_path, "w", encoding="utf-8") as f:
            f.write(response.text)
        print(f"✅ 下载成功: {save_path}")
        return True
    except Exception as e:
        print(f"❌ 下载失败 {save_path}: {e}")
        return False

def create_universal_core():
    """创建通用核心规则文件"""
    claude_path = os.path.join(PROJECT_ROOT, "CLAUDE.md")
    if not os.path.exists(claude_path):
        print("❌ 核心文件 CLAUDE.md 不存在")
        return False
    
    with open(claude_path, "r", encoding="utf-8") as f:
        official_content = f.read()
    
    universal_content = UNIVERSAL_PREFIX + official_content
    
    core_path = os.path.join(PROJECT_ROOT, "LLM-CODING-GUIDELINES.md")
    with open(core_path, "w", encoding="utf-8") as f:
        f.write(universal_content)
    
    print(f"✅ 生成通用核心文件: {core_path}")
    return True

def generate_all_tool_rules():
    """生成所有工具的规则文件"""
    core_path = os.path.join(PROJECT_ROOT, "LLM-CODING-GUIDELINES.md")
    if not os.path.exists(core_path):
        print("❌ 通用核心文件不存在")
        return False
    
    with open(core_path, "r", encoding="utf-8") as f:
        core_content = f.read()
    
    for tool, path_relative in TOOL_RULES.items():
        path = os.path.join(PROJECT_ROOT, path_relative)
        # 创建目录
        dir_path = os.path.dirname(path)
        if dir_path and not os.path.exists(dir_path):
            os.makedirs(dir_path, exist_ok=True)
        
        # 写入规则内容 + 自动生效配置
        with open(path, "w", encoding="utf-8") as f:
            f.write(core_content)
            if tool in ["Cursor", "Trae", "Windsurf"]:
                f.write("\n\nalwaysApply: true")
        
        print(f"✅ 生成 {tool} 规则文件: {path}")
    
    return True

def main():
    print("=== 开始将官方 Karpathy 指南转换为通用版 ===")
    print(f"📁 工程根目录: {PROJECT_ROOT}")
    
    # 1. 下载官方最新文件到 ai-coding-guidelines 目录
    print("\n1. 下载官方最新文件...")
    for filename, url in OFFICIAL_FILES.items():
        save_path = os.path.join(os.path.dirname(__file__), filename)
        download_file(url, save_path)
    
    # 2. 复制 CLAUDE.md 到工程根目录（用于生成核心文件）
    print("\n2. 准备核心文件...")
    local_claude = os.path.join(os.path.dirname(__file__), "CLAUDE.md")
    root_claude = os.path.join(PROJECT_ROOT, "CLAUDE.md")
    if os.path.exists(local_claude):
        import shutil
        shutil.copy(local_claude, root_claude)
        print(f"✅ 复制 CLAUDE.md 到工程根目录")
    
    # 3. 创建通用核心文件
    print("\n3. 创建通用核心规则文件...")
    if not create_universal_core():
        return
    
    # 4. 生成所有工具的规则文件
    print("\n4. 生成所有 AI 工具的规则文件...")
    if not generate_all_tool_rules():
        return
    
    # 5. 重命名示例文件并移动到 ai-coding-guidelines 目录
    print("\n5. 整理示例文件...")
    local_examples = os.path.join(os.path.dirname(__file__), "EXAMPLES.md")
    target_examples = os.path.join(os.path.dirname(__file__), "LLM-CODING-EXAMPLES.md")
    if os.path.exists(local_examples):
        if os.path.exists(target_examples):
            os.remove(target_examples)
        os.rename(local_examples, target_examples)
        print(f"✅ 重命名示例文件: LLM-CODING-EXAMPLES.md")
    
    # 6. 把工程根目录的通用文件移动回 ai-coding-guidelines
    print("\n6. 整理通用文件...")
    files_to_move = ["LLM-CODING-GUIDELINES.md", "README.md", "CLAUDE.md"]
    for filename in files_to_move:
        src = os.path.join(PROJECT_ROOT, filename)
        dst = os.path.join(os.path.dirname(__file__), filename)
        if os.path.exists(src):
            if os.path.exists(dst):
                os.remove(dst)
            import shutil
            shutil.move(src, dst)
            print(f"✅ 移动 {filename} 到 ai-coding-guidelines")
    
    print("\n=== 转换完成！===")
    print("\n📁 通用文件位置: ai-coding-guidelines/")
    print("🔧 规则文件位置: 工程根目录（自动配置）")
    print("\n💡 以后更新只需在 ai-coding-guidelines/ 目录下运行: python convert-to-universal.py")

if __name__ == "__main__":
    main()

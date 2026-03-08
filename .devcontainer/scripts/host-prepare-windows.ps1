# =============================================================================
# FQCompressor DevContainer - Windows 宿主机环境准备脚本
# =============================================================================
# 检查并安装 Windows 上运行 DevContainer 所需的前置组件
#
# 用法（以管理员权限运行 PowerShell）：
#   Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass
#   .\.devcontainer\scripts\host-prepare-windows.ps1
#
# 支持路径:
#   推荐: WSL2 + Docker Desktop + VS Code
#   备选: Docker Desktop + Git Bash + VS Code (性能差)
# =============================================================================

$ErrorActionPreference = "Stop"

# =============================================================================
# 颜色输出
# =============================================================================
function Write-Step   { param($msg) Write-Host "[STEP] $msg" -ForegroundColor Cyan }
function Write-Ok     { param($msg) Write-Host "  [OK] $msg" -ForegroundColor Green }
function Write-Warn   { param($msg) Write-Host "[WARN] $msg" -ForegroundColor Yellow }
function Write-Err    { param($msg) Write-Host " [ERR] $msg" -ForegroundColor Red }
function Write-Info   { param($msg) Write-Host "[INFO] $msg" -ForegroundColor Gray }

# =============================================================================
# 管理员权限检查
# =============================================================================
function Test-Administrator {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = New-Object Security.Principal.WindowsPrincipal($identity)
    return $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

# =============================================================================
# winget 检查（Windows 包管理器）
# =============================================================================
function Test-Winget {
    try {
        $null = Get-Command winget -ErrorAction Stop
        return $true
    } catch {
        return $false
    }
}

# =============================================================================
# 组件检查函数
# =============================================================================

function Test-WSL {
    try {
        $wslOutput = wsl --status 2>&1
        if ($LASTEXITCODE -eq 0) {
            return $true
        }
        return $false
    } catch {
        return $false
    }
}

function Test-WSLDistro {
    try {
        $distros = wsl --list --quiet 2>&1
        if ($LASTEXITCODE -eq 0 -and $distros) {
            return $true
        }
        return $false
    } catch {
        return $false
    }
}

function Test-DockerDesktop {
    $dockerPath = Get-Command docker -ErrorAction SilentlyContinue
    if ($null -eq $dockerPath) { return $false }

    try {
        $null = docker info 2>&1
        return ($LASTEXITCODE -eq 0)
    } catch {
        return $false
    }
}

function Test-DockerDesktopInstalled {
    return (Test-Path "$env:ProgramFiles\Docker\Docker\Docker Desktop.exe") -or
           (Get-Command docker -ErrorAction SilentlyContinue)
}

function Test-VSCode {
    return [bool](Get-Command code -ErrorAction SilentlyContinue)
}

function Test-GitBash {
    return (Test-Path "$env:ProgramFiles\Git\bin\bash.exe") -or
           (Get-Command bash -ErrorAction SilentlyContinue)
}

function Test-VSCodeExtension {
    param($extensionId)
    try {
        $extensions = code --list-extensions 2>&1
        return ($extensions -contains $extensionId)
    } catch {
        return $false
    }
}

# =============================================================================
# 安装函数
# =============================================================================

function Install-WithWinget {
    param($packageId, $name)
    if (-not (Test-Winget)) {
        Write-Err "winget 不可用，请手动安装: $name"
        Write-Info "  下载: https://aka.ms/getwinget"
        return $false
    }
    Write-Info "  正在通过 winget 安装 $name ..."
    winget install --id $packageId --accept-package-agreements --accept-source-agreements
    return ($LASTEXITCODE -eq 0)
}

# =============================================================================
# 主检查流程
# =============================================================================
function Main {
    Write-Host ""
    Write-Host "========================================" -ForegroundColor Cyan
    Write-Host " FQCompressor Windows 环境准备" -ForegroundColor Cyan
    Write-Host "========================================" -ForegroundColor Cyan
    Write-Host ""

    $isAdmin = Test-Administrator
    $hasWinget = Test-Winget
    $allPassed = $true
    $needsReboot = $false

    if (-not $isAdmin) {
        Write-Warn "未以管理员权限运行，安装操作可能失败"
        Write-Info "建议：右键 PowerShell → 以管理员身份运行"
        Write-Host ""
    }

    # -----------------------------------------------------------------
    # 1. WSL2
    # -----------------------------------------------------------------
    Write-Step "检查 WSL2..."
    if (Test-WSL) {
        Write-Ok "WSL2 已启用"

        if (Test-WSLDistro) {
            try {
                $defaultDistro = (wsl --list --quiet 2>&1 | Select-Object -First 1).Trim()
                Write-Ok "已安装 Linux 发行版: $defaultDistro"
            } catch {
                Write-Ok "已安装 Linux 发行版"
            }
        } else {
            Write-Warn "WSL2 已启用但未安装 Linux 发行版"
            Write-Info "  安装 Ubuntu: wsl --install -d Ubuntu"
            $allPassed = $false
        }
    } else {
        Write-Err "WSL2 未启用"
        if ($isAdmin) {
            Write-Info "  正在启用 WSL..."
            wsl --install --no-distribution
            $needsReboot = $true
            Write-Info "  启用后需要重启电脑，然后运行: wsl --install -d Ubuntu"
        } else {
            Write-Info "  以管理员权限运行: wsl --install"
        }
        $allPassed = $false
    }
    Write-Host ""

    # -----------------------------------------------------------------
    # 2. Docker Desktop
    # -----------------------------------------------------------------
    Write-Step "检查 Docker Desktop..."
    if (Test-DockerDesktopInstalled) {
        Write-Ok "Docker Desktop 已安装"

        if (Test-DockerDesktop) {
            Write-Ok "Docker 正在运行"

            # 检查 WSL2 后端
            try {
                $dockerInfo = docker info 2>&1 | Out-String
                if ($dockerInfo -match "Operating System:.*WSL|Server:.*Desktop") {
                    Write-Ok "Docker Desktop WSL2 后端已启用"
                }
            } catch {}
        } else {
            Write-Warn "Docker Desktop 未运行"
            Write-Info "  请启动 Docker Desktop 应用"
            $allPassed = $false
        }
    } else {
        Write-Err "Docker Desktop 未安装"
        if ($hasWinget) {
            Install-WithWinget "Docker.DockerDesktop" "Docker Desktop"
            $needsReboot = $true
        } else {
            Write-Info "  下载: https://www.docker.com/products/docker-desktop/"
        }
        $allPassed = $false
    }
    Write-Host ""

    # -----------------------------------------------------------------
    # 3. VS Code
    # -----------------------------------------------------------------
    Write-Step "检查 VS Code..."
    if (Test-VSCode) {
        Write-Ok "VS Code 已安装"

        # 检查关键扩展
        $requiredExtensions = @(
            @{ Id = "ms-vscode-remote.remote-wsl";        Name = "Remote - WSL" },
            @{ Id = "ms-vscode-remote.remote-containers"; Name = "Dev Containers" },
            @{ Id = "ms-vscode-remote.remote-ssh";        Name = "Remote - SSH" }
        )

        foreach ($ext in $requiredExtensions) {
            if (Test-VSCodeExtension $ext.Id) {
                Write-Ok "  扩展: $($ext.Name)"
            } else {
                Write-Warn "  缺少扩展: $($ext.Name)"
                Write-Info "    安装: code --install-extension $($ext.Id)"
                $allPassed = $false
            }
        }
    } else {
        Write-Err "VS Code 未安装"
        if ($hasWinget) {
            Install-WithWinget "Microsoft.VisualStudioCode" "VS Code"
        } else {
            Write-Info "  下载: https://code.visualstudio.com/"
        }
        $allPassed = $false
    }
    Write-Host ""

    # -----------------------------------------------------------------
    # 4. Git (含 Git Bash)
    # -----------------------------------------------------------------
    Write-Step "检查 Git..."
    if (Test-GitBash) {
        Write-Ok "Git (含 Git Bash) 已安装"
    } else {
        Write-Warn "Git 未安装（Git Bash 是 initializeCommand 的回退方案）"
        if ($hasWinget) {
            Install-WithWinget "Git.Git" "Git"
        } else {
            Write-Info "  下载: https://git-scm.com/download/win"
        }
        $allPassed = $false
    }
    Write-Host ""

    # -----------------------------------------------------------------
    # 5. WSL 内部环境检查
    # -----------------------------------------------------------------
    if (Test-WSL -and (Test-WSLDistro)) {
        Write-Step "检查 WSL 内部环境..."

        # 检查 WSL 内的 git
        try {
            $wslGit = wsl -- which git 2>&1
            if ($LASTEXITCODE -eq 0) {
                Write-Ok "WSL 内 git 已安装"
            } else {
                Write-Warn "WSL 内未安装 git"
                Write-Info "  在 WSL 中运行: sudo apt update && sudo apt install -y git"
                $allPassed = $false
            }
        } catch {
            Write-Warn "无法检查 WSL 内部环境"
        }

        # 检查 WSL 内的 SSH 密钥
        try {
            $wslSshKeys = wsl -- ls ~/.ssh/id_*.pub 2>&1
            if ($LASTEXITCODE -eq 0) {
                Write-Ok "WSL 内 SSH 密钥已存在"
            } else {
                Write-Warn "WSL 内未找到 SSH 密钥"
                Write-Info "  在 WSL 中运行: ssh-keygen -t ed25519 -C `"your_email@example.com`""
                $allPassed = $false
            }
        } catch {}

        # 检查项目是否在 WSL 文件系统中
        try {
            $wslHome = (wsl -- echo '$HOME' 2>&1).Trim()
            Write-Info "WSL HOME: $wslHome"
            Write-Info "建议将项目克隆到 WSL 内: $wslHome/projects/fq-compressor"
        } catch {}
        Write-Host ""
    }

    # -----------------------------------------------------------------
    # 6. Docker Desktop 设置建议
    # -----------------------------------------------------------------
    if (Test-DockerDesktopInstalled) {
        Write-Step "Docker Desktop 推荐配置..."
        Write-Info "请确认以下设置已启用（Docker Desktop → Settings）："
        Write-Info "  General:"
        Write-Info "    [✓] Use the WSL 2 based engine"
        Write-Info "  Resources → WSL Integration:"
        Write-Info "    [✓] Enable integration with my default WSL distro"
        Write-Info "  Resources → Advanced (建议):"
        Write-Info "    Memory: >= 8 GB (C++ 编译需要较多内存)"
        Write-Info "    CPUs:   >= 4"
        Write-Host ""
    }

    # -----------------------------------------------------------------
    # 总结
    # -----------------------------------------------------------------
    Write-Host "========================================" -ForegroundColor Cyan
    if ($needsReboot) {
        Write-Warn "部分组件需要重启电脑后生效"
        Write-Info "重启后请重新运行此脚本验证"
    } elseif ($allPassed) {
        Write-Host " 所有检查通过！" -ForegroundColor Green
        Write-Host ""
        Write-Host " 下一步:" -ForegroundColor Cyan
        Write-Host "   1. 在 WSL 终端中克隆项目到 WSL 文件系统" -ForegroundColor White
        Write-Host "      git clone <repo> ~/projects/fq-compressor" -ForegroundColor Gray
        Write-Host "   2. 准备环境配置" -ForegroundColor White
        Write-Host "      cd ~/projects/fq-compressor" -ForegroundColor Gray
        Write-Host "      cp docker/.env.example docker/.env" -ForegroundColor Gray
        Write-Host "   3. 用 VS Code 打开并进入容器" -ForegroundColor White
        Write-Host "      code ." -ForegroundColor Gray
        Write-Host "      F1 → Dev Containers: Reopen in Container" -ForegroundColor Gray
    } else {
        Write-Warn "部分检查未通过，请按上方提示安装缺失组件"
    }
    Write-Host "========================================" -ForegroundColor Cyan
    Write-Host ""
}

# 运行
Main

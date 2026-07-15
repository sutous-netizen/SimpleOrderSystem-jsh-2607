# Release 통합 시나리오 러너.
# Release 빌드는 설계상(Phase 3) 테스트 프레임워크를 전혀 포함하지 않으므로,
# 이 스크립트가 실제로 빌드된 SimpleOrderSystem.exe(Release)를 표준입력으로 구동하고
# 출력에 기대 문구가 모두 포함되는지 검사하는 외부 E2E 테스트 러너 역할을 한다.
#
# 사용법:
#   powershell -File run-scenarios.ps1 [-ExePath <경로>] [-ScenarioDir <경로>]
#
# 시나리오 파일 형식(scenarios/<이름>.txt, scenarios/<이름>.expect.txt):
#   .txt 첫 줄  : "#ARGS" 또는 "#ARGS --seed" 형태로 실행 인자를 지정한다.
#   .txt 나머지 : 표준입력으로 그대로 주입할 메뉴 입력 시퀀스(한 줄에 하나).
#   .expect.txt : 시나리오 성공 시 출력에 반드시 포함되어야 할 부분 문자열(한 줄에 하나, 순서 무관).
#
# 구현 참고: PowerShell의 네이티브 파이프(`$lines | & $exe`)는 첫 줄에 UTF-8 BOM을
# 끼워넣는 문제가 있어(Windows PowerShell 5.1), 자식 프로세스가 첫 입력을 깨진 문자열로
# 읽는 현상이 발생했다. 이를 피하기 위해 표준입력을 BOM 없는 UTF-8 임시 파일로 만들어
# cmd.exe의 `<` 리다이렉션으로 주입한다(이 방식은 직접 검증했다).
#
# 각 시나리오는 격리된 임시 작업 디렉터리에서 실행되어(data/ 폴더 충돌 방지),
# 실행 후 정리된다.

param(
    [string]$ExePath = (Join-Path $PSScriptRoot "..\..\..\..\x64\Release\SimpleOrderSystem.exe"),
    [string]$ScenarioDir = (Join-Path $PSScriptRoot "scenarios")
)

# 자식 프로세스의 출력을 UTF-8로 디코딩한다.
# (main.cpp가 SetConsoleOutputCP(CP_UTF8)로 콘솔 출력은 UTF-8을 보장하지만,
#  파이프로 캡처할 때는 PowerShell이 별도로 디코딩 인코딩을 맞춰줘야 한글이 깨지지 않는다.)
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8

$NoBomUtf8 = New-Object System.Text.UTF8Encoding $false

$ExePath = (Resolve-Path -Path $ExePath -ErrorAction Stop).Path
$ScenarioDir = (Resolve-Path -Path $ScenarioDir -ErrorAction Stop).Path

Write-Host "실행 파일: $ExePath"
Write-Host "시나리오 폴더: $ScenarioDir"
Write-Host ""

$txtFiles = Get-ChildItem -Path $ScenarioDir -Filter "*.txt" |
    Where-Object { $_.Name -notlike "*.expect.txt" } |
    Sort-Object Name

$totalCount = 0
$passCount = 0
$failedScenarios = @()

foreach ($txtFile in $txtFiles) {
    $totalCount++
    $baseName = $txtFile.BaseName
    $expectPath = Join-Path $ScenarioDir "$baseName.expect.txt"

    if (-not (Test-Path $expectPath)) {
        Write-Host "[SKIP] $baseName - 대응하는 .expect.txt 가 없습니다."
        continue
    }

    $lines = Get-Content -Path $txtFile.FullName -Encoding UTF8
    if ($lines.Length -lt 1) {
        Write-Host "[FAIL] $baseName - 입력 파일이 비어 있습니다(#ARGS 줄 필요)."
        $failedScenarios += $baseName
        continue
    }

    $argsLine = $lines[0]
    if ($lines.Length -gt 1) {
        $stdinLines = $lines[1..($lines.Length - 1)]
    } else {
        $stdinLines = @()
    }

    $exeArgs = ""
    if ($argsLine -match '^#ARGS\s*(.*)$') {
        $exeArgs = $matches[1].Trim()
    } else {
        Write-Host "[FAIL] $baseName - 첫 줄이 '#ARGS'로 시작하지 않습니다."
        $failedScenarios += $baseName
        continue
    }

    $expectLines = Get-Content -Path $expectPath -Encoding UTF8 |
        Where-Object { $_.Trim().Length -gt 0 }

    # 격리된 임시 작업 디렉터리에서 실행(data/ 폴더가 실제 저장소나 다른 시나리오와 섞이지 않도록).
    $workDir = Join-Path ([System.IO.Path]::GetTempPath()) ("sos_scenario_" + [System.Guid]::NewGuid().ToString("N"))
    New-Item -ItemType Directory -Path $workDir | Out-Null

    try {
        # 표준입력을 BOM 없는 UTF-8 파일로 만들어 cmd.exe의 `<` 리다이렉션으로 주입한다.
        $stdinFile = Join-Path $workDir "stdin.txt"
        $stdinText = ($stdinLines -join "`n") + "`n"
        [System.IO.File]::WriteAllText($stdinFile, $stdinText, $NoBomUtf8)

        Push-Location $workDir
        try {
            $cmdLine = "`"$ExePath`" $exeArgs < `"$stdinFile`" 2>&1"
            $output = (cmd /c $cmdLine | Out-String)
        } finally {
            Pop-Location
        }

        $missing = @()
        foreach ($expectLine in $expectLines) {
            if (-not $output.Contains($expectLine)) {
                $missing += $expectLine
            }
        }

        if ($missing.Count -eq 0) {
            Write-Host "[PASS] $baseName"
            $passCount++
        } else {
            Write-Host "[FAIL] $baseName - 출력에서 찾지 못한 기대 문구:"
            foreach ($m in $missing) {
                Write-Host "    - $m"
            }
            Write-Host "  ---- 실제 출력 ----"
            Write-Host $output
            Write-Host "  -------------------"
            $failedScenarios += $baseName
        }
    } finally {
        Remove-Item -Path $workDir -Recurse -Force -ErrorAction SilentlyContinue
    }
}

Write-Host ""
Write-Host "==== 결과: $passCount/$totalCount 시나리오 통과 ===="

if ($failedScenarios.Count -gt 0) {
    Write-Host "실패한 시나리오: $($failedScenarios -join ', ')"
    exit 1
}

exit 0

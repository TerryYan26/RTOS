# STM32L475E-IoT01A1 Power Measurement and Analysis Tool
# 
# Features:
# - Monitor system status via serial port
# - Integrate external power measurement devices
# - Generate power reports and optimization recommendations
# 
# Author: Your Name
# Version: V1.0.0
# Date: 2025-11-07

param(
    [string]$SerialPort = "COM3",
    [int]$BaudRate = 115200,
    [int]$TestDuration = 300,
    [switch]$Verbose,
    [string]$OutputFile = "power_measurement_report.txt"
)

# Global Variables
$script:TestResults = @{
    Samples = @()
    Errors = 0
    StartTime = $null
    Duration = 0
}

# Simulated power measurement data (connect real measurement device in actual use)
$script:PowerModes = @{
    Active = @{ Current_mA = 15.2; Voltage_V = 3.3 }
    Idle = @{ Current_mA = 8.7; Voltage_V = 3.3 }
    Sleep = @{ Current_mA = 2.1; Voltage_V = 3.3 }
    DeepSleep = @{ Current_mA = 0.45; Voltage_V = 3.3 }
}

function Write-Log {
    param([string]$Message, [string]$Level = "INFO")
    
    $timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
    $logEntry = "[$timestamp] [$Level] $Message"
    
    switch ($Level) {
        "ERROR" { Write-Host $logEntry -ForegroundColor Red }
        "WARN"  { Write-Host $logEntry -ForegroundColor Yellow }
        "SUCCESS" { Write-Host $logEntry -ForegroundColor Green }
        default { Write-Host $logEntry }
    }
    
    # Also write to log file
    Add-Content -Path "power_test.log" -Value $logEntry
}

function Test-SerialConnection {
    param([string]$Port, [int]$Baud)
    
    try {
        $serial = New-Object System.IO.Ports.SerialPort $Port, $Baud
        $serial.ReadTimeout = 1000
        $serial.WriteTimeout = 1000
        $serial.Open()
        
        Write-Log "Serial connection successful: $Port @ $Baud baud" "SUCCESS"
        
        # Test communication
        $serial.WriteLine("AT+INFO")
        Start-Sleep -Milliseconds 100
        
        if ($serial.BytesToRead -gt 0) {
            $response = $serial.ReadExisting()
            Write-Log "Device response: $response" "SUCCESS"
        }
        
        return $serial
    }
    catch {
        Write-Log "Serial connection failed: $_" "ERROR"
        return $null
    }
}

function Start-PowerMeasurement {
    param([System.IO.Ports.SerialPort]$SerialConnection, [int]$Duration)
    
    Write-Log "Starting power measurement, duration: $Duration seconds"
    $script:TestResults.StartTime = Get-Date
    
    $endTime = (Get-Date).AddSeconds($Duration)
    $sampleCount = 0
    
    while ((Get-Date) -lt $endTime) {
        try {
            # Read serial port data
            if ($SerialConnection -and $SerialConnection.BytesToRead -gt 0) {
                $data = $SerialConnection.ReadLine()
                Parse-SystemData -Data $data
            }
            
            # Simulate power measurement (replace with real measurement device API in actual use)
            $currentSample = Measure-SimulatedPower
            $script:TestResults.Samples += $currentSample
            
            $sampleCount++
            
            # Report progress every 10 seconds
            if ($sampleCount % 100 -eq 0) {
                $elapsed = ((Get-Date) - $script:TestResults.StartTime).TotalSeconds
                $progress = ($elapsed / $Duration) * 100
                Write-Log "Measurement progress: $([math]::Round($progress, 1))% (samples: $sampleCount)"
            }
            
            if ($Verbose -and ($sampleCount % 50 -eq 0)) {
                Write-Log "Current power: $($currentSample.Power_mW) mW (mode: $($currentSample.Mode))"
            }
            
            Start-Sleep -Milliseconds 100
        }
        catch {
            $script:TestResults.Errors++
            Write-Log "Measurement error: $_" "ERROR"
        }
    }
    
    $script:TestResults.Duration = ((Get-Date) - $script:TestResults.StartTime).TotalSeconds
    Write-Log "Power measurement completed, total samples: $($script:TestResults.Samples.Count)" "SUCCESS"
}

function Parse-SystemData {
    param([string]$Data)
    
    if ($Data -match "System State: (\w+)") {
        $systemState = $Matches[1]
        
        # Infer power mode based on system state
        $powerMode = switch ($systemState) {
            "ACTIVE" { "Active" }
            "IDLE" { "Idle" }
            "SLEEP" { "Sleep" }
            "DEEP_SLEEP" { "DeepSleep" }
            default { "Active" }
        }
        
        if ($Verbose) {
            Write-Log "System state: $systemState -> Power mode: $powerMode"
        }
    }
    
    # Parse other system information
    if ($Data -match "CPU: (\d+)%") {
        $cpuUsage = [int]$Matches[1]
        if ($Verbose) {
            Write-Log "CPU usage: $cpuUsage%"
        }
    }
    
    if ($Data -match "Free Heap: (\d+)") {
        $freeHeap = [int]$Matches[1]
        if ($Verbose) {
            Write-Log "Free memory: $freeHeap bytes"
        }
    }
}

function Measure-SimulatedPower {
    # Simulate power measurement - replace with real measurement device API in actual use
    # For example: Keysight N6705C, Rohde & Schwarz RT-ZVC, or other power analyzers
    
    # Randomly select a power mode (should be based on system state in actual use)
    $modes = @("Active", "Idle", "Sleep")
    $weights = @(0.6, 0.3, 0.1)  # Active 60%, Idle 30%, Sleep 10%
    
    $random = Get-Random -Maximum 1.0
    $selectedMode = "Active"
    
    $cumulative = 0
    for ($i = 0; $i -lt $modes.Length; $i++) {
        $cumulative += $weights[$i]
        if ($random -le $cumulative) {
            $selectedMode = $modes[$i]
            break
        }
    }
    
    $modeData = $script:PowerModes[$selectedMode]
    $baseCurrent = $modeData.Current_mA
    
    # Add some random variation (±5%)
    $variation = (Get-Random -Maximum 0.1) - 0.05
    $actualCurrent = $baseCurrent * (1 + $variation)
    
    $voltage = $modeData.Voltage_V
    $power = $actualCurrent * $voltage  # mW
    
    return @{
        Timestamp = Get-Date
        Mode = $selectedMode
        Current_mA = [math]::Round($actualCurrent, 3)
        Voltage_V = $voltage
        Power_mW = [math]::Round($power, 3)
    }
}

function Analyze-PowerResults {
    $samples = $script:TestResults.Samples
    
    if ($samples.Count -eq 0) {
        Write-Log "No power data available for analysis" "ERROR"
        return
    }
    
    Write-Log "=== Power Analysis Results ===" "SUCCESS"
    
    # Group statistics by mode
    $modeStats = @{}
    foreach ($mode in @("Active", "Idle", "Sleep", "DeepSleep")) {
        $modeSamples = $samples | Where-Object { $_.Mode -eq $mode }
        
        if ($modeSamples.Count -gt 0) {
            $avgPower = ($modeSamples | Measure-Object -Property Power_mW -Average).Average
            $maxPower = ($modeSamples | Measure-Object -Property Power_mW -Maximum).Maximum
            $minPower = ($modeSamples | Measure-Object -Property Power_mW -Minimum).Minimum
            $percentage = ($modeSamples.Count / $samples.Count) * 100
            
            $modeStats[$mode] = @{
                Count = $modeSamples.Count
                Percentage = [math]::Round($percentage, 1)
                AvgPower_mW = [math]::Round($avgPower, 3)
                MaxPower_mW = [math]::Round($maxPower, 3)
                MinPower_mW = [math]::Round($minPower, 3)
            }
            
            Write-Log "$mode mode: $($modeStats[$mode].Percentage)% time, average power $($modeStats[$mode].AvgPower_mW) mW"
        }
    }
    
    # Overall statistics
    $totalAvgPower = ($samples | Measure-Object -Property Power_mW -Average).Average
    $totalMaxPower = ($samples | Measure-Object -Property Power_mW -Maximum).Maximum
    $totalMinPower = ($samples | Measure-Object -Property Power_mW -Minimum).Minimum
    
    Write-Log "Overall average power: $([math]::Round($totalAvgPower, 3)) mW"
    Write-Log "Maximum power: $([math]::Round($totalMaxPower, 3)) mW"
    Write-Log "Minimum power: $([math]::Round($totalMinPower, 3)) mW"
    
    # Estimate battery life (assuming 3.7V 2000mAh lithium battery)
    $batteryCapacity_mAh = 2000
    $batteryVoltage_V = 3.7
    $batteryEnergy_mWh = $batteryCapacity_mAh * $batteryVoltage_V
    
    $estimatedLifetime_hours = $batteryEnergy_mWh / $totalAvgPower
    
    Write-Log "Estimated battery life (2000mAh): $([math]::Round($estimatedLifetime_hours, 1)) hours ($([math]::Round($estimatedLifetime_hours/24, 1)) days)"
    
    return @{
        ModeStats = $modeStats
        TotalStats = @{
            AvgPower_mW = [math]::Round($totalAvgPower, 3)
            MaxPower_mW = [math]::Round($totalMaxPower, 3)
            MinPower_mW = [math]::Round($totalMinPower, 3)
            EstimatedLifetime_hours = [math]::Round($estimatedLifetime_hours, 1)
        }
    }
}

function Generate-PowerReport {
    param([string]$OutputPath, [object]$AnalysisResults)
    
    $report = @"
STM32L475E-IoT01A1 Power Measurement Report
============================================

Test Time: $(Get-Date -Format "yyyy-MM-dd HH:mm:ss")
Test Duration: $([math]::Round($script:TestResults.Duration, 1)) seconds
Sample Count: $($script:TestResults.Samples.Count)
Error Count: $($script:TestResults.Errors)

Overall Power Statistics:
- Average Power: $($AnalysisResults.TotalStats.AvgPower_mW) mW
- Maximum Power: $($AnalysisResults.TotalStats.MaxPower_mW) mW  
- Minimum Power: $($AnalysisResults.TotalStats.MinPower_mW) mW
- Estimated Battery Life: $($AnalysisResults.TotalStats.EstimatedLifetime_hours) hours

Power Analysis by Mode:
"@

    foreach ($mode in $AnalysisResults.ModeStats.Keys) {
        $stats = $AnalysisResults.ModeStats[$mode]
        $report += @"

$mode Mode:
- Time Percentage: $($stats.Percentage)%
- Sample Count: $($stats.Count)
- Average Power: $($stats.AvgPower_mW) mW
- Maximum Power: $($stats.MaxPower_mW) mW
- Minimum Power: $($stats.MinPower_mW) mW
"@
    }
    
    $report += @"

Power Optimization Recommendations:
1. Increase usage time of idle and sleep modes
2. Optimize task scheduling to reduce Active mode time
3. Use Tickless IDLE and deep sleep
4. Optimize peripheral clock configuration
5. Consider using DMA to reduce CPU load

Test Configuration:
- Serial Port: $SerialPort @ $BaudRate baud
- Sampling Interval: 100ms
- Battery Assumption: 2000mAh @ 3.7V

Note: This report uses simulated data. Use professional power analysis equipment for actual deployment.

Report Generation Time: $(Get-Date -Format "yyyy-MM-dd HH:mm:ss")
"@

    try {
        Set-Content -Path $OutputPath -Value $report -Encoding UTF8
        Write-Log "Power report saved: $OutputPath" "SUCCESS"
    }
    catch {
        Write-Log "Failed to save report: $_" "ERROR"
    }
}

function Export-PowerData {
    param([string]$CsvPath)
    
    try {
        $csvData = $script:TestResults.Samples | Select-Object @{n='Timestamp';e={$_.Timestamp.ToString("yyyy-MM-dd HH:mm:ss.fff")}}, Mode, Current_mA, Voltage_V, Power_mW
        
        $csvData | Export-Csv -Path $CsvPath -NoTypeInformation -Encoding UTF8
        Write-Log "Power data exported to CSV: $CsvPath" "SUCCESS"
    }
    catch {
        Write-Log "CSV export failed: $_" "ERROR"
    }
}

function Show-PowerVisualization {
    # Generate simple PowerShell charts (text mode)
    Write-Log "=== Power Visualization ==="
    
    $modeColors = @{
        Active = "Red"
        Idle = "Yellow" 
        Sleep = "Green"
        DeepSleep = "Blue"
    }
    
    # Display time distribution for each mode
    $totalSamples = $script:TestResults.Samples.Count
    Write-Host "`nMode Time Distribution:" -ForegroundColor Cyan
    
    foreach ($mode in @("Active", "Idle", "Sleep", "DeepSleep")) {
        $modeSamples = ($script:TestResults.Samples | Where-Object { $_.Mode -eq $mode }).Count
        $percentage = if ($totalSamples -gt 0) { ($modeSamples / $totalSamples) * 100 } else { 0 }
        
        $barLength = [math]::Floor($percentage / 2)  # One character per 2%
        $bar = "█" * $barLength
        
        $color = $modeColors[$mode]
        Write-Host ("{0,-10}: [{1,-50}] {2:F1}%" -f $mode, $bar, $percentage) -ForegroundColor $color
    }
    
    # Display power trend (last 20 samples)
    Write-Host "`nRecent Power Trend (mW):" -ForegroundColor Cyan
    $recentSamples = $script:TestResults.Samples | Select-Object -Last 20
    
    if ($recentSamples.Count -gt 0) {
        $maxPower = ($recentSamples | Measure-Object -Property Power_mW -Maximum).Maximum
        
        for ($i = 0; $i -lt $recentSamples.Count; $i++) {
            $sample = $recentSamples[$i]
            $normalizedHeight = [math]::Floor(($sample.Power_mW / $maxPower) * 10)
            $bar = "▌" * $normalizedHeight
            
            $color = $modeColors[$sample.Mode]
            Write-Host ("{0,2}: [{1,-10}] {2:F1}mW ({3})" -f ($i+1), $bar, $sample.Power_mW, $sample.Mode) -ForegroundColor $color
        }
    }
}

# Main Program Execution
function Main {
    Write-Log "STM32L475E-IoT01A1 Power Measurement Tool Starting" "SUCCESS"
    Write-Log "Configuration: Port=$SerialPort, Baud Rate=$BaudRate, Test Duration=$TestDuration seconds"
    
    # Test serial connection
    $serialConnection = Test-SerialConnection -Port $SerialPort -Baud $BaudRate
    
    if ($serialConnection) {
        Write-Log "Preparing to start power measurement..."
        Read-Host "Press Enter to start test"
    } else {
        Write-Log "Will use simulated data for testing..." "WARN"
        $serialConnection = $null
    }
    
    try {
        # Start power measurement
        Start-PowerMeasurement -SerialConnection $serialConnection -Duration $TestDuration
        
        # Analyze results
        $analysisResults = Analyze-PowerResults
        
        # Show visualization
        Show-PowerVisualization
        
        # Generate report
        Generate-PowerReport -OutputPath $OutputFile -AnalysisResults $analysisResults
        
        # Export CSV data
        $csvFile = $OutputFile -replace "\.txt$", ".csv"
        Export-PowerData -CsvPath $csvFile
        
        Write-Log "Power test completed! Result file: $OutputFile" "SUCCESS"
        
    }
    catch {
        Write-Log "Error occurred during testing: $_" "ERROR"
    }
    finally {
        # Clean up resources
        if ($serialConnection -and $serialConnection.IsOpen) {
            $serialConnection.Close()
            Write-Log "Serial connection closed"
        }
    }
}

# Check PowerShell version
if ($PSVersionTable.PSVersion.Major -lt 5) {
    Write-Warning "This script requires PowerShell 5.0 or higher"
    exit 1
}

# Run main program
Main
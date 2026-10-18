param([Parameter(Mandatory=$true)][string]$TestExecutable)
$ErrorActionPreference = 'Stop'
Add-Type -Path (Join-Path $PSScriptRoot 'LocalTlsServer.cs')
$server = [PiwikLocalTlsServer]::new()
$previousPort = $env:PIWIK_TEST_TLS_PORT
try {
    $env:PIWIK_TEST_TLS_PORT = [string]$server.Port
    & $TestExecutable
    if ($LASTEXITCODE -ne 0) {
        if ($server.LastError) { Write-Output $server.LastError }
        throw "Live TLS tests failed with exit code $LASTEXITCODE"
    }
}
finally {
    $env:PIWIK_TEST_TLS_PORT = $previousPort
    $server.Dispose()
}

@echo off
echo ============================================================================
echo SUDOKU HPC - TEST REGRESYJNY - KOMPILACJA
echo ============================================================================
echo.

echo [1/4] Kompilacja glownych testow regresji...
g++ -std=c++20 -O2 -pthread -I.. main_test.cpp -o sudoku_test.exe
if %errorlevel% neq 0 (
    echo BLAD: Kompilacja sudoku_test.exe nie powiodla sie!
    goto :error
)
echo OK: sudoku_test.exe
echo.

echo [2/4] Kompilacja testow poziomu 1 (asymetryczne)...
g++ -std=c++20 -O2 -pthread -I.. main_test_level1.cpp -o sudoku_test_level1.exe
if %errorlevel% neq 0 (
    echo BLAD: Kompilacja sudoku_test_level1.exe nie powiodla sie!
    goto :error
)
echo OK: sudoku_test_level1.exe
echo.

echo [3/4] Kompilacja testow poziomu 1 (9x9)...
g++ -std=c++20 -O2 -pthread -I.. main_test_level1_9x9.cpp -o sudoku_test_level1_9x9.exe
if %errorlevel% neq 0 (
    echo BLAD: Kompilacja sudoku_test_level1_9x9.exe nie powiodla sie!
    goto :error
)
echo OK: sudoku_test_level1_9x9.exe
echo.

echo [4/4] Kompilacja testu katalogu geometrii Level 1 (5 min)...
g++ -std=c++20 -O2 -pthread -I.. main_test_level1_generate_catalog_5min.cpp -o sudoku_test_level1_generate_catalog_5min.exe -lpsapi -ladvapi32
if %errorlevel% neq 0 (
    echo BLAD: Kompilacja sudoku_test_level1_generate_catalog_5min.exe nie powiodla sie!
    goto :error
)
echo OK: sudoku_test_level1_generate_catalog_5min.exe
echo.

echo ============================================================================
echo KOMPILACJA UKONCZONA POMYSLNIE
echo ============================================================================
echo.
echo Uruchom:
echo   sudoku_test.exe         - podstawowe testy regresji
echo   sudoku_test_level1.exe  - testy poziomu 1 z geometriami asymetrycznymi
echo   sudoku_test_level1_9x9.exe - testy poziomu 1 dla 9x9 (3x3)
echo   sudoku_test_level1_generate_catalog_5min.exe - 5-min checklist 4x4..36x36
echo.
goto :end

:error
echo.
echo ============================================================================
echo BLAD KOMPILACJI
echo ============================================================================
exit /b 1

:end
pause

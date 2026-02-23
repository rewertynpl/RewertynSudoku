@echo off
echo ============================================================================
echo SUDOKU HPC - TEST REGRESYJNY - KOMPILACJA
echo ============================================================================
echo.

echo [1/9] Kompilacja glownych testow regresji...
g++ -std=c++20 -O2 -pthread -I.. main_test.cpp -o sudoku_test.exe
if %errorlevel% neq 0 (
    echo BLAD: Kompilacja sudoku_test.exe nie powiodla sie!
    goto :error
)
echo OK: sudoku_test.exe
echo.

echo [2/9] Kompilacja testow poziomu 1 (asymetryczne)...
g++ -std=c++20 -O2 -pthread -I.. main_test_level1.cpp -o sudoku_test_level1.exe
if %errorlevel% neq 0 (
    echo BLAD: Kompilacja sudoku_test_level1.exe nie powiodla sie!
    goto :error
)
echo OK: sudoku_test_level1.exe
echo.

echo [3/9] Kompilacja testow poziomu 1 (9x9)...
g++ -std=c++20 -O2 -pthread -I.. main_test_level1_9x9.cpp -o sudoku_test_level1_9x9.exe
if %errorlevel% neq 0 (
    echo BLAD: Kompilacja sudoku_test_level1_9x9.exe nie powiodla sie!
    goto :error
)
echo OK: sudoku_test_level1_9x9.exe
echo.

echo [4/9] Kompilacja testu katalogu geometrii Level 1 (5 min)...
g++ -std=c++20 -O2 -pthread -I.. main_test_level1_generate_catalog_5min.cpp -o sudoku_test_level1_generate_catalog_5min.exe -lpsapi -ladvapi32
if %errorlevel% neq 0 (
    echo BLAD: Kompilacja sudoku_test_level1_generate_catalog_5min.exe nie powiodla sie!
    goto :error
)
echo OK: sudoku_test_level1_generate_catalog_5min.exe
echo.

echo [5/9] Kompilacja testow required-strategy L4...
g++ -std=c++20 -O2 -pthread -I.. main_test_level4_required.cpp -o sudoku_test_level4_required.exe -lpsapi -ladvapi32
if %errorlevel% neq 0 (
    echo BLAD: Kompilacja sudoku_test_level4_required.exe nie powiodla sie!
    goto :error
)
echo OK: sudoku_test_level4_required.exe
echo.

echo [6/9] Kompilacja testow required-strategy L5...
g++ -std=c++20 -O2 -pthread -I.. main_test_level5_required.cpp -o sudoku_test_level5_required.exe -lpsapi -ladvapi32
if %errorlevel% neq 0 (
    echo BLAD: Kompilacja sudoku_test_level5_required.exe nie powiodla sie!
    goto :error
)
echo OK: sudoku_test_level5_required.exe
echo.

echo [7/9] Kompilacja testow required-strategy L6...
g++ -std=c++20 -O2 -pthread -I.. main_test_level6_required.cpp -o sudoku_test_level6_required.exe -lpsapi -ladvapi32
if %errorlevel% neq 0 (
    echo BLAD: Kompilacja sudoku_test_level6_required.exe nie powiodla sie!
    goto :error
)
echo OK: sudoku_test_level6_required.exe
echo.

echo [8/9] Kompilacja testow required-strategy L7...
g++ -std=c++20 -O2 -pthread -I.. main_test_level7_required.cpp -o sudoku_test_level7_required.exe -lpsapi -ladvapi32
if %errorlevel% neq 0 (
    echo BLAD: Kompilacja sudoku_test_level7_required.exe nie powiodla sie!
    goto :error
)
echo OK: sudoku_test_level7_required.exe
echo.

echo [9/9] Kompilacja testow required-strategy L8...
g++ -std=c++20 -O2 -pthread -I.. main_test_level8_required.cpp -o sudoku_test_level8_required.exe -lpsapi -ladvapi32
if %errorlevel% neq 0 (
    echo BLAD: Kompilacja sudoku_test_level8_required.exe nie powiodla sie!
    goto :error
)
echo OK: sudoku_test_level8_required.exe
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
echo   sudoku_test_level4_required.exe - L4 required-strategy (target=2, timeout globalny)
echo   sudoku_test_level5_required.exe - L5 required-strategy (9x9/12x12, timeout globalny)
echo   sudoku_test_level6_required.exe - L6 required-strategy (9x9/12x12, timeout globalny)
echo   sudoku_test_level7_required.exe - L7 required-strategy (9x9/12x12, timeout globalny)
echo   sudoku_test_level8_required.exe - L8 required-strategy (9x9/12x12, timeout globalny)
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

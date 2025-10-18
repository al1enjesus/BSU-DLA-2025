# 1. Сохраняем полные логи
echo "=== COLLECTING LOGS ==="
LD_PRELOAD=./wsl_spy.so gcc hello.c -o hello 2> gcc_full.log
LD_PRELOAD=./wsl_spy.so make hello 2> make_full.log
LD_PRELOAD=./wsl_spy.so as test.s -o test.o 2> as_full.log

# 2. Анализируем статистику
echo "=== COMPLETE STATISTICS ==="
echo "GCC total calls: $(grep "\[SPY\]" gcc_full.log | wc -l)"
echo "MAKE total calls: $(grep "\[SPY\]" make_full.log | wc -l)"
echo "AS total calls: $(grep "\[SPY\]" as_full.log | wc -l)"

echo "=== CALL TYPE BREAKDOWN ==="
echo "GCC:"
grep "\[SPY\]" gcc_full.log | cut -d' ' -f2 | sort | uniq -c | sort -nr
echo ""
echo "MAKE:"
grep "\[SPY\]" make_full.log | cut -d' ' -f2 | sort | uniq -c | sort -nr
echo ""
echo "AS:"
grep "\[SPY\]" as_full.log | cut -d' ' -f2 | sort | uniq -c | sort -nr

# 3. Тест статической компиляции
echo "=== STATIC COMPILATION TEST ==="
gcc -static -o hello_static hello.c
echo "Testing static binary:"
LD_PRELOAD=./wsl_spy.so ./hello_static 2>&1 | head -5
echo "Number of spy calls for static binary: $(LD_PRELOAD=./wsl_spy.so ./hello_static 2>&1 | grep -c "\[SPY\]")"

# 4. Показываем примеры вызовов из логов
echo "=== SAMPLE CALLS FROM LOGS ==="
echo "GCC first 10 calls:"
grep "\[SPY\]" gcc_full.log | head -10
echo ""
echo "MAKE first 10 calls:"
grep "\[SPY\]" make_full.log | head -10
echo ""
echo "AS first 10 calls:"
grep "\[SPY\]" as_full.log | head -10
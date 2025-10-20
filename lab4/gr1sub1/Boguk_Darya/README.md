# Лабораторная 4 — Системные вызовы
## Воспроизводимость  
### Сборка

Для сборки всех компонентов:

```bash
make
```

### Задание A - Перехват системных вызовов через LD_PRELOAD
Тестирование на различных утилитах
```bash
LD_PRELOAD=./libsyscall_spy.so find . -name "*.txt"
LD_PRELOAD=./libsyscall_spy.so cp file1.txt file1_copy.txt
LD_PRELOAD=./libsyscall_spy.so tar -cf archive.tar *.txt
```

### Эксперимент со статической программой

```bash
make test
```


### Задание B - Измерение производительности системных вызовов

```bash
./benchmark
```

### Очистка

Для удаления сгенерированных файлов:

```bash
make clean
```
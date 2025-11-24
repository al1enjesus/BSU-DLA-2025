import time
import resource

# Установим ограничение на виртуальную память (например, 500 MB)
limit_mb = 500
limit_bytes = limit_mb * 1024 * 1024
resource.setrlimit(resource.RLIMIT_AS, (limit_bytes, limit_bytes))

print(f"[mem_touch] RLIMIT_AS установлен: {limit_mb} MB")

# Постепенно выделяем и заполняем память
blocks = []
step_mb = 10
step_bytes = step_mb * 1024 * 1024

try:
    for i in range(100):
        block = bytearray(step_bytes)
        for j in range(0, step_bytes, 4096):
            block[j] = 1  # трогаем каждую страницу
        blocks.append(block)
        print(f"[mem_touch] Выделено: {(i+1)*step_mb} MB")
        time.sleep(1)
except MemoryError:
    print("[mem_touch] MemoryError: достигнут предел RLIMIT_AS")
except Exception as e:
    print(f"[mem_touch] Ошибка: {e}")

print("[mem_touch] Завершение")
time.sleep(60)  # держим процесс активным


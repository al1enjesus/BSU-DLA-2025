import time

data = []

try:
    while True:
        data.append(bytearray(10**7))  # выделяем 10 МБ за раз
        print("Allocated", len(data)*10, "MB")
        time.sleep(1)
except MemoryError:
    print("MemoryError caught!")

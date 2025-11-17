import os
import sys

def read_file(path):
    """Безопасное чтение файла — возвращает строку или сообщение об ошибке."""
    try:
        with open(path, "r") as f:
            return f.read().strip()
    except Exception as e:
        return f"[Ошибка чтения {path}: {e}]"

def parse_stat(stat_content):
    """Разбор /proc/[pid]/stat — возвращает словарь с основными полями.
    Индексы соответствуют документации proc(5):
      pid    (0)
      comm   (1)
      state  (2)
      ppid   (3)
      utime  (13)
      stime  (14)
      rss    (23)
    """
    try:
        parts = stat_content.split()
        if len(parts) < 24:  # проверка на достаточную длину
            raise ValueError("Недостаточно полей в stat")
        return {
            "pid": parts[0],
            "comm": parts[1].strip("()"),
            "state": parts[2],
            "ppid": parts[3],
            "utime": parts[13],
            "stime": parts[14],
            "rss": parts[23],
        }
    except Exception as e:
        print(f"[Ошибка разбора stat]: {e}")
        return {}

def main():
    # --- Валидация PID ---
    if len(sys.argv) > 1:
        pid = sys.argv[1]
        if not pid.isdigit():
            print(f"Ошибка: PID должен быть числом, получено '{pid}'")
            sys.exit(1)
    else:
        pid = str(os.getpid())

    proc_dir = f"/proc/{pid}"
    if not os.path.exists(proc_dir):
        print(f"Процесс с PID {pid} не найден.")
        sys.exit(1)

    print("=" * 28)
    print(f"pstat — Анализ процесса {pid}")
    print("=" * 28)

    # --- /proc/[pid]/stat ---
    stat_path = os.path.join(proc_dir, "stat")
    stat_content = read_file(stat_path)
    stat_data = parse_stat(stat_content)
    if stat_data:
        print(f"PID: {stat_data['pid']}")
        print(f"Команда: {stat_data['comm']}")
        print(f"Состояние: {stat_data['state']}")
        print(f"PPID: {stat_data['ppid']}")
        print(f"utime: {stat_data['utime']}")
        print(f"stime: {stat_data['stime']}")
        print(f"RSS (страницы): {stat_data['rss']}")
    else:
        print("Не удалось разобрать /proc/[pid]/stat")

    # --- /proc/[pid]/status ---
    status_path = os.path.join(proc_dir, "status")
    status_content = read_file(status_path)
    if not status_content.startswith("[Ошибка"):
        print("\n--- /proc/[pid]/status ---")
        lines = status_content.splitlines()
        for line in lines:
            if any(key in line for key in ["Name:", "State:", "Pid:", "PPid:", "Uid:", "VmRSS", "VmSize"]):
                print(line)

    # --- /proc/[pid]/cmdline ---
    cmdline_path = os.path.join(proc_dir, "cmdline")
    cmdline_content = read_file(cmdline_path)
    print("\n--- /proc/[pid]/cmdline ---")
    if "\x00" in cmdline_content:
        cmdline_content = cmdline_content.replace("\x00", " ")
    print(cmdline_content or "(пусто)")

    print("\nЗавершено успешно")

if __name__ == "__main__":
    main()

#!/usr/bin/env python3
import os
import sys
import argparse
from pathlib import Path

class Colors:
    """Цвета для красивого вывода"""
    HEADER = '\033[95m'
    BLUE = '\033[94m'
    CYAN = '\033[96m'
    GREEN = '\033[92m'
    YELLOW = '\033[93m'
    RED = '\033[91m'
    END = '\033[0m'
    BOLD = '\033[1m'
    UNDERLINE = '\033[4m'

def read_proc_file(pid, filename):
    """Чтение файла из /proc с обработкой ошибок"""
    try:
        with open(f"/proc/{pid}/{filename}", "r") as f:
            return f.read()
    except FileNotFoundError:
        return None
    except PermissionError:
        return None

def parse_proc_stat(pid):
    """Парсинг /proc/<pid>/stat"""
    content = read_proc_file(pid, "stat")
    if not content:
        return None
    
    parts = content.split()
    return {
        'pid': parts[0],
        'comm': parts[1][1:-1],
        'state': parts[2],
        'ppid': parts[3],
        'utime': int(parts[13]),
        'stime': int(parts[14]),
        'num_threads': int(parts[19])
    }

def parse_proc_status(pid):
    """Парсинг /proc/<pid>/status"""
    content = read_proc_file(pid, "status")
    if not content:
        return None
    
    result = {}
    for line in content.split('\n'):
        if ':' in line:
            key, value = line.split(':', 1)
            result[key.strip()] = value.strip()
    return result

def parse_proc_io(pid):
    """Парсинг /proc/<pid>/io"""
    content = read_proc_file(pid, "io")
    if not content:
        return None
    
    result = {}
    for line in content.split('\n'):
        if ':' in line:
            key, value = line.split(':', 1)
            result[key.strip()] = int(value.strip())
    return result

def parse_smaps_data(pid):
    """Парсинг smaps_rollup или smaps для получения RssAnon и RssFile"""
    content = read_proc_file(pid, "smaps_rollup")
    if not content:
        content = read_proc_file(pid, "smaps")
    
    if not content:
        return None
    
    result = {}
    rss_anon = 0
    rss_file = 0
    rss_shmem = 0
    
    for line in content.split('\n'):
        if line.startswith('RssAnon:'):
            rss_anon = int(line.split()[1])
        elif line.startswith('RssFile:'):
            rss_file = int(line.split()[1])
        elif line.startswith('RssShmem:'):
            rss_shmem = int(line.split()[1])
    
    result['RssAnon'] = rss_anon
    result['RssFile'] = rss_file + rss_shmem
    return result

def format_bytes(size_bytes):
    """Форматирование байтов в человекочитаемый вид"""
    if size_bytes >= 1024 * 1024:
        return f"{size_bytes / (1024 * 1024):.2f} MiB"
    elif size_bytes >= 1024:
        return f"{size_bytes / 1024:.2f} KiB"
    else:
        return f"{size_bytes} B"

def format_kb_to_mib(kb_value):
    """Конвертация килобайтов в мегабайты"""
    try:
        if isinstance(kb_value, str):
            kb = int(kb_value.split()[0])
        else:
            kb = int(kb_value)
        return kb / 1024.0
    except (ValueError, TypeError, IndexError):
        return 0

def ticks_to_seconds(ticks, HZ=100):
    """Конвертация тиков в секунды"""
    return ticks / HZ

def print_section(title):
    """Печать заголовка секции"""
    print(f"\n{Colors.HEADER}{Colors.BOLD}{title}{Colors.END}")
    print("=" * 50)

def print_field(name, value, color=Colors.CYAN):
    """Печать поля с выравниванием"""
    print(f"{color}{name:<30}{Colors.END} {value}")

def main():
    parser = argparse.ArgumentParser(description='Утилита pstat для просмотра метрик процесса')
    parser.add_argument('pid', type=int, help='PID процесса')
    args = parser.parse_args()
    
    pid = args.pid
    
    # Проверяем существование процесса
    if not Path(f"/proc/{pid}").exists():
        print(f"{Colors.RED}Ошибка: Процесс с PID {pid} не найден{Colors.END}")
        sys.exit(1)
    
    # Собираем данные
    stat_data = parse_proc_stat(pid)
    status_data = parse_proc_status(pid)
    io_data = parse_proc_io(pid)
    smaps_data = parse_smaps_data(pid)
    
    if not stat_data:
        print(f"{Colors.RED}Не удалось прочитать данные процесса{Colors.END}")
        sys.exit(1)
    
    # Вычисляем производные значения
    HZ = os.sysconf(os.sysconf_names['SC_CLK_TCK'])
    cpu_time_sec = ticks_to_seconds(stat_data['utime'] + stat_data['stime'], HZ)
    
    # Получаем информацию о памяти
    vm_rss_mib = 0
    if status_data and 'VmRSS' in status_data:
        vm_rss_mib = format_kb_to_mib(status_data['VmRSS'])
    
    # Получаем RssAnon и RssFile
    rss_anon_mib = 0
    rss_file_mib = 0
    if smaps_data:
        rss_anon_mib = format_kb_to_mib(smaps_data.get('RssAnon', 0))
        rss_file_mib = format_kb_to_mib(smaps_data.get('RssFile', 0))
    
    # Извлекаем значения переключений контекста
    voluntary_switches = status_data.get('voluntary_ctxt_switches', 'N/A') if status_data else 'N/A'
    nonvoluntary_switches = status_data.get('nonvoluntary_ctxt_switches', 'N/A') if status_data else 'N/A'
    
    # Вывод результатов
    print(f"\n{Colors.BOLD}{Colors.HEADER}🔍 СТАТИСТИКА ПРОЦЕССА {pid} ({stat_data['comm']}){Colors.END}")
    print("=" * 60)
    
    print_section("📋 ОСНОВНАЯ ИНФОРМАЦИЯ")
    print_field("PID", f"{pid}")
    print_field("Имя процесса", stat_data['comm'])
    print_field("Родительский PID (PPid)", stat_data['ppid'])
    print_field("Количество потоков", stat_data['num_threads'])
    print_field("Состояние", stat_data['state'])
    
    print_section("⏱️  ПРОЦЕССОРНОЕ ВРЕМЯ")
    print_field("Общее время CPU", f"{cpu_time_sec:.2f} сек")
    print_field("Время в пользовательском режиме", f"{stat_data['utime']} тиков")
    print_field("Время в режиме ядра", f"{stat_data['stime']} тиков")
    print_field("Частота HZ системы", f"{HZ}")
    
    print_section("🔄 ПЕРЕКЛЮЧЕНИЯ КОНТЕКСТА")
    print_field("Добровольные переключения", voluntary_switches)
    print_field("Принудительные переключения", nonvoluntary_switches)
    
    print_section("💾 ПАМЯТЬ")
    print_field("Физическая память (VmRSS)", f"{vm_rss_mib:.2f} MiB")
    print_field("Анонимная память (RssAnon)", f"{rss_anon_mib:.2f} MiB")
    print_field("Файловая память (RssFile)", f"{rss_file_mib:.2f} MiB")
    
    print_section("📁 ВВОД-ВЫВОД")
    if io_data:
        print_field("Прочитано байт", format_bytes(io_data.get('read_bytes', 0)))
        print_field("Записано байт", format_bytes(io_data.get('write_bytes', 0)))
    else:
        print_field("Прочитано байт", "N/A")
        print_field("Записано байт", "N/A")
    
    # Дополнительная информация из statm
    statm_content = read_proc_file(pid, "statm")
    if statm_content:
        statm_parts = statm_content.split()
        print_section("📊 ДОПОЛНИТЕЛЬНАЯ ИНФОРМАЦИЯ")
        print_field("Всего страниц памяти", statm_parts[0])
        print_field("Страниц RSS", statm_parts[1])
        print_field("Размер страницы", "4 KiB (типично)")
        print_field("Общий размер памяти", f"{int(statm_parts[0]) * 4} KiB")

if __name__ == "__main__":
    main()
import subprocess
import sys
from pathlib import Path


def check_python_version():
    if sys.version_info < (3, 6):
        print("❌ Требуется Python 3.6 или выше")
        sys.exit(1)
    print(f"✅ Python {sys.version_info.major}.{sys.version_info.minor}")


def install_requirements():
    """Устанавливает зависимости из requirements.txt"""
    requirements_file = Path("requirements.txt")

    if not requirements_file.exists():
        print("❌ Файл requirements.txt не найден")
        sys.exit(1)

    try:
        subprocess.check_call([sys.executable, "-m", "pip", "install", "-r", "requirements.txt"])
        print("✅ Зависимости установлены успешно")
    except subprocess.CalledProcessError:
        print("❌ Ошибка при установке зависимостей")
        sys.exit(1)


if __name__ == "__main__":
    print("🔧 Установка зависимостей...")
    check_python_version()
    install_requirements()
    print("🎉 Установка завершена!")
import json
import logging
import os
import sys

logger = logging.getLogger("ConfigLoader")

def load_config(filepath):
    """
    Читает JSON конфиг и возвращает словарь.
    При ошибке завершает программу (если критично) или выбрасывает исключение.
    """
    if not os.path.exists(filepath):
        logger.error(f"Файл конфигурации не найден: {filepath}")
        sys.exit(1)

    try:
        with open(filepath, 'r') as f:
            config = json.load(f)
        logger.info(f"Конфигурация успешно загружена из {filepath}")
        return config
    except json.JSONDecodeError as e:
        logger.error(f"Ошибка синтаксиса JSON: {e}")
        sys.exit(1)
    except Exception as e:
        logger.error(f"Непредвиденная ошибка при чтении конфига: {e}")
        sys.exit(1)
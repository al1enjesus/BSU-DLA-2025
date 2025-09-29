import json
import os
from dataclasses import dataclass
from typing import Dict, Any, List, Optional


@dataclass
class ModeConfig:
    work_us: int
    sleep_us: int


@dataclass
class WorkerScheduling:
    worker_id: int
    nice: Optional[int] = None
    cpu_affinity: Optional[List[int]] = None


@dataclass
class SupervisorConfig:
    workers: int
    mode_heavy: ModeConfig
    mode_light: ModeConfig
    scheduling: List[WorkerScheduling] = None

    @classmethod
    def from_file(cls, filename: str) -> 'SupervisorConfig':
        """Загрузка конфигурации из JSON файла с валидацией"""
        if not os.path.exists(filename):
            raise FileNotFoundError(f"Config file {filename} not found")

        try:
            with open(filename, 'r') as f:
                data = json.load(f)
        except Exception as e:
            raise ValueError(f"Ошибка чтения JSON {filename}: {e}")

        # Валидация ключей
        required_keys = ["workers", "mode_heavy", "mode_light", "scheduling"]
        for key in required_keys:
            if key not in data:
                raise ValueError(f"Отсутствует ключ '{key}' в конфигурации")

        # Валидация workers
        if not isinstance(data["workers"], int) or data["workers"] <= 0:
            raise ValueError("Поле 'workers' должно быть положительным числом")

        # Валидация mode_heavy и mode_light
        for mode in ["mode_heavy", "mode_light"]:
            if not isinstance(data[mode], dict):
                raise ValueError(f"{mode} должен быть объектом с полями work_us и sleep_us")
            if "work_us" not in data[mode] or "sleep_us" not in data[mode]:
                raise ValueError(f"{mode} должен содержать work_us и sleep_us")
            if not isinstance(data[mode]["work_us"], int) or not isinstance(data[mode]["sleep_us"], int):
                raise ValueError(f"{mode}: work_us и sleep_us должны быть числами")

        # Валидация scheduling
        scheduling = []
        if not isinstance(data["scheduling"], list):
            raise ValueError("Поле 'scheduling' должно быть списком")

        for sched_data in data["scheduling"]:
            if not isinstance(sched_data, dict):
                raise ValueError("Элемент scheduling должен быть объектом")
            if "worker_id" not in sched_data:
                raise ValueError("Каждый элемент scheduling должен содержать worker_id")
            if not isinstance(sched_data["worker_id"], int) or sched_data["worker_id"] < 0:
                raise ValueError("worker_id должен быть неотрицательным числом")
            if "nice" in sched_data and not isinstance(sched_data["nice"], int):
                raise ValueError("Поле 'nice' должно быть числом")
            if "cpu_affinity" in sched_data and not all(isinstance(c, int) for c in sched_data["cpu_affinity"]):
                raise ValueError("cpu_affinity должен быть списком чисел")

            scheduling.append(WorkerScheduling(**sched_data))

        return cls(
            workers=data['workers'],
            mode_heavy=ModeConfig(**data['mode_heavy']),
            mode_light=ModeConfig(**data['mode_light']),
            scheduling=scheduling
        )

    def to_dict(self) -> Dict[str, Any]:
        """Конвертация в словарь для сериализации"""
        result = {
            'workers': self.workers,
            'mode_heavy': {
                'work_us': self.mode_heavy.work_us,
                'sleep_us': self.mode_heavy.sleep_us
            },
            'mode_light': {
                'work_us': self.mode_light.work_us,
                'sleep_us': self.mode_light.sleep_us
            }
        }

        if self.scheduling:
            result['scheduling'] = [
                {
                    'worker_id': s.worker_id,
                    'nice': s.nice,
                    'cpu_affinity': s.cpu_affinity
                }
                for s in self.scheduling
            ]

        return result

    def get_scheduling_for_worker(self, worker_id: int) -> Optional[WorkerScheduling]:
        """Получение настроек планирования для конкретного воркера"""
        if not self.scheduling:
            return None

        for sched in self.scheduling:
            if sched.worker_id == worker_id:
                return sched
        return None

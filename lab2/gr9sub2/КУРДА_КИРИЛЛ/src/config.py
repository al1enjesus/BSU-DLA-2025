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
        """Загрузка конфигурации из JSON файла"""
        if not os.path.exists(filename):
            raise FileNotFoundError(f"Config file {filename} not found")

        with open(filename, 'r') as f:
            data = json.load(f)

        scheduling = []
        if 'scheduling' in data:
            for sched_data in data['scheduling']:
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
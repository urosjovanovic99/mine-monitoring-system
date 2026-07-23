import csv
import os
from datetime import datetime

COLUMNS = [
    "Timestamp", "CH4", "CO", "Air Flow",
    "Pump water flow", "WATER_LEVEL", "Pump flag", "Alarm flag", "User action",
]

# Mirrors the firmware WaterLevelEvent_t enum (main.h):
#   WATER_LEVEL_NORMAL = 0, WATER_LEVEL_HIGH = 1, WATER_LEVEL_LOW = 2
_WATER_LEVEL_NAMES = {
    0: "WATER_LEVEL_NORMAL",
    1: "WATER_LEVEL_HIGH",
    2: "WATER_LEVEL_LOW",
}


def water_level_text(value):
    """Map a raw water_level code to its label, or '--' if unknown/missing."""
    if value is None:
        return "--"
    return _WATER_LEVEL_NAMES.get(value, "--")


class TelemetryArchiver:
    def __init__(self, directory=None):
        self.directory = directory or os.path.join(
            os.path.dirname(os.path.abspath(__file__)), "archive")
        os.makedirs(self.directory, exist_ok=True)
        stamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        self.filepath = os.path.join(self.directory, f"telemetry_{stamp}.csv")

        self._last_snapshot = None
        self._latest_data = {}

        self._file = open(self.filepath, "w", newline="", encoding="utf-8")
        self._writer = csv.writer(self._file)
        self._writer.writerow(COLUMNS)
        self._file.flush()

    @staticmethod
    def _snapshot(data: dict):
        return (
            data.get("methane"), data.get("co"), data.get("airflow"),
            data.get("waterflow"), data.get("water_level"),
            data.get("pump"), data.get("alarm"),
        )

    @staticmethod
    def _row(data: dict, user_action: str = ""):
        return [
            datetime.now().isoformat(timespec="seconds"),
            data.get("methane", ""),
            data.get("co", ""),
            data.get("airflow", ""),
            "FLOW" if data.get("waterflow") else "NO FLOW",
            water_level_text(data.get("water_level")),
            "ON" if data.get("pump") else "OFF",
            "ACTIVE" if data.get("alarm") else "DEACTIVATED",
            user_action,
        ]

    def _write(self, row):
        self._writer.writerow(row)
        self._file.flush()

    def log_telemetry(self, data: dict):
        snapshot = self._snapshot(data)
        self._latest_data = data
        if snapshot == self._last_snapshot:
            return
        self._last_snapshot = snapshot
        self._write(self._row(data))

    def log_user_action(self, action: str):
        self._write(self._row(self._latest_data, user_action=action))
        self._last_snapshot = self._snapshot(self._latest_data)

    def close(self):
        try:
            self._file.close()
        except OSError:
            pass

import sys
import json
import socket
from PyQt5.QtCore import QThread, pyqtSignal, QObject, Qt
from PyQt5.QtWidgets import (
    QApplication, QMainWindow, QWidget, QVBoxLayout, QFormLayout,
    QLabel, QPushButton, QHBoxLayout, QStatusBar,
    QGroupBox, QSpinBox, QDoubleSpinBox, QGridLayout,
)

from telemetry_logger import TelemetryArchiver, water_level_text


class TelemetryClient(QThread):
    telemetry_received = pyqtSignal(dict)
    connection_changed = pyqtSignal(bool, str)

    def __init__(self, host="127.0.0.1", port=3456, parent=None):
        super().__init__(parent)
        self.host = host
        self.port = port
        self._sock = None
        self._running = False

    def run(self):
        self._running = True
        while self._running:
            try:
                self._sock = socket.create_connection((self.host, self.port), timeout=5)
                self._sock.settimeout(1.0)
                self.connection_changed.emit(True, f"Connected to {self.host}:{self.port}")
                self._read_loop()
            except (ConnectionRefusedError, OSError) as exc:
                self.connection_changed.emit(False, f"Connection failed: {exc}")
                self.msleep(2000)  # retry - Renode may not have started the bridge yet
            finally:
                if self._sock:
                    self._sock.close()
                    self._sock = None

    def _read_loop(self):
        buf = b""
        while self._running:
            try:
                chunk = self._sock.recv(4096)
            except socket.timeout:
                continue
            if not chunk:
                self.connection_changed.emit(False, "Peer closed connection")
                return
            buf += chunk
            while b"\n" in buf:
                line, buf = buf.split(b"\n", 1)
                self._handle_line(line)

    def _handle_line(self, line: bytes):
        line = line.strip()
        if not line:
            return
        try:
            data = json.loads(line.decode("utf-8", errors="replace"))
        except json.JSONDecodeError:
            return  # ignore partial/garbled frames (e.g. Renode boot banner)
        self.telemetry_received.emit(data)

    def send_command(self, command: dict):
        if self._sock is None:
            return False
        try:
            payload = (json.dumps(command) + "\n").encode("utf-8")
            print(f"Sending command: {payload!r}")
            self._sock.sendall(payload)
            return True
        except OSError:
            return False

    def stop(self):
        self._running = False
        if self._sock:
            try:
                self._sock.shutdown(socket.SHUT_RDWR)
            except OSError:
                pass


class DashboardWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Mine Monitoring - Telemetry")
        self.resize(560, 520)

        central = QWidget()
        self.setCentralWidget(central)
        layout = QVBoxLayout(central)

        # Left side: sensor readings. Right side: current water level.
        top = QHBoxLayout()

        form = QFormLayout()
        self.methane_label = QLabel("--")
        self.co_label = QLabel("--")
        self.airflow_label = QLabel("--")
        self.pump_water_flow_label = QLabel("--")
        self.pump_label = QLabel("--")
        self.alarm_label = QLabel("--")
        form.addRow("Methane (raw ADC):", self.methane_label)
        form.addRow("CO (raw ADC):", self.co_label)
        form.addRow("Airflow (raw ADC):", self.airflow_label)
        form.addRow("Pump water flow:", self.pump_water_flow_label)
        form.addRow("Pump running:", self.pump_label)
        form.addRow("Alarm active:", self.alarm_label)
        top.addLayout(form)

        top.addStretch(1)

        water_panel = QVBoxLayout()
        water_title = QLabel("Water level")
        water_title.setAlignment(Qt.AlignCenter)
        self.water_level_label = QLabel("--")
        self.water_level_label.setAlignment(Qt.AlignCenter)
        self.water_level_label.setStyleSheet("font-size: 16px; font-weight: bold;")
        self.tank_level_label = QLabel("-- %")
        self.tank_level_label.setAlignment(Qt.AlignCenter)
        water_panel.addStretch(1)
        water_panel.addWidget(water_title)
        water_panel.addWidget(self.water_level_label)
        water_panel.addWidget(self.tank_level_label)
        water_panel.addStretch(1)
        top.addLayout(water_panel)

        layout.addLayout(top)

        buttons = QHBoxLayout()
        self.ack_btn = QPushButton("Acknowledge alarm")
        self.pump_switch = QPushButton("Pump switch")
        buttons.addWidget(self.ack_btn)
        buttons.addWidget(self.pump_switch)
        layout.addLayout(buttons)

        layout.addWidget(self._build_sim_panel())

        self.status = QStatusBar()
        self.setStatusBar(self.status)

        self.archiver = TelemetryArchiver()

        self.client = TelemetryClient(host="127.0.0.1", port=3456)
        self.client.telemetry_received.connect(self.on_telemetry)
        self.client.connection_changed.connect(self.on_connection_changed)

        self.ack_btn.clicked.connect(self.on_ack_clicked)
        self.pump_switch.clicked.connect(self.on_pump_switch_clicked)

        self.client.start()

    def _build_sim_panel(self):
        """Interactive simulation controls (Zadatak 2: setting timing
        characteristics and fault situations). All controls send JSON commands
        to the MCU over the same socket used for pump/alarm commands."""
        box = QGroupBox("Simulation control")
        grid = QGridLayout(box)

        # --- water-tank change rate (timing characteristic) ---
        grid.addWidget(QLabel("<b>Water tank</b>"), 0, 0, 1, 4)

        self.fill_spin = QDoubleSpinBox()
        self.fill_spin.setRange(0.0, 50.0)
        self.fill_spin.setSingleStep(0.5)
        self.fill_spin.setValue(1.0)
        self.fill_spin.setSuffix(" %/s")
        self.drain_spin = QDoubleSpinBox()
        self.drain_spin.setRange(0.0, 50.0)
        self.drain_spin.setSingleStep(0.5)
        self.drain_spin.setValue(3.0)
        self.drain_spin.setSuffix(" %/s")
        rates_btn = QPushButton("Apply rates")
        rates_btn.clicked.connect(self.on_apply_rates)

        grid.addWidget(QLabel("Fill (pump off):"), 1, 0)
        grid.addWidget(self.fill_spin, 1, 1)
        grid.addWidget(QLabel("Drain (pump on):"), 1, 2)
        grid.addWidget(self.drain_spin, 1, 3)
        grid.addWidget(rates_btn, 2, 0, 1, 4)

        # --- per-sensor read-failure injection (fault situation) ---
        grid.addWidget(QLabel("<b>Sensor read faults</b>"), 3, 0, 1, 4)
        grid.addWidget(QLabel("fail %"), 4, 1)
        grid.addWidget(QLabel("force N"), 4, 3)

        self.fail_pct_spins = {}
        self.force_spins = {}
        sensors = [("Methane", "methane"), ("CO", "co"), ("Airflow", "airflow")]
        for i, (label, key) in enumerate(sensors):
            row = 5 + i
            grid.addWidget(QLabel(label + ":"), row, 0)

            pct = QSpinBox()
            pct.setRange(0, 100)
            pct.setSuffix(" %")
            self.fail_pct_spins[key] = pct
            pct_btn = QPushButton("Set")
            pct_btn.clicked.connect(lambda _, k=key: self.on_apply_fail_pct(k))
            grid.addWidget(pct, row, 1)
            grid.addWidget(pct_btn, row, 2)

            force = QSpinBox()
            force.setRange(0, 1000)
            force.setValue(2)
            self.force_spins[key] = force
            force_btn = QPushButton("Force")
            force_btn.clicked.connect(lambda _, k=key: self.on_force_fail(k))
            grid.addWidget(force, row, 3)
            grid.addWidget(force_btn, row, 4)

        return box

    def on_apply_rates(self):
        cmd = {"cmd": "SIM_WATER_RATE",
               "fill": round(self.fill_spin.value(), 2),
               "drain": round(self.drain_spin.value(), 2)}
        self.archiver.log_user_action(
            f"SIM_WATER_RATE fill={cmd['fill']} drain={cmd['drain']}")
        self.client.send_command(cmd)

    def on_apply_fail_pct(self, sensor):
        pct = self.fail_pct_spins[sensor].value()
        self.archiver.log_user_action(f"SIM_FAIL_PCT {sensor}={pct}")
        self.client.send_command({"cmd": "SIM_FAIL_PCT", "sensor": sensor, "pct": pct})

    def on_force_fail(self, sensor):
        n = self.force_spins[sensor].value()
        self.archiver.log_user_action(f"SIM_FORCE_FAIL {sensor}={n}")
        self.client.send_command({"cmd": "SIM_FORCE_FAIL", "sensor": sensor, "n": n})

    def on_ack_clicked(self):
        self.archiver.log_user_action("ALARM_ACK")
        self.client.send_command({"cmd": "ALARM_ACK"})

    def on_pump_switch_clicked(self):
        self.archiver.log_user_action("PUMP_TOGGLE")
        self.client.send_command({"cmd": "PUMP_TOGGLE"})

    def on_telemetry(self, data: dict):
        self.archiver.log_telemetry(data)

        def fmt(value, valid_key):
            if valid_key is not None and not data.get(valid_key, 1):
                return f"{value} (not yet valid)"
            return str(value)

        self.methane_label.setText(fmt(data.get('methane', '--'), 'methane_valid'))
        self.co_label.setText(fmt(data.get('co', '--'), 'co_valid'))
        self.airflow_label.setText(fmt(data.get('airflow', '--'), 'airflow_valid'))
        self.pump_water_flow_label.setText("FLOW" if data.get('waterflow') else "NO FLOW")
        self.pump_label.setText("ON" if data.get('pump') else "OFF")
        self.alarm_label.setText("ACTIVE" if data.get('alarm') else "DEACTIVATED")
        self.water_level_label.setText(water_level_text(data.get('water_level')))
        tank = data.get('tank_level')
        self.tank_level_label.setText("-- %" if tank is None else f"tank: {tank} %")

    def on_connection_changed(self, connected: bool, message: str):
        self.status.showMessage(message)

    def closeEvent(self, event):
        self.client.stop()
        self.client.wait(2000)
        self.archiver.close()
        event.accept()


def main():
    app = QApplication(sys.argv)
    window = DashboardWindow()
    window.show()
    sys.exit(app.exec_())


if __name__ == "__main__":
    main()

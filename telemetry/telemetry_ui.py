import sys
import json
import socket
from PyQt5.QtCore import QThread, pyqtSignal, QObject, Qt
from PyQt5.QtWidgets import (
    QApplication, QMainWindow, QWidget, QVBoxLayout, QFormLayout,
    QLabel, QPushButton, QHBoxLayout, QStatusBar,
    QGroupBox, QCheckBox, QSlider, QProgressBar, QComboBox,
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
        self.resize(520, 420)

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
        water_panel.addStretch(1)
        water_panel.addWidget(water_title)
        water_panel.addWidget(self.water_level_label)
        water_panel.addStretch(1)
        top.addLayout(water_panel)

        layout.addLayout(top)

        buttons = QHBoxLayout()
        self.ack_btn = QPushButton("Acknowledge alarm")
        self.pump_switch = QPushButton("Pump switch")
        buttons.addWidget(self.ack_btn)
        buttons.addWidget(self.pump_switch)
        layout.addLayout(buttons)

        # --- Simulation / Environment panel -------------------------------
        # Test-harness controls, not operator controls: when the master switch
        # is on, the UI plays the role of the physical environment - it drives
        # the water-level rate and can inject sensor (ADC) faults. With it off,
        # the firmware runs purely on real hardware stimuli (EXTI, real ADC).
        sim_group = QGroupBox("Simulation / Environment (test)")
        sim_layout = QVBoxLayout(sim_group)

        self.sim_enable = QCheckBox("Simulation mode (UI drives environment)")
        sim_layout.addWidget(self.sim_enable)

        rate_row = QHBoxLayout()
        rate_row.addWidget(QLabel("Water rate:"))
        self.rate_slider = QSlider(Qt.Horizontal)
        self.rate_slider.setMinimum(-100)   # mm/s, draining
        self.rate_slider.setMaximum(100)    # mm/s, filling
        self.rate_slider.setValue(0)
        self.rate_slider.setTickInterval(25)
        self.rate_slider.setTickPosition(QSlider.TicksBelow)
        self.rate_slider.setEnabled(False)
        rate_row.addWidget(self.rate_slider, 1)
        self.rate_value = QLabel("0 mm/s")
        rate_row.addWidget(self.rate_value)
        sim_layout.addLayout(rate_row)

        level_row = QHBoxLayout()
        level_row.addWidget(QLabel("Sim level:"))
        self.level_bar = QProgressBar()
        self.level_bar.setMinimum(0)
        self.level_bar.setMaximum(1000)     # WATER_SIM_MAX_MM
        self.level_bar.setValue(400)        # WATER_SIM_START_MM
        self.level_bar.setFormat("%v mm")
        level_row.addWidget(self.level_bar, 1)
        sim_layout.addLayout(level_row)

        # Exceptional-case injection: crash a chosen sensor twice in a row so
        # its consecutive-error counter trips the alarm.
        crash_row = QHBoxLayout()
        crash_row.addWidget(QLabel("Crash sensor:"))
        self.sensor_combo = QComboBox()
        self.sensor_combo.addItems(["METHANE", "CO", "AIRFLOW"])
        self.sensor_combo.setEnabled(False)
        crash_row.addWidget(self.sensor_combo, 1)
        self.crash_btn = QPushButton("Crash (2×)")
        self.crash_btn.setEnabled(False)
        crash_row.addWidget(self.crash_btn)
        sim_layout.addLayout(crash_row)

        self.crashing_label = QLabel("Crashing: NONE")
        sim_layout.addWidget(self.crashing_label)

        layout.addWidget(sim_group)

        self.status = QStatusBar()
        self.setStatusBar(self.status)

        self.archiver = TelemetryArchiver()

        self.client = TelemetryClient(host="127.0.0.1", port=3456)
        self.client.telemetry_received.connect(self.on_telemetry)
        self.client.connection_changed.connect(self.on_connection_changed)

        self.ack_btn.clicked.connect(self.on_ack_clicked)
        self.pump_switch.clicked.connect(self.on_pump_switch_clicked)
        self.sim_enable.toggled.connect(self.on_sim_toggled)
        self.rate_slider.valueChanged.connect(self.on_rate_changed)
        self.crash_btn.clicked.connect(self.on_crash_clicked)

        self.client.start()

    def on_ack_clicked(self):
        self.archiver.log_user_action("ALARM_ACK")
        self.client.send_command({"cmd": "ALARM_ACK"})

    def on_pump_switch_clicked(self):
        self.archiver.log_user_action("PUMP_TOGGLE")
        self.client.send_command({"cmd": "PUMP_TOGGLE"})

    def on_sim_toggled(self, enabled: bool):
        self.rate_slider.setEnabled(enabled)
        self.sensor_combo.setEnabled(enabled)
        self.crash_btn.setEnabled(enabled)
        cmd = "SIM_ON" if enabled else "SIM_OFF"
        self.archiver.log_user_action(cmd)
        self.client.send_command({"cmd": cmd})
        if enabled:
            # push the current slider value so firmware and UI agree immediately
            self.on_rate_changed(self.rate_slider.value())

    def on_rate_changed(self, value: int):
        self.rate_value.setText(f"{value} mm/s")
        self.archiver.log_user_action(f"SET_WATER_RATE={value}")
        self.client.send_command({"cmd": "SET_WATER_RATE", "rate": value})

    def on_crash_clicked(self):
        sensor = self.sensor_combo.currentText()
        self.archiver.log_user_action(f"CRASH_SENSOR={sensor}")
        self.client.send_command({"cmd": "CRASH_SENSOR", "sensor": sensor})

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

        # Reflect firmware-side sim state without echoing our own signals back
        # out as new commands (block signals while syncing the widgets).
        if 'water_level_mm' in data:
            self.level_bar.setValue(int(data.get('water_level_mm', 0)))
        if 'sim_mode' in data:
            sim_on = bool(data.get('sim_mode'))
            if sim_on != self.sim_enable.isChecked():
                self.sim_enable.blockSignals(True)
                self.sim_enable.setChecked(sim_on)
                self.rate_slider.setEnabled(sim_on)
                self.sensor_combo.setEnabled(sim_on)
                self.crash_btn.setEnabled(sim_on)
                self.sim_enable.blockSignals(False)
        if 'fault_sensor' in data:
            self.crashing_label.setText(f"Crashing: {data.get('fault_sensor', 'NONE')}")

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

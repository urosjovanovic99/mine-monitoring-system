"""
PyQt5 dashboard - connects to the Renode UART<->TCP bridge, not a serial port.

Renode exposes USART2 as a plain TCP socket via:
    emulation CreateServerSocketTerminal 3456 "uartBridge" true
    connector Connect sysbus.usart2 "uartBridge"

So the PC side is a normal socket client - no COM port, no pyserial needed.
Framing matches the firmware (task_ui_comms.c): one JSON object per line,
both directions. Downlink schema:
    {"methane":<u16>,"methane_valid":0|1,
     "co":<u16>,"co_valid":0|1,
     "airflow":<u16>,"airflow_valid":0|1,
     "pump":0|1,"alarm":0|1}
Uplink: only {"cmd":"ALARM_ACK"} is wired up on the firmware side so far.
A pump manual-override command isn't implemented yet - it needs a
safety-gated entry point added in task_pump_manager.c first (see the
comment in task_ui_comms.c) so it can't just force the pump on during a
methane-critical condition.

pip install PyQt5
"""

import sys
import json
import socket
from PyQt5.QtCore import QThread, pyqtSignal, QObject
from PyQt5.QtWidgets import (
    QApplication, QMainWindow, QWidget, QVBoxLayout, QFormLayout,
    QLabel, QPushButton, QHBoxLayout, QStatusBar,
)


class TelemetryClient(QThread):
    """Owns the TCP socket. Runs its own read loop; emits one signal per
    telemetry line and one signal on connection state changes."""

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
        """Uplink: PC -> MCU. Thread-safe enough for occasional button clicks;
        add a queue/mutex if commands can be sent from multiple threads."""
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
        self.resize(360, 260)

        central = QWidget()
        self.setCentralWidget(central)
        layout = QVBoxLayout(central)

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
        layout.addLayout(form)

        buttons = QHBoxLayout()
        self.ack_btn = QPushButton("Acknowledge alarm")
        buttons.addWidget(self.ack_btn)
        layout.addLayout(buttons)

        self.status = QStatusBar()
        self.setStatusBar(self.status)

        self.client = TelemetryClient(host="127.0.0.1", port=3456)
        self.client.telemetry_received.connect(self.on_telemetry)
        self.client.connection_changed.connect(self.on_connection_changed)

        self.ack_btn.clicked.connect(lambda: self.client.send_command(
            {"cmd": "ALARM_ACK"}))

        self.client.start()

    def on_telemetry(self, data: dict):
        def fmt(value, valid_key):
            if valid_key is not None and not data.get(valid_key, 1):
                return f"{value} (not yet valid)"
            return str(value)

        self.methane_label.setText(fmt(data.get('methane', '--'), 'methane_valid'))
        self.co_label.setText(fmt(data.get('co', '--'), 'co_valid'))
        self.airflow_label.setText(fmt(data.get('airflow', '--'), 'airflow_valid'))
        self.pump_water_flow_label.setText("FLOW" if data.get('waterflow') else "NO FLOW")
        self.pump_label.setText("ON" if data.get('pump') else "OFF")
        self.alarm_label.setText("ACTIVE" if data.get('alarm') else "clear")

    def on_connection_changed(self, connected: bool, message: str):
        self.status.showMessage(message)

    def closeEvent(self, event):
        self.client.stop()
        self.client.wait(2000)
        event.accept()


def main():
    app = QApplication(sys.argv)
    window = DashboardWindow()
    window.show()
    sys.exit(app.exec_())


if __name__ == "__main__":
    main()

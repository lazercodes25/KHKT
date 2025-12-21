import subprocess
import sys
import os
import time
import platform
import serial
import serial.tools.list_ports
import requests
import json
import threading
import tkinter as tk
from tkinter import ttk, messagebox


# ==== CAMERA CONFIG ====
cameras = [
    {"source": "1", "port": 8000},
    {"source": "2", "port": 8001},
]
VENV_DIR = os.path.join(os.path.dirname(__file__), ".venv")

# ==== GREEN TIME CONFIG ====
GREEN_BASE = 15
GREEN_MAX = 45
K = 0.8

CYCLE = 60
INTERGREEN = 4  # t_xk
effective = CYCLE - INTERGREEN


def clamp(value, min_v, max_v):
    return max(min_v, min(max_v, value))


def get_venv_python(venv_dir):
    system = platform.system()
    if system == "Windows":
        py = os.path.join(venv_dir, "Scripts", "python.exe")
    else:
        py = os.path.join(venv_dir, "bin", "python")

    if not os.path.isfile(py):
        raise FileNotFoundError(f"Không tìm thấy python trong venv tại: {py}")
    return py


# ===========================================================
#                     GUI APPLICATION
# ===========================================================
class App:
    def __init__(self, root):
        self.root = root
        self.root.title("Traffic Light Controller GUI")
        self.root.geometry("900x600")

        self.serial_port = None
        self.running = False
        self.processes = []

        # ================= UI COMPONENTS =====================

        frame_top = ttk.Frame(root, padding=10)
        frame_top.pack(fill="x")

        ttk.Label(frame_top, text="Chọn cổng COM: ").pack(side="left")

        self.combobox = ttk.Combobox(frame_top, width=20, values=self.get_serial_ports())
        self.combobox.pack(side="left", padx=5)

        ttk.Button(frame_top, text="Refresh", command=self.refresh_com_ports).pack(side="left", padx=5)

        self.btn_start = ttk.Button(frame_top, text="START", command=self.start_all)
        self.btn_start.pack(side="left", padx=10)

        self.btn_stop = ttk.Button(frame_top, text="STOP", command=self.stop_all, state="disabled")
        self.btn_stop.pack(side="left", padx=10)

        # ===== Serial Output Panels =====
        frame_bottom = ttk.Frame(root, padding=10)
        frame_bottom.pack(fill="both", expand=True)

        # Serial Read
        frame_read = ttk.LabelFrame(frame_bottom, text="Serial READ")
        frame_read.pack(side="left", fill="both", expand=True, padx=5)

        self.list_read = tk.Listbox(frame_read)
        self.list_read.pack(fill="both", expand=True)

        # Serial Write
        frame_write = ttk.LabelFrame(frame_bottom, text="Serial WRITE")
        frame_write.pack(side="left", fill="both", expand=True, padx=5)

        self.list_write = tk.Listbox(frame_write)
        self.list_write.pack(fill="both", expand=True)

    # -------------------------------------------------------

    def get_serial_ports(self):
        ports = serial.tools.list_ports.comports()
        return [p.device for p in ports]

    def refresh_com_ports(self):
        self.combobox["values"] = self.get_serial_ports()

    # -------------------------------------------------------

    def start_all(self):
        com = self.combobox.get()
        if not com:
            messagebox.showerror("Lỗi", "Vui lòng chọn cổng COM")
            return

        self.running = True
        self.btn_start.config(state="disabled")
        self.btn_stop.config(state="normal")

        threading.Thread(target=self.run_main_logic, args=(com,), daemon=True).start()

    # -------------------------------------------------------

    def stop_all(self):
        self.running = False
        self.btn_start.config(state="normal")
        self.btn_stop.config(state="disabled")

        for p in self.processes:
            try:
                p.terminate()
            except:
                pass

        self.processes.clear()
        messagebox.showinfo("STOP", "Đã dừng tất cả tiến trình")

    # -------------------------------------------------------

    def add_read(self, text):
        self.list_read.insert("end", text)
        self.list_read.yview_moveto(1)

    def add_write(self, text):
        self.list_write.insert("end", text)
        self.list_write.yview_moveto(1)

    # -------------------------------------------------------

    def run_main_logic(self, sport):
        # --------- Start Camera Processes ----------
        try:
            python_exe = get_venv_python(VENV_DIR)
        except Exception as e:
            messagebox.showerror("Lỗi VENV", str(e))
            return

        self.add_read("▶️ Đang khởi động camera...")

        for cam in cameras:
            cmd = [
                python_exe,
                "vehicle_counter.py",
                "--source", cam["source"],
                "--device", "cuda",
                "--show"
            ]
            p = subprocess.Popen(cmd)
            self.processes.append(p)
            time.sleep(2)

        time.sleep(5)
        self.add_read("✅ Camera đã hoạt động")

        # --------- Open Serial ----------
        try:
            ser = serial.Serial(sport, 115200, timeout=1)
            ser.dtr = False
            ser.rts = False
            time.sleep(2)
            self.add_read(f"🔌 ESP32 đã kết nối tại {sport}")
        except Exception as e:
            messagebox.showerror("Serial Error", str(e))
            return

        last_write = 0

        while self.running:
            # Serial READ
            if ser.in_waiting > 0:
                line = ser.readline().decode().strip()
                if line:
                    self.add_read("ESP → PC: " + line)

            # Every 2 seconds → read camera API + send JSON
            now = time.time()
            if now - last_write >= 2:
                total_G0 = total_G1 = 0

                for cam in cameras:
                    try:
                        resp = requests.get(f"http://127.0.0.1:{cam['port']}/total_vehicle", timeout=1)
                        if resp.status_code == 200:
                            data = resp.json()
                            if cam["source"] == "1":
                                total_G0 = data.get("total_vehicle", 0)
                            if cam["source"] == "2":
                                total_G1 = data.get("total_vehicle", 0)
                    except:
                        pass
                sum_all = max(total_G0 + total_G1, 1) # tránh chia 0
                b0 = total_G0 / sum_all
                b1 = total_G1 / sum_all
                green_G0 = clamp(int(effective * b0), GREEN_BASE, GREEN_MAX)
                green_G1 = clamp(int(effective * b1), GREEN_BASE, GREEN_MAX)
                payload = {
                    "green_time": {"G0": green_G0, "G1": green_G1}
                }

                json_str = json.dumps(payload)
                ser.write((json_str + "\n").encode())

                self.add_write("PC → ESP: " + json_str)

                last_write = now

            time.sleep(0.1)

        # End while
        ser.close()
        self.add_read("🔌 Đã ngắt kết nối serial")

# ===========================================================
# START GUI
# ===========================================================

if __name__ == "__main__":
    root = tk.Tk()
    app = App(root)
    root.mainloop()


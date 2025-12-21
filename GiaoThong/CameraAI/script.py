import subprocess
import sys
import os
import time
import platform
import serial
import argparse
import requests
import json

# ==== Cấu hình ====
cameras = [
    {"source": "0", "port": 8000},
    {"source": "1", "port": 8001},
]
VENV_DIR = os.path.join(os.path.dirname(__file__), ".venv")

# ==== Hệ số điều chỉnh đèn ====
GREEN_BASE = 15   # thời gian đèn xanh cơ bản (giây)
GREEN_MAX = 45    # giới hạn tối đa
K = 0.8           # hệ số nhạy cảm với chênh lệch mật độ

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

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--sport", required=True, help="Cổng serial ESP32 (VD: COM3 hoặc /dev/ttyUSB0)")
    args = parser.parse_args()


    # Khởi động các tiến trình camera
    try:
        python_exe = get_venv_python(VENV_DIR)
    except Exception as e:
        print("[ERROR]", e)
        sys.exit(1)

    processes = []
    for cam in cameras:
        cmd = [
            python_exe,
            "vehicle_counter.py",
            "--source", cam["source"],
            "--device", "cuda",     # đổi thành "cpu" nếu không có GPU
            "--show"
        ]
        print("▶️ Khởi động:", " ".join(cmd))
        p = subprocess.Popen(cmd)
        processes.append(p)
        time.sleep(2)

    print("\n✅ Đã khởi động xong cameras:")
    #delay to ensure cameras are up
    time.sleep(10)
    for cam in cameras:
        print(f"  http://127.0.0.1:{cam['port']}/total_vehicle  (camera {cam['source']})")

    # ==== Vòng lặp gửi dữ liệu ====
        # Kết nối serial
    ser = None
    if args.sport:
        try:
            ser = serial.Serial(args.sport, 115200, timeout=1)
            ser.dtr = False
            ser.rts = False
            time.sleep(2)  # chờ kết nối ổn định
        except Exception as e:
            print(f"[ERROR] Không thể kết nối ESP32 qua {args.sport}: {e}")
            sys.exit(1)
        print(f"🔌 Đã kết nối ESP32 qua {args.sport}")
    try:
        last_write = 0.0
        while True:
            #Read serial input (if any)
            if ser and ser.in_waiting > 0:
                line = ser.readline().decode().strip()
                if line:
                    print("📥 Nhận từ ESP32:", line)
            now = time.time()
            if now - last_write >= 2.0:
                total_G0, total_G1 = 0, 0
                for cam in cameras:
                    try:
                        resp = requests.get(f"http://127.0.0.1:{cam['port']}/total_vehicle", timeout=1)
                        if resp.status_code == 200:
                            data = resp.json()
                            if cam["source"] == "0":
                                total_G0 = data.get("total_vehicle", 0)
                            elif cam["source"] == "1":
                                total_G1 = data.get("total_vehicle", 0)
                    except Exception as e:
                        print(f"[WARN] Không lấy được dữ liệu từ camera {cam['source']}: {e}")
                # === Tính toán thời gian đèn xanh ===
                diff = total_G0 - total_G1
                green_G0 = clamp(int(GREEN_BASE + K * diff), 10, GREEN_MAX)
                green_G1 = clamp(int(GREEN_BASE - K * diff), 10, GREEN_MAX)

                payload = {
                    "green_time": {"G0": green_G0, "G1": green_G1}
                }

                json_str = json.dumps(payload)
                print("📤 Gửi:", json_str)

                if ser:
                    ser.write((json_str + "\n").encode())
                last_write = now
            time.sleep(0.1)
    except KeyboardInterrupt:
        print("\n🛑 Dừng tất cả tiến trình...")
        for p in processes:
            try:
                p.terminate()
            except Exception:
                pass

if __name__ == "__main__":
    main()

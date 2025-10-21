
## ⚙️ Sơ đồ logic (đơn giản hóa)

Ví dụ:
Ngã tư có 2 hướng chính đối xứng:

* Hướng **Đông–Tây (EW)**
* Hướng **Bắc–Nam (NS)**

Chu kỳ đèn có thể như sau:

| Trạng thái | EW | NS | Thời gian (s) |
| ---------- | -- | -- | ------------- |
| 1          | 🟢 | 🔴 | 15            |
| 2          | 🟡 | 🔴 | 3             |
| 3          | 🔴 | 🟢 | 15            |
| 4          | 🔴 | 🟡 | 3             |

Tổng chu kỳ: 36 giây → Lặp lại liên tục.

---

## 🔌 Phần cứng cơ bản

### 1️⃣ LED đèn giao thông

Mỗi LED nối qua **điện trở 220Ω** tới GPIO của ESP32.
VD:

```text
GPIO16 -> LED đỏ EW
GPIO17 -> LED vàng EW
GPIO18 -> LED xanh EW

GPIO19 -> LED đỏ NS
GPIO21 -> LED vàng NS
GPIO22 -> LED xanh NS
```

---

### 2️⃣ Hiển thị LED 7 đoạn (qua 74HC595)

Bạn cần **2 con 74HC595** để điều khiển **2 LED 7 đoạn** (chữ số hàng chục và hàng đơn vị).

Kết nối cơ bản:

| ESP32 pin | 74HC595 pin | Mô tả               |
| --------- | ----------- | ------------------- |
| GPIO23    | DS          | Data (serial input) |
| GPIO25    | SH_CP       | Clock               |
| GPIO26    | ST_CP       | Latch               |
| VCC/GND   | VCC/GND     | Nguồn               |


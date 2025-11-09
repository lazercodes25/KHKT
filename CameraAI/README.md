### 💡 Hướng dẫn cài đặt

1. **Tạo virtual environment (tốt cho project)**:

```bash
python -m venv venv
```

2. **Kích hoạt virtual environment**:

* Windows:

```bash
venv\Scripts\activate
```

* Linux/macOS:

```bash
source venv/bin/activate
```

3. **Cài tất cả package từ requirements.txt**:

```bash
pip install -r requirements.txt
```
4. **Cài thư viện pytorch với cuda**:
```bash
pip3 install torch torchvision --index-url https://download.pytorch.org/whl/cu126
```
5. **Tải CUDA Toolkit**
Truy cập https://developer.nvidia.com/cuda-12-6-0-download-archive để tải và cài đặt CUDA Toolkit phù hợp với hệ điều hành của bạn.\
Kiểm tra cài đặt bằng lệnh:
```bash
nvcc --version
```
6. **Kiểm tra torch đã chạy với CUDA chưa**
Mở Python shell và chạy:
```python
import torch
print(torch.cuda.is_available())
```
Nếu in ra `True` nghĩa là torch đã chạy với CUDA.
---
### 💡 Cách chạy

**1️⃣ Đếm xe trong ảnh và xuất file `output.jpg`:**

```bash
python vehicle_counter_final.py --source traffic.jpg --show
```

➡️ Kết quả:

* In số lượng xe ra terminal
* Lưu ảnh có khung + ID + tổng xe tại **`output.jpg`**

---

**2️⃣ Đếm xe trong video:**

```bash
python vehicle_counter_final.py --source traffic.mp4 --show --save-out out.mp4
```

---

**3️⃣ Đếm xe từ webcam (GPU):**

```bash
python vehicle_counter_final.py --source 0 --device cuda --show
```

---

**Tính năng:**

✅ Tự động nhận diện loại input (ảnh / video / webcam)

✅ Đếm số lượng phương tiện trong ảnh

✅ Xuất file ảnh kết quả tự động

✅ Giảm kích thước hiển thị phù hợp laptop

✅ Theo dõi xe bằng ID khi video / webcam

---


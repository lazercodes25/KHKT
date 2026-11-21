# Vercel Toilet Dashboard

Deploy thư mục này lên Vercel. Trên Vercel tạo Environment Variable:

```text
TOILET_DEVICE_KEY=doi-khoa-nay
```

Trong sketch ESP32, đặt cùng giá trị cho `web_device_key` và thay `web_url` bằng URL Vercel.

API `POST /api/telemetry` nhận JSON dạng `{"device_id":"toilet-01","DATA1":1}`.

Lưu ý: bản demo lưu dữ liệu trong bộ nhớ function nên có thể mất khi Vercel khởi động lại. Muốn lưu lâu dài cần nối thêm database/Redis.

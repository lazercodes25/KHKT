import cv2
import torch
from ultralytics import YOLO
import supervision as sv

# ======================
# Cấu hình mô hình
# ======================
model = YOLO("yolov8n.pt")  # dùng model nhẹ
device = "cuda" if torch.cuda.is_available() else "cpu"
model.to(device)

# ======================
# Video input (có thể là video file hoặc webcam)
# ======================
VIDEO_PATH = "vehicle-counting.mp4"  # hoặc đường dẫn ảnh
cap = cv2.VideoCapture(VIDEO_PATH)

# ======================
# Bộ đếm phương tiện đang hiện diện
# ======================
active_ids = set()
object_tracker = sv.ByteTrack()

# Danh sách class cần đếm (bạn có thể chỉnh)
TARGET_CLASSES = ["car", "bus", "truck", "motorbike"]

# ======================
# Hàm xử lý từng khung hình
# ======================
while cap.isOpened():
    ret, frame = cap.read()
    if not ret:
        break

    # Resize cho Jetson Nano (tối ưu tốc độ)
    frame = cv2.resize(frame, (640, 360))

    # Dò đối tượng
    results = model.track(frame, persist=True, tracker="bytetrack.yaml", verbose=False)

    # Vẽ bounding box
    annotated_frame = frame.copy()
    current_ids = set()

    if results[0].boxes.id is not None:
        boxes = results[0].boxes.xyxy.cpu().numpy()
        ids = results[0].boxes.id.cpu().numpy()
        cls = results[0].boxes.cls.cpu().numpy()

        for box, obj_id, cls_id in zip(boxes, ids, cls):
            class_name = model.names[int(cls_id)]
            if class_name in TARGET_CLASSES:
                x1, y1, x2, y2 = map(int, box)
                color = (0, 255, 0)
                cv2.rectangle(annotated_frame, (x1, y1), (x2, y2), color, 2)
                cv2.putText(
                    annotated_frame,
                    f"ID{int(obj_id)} {class_name}",
                    (x1, y1 - 10),
                    cv2.FONT_HERSHEY_SIMPLEX,
                    0.5,
                    color,
                    2,
                )
                current_ids.add(int(obj_id))

    # Cập nhật số lượng hiện diện
    new_ids = current_ids - active_ids
    gone_ids = active_ids - current_ids
    active_ids = current_ids

    count = len(active_ids)

    # Hiển thị số lượng phương tiện đang hiện diện
    cv2.putText(
        annotated_frame,
        f"Active Vehicles: {count}",
        (20, 40),
        cv2.FONT_HERSHEY_SIMPLEX,
        1,
        (0, 255, 255),
        3,
    )

    # Hiển thị lên màn hình
    cv2.imshow("Vehicle Real-Time Count", annotated_frame)

    # Lưu frame ra file (tùy chọn)
    # cv2.imwrite("output.jpg", annotated_frame)

    # Nhấn 'q' để thoát
    if cv2.waitKey(1) & 0xFF == ord("q"):
        break

cap.release()
cv2.destroyAllWindows()

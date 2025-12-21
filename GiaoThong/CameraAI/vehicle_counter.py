import argparse
import os
import cv2
from ultralytics import YOLO
import supervision as sv
from collections import Counter
from flask import Flask, jsonify
import threading

total_vehicle = 0  # giá trị global được cập nhật trong vòng lặp video

# Danh sách các class phương tiện trong COCO
VEHICLE_CLASSES = {
    1: "bicycle",
    2: "car",
    3: "motorcycle",
    5: "bus",
    6: "train",
    7: "truck",
}
def start_api(port=5000):
    app = Flask(__name__)

    @app.route("/total_vehicle", methods=["GET"])
    def get_total_vehicle():
        return jsonify({"total_vehicle": total_vehicle})
    print(f"[INFO] Starting API server on port {port}...")
    app.run(host="0.0.0.0", port=port)  

def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", required=True, help="Ảnh, video hoặc webcam (vd: 0)")
    parser.add_argument("--model", default="yolo11x.pt", help="Đường dẫn model YOLO")
    parser.add_argument("--device", default="cpu", choices=["cpu", "cuda"], help="Thiết bị chạy")
    parser.add_argument("--imgsz", type=int, default=640, help="Kích thước ảnh input")
    parser.add_argument("--conf", type=float, default=0.4, help="Ngưỡng confidence")
    parser.add_argument("--save-out", default="output.jpg", help="Tên file ảnh/video output")
    parser.add_argument("--show", action="store_true", help="Hiển thị realtime")
    return parser.parse_args()

def process_image(image_path, model, args):
    """Xử lý nếu input là ảnh"""
    frame = cv2.imread(image_path)
    if frame is None:
        print(f"[ERROR] Không đọc được ảnh: {image_path}")
        return

    results = model(frame, conf=args.conf, imgsz=args.imgsz, device=args.device, verbose=False)[0]
    detections = sv.Detections.from_ultralytics(results)

    # Lọc phương tiện
    vehicle_mask = [cls in VEHICLE_CLASSES for cls in detections.class_id]
    detections = detections[vehicle_mask]

    # Annotate
    box_annotator = sv.BoxAnnotator(thickness=2)
    frame = box_annotator.annotate(scene=frame, detections=detections)

    # Đếm
    current_counts = Counter()
    for cls in detections.class_id:
        current_counts[VEHICLE_CLASSES.get(cls, str(cls))] += 1
    total = sum(current_counts.values())

    # Ghi text
    y0 = 30
    cv2.putText(frame, f"Tổng xe: {total}", (10, y0),
                cv2.FONT_HERSHEY_SIMPLEX, 0.9, (0, 255, 0), 2)
    y0 += 30
    for k, v in current_counts.items():
        cv2.putText(frame, f"{k}: {v}", (10, y0),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.8, (255, 255, 255), 2)
        y0 += 25

    # Lưu ảnh output
    cv2.imwrite(args.save_out, frame)
    print(f"[INFO] Đã lưu ảnh kết quả tại: {args.save_out}")
    print("=== KẾT QUẢ ===")
    for k, v in current_counts.items():
        print(f"{k}: {v}")
    print(f"Tổng phương tiện: {total}")

    # Hiển thị nếu cần
    if args.show:
        resized = cv2.resize(frame, (960, 540))
        cv2.imshow("Vehicle Counter", resized)
        cv2.waitKey(0)
        cv2.destroyAllWindows()


def process_video_or_webcam(source, model, args):
    """Xử lý video hoặc webcam"""
    tracker = sv.ByteTrack()
    global total_vehicle
    cap = cv2.VideoCapture(source)
    if not cap.isOpened():
        print(f"[ERROR] Không mở được nguồn: {source}")
        return

    writer = None
    if args.save_out and not str(source).isdigit():
        fourcc = cv2.VideoWriter_fourcc(*"mp4v")
        fps = cap.get(cv2.CAP_PROP_FPS) or 20
        w, h = int(cap.get(3)), int(cap.get(4))
        writer = cv2.VideoWriter(args.save_out, fourcc, fps, (w, h))

    current_counts = Counter()

    print("[INFO] Bắt đầu xử lý... nhấn 'q' để thoát.")

    while True:
        ret, frame = cap.read()
        if not ret:
            break

        results = model(frame, conf=args.conf, imgsz=args.imgsz, device=args.device, verbose=False)[0]
        detections = sv.Detections.from_ultralytics(results)

        # Lọc phương tiện
        vehicle_mask = [cls in VEHICLE_CLASSES for cls in detections.class_id]
        detections = detections[vehicle_mask]
        detections = tracker.update_with_detections(detections)

        # Annotate
        box_annotator = sv.BoxAnnotator(thickness=2)
        frame = box_annotator.annotate(scene=frame, detections=detections)

        # Hiển thị ID
        for xyxy, cls_id, track_id in zip(detections.xyxy, detections.class_id, detections.tracker_id):
            x1, y1, x2, y2 = map(int, xyxy)
            label = f"{VEHICLE_CLASSES.get(cls_id, 'other')} #{track_id}"
            cv2.putText(frame, label, (x1, y1 - 8),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 255, 0), 2)

        # Đếm
        current_counts.clear()
        for cls in detections.class_id:
            current_counts[VEHICLE_CLASSES.get(cls, str(cls))] += 1
        total = sum(current_counts.values())

        # Ghi tổng
        y0 = 30
        cv2.putText(frame, f"Tổng xe trong khung: {total}", (10, y0),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.8, (0, 255, 0), 2)
        y0 += 25
        for k, v in current_counts.items():
            cv2.putText(frame, f"{k}: {v}", (10, y0),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.7, (255, 255, 255), 2)
            y0 += 25

        # Hiển thị nhỏ lại cho nhẹ
        display = cv2.resize(frame, (960, 540))
        if args.show:
            cv2.imshow("Vehicle Counter", display)
            if cv2.waitKey(1) & 0xFF == ord("q"):
                break

        if writer:
            writer.write(frame)

    cap.release()
    if writer:
        writer.release()
        print(f"[INFO] Video output: {args.save_out}")
    if args.show:
        cv2.destroyAllWindows()

    print("=== KẾT QUẢ CUỐI ===")
    for k, v in current_counts.items():
        print(f"{k}: {v}")
    print(f"Tổng phương tiện cuối khung: {total}")
    total_vehicle = total  # Cập nhật giá trị global

def main():
    args = parse_args()
    model = YOLO(args.model)
    model.to(args.device)
    # Khởi chạy API song song trong thread
    api_port = 8000 if str(args.source) == "0" else 8001
    api_thread = threading.Thread(target=start_api, args=(api_port,), daemon=True)
    api_thread.start()
    # Kiểm tra nếu source là ảnh
    if isinstance(args.source, str) and os.path.splitext(args.source)[1].lower() in [".jpg", ".png", ".jpeg"]:
        process_image(args.source, model, args)
    else:
        # Nếu là webcam (số) hoặc video
        src = int(args.source) if args.source.isdigit() else args.source
        process_video_or_webcam(src, model, args)

if __name__ == "__main__":
    main()

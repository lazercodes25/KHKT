#!/usr/bin/env python3
"""
smart_vehicle_counter.py
- Hỗ trợ cả ảnh và video (tự động nhận dạng)
- YOLOv8 + tracking đơn giản bằng IoU
- GPU (Jetson Nano / CUDA) + FP16 inference
Usage:
  python smart_vehicle_counter.py --source traffic.mp4
  python smart_vehicle_counter.py --source frame.jpg
"""

import os
import time
import argparse
from pathlib import Path
import cv2
import numpy as np
import torch
from ultralytics import YOLO

# ---------------- CONFIG ----------------
IMG_SHORT_SIDE = 640
CONF_THRESH = 0.35
IOU_TRACK_THRESH = 0.35
MAX_LOST_FRAMES = 6
COUNT_LINE_Y_RATIO = 0.6
VEHICLE_CLASSES = {"car", "motorbike", "truck", "bus", "bicycle"}
# ----------------------------------------

parser = argparse.ArgumentParser()
parser.add_argument('--source', type=str, required=True, help='Video file, image, or folder')
parser.add_argument('--device', type=str, default='auto', help='cpu or cuda:0')
parser.add_argument('--model', type=str, default='yolov8n.pt', help='YOLOv8 model')
parser.add_argument('--save', type=str, default=None, help='Output video path')
parser.add_argument('--fps', type=float, default=None, help='Playback fps for folder')
parser.add_argument('--night', action='store_true', help='Enable night enhancement (CLAHE)')
parser.add_argument('--display', action='store_true', help='Show preview window')
parser.add_argument('--imgsz', type=int, default=IMG_SHORT_SIDE, help='Resize short side')
parser.add_argument('--conf', type=float, default=CONF_THRESH, help='Confidence threshold')
parser.add_argument('--maxdet', type=int, default=50, help='Max detections per frame')
parser.add_argument('--save-frames', action='store_true', help='Save processed frames')
args = parser.parse_args()

# ---------------- DEVICE ----------------
if args.device == 'auto':
    args.device = 'cuda:0' if torch.cuda.is_available() else 'cpu'
print(f"Using device: {args.device}")

# ---------------- MODEL ----------------
model = YOLO(args.model)
if args.device.startswith('cuda') and torch.cuda.is_available():
    model.to('cuda')
    model.float()  # FP16 for Jetson Nano
else:
    model.to('cpu')
    model.float()

try:
    names = {int(k): v for k, v in model.names.items()}
except Exception:
    names = {}

# ---------------- HELPERS ----------------
def night_preprocess(frame):
    lab = cv2.cvtColor(frame, cv2.COLOR_BGR2LAB)
    l, a, b = cv2.split(lab)
    clahe = cv2.createCLAHE(clipLimit=2.0, tileGridSize=(8,8))
    cl = clahe.apply(l)
    limg = cv2.merge((cl,a,b))
    return cv2.cvtColor(limg, cv2.COLOR_LAB2BGR)

def resize_short_side(frame, short_side):
    h, w = frame.shape[:2]
    scale = short_side / min(h, w)
    return cv2.resize(frame, (int(w * scale), int(h * scale)))

def iou(a, b):
    xA, yA, xB, yB = max(a[0], b[0]), max(a[1], b[1]), min(a[2], b[2]), min(a[3], b[3])
    inter = max(0, xB-xA) * max(0, yB-yA)
    areaA = max(0, (a[2]-a[0])*(a[3]-a[1]))
    areaB = max(0, (b[2]-b[0])*(b[3]-b[1]))
    return inter / (areaA + areaB - inter + 1e-6)

# --- Tracking ---
tracks = {}
next_id = 0
def update_tracks(dets, ts):
    global next_id
    matched = set()
    for d in dets:
        best_iou, best_id = 0, None
        for tid, t in tracks.items():
            ov = iou(d['box'], t['box'])
            if ov > best_iou and ov > IOU_TRACK_THRESH:
                best_iou, best_id = ov, tid
        if best_id is not None:
            tracks[best_id].update({'box': d['box'], 'last': ts, 'lost': 0})
            d['id'] = best_id
            cx = (d['box'][0]+d['box'][2])//2
            cy = (d['box'][1]+d['box'][3])//2
            tracks[best_id]['centroid'] = (cx, cy)
            matched.add(best_id)
        else:
            tid = next_id; next_id += 1
            cx = (d['box'][0]+d['box'][2])//2
            cy = (d['box'][1]+d['box'][3])//2
            tracks[tid] = {'box': d['box'], 'centroid': (cx, cy), 'last': ts, 'lost': 0, 'counted': False}
            d['id'] = tid
            matched.add(tid)
    for tid in list(tracks.keys()):
        if tid not in matched:
            tracks[tid]['lost'] += 1
        if tracks[tid]['lost'] > MAX_LOST_FRAMES:
            del tracks[tid]

def check_cross(t, y_line):
    cy = t['centroid'][1]; prev_y = t.get('prev_y')
    t['prev_y'] = cy
    if prev_y is None: return False
    if prev_y < y_line <= cy and not t['counted']:
        t['counted'] = True
        return True
    return False

def count_static_image(dets, line_y):
    count = 0
    for d in dets:
        y1, y2 = d['box'][1], d['box'][3]
        if y2 >= line_y:  # object nằm dưới hoặc chạm đường đỏ
            count += 1
    return count

# ---------------- FRAME GENERATOR ----------------
def frame_generator(source, sim_fps=None):
    p = Path(source)
    if p.is_dir():
        exts = ('.jpg', '.jpeg', '.png', '.bmp')
        files = sorted([str(x) for x in p.iterdir() if x.suffix.lower() in exts])
        if not files:
            raise RuntimeError(f"No images found in {source}")
        delay = 1.0 / sim_fps if sim_fps else 0.2
        for f in files:
            img = cv2.imread(f)
            if img is None: continue
            yield img
            time.sleep(delay)
    else:
        cap = cv2.VideoCapture(str(source))
        if not cap.isOpened():
            raise RuntimeError(f"Cannot open video: {source}")
        fps = cap.get(cv2.CAP_PROP_FPS) or 25.0
        delay = 1.0 / (sim_fps or fps)
        while True:
            ret, frame = cap.read()
            if not ret: break
            yield frame
            time.sleep(delay)
        cap.release()

# ---------------- MAIN ----------------
def main():
    p = Path(args.source)
    is_image_mode = p.is_file() and p.suffix.lower() in ['.jpg', '.png', '.jpeg']

    # --- Image Mode ---
    if is_image_mode:
        frame = cv2.imread(str(p))
        if args.night:
            frame = night_preprocess(frame)
        frame = resize_short_side(frame, args.imgsz)
        h, w = frame.shape[:2]
        line_y = int(h * COUNT_LINE_Y_RATIO)

        results = model.predict(frame, device=args.device, imgsz=args.imgsz, conf=args.conf, verbose=False)
        res = results[0]
        boxes = res.boxes.xyxy.cpu().numpy()
        cls = res.boxes.cls.cpu().numpy()
        dets = []
        for i, b in enumerate(boxes):
            cls_name = names.get(int(cls[i]), str(cls[i]))
            if cls_name.lower() in VEHICLE_CLASSES:
                dets.append({'box': [int(b[0]), int(b[1]), int(b[2]), int(b[3])], 'class': cls_name})

        total = count_static_image(dets, line_y)
        for d in dets:
            x1,y1,x2,y2 = d['box']
            cv2.rectangle(frame, (x1,y1), (x2,y2), (0,255,0), 2)
            cv2.putText(frame, d['class'], (x1, y1-5), cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255,255,0), 2)
        cv2.line(frame, (0,line_y), (w,line_y), (0,0,255), 2)
        cv2.putText(frame, f"Count: {total}", (10,30), cv2.FONT_HERSHEY_SIMPLEX, 1, (0,255,255), 2)
        cv2.imwrite("output_static.jpg", frame)
        print(f"✅ Counted {total} vehicles in image. Saved as output_static.jpg")
        if args.display:
            cv2.imshow("Result", frame)
            cv2.waitKey(0)
            cv2.destroyAllWindows()
        return

    # --- Video Mode ---
    gen = frame_generator(args.source, sim_fps=args.fps)
    out_writer = None
    total, frames = 0, 0
    t0 = time.time()
    line_y = None

    for frame in gen:
        frames += 1
        if args.night:
            frame = night_preprocess(frame)
        frame = resize_short_side(frame, args.imgsz)
        h, w = frame.shape[:2]
        if line_y is None:
            line_y = int(h * COUNT_LINE_Y_RATIO)
            if args.save:
                fourcc = cv2.VideoWriter_fourcc(*'mp4v')
                out_writer = cv2.VideoWriter(args.save, fourcc, args.fps or 20, (w, h))

        results = model.predict(frame, device=args.device, imgsz=args.imgsz, conf=args.conf, max_det=args.maxdet, verbose=False)
        res = results[0]
        boxes = res.boxes.xyxy.cpu().numpy()
        cls = res.boxes.cls.cpu().numpy()
        dets = []
        for i, b in enumerate(boxes):
            cls_name = names.get(int(cls[i]), str(cls[i]))
            if cls_name.lower() in VEHICLE_CLASSES:
                dets.append({'box': [int(b[0]), int(b[1]), int(b[2]), int(b[3])], 'class': cls_name})

        update_tracks(dets, time.time())
        vis = frame.copy()
        for d in dets:
            x1,y1,x2,y2 = d['box']
            tid = d.get('id', -1)
            cv2.rectangle(vis, (x1,y1), (x2,y2), (0,255,0), 2)
            cv2.putText(vis, f"ID{tid} {d['class']}", (x1, y1-5), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255,255,0), 1)

        for tid, t in list(tracks.items()):
            if check_cross(t, line_y):
                total += 1
                print(f"[INFO] Counted ID{tid} Total={total}")
            cx, cy = t['centroid']
            cv2.circle(vis, (int(cx), int(cy)), 3, (0,255,255), -1)

        cv2.line(vis, (0,line_y), (w,line_y), (0,0,255), 2)
        cv2.putText(vis, f"Count: {total}", (10,30), cv2.FONT_HERSHEY_SIMPLEX, 1, (0,255,255), 2)

        if args.display:
            cv2.imshow("Vehicle Counter", vis)
            if cv2.waitKey(1) & 0xFF == ord('q'):
                break
        if out_writer:
            out_writer.write(vis)
        if args.save_frames:
            os.makedirs("processed_frames", exist_ok=True)
            cv2.imwrite(os.path.join("processed_frames", f"frame_{frames:05d}.jpg"), vis)

    elapsed = max(1e-3, time.time() - t0)
    print(f"Processed {frames} frames, {total} vehicles, avg FPS={frames/elapsed:.2f}")
    if out_writer: out_writer.release()
    if args.display: cv2.destroyAllWindows()

if __name__ == '__main__':
    main()

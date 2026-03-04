from ultralytics import YOLO
import cv2
import numpy as np
import time

# Load model YOLOv11 bạn đã train
model = YOLO(r"D:\ĐÒ ÁN TỐT NGHIỆP\PHẦN AI\weights\best.pt")  
# Mở webcam
cap = cv2.VideoCapture(0)  # Đổi số nếu bạn dùng nhiều webcam

while True:
    ret, frame = cap.read()
    if not ret:
        break

    # Resize nếu cần (YOLO thường dùng 640x640)
    resized = cv2.resize(frame, (640, 640))

    # Nhận diện bằng YOLO
    results = model.predict(source=resized, conf=0.5, save=False, verbose=False)

    for r in results:
        boxes = r.boxes
        for box in boxes:
            # Lấy thông tin bounding box
            x1, y1, x2, y2 = map(int, box.xyxy[0])
            cls_id = int(box.cls[0])
            conf = float(box.conf[0])
            label = model.names[cls_id]

            # Cắt ảnh lá ra từ khung hình
            leaf_crop = frame[y1:y2, x1:x2]

            # ====== TIỀN XỬ LÝ ẢNH LÁ ======
            gray = cv2.cvtColor(leaf_crop, cv2.COLOR_BGR2GRAY)
            enhanced = cv2.equalizeHist(gray)  # Cân bằng sáng
            sharpen_kernel = np.array([[-1,-1,-1], [-1,9,-1], [-1,-1,-1]])
            sharpened = cv2.filter2D(enhanced, -1, sharpen_kernel)

            # Ghép lại ảnh lá vào góc để xem tiền xử lý
            preview = cv2.resize(sharpened, (100,100))
            frame[10:110, 10:110] = cv2.cvtColor(preview, cv2.COLOR_GRAY2BGR)

            # Hiển thị thông tin trên khung hình
            cv2.rectangle(frame, (x1, y1), (x2, y2), (0,255,0), 2)
            cv2.putText(frame, f"{label} {conf:.2f}", (x1, y1-10),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0,255,0), 2)

    # Hiển thị kết quả
    cv2.imshow("YOLOv11 Detection", frame)
    if cv2.waitKey(1) == ord("q"):
        break

cap.release()
cv2.destroyAllWindows()

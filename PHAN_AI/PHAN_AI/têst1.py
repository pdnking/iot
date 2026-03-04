import cv2
import numpy as np
from ultralytics import YOLO
import telegram
import asyncio
from io import BytesIO
import os
import tkinter as tk
from tkinter import filedialog, messagebox, ttk
from PIL import Image, ImageTk
from pathlib import Path
import json
import shutil
from datetime import datetime

class Config:
    MODEL_PATH = r"C:\doantn\PHAN_AI\PHAN_AI\train1\weights\best.pt"
    TELEGRAM_BOT_TOKEN = "8592609529:AAFhrXT9hWyqpVgq5oCm5jOtWBv8wYfOI3s"
    TELEGRAM_CHAT_ID = "8592609529"
    IMAGE_FOLDER = r"C:\doantn\PHAN_AI\PHAN_AI\testanh"
    OUTPUT_DIR = "unhealthy_crops"
    TEMP_IMAGE = "temp_camera.jpg"
    CONFIDENCE_THRESHOLD = 0.3
    IOU_THRESHOLD = 0.6
    MAX_DETECTIONS = 100
    CANVAS_SIZE = (500, 500)
    SUPPORTED_FORMATS = ('.jpg', '.jpeg', '.png', '.bmp', '.tiff')
    VALID_LABELS = ['healthy', 'unhealthy', 'mature', 'not mature']
    LABEL_COLORS = {
        'healthy': (0, 255, 0),
        'unhealthy': (0, 0, 255),
        'mature': (255, 0, 0),
        'not mature': (255, 255, 0)
    }

class DetectionResult:
    def __init__(self, label: str, confidence: float, bbox: tuple, image_path: str = None):
        self.label = label
        self.confidence = confidence
        self.bbox = bbox
        self.image_path = image_path

class ModelManager:
    def __init__(self, model_path: str):
        self.model = YOLO(model_path)

    def predict(self, image: np.ndarray, conf: float = 0.3, iou: float = 0.6, max_det: int = 100) -> list:
        results = self.model(image, conf=conf, iou=iou, max_det=max_det, agnostic_nms=True)
        detections = []
        for result in results:
            if result.boxes is None:
                continue
            for box in result.boxes:
                x1, y1, x2, y2 = map(int, box.xyxy[0].cpu().numpy())
                confidence = float(box.conf[0].cpu().numpy())
                cls_id = int(box.cls[0].cpu().numpy())
                label = self.model.names[cls_id]
                detections.append(DetectionResult(label, confidence, (x1, y1, x2, y2)))
        return detections

class TelegramManager:
    def __init__(self, token: str, chat_id: str):
        self.bot = telegram.Bot(token=token)
        self.chat_id = chat_id

    async def send_notification(self, image: np.ndarray, message: str) -> bool:
        try:
            _, buffer = cv2.imencode('.jpg', image)
            img_bytes = BytesIO(buffer)
            img_bytes.seek(0)
            await self.bot.send_photo(chat_id=self.chat_id, photo=img_bytes, caption=message, parse_mode='HTML')
            return True
        except Exception as e:
            print(f"Lỗi khi gửi Telegram: {e}")
            return False

class CameraManager:
    def __init__(self):
        self.cap = None
        self.camera_index = -1

    def find_available_camera(self, max_index: int = 20) -> bool:
        for index in range(max_index):
            cap = cv2.VideoCapture(index, cv2.CAP_DSHOW)
            if cap.isOpened():
                ret, frame = cap.read()
                if ret and frame is not None:
                    self.cap = cap
                    self.camera_index = index
                    return True
                cap.release()
        return False

    def read_frame(self) -> np.ndarray:
        if self.cap and self.cap.isOpened():
            ret, frame = self.cap.read()
            return frame if ret else None
        return None

    def release(self) -> None:
        if self.cap:
            self.cap.release()
            self.cap = None
            cv2.destroyAllWindows()

class ImageProcessor:
    def __init__(self, config: Config, model_manager: ModelManager, telegram_manager: TelegramManager):
        self.config = config
        self.model_manager = model_manager
        self.telegram_manager = telegram_manager

    def has_valid_plants(self, detections: list) -> bool:
        return any(d.label in self.config.VALID_LABELS for d in detections)

    def draw_detections(self, image: np.ndarray, detections: list) -> np.ndarray:
        result_image = image.copy()
        for detection in detections:
            if detection.label not in self.config.VALID_LABELS:
                continue
            x1, y1, x2, y2 = detection.bbox
            color = self.config.LABEL_COLORS.get(detection.label, (128, 128, 128))
            cv2.rectangle(result_image, (x1, y1), (x2, y2), color, 2)
            label_text = f"{detection.label} {detection.confidence:.2f}"
            cv2.putText(result_image, label_text, (x1, y1-10), cv2.FONT_HERSHEY_SIMPLEX, 0.5, color, 2)
        return result_image

    def extract_unhealthy_crops(self, image: np.ndarray, detections: list, output_dir: str, base_name: str = "crop") -> list:
        unhealthy_paths = []
        crop_count = 0
        os.makedirs(output_dir, exist_ok=True)
        for detection in detections:
            if detection.label != 'unhealthy' or detection.confidence <= 0.5:
                continue
            x1, y1, x2, y2 = detection.bbox
            crop = image[y1:y2, x1:x2]
            if crop.shape[0] > 10 and crop.shape[1] > 10:
                crop_filename = os.path.join(output_dir, f'{base_name}_unhealthy_{crop_count}.jpg')
                cv2.imwrite(crop_filename, crop)
                unhealthy_paths.append(crop_filename)
                crop_count += 1
        return unhealthy_paths

    async def process_single_image(self, image_path: str, send_telegram: bool = False) -> tuple:
        image = cv2.imread(image_path)
        if image is None:
            return None, [], []

        detections = self.model_manager.predict(
            image, self.config.CONFIDENCE_THRESHOLD, self.config.IOU_THRESHOLD, self.config.MAX_DETECTIONS
        )

        if not self.has_valid_plants(detections):
            return None, [], []

        result_image = self.draw_detections(image, detections)
        base_name = Path(image_path).stem
        unhealthy_paths = self.extract_unhealthy_crops(image, detections, self.config.OUTPUT_DIR, base_name)

        if send_telegram:
            unhealthy_detections = [d for d in detections if d.label == 'unhealthy']
            if unhealthy_detections:
                message = f"🚨 <b>CẢNH BÁO: Phát hiện cây không khỏe!</b>\n"
                message += f"📁 File: <code>{Path(image_path).name}</code>\n"
                message += f"🔍 Tìm thấy {len(unhealthy_detections)} vùng không khỏe:\n\n"
                for i, d in enumerate(unhealthy_detections, 1):
                    message += f"• Vùng {i}: <b>{d.label}</b> (tin cậy: {d.confidence:.2f})\n"
                message += f"\n📊 Tổng số vùng đã cắt: {len(unhealthy_paths)}"
                success = await self.telegram_manager.send_notification(result_image, message)
                if not success:
                    print(f"Không thể gửi thông báo Telegram cho {image_path}")

        return result_image, unhealthy_paths, detections

    async def process_folder(self, folder_path: str, send_telegram: bool = False) -> tuple:
        folder = Path(folder_path)
        image_files = [f for f in folder.iterdir() if f.suffix.lower() in self.config.SUPPORTED_FORMATS]
        if not image_files:
            return [], []

        all_unhealthy_paths = []
        all_detections = []

        for image_file in image_files:
            result_image, unhealthy_paths, detections = await self.process_single_image(str(image_file), send_telegram)
            all_unhealthy_paths.extend(unhealthy_paths)
            for d in detections:
                d.image_path = str(image_file)
            all_detections.extend(detections)

        return all_unhealthy_paths, all_detections

class PlantDiseaseGUI:
    def __init__(self, root: tk.Tk, config: Config):
        self.root = root
        self.config = config
        self.model_manager = ModelManager(config.MODEL_PATH)
        self.telegram_manager = TelegramManager(config.TELEGRAM_BOT_TOKEN, config.TELEGRAM_CHAT_ID)
        self.camera_manager = CameraManager()
        self.image_processor = ImageProcessor(config, self.model_manager, self.telegram_manager)
        self.image_path = None
        self.result_image = None
        self.unhealthy_crops = []
        self.detections = []
        self.current_crop_index = 0
        self.camera_active = False
        self._setup_ui()
        self.root.protocol("WM_DELETE_WINDOW", self._on_closing)

    def _setup_ui(self) -> None:
        self.root.title("Nhận diện sâu bệnh với YOLOv11")
        self.root.geometry("800x600")

        main_frame = ttk.Frame(self.root)
        main_frame.pack(fill='both', expand=True, padx=10, pady=10)

        canvas_frame = ttk.LabelFrame(main_frame, text="Hiển thị hình ảnh", padding=10)
        canvas_frame.pack(fill='x', pady=(0, 10))

        self.canvas = tk.Canvas(canvas_frame, width=self.config.CANVAS_SIZE[0], height=self.config.CANVAS_SIZE[1], bg="black")
        self.canvas.pack()

        self.status_label = ttk.Label(canvas_frame, text="Sẵn sàng", font=("Arial", 10))
        self.status_label.pack(pady=5)

        button_frame = ttk.LabelFrame(main_frame, text="Điều khiển", padding=10)
        button_frame.pack(fill='x', pady=(0, 10))

        buttons = [
            ("Mở camera", self._open_camera),
            ("Tải ảnh", self._load_image),
            ("Tải thư mục", self._load_folder),
            ("Chụp ảnh", self._capture_image),
            ("Nhận diện", self._run_detection),
            ("Ảnh tiếp theo", self._show_next_crop),
            ("Lưu kết quả", self._save_results),
            ("Dừng camera", self._stop_camera)
        ]
        for i, (text, command) in enumerate(buttons):
            btn = ttk.Button(button_frame, text=text, command=command, width=15)
            btn.grid(row=i//4, column=i%4, padx=5, pady=5)

        results_frame = ttk.LabelFrame(main_frame, text="Kết quả", padding=10)
        results_frame.pack(fill='both', expand=True)

        self.result_text = tk.Text(results_frame, height=10, font=("Consolas", 9))
        scrollbar = ttk.Scrollbar(results_frame, orient='vertical', command=self.result_text.yview)
        self.result_text.configure(yscrollcommand=scrollbar.set)
        self.result_text.pack(side='left', fill='both', expand=True)
        scrollbar.pack(side='right', fill='y')

    def _update_status(self, message: str) -> None:
        self.status_label.config(text=message)

    def _add_result_text(self, text: str) -> None:
        self.result_text.insert(tk.END, text + "\n")
        self.result_text.see(tk.END)

    def _clear_results(self) -> None:
        self.result_text.delete(1.0, tk.END)
        self.unhealthy_crops = []
        self.detections = []
        self.current_crop_index = 0

    def _display_image(self, image_data, is_pil: bool = False) -> None:
        if is_pil:
            img = image_data
        else:
            img = Image.fromarray(cv2.cvtColor(image_data, cv2.COLOR_BGR2RGB))
        img = img.resize(self.config.CANVAS_SIZE, Image.Resampling.LANCZOS)
        photo = ImageTk.PhotoImage(img)
        self.canvas.delete("all")  
        self.canvas.create_image(0, 0, anchor="nw", image=photo)
        self.canvas.image = photo

    def _load_image(self) -> None:
        self._stop_camera()
        file_path = filedialog.askopenfilename(filetypes=[("Image files", "*.jpg *.jpeg *.png *.bmp *.tiff")])
        if file_path:
            self.image_path = file_path
            self._clear_results()
            img = Image.open(file_path)
            self._display_image(img, is_pil=True)
            self._update_status("Ảnh đã tải - Nhấn 'Nhận diện' để xử lý")
            self._add_result_text(f"Đã tải ảnh: {Path(file_path).name}")

    def _load_folder(self) -> None:
        self._stop_camera()
        folder_path = filedialog.askdirectory()
        if folder_path:
            self.image_path = folder_path
            self._clear_results()
            folder = Path(folder_path)
            image_count = len([f for f in folder.iterdir() if f.suffix.lower() in self.config.SUPPORTED_FORMATS])
            self._update_status(f"Đã chọn thư mục - {image_count} ảnh")
            self._add_result_text(f"Đã chọn thư mục: {folder_path}")
            self._add_result_text(f"Tìm thấy {image_count} ảnh hợp lệ")

    def _open_camera(self) -> None:
        self._stop_camera()
        self._clear_results()
        if not self.camera_manager.find_available_camera():
            messagebox.showerror("Lỗi", "Không tìm thấy camera!")
            self._update_status("Không thể mở camera")
            return
        self.camera_active = True
        self._update_status(f"Camera đã mở (chỉ số {self.camera_manager.camera_index})")
        self._camera_loop()

    def _camera_loop(self) -> None:
        if not self.camera_active:
            return
        frame = self.camera_manager.read_frame()
        if frame is not None:
            self._display_image(frame)
            self.root.after(90, self._camera_loop)

    def _stop_camera(self) -> None:
        self.camera_active = False
        self.camera_manager.release()
        self.canvas.delete("all")
        self._update_status("Camera đã dừng")
        self._add_result_text("Camera đã dừng")

    def _capture_image(self) -> None:
        if not self.camera_active:
            messagebox.showerror("Lỗi", "Vui lòng mở camera trước!")
            return
        frame = self.camera_manager.read_frame()
        if frame is None:
            messagebox.showerror("Lỗi", "Không thể chụp ảnh từ camera!")
            return
        cv2.imwrite(self.config.TEMP_IMAGE, frame)
        self.image_path = self.config.TEMP_IMAGE
        self._stop_camera()
        self._clear_results()
        self._display_image(frame)
        self._update_status("Đã chụp ảnh - Nhấn 'Nhận diện' để xử lý")
        self._add_result_text("Đã chụp ảnh từ camera")

    def _run_detection(self) -> None:
        if not self.image_path:
            messagebox.showerror("Lỗi", "Vui lòng chọn ảnh, thư mục, hoặc chụp ảnh từ camera!")
            return
        self._clear_results()
        self._update_status("Đang xử lý...")
        if os.path.isfile(self.image_path):
            result_image, unhealthy_paths, detections = asyncio.run(self.image_processor.process_single_image(self.image_path, send_telegram=True))
            if result_image is None:
                self._add_result_text("Không phát hiện rau cải trong ảnh")
                self._update_status("Không tìm thấy cây")
                return
            self.result_image = result_image
            self.unhealthy_crops = unhealthy_paths
            self.detections = detections
            self._display_image(result_image)
            unhealthy_count = len([d for d in detections if d.label == 'unhealthy'])
            healthy_count = len([d for d in detections if d.label == 'healthy'])
            mature_count = len([d for d in detections if d.label == 'mature'])
            not_mature_count = len([d for d in detections if d.label == 'not mature'])
            total_plants = len(detections)
            self._add_result_text(f"=== KẾT QUẢ NHẬN DIỆN ===")
            self._add_result_text(f"Tổng số vùng phát hiện: {total_plants}")
            self._add_result_text(f"• Khỏe mạnh: {healthy_count}")
            self._add_result_text(f"• Không khỏe: {unhealthy_count}")
            self._add_result_text(f"• Chín: {mature_count}")
            self._add_result_text(f"• Chưa chín: {not_mature_count}")
            self._add_result_text(f"Số ảnh unhealthy đã cắt: {len(unhealthy_paths)}")
            if unhealthy_count > 0:
                self._add_result_text(f"⚠️ CẢNH BÁO: Phát hiện {unhealthy_count} vùng không khỏe!")
                self._add_result_text("✅ Đã gửi thông báo qua Telegram")
            self._update_status(f"Hoàn tất - Phát hiện {total_plants} vùng")
        elif os.path.isdir(self.image_path):
            unhealthy_paths, all_detections = asyncio.run(self.image_processor.process_folder(self.image_path, send_telegram=False))
            self.unhealthy_crops = unhealthy_paths
            self.detections = all_detections
            if not all_detections:
                self._add_result_text("Không phát hiện rau cải trong thư mục")
                self._update_status("Không tìm thấy cây")
                return
            total_images = len(set(d.image_path for d in all_detections if d.image_path))
            unhealthy_count = len([d for d in all_detections if d.label == 'unhealthy'])
            healthy_count = len([d for d in all_detections if d.label == 'healthy'])
            mature_count = len([d for d in all_detections if d.label == 'mature'])
            not_mature_count = len([d for d in all_detections if d.label == 'not mature'])
            self._add_result_text(f"=== KẾT QUẢ XỬ LÝ THƯ MỤC ===")
            self._add_result_text(f"Số ảnh đã xử lý: {total_images}")
            self._add_result_text(f"Tổng số vùng phát hiện: {len(all_detections)}")
            self._add_result_text(f"• Khỏe mạnh: {healthy_count}")
            self._add_result_text(f"• Không khỏe: {unhealthy_count}")
            self._add_result_text(f"• Lớn: {mature_count}")
            self._add_result_text(f"• Chưa Lớn: {not_mature_count}")
            self._add_result_text(f"Số ảnh unhealthy đã cắt: {len(unhealthy_paths)}")
            if unhealthy_count > 0:
                self._add_result_text(f"⚠️ CẢNH BÁO: Tổng cộng {unhealthy_count} vùng không khỏe!")
            self._update_status(f"Hoàn tất - Xử lý {total_images} ảnh")

    def _show_next_crop(self) -> None:
        if not self.unhealthy_crops:
            messagebox.showinfo("Thông báo", "Không có ảnh unhealthy nào để hiển thị!")
            return
        crop_path = self.unhealthy_crops[self.current_crop_index]
        if os.path.exists(crop_path):
            img = Image.open(crop_path)
            self._display_image(img, is_pil=True)
            self._update_status(f"Ảnh unhealthy {self.current_crop_index + 1}/{len(self.unhealthy_crops)}")
            self._add_result_text(f"Hiển thị: {Path(crop_path).name}")
        else:
            self._add_result_text(f"Không thể mở ảnh: {crop_path}")
        self.current_crop_index = (self.current_crop_index + 1) % len(self.unhealthy_crops)

    def _save_results(self) -> None:
        if not self.detections:
            messagebox.showinfo("Thông báo", "Không có kết quả để lưu!")
            return
        save_dir = filedialog.askdirectory(title="Chọn thư mục lưu kết quả")
        if not save_dir:
            return
        try:
            timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
            result_dir = os.path.join(save_dir, f"plant_detection_results_{timestamp}")
            os.makedirs(result_dir, exist_ok=True)
            if self.result_image is not None:
                result_image_path = os.path.join(result_dir, "detection_result.jpg")
                cv2.imwrite(result_image_path, self.result_image)
                self._add_result_text(f"Đã lưu ảnh kết quả: {result_image_path}")
            results_data = {
                "timestamp": timestamp,
                "source": self.image_path,
                "total_detections": len(self.detections),
                "statistics": {
                    "healthy": len([d for d in self.detections if d.label == 'healthy']),
                    "unhealthy": len([d for d in self.detections if d.label == 'unhealthy']),
                    "mature": len([d for d in self.detections if d.label == 'mature']),
                    "not_mature": len([d for d in self.detections if d.label == 'not mature'])
                },
                "detections": [
                    {
                        "label": d.label,
                        "confidence": d.confidence,
                        "bbox": d.bbox
                    } for d in self.detections
                ],
                "unhealthy_crops_saved": len(self.unhealthy_crops)
            }
            json_path = os.path.join(result_dir, "detection_results.json")
            with open(json_path, 'w', encoding='utf-8') as f:
                json.dump(results_data, f, ensure_ascii=False, indent=2)
            if self.unhealthy_crops:
                crops_dir = os.path.join(result_dir, "unhealthy_crops")
                os.makedirs(crops_dir, exist_ok=True)
                for crop_path in self.unhealthy_crops:
                    if os.path.exists(crop_path):
                        crop_name = Path(crop_path).name
                        new_crop_path = os.path.join(crops_dir, crop_name)
                        shutil.copy2(crop_path, new_crop_path)
            self._add_result_text(f"✅ Đã lưu kết quả vào: {result_dir}")
            messagebox.showinfo("Thành công", f"Đã lưu kết quả vào:\n{result_dir}")
        except Exception as e:
            messagebox.showerror("Lỗi", f"Không thể lưu kết quả: {e}")

    def _on_closing(self) -> None:
        self._stop_camera()
        if os.path.exists(self.config.TEMP_IMAGE):
            os.remove(self.config.TEMP_IMAGE)
        self.root.destroy()

def main():
    root = tk.Tk()
    app = PlantDiseaseGUI(root, Config())
    root.mainloop()

if __name__ == "__main__":
    main()
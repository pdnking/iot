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
import json
import logging
from concurrent.futures import ThreadPoolExecutor
import threading
from dataclasses import dataclass
from typing import List, Tuple, Optional, Dict, Any
from pathlib import Path

# Thiết lập logging
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

@dataclass
class Config:
    """Cấu hình ứng dụng"""
    MODEL_PATH: str = r"D:\DO AN TOT NGHIEP\PHAN_AI\weights\best.pt"
    TELEGRAM_BOT_TOKEN: str = "7854048371:AAHLrpp3ZxSeiNAGS49n8-W_Nx3iUeqFbT4"
    TELEGRAM_CHAT_ID: str = "-1002524466925"
    IMAGE_FOLDER: str = r"D:\DO AN TOT NGHIEP\PHAN_AI\testanh"
    OUTPUT_DIR: str = "unhealthy_crops"
    TEMP_IMAGE: str = "temp_camera.jpg"
    CONFIDENCE_THRESHOLD: float = 0.3
    IOU_THRESHOLD: float = 0.6
    MAX_DETECTIONS: int = 100
    CANVAS_SIZE: Tuple[int, int] = (500, 500)
    SUPPORTED_FORMATS: Tuple[str, ...] = ('.jpg', '.jpeg', '.png', '.bmp', '.tiff')
    VALID_LABELS: List[str] = None
    LABEL_COLORS: Dict[str, Tuple[int, int, int]] = None
    
    def __post_init__(self):
        if self.VALID_LABELS is None:
            self.VALID_LABELS = ['healthy', 'unhealthy', 'mature', 'not mature']
        if self.LABEL_COLORS is None:
            self.LABEL_COLORS = {
                'healthy': (0, 255, 0),
                'unhealthy': (0, 0, 255),
                'mature': (255, 0, 0),
                'not mature': (255, 255, 0)
            }

@dataclass
class DetectionResult:
    """Kết quả phát hiện"""
    label: str
    confidence: float
    bbox: Tuple[int, int, int, int]
    image_path: Optional[str] = None

class ModelManager:
    """Quản lý mô hình YOLO"""
    def __init__(self, model_path: str):
        self.model = None
        self.model_path = model_path
        self._load_model()
    
    def _load_model(self) -> None:
        """Tải mô hình YOLO"""
        try:
            if not Path(self.model_path).exists():
                raise FileNotFoundError(f"Không tìm thấy file mô hình: {self.model_path}")
            
            self.model = YOLO(self.model_path)
            logger.info("Đã tải mô hình YOLOv11 thành công")
        except Exception as e:
            logger.error(f"Lỗi khi tải mô hình YOLOv11: {e}")
            raise
    
    def predict(self, image: np.ndarray, conf: float = 0.3, iou: float = 0.6, max_det: int = 100) -> List[DetectionResult]:
        """Dự đoán với mô hình"""
        if self.model is None:
            raise RuntimeError("Mô hình chưa được tải")
        
        try:
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
                    
                    detections.append(DetectionResult(
                        label=label,
                        confidence=confidence,
                        bbox=(x1, y1, x2, y2)
                    ))
            
            return detections
        except Exception as e:
            logger.error(f"Lỗi trong quá trình dự đoán: {e}")
            return []

class TelegramManager:
    """Quản lý Telegram Bot"""
    def __init__(self, token: str, chat_id: str):
        self.bot = None
        self.chat_id = chat_id
        self._init_bot(token)
    
    def _init_bot(self, token: str) -> None:
        """Khởi tạo bot"""
        try:
            self.bot = telegram.Bot(token=token)
            logger.info("Đã khởi tạo Telegram Bot thành công")
        except Exception as e:
            logger.warning(f"Không thể khởi tạo Telegram Bot: {e}")
    
    async def send_notification(self, image: np.ndarray, message: str) -> bool:
        """Gửi thông báo qua Telegram"""
        if self.bot is None:
            logger.warning("Telegram Bot chưa được khởi tạo")
            return False
        
        try:
            # Encode ảnh
            _, buffer = cv2.imencode('.jpg', image)
            img_bytes = BytesIO(buffer)
            img_bytes.seek(0)  # Reset về đầu buffer
            
            # Gửi ảnh với caption
            await self.bot.send_photo(
                chat_id=self.chat_id, 
                photo=img_bytes, 
                caption=message,
                parse_mode='HTML'  # Hỗ trợ format HTML
            )
            logger.info(f"Đã gửi thông báo Telegram: {message}")
            return True
        except Exception as e:
            logger.error(f"Lỗi khi gửi Telegram: {e}")
            return False

class CameraManager:
    """Quản lý camera"""
    def __init__(self):
        self.cap = None
        self.camera_index = -1
    
    def find_available_camera(self, max_index: int = 20) -> bool:
        """Tìm camera khả dụng"""
        for index in range(max_index):
            cap = cv2.VideoCapture(index, cv2.CAP_DSHOW)
            if cap.isOpened():
                # Kiểm tra có thể đọc frame không
                ret, frame = cap.read()
                if ret and frame is not None:
                    self.cap = cap
                    self.camera_index = index
                    logger.info(f"Tìm thấy camera tại chỉ số {index}")
                    return True
                cap.release()
        return False
    
    def read_frame(self) -> Optional[np.ndarray]:
        """Đọc frame từ camera"""
        if self.cap is None or not self.cap.isOpened():
            return None
        
        ret, frame = self.cap.read()
        return frame if ret else None
    
    def release(self) -> None:
        """Giải phóng camera"""
        if self.cap is not None:
            self.cap.release()
            self.cap = None
            cv2.destroyAllWindows()
        self.camera_index = -1

class ImageProcessor:
    """Xử lý hình ảnh"""
    def __init__(self, config: Config, model_manager: ModelManager, telegram_manager: TelegramManager):
        self.config = config
        self.model_manager = model_manager
        self.telegram_manager = telegram_manager
        self.executor = ThreadPoolExecutor(max_workers=4)
    
    def has_valid_plants(self, detections: List[DetectionResult]) -> bool:
        """Kiểm tra có phát hiện cây hợp lệ không"""
        return any(detection.label in self.config.VALID_LABELS for detection in detections)
    
    def draw_detections(self, image: np.ndarray, detections: List[DetectionResult]) -> np.ndarray:
        """Vẽ các vùng phát hiện lên ảnh"""
        result_image = image.copy()
        
        for detection in detections:
            if detection.label not in self.config.VALID_LABELS:
                continue
                
            x1, y1, x2, y2 = detection.bbox
            color = self.config.LABEL_COLORS.get(detection.label, (128, 128, 128))
            
            # Vẽ bounding box
            cv2.rectangle(result_image, (x1, y1), (x2, y2), color, 2)
            
            # Vẽ label
            label_text = f"{detection.label} {detection.confidence:.2f}"
            cv2.putText(result_image, label_text, (x1, y1-10),
                       cv2.FONT_HERSHEY_SIMPLEX, 0.5, color, 2)
        
        return result_image
    
    def extract_unhealthy_crops(self, image: np.ndarray, detections: List[DetectionResult], 
                              output_dir: str, base_name: str = "crop") -> List[str]:
        """Cắt và lưu các vùng unhealthy"""
        unhealthy_paths = []
        crop_count = 0
        
        os.makedirs(output_dir, exist_ok=True)
        
        for detection in detections:
            if detection.label != 'unhealthy' or detection.confidence <= 0.5:
                continue
            
            x1, y1, x2, y2 = detection.bbox
            crop = image[y1:y2, x1:x2]
            
            # Kiểm tra kích thước hợp lệ
            if crop.shape[0] > 10 and crop.shape[1] > 10:
                crop_filename = os.path.join(output_dir, f'{base_name}_unhealthy_{crop_count}.jpg')
                cv2.imwrite(crop_filename, crop)
                unhealthy_paths.append(crop_filename)
                crop_count += 1
                logger.info(f"Đã lưu ảnh unhealthy: {crop_filename}")
        
        return unhealthy_paths
    
    async def process_single_image(self, image_path: str, send_telegram: bool = False) -> Tuple[Optional[np.ndarray], List[str], List[DetectionResult]]:
        """Xử lý một ảnh"""
        try:
            # Đọc ảnh
            image = cv2.imread(image_path)
            if image is None:
                logger.error(f"Không thể đọc ảnh: {image_path}")
                return None, [], []
            
            # Dự đoán
            detections = self.model_manager.predict(
                image, 
                conf=self.config.CONFIDENCE_THRESHOLD,
                iou=self.config.IOU_THRESHOLD,
                max_det=self.config.MAX_DETECTIONS
            )
            
            # Kiểm tra có cây hợp lệ không
            if not self.has_valid_plants(detections):
                logger.info(f"Không phát hiện rau cải trong {image_path}")
                return None, [], []
            
            # Vẽ kết quả
            result_image = self.draw_detections(image, detections)
            
            # Cắt vùng unhealthy
            base_name = Path(image_path).stem
            unhealthy_paths = self.extract_unhealthy_crops(
                image, detections, self.config.OUTPUT_DIR, base_name
            )
            
            # Gửi Telegram nếu có unhealthy và được yêu cầu
            if send_telegram:
                unhealthy_detections = [d for d in detections if d.label == 'unhealthy']
                if unhealthy_detections:
                    try:
                        message = f"🚨 <b>CẢNH BÁO: Phát hiện cây không khỏe!</b>\n"
                        message += f"📁 File: <code>{Path(image_path).name}</code>\n"
                        message += f"🔍 Tìm thấy {len(unhealthy_detections)} vùng không khỏe:\n\n"
                        
                        for i, d in enumerate(unhealthy_detections, 1):
                            message += f"• Vùng {i}: <b>{d.label}</b> (tin cậy: {d.confidence:.2f})\n"
                        
                        message += f"\n📊 Tổng số vùng đã cắt: {len(unhealthy_paths)}"
                        
                        # Gửi thông báo
                        success = await self.telegram_manager.send_notification(result_image, message)
                        if success:
                            logger.info(f"Đã gửi thông báo Telegram cho {image_path}")
                        else:
                            logger.warning(f"Không thể gửi thông báo Telegram cho {image_path}")
                    except Exception as e:
                        logger.error(f"Lỗi khi gửi thông báo Telegram: {e}")
                else:
                    logger.info(f"Không có vùng unhealthy để gửi thông báo cho {image_path}")
            
            return result_image, unhealthy_paths, detections
            
        except Exception as e:
            logger.error(f"Lỗi khi xử lý ảnh {image_path}: {e}")
            return None, [], []
    
    async def process_folder(self, folder_path: str, send_telegram: bool = False) -> Tuple[List[str], List[DetectionResult]]:
        """Xử lý thư mục ảnh"""
        folder = Path(folder_path)
        image_files = [f for f in folder.iterdir() 
                      if f.suffix.lower() in self.config.SUPPORTED_FORMATS]
        
        if not image_files:
            logger.warning(f"Không tìm thấy ảnh hợp lệ trong {folder_path}")
            return [], []
        
        all_unhealthy_paths = []
        all_detections = []
        
        # Xử lý song song
        tasks = []
        for image_file in image_files:
            task = self.process_single_image(str(image_file), send_telegram)
            tasks.append(task)
        
        results = await asyncio.gather(*tasks, return_exceptions=True)
        
        for result in results:
            if isinstance(result, Exception):
                logger.error(f"Lỗi trong quá trình xử lý: {result}")
                continue
            
            _, unhealthy_paths, detections = result
            all_unhealthy_paths.extend(unhealthy_paths)
            all_detections.extend(detections)
        
        return all_unhealthy_paths, all_detections

class PlantDiseaseGUI:
    """Giao diện người dùng"""
    def __init__(self, root: tk.Tk, config: Config):
        self.root = root
        self.config = config
        
        # Khởi tạo các manager
        self.model_manager = ModelManager(config.MODEL_PATH)
        self.telegram_manager = TelegramManager(config.TELEGRAM_BOT_TOKEN, config.TELEGRAM_CHAT_ID)
        self.camera_manager = CameraManager()
        self.image_processor = ImageProcessor(config, self.model_manager, self.telegram_manager)
        
        # Trạng thái ứng dụng
        self.image_path = None
        self.result_image = None
        self.unhealthy_crops = []
        self.detections = []
        self.current_crop_index = 0
        self.photo_references = []
        self.camera_active = False
        self.camera_thread = None
        
        self._setup_ui()
        
        # Cleanup khi đóng ứng dụng
        self.root.protocol("WM_DELETE_WINDOW", self._on_closing)
    
    def _setup_ui(self) -> None:
        """Thiết lập giao diện"""
        self.root.title("Nhận diện sâu bệnh với YOLOv11")
        self.root.geometry("1400x900")
        
        # Style
        style = ttk.Style()
        style.theme_use('clam')
        
        # Main frame
        main_frame = ttk.Frame(self.root)
        main_frame.pack(fill='both', expand=True, padx=10, pady=10)
        
        # Canvas frame
        canvas_frame = ttk.LabelFrame(main_frame, text="Hiển thị hình ảnh", padding=10)
        canvas_frame.pack(fill='x', pady=(0, 10))
        
        self.canvas = tk.Canvas(canvas_frame, width=self.config.CANVAS_SIZE[0], 
                               height=self.config.CANVAS_SIZE[1], bg="black")
        self.canvas.pack()
        
        # Status label
        self.status_label = ttk.Label(canvas_frame, text="Sẵn sàng - Chọn ảnh/thư mục hoặc mở camera", 
                                     font=("Arial", 10))
        self.status_label.pack(pady=5)
        
        # Progress bar
        self.progress = ttk.Progressbar(canvas_frame, mode='indeterminate')
        self.progress.pack(fill='x', pady=5)
        
        # Button frame
        button_frame = ttk.LabelFrame(main_frame, text="Điều khiển", padding=10)
        button_frame.pack(fill='x', pady=(0, 10))
        
        # Create buttons
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
        
        # Results frame
        results_frame = ttk.LabelFrame(main_frame, text="Kết quả", padding=10)
        results_frame.pack(fill='both', expand=True)
        
        # Text widget with scrollbar
        text_frame = ttk.Frame(results_frame)
        text_frame.pack(fill='both', expand=True)
        
        self.result_text = tk.Text(text_frame, height=15, font=("Consolas", 9))
        scrollbar = ttk.Scrollbar(text_frame, orient='vertical', command=self.result_text.yview)
        self.result_text.configure(yscrollcommand=scrollbar.set)
        
        self.result_text.pack(side='left', fill='both', expand=True)
        scrollbar.pack(side='right', fill='y')
    
    def _update_status(self, message: str) -> None:
        """Cập nhật trạng thái"""
        self.status_label.config(text=message)
        self.root.update_idletasks()
    
    def _add_result_text(self, text: str) -> None:
        """Thêm text vào kết quả"""
        self.result_text.insert(tk.END, text + "\n")
        self.result_text.see(tk.END)
        self.root.update_idletasks()
    
    def _clear_results(self) -> None:
        """Xóa kết quả"""
        self.result_text.delete(1.0, tk.END)
        self.unhealthy_crops = []
        self.detections = []
        self.current_crop_index = 0
    
    def _display_image(self, image_data, is_pil: bool = False) -> None:
        """Hiển thị ảnh trên canvas"""
        try:
            if is_pil:
                img = image_data
            else:
                # Chuyển từ OpenCV sang PIL
                if len(image_data.shape) == 3:
                    img = Image.fromarray(cv2.cvtColor(image_data, cv2.COLOR_BGR2RGB))
                else:
                    img = Image.fromarray(image_data)
            
            # Resize để vừa canvas
            img = img.resize(self.config.CANVAS_SIZE, Image.Resampling.LANCZOS)
            photo = ImageTk.PhotoImage(img)
            
            self.canvas.delete("all")
            self.canvas.create_image(0, 0, anchor="nw", image=photo)
            
            # Lưu reference để tránh garbage collection
            self.photo_references.append(photo)
            if len(self.photo_references) > 10:  # Giới hạn số lượng
                self.photo_references.pop(0)
                
        except Exception as e:
            logger.error(f"Lỗi hiển thị ảnh: {e}")
    
    def _load_image(self) -> None:
        """Tải ảnh"""
        self._stop_camera()
        
        filetypes = [("Tất cả ảnh", " ".join([f"*{ext}" for ext in self.config.SUPPORTED_FORMATS]))]
        filetypes.extend([(f"{ext.upper()} files", f"*{ext}") for ext in self.config.SUPPORTED_FORMATS])
        
        file_path = filedialog.askopenfilename(filetypes=filetypes)
        if file_path:
            self.image_path = file_path
            self._clear_results()
            
            # Hiển thị ảnh
            img = Image.open(file_path)
            self._display_image(img, is_pil=True)
            
            self._update_status("Ảnh đã tải - Nhấn 'Nhận diện' để xử lý")
            self._add_result_text(f"Đã tải ảnh: {Path(file_path).name}")
    
    def _load_folder(self) -> None:
        """Tải thư mục"""
        self._stop_camera()
        
        folder_path = filedialog.askdirectory()
        if folder_path:
            self.image_path = folder_path
            self._clear_results()
            
            # Đếm số ảnh hợp lệ
            folder = Path(folder_path)
            image_count = len([f for f in folder.iterdir() 
                             if f.suffix.lower() in self.config.SUPPORTED_FORMATS])
            
            self._update_status(f"Đã chọn thư mục - {image_count} ảnh")
            self._add_result_text(f"Đã chọn thư mục: {folder_path}")
            self._add_result_text(f"Tìm thấy {image_count} ảnh hợp lệ")
    
    def _open_camera(self) -> None:
        """Mở camera"""
        self._stop_camera()
        self._clear_results()
        
        self._update_status("Đang tìm camera...")
        
        if not self.camera_manager.find_available_camera():
            messagebox.showerror(
                "Lỗi camera",
                "Không tìm thấy camera! Vui lòng kiểm tra:\n"
                "• Webcam đã được kết nối\n"
                "• Driver webcam đã được cài đặt\n"
                "• Không có ứng dụng nào khác đang sử dụng camera\n"
                "• Thử cắm webcam vào cổng USB khác"
            )
            self._update_status("Không thể mở camera")
            return
        
        self.camera_active = True
        self._update_status(f"Camera đã mở (chỉ số {self.camera_manager.camera_index})")
        self._add_result_text(f"Đã mở camera tại chỉ số {self.camera_manager.camera_index}")
        
        # Bắt đầu thread hiển thị camera
        self.camera_thread = threading.Thread(target=self._camera_loop, daemon=True)
        self.camera_thread.start()
    
    def _camera_loop(self) -> None:
        """Vòng lặp hiển thị camera"""
        while self.camera_active:
            frame = self.camera_manager.read_frame()
            if frame is None:
                self._add_result_text("Lỗi đọc frame từ camera")
                break
            
            # Hiển thị frame trên main thread
            self.root.after(0, lambda f=frame: self._display_image(f))
            
            # Giới hạn FPS
            threading.Event().wait(0.090)  
    
    def _stop_camera(self) -> None:
        """Dừng camera"""
        if self.camera_active:
            self.camera_active = False
            if self.camera_thread and self.camera_thread.is_alive():
                self.camera_thread.join(timeout=1)
            
            self.camera_manager.release()
            self.canvas.delete("all")
            self._update_status("Camera đã dừng")
            self._add_result_text("Camera đã dừng")
    
    def _capture_image(self) -> None:
        """Chụp ảnh từ camera"""
        if not self.camera_active:
            messagebox.showerror("Lỗi", "Vui lòng mở camera trước!")
            return
        
        frame = self.camera_manager.read_frame()
        if frame is None:
            messagebox.showerror("Lỗi", "Không thể chụp ảnh từ camera!")
            return
        
        # Lưu ảnh
        cv2.imwrite(self.config.TEMP_IMAGE, frame)
        self.image_path = self.config.TEMP_IMAGE
        
        self._stop_camera()
        self._clear_results()
        
        # Hiển thị ảnh chụp
        self._display_image(frame)
        self._update_status("Đã chụp ảnh - Nhấn 'Nhận diện' để xử lý")
        self._add_result_text("Đã chụp ảnh từ camera")
    
    def _run_detection(self) -> None:
        """Chạy nhận diện"""
        if not self.image_path:
            messagebox.showerror("Lỗi", "Vui lòng chọn ảnh, thư mục, hoặc chụp ảnh từ camera trước!")
            return
        
        # Chạy trong thread riêng để không block UI
        thread = threading.Thread(target=self._detection_worker, daemon=True)
        thread.start()
    
    def _detection_worker(self) -> None:
        """Worker cho nhận diện"""
        self.root.after(0, lambda: self.progress.start())
        self.root.after(0, lambda: self._update_status("Đang xử lý..."))
        
        try:
            asyncio.run(self._run_detection_async())
        except Exception as e:
            self.root.after(0, lambda: self._add_result_text(f"Lỗi: {e}"))
        finally:
            self.root.after(0, lambda: self.progress.stop())
    
    async def _run_detection_async(self) -> None:
        """Nhận diện bất đồng bộ"""
        self._clear_results()
        
        if os.path.isfile(self.image_path):
            # Xử lý ảnh đơn - BẮT BUỘC gửi Telegram
            self.root.after(0, lambda: self._add_result_text("Đang xử lý ảnh và chuẩn bị gửi thông báo..."))
            
            result_image, unhealthy_paths, detections = await self.image_processor.process_single_image(
                self.image_path, send_telegram=True  # Luôn gửi Telegram cho ảnh đơn
            )
            
            if result_image is None:
                self.root.after(0, lambda: self._add_result_text("Không phát hiện rau cải trong ảnh"))
                self.root.after(0, lambda: self._update_status("Không tìm thấy cây"))
                return
            
            self.result_image = result_image
            self.unhealthy_crops = unhealthy_paths
            self.detections = detections
            
            # Hiển thị kết quả
            self.root.after(0, lambda: self._display_image(result_image))
            
            # Thống kê kết quả
            unhealthy_count = len([d for d in detections if d.label == 'unhealthy'])
            healthy_count = len([d for d in detections if d.label == 'healthy'])
            mature_count = len([d for d in detections if d.label == 'mature'])
            not_mature_count = len([d for d in detections if d.label == 'not mature'])
            
            # Hiển thị thống kê
            total_plants = len(detections)
            self.root.after(0, lambda: self._add_result_text(f"=== KẾT QUẢ NHẬN DIỆN ==="))
            self.root.after(0, lambda: self._add_result_text(f"Tổng số vùng phát hiện: {total_plants}"))
            self.root.after(0, lambda: self._add_result_text(f"• Khỏe mạnh: {healthy_count}"))
            self.root.after(0, lambda: self._add_result_text(f"• Không khỏe: {unhealthy_count}"))
            self.root.after(0, lambda: self._add_result_text(f"• Chín: {mature_count}"))
            self.root.after(0, lambda: self._add_result_text(f"• Chưa chín: {not_mature_count}"))
            self.root.after(0, lambda: self._add_result_text(f"Số ảnh unhealthy đã cắt: {len(unhealthy_paths)}"))
            
            if unhealthy_count > 0:
                self.root.after(0, lambda: self._add_result_text(f"⚠️ CẢNH BÁO: Phát hiện {unhealthy_count} vùng không khỏe!"))
                self.root.after(0, lambda: self._add_result_text("✅ Đã gửi thông báo qua Telegram"))
            
            self.root.after(0, lambda: self._update_status(f"Hoàn tất - Phát hiện {total_plants} vùng"))
            
        elif os.path.isdir(self.image_path):
            # Xử lý thư mục
            self.root.after(0, lambda: self._add_result_text("Đang xử lý thư mục..."))
            
            unhealthy_paths, all_detections = await self.image_processor.process_folder(
                self.image_path, send_telegram=False  # Không gửi Telegram cho từng ảnh trong thư mục
            )
            
            self.unhealthy_crops = unhealthy_paths
            self.detections = all_detections
            
            if not all_detections:
                self.root.after(0, lambda: self._add_result_text("Không phát hiện rau cải trong thư mục"))
                self.root.after(0, lambda: self._update_status("Không tìm thấy cây"))
                return
            
            # Thống kê tổng hợp
            total_images = len(set(d.image_path for d in all_detections if d.image_path))
            unhealthy_count = len([d for d in all_detections if d.label == 'unhealthy'])
            healthy_count = len([d for d in all_detections if d.label == 'healthy'])
            mature_count = len([d for d in all_detections if d.label == 'mature'])
            not_mature_count = len([d for d in all_detections if d.label == 'not mature'])
            
            self.root.after(0, lambda: self._add_result_text(f"=== KẾT QUẢ XỬ LÝ THU MỤC ==="))
            self.root.after(0, lambda: self._add_result_text(f"Số ảnh đã xử lý: {total_images}"))
            self.root.after(0, lambda: self._add_result_text(f"Tổng số vùng phát hiện: {len(all_detections)}"))
            self.root.after(0, lambda: self._add_result_text(f"• Khỏe mạnh: {healthy_count}"))
            self.root.after(0, lambda: self._add_result_text(f"• Không khỏe: {unhealthy_count}"))
            self.root.after(0, lambda: self._add_result_text(f"• Lớn: {mature_count}"))
            self.root.after(0, lambda: self._add_result_text(f"• Chưa Lớn: {not_mature_count}"))
            self.root.after(0, lambda: self._add_result_text(f"Số ảnh unhealthy đã cắt: {len(unhealthy_paths)}"))
            
            if unhealthy_count > 0:
                self.root.after(0, lambda: self._add_result_text(f"⚠️ CẢNH BÁO: Tổng cộng {unhealthy_count} vùng không khỏe!"))
            
            self.root.after(0, lambda: self._update_status(f"Hoàn tất - Xử lý {total_images} ảnh"))
    
    def _show_next_crop(self) -> None:
        """Hiển thị ảnh crop tiếp theo"""
        if not self.unhealthy_crops:
            messagebox.showinfo("Thông báo", "Không có ảnh unhealthy nào để hiển thị!")
            return
        
        # Hiển thị ảnh crop hiện tại
        crop_path = self.unhealthy_crops[self.current_crop_index]
        if os.path.exists(crop_path):
            img = Image.open(crop_path)
            self._display_image(img, is_pil=True)
            
            self._update_status(f"Ảnh unhealthy {self.current_crop_index + 1}/{len(self.unhealthy_crops)}")
            self._add_result_text(f"Hiển thị: {Path(crop_path).name}")
        else:
            self._add_result_text(f"Không thể mở ảnh: {crop_path}")
        
        # Chuyển đến ảnh tiếp theo
        self.current_crop_index = (self.current_crop_index + 1) % len(self.unhealthy_crops)
    
    def _save_results(self) -> None:
        """Lưu kết quả"""
        if not self.detections:
            messagebox.showinfo("Thông báo", "Không có kết quả để lưu!")
            return
        
        # Chọn thư mục lưu
        save_dir = filedialog.askdirectory(title="Chọn thư mục lưu kết quả")
        if not save_dir:
            return
        
        try:
            # Tạo thư mục con
            timestamp = datetime.now().strftime("%Y%m%D_%H%M%S")
            result_dir = os.path.join(save_dir, f"plant_detection_results_{timestamp}")
            os.makedirs(result_dir, exist_ok=True)
            
            # Lưu ảnh kết quả nếu có
            if self.result_image is not None:
                result_image_path = os.path.join(result_dir, "detection_result.jpg")
                cv2.imwrite(result_image_path, self.result_image)
                self._add_result_text(f"Đã lưu ảnh kết quả: {result_image_path}")
            
            # Lưu thông tin JSON
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
            
            # Copy các ảnh unhealthy
            if self.unhealthy_crops:
                crops_dir = os.path.join(result_dir, "unhealthy_crops")
                os.makedirs(crops_dir, exist_ok=True)
                
                for crop_path in self.unhealthy_crops:
                    if os.path.exists(crop_path):
                        crop_name = Path(crop_path).name
                        new_crop_path = os.path.join(crops_dir, crop_name)
                        import shutil
                        shutil.copy2(crop_path, new_crop_path)
            
            self._add_result_text(f"✅ Đã lưu kết quả vào: {result_dir}")
            messagebox.showinfo("Thành công", f"Đã lưu kết quả vào:\n{result_dir}")
            
        except Exception as e:
            logger.error(f"Lỗi khi lưu kết quả: {e}")
            messagebox.showerror("Lỗi", f"Không thể lưu kết quả: {e}")
    
    def _on_closing(self) -> None:
        """Xử lý khi đóng ứng dụng"""
        try:
            # Dừng camera
            self._stop_camera()
            
            # Dọn dẹp thread pool
            if hasattr(self.image_processor, 'executor'):
                self.image_processor.executor.shutdown(wait=False)
            
            # Xóa file temp nếu có
            if os.path.exists(self.config.TEMP_IMAGE):
                try:
                    os.remove(self.config.TEMP_IMAGE)
                except:
                    pass
            
            logger.info("Ứng dụng đã đóng")
        except Exception as e:
            logger.error(f"Lỗi khi đóng ứng dụng: {e}")
        finally:
            self.root.destroy()

def create_config_file(config_path: str = "config.json") -> None:
    """Tạo file cấu hình mẫu"""
    default_config = {
        "MODEL_PATH": "weights/best.pt",
        "TELEGRAM_BOT_TOKEN": "7854048371:AAHLrpp3ZxSeiNAGS49n8-W_Nx3iUeqFbT4",
        "TELEGRAM_CHAT_ID": "-1002524466925",
        "IMAGE_FOLDER": "test_images",
        "OUTPUT_DIR": "unhealthy_crops",
        "CONFIDENCE_THRESHOLD": 0.3,
        "IOU_THRESHOLD": 0.6,
        "MAX_DETECTIONS": 100
    }
    
    try:
        with open(config_path, 'w', encoding='utf-8') as f:
            json.dump(default_config, f, ensure_ascii=False, indent=2)
        print(f"Đã tạo file cấu hình mẫu: {config_path}")
    except Exception as e:
        print(f"Lỗi khi tạo file cấu hình: {e}")

def load_config_from_file(config_path: str = "config.json") -> Config:
    """Tải cấu hình từ file"""
    if not os.path.exists(config_path):
        print(f"Không tìm thấy file cấu hình {config_path}, sử dụng cấu hình mặc định")
        return Config()
    
    try:
        with open(config_path, 'r', encoding='utf-8') as f:
            config_data = json.load(f)
        
        # Tạo Config với các giá trị từ file
        config = Config()
        for key, value in config_data.items():
            if hasattr(config, key):
                setattr(config, key, value)
        
        print(f"Đã tải cấu hình từ {config_path}")
        return config
    except Exception as e:
        print(f"Lỗi khi tải cấu hình từ {config_path}: {e}")
        print("Sử dụng cấu hình mặc định")
        return Config()

def main():
    """Hàm main"""
    try:
        # Import thêm các thư viện cần thiết
        from datetime import datetime
        import sys
        
        ## Kiểm tra phiên bản Python
        if sys.version_info < (3,7):
            print("Yêu cầu Python 3.7 trở lên!")
            return
        print("=" * 60)
        print("🌱 ỨNG DỤNG NHẬN DIỆN SÂU BỆNH CÂY TRỒNG VỚI YOLOv11")
        print("=" * 60)
        print("Tác giả: DUY HOAN")
        print("Phiên bản: NHO NHUNG KHONG CO VO")
        print("Ngày: 2025")
        print("=" * 60)
        
        # Tải cấu hình
        config = load_config_from_file()
        
        # Kiểm tra file mô hình
        if not os.path.exists(config.MODEL_PATH):
            print(f"❌ CẢNH BÁO: Không tìm thấy file mô hình tại {config.MODEL_PATH}")
            print("Vui lòng kiểm tra đường dẫn mô hình trong config.json")
            input("Nhấn Enter để tiếp tục với GUI (có thể gặp lỗi)...")
        
        # Khởi tạo GUI
        root = tk.Tk()
        app = PlantDiseaseGUI(root, config)
        
        print("✅ Ứng dụng đã khởi động thành công!")
        print("📋 Hướng dẫn sử dụng:")
        print("   1. Chọn 'Tải ảnh' để xử lý một ảnh")
        print("   2. Chọn 'Tải thư mục' để xử lý nhiều ảnh")
        print("   3. Chọn 'Mở camera' để sử dụng webcam")
        print("   4. Nhấn 'Nhận diện' để phân tích")
        print("   5. Sử dụng 'Ảnh tiếp theo' để xem các vùng unhealthy")
        print("   6. Nhấn 'Lưu kết quả' để xuất báo cáo")
        print("=" * 60)
        
        # Chạy ứng dụng
        root.mainloop()
        
    except KeyboardInterrupt:
        print("\n🛑 Ứng dụng bị dừng bởi người dùng")
    except Exception as e:
        print(f"❌ Lỗi nghiêm trọng: {e}")
        logger.exception("Lỗi nghiêm trọng trong main")
    finally:
        print("👋 Cảm ơn bạn đã sử dụng ứng dụng!")

if __name__ == "__main__":
    # Tạo file cấu hình mẫu nếu chưa có
    if not os.path.exists("config.json"):
        create_config_file()
        print("📄 Đã tạo file config.json mẫu. Vui lòng cập nhật thông tin Telegram Bot!")
    
    main()
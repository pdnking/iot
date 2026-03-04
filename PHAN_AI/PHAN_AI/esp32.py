from flask import Flask, request, jsonify
from flask_cors import CORS
import cv2
import numpy as np
from ultralytics import YOLO
import base64
from PIL import Image
import io
import logging
import os
from typing import Dict, List, Any
import json
from datetime import datetime

# Setup logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

app = Flask(__name__)
CORS(app)  # Enable CORS for all routes

class PlantDiseaseAI:
    def __init__(self, model_path: str):
        """Initialize the AI model"""
        self.model = None
        self.model_path = model_path
        self.class_names = ['healthy', 'unhealthy', 'mature', 'not mature']
        self.class_colors = {
            'healthy': (0, 255, 0),
            'unhealthy': (0, 0, 255),
            'mature': (255, 0, 0),
            'not mature': (255, 255, 0)
        }
        self.load_model()
    
    def load_model(self):
        """Load YOLO model"""
        try:
            if not os.path.exists(self.model_path):
                raise FileNotFoundError(f"Model file not found: {self.model_path}")
            
            self.model = YOLO(self.model_path)
            logger.info(f"✅ YOLO model loaded successfully from {self.model_path}")
            
            # Print model info
            logger.info(f"Model classes: {self.model.names}")
            
        except Exception as e:
            logger.error(f"❌ Error loading YOLO model: {e}")
            raise
    
    def preprocess_image(self, image_data) -> np.ndarray:
        """Preprocess image for YOLO"""
        try:
            # If base64 string, decode it
            if isinstance(image_data, str):
                # Remove data:image/jpeg;base64, prefix if present
                if image_data.startswith('data:image'):
                    image_data = image_data.split(',')[1]
                
                # Decode base64
                image_bytes = base64.b64decode(image_data)
                image = Image.open(io.BytesIO(image_bytes))
                image = cv2.cvtColor(np.array(image), cv2.COLOR_RGB2BGR)
            
            # If PIL Image
            elif isinstance(image_data, Image.Image):
                image = cv2.cvtColor(np.array(image_data), cv2.COLOR_RGB2BGR)
            
            # If already numpy array
            else:
                image = image_data
            
            return image
            
        except Exception as e:
            logger.error(f"Error preprocessing image: {e}")
            raise
    
    def predict(self, image: np.ndarray, confidence: float = 0.3) -> Dict[str, Any]:
        """Run YOLO prediction on image"""
        try:
            if self.model is None:
                raise RuntimeError("Model not loaded")
            
            # Run inference
            results = self.model(image, conf=confidence, iou=0.6, max_det=100)
            
            detections = []
            statistics = {'healthy': 0, 'unhealthy': 0, 'mature': 0, 'not mature': 0}
            
            for result in results:
                if result.boxes is None:
                    continue
                
                for box in result.boxes:
                    # Extract box info
                    x1, y1, x2, y2 = map(int, box.xyxy[0].cpu().numpy())
                    conf = float(box.conf[0].cpu().numpy())
                    cls_id = int(box.cls[0].cpu().numpy())
                    
                    # Get class name
                    class_name = self.model.names[cls_id]
                    
                    detection = {
                        'class': class_name,
                        'confidence': conf,
                        'bbox': [x1, y1, x2, y2],
                        'center': [(x1 + x2) // 2, (y1 + y2) // 2],
                        'area': (x2 - x1) * (y2 - y1)
                    }
                    
                    detections.append(detection)
                    
                    # Update statistics
                    if class_name in statistics:
                        statistics[class_name] += 1
            
            # Calculate percentages
            total_detections = len(detections)
            percentages = {}
            for class_name, count in statistics.items():
                if total_detections > 0:
                    percentages[class_name] = round((count / total_detections) * 100, 2)
                else:
                    percentages[class_name] = 0
            
            return {
                'detections': detections,
                'statistics': statistics,
                'percentages': percentages,
                'total_detections': total_detections,
                'image_shape': image.shape
            }
        
        except Exception as e:
            logger.error(f"Error during prediction: {e}")
            raise
    
    def draw_detections(self, image: np.ndarray, detections: List[Dict]) -> np.ndarray:
        """Draw bounding boxes and labels on image"""
        try:
            result_image = image.copy()
            
            for detection in detections:
                bbox = detection['bbox']
                class_name = detection['class']
                confidence = detection['confidence']
                
                # Get color for this class
                color = self.class_colors.get(class_name, (255, 255, 255))
                
                # Draw bounding box
                cv2.rectangle(result_image, (bbox[0], bbox[1]), (bbox[2], bbox[3]), color, 2)
                
                # Draw label with confidence
                label = f"{class_name}: {confidence:.2f}"
                label_size = cv2.getTextSize(label, cv2.FONT_HERSHEY_SIMPLEX, 0.5, 2)[0]
                
                # Draw label background
                cv2.rectangle(result_image, 
                             (bbox[0], bbox[1] - label_size[1] - 10),
                             (bbox[0] + label_size[0], bbox[1]),
                             color, -1)
                
                # Draw label text
                cv2.putText(result_image, label,
                           (bbox[0], bbox[1] - 5),
                           cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255, 255, 255), 2)
            
            return result_image
        
        except Exception as e:
            logger.error(f"Error drawing detections: {e}")
            return image
    
    def generate_summary(self, statistics: Dict, percentages: Dict) -> str:
        """Generate text summary of detection results"""
        try:
            summary = []
            total = sum(statistics.values())
            
            if total == 0:
                return "No plants detected in the image."
            
            summary.append(f"Total plants detected: {total}")
            
            # Health status
            healthy_count = statistics.get('healthy', 0)
            unhealthy_count = statistics.get('unhealthy', 0)
            
            if healthy_count > 0 or unhealthy_count > 0:
                health_total = healthy_count + unhealthy_count
                if health_total > 0:
                    healthy_pct = round((healthy_count / health_total) * 100, 1)
                    summary.append(f"Health status: {healthy_pct}% healthy, {100-healthy_pct}% unhealthy")
            
            # Maturity status
            mature_count = statistics.get('mature', 0)
            not_mature_count = statistics.get('not mature', 0)
            
            if mature_count > 0 or not_mature_count > 0:
                maturity_total = mature_count + not_mature_count
                if maturity_total > 0:
                    mature_pct = round((mature_count / maturity_total) * 100, 1)
                    summary.append(f"Maturity status: {mature_pct}% mature, {100-mature_pct}% not mature")
            
            return " | ".join(summary)
        
        except Exception as e:
            logger.error(f"Error generating summary: {e}")
            return "Error generating summary"

# Initialize AI model
MODEL_PATH = "best.pt"  # Update this path to your model
ai_model = None

try:
    ai_model = PlantDiseaseAI(MODEL_PATH)
except Exception as e:
    logger.error(f"Failed to initialize AI model: {e}")

@app.route('/', methods=['GET'])
def health_check():
    """Health check endpoint"""
    return jsonify({
        'status': 'ok',
        'message': 'Plant Disease AI API is running',
        'model_loaded': ai_model is not None,
        'timestamp': datetime.now().isoformat()
    })

@app.route('/predict', methods=['POST'])
def predict():
    """Main prediction endpoint"""
    try:
        if ai_model is None:
            return jsonify({
                'error': 'AI model not loaded',
                'success': False
            }), 500
        
        # Get request data
        if request.is_json:
            data = request.get_json()
            image_data = data.get('image')
            confidence = data.get('confidence', 0.3)
            return_image = data.get('return_image', False)
        else:
            # Handle file upload
            if 'image' not in request.files:
                return jsonify({
                    'error': 'No image provided',
                    'success': False
                }), 400
            
            file = request.files['image']
            if file.filename == '':
                return jsonify({
                    'error': 'No image selected',
                    'success': False
                }), 400
            
            # Read image from file
            image_data = Image.open(file.stream)
            confidence = float(request.form.get('confidence', 0.3))
            return_image = request.form.get('return_image', 'false').lower() == 'true'
        
        # Preprocess image
        image = ai_model.preprocess_image(image_data)
        
        # Run prediction
        results = ai_model.predict(image, confidence=confidence)
        
        # Prepare response
        response_data = {
            'success': True,
            'results': results,
            'summary': ai_model.generate_summary(results['statistics'], results['percentages']),
            'timestamp': datetime.now().isoformat()
        }
        
        # Add annotated image if requested
        if return_image:
            annotated_image = ai_model.draw_detections(image, results['detections'])
            
            # Convert to base64
            _, buffer = cv2.imencode('.jpg', annotated_image)
            img_base64 = base64.b64encode(buffer).decode('utf-8')
            response_data['annotated_image'] = f"data:image/jpeg;base64,{img_base64}"
        
        return jsonify(response_data)
    
    except Exception as e:
        logger.error(f"Error in prediction: {e}")
        return jsonify({
            'error': str(e),
            'success': False,
            'timestamp': datetime.now().isoformat()
        }), 500

@app.route('/model/info', methods=['GET'])
def model_info():
    """Get model information"""
    try:
        if ai_model is None:
            return jsonify({
                'error': 'Model not loaded',
                'success': False
            }), 500
        
        return jsonify({
            'success': True,
            'model_path': ai_model.model_path,
            'classes': ai_model.class_names,
            'class_colors': ai_model.class_colors,
            'model_names': ai_model.model.names if ai_model.model else None,
            'timestamp': datetime.now().isoformat()
        })
    
    except Exception as e:
        logger.error(f"Error getting model info: {e}")
        return jsonify({
            'error': str(e),
            'success': False
        }), 500

@app.route('/batch_predict', methods=['POST'])
def batch_predict():
    """Batch prediction endpoint for multiple images"""
    try:
        if ai_model is None:
            return jsonify({
                'error': 'AI model not loaded',
                'success': False
            }), 500
        
        data = request.get_json()
        images = data.get('images', [])
        confidence = data.get('confidence', 0.3)
        
        if not images:
            return jsonify({
                'error': 'No images provided',
                'success': False
            }), 400
        
        results = []
        
        for i, image_data in enumerate(images):
            try:
                # Preprocess image
                image = ai_model.preprocess_image(image_data)
                
                # Run prediction
                prediction_result = ai_model.predict(image, confidence=confidence)
                
                results.append({
                    'image_index': i,
                    'success': True,
                    'results': prediction_result,
                    'summary': ai_model.generate_summary(
                        prediction_result['statistics'], 
                        prediction_result['percentages']
                    )
                })
                
            except Exception as e:
                results.append({
                    'image_index': i,
                    'success': False,
                    'error': str(e)
                })
        
        return jsonify({
            'success': True,
            'batch_results': results,
            'total_processed': len(results),
            'timestamp': datetime.now().isoformat()
        })
    
    except Exception as e:
        logger.error(f"Error in batch prediction: {e}")
        return jsonify({
            'error': str(e),
            'success': False
        }), 500

if __name__ == '__main__':
    # Check if model exists before starting
    if not os.path.exists(MODEL_PATH):
        logger.warning(f"⚠️  Model file not found at {MODEL_PATH}")
        logger.warning("Please ensure your YOLO model file is in the correct location")
    
    # Start Flask app
    logger.info("🚀 Starting Plant Disease AI Flask server...")
    app.run(debug=True, host='0.0.0.0', port=5000)
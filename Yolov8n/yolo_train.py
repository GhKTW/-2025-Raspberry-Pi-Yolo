# YOLOv8 모델을 Google Colab에서 훈련시키는 코드입니다.
# 필요한 라이브러리 설치
from ultralytics import YOLO
from google.colab import files
import zipfile
import os

model = YOLO('yolov8n.pt')

# Google Colab에서 YOLOv8 모델 훈련을 위한 데이터셋 업로드
uploaded = files.upload()

# 압축 해제할 경로 생성
os.makedirs("yolo_dataset", exist_ok=True)

# 압축 해제
with zipfile.ZipFile("/content/yolov11s.v5i.yolov11.zip", "r") as zip_ref:
    zip_ref.extractall("yolo_dataset")

model.train(
    data="yolo_dataset/data.yaml",  # 또는 정확한 경로 확인
    epochs=50,
    imgsz=640
)

from google.colab import files
uploaded = files.upload()

model = YOLO("runs/detect/train/weights/best.pt")

results = model.predict(source="video.mp4", save=True, conf=0.3)

from google.colab import files

# 모델 다운로드
files.download("runs/detect/train/weights/best.pt")

# data.yaml 다운로드
files.download("yolo_dataset/data.yaml")

# 원하면 test_video도 다운로드 가능
files.download()
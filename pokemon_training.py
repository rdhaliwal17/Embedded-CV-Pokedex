from ultralytics import YOLO

def main():
    model = YOLO("yolov8n.pt")
    model.train(
        data=r"E:\General\Projects\pokedex_project\pokemon_dataset\data.yaml",
        epochs=30,
        imgsz=640,
        batch=16,
        device=0,  # RTX 3070 Ti
        workers=4
    )

if __name__ == "__main__":
    main()
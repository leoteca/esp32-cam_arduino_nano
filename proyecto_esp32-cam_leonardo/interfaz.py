import requests
import tkinter as tk
from PIL import Image, ImageTk
import io

class SecurityApp:
    def __init__(self, root):
        self.root = root
        self.root.title("Sistema de Seguridad ESP32-CAM v1.0")
        self.esp32_ip = "http://192.168.1.100"  # Dirección IP del ESP32
        
        # Variables de video
        self.video_label = tk.Label(root)
        self.video_label.pack(padx=10, pady=10)
        
        # Variables para mostrar temperatura y humedad
        self.temp_label = tk.Label(root, text="Temperatura: -- °C")
        self.temp_label.pack(padx=10, pady=5)
        
        self.humidity_label = tk.Label(root, text="Humedad: -- %")
        self.humidity_label.pack(padx=10, pady=5)
        
        # Botón para iniciar sesión (si es necesario)
        self.login_button = tk.Button(root, text="Iniciar Sesión", command=self.login)
        self.login_button.pack(padx=10, pady=10)

        # Iniciar el video en vivo
        self.stream_video()

    def stream_video(self):
        """Recibir video en vivo desde el ESP32"""
        try:
            # Obtener los datos del video (streaming)
            video_response = requests.get(f"{self.esp32_ip}/video", stream=True)
            img_data = video_response.content
            image = Image.open(io.BytesIO(img_data))
            photo = ImageTk.PhotoImage(image)
            self.video_label.config(image=photo)
            self.video_label.image = photo  # Evitar que la imagen se pierda

            self.root.after(50, self.stream_video)  # Recursión para seguir actualizando el video
        except Exception as e:
            print("Error al recibir video:", e)
        
        # Obtener temperatura y humedad
        try:
            temp_response = requests.get(f"{self.esp32_ip}/temp_humidity")
            data = temp_response.json()  # Obtener los datos JSON enviados desde el ESP32
            
            # Mostrar los datos de temperatura y humedad en la interfaz
            self.temp_label.config(text=f"Temperatura: {data['temperature']} °C")
            self.humidity_label.config(text=f"Humedad: {data['humidity']} %")
        except Exception as e:
            print("Error al recibir datos de temperatura/humedad:", e)
        
        # Actualizar cada 5 segundos
        self.root.after(5000, self.stream_video)  # Actualización cada 5 segundos para mantener los datos actualizados

    def login(self):
        """Simulación de un proceso de login"""
        print("Login exitoso")  # En este caso solo se simula un login
        self.login_button.config(state="disabled")  # Deshabilitar el botón de login

if __name__ == "__main__":
    root = tk.Tk()
    app = SecurityApp(root)
    root.mainloop()
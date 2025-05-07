import socket
import time
import threading
import tkinter as tk
from tkinter import scrolledtext
import json
import queue

class UDPBroadcastApp:
    def __init__(self, root):
        self.root = root
        self.root.title("UDP Broadcast Tool")
        
        self.port = 12345  # Porta padrão
        self.running = False
        self.periodic_sending = False
        self.log_queue = queue.Queue()

        # Frame principal
        main_frame = tk.Frame(root)
        main_frame.pack(padx=10, pady=10)

        # Envio único
        single_frame = tk.LabelFrame(main_frame, text="Envio Único", padx=5, pady=5)
        single_frame.grid(row=0, column=0, sticky="ew", padx=5, pady=5)

        tk.Label(single_frame, text="Mensagem:").grid(row=0, column=0, sticky="w")
        self.message_entry = tk.Entry(single_frame, width=60)
        self.message_entry.grid(row=0, column=1, padx=5)
        self.send_button = tk.Button(single_frame, text="Enviar", command=self.send_message)
        self.send_button.grid(row=0, column=2, padx=5)

        # Envio periódico
        periodic_frame = tk.LabelFrame(main_frame, text="Envio Periódico", padx=5, pady=5)
        periodic_frame.grid(row=1, column=0, sticky="ew", padx=5, pady=5)

        tk.Label(periodic_frame, text="Mensagem:").grid(row=0, column=0, sticky="w")
        self.periodic_message_entry = tk.Entry(periodic_frame, width=30)
        self.periodic_message_entry.insert(0, '{"URI":"200/20"}')
        self.periodic_message_entry.grid(row=0, column=1, padx=5)

        tk.Label(periodic_frame, text="Intervalo (s):").grid(row=1, column=0, sticky="w")
        self.interval_entry = tk.Entry(periodic_frame, width=10)
        self.interval_entry.insert(0, "5")  # Valor padrão
        self.interval_entry.grid(row=1, column=1, sticky="w", padx=5)

        self.start_periodic_button = tk.Button(periodic_frame, text="Iniciar", command=self.start_periodic_sending)
        self.start_periodic_button.grid(row=2, column=0, pady=5)
        self.stop_periodic_button = tk.Button(periodic_frame, text="Parar", command=self.stop_periodic_sending, state='disabled')
        self.stop_periodic_button.grid(row=2, column=1, pady=5)

        # Recepção
        receive_frame = tk.LabelFrame(main_frame, text="Recepção", padx=5, pady=5)
        receive_frame.grid(row=2, column=0, sticky="ew", padx=5, pady=5)

        self.start_button = tk.Button(receive_frame, text="Iniciar", command=self.start_receiving)
        self.start_button.grid(row=0, column=0, pady=5)
        self.stop_button = tk.Button(receive_frame, text="Parar", command=self.stop_receiving, state='disabled')
        self.stop_button.grid(row=0, column=1, pady=5)

        # Log
        log_frame = tk.LabelFrame(main_frame, text="Log", padx=5, pady=5)
        log_frame.grid(row=3, column=0, sticky="ew", padx=5, pady=5)

        self.log_area = scrolledtext.ScrolledText(log_frame, width=83, height=15, state='disabled')
        self.log_area.pack()

        self.clear_button = tk.Button(log_frame, text="Limpar", command=self.clear_log)
        self.clear_button.pack(pady=5)

        # botão para enviar mensagem JSON de comando
        self.json_label = tk.Label(root, text="Comando URI=100/10 (0 para desligar, 1 para ligar):")
        self.json_label.pack(pady=5)

        self.json_command_entry = tk.Entry(root, width=10)
        self.json_command_entry.insert(0, "1")  # Valor padrão
        self.json_command_entry.pack(pady=5)

        self.json_button = tk.Button(root, text="Enviar JSON de comando", command=self.send_json_message)
        self.json_button.pack(pady=5)

        # botão e campos para enviar mensagem JSON de Controle com "parametro" e "valor"
        self.param_label = tk.Label(root, text="Controle URI=100/11 com Parâmetro (CALMA ou WATCHDOG):")
        self.param_label.pack(pady=5)

        self.param_entry = tk.Entry(root, width=20)
        self.param_entry.insert(0, "CALMA")  # Valor padrão
        self.param_entry.pack(pady=5)

        self.value_label = tk.Label(root, text="Valor (ex: 1.0):")
        self.value_label.pack(pady=5)

        self.value_entry = tk.Entry(root, width=10)
        self.value_entry.insert(0, "1.0")  # Valor padrão
        self.value_entry.pack(pady=5)

        self.json_button = tk.Button(root, text="Enviar JSON de Controle", command=self.send_custom_json_message)
        self.json_button.pack(pady=5)

        # Processa mensagens da fila de log periodicamente
        self.root.after(100, self.process_log_queue)

    def log(self, message):
        """Adiciona mensagens ao log da interface."""
        self.log_queue.put(message)

    def process_log_queue(self):
        """Processa mensagens da fila de log."""
        while not self.log_queue.empty():
            message = self.log_queue.get()
            self.log_area.config(state='normal')
            self.log_area.insert(tk.END, message + "\n")
            self.log_area.see(tk.END)
            self.log_area.config(state='disabled')
        self.root.after(100, self.process_log_queue)

    def clear_log(self):
        """Limpa todas as mensagens do log."""
        self.log_area.config(state='normal')
        self.log_area.delete(1.0, tk.END)
        self.log_area.config(state='disabled')

    def send_udp_message(self, message):
        """Envia uma mensagem UDP broadcast."""
        try:
            with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
                sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
                sock.sendto(message.encode(), ('192.168.1.255', self.port))
                self.log(f"Mensagem enviada:\n {message}")
        except Exception as e:
            self.log(f"Erro ao enviar mensagem: {e}")

    def send_message(self):
        """Envia uma mensagem UDP broadcast."""
        message = self.message_entry.get()
        if not message:
            self.log("Mensagem vazia. Nada foi enviado.")
            return
        self.send_udp_message(message)

    def send_custom_json_message(self):
        """Envia uma mensagem JSON UDP broadcast com parâmetros modificáveis."""
        parametro = self.param_entry.get()
        try:
            valor = float(self.value_entry.get())
        except ValueError:
            self.log("Valor inválido. Insira um número válido para o campo 'valor'.")
            return

        if not parametro.strip():
            self.log("Parâmetro inválido. Insira um valor válido.")
            return

        json_message = {
            "URI": "100/11",
            "idAtuador": 5,
            "parametro": parametro,
            "valor": valor,
            "ACK": True
        }
        self.send_udp_message(json.dumps(json_message))

    def send_json_message(self):
        """Envia uma mensagem JSON UDP broadcast."""
        try:
            comando = int(self.json_command_entry.get())
            if comando not in [0, 1]:
                self.log("Comando inválido. Use 0 para desligar ou 1 para ligar.")
                return

            json_message = {
                "URI": "100/10",
                "idAtuador": 5,
                "comando": comando
            }
            self.send_udp_message(json.dumps(json_message))
        except ValueError:
            self.log("Comando inválido. Insira um número (0 ou 1).")

    def start_periodic_sending(self):
        """Inicia o envio periódico de mensagens."""
        if self.periodic_sending:
            self.log("Envio periódico já está em execução.")
            return

        periodic_message = self.periodic_message_entry.get()
        if not periodic_message:
            self.log("Mensagem periódica vazia. Nada será enviado.")
            return

        try:
            interval = float(self.interval_entry.get())
            if interval <= 0:
                self.log("Intervalo inválido. Deve ser maior que 0.")
                return
        except ValueError:
            self.log("Intervalo inválido. Insira um número.")
            return

        self.periodic_sending = True
        self.start_periodic_button.config(state='disabled')
        self.stop_periodic_button.config(state='normal')
        self.periodic_thread = threading.Thread(target=self.periodic_send, args=(periodic_message, interval), daemon=True)
        self.periodic_thread.start()
        self.log("Envio periódico iniciado.")

    def stop_periodic_sending(self):
        """Para o envio periódico de mensagens."""
        self.periodic_sending = False
        self.start_periodic_button.config(state='normal')
        self.stop_periodic_button.config(state='disabled')
        self.log("Envio periódico parado.")

    def periodic_send(self, message, interval):
        """Envia mensagens periodicamente no intervalo especificado."""
        while self.periodic_sending:
            self.send_udp_message(message)
            time.sleep(interval)

    def start_receiving(self):
        """Inicia a recepção de mensagens UDP em uma thread separada."""
        if self.running:
            self.log("Recepção já está em execução.")
            return

        self.running = True
        self.start_button.config(state='disabled')
        self.stop_button.config(state='normal')
        self.receiver_thread = threading.Thread(target=self.receive_messages, daemon=True)
        self.receiver_thread.start()
        self.log("Recepção iniciada.")

    def stop_receiving(self):
        """Para a recepção de mensagens UDP."""
        self.running = False
        self.start_button.config(state='normal')
        self.stop_button.config(state='disabled')
        self.log("Recepção parada.")

    def receive_messages(self):
        """Recebe mensagens UDP broadcast."""
        try:
            with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
                sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
                sock.bind(('', self.port))
                self.log(f"Escutando mensagens UDP na porta {self.port}...")

                while self.running:
                    sock.settimeout(1)
                    try:
                        data, addr = sock.recvfrom(1024)
                        self.log(f"Mensagem recebida de {addr}:\n {data.decode()}")
                    except socket.timeout:
                        continue
        except Exception as e:
            self.log(f"Erro na recepção: {e}")

if __name__ == "__main__":
    root = tk.Tk()
    app = UDPBroadcastApp(root)
    root.mainloop()
import socket
import sqlite3
import threading
import queue
import json
import time
from http.server import BaseHTTPRequestHandler, HTTPServer
from urllib.parse import urlparse, parse_qs

UDP_PORT = 9999
HTTP_PORT = 8000
DB_FILE = "embedded_logs.db"
log_queue = queue.Queue()

def init_db():
    with sqlite3.connect(DB_FILE) as conn:
        conn.execute("""
            CREATE TABLE IF NOT EXISTS logs (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                timestamp DATETIME DEFAULT (datetime('now', 'localtime')),
                message TEXT
            )
        """)
        conn.execute("CREATE INDEX IF NOT EXISTS idx_msg ON logs(message)")
    print(f"[*] SQLite Database [{DB_FILE}] initialized and ready.")

# Consumes in-memory queue arrays and writes to SQLite using transactional batches
def db_writer_worker():
    while True:
        item = log_queue.get()
        batch = [item]
        
        # Drain up to 100 available items instantly
        while not log_queue.empty() and len(batch) < 100:
            batch.append(log_queue.get_nowait())
        
        try:
            with sqlite3.connect(DB_FILE) as conn:
                conn.executemany("INSERT INTO logs (message) VALUES (?)", [(m,) for m in batch])
        except Exception as e:
            print(f"[!] SQLite Batch Write Error: {e}")
        
        for _ in batch:
            log_queue.task_done()

def udp_listener_worker():
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(("127.0.0.1", UDP_PORT))
    print(f"[*] UDP Receiver socket listening on loopback port: {UDP_PORT}")
    
    while True:
        data, _ = sock.recvfrom(2048)
        raw_msg = data.decode('utf-8', errors='ignore').strip()
        if raw_msg:
            log_queue.put(raw_msg)

class NativeLogServer(BaseHTTPRequestHandler):
    # Quiet server console output spamming
    def log_message(self, format, *args):
        return

    def do_GET(self):
        parsed_url = urlparse(self.path)
        
        # Route API queries
        if parsed_url.path == "/api/logs":
            query_params = parse_qs(parsed_url.query)
            filter_text = query_params.get('search', [''])[0].strip()
            
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.end_headers()
            
            with sqlite3.connect(DB_FILE) as conn:
                cursor = conn.cursor()
                if filter_text:
                    cursor.execute("""
                        SELECT timestamp, message FROM logs 
                        WHERE message LIKE ? 
                        ORDER BY id DESC LIMIT 250
                    """, (f"%{filter_text}%",))
                else:
                    cursor.execute("SELECT timestamp, message FROM logs ORDER BY id DESC LIMIT 250")
                
                logs = [{"time": r[0], "text": r[1]} for r in cursor.fetchall()]
                self.wfile.write(json.dumps(logs).encode('utf-8'))
                
        # Serve Dashboard Asset
        elif parsed_url.path == "/" or parsed_url.path == "/index.html":
            self.send_response(200)
            self.send_header("Content-Type", "text/html")
            self.end_headers()
            try:
                with open("index.html", "rb") as f:
                    self.wfile.write(f.read())
            except FileNotFoundError:
                self.wfile.write(b"HTML UI file 'index.html' not found in directory.")
        else:
            self.send_response(404)
            self.end_headers()

def run_web_server():
    server = HTTPServer(("127.0.0.1", HTTP_PORT), NativeLogServer)
    print(f"[*] Web Server running. Open offline browser dashboard at: http://localhost:{HTTP_PORT}")
    server.serve_forever()

if __name__ == "__main__":
    init_db()
    threading.Thread(target=db_writer_worker, daemon=True).start()
    threading.Thread(target=udp_listener_worker, daemon=True).start()
    run_web_server()
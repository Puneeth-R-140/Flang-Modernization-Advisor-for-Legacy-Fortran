import http.server
import socketserver
import json
import subprocess
import os

PORT = 8000
DIRECTORY = os.path.join(os.path.dirname(__file__), "web")

class ModernizationAdvisorHandler(http.server.SimpleHTTPRequestHandler):
    def do_POST(self):
        if self.path == '/api/analyze':
            content_length = int(self.headers['Content-Length'])
            post_data = self.rfile.read(content_length)
            
            try:
                data = json.loads(post_data.decode('utf-8'))
                file_content = data.get('content', '')
                filename = data.get('filename', 'input.f90')
                
                temp_dir = os.path.join(os.path.dirname(__file__), "temp_build")
                os.makedirs(temp_dir, exist_ok=True)
                
                temp_file_path = os.path.join(temp_dir, filename)
                with open(temp_file_path, "w", encoding="utf-8") as f:
                    f.write(file_content)
                
                advisor_exe = os.path.join(os.path.dirname(__file__), "build", "Debug", "flang-modernization-advisor.exe")
                plan_json_path = os.path.join(temp_dir, "plan.json")
                
                cmd = [advisor_exe, temp_file_path, "--output", plan_json_path]
                result = subprocess.run(cmd, capture_output=True, text=True, check=False)
                
                response_data = {}
                if result.returncode == 0 and os.path.exists(plan_json_path):
                    with open(plan_json_path, "r", encoding="utf-8") as f:
                        response_data = json.load(f)
                    response_data["sourceLines"] = file_content.splitlines()
                else:
                    response_data = {
                        "error": "Failed to run advisor binary",
                        "details": result.stderr,
                        "code": result.returncode
                    }
                
                try:
                    if os.path.exists(temp_file_path):
                        os.remove(temp_file_path)
                    if os.path.exists(plan_json_path):
                        os.remove(plan_json_path)
                except Exception:
                    pass
                
                self.send_response(200)
                self.send_header('Content-Type', 'application/json')
                self.send_header('Access-Control-Allow-Origin', '*')
                self.end_headers()
                self.wfile.write(json.dumps(response_data).encode('utf-8'))
                
            except Exception as e:
                self.send_response(500)
                self.send_header('Content-Type', 'application/json')
                self.end_headers()
                self.wfile.write(json.dumps({"error": str(e)}).encode('utf-8'))
        else:
            self.send_response(404)
            self.end_headers()

    def do_OPTIONS(self):
        self.send_response(200)
        self.send_header('Access-Control-Allow-Origin', '*')
        self.send_header('Access-Control-Allow-Methods', 'POST, GET, OPTIONS')
        self.send_header('Access-Control-Allow-Headers', 'Content-Type')
        self.end_headers()

if __name__ == '__main__':
    os.makedirs(DIRECTORY, exist_ok=True)
    
    # Simple lambda adapter to pass the directory configuration in a backward-compatible way
    handler = lambda *args, **kwargs: ModernizationAdvisorHandler(*args, directory=DIRECTORY, **kwargs)
    
    # Allow address reuse
    socketserver.TCPServer.allow_reuse_address = True
    with socketserver.TCPServer(("", PORT), handler) as httpd:
        print(f"Serving Flang Modernization Advisor UI at http://localhost:{PORT}")
        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            print("\nShutting down server.")

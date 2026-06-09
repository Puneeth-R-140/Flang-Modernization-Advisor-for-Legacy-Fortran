import http.server
import socketserver
import json
import subprocess
import os
import shutil

PORT = 8000
DIRECTORY = os.path.join(os.path.dirname(__file__), "web")

class ModernizationAdvisorHandler(http.server.SimpleHTTPRequestHandler):
    def do_POST(self):
        print(f"do_POST called for path: {self.path}")
        if self.path == '/api/analyze':
            content_length = int(self.headers['Content-Length'])
            print(f"Content-Length: {content_length}")
            post_data = self.rfile.read(content_length)
            print("Post data read successfully")
            
            try:
                data = json.loads(post_data.decode('utf-8'))
                print("JSON parsed successfully")
                
                files_to_write = []
                if "files" in data:
                    files_to_write = data["files"]
                else:
                    files_to_write = [{
                        "content": data.get('content', ''),
                        "filename": data.get('filename', 'input.f90')
                    }]
                
                temp_dir = os.path.join(os.path.dirname(__file__), "temp_build")
                if os.path.exists(temp_dir):
                    shutil.rmtree(temp_dir)
                os.makedirs(temp_dir, exist_ok=True)
                
                for f_info in files_to_write:
                    f_path = os.path.join(temp_dir, f_info["filename"])
                    os.makedirs(os.path.dirname(f_path), exist_ok=True)
                    with open(f_path, "w", encoding="utf-8") as f:
                        f.write(f_info["content"])
                
                advisor_exe = os.path.join(os.path.dirname(__file__), "build", "Debug", "flang-modernization-advisor.exe")
                # Fallback to Release path or standard Linux binary if debug exe doesn't exist
                if not os.path.exists(advisor_exe):
                    advisor_exe_release = os.path.join(os.path.dirname(__file__), "build", "Release", "flang-modernization-advisor.exe")
                    if os.path.exists(advisor_exe_release):
                        advisor_exe = advisor_exe_release
                    else:
                        advisor_exe = os.path.join(os.path.dirname(__file__), "build", "flang-modernization-advisor")
                
                plan_json_path = os.path.join(temp_dir, "plan.json")
                
                input_arg = temp_dir if len(files_to_write) > 1 else os.path.join(temp_dir, files_to_write[0]["filename"])
                
                cmd = [advisor_exe, input_arg, "--output", plan_json_path]
                print(f"Running command: {' '.join(cmd)}")
                result = subprocess.run(cmd, capture_output=True, text=True, check=False)
                print(f"Command finished with return code {result.returncode}")
                
                response_data = {}
                if result.returncode == 0 and os.path.exists(plan_json_path):
                    with open(plan_json_path, "r", encoding="utf-8") as f:
                        response_data = json.load(f)
                    
                    if len(files_to_write) > 1:
                        response_data["files"] = files_to_write
                    else:
                        response_data["sourceLines"] = files_to_write[0]["content"].splitlines()
                else:
                    response_data = {
                        "error": "Failed to run advisor binary",
                        "details": result.stderr if result.stderr else result.stdout,
                        "code": result.returncode
                    }
                
                try:
                    if os.path.exists(temp_dir):
                        shutil.rmtree(temp_dir)
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
    handler = lambda *args, **kwargs: ModernizationAdvisorHandler(*args, directory=DIRECTORY, **kwargs)
    socketserver.TCPServer.allow_reuse_address = True
    with socketserver.TCPServer(("", PORT), handler) as httpd:
        print(f"Serving Flang Modernization Advisor UI at http://localhost:{PORT}")
        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            print("\nShutting down server.")

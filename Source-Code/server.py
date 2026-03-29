from flask import Flask, request, jsonify
import subprocess
import os

app = Flask(__name__, static_folder='.', static_url_path='')

@app.route('/')
def index():
    return app.send_static_file('index.html')

@app.route('/move', methods=['POST'])
def get_move():
    data = request.json
    fen = data.get('fen')
    

    try:
        engine_process = subprocess.run(
            ['main.exe', fen], 
            capture_output=True, 
            text=True,
            shell=True 
        )
        
        best_move = engine_process.stdout.strip()
        print(f"Engine thought: {best_move}") 
        return jsonify({'move': best_move})
    except Exception as e:
        print(f"Error running main.exe: {e}")
        return jsonify({'error': str(e)}), 500

if __name__ == '__main__':
    print("Chess server running at http://127.0.0.1:5000")
    app.run(port=5000)
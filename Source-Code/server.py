from flask import Flask, render_template, request, jsonify
from pathlib import Path
import subprocess

app = Flask(__name__)

@app.route('/')
def index():
    return render_template('index.html')

ENGINE_PATH = Path(__file__).resolve().parent / "main.exe"

@app.route('/move', methods=['POST'])
def move():
    fen = request.form.get('fen')
    try:
        result = subprocess.run([ENGINE_PATH, fen], capture_output=True, text=True)
        
        move_found = result.stdout.strip()
        print(f"DEBUG: Raw Output: '{move_found}'") 
        
        return jsonify({'move': move_found if move_found else "none"})
    except Exception as e:
        print(f"Py Error: {e}")
        return jsonify({'error': str(e)})
if __name__ == '__main__':
    app.run(debug=True)
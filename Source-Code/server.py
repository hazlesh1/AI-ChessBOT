from flask import Flask, render_template, request, jsonify
import subprocess

app = Flask(__name__)

@app.route('/')
def index():
    return render_template('index.html')

# Use the 'r' before the string to handle Windows backslashes correctly!
ENGINE_PATH = r'C:\Users\leogi\Downloads\AI-ChessBOT\Source-Code\main.exe'

@app.route('/move', methods=['POST'])
def move():
    fen = request.form.get('fen')
    try:
        # We point directly to the file so Windows can't miss it
        result = subprocess.run([ENGINE_PATH, fen], capture_output=True, text=True)
        
        move_found = result.stdout.strip()
        print(f"DEBUG: Raw Engine Output: '{move_found}'") 
        
        return jsonify({'move': move_found if move_found else "none"})
    except Exception as e:
        print(f"Python Error: {e}")
        return jsonify({'error': str(e)})
if __name__ == '__main__':
    app.run(debug=True)
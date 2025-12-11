from flask import Flask, render_template, request, redirect
import sqlite3
from database_service import add_mapping, delete_mapping, add_device

app = Flask(__name__)

@app.route('/')
def index():
    """Hiển thị trang cấu hình ánh xạ"""
    conn = sqlite3.connect('modbus_mapping.db')
    cursor = conn.cursor()
    cursor.execute("SELECT * FROM mapping")
    rows = cursor.fetchall()
    mappings = [{'tcp_address': row[0], 'rtu_id': row[1], 'rtu_address': row[2]} for row in rows]
    conn.close()
    return render_template('templates_index.html', mappings=mappings)

#======================================================================================================
@app.route('/mapping')
def mapping_page():
    conn = sqlite3.connect('modbus_mapping.db')
    cursor = conn.cursor()
    cursor.execute("SELECT * FROM mapping")
    rows = cursor.fetchall()
    mappings = [{'tcp_address': row[0], 'rtu_id': row[1], 'rtu_address': row[2]} for row in rows]
    conn.close()
    return render_template('templates_index.html', mappings=mappings)


#======================================================================================================
@app.route('/mapping')
@app.route('/add_mapping', methods=['POST'])
def add_mapping_route():
    """Xử lý yêu cầu thêm ánh xạ từ form"""
    tcp_address = int(request.form['tcp_address'])
    rtu_id = int(request.form['rtu_id'])
    rtu_address = int(request.form['rtu_address'])
    add_mapping(tcp_address, rtu_id, rtu_address)
    return redirect('/')

#======================================================================================================
@app.route('/mapping')
@app.route('/delete_mapping/<int:tcp_address>', methods=['POST'])
def delete_mapping_route(tcp_address):
    """Xử lý yêu cầu xóa ánh xạ dựa trên tcp_address"""
    delete_mapping(tcp_address)
    return redirect('/')

#======================================================================================================
@app.route('/add_device', methods=['POST'])
def add_device_route():
    ip_address   = request.form['ip_address']
    tcp_port     = int(request.form['tcp_port'])
    device_name  = request.form['device_name']
    device_model = request.form['device_model']
    device_type  = request.form['device_type']

    # gọi hàm add_device trong database_service
    add_device(
        id = None,  # để None vì id AUTOINCREMENT
        update = None,  # DB tự cập nhật thời gian
        ip_address = ip_address,
        tcp_port = tcp_port,
        device_name = device_name,
        device_model = device_model,
        device_type = device_type
    )

    return redirect('/')  # quay lại trang chính sau khi thêm

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=8080, debug=True)


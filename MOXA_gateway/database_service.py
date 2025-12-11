import sqlite3
import logging

# Cấu hình logging vào file
logging.basicConfig(filename='database_service.log', level=logging.INFO,
                    format='%(asctime)s %(message)s')

#======================================================================================================
#======================= Function for initializing database ===========================================
def init_db():
    """Initializing database""" # docstring, No effect on code execution
    conn = sqlite3.connect('modbus_mapping.db')
    cursor = conn.cursor()

    # mapping table: tcp_address, rtu_id, rtu_address
    cursor.execute('''CREATE TABLE IF NOT EXISTS mapping 
                     (tcp_address INTEGER PRIMARY KEY, 
                      rtu_id INTEGER, 
                      rtu_address INTEGER)''')
    
    # logs table: timestamp, service, message
    cursor.execute('''CREATE TABLE IF NOT EXISTS logs 
                     (timestamp TEXT, 
                      service TEXT, 
                      message TEXT)''')
    
    # info_devices table: id, update, ip_address, tcp_port, device_name, device_model, device_type
    # CURRENT_TIMESTAMP -> UTC time
    # datetime('now', 'localtime') -> Local time
    cursor.execute(''' CREATE TABLE IF NOT EXISTS info_devices
                   (id INTEGER PRIMARY KEY AUTOINCREMENT,
                    update DATETIME DEFAULT datetime('now', 'localtime'),
                    ip_address TEXT,
                    tcp_port INTEGER,
                    device_name TEXT,
                    device_model TEXT,
                    device_type TEXT)''')
    
    # data table: tcp_address, tcp_port, function_code, start_address, quantity, description
    # contain data to read from Modbus TCP devices
    cursor.execute(''' CREATE TABLE IF NOT EXISTS get_data
                   (tcp_address INTEGER PRIMARY KEY,
                   tcp_port INTEGER,
                   function_code INTEGER,
                   start_address INTEGER,
                   quantity INTEGER,
                   description TEXT)''')
    conn.commit()
    conn.close()

#======================================================================================================
#======================= Functinons for work with adding new devices ==================================
def add_device(id, update, ip_address, tcp_port, device_name, device_model, device_type):
    conn = sqlite3.connect('modbus_mapping.db')
    cursor = conn.cursor()
    cursor.execute("INSERT OR REPLACE INTO device_info VALUES (?, ?, ?, ?, ?, ?, ?)",
                   (id, update, ip_address, tcp_port, device_name, device_model, device_type))
    conn.commit()
    conn.close()

def edit_device():
    pass

def get_devices():
    conn = sqlite3.connect('modbus_mapping.db')
    cursor = conn.cursor()
    cursor.execute("SELECT * FROM device_info")
    rows = cursor.fetchall()
    conn.close()
    return rows  # list of (id, name, ip_address, port)

def delete_device(device_id):
    conn = sqlite3.connect('modbus_mapping.db')
    cursor = conn.cursor()
    cursor.execute("DELETE FROM device_info WHERE id = ?", (device_id,))
    conn.commit()
    conn.close()

#======================================================================================================
#======================= Functinons for working with database =========================================
def add_mapping(tcp_address, rtu_id, rtu_address):
    """Thêm hoặc cập nhật ánh xạ"""
    conn = sqlite3.connect('modbus_mapping.db')
    cursor = conn.cursor()
    cursor.execute("INSERT OR REPLACE INTO mapping VALUES (?, ?, ?)", 
                   (tcp_address, rtu_id, rtu_address))
    conn.commit()
    conn.close()
    logging.info("Added mapping: TCP {} -> RTU ID {}, Address {}".format(tcp_address, rtu_id, rtu_address))

def get_mapping(tcp_address):
    """Lấy thông tin ánh xạ theo địa chỉ TCP"""
    conn = sqlite3.connect('modbus_mapping.db')
    cursor = conn.cursor()
    cursor.execute("SELECT rtu_id, rtu_address FROM mapping WHERE tcp_address = ?", (tcp_address,))
    result = cursor.fetchone()
    conn.close()
    return result  # (rtu_id, rtu_address) hoặc None

def delete_mapping(tcp_address):
    """Xóa một ánh xạ khỏi database dựa trên tcp_address"""
    conn = sqlite3.connect('modbus_mapping.db')
    cursor = conn.cursor()
    cursor.execute("DELETE FROM mapping WHERE tcp_address = ?", (tcp_address,))
    conn.commit()
    conn.close()

#======================================================================================================
#======================================= Function for work with log fie ===============================
def add_log(service, message):
    """Thêm log giao tiếp"""
    conn = sqlite3.connect('modbus_mapping.db')
    cursor = conn.cursor()
    cursor.execute("INSERT INTO logs VALUES (datetime('now', 'localtime'), ?, ?)", (service, message))
    conn.commit()
    conn.close()
    logging.info("Log [{}]: {}".format(service, message))

if __name__ == "__main__":
    init_db()


import socket
import struct
import signal
import sys

HOST = '127.0.0.1'
PORT = 1502

transaction_id = 1
protocol_id = 0
length = 6  # Số lượng byte của phần dữ liệu
rtu_id = 10
address = 41060
function = 3
quantity = 2

# Đóng gói thành 7 trường 2 byte (big-endian)
packet = struct.pack('!7H', transaction_id, protocol_id, length, rtu_id, address, function, quantity)

# Biến toàn cục lưu socket
sock = None


try:
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect((HOST, PORT))
    print("[Client] Successful connect at {}:{}".format(HOST, PORT))

    sock.sendall(packet)
    print("[Client] Sent Modbus TCP packet.")
    sock.settimeout(5)  # Đặt timeout cho việc nhận dữ liệu
    try:
        response = sock.recv(1024)
        print("[Client] Response from server (raw bit):", response)
        if response:
            response_values = list(response)
            print("[Client] response is :", response_values)
    except socket.timeout:
        print("[Client] No response received from server.")
except ConnectionRefusedError:
    print("[Client] Disconnect to server at {}:{}".format(HOST, PORT))

except Exception as e:
    print("[Client] Error at:", str(e))

finally:
    if sock:
        sock.close()
        print("[Client] Socket closed.")

import socket
import json


class GtfsProcSocket:
    def __init__(self, host: str, port: int):
        self.host_name = host
        self.host_port = port

    def req_resp(self, command: str) -> dict:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
            sock.connect((self.host_name, self.host_port))
            cmdb = bytearray()
            cmdb.extend(command.encode())
            sock.sendall(cmdb)

            # Receive the data until the termination character (\n)
            buff = ''
            # buff = bytes()
            while True:
                data = sock.recv(1048576)
                buff = buff + data.decode('utf-8')
                # buff = buff + sock.recv(1024)
                if '\n' in buff:
                    break

            json_dec = json.loads(buff)
            # json_dec = json.loads(buff.decode('utf-8'))
            return json_dec

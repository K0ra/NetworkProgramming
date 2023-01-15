import threading
import socket

PORT = 8000

class Threading_socket():
    def __init__(self, gui):
        super().__init__()
        self.dataReceive = ""
        self.conn = None
        self.gui = gui
        self.name = ""

    def clientAction(self, inputIP):
        self.name = "client"
        print("client connect ...............")
        HOST = inputIP  # Server address
        self.conn = socket.socket(
            socket.AF_INET, socket.SOCK_STREAM)
        self.conn.connect((HOST, PORT))
        self.gui.notification("Connected to", str(HOST))
        t1 = threading.Thread(target=self.client)  # Create client thread
        t1.start()

    def client(self):
        while True:
            self.dataReceive = self.conn.recv(1024).decode()  # Read data from server
            if(self.dataReceive != ""):
                who = self.dataReceive.split("|")[0]
                action = self.dataReceive.split("|")[1]
                # turn = self.dataReceive.split("|")[2]
                if(action == "hit" and who == "server"):
                    #     print(self.dataReceive)
                    x = int(self.dataReceive.split("|")[2])
                    y = int(self.dataReceive.split("|")[3])
                    self.gui.handleButton(x, y)
                if(action == "Undo" and who == "server"):
                    self.gui.Undo(False)
            self.dataReceive = ""

    def serverAction(self):
        self.name = "server"
        # Set up an address of currently-used computer
        HOST = socket.gethostbyname(socket.gethostname())
        print("Make server.........." + HOST)
        self.gui.notification("Gui IP chp ban", str(HOST))
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.bind((HOST, PORT))
        s.listen(1)  # Set up 1 concurrent connection
        self.conn, addr = s.accept()  # accept connection and return parameters
        t2 = threading.Thread(target=self.server, args=(addr, s))
        t2.start()

    def server(self, addr, s):
        try:
            # Print the client's address
            print('Connected by', addr)
            while True:
                # Read/parse the data sent by the client
                self.dataReceive = self.conn.recv(1024).decode()
                if(self.dataReceive != ""):
                    turn = self.dataReceive.split("|")[0]
                    action = self.dataReceive.split("|")[1]
                    print(action)
                    if(action == "hit" and turn == "client"):
                        x = int(self.dataReceive.split("|")[2])
                        y = int(self.dataReceive.split("|")[3])
                        self.gui.handleButton(x, y)
                    if(action == "Undo" and turn == "client"):
                        self.gui.Undo(False)
                self.dataReceive = ""
        finally:
            s.close()

    def sendData(self, data):
        # Send data to server
        self.conn.sendall(str("{}|".format(self.name) + data).encode())

import tkinter as tk
from functools import partial
from tkinter import messagebox

from threading_socket import Threading_socket

Ox = 20     # Number of cells on the X-axis
Oy = 20     # Number of cells on the Y-axis

class Window(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("Caro game")
        self.Buts = {}
        self.memory = []
        self.Threading_socket = Threading_socket(self)
        print(self.Threading_socket.name)
    
    def notification(self, title, msg):
        messagebox.showinfo(str(title), str(msg))

    def handleButton(self, x, y):
        # Check if the cell has a null character or not
        if self.Buts[x, y]['text'] == "":
            if self.memory.count([x, y]) == 0:
                self.memory.append([x, y])
            
            turn = (len(self.memory) % 2)
            
            if turn == 1:
                self.Buts[x, y]['text'] = 'O'
                self.Threading_socket.sendData("{}|{}|{}|{}|".format("hit", turn, x, y))
                if(self.checkWin(x, y, "O")):
                    self.notification("Winner", "O")
                    self.newGame()
            else:
                print(self.Threading_socket.name)
                self.Buts[x, y]['text'] = 'X'
                self.Threading_socket.sendData("{}|{}|{}|{}|".format("hit", turn, x, y))
                if(self.checkWin(x, y, "X")):
                    self.notification("Winner", "X")
                    self.newGame()

    def checkWin(self, x, y, XO):
        # check the row (up and down)
        count = 0
        i, j = x, y
        while(j < Ox and self.Buts[i, j]["text"] == XO):
            count += 1
            j += 1
        j = y
        while(j >= 0 and self.Buts[i, j]["text"] == XO):
            count += 1
            j -= 1
        if count >= 6:
            return True
        
        # check the column (left and right)
        count = 0
        i, j = x, y
        while(i < Oy and self.Buts[i, j]["text"] == XO):
            count += 1
            i += 1
        i = x
        while(i >= 0 and self.Buts[i, j]["text"] == XO):
            count += 1
            i -= 1
        if count >= 6:
            return True

        # check diagonal y=-x
        count = 0
        i, j = x, y
        while(i >= 0 and j < Ox and self.Buts[i, j]["text"] == XO):
            count += 1
            i -= 1
            j += 1
        i, j = x, y
        while(i <= Oy and j >= 0 and self.Buts[i, j]["text"] == XO):
            count += 1
            i += 1
            j -= 1
        if count >= 6:
            return True
        
        # check diagonal y=x
        count = 0
        i, j = x, y
        while(i < Ox and j < Oy and self.Buts[i, j]["text"] == XO):
            count += 1
            i += 1
            j += 1
        i, j = x, y
        while(i >= 0 and j >= 0 and self.Buts[i, j]["text"] == XO):
            count += 1
            i -= 1
            j -= 1
        if count >= 6:
            return True
        
        return False

    def newGame(self):
        for x in range(Ox):
            for y in range(Oy):
                self.Buts[x, y]["text"] = ""

    def Undo(self, synchronized):
        if(len(self.memory) > 0):
            x = self.memory[len(self.memory) - 1][0]
            y = self.memory[len(self.memory) - 1][1]
            # print(x,y)
            self.Buts[x, y]['text'] = ""
            self.memory.pop()
            if synchronized == True:
                self.Threading_socket.sendData("{}|".format("Undo"))
            print(self.memory)
        else:
            print("No character")
    
    def showFrame(self):
        frame1 = tk.Frame(self)
        frame1.pack()
        frame2 = tk.Frame(self)
        frame2.pack()

        Undo = tk.Button(frame1, text="Undo", width=10,  # Undo/back button
                         command=partial(self.Undo, synchronized=True))
        Undo.grid(row=0, column=0, padx=30)

        tk.Label(frame1, text="IP", pady=4).grid(row=0, column=1)
        inputIp = tk.Entry(frame1, width=20)  # IP address input box
        inputIp.grid(row=0, column=2, padx=5)

        connectBT = tk.Button(frame1, text="Connect", width=10,
                              command=lambda: self.Threading_socket.clientAction(inputIp.get()))
        connectBT.grid(row=0, column=3, padx=3)

        makeServerBT = tk.Button(frame1, text="MakeServer", width=10,  # Server create button
                               command=lambda: self.Threading_socket.serverAction())
        makeServerBT.grid(row=0, column=4, padx=30)

        for x in range(Ox):   # create matrix of buttons (Ox * Oy)
            for y in range(Oy):
                self.Buts[x, y] = tk.Button(frame2, font=('arial', 15, 'bold'), height=1, width=2,
                                            borderwidth=2, command=partial(self.handleButton, x=x, y=y))
                self.Buts[x, y].grid(row=x, column=y)


if __name__ == "__main__":
    window = Window()
    window.showFrame()
    window.mainloop()
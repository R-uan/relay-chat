from websockets.sync.client import connect 


def hello():
    with connect("ws://localhost:8081") as websocket:
        websocket.send("Hello World!")
        message = websocket.recv()
        print(message)


if __name__ == "__main__":
    hello()

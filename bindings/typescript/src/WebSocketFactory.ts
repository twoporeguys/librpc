export class WebSocketFactory {
    public CLOSED = WebSocket.CLOSED;
    public CLOSING = WebSocket.CLOSING;
    public CONNECTING = WebSocket.CONNECTING;
    public OPEN = WebSocket.OPEN;

    public get(url: string): WebSocket {
        return new WebSocket(url);
    }
}

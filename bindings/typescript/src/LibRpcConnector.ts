import {Observable} from 'rxjs/Observable';
import {Subject} from 'rxjs/Subject';
import {v4} from './uuidv4';
import {decode, encode, createCodec, Codec} from 'msgpack-lite';
import {Subscription} from 'rxjs/Subscription';

export class LibRpcConnector {
    private ws: WebSocket;
    private messages$: Subject<any>;
    private messageBuffer: Buffer[];
    private keepAlive: Subscription;
    private codec: Codec;

    public constructor(private url: string, private isDebugEnabled: boolean = false) {
        this.codec = createCodec();
        this.codec.addExtUnpacker(0x01, (buffer: Uint8Array) => {
            return new Date(new DataView(buffer.buffer, 0).getUint32(0, true) * 1000);
        });
        this.messages$ = new Subject<any>();
        this.messageBuffer = [];

        if (this.isDebugEnabled) {
            // tslint:disable-next-line:no-console
            this.messages$.subscribe((message: any) => console.log('<-', JSON.stringify(message)));
        }
        this.connect();

    }

    public send(message: any): Observable<any> {
        if (this.isDebugEnabled) {
            // tslint:disable-next-line:no-console
            console.log('->', JSON.stringify(message));
        }
        const data = encode(message);
        switch (this.ws.readyState) {
            case WebSocket.OPEN:
                this.ws.send(data);
                break;
            case WebSocket.CLOSED:
            case WebSocket.CLOSING:
                this.connect();
                this.messageBuffer.push(data);
                break;
            case WebSocket.CONNECTING:
                this.messageBuffer.push(data);
                break;
        }
        return this.messages$;
    }

    private connect() {
        this.ws = new WebSocket(this.url);
        this.ws.binaryType = 'arraybuffer';
        this.ws.onopen = () => {
            let data = this.messageBuffer.shift();
            while (data) {
                this.ws.send(data);
                data = this.messageBuffer.shift();
            }
            this.startKeepAlive();
            this.ws.onclose = () => {
                this.stopKeepAlive();
            };
            this.ws.onmessage = (message: MessageEvent) => {
                this.messages$.next(decode(new Uint8Array(message.data), {codec: this.codec}));
            };
        };
    }

    private startKeepAlive() {
        this.stopKeepAlive();
        this.keepAlive = Observable
            .interval(30000)
            .subscribe(() => {
                this.ws.send(encode({
                    id: v4(),
                    namespace: 'rpc',
                    name: 'call',
                    args: {
                        path: '/server',
                        interface: 'com.twoporeguys.momd.Builtin',
                        method: 'ping',
                        args: []
                    }
                }));
            });
    }

    private stopKeepAlive() {
        if (this.keepAlive) {
            this.keepAlive.unsubscribe();
        }
    }
}

import {Codec, createCodec, decode, encode} from 'msgpack-lite';
import 'rxjs/add/observable/interval';
import 'rxjs/add/observable/never';
import 'rxjs/add/observable/of';
import 'rxjs/add/operator/switchMap';
import {BehaviorSubject} from 'rxjs/BehaviorSubject';
import {Observable} from 'rxjs/Observable';
import {interval} from 'rxjs/observable/interval';
import {Subject} from 'rxjs/Subject';
import {LibRpcRequest} from './model/LibRpcRequest';
import {v4} from './uuidv4';
import {WebSocketFactory} from './WebSocketFactory';

export class LibRpcConnector {
    public isConnected$ = new BehaviorSubject<boolean>(false);

    private EXT_UNPACKERS: Map<number, (buffer: Uint8Array) => any> = new Map([
        [0x01, (buffer: Uint8Array) => new Date(new DataView(buffer.buffer, 0).getUint32(0, true) * 1000)],
        [0x04, (buffer: Uint8Array) => decode(buffer, {codec: this.codec})],
    ]);

    private ws: WebSocket;
    private messages$: Subject<any>;
    private messageBuffer: Buffer[];
    private codec: Codec;

    public constructor(
        private url: string,
        private isDebugEnabled: boolean = false,
        private webSocketFactory: WebSocketFactory = new WebSocketFactory()
    ) {
        this.codec = createCodec();
        this.EXT_UNPACKERS.forEach(
            (unPacker: (buffer: Uint8Array) => any, eType: number) => this.codec.addExtUnpacker(eType, unPacker)
        );
        this.messages$ = new Subject<any>();
        this.messageBuffer = [];

        if (this.isDebugEnabled) {
            // tslint:disable-next-line:no-console
            this.messages$.subscribe((message: any) => console.log('RECV\n', JSON.stringify(message)));
        }
        this.startKeepAlive();
        this.connect();

    }

    public send(message: LibRpcRequest): Observable<any> {
        if (this.isDebugEnabled) {
            // tslint:disable-next-line:no-console
            console.log('SEND\n', JSON.stringify(message));
        }
        const data = encode(message);
        switch (this.ws.readyState) {
            case this.webSocketFactory.OPEN:
                this.ws.send(data);
                break;
            case this.webSocketFactory.CLOSED:
            case this.webSocketFactory.CLOSING:
                this.connect();
                this.messageBuffer.push(data);
                break;
            case this.webSocketFactory.CONNECTING:
                this.messageBuffer.push(data);
                break;
        }
        return this.messages$;
    }

    public listen(): Observable<any> {
        if (!this.ws ||
            this.ws.readyState === this.webSocketFactory.CLOSED ||
            this.ws.readyState === this.webSocketFactory.CLOSING
        ) {
            this.connect();
        }
        return this.messages$;
    }

    private connect() {
        this.ws = this.webSocketFactory.get(this.url);
        this.isConnected$.next(this.ws.readyState === this.webSocketFactory.OPEN);
        this.ws.binaryType = 'arraybuffer';
        this.ws.onopen = () => {
            this.isConnected$.next(true);
            this.emptyMessageQueue();

            this.ws.onclose = () => {
                this.isConnected$.next(false);
            };

            this.ws.onmessage = (message: MessageEvent) => {
                this.messages$.next(
                    typeof message.data === 'object' ?
                        decode(new Uint8Array(message.data), {codec: this.codec}) : JSON.parse(message.data)
                );
            };
        };
    }

    private emptyMessageQueue() {
        let data = this.messageBuffer.shift();
        while (data) {
            this.ws.send(data);
            data = this.messageBuffer.shift();
        }
    }

    private startKeepAlive() {
        interval(30000).subscribe(() => this.send({
            id: v4(),
            namespace: 'rpc',
            name: 'call',
            args: {
                path: '/server',
                interface: 'com.twoporeguys.momd.Builtin',
                method: 'ping',
                args: [],
            },
        }));
    }
}

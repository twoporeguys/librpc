import {expect} from 'chai';
import 'mocha';
import 'rxjs/add/observable/from';
import 'rxjs/add/observable/of';
import 'rxjs/add/operator/filter';
import 'rxjs/add/operator/observeOn';
import {Observable} from 'rxjs/Observable';

import {Observer} from 'rxjs/Observer';
import {take} from 'rxjs/operators';
import {Scheduler} from 'rxjs/Rx';
import {LibRpcConnector} from '../src/LibRpcConnector';
import {WebSocketFactory} from '../src/WebSocketFactory';

const noop = () => {/* nothing to see here */};

describe('LibRpcConnector', () => {
    let webSocketFactoryMock: WebSocketFactory;

    beforeEach(() => {
        webSocketFactoryMock = {
            CLOSED: 0,
            CLOSING: 1,
            CONNECTING: 2,
            OPEN: 3,
            get: () => ({
                send: noop
            } as any)
        };
    });

    describe('listen', () => {
        it('should return an Observable', () => {
            const connector = new LibRpcConnector('foo', false, webSocketFactoryMock);

            const result = connector.listen();

            expect(result).to.be.instanceof(Observable);
        });
    });

    describe('isConnected$', () => {
        it('should return current connection state', (done: () => void) => {
            webSocketFactoryMock.get = () => {
                return {
                    send: noop,
                    readyState: webSocketFactoryMock.OPEN
                } as any;
            };
            const connector = new LibRpcConnector('foo', false, webSocketFactoryMock);

            connector.isConnected$.pipe(
                take(1),
            ).subscribe(
                (isConnected: boolean) => {
                    expect(isConnected).to.be.true;
                },
                noop,
                done
            );
        });

        it('should notify connection changes', (done: () => void) => {
            const receivedStatuses: boolean[] = [];
            const fakeWs = {
                send: noop,
                readyState: webSocketFactoryMock.CLOSED
            } as any;
            webSocketFactoryMock.get = () => {
                return fakeWs;
            };
            const connector = new LibRpcConnector('foo', false, webSocketFactoryMock);

            connector.isConnected$.pipe(
                take(2),
            ).subscribe(
                (isConnected: boolean) => {
                    receivedStatuses.push(isConnected);
                    fakeWs.onopen();
                },
                noop,
                () => {
                    expect(receivedStatuses).to.deep.equal([
                        false,
                        true,
                    ]);
                    done();
                }
            );
        });
    });
});

import {expect} from 'chai';
import 'mocha';
import 'rxjs/add/observable/from';
import 'rxjs/add/observable/of';
import 'rxjs/add/operator/filter';
import 'rxjs/add/operator/observeOn';
import {Observable} from 'rxjs/Observable';

import {Observer} from 'rxjs/Observer';
import {Scheduler} from 'rxjs/Rx';
import {LibRpcClient} from '../src/LibRpcClient';
import {LibRpcConnector} from '../src/LibRpcConnector';

describe('LibRpcClient', () => {
    const connectorMock: LibRpcConnector = {} as LibRpcConnector;
    const noop = () => {/* nothing to see here */};

    describe('listObjectsInPath', () => {
        it('should call com.twoporeguys.librpc.Discoverable.get_instances on /', () => {
            let sentMessage: any = null;
            connectorMock.send = (message: any) => {
                sentMessage = message;
                return Observable.from([]);
            };
            const client = new LibRpcClient(connectorMock);

            client.listObjectsInPath('/', 'foo');

            expect(sentMessage).to.eql({
                id: 'foo',
                namespace: 'rpc',
                name: 'call',
                args: {
                    path: '/',
                    interface: 'com.twoporeguys.librpc.Discoverable',
                    method: 'get_instances',
                    args: []
                }
            });
        });

        it('should return all matching objects paths', (done: () => void) => {
            const objects = [
                {path: '/foo/1', description: 'Foo 1'},
                {path: '/foo/2', description: 'Foo 2'},
                {path: '/bar/1', description: 'Bar 1'},
                {path: '/foo',   description: 'Foo'},
            ];
            connectorMock.send = (message: {id: string, name: string, args: any}) => {
                return message.name === 'call' ?
                    Observable.create((observer: Observer<any>) => {
                        observer.next({id: message.id, name: 'fragment', args: {seqno: 0, fragment: objects[0]}});
                    }).observeOn(Scheduler.asap) :
                    message.args < objects.length ?
                        Observable.create((observer: Observer<any>) => {
                            observer.next({id: message.id, name: 'fragment', args: {
                                seqno: message.args, fragment: objects[message.args]}
                            });
                        }).observeOn(Scheduler.asap) :
                        Observable.create((observer: Observer<any>) => {
                            observer.next({id: message.id, name: 'end'});
                        }).observeOn(Scheduler.asap);
            };
            const client = new LibRpcClient(connectorMock);
            const resultPath: string[] = [];

            client.listObjectsInPath('/foo').subscribe(
                (objectPath: string) => resultPath.push(objectPath),
                noop,
                () => {
                    expect(resultPath.length).to.equal(3);
                    expect(resultPath).to.eql([
                        '/foo/1',
                        '/foo/2',
                        '/foo',
                    ]);
                    done();
                }
            );
        });
    });

    describe('getObject', () => {
        it('should call com.twoporeguys.librpc.Observable.get_all on /foo with interface bar', () => {
            let sentMessage: any = null;
            connectorMock.send = (message: any) => {
                sentMessage = message;
                return Observable.from([]);
            };
            const client = new LibRpcClient(connectorMock);

            client.getObject('/foo', 'bar', '1234');

            expect(sentMessage).to.eql({
                id: '1234',
                namespace: 'rpc',
                name: 'call',
                args: {
                    path: '/foo',
                    interface: 'com.twoporeguys.librpc.Observable',
                    method: 'get_all',
                    args: ['bar']
                }
            });
        });

        it('should return object built from properties', (done: () => void) => {
            connectorMock.send = (message: {id: string}) => Observable.create(
                (observer: Observer<any>) => observer.next({
                    id: message.id,
                    name: 'response',
                    args: [
                        {name: 'key1', value: 'value1'},
                        {name: 'key2', value: 'value2'},
                        {name: 'key4', value: 42},
                        {name: 'key3', value: 'value3'},
                    ]
                })
            ).observeOn(Scheduler.asap);
            const client = new LibRpcClient(connectorMock);

            client.getObject('/foo', 'bar').subscribe((object: any) => {
                expect(object).to.not.be.null;
                expect(object).to.have.property('key1').that.equal('value1');
                expect(object).to.have.property('key2').that.equal('value2');
                expect(object).to.have.property('key3').that.equal('value3');
                expect(object).to.have.property('key4').that.equal(42);
                done();
            });
        });
    });

    describe('getProperty', () => {
        it('should call com.twoporeguys.librpc.Observable.get on /foo with interface bar and property baz', () => {
            let sentMessage: any = null;
            connectorMock.send = (message: any) => {
                sentMessage = message;
                return Observable.of();
            };
            const client = new LibRpcClient(connectorMock);

            client.getProperty('/foo', 'bar', 'baz', '1234');

            expect(sentMessage).to.eql({
                id: '1234',
                namespace: 'rpc',
                name: 'call',
                args: {
                    path: '/foo',
                    interface: 'com.twoporeguys.librpc.Observable',
                    method: 'get',
                    args: ['bar', 'baz']
                }
            });
        });
    });

    describe('setProperty', () => {
        it(
            'should call com.twoporeguys.librpc.Observable.set on /foo with interface bar, property baz and value qux',
            () => {
                let sentMessage: any = null;
                connectorMock.send = (message: any) => {
                    sentMessage = message;
                    return Observable.of();
                };
                const client = new LibRpcClient(connectorMock);

                client.setProperty('/foo', 'bar', 'baz', 'qux', '1234');

                expect(sentMessage).to.eql({
                    id: '1234',
                    namespace: 'rpc',
                    name: 'call',
                    args: {
                        path: '/foo',
                        interface: 'com.twoporeguys.librpc.Observable',
                        method: 'set',
                        args: ['bar', 'baz', 'qux']
                    }
                });
        });
    });

    describe('callMethod', () => {
        it('should send rpc call with passed arguments', () => {
            let sentMessage: any = null;
            connectorMock.send = (message: any) => {
                sentMessage = message;
                return Observable.of();
            };
            const client = new LibRpcClient(connectorMock);

            client.callMethod(
                '/foo',
                'bar',
                'baz',
                ['qux', 'quux', 42],
                '1234'
            );

            expect(sentMessage).to.eql({
                id: '1234',
                namespace: 'rpc',
                name: 'call',
                args: {
                    path: '/foo',
                    interface: 'bar',
                    method: 'baz',
                    args: [
                        'qux',
                        'quux',
                        42
                    ]
                }
            });
        });

        it('should call returned observable\'s complete method on end message received', (done: () => void) => {
            connectorMock.send = (message: {id: string}) => (
                Observable.create((observer: Observer<any>) => {
                    observer.next({id: message.id, name: 'end'});
                }).observeOn(Scheduler.asap)
            );
            const client = new LibRpcClient(connectorMock);

            client.callMethod('/foo', 'bar', 'bar').subscribe(
                noop,
                noop,
                () => {
                    done();
                }
            );
        });

        it('should call returned observable\'s error method on error message received', (done: () => void) => {
            connectorMock.send = (message: {id: string}) => (
                Observable.create((observer: Observer<any>) => {
                    observer.next({id: message.id, name: 'error', args: 'FOO'});
                }).observeOn(Scheduler.asap)
            );
            const client = new LibRpcClient(connectorMock);

            client.callMethod('/foo', 'bar', 'bar').subscribe(
                noop,
                (error: any) => {
                    expect(error).to.equal('FOO');
                    done();
                },
                noop
            );
        });
    });

    describe('subscribe', () => {
        it('should subscribe to com.twoporeguys.librpc.Observable.changed events on /foo', () => {
            let sentMessage: any = null;
            connectorMock.send = (message: any) => {
                sentMessage = message;
                return Observable.of();
            };
            const client = new LibRpcClient(connectorMock);

            client.subscribe('/foo', '1234');

            expect(sentMessage).to.eql({
                id: '1234',
                namespace: 'events',
                name: 'subscribe',
                args: [{
                    path: '/foo',
                    interface: 'com.twoporeguys.librpc.Observable',
                    name: 'changed'
                }]
            });
        });

        it('should return changes only for /foo', (done: () => void) => {
            const events = [
                {
                    id: null, namespace: 'events', name: 'event',
                    args: {name: 'changed', path: '/foo', args: {name: 'key1', value: 1}}
                },
                {
                    id: null, namespace: 'events', name: 'event',
                    args: {name: 'changed', path: '/bar', args: {name: 'key1', value: 12}}
                },
                {
                    id: null, namespace: 'events', name: 'event',
                    args: {name: 'changed', path: '/foo', args: {name: 'key2', value: 321}}
                },
            ];
            connectorMock.send = () => (
                Observable.create((observer: Observer<any>) => {
                    observer.next(events[0]);
                    observer.next(events[1]);
                    observer.next(events[2]);
                    observer.complete();
                }).observeOn(Scheduler.asap)
            );
            const client = new LibRpcClient(connectorMock);
            const relevantChanges: any[] = [];

            client.subscribe('/foo').subscribe(
                (change: any) => relevantChanges.push(change),
                noop,
                () => {
                    expect(relevantChanges.length).to.equal(2);
                    expect(relevantChanges).to.eql([
                        {key1: 1},
                        {key2: 321},
                    ]);
                    done();
                }
            );
        });
    });
});

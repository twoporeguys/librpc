import {expect} from 'chai';
import 'mocha';

import 'rxjs/add/observable/from';
import 'rxjs/add/observable/of';
import 'rxjs/add/observable/timer';
import 'rxjs/add/operator/filter';
import 'rxjs/add/operator/observeOn';
import {Observable} from 'rxjs/Observable';
import {Observer} from 'rxjs/Observer';
import {Scheduler} from 'rxjs/Rx';

import {LibRpcClient} from '../src';
import {LibRpcConnector} from '../src/LibRpcConnector';
import {LibRpcEvent} from '../src/model/LibRpcEvent';
import {LibRpcRequest} from '../src/model/LibRpcRequest';
import {LibRpcResponse} from '../src/model/LibRpcResponse';

describe('LibRpcClient', () => {
    const connectorMock: LibRpcConnector = {} as LibRpcConnector;
    const noop = () => {/* nothing to see here */};

    describe('listObjectsInPath', () => {
        it('should call com.twoporeguys.librpc.Discoverable.get_instances on /', () => {
            let sentMessage: any = null;
            connectorMock.send = (message: any) => {
                sentMessage = message;
                return Observable.of({id: 'foo'});
            };
            const client = new LibRpcClient('', false, connectorMock);

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
            let resultPaths: string[] = [];
            const objects = [
                {path: '/foo/1', description: 'Foo 1'},
                {path: '/foo/2', description: 'Foo 2'},
                {path: '/bar/1', description: 'Bar 1'},
                {path: '/foo',   description: 'Foo'},
            ];
            connectorMock.send = (message: {id: string}) => Observable.of({
                id: message.id,
                name: 'response',
                args: objects
            }).observeOn(Scheduler.asap);
            const client = new LibRpcClient('', false, connectorMock);

            client.listObjectsInPath('/foo').subscribe(
                (objectPaths: string[]) => resultPaths = objectPaths,
                noop,
                () => {
                    expect(resultPaths).to.deep.equal([
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
                return Observable.of({id: '1234'});
            };
            const client = new LibRpcClient('', false, connectorMock);

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
            const client = new LibRpcClient('', false, connectorMock);

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
                return Observable.of({id: '1234'});
            };
            const client = new LibRpcClient('', false, connectorMock);

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
                    return Observable.of({id: '1234'});
                };
                const client = new LibRpcClient('', false, connectorMock);

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
                return Observable.of({id: '1234'});
            };
            const client = new LibRpcClient('', false, connectorMock);

            client.callMethod(
                '/foo',
                'bar',
                'baz',
                ['qux', 'quux', 42],
                true,
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
            const client = new LibRpcClient('', false, connectorMock);

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
            const client = new LibRpcClient('', false, connectorMock);

            client.callMethod('/foo', 'bar', 'bar').subscribe(
                noop,
                (error: any) => {
                    expect(error).to.equal('FOO');
                    done();
                },
                noop
            );
        });

        it('should iterate through fragments received', (done: () => void) => {
            const fakeData = ['foo', 'bar', 'baz', 'qux', 'quux'];
            const responses: string[] = [];
            connectorMock.send = (message: LibRpcRequest) => Observable
                .timer(100)
                .map(() => {
                    let result: any;
                    switch (message.name) {
                        case 'continue':
                            result = message.args < fakeData.length ?
                                {
                                    id: message.id,
                                    namespace: message.namespace,
                                    name: 'fragment',
                                    args: {
                                        seqno: message.args,
                                        fragment: fakeData[message.args],
                                    },
                                } :
                                {
                                    id: message.id,
                                    namespace: message.namespace,
                                    name: 'end',
                                };
                            break;
                        default:
                            result = {
                                id: message.id,
                                namespace: message.namespace,
                                name: 'fragment',
                                args: {
                                    seqno: 0,
                                    fragment: fakeData[0],
                                },
                            };
                            break;
                    }
                    return result;
                }).observeOn(Scheduler.asap);
            const client = new LibRpcClient('', false, connectorMock);

            client.callMethod('/foo', 'bar', 'bar').subscribe(
                (response: string) => {
                    responses.push(response);
                },
                noop,
                () => {
                    expect(responses).to.deep.equal(['foo', 'bar', 'baz', 'qux', 'quux']);
                    done();
                }
            );
        });

        it('should stop iterating through fragments when there is no subscriber', (done: () => void) => {
            const fakeData = ['foo', 'bar', 'baz', 'qux', 'quux'];
            let callsCount: number = 0;
            const responses: string[] = [];
            connectorMock.send = (message: LibRpcRequest) => {
                callsCount++;
                return Observable
                    .timer(100)
                    .map(() => {
                        let result: any;
                        switch (message.name) {
                            case 'continue':
                                result = message.args < fakeData.length ?
                                    {
                                        id: message.id,
                                        namespace: message.namespace,
                                        name: 'fragment',
                                        args: {
                                            seqno: message.args,
                                            fragment: fakeData[message.args],
                                        },
                                    } :
                                    {
                                        id: message.id,
                                        namespace: message.namespace,
                                        name: 'end',
                                    };
                                break;
                            default:
                                result = {
                                    id: message.id,
                                    namespace: message.namespace,
                                    name: 'fragment',
                                    args: {
                                        seqno: 0,
                                        fragment: fakeData[0],
                                    },
                                };
                                break;
                        }
                        return result;
                    }).observeOn(Scheduler.asap);
            };
            const client = new LibRpcClient('', false, connectorMock);

            client.callMethod('/foo', 'bar', 'bar')
                .take(2)
                .subscribe(
                (response: string) => {
                    responses.push(response);
                },
                noop,
                () => {
                    expect(responses).to.deep.equal(['foo', 'bar']);
                    setTimeout(() => {
                        expect(callsCount).to.equal(3);
                        done();
                    }, 1000);
                }
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
            const client = new LibRpcClient('', false, connectorMock);

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
                    args: {
                        interface: 'com.twoporeguys.librpc.Observable',
                        name: 'changed',
                        path: '/foo',
                        args: {name: 'key1', value: 1},
                    },
                },
                {
                    id: null, namespace: 'events', name: 'event',
                    args: {
                        interface: 'com.twoporeguys.librpc.Observable',
                        name: 'changed',
                        path: '/bar',
                        args: {name: 'key1', value: 12},
                    },
                },
                {
                    id: null, namespace: 'events', name: 'event',
                    args: {
                        interface: 'com.twoporeguys.librpc.Observable',
                        name: 'changed',
                        path: '/foo',
                        args: {name: 'key2', value: 321},
                    },
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
            const client = new LibRpcClient('', false, connectorMock);
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

    describe('unsubscribe', () => {
        it('should unsubscribe from com.twoporeguys.librpc.Observable.changed events on /foo', () => {
            let sentMessage: any = null;
            connectorMock.send = (message: any) => {
                sentMessage = message;
                return Observable.of();
            };
            const client = new LibRpcClient('', false, connectorMock);

            client.unsubscribe('/foo', '1234');

            expect(sentMessage).to.eql({
                id: '1234',
                namespace: 'events',
                name: 'unsubscribe',
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
                    args: {
                        interface: 'com.twoporeguys.librpc.Observable',
                        name: 'changed',
                        path: '/foo',
                        args: {name: 'key1', value: 1},
                    },
                },
                {
                    id: null, namespace: 'events', name: 'event',
                    args: {
                        interface: 'com.twoporeguys.librpc.Observable',
                        name: 'changed',
                        path: '/bar',
                        args: {name: 'key1', value: 12},
                    },
                },
                {
                    id: null, namespace: 'events', name: 'event',
                    args: {
                        interface: 'com.twoporeguys.librpc.Observable',
                        name: 'changed',
                        path: '/foo',
                        args: {name: 'key2', value: 321},
                    },
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
            const client = new LibRpcClient('', false, connectorMock);
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

    describe('subscribeToCreation', () => {
        it('should send object creations matching given interface', (done: () => void) => {
            const createdIds: string[] = [];
            connectorMock.listen = () => Observable.from<LibRpcResponse<LibRpcEvent>>([
                {
                    id: null, namespace: 'events', name: 'event',
                    args: {
                        path: '/',
                        interface: 'com.twoporeguys.librpc.Discoverable', name: 'instance_added',
                        args: {
                            path: '/foo/1',
                            interfaces: ['foo', 'bar'],
                        },
                    },
                },
                {
                    id: null, namespace: 'events', name: 'event',
                    args: {
                        path: '/',
                        interface: 'com.twoporeguys.librpc.Discoverable', name: 'instance_added',
                        args: {
                            path: '/foo/2',
                            interfaces: ['bar'],
                        },
                    },
                },
                {
                    id: null,
                    namespace: 'events',
                    name: 'event',
                    args: {
                        path: '/',
                        interface: 'com.twoporeguys.librpc.Discoverable', name: 'instance_added',
                        args: {
                            path: '/baz/2',
                            interfaces: ['baz'],
                        },
                    },
                },
                {
                    id: null, namespace: 'events', name: 'event',
                    args: {
                        path: '/foo/2',
                        interface: 'com.twoporeguys.librpc.Introspectable', name: 'interface_added',
                        args: 'foo',
                    },
                },
            ]).observeOn(Scheduler.asap);
            const client = new LibRpcClient('', false, connectorMock);

            client.subscribeToCreation('foo')
                .subscribe(
                    (id: string) => createdIds.push(id),
                    noop,
                    () => {
                        expect(createdIds).to.deep.equal([
                            '/foo/1',
                            '/foo/2'
                        ]);
                        done();
                    }
                );
        });
    });

    describe('subscribeToChanges', () => {
        it('should send object changes matching given path and interface', (done: () => void) => {
            const changes: Array<{[key: string]: any}> = [];
            connectorMock.listen = () => Observable.from<LibRpcResponse<LibRpcEvent>>([
                {
                    id: null, namespace: 'events', name: 'event',
                    args: {
                        path: '/foo/1',
                        interface: 'com.twoporeguys.librpc.Observable', name: 'changed',
                        args: {
                            interface: 'bar', name: 'baz', value: 42,
                        },
                    },
                },
                {
                    id: null, namespace: 'events', name: 'event',
                    args: {
                        path: '/foo/2',
                        interface: 'com.twoporeguys.librpc.Observable', name: 'changed',
                        args: {
                            interface: 'bar', name: 'baz', value: 123,
                        },
                    },
                },
                {
                    id: null, namespace: 'events', name: 'event',
                    args: {
                        path: '/foo/1',
                        interface: 'com.twoporeguys.librpc.Observable', name: 'changed',
                        args: {
                            interface: 'bar', name: 'qux', value: 'quux',
                        },
                    },
                },
                {
                    id: null, namespace: 'events', name: 'event',
                    args: {
                        path: '/foo/1',
                        interface: 'com.twoporeguys.librpc.Observable', name: 'changed',
                        args: {
                            interface: 'quux', name: 'bar', value: -1,
                        },
                    },
                },
            ]).observeOn(Scheduler.asap);
            const client = new LibRpcClient('', false, connectorMock);

            client.subscribeToChange('/foo/1', 'bar')
                .subscribe(
                    (change: {[key: string]: any}) => changes.push(change),
                    noop,
                    () => {
                        expect(changes).to.deep.equal([
                            {baz: 42},
                            {qux: 'quux'}
                        ]);
                        done();
                    }
                );
        });
    });

    describe('events$', () => {
        it('should send events', (done: () => void) => {
            const receivedEvents: any[] = [];
            connectorMock.listen = () => Observable.create((observer: Observer<LibRpcResponse>) => {
                observer.next({
                    id: null, namespace: 'events', name: 'event',
                    args: 'foo',
                });
                observer.next({
                    id: null, namespace: 'rpc', name: 'call',
                    args: 'bar',
                });
                observer.next({
                    id: null, namespace: 'events', name: 'event',
                    args: 'baz',
                });
                observer.complete();
            });
            const client = new LibRpcClient('', false, connectorMock);

            client.events$.subscribe(
                (event: LibRpcEvent) => receivedEvents.push(event),
                noop,
                () => {
                    expect(receivedEvents).to.deep.equal([
                        'foo',
                        'baz'
                    ]);
                    done();
                }
            );
        });
    });

});

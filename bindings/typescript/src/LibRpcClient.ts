/**
 * @module LibRpcClient
 * Client for librpc protocol.
 * @see https://github.com/twoporeguys/librpc
 */
import {assign, fromPairs, startsWith} from 'lodash';
import {Observable} from 'rxjs/Observable';
import {distinctUntilChanged, filter, map, take} from 'rxjs/operators';
import {Subject} from 'rxjs/Subject';
import {LibRpcConnector} from './LibRpcConnector';
import {LibRpcEvent, LibRpcFragment, LibRpcRequest, LibRpcResponse} from './model';
import {v4} from './uuidv4';

interface DiscoverableInstance {
    path: string;
    description: string;
}

interface Property {
    name: string;
    value: any;
}

export class LibRpcClient {
    private static isEvent(message: LibRpcResponse): boolean {
        return !!(message && message.namespace === 'events' && message.name === 'event');
    }

    private static isInstanceAddedEvent(event: LibRpcEvent, interfaceName: string): boolean {
        return event &&
            event.interface === 'com.twoporeguys.librpc.Discoverable' &&
            event.name === 'instance_added' &&
            event.args &&
            Array.isArray(event.args.interfaces) &&
            event.args.interfaces.indexOf(interfaceName) !== -1;
    }

    private static isInterfaceAddedEvent(event: LibRpcEvent, interfaceName: string): boolean {
        return event &&
            event.interface === 'com.twoporeguys.librpc.Introspectable' &&
            event.name === 'interface_added' &&
            event.args === interfaceName;
    }

    private static isChangedEvent(event: LibRpcEvent, path: string, interfaceName: string): boolean {
        return event &&
            event.interface === 'com.twoporeguys.librpc.Observable' &&
            event.name === 'changed' &&
            event.path === path &&
            event.args &&
            event.args.interface === interfaceName;
    }

    private static isObjectChangeEvent(event: LibRpcEvent): boolean {
        return event && event.interface === 'com.twoporeguys.librpc.Observable' && event.name === 'changed';
    }

    private connector: LibRpcConnector;

    /**
     * Create a new LibRpcClient.
     * @param {string} url librpc server's URL
     * @param {boolean} isDebugEnabled Should messages sent and received be logged in the console?
     * @param {LibRpcConnector} connector Override url and re-use provided LibRpcConnector instance
     */
    public constructor(url: string, isDebugEnabled: boolean = false, connector?: LibRpcConnector) {
        this.connector = (connector || new LibRpcConnector(url, isDebugEnabled));
    }

    /**
     * Streams payload of 'events.event' messages.
     * @returns {Observable<LibRpcEvent>}
     */
    public get events$(): Observable<LibRpcEvent> {
        return this.connector.listen().pipe(
            filter(LibRpcClient.isEvent),
            map((message: LibRpcResponse<LibRpcEvent>) => message.args)
        );
    }

    /**
     * Streams connection status as a boolean value (true = connected, false = disconnected).
     * @returns {Observable<boolean>}
     */
    public get isConnected$(): Observable<boolean> {
        return this.connector.isConnected$.pipe(
            distinctUntilChanged()
        );
    }

    /**
     * Return the list of ids under the given path.
     * @param {string} path The root path of the objects
     * @returns {Observable<string[]>}
     */
    public listObjectsInPath(path: string): Observable<string[]> {
        return this.callMethod<DiscoverableInstance[]>(
            '/', 'com.twoporeguys.librpc.Discoverable', 'get_instances', [], true
        ).pipe(
            map<DiscoverableInstance[], string[]>((instances: DiscoverableInstance[]) => instances
                .filter((instance: DiscoverableInstance) => startsWith(instance.path, path))
                .map((instance: DiscoverableInstance) => instance.path)
            )
        );
    }

    /**
     * Returns the properties of an object.
     * @param {string} path Path of the object
     * @param {string} interfaceName Interface properties belong to
     * @returns {Observable<T>}
     */
    public getObject<T = any>(path: string, interfaceName: string): Observable<T> {
        return this.callMethod(path, 'com.twoporeguys.librpc.Observable', 'get_all', [interfaceName], true).pipe(
            map<Property[], T>((properties: Property[]) => fromPairs(
                properties.map((property: Property) => [property.name, property.value])
            ) as T)
        );
    }

    /**
     * Returns the value of given property of an object.
     * @param {string} path Path of the object
     * @param {string} interfaceName Interface the property belongs to
     * @param {string} property Property name
     * @returns {Observable<T>}
     */
    public getProperty<T = any>(path: string, interfaceName: string, property: string): Observable<T> {
        return this.callMethod(path, 'com.twoporeguys.librpc.Observable', 'get', [interfaceName, property], true);
    }

    /**
     * Set the value of a property of an object.
     * @param {string} path Path of the object
     * @param {string} interfaceName Interface the property belongs to
     * @param {string} property Property name
     * @param {T} value New value to assign to the property
     * @returns {Observable<T>}
     */
    public setProperty<T = any>(path: string, interfaceName: string, property: string, value: T) {
        return this.callMethod<T>(
            path,
            'com.twoporeguys.librpc.Observable',
            'set',
            [interfaceName, property, value],
            true
        );
    }

    /**
     * Call a method of an interface on an object.
     * @param {string} path Path of the object
     * @param {string} interfaceName Interface the method belongs to
     * @param {string} method Method name
     * @param {any[]} args Positional arguments to pass to the method
     * @param {boolean} isBufferized In case the server is currently disconnected, should the call be queued for
     * execution when it reconnects?
     * @returns {Observable<T>}
     */
    public callMethod<T = any>(
        path: string,
        interfaceName: string,
        method: string,
        args: any[] = [],
        isBufferized: boolean = true,
    ): Observable<T> {
        return this.send('rpc', 'call', {
            path: path,
            interface: interfaceName,
            method: method,
            args: args,
        }, isBufferized);
    }

    /**
     * Subscribe to `com.twoporeguys.librpc.Observable.changed` events on object.
     * @param {string} path Path of the object
     * @returns {Observable<T>}
     */
    public subscribe<T = any>(path: string): Observable<T> {
        const outMessage: LibRpcRequest = {
            id: v4(),
            namespace: 'events',
            name: 'subscribe',
            args: [{
                path: path,
                interface: 'com.twoporeguys.librpc.Observable',
                name: 'changed'
            }]
        };
        return this.connector.send(outMessage).pipe(
            filter((incomingMessage: LibRpcResponse<LibRpcEvent>) => (
                    LibRpcClient.isEvent(incomingMessage) &&
                    LibRpcClient.isObjectChangeEvent(incomingMessage.args) &&
                    incomingMessage.args.path === path
            )),
            map((incomingMessage: LibRpcResponse) => fromPairs([[
                incomingMessage.args.args.name,
                incomingMessage.args.args.value,
            ]]) as T)
        );
    }

    /**
     * Subscribe from `com.twoporeguys.librpc.Observable.changed` events on object.
     * @param {string} path Path of the object
     * @returns {Observable<T>}
     */
    public unsubscribe<T = any>(path: string): Observable<T> {
        return this.connector.send({
            id: v4(),
            namespace: 'events',
            name: 'unsubscribe',
            args: [{
                path: path,
                interface: 'com.twoporeguys.librpc.Observable',
                name: 'changed'
            }]
        });
    }

    private send<T = any>(
        namespace: string,
        name: string,
        payload?: any,
        isBufferized: boolean = true,
    ): Observable<T> {
        const request: LibRpcRequest = assign({
            id: v4(),
            namespace: namespace,
            name: name
        }, {args: payload || {}});
        const responses$ = new Subject<LibRpcResponse<T>>();

        this.sendRequest<T>(request, responses$, isBufferized);

        return responses$.pipe(
            map((response: LibRpcResponse<T|LibRpcFragment<T>>) => {
                let result: T;
                if (response.name === 'fragment') {
                    const fragmentWrapper = (response.args as LibRpcFragment<T>);
                    this.sendRequest({
                        id: response.id as string,
                        namespace: response.namespace,
                        name: 'continue',
                        args: fragmentWrapper.seqno + 1
                    }, responses$, isBufferized);
                    result = fragmentWrapper.fragment;
                } else {
                    result = (response.args as T);
                }
                return result;
            }),
        );
    }

    private sendRequest<T>(
        request: LibRpcRequest,
        responses$: Subject<LibRpcResponse<T>>,
        isBufferized: boolean = true
    ) {
        this.connector.send(request, isBufferized).pipe(
            filter((response: LibRpcResponse<T>) => response.id === request.id),
            take(1)
        ).subscribe((response: LibRpcResponse<T>) => {
                if (response.name === 'error') {
                    responses$.error(response.args);
                } else {
                    if (response.name !== 'end') {
                        responses$.next(response);
                    }
                    if (response.name === 'response' || response.name === 'end') {
                        responses$.complete();
                    }
                }
            });
    }
}

import {assign, fromPairs, map, startsWith} from 'lodash';
import {Observable} from 'rxjs/Observable';
import 'rxjs/add/operator/filter';
import 'rxjs/add/operator/map';
import 'rxjs/add/operator/switchMap';
import 'rxjs/add/operator/first';
import 'rxjs/add/observable/from';
import {Subject} from 'rxjs/Subject';
import {LibRpcConnector} from './LibRpcConnector';
import {LibRpcEvent} from './model/LibRpcEvent';
import {LibRpcFragment} from './model/LibRpcFragment';
import {LibRpcRequest} from './model/LibRpcRequest';
import {LibRpcResponse} from './model/LibRpcResponse';
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
    private static connector: LibRpcConnector;

    private connector: LibRpcConnector;
    private askedFragments: Map<string, number>;

    public constructor(url: string, isDebugEnabled: boolean = false, connector?: LibRpcConnector) {
        if (LibRpcClient.connector) {
            this.connector = LibRpcClient.connector;
        } else {
            LibRpcClient.connector = this.connector = (connector || new LibRpcConnector(url, isDebugEnabled));
        }
        this.askedFragments = new Map<string, number>();
    }

    public listObjectsInPath(path: string, id?: string): Observable<string> {
        return this
            .callMethod<DiscoverableInstance[]>('/', 'com.twoporeguys.librpc.Discoverable', 'get_instances', [], id)
            .switchMap<DiscoverableInstance[], DiscoverableInstance>(
                (instances: DiscoverableInstance[]) => Observable.from(instances)
            )
            .filter((instance: DiscoverableInstance) => startsWith(instance.path, path))
            .map<DiscoverableInstance, string>((instance: DiscoverableInstance) => instance.path);
    }

    public getObject<T = any>(path: string, interfaceName: string, id?: string): Observable<T> {
        return this.callMethod(path, 'com.twoporeguys.librpc.Observable', 'get_all', [interfaceName], id)
            .map((properties: Property[]) => fromPairs(
                map(properties, (property: Property) => [property.name, property.value])
            ) as T);
    }

    public getProperty<T = any>(path: string, interfaceName: string, property: string, id?: string): Observable<T> {
        return this.callMethod(path, 'com.twoporeguys.librpc.Observable', 'get', [interfaceName, property], id);
    }

    public setProperty<T = any>(path: string, interfaceName: string, property: string, value: T, id?: string) {
        return this.callMethod(path, 'com.twoporeguys.librpc.Observable', 'set', [interfaceName, property, value], id);
    }

    public callMethod<T = any>(
        path: string,
        interfaceName: string,
        method: string,
        args: any[] = [],
        id?: string
    ): Observable<T> {
        return this.send(
            'rpc',
            'call',
            {
                path: path,
                interface: interfaceName,
                method: method,
                args: args,
            },
            id);
    }

    public subscribe<T = any>(path: string, id: string = v4()): Observable<T> {
        const outMessage: LibRpcRequest = {
            id: id,
            namespace: 'events',
            name: 'subscribe',
            args: [{
                path: path,
                interface: 'com.twoporeguys.librpc.Observable',
                name: 'changed'
            }]
        };
        return this.connector.send(outMessage)
            .filter((incomingMessage: LibRpcResponse<LibRpcEvent>) => (
                    this.isEvent(incomingMessage) &&
                    this.isObjectChangeEvent(incomingMessage.args) &&
                    incomingMessage.args.path === path
            ))
            .map((incomingMessage: LibRpcResponse) => fromPairs([[
                incomingMessage.args.args.name,
                incomingMessage.args.args.value,
            ]]) as T);
    }

    private send<T = any>(namespace: string, name: string, payload?: any, id: string = v4()): Observable<T> {
        const request: LibRpcRequest = assign({
            id: id,
            namespace: namespace,
            name: name
        }, {args: payload || {}});
        const responses$ = new Subject<LibRpcResponse<T>>();

        this.sendRequest<T>(request, responses$);

        return responses$
            .map((response: LibRpcResponse<T|LibRpcFragment<T>>) => {
                let result: T;
                if (response.name === 'fragment') {
                    const fragmentWrapper = (response.args as LibRpcFragment<T>);
                    this.sendRequest({
                        id: response.id as string,
                        namespace: response.namespace,
                        name: 'continue',
                        args: fragmentWrapper.seqno + 1
                    }, responses$);
                    result = fragmentWrapper.fragment;
                } else {
                    result = (response.args as T);
                }
                return result;
            });
    }

    private sendRequest<T>(request: LibRpcRequest, responses$: Subject<LibRpcResponse<T>>) {
        this.connector.send(request)
            .filter((response: LibRpcResponse<T>) => response.id === request.id)
            .subscribe((response: LibRpcResponse<T>) => {
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

    private isEvent(message: LibRpcResponse): boolean {
        return !!(message && message.namespace === 'events' && message.name === 'event');
    }

    private isInterfaceAddEvent(event: LibRpcEvent, interfaceName: string): boolean {
        return event &&
            event.interface === 'com.twoporeguys.librpc.Introspectable' &&
            event.name === 'interface_added' &&
            event.args === interfaceName;
    }

    private isObjectChangeEvent(event: LibRpcEvent): boolean {
        return event && event.interface === 'com.twoporeguys.librpc.Observable' && event.name === 'changed';
    }
}

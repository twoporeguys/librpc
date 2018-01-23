import {assign, fromPairs, map, startsWith} from 'lodash';
import {Observable} from 'rxjs/Observable';
import 'rxjs/add/operator/filter';
import 'rxjs/add/operator/map';
import 'rxjs/add/operator/take';
import 'rxjs/add/observable/interval';
import {Subject} from 'rxjs/Subject';
import {LibRpcConnector} from './LibRpcConnector';
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

    public constructor(url: string, isDebugEnabled: boolean = false) {
        if (LibRpcClient.connector) {
            this.connector = LibRpcClient.connector;
        } else {
            LibRpcClient.connector = this.connector = new LibRpcConnector(url, isDebugEnabled);
        }
        this.askedFragments = new Map<string, number>();
    }

    public listObjectsInPath(path: string, id?: string): Observable<string> {
        return this
            .callMethod<DiscoverableInstance>('/', 'com.twoporeguys.librpc.Discoverable', 'get_instances', [], id)
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
        const outMessage = {
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
            .filter((incomingMessage: any) => (
                    incomingMessage.namespace === 'events' &&
                    incomingMessage.name === 'event' &&
                    incomingMessage.args &&
                    incomingMessage.args.name === 'changed' &&
                    incomingMessage.args.path === path
            ))
            .map((incomingMessage: any) => fromPairs([[
                incomingMessage.args.args.name,
                incomingMessage.args.args.value,
            ]]) as T);
    }

    private send<T = any>(namespace: string, name: string, payload?: any, id: string = v4()): Observable<T> {
        const outMessage = assign({
            id: id,
            namespace: namespace,
            name: name
        }, {args: payload || {}});
        const results$ = new Subject<any>();

        this.connector.send(outMessage)
            .filter((incomingMessage: any) => incomingMessage.id === id)
            .take(1)
            .subscribe((response: any) => {
                switch (response.name) {
                    case 'response':
                        results$.next(response.args);
                        results$.complete();
                        break;
                    case 'fragment':
                        this.fetchFragments(response, results$);
                        break;
                    case 'end':
                        results$.complete();
                        break;
                    case 'error':
                        results$.error(response.args);
                        break;
                }
            });
        return results$;
    }

    private fetchFragments(previousResponse: any, results$: Subject<any>) {
        switch (previousResponse.name) {
            case 'fragment':
                results$.next(previousResponse.args.fragment);
                const nextSeqno = previousResponse.args.seqno + 1;
                this.connector.send({
                    id: previousResponse.id,
                    namespace: previousResponse.namespace,
                    name: 'continue',
                    args: nextSeqno
                })
                    .filter((incomingMessage: any) => incomingMessage.id === previousResponse.id)
                    .take(1)
                    .subscribe((response: any) => {
                        this.fetchFragments(response, results$);
                    });
                break;
            case 'end':
                results$.complete();
                break;
        }
    }
}
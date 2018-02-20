/**
 * @module LibRpcClient
 */
export interface LibRpcRequest<T = any> {
    id: string;
    namespace: string;
    name: string;
    args: T;
}

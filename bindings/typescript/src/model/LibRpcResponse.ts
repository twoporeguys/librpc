/**
 * @module LibRpcClient
 */
export interface LibRpcResponse<T = any> {
    id: string|null;
    namespace: string;
    name: string;
    args: T;
}

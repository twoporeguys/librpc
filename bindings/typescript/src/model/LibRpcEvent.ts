/**
 * @module LibRpcClient
 */
export interface LibRpcEvent {
    interface: string;
    name: string;
    path: string;
    args: any;
}

/**
 * @module LibRpcClient
 */
export interface LibRpcFragment<T> {
    seqno: number;
    fragment: T;
}

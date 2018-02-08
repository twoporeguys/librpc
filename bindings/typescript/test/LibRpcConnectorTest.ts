import {expect} from 'chai';
import 'mocha';
import 'rxjs/add/observable/from';
import 'rxjs/add/observable/of';
import 'rxjs/add/operator/filter';
import 'rxjs/add/operator/observeOn';
import {Observable} from 'rxjs/Observable';

import {Observer} from 'rxjs/Observer';
import {Scheduler} from 'rxjs/Rx';
import {LibRpcConnector} from '../src/LibRpcConnector';

const noop = () => {/* nothing to see here */};

describe('LibRpcConnector', () => {
    const webSocketFactoryMock = {
        CLOSED: 0,
        CLOSING: 1,
        CONNECTING: 2,
        OPEN: 3,
        get: () => ({} as any)
    };
    describe('listen', () => {
        it('should return an Observable', () => {
            const connector = new LibRpcConnector('foo', false, webSocketFactoryMock);

            const result = connector.listen();

            expect(result).to.be.instanceof(Observable);
        });

    });
});

const packageInfo = require('./package.json');
const path = require('path');
const _ = require('lodash');

module.exports = {
    entry: {
        'index': './src/index.ts',
    },
    output: {
        path: path.join(__dirname, './dist'),
        filename: '[name].js',
        library: [packageInfo.name + '/[name]'],
        libraryTarget: 'umd',
        publicPath: '/dist'
    },
    externals: _.keys(packageInfo.dependencies),
    devtool: 'source-map',
    resolve: {
        extensions: ['.ts', '.tsx', '.js', '.json'],
    },
    module: {
        rules: [
            {
                test: /\.ts$/,
                use: [
                    {
                        loader: 'ts-loader',
                        options: {
                            configFile: 'tsconfig.json'
                        }
                    }
                ]
            },
            {
                test: /\.js$/,
                loader: 'source-map-loader',
                enforce: 'pre'
            },
        ]
    },
};

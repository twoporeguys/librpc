pipeline {
    agent {
        docker {
            image 'ubuntu:18.04'
            args "-u root:sudo -v ${env.DOCS_PATH}:${env.DOCS_PATH}"
        }
    }

    environment {
        CC = 'clang'
        CXX = 'clang++'
        DEBIAN_FRONTEND = 'noninteractive'
        LANG = 'C'
        npm_config_cache = "${pwd()}/.npm"
    }

    stages {
        stage('Build Supermom') {
            steps {
                build job: 'supermom/master', wait: false
            }
        }

        stage('Bootstrap') {
            steps {
                sh 'apt-get update'
                sh 'apt-get -y install build-essential'
                sh 'make bootstrap'
            }
        }

        stage('Build') {
            steps {
                sh 'mkdir -p build'
                sh 'cd build && cmake .. -DBUILD_LIBUSB=ON -DBUILD_DOC=ON'
                sh 'cd build && make'
            }
        }

        stage('Run tests') {
            options {
                timeout(time: 30)
            }
            steps {
                sh 'make test'
            }
        }

        stage('Deploy docs') {
            when {
                expression { "${env.DOCS_PATH}" != "" }
            }
            steps {
                sh "mkdir -p ${DOCS_PATH}/librpc"
                sh "rm -rf ${DOCS_PATH}/librpc/*"
                sh "cp -dR build/docs/* ${DOCS_PATH}/librpc/"
            }
        }


        stage('Cleanup') {
            steps {
                sh 'chown -v -R 9001:9001 .'
            }
        }
    }

    post {
        always {
            build job: 'ports/master', wait: false
            junit 'junit-test-results.xml'
                publishHTML target: [
                    allowMissing: false,
                    alwaysLinkToLastBuild: false,
                    keepAll: true,
                    reportDir: 'coverage-report',
                    reportFiles: 'index.html',
                    reportName: 'Code coverage report'
                ]
        }
    }
}

pipeline {
    agent any

    environment {
        CC = 'clang'
        CXX = 'clang++'
    }

    stages {
        stage('Bootstrap') {
            steps {
                lock('apt-get') {
	                sh 'sudo make bootstrap'
                }
            }
        }

        stage('Build') {
            steps {
                sh 'mkdir -p build' 
                sh 'cd build && cmake .. -DBUILD_LIBUSB=ON -DBUILD_DOC=ON'
                sh 'cd build && make'
            }
        }

        stage('Deploy docs') {
            when {
                expression { "${env.DOCS_PATH}" != "" }
            }
            steps {
                sh "mkdir -p ${DOCS_PATH}/librpc"
                sh "rm -rf ${DOCS_PATH}/librpc/*"
                sh "cp -a build/docs/* ${DOCS_PATH}/librpc/"
            }
        }

        stage('Generate typescript docs') {
            steps {
                sh 'cd bindings/typescript && make doc'
            }
        }

        stage('Deploy docs') {
            when {
                expression { "${env.DOCS_PATH}" != "" }
            }
            steps {
                sh "mkdir -p ${DOCS_PATH}/typescript/librpc-client"
                sh "rm -rf ${DOCS_PATH}/typescript/librpc-client/*"
                sh "cp -a bindings/typescript/doc/* ${DOCS_PATH}/typescript/librpc-client/"
            }
        }
    }
}

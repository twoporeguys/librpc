pipeline {
    agent any

    environment {
        CC = 'clang'
        CXX = 'clang++'
    }

    stages {
        lock('apt-get') {
            stage('Bootstrap') {
                steps {
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
    }
}

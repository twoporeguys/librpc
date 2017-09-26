pipeline {
    agent any

    stages {
        stage('Build') {
            steps {
                sh 'mkdir -p build' 
                sh 'cd build && cmake .. -DBUILD_LIBUSB=ON -DBUILD_DOC=ON'
                sh 'cd build && make'
            }
        }

        stage('Deploy docs') {
            steps {
                sh
            }
        }
    }
}

pipeline {
    agent any

    stages {
        stage('Build') {
            steps {
                sh 'mkdir build' 
                sh 'cd build'
                sh 'cmake .. -DBUILD_LIBUSB=ON'
                sh 'make'
            }
        }
    }
}

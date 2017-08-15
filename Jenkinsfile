pipeline {
    agent any

    stages {
        stage('Build') {
            steps {
                sh 'mkdir build' 
                sh 'cd build && cmake .. -DBUILD_LIBUSB=ON'
                sh 'cd build && make'
            }
        }
    }
}

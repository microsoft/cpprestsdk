#!groovy

DOCKER_REGISTRY  = 'https://dockerregistry.nexenio.com'
DOCKER_IMAGE     = 'desktop-client/linux:latest'

void updateSourcecode() {
    cleanWs()
    checkout scm
}

Closure buildOnMac() {
    return {
        node('Mac') {
            updateSourcecode()
            sh 'ci/unix/build.sh all'
            sh 'ci/unix/test.sh'
        }
    }
}

node {
    try {
        stage('Build') {
            buildSteps = [:]

            buildSteps["macOS"]     = buildOnMac()

            parallel buildSteps
        }

        currentBuild.result = 'SUCCESS'
    } catch (err) {
        currentBuild.result = 'FAILED'
    }
}
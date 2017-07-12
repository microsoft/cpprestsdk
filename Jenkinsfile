#!groovy

void updateSourcecode() {
    cleanWs()
    checkout scm
}

Closure buildOnMac() {
    return {
        node('Mac') {
            updateSourcecode()

            // tentatively set result to SUCCESS, maybe set to FAILED below
            currentBuild.result = 'SUCCESS'
            sh 'ci/unix/build.sh all'

            try {
                sh 'ci/unix/test.sh'
            } catch (err) {
                // catch failing tests, not marking the whole build as failure
                // but just as unstable
                currentBuild.result = 'UNSTABLE'
            }
        }
    }
}

Closure buildOnWindows7() {
    return {
        node('windows7') {
            updateSourcecode()

            currentBuild.result = 'SUCCESS'
            sh 'ci/windows/build.sh'
            try {
                sh 'ci/windows/bundle.sh'
            } catch(err) {
                currentBuild.result = 'FAILED'
            }
        }
    }
}

node {
    try {
        stage('Build') {
            buildSteps = [:]
            buildSteps["macOS"] = buildOnMac()
            buildSteps["Windows 7"] = buildOnWindows7()
            parallel buildSteps
        }
    } catch (err) {
        currentBuild.result = 'FAILED'
    }
}

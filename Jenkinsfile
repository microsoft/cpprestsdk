#!groovy

void updateSourcecode() {
    cleanWs()
    checkout scm
}

Closure buildOnMac() {
    return {
        node('Mac') {
            updateSourcecode()

            sh 'ci/unix/build.sh all'

            try {
                sh 'ci/unix/test.sh'
            } catch (err) {
                // catch failing tests, not marking the whole build as failure
                // but just as unstable;
                // if build is already failed, leave it that way
                if(currentBuild.result != 'FAILED') {
                    currentBuild.result = 'UNSTABLE'
                }
            }
        }
    }
}

Closure buildOnWindows7() {
    return {
        node('windows7') {
            updateSourcecode()

            bat 'powershell ci/windows/build.ps1'
            bat 'powershell ci/windows/bundle.ps1'
            archiveArtifacts 'cpprestsdk.zip'
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


        currentBuild.result = 'SUCCESS'
    } catch (err) {
        currentBuild.result = 'FAILED'
    }
}

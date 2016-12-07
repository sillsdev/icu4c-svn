#!groovy
// Copyright (c) 2016 SIL International
// This software is licensed under the MIT license (http://opensource.org/licenses/MIT)

ansiColor('xterm') {
    timestamps {
        properties([parameters([choice(name: 'buildKind', choices: 'Continuous\nRelease',
            description: 'Is this a continuous (pre-release) or a release build?'),
         choice(name: 'Xtest', choices: 'Continuous\nRelease',
             description: 'Is this a continuous (pre-release) or a release build?')]),
             pipelineTriggers([[$class: 'GitHubPushTrigger']])])

        // Set default. This is only needed for the first build.
        buildKind = buildKind ?: 'XContinuous'

        node('windows && supported') {
            echo "buildKind=${buildKind}"
            def msbuild = tool 'msbuild12'

            def PkgVersion
            stage('Checkout') {
                checkout scm

                // We expect that the branch name contains the ICU version number, otherwise default to 54
                def IcuVersion = (env.BRANCH_NAME =~ /[0-9]+/)[0] ?: 54
                def PreRelease = buildKind != 'Release' ? "-beta${BUILD_NUMBER}" : ""
                PkgVersion = "${IcuVersion}.1.${BUILD_NUMBER}${PreRelease}"

                currentBuild.displayName = PkgVersion
            }

            dir("nugetpackage") {
                dir("build") {
                    stage('Build ICU') {
                        echo "Compiling ICU"
                        bat """
                        "${msbuild}" /t:Build
                        """
                    }

                    stage('Pack nuget') {
                        echo "Creating nuget package ${PkgVersion}"
                        bat """
                        "${msbuild}" /t:BuildPackage /p:PkgVersion=${PkgVersion}
                        """
                    }
                }

                archiveArtifacts "*.nupkg"
            }
        }
    }
}
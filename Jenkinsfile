#!groovy
// Copyright (c) 2016 SIL International
// This software is licensed under the MIT license (http://opensource.org/licenses/MIT)

ansiColor('xterm') {
    timestamps {
        properties([
            // Add buildKind parameter
            parameters([choice(name: 'buildKind', choices: 'Continuous\nRelease',
                description: 'Is this a continuous (pre-release) or a release build?')]),
            // Add Gerrit Trigger
            pipelineTriggers([gerrit(customUrl: '', gerritProjects: [[branches: [[compareType: 'PLAIN', pattern: env.BRANCH_NAME]],
                compareType: 'PLAIN', disableStrictForbiddenFileVerification: false, pattern: 'icu4c']],
                triggerOnEvents: [patchsetCreated(excludeDrafts: false, excludeNoCodeChange: true, excludeTrivialRebase: false),
                refUpdated()])])
        ])

        // Set default. This is only needed for the first build.
        buildKind = buildKind ?: 'Continuous'

        node('windows && supported') {
            def msbuild = tool 'msbuild12'

            stage('Checkout') {
                checkout scm

                // We expect that the branch name contains the ICU version number, otherwise default to 54
                def IcuVersion = (env.BRANCH_NAME =~ /[0-9]+/)[0] ?: 54
                def PreRelease = GERRIT_CHANGE_NUMBER ? "-ci" : (buildKind != 'Release' ? "-beta" : "")
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

                setGerritReview()
            }
        }
    }
}
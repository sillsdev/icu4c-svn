#!groovy
// Copyright (c) 2016 SIL International
// This software is licensed under the MIT license (http://opensource.org/licenses/MIT)

ansiColor('xterm') {
    timestamps {

        properties([parameters([choice(name: 'buildKind', choices: 'Continuous\nRelease',
            description: 'Is this a continuous (pre-release) or a release build?')])])

        node('windows') {
            def msbuild = tool 'msbuild12'

            milestone label: 'Checkout'

            stage('Checkout') {
                checkout scm
            }

            // We expect that the branch name contains the ICU version number, otherwise default to 54
            def IcuVersion = env.BRANCH_NAME =~ /[0-9]+/ ?: 54
            def PreRelease = buildKind != 'Release' ? "-beta${BUILD_NUMBER}" : ""
            def PkgVersion = "${IcuVersion}.1.${BUILD_NUMBER}${PreRelease}"

            currentBuild.displayName = PkgVersion

            dir("nugetpackage/build") {
                milestone label: 'Compile'

                stage('Build ICU') {
                    bat """
                    "${msbuild}" /t:Build
                    """
                }

                milestone label: 'Build nuget package'

                stage('Pack nuget') {
                    bat """
                    "${msbuild}" /t:BuildPackage /p:PkgVersion=${PkgVersion}
                    """
                }
            }

            archiveArtifacts "*.nupkg"
        }
    }
}
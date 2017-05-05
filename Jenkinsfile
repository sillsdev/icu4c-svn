#!groovy
// Copyright (c) 2016-2017 SIL International
// This software is licensed under the MIT license (http://opensource.org/licenses/MIT)

ansiColor('xterm') {
	timestamps {
		properties([
			// Add buildKind parameter
			parameters([choice(name: 'buildKind', choices: 'Continuous\nRelease',
				description: 'Is this a continuous (pre-release) or a release build?')]),
			pipelineTriggers([[$class: 'GitHubPushTrigger']])
		])

		// Set default. This is only needed for the first build.
		def buildKindVar = params.buildKind ?: 'Continuous'

		try {
			isPR = BRANCH_NAME.startsWith("PR-") ? true : false
		}
		catch(err) {
			isPR = false
		}

		try {
			node('windows && supported') {
				echo "Testing again!!!"
				def msbuild = tool 'msbuild12'
				def git = tool(name: 'Default', type: 'git')

				stage('Checkout') {
					checkout scm

					def uvernum = readFile 'source/common/unicode/uvernum.h'
					def IcuVersion = (uvernum =~ "#define U_ICU_VERSION_MAJOR_NUM ([0-9]+)")[0][1]
					def IcuMinor = (uvernum =~ "#define U_ICU_VERSION_MINOR_NUM ([0-9]+)")[0][1]
					def PreRelease = isPR ? "-${BRANCH_NAME}" :
						(buildKindVar != 'Release' ? "-beta" : "")
					PkgVersion = "${IcuVersion}.${IcuMinor}.${BUILD_NUMBER}${PreRelease}"

					currentBuild.displayName = PkgVersion
				}

				dir("nugetpackage") {
					dir("build") {
						stage('Build ICU') {
							echo "Compiling ICU"
						/*
							bat """
							"${msbuild}" /t:Build
							"""
							*/
						}

						stage('Pack nuget') {
							echo "Creating nuget package ${PkgVersion}"
							/*
							bat """
							"${msbuild}" /t:BuildPackage /p:PkgVersion=${PkgVersion}
							"""
							*/
						}
					}

					if (!isPR) {
						archiveArtifacts "*.nupkg"
					}

					currentBuild.result = "SUCCESS"
				}
			}
		} catch(error) {
			currentBuild.result = "FAILED"
		}
	}
}
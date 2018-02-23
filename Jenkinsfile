#!groovy
// Copyright (c) 2016-2018 SIL International
// This software is licensed under the MIT license (http://opensource.org/licenses/MIT)

ansiColor('xterm') {
	timestamps {
		properties([
			// Add buildKind parameter
			parameters([
				choice(name: 'buildKind', choices: 'Continuous\nRelease\nReleaseCandidate',
					description: 'Is this a continuous (pre-release) or a release build?'),
				string(name: 'DistributionsToPackage', defaultValue: 'trusty xenial',
					description: 'The distributions to build packages for (separated by space)'),
				string(name: 'ArchesToPackage', defaultValue: 'amd64 i386',
					description: 'The architectures to build packages for (separated by space)')
			]),
			pipelineTriggers([[$class: 'GitHubPushTrigger']])
		])

		// Set default. This is only needed for the first build.
		def buildKindVar = params.buildKind ?: 'Continuous'
		def supported_distros = 'bionic xenial trusty'

		try {
			isPR = BRANCH_NAME.startsWith("PR-") ? true : false
		} catch(err) {
			isPR = false
		}

		try {
			parallel('Windows': {
				def PkgVersion
				node('windows && supported') {
					def msbuild = tool 'msbuild12'
					def git = tool(name: 'Default', type: 'git')

					stage('Checkout Windows') {
						checkout([$class: 'GitSCM', branches: [[name: BRANCH_NAME]],
							doGenerateSubmoduleConfigurations: false, extensions:
								[[$class: 'CloneOption', depth: 1, noTags: false, shallow: true]],
							userRemoteConfigs: [[url: 'https://github.com/sillsdev/icu4c']]])

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

						if (!isPR) {
							archiveArtifacts "*.nupkg"
						}

						currentBuild.result = "SUCCESS"
					}
			}}, 'Linux': {
				def PkgVersion
				node('packager') {
					stage('Checkout Linux') {
						dir('icu-fw')
						{
							checkout([$class: 'GitSCM', branches: [[name: BRANCH_NAME]],
								doGenerateSubmoduleConfigurations: false, extensions:
									[[$class: 'CloneOption', depth: 1, noTags: false, shallow: true]],
								userRemoteConfigs: [[url: 'https://github.com/sillsdev/icu4c']]])

							def uvernum = readFile 'source/common/unicode/uvernum.h'
							def IcuVersion = (uvernum =~ "#define U_ICU_VERSION_MAJOR_NUM ([0-9]+)")[0][1]
							def IcuMinor = (uvernum =~ "#define U_ICU_VERSION_MINOR_NUM ([0-9]+)")[0][1]
							String pr
							try {
								pr = isPR ? (BRANCH_NAME =~ "PR-([0-9]+)")[0][1] : ""
							} catch(err) {
								pr = ""
							}
							def Build = isPR ? "~PR${pr}.${BUILD_NUMBER}" :
								(buildKindVar != 'Release' ? "~beta${BUILD_NUMBER}" : ".${BUILD_NUMBER}")
							PkgVersion = "${IcuVersion}.${IcuMinor}.1${Build}"
						}
					}

					stage('Package') {
						echo "Creating package ${PkgVersion}"
						sh """#!/bin/bash
							export FULL_BUILD_NUMBER=${PkgVersion}

							if [ "${buildKindVar}" = "Release" ]; then
								MAKE_SOURCE_ARGS="--preserve-changelog"
								BUILD_PACKAGE_ARGS="--no-upload"
							elif [ "${buildKindVar}" = "ReleaseCandidate" ]; then
								MAKE_SOURCE_ARGS="--preserve-changelog"
								BUILD_PACKAGE_ARGS="--no-upload"
							fi

							if ${isPR}; then
								BUILD_PACKAGE_ARGS="--no-upload"
							fi

							cd "icu-fw"
							\$HOME/ci-builder-scripts/bash/make-source --dists "\$DistributionsToPackage" \
								--arches "\$ArchesToPackage" \
								--main-package-name "icu-fw" \
								--supported-distros "${supported_distros}" \
								--debkeyid \$DEBSIGNKEY \
								--main-repo-dir . \
								--package-version "${PkgVersion}" \
								\$MAKE_SOURCE_ARGS

							\$HOME/ci-builder-scripts/bash/build-package --dists "\$DistributionsToPackage" \
								--arches "\$ArchesToPackage" \
								--main-package-name "icu-fw" \
								--supported-distros "${supported_distros}" \
								--debkeyid \$DEBSIGNKEY \
								\$BUILD_PACKAGE_ARGS
							"""

						archiveArtifacts artifacts: 'results/*'
					}
				}
			})
		} catch(error) {
			echo error
			currentBuild.result = "FAILED"
		}
	}
}
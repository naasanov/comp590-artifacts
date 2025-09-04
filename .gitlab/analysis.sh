#!/usr/bin/env bash
###
#
#  @file analysis.sh
#  @copyright 2013-2024 Bordeaux INP, CNRS (LaBRI UMR 5800), Inria,
#                       Univ. Bordeaux. All rights reserved.
#
#  @version 1.2.4
#  @author Mathieu Faverge
#  @author Florent Pruvost
#  @date 2024-05-29
#
###

set -ex
conda activate openvibe

PROJECT=$CI_PROJECT_NAME

# Performs an analysis of the project source code:
# - we consider to be in the project's source code root
# - we consider having build log files $PROJECT-build*.log in the root directory
# - we consider having junit files $PROJECT-test*junit.xml in the root directory
# - we consider having coverage files $PROJECT-test*.lcov in the root directory
# - we consider having cppcheck, sonar-scanner programs available in the environment

if [ $# -gt 0 ]
then
    BUILDDIR=$1
fi
BUILDDIR=${BUILDDIR:-build}

# Analyse files : generate filelist.txt

# Undefine this because not relevant in our configuration
#export UNDEFINITIONS="-UWIN32 -UWIN64 -U_MSC_EXTENSIONS -U_MSC_VER -U__SUNPRO_C -U__SUNPRO_CC -U__sun -Usun -U__cplusplus"

# run cppcheck analysis
#CPPCHECK_OPT=" -v -f --language=c --platform=unix64 --enable=all --xml --xml-version=2 --suppress=missingIncludeSystem ${UNDEFINITIONS}"
#cppcheck $CPPCHECK_OPT --file-list=./filelist_none.txt 2> ${PROJECT}-cppcheck.xml
#cppcheck $CPPCHECK_OPT -DPRECISION_s -UPRECISION_d -UPRECISION_c -UPRECISION_z -UPRECISION_z --file-list=./filelist_s.txt 2>> ${PROJECT}-cppcheck.xml
#cppcheck $CPPCHECK_OPT -UPRECISION_s -DPRECISION_d -UPRECISION_c -UPRECISION_z -UPRECISION_z --file-list=./filelist_d.txt 2>> ${PROJECT}-cppcheck.xml
#cppcheck $CPPCHECK_OPT -UPRECISION_s -UPRECISION_d -DPRECISION_c -UPRECISION_z -UPRECISION_z --file-list=./filelist_c.txt 2>> ${PROJECT}-cppcheck.xml
#cppcheck $CPPCHECK_OPT -UPRECISION_s -UPRECISION_d -UPRECISION_c -DPRECISION_z -UPRECISION_z --file-list=./filelist_z.txt 2>> ${PROJECT}-cppcheck.xml
#cppcheck $CPPCHECK_OPT -UPRECISION_s -UPRECISION_d -UPRECISION_c -UPRECISION_z -DPRECISION_p --file-list=./filelist_p.txt 2>> ${PROJECT}-cppcheck.xml

# GNU coverage
gcovr --xml-pretty --exclude-unreachable-branches --print-summary -o coverage.xml --root $PWD

# cppcheck
# TODO : Have to fix "from compilation database does not exist"
#cppcheck -v --max-configs=1 --language=c++ --platform=unix64 --enable=all --xml --xml-version=2 --suppress=missingIncludeSystem --project=build/compile_commands.json 2> cppcheck.xml

# valgrind
#valgrind --xml=yes --xml-file=valgrind.xml --memcheck:leak-check=full --show-reachable=yes "./build/hello"


# create the sonarqube config file
# build.log : result of make at build step
# no need, take the default for sonar.cxx.gcc.regex=(?<file>.*):(?<line>[0-9]+):[0-9]+:\\\x20warning:\\\x20(?<message>.*)\\\x20\\\[(?<id>.*)\\\]

# -Dsonar.login=sqp_3d4dce666df72f6fd478ed983a6367f464b82679
# -Dsonar.projectKey=openvibe_meta_AZJXpMOysbMNg1jXgnB3

cat > sonar-project.properties << EOF
sonar.host.url=https://sonarqube.inria.fr/sonarqube
sonar.qualitygate.wait=true
sonar.verbose=true

sonar.links.homepage=$CI_PROJECT_URL
sonar.links.scm=$CI_REPOSITORY_URL
sonar.links.ci=$CI_PROJECT_URL/pipelines
sonar.links.issue=$CI_PROJECT_URL/issues

# From https://sonarqube.inria.fr/sonarqube/dashboard
sonar.projectKey=openvibe_meta_AZJXpMOysbMNg1jXgnB3
sonar.projectDescription=OpenViBE, software for real-time neurosciences



# From https://gitlab.inria.fr/openvibe/meta/-/settings/ci_cd#js-cicd-variables-settings
sonar.token=$SONAR_TOKEN

sonar.projectDescription=OpenVibe quality tests
sonar.projectVersion=0.1

sonar.scm.disabled=false
sonar.scm.provider=git
sonar.scm.exclusions.disabled=true  

sonar.pullrequest.key=$CI_MERGE_REQUEST_IID

# Should be equal the MR
sonar.pullrequest.branch=$CI_MERGE_REQUEST_SOURCE_BRANCH_NAME

sonar.exclusions=conda/**,**/CMakeFiles/**,**/cmake_modules/**,**/doc/**,**/*.html,**/*.svg,**/*.material,**/*.in,**/*.f90,**/*.wav,**/*.ico,**/*.mp3,**/*.inl,**/*.mxb,**/*.material,**/*.ovw,**/*.script,**/*.ui-base,**/*.sh-base,**/*.cmd-base,**/*.ttf,**/*.xml,**/*.css,**/miniconda3/**,**/*.js,**/*.ts,**/*.css,**/moc_*.*,**/*.png,**/*.dia,**/*.md,**/*.txt,**/*.ui,**/*.conf,**/*.bin,**/*.bmp,**/*.dox-part,**/**/COPYING,**/README, **/*.sh,**/InstallDependencies,**/*.cmake,**/*.ov,**/*.obj,**/*.png-offscreen,**/*.gdf,**/*.mesh,**/*.csv,**/*.m,**/*.lua,**/*.qml,**/*.cfg,**/*.mxs,**/qt6/**

sonar.sources=designer, sdk, extras, CMake
sonar.sourceEncoding=UTF-8
sonar.cxx.file.suffixes=.cxx,.cpp,.cc,.c,.hxx,.hpp,.h,.hh
sonar.cxx.jsonCompilationDatabase=build/compile_commands.json
sonar.cxx.errorRecoveryEnabled=true
sonar.cxx.gcc.encoding=UTF-8
sonar.cxx.gcc.regex=(?<file>.*):(?<line>[0-9]+):[0-9]+:\\\x20warning:\\\x20(?<message>.*)\\\x20\\\[(?<id>.*)\\\]
sonar.cxx.gcc.reportPaths=build/build.log

sonar.cxx.xunit.reportPaths=test.xml
sonar.cxx.cobertura.reportPaths=coverage.xml
sonar.cxx.cppcheck.reportPaths=cppcheck.xml
sonar.cxx.clangsa.reportPaths=build/analyzer_reports/*/*.plist
sonar.cxx.clangtidy.encoding=UTF-8
sonar.cxx.valgrind.reportPaths=valgrind.xml

sonar.python.version=3.10
# get from /builds/miniconda3/envs/openvibe/bin/x86_64-conda-linux-gnu-c++ -xc -E -v -
sonar.cxx.includeDirectories=sdk/openvibe/include/, sdk/toolkit/include/, sdk/common/include/, sdk/modules/xml/include/, designer/visualization-toolkit/include/, /builds/miniconda3/envs/openvibe/bin/../lib/gcc/x86_64-conda-linux-gnu/9.4.0/include, /builds/miniconda3/envs/openvibe/bin/../lib/gcc/x86_64-conda-linux-gnu/9.4.0/include-fixed, /builds/miniconda3/envs/openvibe/bin/../lib/gcc/x86_64-conda-linux-gnu/9.4.0/../../../../x86_64-conda-linux-gnu/include,  /builds/miniconda3/envs/openvibe/bin/../x86_64-conda-linux-gnu/sysroot/usr/include,/builds/miniconda3/envs/openvibe/x86_64-conda-linux-gnu/include/c++/9.4.0/
# end of file
EOF
echo "====== sonar-project.properties ============"
cat sonar-project.properties
echo "============================================"

# run sonar analysis + publish on sonarqube
/builds/sonar-scanner-cli/bin/sonar-scanner -X > sonar.log

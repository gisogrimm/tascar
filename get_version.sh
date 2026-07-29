#!/bin/sh
VERSION=$(cat config.mk | sed -e '2 ! d' -e 's/.*=//1')
# If the source tree contains modifications, append "-modified" to the version string.
# But ignore alleged symlink modifications detected on Github Windows runners.
GITMODIFIED=$(test -z "`git status --porcelain -uno | grep -v T.external_libs/libmysofa-1.3.2/share/default.sofa | grep -v D.libtascar/tascar`" || echo "-modified")
COMMITHASH=$(git log -1 --abbrev=7 --pretty='format:%h')
LATEST_RELEASETAG=$(git tag -l "release*" |tail -1)
COMMIT_SINCE_RELEASE=$(git rev-list --count ${LATEST_RELEASETAG}..)

FULLVERSION=${VERSION}.${COMMIT_SINCE_RELEASE}-${COMMITHASH}${GITMODIFIED}
#HAS_LSL=$(sh check_for_lsl)
#HAS_OPENMHA=$(sh check_for_openmha)
#HAS_OPENCV2=$(sh check_for_opencv2)
#HAS_OPENCV4=$(sh check_for_opencv4)

echo "Creating tascarver.h with version ${FULLVERSION}"
mkdir -p libtascar/build
echo '#define TASCARVER "'${FULLVERSION}'"' > libtascar/build/tascarver.h


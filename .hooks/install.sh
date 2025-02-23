#!/bin/sh

HERE=$(cd $(dirname $0); pwd -P)
GIT_DIR=${HERE}/../.git
CP=$(which cp)
RM=$(which rm)

if [ -e ${GIT_DIR}/hooks ]; then
  ${CP} ${HERE}/pre-commit ${GIT_DIR}/hooks
  ${CP} ${HERE}/post-commit ${GIT_DIR}/hooks
  #${CP} ${HERE}/post-merge ${GIT_DIR}/hooks
  #${CP} ${HERE}/version.sh ${GIT_DIR}/hooks
  ${RM} -f ${GIT_DIR}/hooks/version.sh
  ${RM} -f ${GIT_DIR}/hooks/post-merge
  ${RM} -f ${GIT_DIR}/.commit
fi

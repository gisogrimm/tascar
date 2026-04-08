# Create a new release

On branch development:

- build, test and package: `make clean; make -j $(nproc) all googletest && make pack`

- increment version number in `config.mk`, edit `changelog`

- commit changes to git (do not push)

- create release tag: `make releasetag`

- swich to master branch, pull, then merge development

- push tags and master to all remotes

- switch back to development branch

- create release on github, both repos

- create a brew release

- update release on zenodo

- edit news page of tascarwww repository

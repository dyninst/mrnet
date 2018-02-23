Project repository and mirror for upstream MRNet project on github
https://github.com/dyninst/mrnet

Steps for updating the mirror from your local stash clone:

1. git remote add upstream git@github.com:dyninst/mrnet.git
2. git fetch upstream
3. git fetch upstream --tags
4. git cehckout master
5. git merge upstream/master
6. git push origin master

Done. Once updated, make sure to update any submodule references in
other debugger projects to the latest commit/release tag.

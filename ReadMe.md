*[See the original project's readme at readme.html]*

This project is a mirror of the svn project at http://source.icu-project.org/repos/icu/icu/ (see [icu-project.org](http://icu-project.org)).

Changes should be submitted to the git repo at [https://gerrit.lsdev.sil.org/icu4c](https://gerrit.lsdev.sil.org/icu4c) rather than directly to this repository. See [https://gerrit.lsdev.sil.org/#/admin/projects/icu4c](https://gerrit.lsdev.sil.org/#/admin/projects/icu4c).

## Special long-lived branches ##

**MinimumStaticallyLinked54**

This branch is based on -- but has two major differences from -- the 54.1 release tag:

- It is a "minimum" build as described [here](https://github.com/sillsdev/icu-dotnet#windows-1)
- It is statically linked to remove the dependency on the C++ Redistributable
	- This is accomplished by setting the runtime library in each .vcxproj file to MultiThreaded rather than MultiThreadedDLL
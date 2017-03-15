# Icu4c

**NOTE:** See the original project's readme at [readme.html](https://github.com/sillsdev/icu4c/blob/FullStaticallyLinked56/readme.html)

This project is a mirror of the svn project at <http://source.icu-project.org/repos/icu/icu/> (see [icu-project.org](http://icu-project.org)).

Changes should be submitted to the git repo at <https://gerrit.lsdev.sil.org/icu4c> rather than directly to this repository. See <https://gerrit.lsdev.sil.org/admin/projects/icu4c>.

## Special long-lived branches

### Fieldworks

Based on the 54.1 release tag, this branch contains some specific changes for the [Fieldworks project](https://github.com/sillsdev/fieldworks)

### MinimumStaticallyLinked54 / MinimumStaticallyLinked56

These branches are based on -- but have two major differences from -- the 54.1 resp. 56.1 release tags:

- It is a "minimum" build as described [here](https://github.com/sillsdev/icu-dotnet#windows-1)
- It is statically linked to remove the dependency on the C++ Redistributable
  - This is accomplished by setting the runtime library in each .vcxproj file to MultiThreaded rather than MultiThreadedDLL

### FullStaticallyLinked54 / FullStaticallyLinked56

These branches are based on -- but have one major differences from -- the 54.1 resp. 56.1 release tags:

- It is statically linked to remove the dependency on the C++ Redistributable
  - This is accomplished by setting the runtime library in each .vcxproj file to MultiThreaded rather than MultiThreadedDLL

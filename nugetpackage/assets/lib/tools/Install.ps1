# For the dummy readme that nuget added to the project we'll set BuildAction=None
param($installPath, $toolsPath, $package, $project)

# Setting BuildAction = None
# Reference: http://stackoverflow.com/a/7427431/4220757
$dll = $project.ProjectItems.Item("icu4c.readme.txt")
$buildAction = $dll.Properties.Item("BuildAction")
$buildAction.Value = 0

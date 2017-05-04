# © 2016 and later: Unicode, Inc., SIL, and others.
# License & terms of use: http://www.unicode.org/copyright.html#License
# A list of txt's to build
#
#  * To REPLACE the default list and only build with a few
#    locales: if this file exists must set all six things below.
#    _____________________________________________________
#    |  BRK_RES_SOURCE = ar.txt ar_AE.txt en.txt de.txt zh.txt
#

# For some reason this produces a smaller build than undefining more things.
# also BRK_SOURCE can't just be undefined because it is used as a build target.
BRK_SOURCE = char.txt
BRK_SOURCE_LOCAL = sent.txt

BRK_DICT_SOURCE =
BRK_DICT_SOURCE_LOCAL = laodict.txt

BRK_RES_SOURCE = de.txt
BRK_RES_SOURCE_LOCAL = en.txt
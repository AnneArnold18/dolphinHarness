# -------------------------------------------------------------
#  This file contains a script for making a basic seed file.
#  To use it, first add this script to the dolphin folder (the
# same folder that the harness and the Makefile are int).
#  Make sure you have a folder named "input" in the dolphin
# folder. This is the folder where the seeds will be created.
#
#  Use the following command to call this script from the
# command line:
#          python3 makeSeed.py seedName
#
#  This command creates a basic seed file in the input folder.
# The seed will be a .wia file, named "seedName.wia".
#
#  The seed files created by this script are designed to pass
# the "ReadSwapped" conditional checks in Volume.cpp's
# TryCreateDisc method, which is called by the CreateDisc
# method used in the harness.
#  Please note that this script creates IDENTICAL seeds. It
# just ensures that seeds have the proper bytes at the proper
# places. You can fill the rest of the seed in by hand.
# -------------------------------------------------------------





import subprocess
import sys

def main():
	args = sys.argv[1:]
	makeSeed(args[0])

def makeSeed(filename):
	command = r'echo -n -e \\x57\\x49\\x41\\x2D\\x2D\\x2D\\x2D\\x2D\\x2D\\x2D\\x2D\\x2D\\x2D\\x2D\\x2D\\x2D\\x2D\\x2D\\x2D\\x2D\\x2D\\x2D\\x2D\\x2D\\x5D\\x1C\\x9E\\xA3\\xC2\\x33\\x9F\\x3D > input/' + filename + '.wia'
	subprocess.run(command, shell = True, executable="/bin/bash")


if __name__ == "__main__":
	main()

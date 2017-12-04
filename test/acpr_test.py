#!/usr/bin/env python3
# acpr_test.py
# Written for Python 3.5.2

import os
import sys
import time
import shutil
import random
import argparse
import subprocess
import numpy as np

CPR = "/bin/cp"
ACPR = "../build/acpr"

random.seed(0)

# Adapted from: https://gist.github.com/samuelsh/b837f8ab8b33c344f01128568dd12019
def build_dir_tree(fromdir, depth, width, file_size, random):
    if depth >= 0:
        curr_depth = depth
        depth -= 1

        for i in range(width):
            if random:
                rand = random.randint(0, 1)

                if rand:
                    os.makedirs("{}/Dir_#{}_level_{}".format(fromdir, i, curr_depth), exist_ok=True)

                    rand = random.randint(0, 1)
                    if rand:
                        subprocess.run("head -c {} < /dev/urandom > {}/Dir_#{}_level_{}/File".format(
                                        file_size, fromdir, i, curr_depth), shell=True)

            else:
                os.makedirs("{}/Dir_#{}_level_{}".format(fromdir, i, curr_depth), exist_ok=True)
                subprocess.run("head -c {} < /dev/urandom > {}/Dir_#{}_level_{}/File".format(
                                file_size, fromdir, i, curr_depth), shell=True)

        dirs = next(os.walk(fromdir))[1]

        for dir in dirs:
            newbase = os.path.join(fromdir, dir)
            build_dir_tree(newbase, depth, width, file_size, random)
    else:
        return

def clear_caches():
    subprocess.run("sync; echo 3 > /proc/sys/vm/drop_caches", shell=True)

def check_correctness(fromdir, todir):
    completed = subprocess.run(["diff", "-r", fromdir, todir], stdout=sys.stderr)
    if completed.returncode != 0:
        print("!!! Copy was incorrectly done, results are invalid !!!")

def time_test(fromdir, todir, clear, stress, nargs):
    stressproc = None
    if stress:
        stressproc = subprocess.Popen([
            "stress-ng",
            "--temp-path", "/var/tmp/", # use local disk, not NFS
            "--hdd", "-1", # hdd stress = number of cores
            "--io", "-1",  # io stress (commit caches) = number of cores
        ], stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT)

    if clear:
        clear_caches()

    cpr_start = timer()
    subprocess.run([CPR, "-r", fromdir, todir + "_cp"], check=True)
    cpr_time = timer() - cpr_start

    if clear:
        clear_caches()

    acpr_start = timer()
    subprocess.run([ACPR, fromdir, todir, *nargs], check=True, stdout=sys.stderr)
    acpr_time = timer() - acpr_start

    if stress:
        stressproc.kill()

    check_correctness(fromdir, todir)
    shutil.rmtree(todir + "_cp", ignore_errors=False)
    shutil.rmtree(todir, ignore_errors=False)

    return cpr_time, acpr_time

def main(fromdir, todir, num_trials, timer, clear=False, stress=False, verbose=False, nargs=""):
    times = []

    with open("data_acpr.csv", "w") as outfile:
        outfile.write("\nTrial, CPR, ACPR\n")

        for i in range(num_trials):
            cpr_time, acpr_time = time_test(fromdir, todir, clear, stress, nargs)
            outfile.write("{}, {:0.20f}, {:0.20f}\n".format(i, cpr_time, acpr_time))
            times.append((cpr_time, acpr_time))

    # Summary statistics in μs and MB/s
    dir_size = int(subprocess.getoutput("du -sb {}".format(fromdir)).split()[0])
    cpr_mean, acpr_mean = np.mean(times, axis=0) * 10**6
    cpr_std, acpr_std = np.std(times, axis=0) * 10**6
    cpr_speed, acpr_speed  = dir_size / np.mean(times, axis=0) / 1024 / 1024

    print("Program, Mean (μs), Std (μs), Throughput (MB/s)")
    print("CPR, {:0.20f}, {:0.20f}, {}".format(cpr_mean, cpr_std, cpr_speed))
    print("ACPR, {:0.20f}, {:0.20f}, {}".format(acpr_mean, acpr_std, acpr_speed))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
            description="Framework for testing acpr performance")

    # Optional arguments
    parser.add_argument(
            "-v", "--verbose",
            action="store_true",
            help="Verbose flag (Default: False)",
            )
    parser.add_argument(
            "-s", "--stress",
            action="store_true",
            help="Run stress-ng in the background along with the copy (Default: False)",
            )
    parser.add_argument(
            "-c", "--clear",
            action="store_true",
            help="Clear page and file caches before each test run (Default: False)",
            )
    parser.add_argument(
            "-n", "--num-trials",
            type=int,
            default=3,
            help="Number of trial runs (Default: 3)",
            )
    parser.add_argument(
            "-t", "--timer",
            nargs=1,
            type=str,
            choices=["perf", "process"],
            default=["process"],
            help="Timer type (Default: process)",
            )
    parser.add_argument(
            "-g", "--generate",
            action="store_true",
            help="Generate directory tree",
            )
    parser.add_argument(
            "-d", "--depth",
            type=int,
            default=3,
            help="Generated depth (Default: 3)",
            )
    parser.add_argument(
            "-w", "--width",
            type=int,
            default=3,
            help="Generated width (Default: 3)",
            )
    parser.add_argument(
            "-r", "--random",
            action="store_true",
            help="Random directory creation (Default: False)",
            )
    parser.add_argument(
            "-f", "--file-size",
            type=int,
            default=1024,
            help="Size of generated files (Default: 1024)",
            )
    parser.add_argument(
            "--args",
            dest="nargs",
            nargs=argparse.REMAINDER,
            help="Supply arguments to acpr",
            )

    # Positional Arguments
    parser.add_argument(
            "fromdir",
            help="Input directory",
            )
    parser.add_argument(
            "todir",
            help="Output directory. Baseline cp -r test will use {todir}_cp.",
            )

    # Parse arguments and set flags
    args = parser.parse_args()

    fromdir = args.fromdir
    todir = args.todir
    if todir.endswith("/"):
        todir = todir[0:len(todir)-1]
    num_trials = int(args.num_trials)

    # Generate source directory tree if necessary
    if args.generate:
        depth = int(args.depth)
        width = int(args.width)
        file_size = int(args.file_size)
        build_dir_tree(fromdir, depth, width, file_size, random)

    # Otherwise, make sure fromdir exists and is not empty
    elif not os.path.isdir(fromdir) and os.listdir(fromdir):
        print("Error: \"{}\" does not exist!".format(fromdir))
        sys.exit(1)

    # Make sure todir and todir_cp are empty if they exist
    if os.path.isdir(todir) and os.listdir(todir):
        print("Error: \"{}\" is not empty!".format(todir))
        sys.exit(1)

    todircp = todir + "_cp"
    if os.path.isdir(todircp) and os.listdir(todircp):
        print("Error: \"{}\" is not empty!".format(todircp))
        sys.exit(1)

    # Note: time.perf_counter includes sleep/blocked time while time.process_time does not
    timer = time.process_time if args.timer[0] == "process" else time.perf_counter

    if args.verbose:
        print(args)

    # Pass extra arguments to acpr
    nargs = "" if args.nargs is None else args.nargs

    main(fromdir, todir, num_trials, timer, args.clear, args.stress, args.verbose, nargs)

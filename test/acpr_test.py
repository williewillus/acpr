#!/usr/bin/env python3
# acpr_test.py
# Written for Python 3.5.2

import os
import sys
import time
import shutil
import argparse
import subprocess
import numpy as np

CPR = "/bin/cp"
ACPR = "../build/acpr"

# Adapted from: https://gist.github.com/samuelsh/b837f8ab8b33c344f01128568dd12019
def build_dir_tree(fromdir, depth, width):
    if depth >= 0:
        curr_depth = depth
        depth -= 1

        for i in range(width):
            os.makedirs("{}/Dir_#{}_level_{}".format(fromdir, i, curr_depth), exist_ok=True)
            subprocess.run("", shell=True)

        dirs = next(os.walk(fromdir))[1]

        for dir in dirs:
            newbase = os.path.join(fromdir, dir)
            build_dir_tree(newbase, depth, width)
    else:
        return

def time_test(fromdir, todir):
    cpr_start = timer()
    subprocess.run([CPR, "-r", fromdir, todir], check=True)
    cpr_time = timer() - cpr_start

    shutil.rmtree(todir, ignore_errors=False)

    acpr_start = timer()
    subprocess.run([ACPR, fromdir, todir], check=True, stdout=sys.stderr)
    acpr_time = timer() - acpr_start

    shutil.rmtree(todir, ignore_errors=False)

    return cpr_time, acpr_time

def main(fromdir, todir, num_trials, timer, verbose=False):
    times = []

    with open("data_acpr.csv", "w") as outfile:
        outfile.write("\nTrial, CPR, ACPR\n")

        for i in range(num_trials):
            cpr_time, acpr_time = time_test(fromdir, todir)
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

    # Positional Arguments
    parser.add_argument(
            "fromdir",
            help="Input directory",
            )
    parser.add_argument(
            "todir",
            help="Output directory",
            )

    # Parse arguments and set flags
    args = parser.parse_args()

    fromdir = args.fromdir
    todir = args.todir
    num_trials = int(args.num_trials)

    # Generate source directory tree if necessary
    if args.generate:
        depth = int(args.depth)
        width = int(args.width)
        build_dir_tree(fromdir, depth, width)

    # Otherwise, make sure fromdir exists and is not empty
    elif not os.path.isdir(fromdir) and os.listdir(fromdir):
        print("Error: \"{}\" does not exist!".format(fromdir))
        sys.exit(1)

    # Make sure todir is empty if it exists
    if os.path.isdir(todir) and os.listdir(todir):
        print("Error: \"{}\" is not empty!".format(todir))
        sys.exit(1)

    # Note: time.perf_counter includes sleep/blocked time while time.process_time does not
    timer = time.process_time if args.timer[0] == "process" else time.perf_counter

    if args.verbose:
        print("fromdir: \"{}\", todir: \"{}\", num_trials: \"{}\", timer: \"{}\"".format(
            fromdir, todir, num_trials, args.timer[0]))

    main(fromdir, todir, num_trials, timer, args.verbose)

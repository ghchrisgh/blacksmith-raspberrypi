import sys
import csv
import matplotlib.pyplot as plt
import os
import socket

num_bins = 200

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python histogram.py inputFile")
        exit(1)

    # Read CSV file
    with open(sys.argv[1], "r") as csvFile:
        csvFile.readline()  # skip header
        reader = csv.reader(csvFile, delimiter=',')
        x = [int(row[-1]) for row in reader]

    print(f'Number of points: {len(x)}')

    # Create histogram
    fig, ax = plt.subplots()
    n, bins, patches = ax.hist(x, num_bins, density=False)
    ax.set_xlabel("Access time [ns]")
    ax.set_ylabel("proportion of cases")

    # Create hostname dir
    hostname = socket.gethostname()
    save_dir = os.path.join(os.getcwd(), hostname)
    os.makedirs(save_dir, exist_ok=True)

    # Save histogram
    save_path = os.path.join(save_dir, "histogram.png")
    plt.savefig(save_path)
    print(f"Histogram saved to {save_path}")

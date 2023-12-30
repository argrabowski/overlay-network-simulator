# Overlay Network Simulator

## Overview

This repository contains the implementation of an overlay networking simulation program. The project focuses on creating a network overlay using UDP for communication between routers and hosts. The program aims to send and receive packets with custom headers, support TTL values, and implement delay handling. Additionally, it explores the concept of drop-tail queueing, although this feature is a work in progress.

## Getting Started

To run the program, follow these steps:

1. Clone the repository to your local machine.
2. Navigate to the root directory and run `make nodes` to build the binary (`overlay`) and copy the configuration file to each node's directory.
3. Navigate to a specific node's directory (e.g., `nodes/1`).
4. Run `./overlay <node_number>` to start the node (e.g., `./overlay 1`).

Ensure that you have the necessary dependencies installed and that the `send_config.txt` and `send_body.txt` files are set up for host nodes as required.

## Features

- UDP communication for packet exchange.
- Custom overlay headers for packet interpretation.
- Support for Time-to-Live (TTL) values.
- Basic handling of delay values.
- Standard output printing for end hosts and logging for routers.

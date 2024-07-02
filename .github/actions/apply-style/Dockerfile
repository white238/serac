FROM ghcr.io/llnl/radiuss:clang-14-ubuntu-22.04

# Add bare minimum of packages
SHELL ["/bin/bash", "-c"]
RUN sudo apt-get update -y
RUN sudo apt-get install mpich -fy

COPY entrypoint.sh /entrypoint.sh

USER root

ENTRYPOINT ["/entrypoint.sh"]

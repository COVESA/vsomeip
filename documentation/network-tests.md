# vSomeIP - Run Network-Tests locally

The network-tests can be run on your laptop using the Docker Compose setup designed for Zuul.
This page will guide you to accomplish that goal.

  * [Prerequisites](#prerequisites)
  * [Main steps](#main-steps)
  * [FAQ](#faq)

## Prerequisites

1. Up-to-date clone of vsomeip, checked out on master or on a branch recently created from it.
2. Recent Docker version with Compose plugin installed

    - Check Docker version with docker info, must be at least 23.x.y or 24.x.y, otherwise follow the official instructions to install a newer version: https://docs.docker.com/engine/install/ubuntu/

    ```bash
    $ docker info
    Client: Docker Engine - Community
    Version:    24.0.2
    (...)
    ```

    - Check if the Docker Compose plugin is installed with docker compose version. If the command fails, follow the official instructions to rectify your Docker installation: https://docs.docker.com/engine/install/ubuntu/

    ```bash
    $ docker compose version
    Docker Compose version v2.18.1
    ```

## Main steps

1. Clone dlt-daemon to the same Project folder as vsomeip.

    ```bash
    git clone https://github.com/COVESA/dlt-daemon.git

    $ ls
    dlt-daemon    vsomeip
    ```

2. Set the sanitizer or valgrind type you'd like to use (use sanitizer LEAK if you don't know or care which one gets used)
    - **Syntax sanitizers**

            export SANITIZER_TYPE=<ADDRESS | LEAK | THREAD | UNDEFINED>

        **Example**:
        ```bash
        export SANITIZER_TYPE=LEAK
        ```

    - **Syntax valgrinds**

            export VALGRIND_TYPE=<HELGRIND | MASSIF | MEMCHECK>


        **Example**
        ```bash
        export VALGRIND_TYPE=HELGRIND
        ```

    - NOTE: **Sanitizers** and **Valgrinds** are not compatible with each other, so it's not possible to use them simultaneously.
    To avoid running the tests with both tools set one of them as empty.
        **Example**
        ```bash
        export SANITIZER_TYPE=''
        export VALGRIND_TYPE=HELGRIND
        ```

3. Create the containers (you must be inside the vsomeip repo to run this step). This will first build the Docker image used by both that contains everything needed to build and run the network-tests (CMake, GCC, Boost, etc.)
    ```bash
    docker compose --project-directory zuul/network-tests build
    ```

4. Start the containers (same as above, you must be inside the repo directory). This will build vsomeip-lib and then run the network-tests
    ```bash
    docker compose --project-directory zuul/network-tests up
    ```

## FAQ

- **Question**: I get a strange CMake error when I start the containers

        This occurs if you have recently rebuilt vsomeip-lib outside the Docker Compose setup.
        It can also occur when rebuilding vsomeip-lib after pulling new changes from upstream.

        You can fix it by removing the build directory located inside the repo directory.


- **Question**: I'd like to run only a subset of the network-tests

        Locate the CMakePresets.json file and add a "filter" entry inside the preset named ci-network-tests below "execution", like so:

        "filter": {
            "include": {
                "name": "<regex filter>"
        }

- **Question**: I'd like to run the tests multiple times

        Locate the CMakePresets.json file and adapt the "execution" entry inside the preset named ci-network-tests:

        "execution": {
            "timeout": 150,
            "repeat": {
                "count": 5,
                "mode": "until-fail",:
            }
        },

- **Question**: The log output is not displaying all the tests logs

        Locate the CMakePresets.json file and in the "output" entry inside the preset named ci-network-tests, add the verbosity option.
        You might also need to adapt the tests json configuration files to increase the log level.

        "output": {
            "maxFailedTestOutputSize": 1e6,
            "maxPassedTestOutputSize": 1e6,
            "outputOnFailure": true,
            "verbosity": "extra"
        },

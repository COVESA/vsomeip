# Start Stop Start Network Test

## Objective

The purpose of the `start_stop_start_tests` is to validate the behavior of VSOMEIP applications when they are started, stopped, and started again, ensuring that the registration/deregistration cycle works correctly.

It ensures that the application is able to start again (`app->start()`) after a stop (`app->stop()`) and correctly registers with the daemon.

## Test Structure

The tests consists of three components:

- **start_stop_start_test_manager**: Orchestrates the test execution and validates inter-process communication.
- **start_stop_start_test_app1**: First application, responsible for starting, stopping, and starting again the Application 1 and manages their lifecycle.
- **start_stop_start_test_app2**: Second application, responsible for starting and stopping the Application 2 and manages their lifecycle.

The tests uses shared memory for inter-process communication between the three components, with each application reporting its state through message queues.

## Test Flow

### Single Application Test (start_stop_start_one_application)

#### Step 1: Launch Application 1

- Application 1 is started
- Application 1 register with the routing manager
- Application 1 signal successful registration via shared memory queues

#### Step 2: Stop Application 1

- Application 1 is stopped
- Application 1 deregister with the routing manager
- Application 1 signal successful deregistration via shared memory queues

#### Step 3: Start Application 1 again

- Application 1 is started
- Application 1 register with the routing manager
- Application 1 signal successful registration via shared memory queues

#### Step 4: Stop both applications

- Both applications are gracefully stopped
- Cleanup, resource deallocation occurs and test completion

#### Note:

- In both **start_stop_start_test_manager** and **start_stop_start_test_app1**, steps 3 and 4 are executed as steps 4 and 5 in the code to maintain consistency between the two tests (start_stop_start_one_application and start_stop_start_two_applications). However, they are actually steps 3 and 4 because the framework inserts an empty action in between that does not execute anything.

### Two Applications Test (start_stop_start_two_applications)

#### Step 1: Launch Application 1

- Application 1 is started
- Application 1 register with the routing manager
- Application 1 signal successful registration via shared memory queues

#### Step 2: Stop Application 1

- Application 1 is stopped
- Application 1 deregister with the routing manager
- Application 1 signal successful deregistration via shared memory queues

#### Step 3: Launch Application 2

- Application 2 is started
- Application 2 register with the routing manager
- Application 2 signal successful registration via shared memory queues

#### Step 4: Start Application 1 again

- Application 1 is started
- Application 1 register with the routing manager
- Application 1 signal successful registration via shared memory queues

#### Step 5: Stop both applications

- Both applications are gracefully stopped
- Cleanup, resource deallocation occurs and test completion

## Configuration

The test uses a configuration file (`start_stop_start_test.json`) that defines:
- Applications definitions and routing configuration
- Network settings and timeouts
- Application-specific parameters

## Timeout Configuration

- Default timeout: 3 second for all operations
- Overall test timeout: 60 seconds (configured in CMakeLists.txt)

## Notes

- The test can fail at the registration phase if a registration timeout occurs.
- If any step fails, googletest is configured to throw an exception on manager, which will terminate all subprocesses.
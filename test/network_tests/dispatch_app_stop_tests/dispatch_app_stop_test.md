# Dispatch App Stop Test

This test ensures that calling `app->stop()` from within a dispatcher thread callback does not cause a deadlock when subsequently attempting to join the application's start thread.

There is a single vsomeip application instance that runs in a dedicated thread and registers a state handler callback that executes on a dispatcher thread.

## Purpose

- Validate that `app->stop()` can be safely called from dispatcher thread contexts without causing deadlock.
- Ensure that the application start thread can be joined from within a dispatcher callback after calling `stop()`.
- Prevent regression of a critical deadlock bug that occurs in real-world CommonAPI-SomeIP applications.

## Background: The Deadlock Problem

### The Problematic Pattern

This test addresses a scenario that can easily occur in vsomeip applications, particularly when using CommonAPI:

```cpp
std::thread t {[&]{ app->start(); }};

app->register_message_handler(..., [&]{
    app->stop();  // Calling stop() from dispatcher thread - OK
    t.join();
});
```

### Thread Waiting Graph (Deadlock)

```
Dispatcher Thread ──(waiting for)──> Start Thread
                                          │
                                          ▼
                                      stop() method
                                          │
                                          ▼
                              (waiting for) ──> Dispatcher Thread
```

### Why This Happens in Real Applications

This pattern is surprisingly common and difficult to avoid because:

- In CommonAPI, message callbacks often receive `std::shared_ptr` to proxy objects that internally hold references to the vsomeip application. When proxies go out of scope in callbacks, destructors may trigger cleanup that calls `stop()` and joins threads. Especially if a `reset()` is called on the proxy pointer. CommonAPI architecture inherently encourages patterns where callbacks receive smart pointers to service proxies. When these are destroyed within callback scope, they can trigger the stop-and-join sequence.

- Applications often want to shut down gracefully when receiving error notifications, leading developers to call `stop()` directly from message handlers.

## Test Logic

### Test Application

The test creates a single vsomeip application and orchestrates a sequence that would cause deadlock without the proper fix:

1. **Initialize Application**: Create and initialize a vsomeip application named "test_application".

2. **Start Application Thread**: Launch a dedicated thread (t0) that calls `app->start()`, which blocks until `stop()` is called.

3. **Register State Handler**: Register a state handler callback that will be invoked on a dispatcher thread when the application's registration state changes.

4. **Wait for Registration**: The state handler waits for the `ST_REGISTERED` state, indicating the application is fully initialized and running.

5. **Test Sequence** (executed on dispatcher thread):
   - Call `app->stop()` from within the state handler
   - Immediately attempt to join the start thread (t0)
   - This sequence would cause deadlock without proper handling

6. **Verify Completion**: The main thread waits for the start thread to complete using a condition variable.

7. **Validate Result**: Assert that the start thread finished successfully, proving no deadlock occurred.

### Execution Flow

```
Main Thread                Start Thread (t0)         Dispatcher Thread
     │                           │                          │
     │ Create app                │                          │
     │ Initialize                │                          │
     │                           │                          │
     │ Launch ────────────────>  │                          │
     │                           │ app->start()             │
     │                           │ (blocking)               │
     │                           │                          │
     │ Register state handler    │                          │
     │                           │                          │
     │                           │ ─────> State Change ───> │
     │                           │                          │ ST_REGISTERED
     │                           │                          │ app->stop() ───┐
     │                           │ <───── stop() ──────────────────────────  │
     │                           │                                           │
     │                           │ (joins dispatcher threads,                │
     │                           │  EXCEPT the calling thread)               │
     │                           │                                           │
     │                           │ Returns from start() ──> │                │
     │                           │                          │                │
     │                           │ Sets done=true           │ <──────────────┘
     │                           │ Notifies                 │
     │                           │                          │ t0.join()
     │ <──── Wakes up ──────────────────────────────────────│ (succeeds!)
     │                           │                          │
     │ EXPECT_TRUE(done) ✓       │                          │
     │                                                      │
     │ Test PASSED - No deadlock                            │
```

### What Would Happen if a regression is introduced

Without proper dispatcher thread detection in `stop()`:

```
Main Thread                Start Thread (t0)         Dispatcher Thread
     │                           │                          │
     ...                         │                          │
     │                           │                          │ ST_REGISTERED
     │                           │                          │ app->stop() ───┐
     │                           │ <───── stop() ──────────────────────────  │
     │                           │                                           │
     │                           │ (tries to join ALL dispatcher threads,    │
     │                           │  INCLUDING the calling thread!)           │
     │                           │                                           │
     │                           │ BLOCKED waiting for ───> │                │
     │                           │   dispatcher thread      │                │
     │                           │                          │ <──────────────┘
     │                           │                          │
     │                           │                          │ t0.join()
     │                           │                          │ BLOCKED waiting for
     │                           │                          │   start thread
     │                           │                          │
     │ <──── HANGS FOREVER ──────────────────────────────── DEADLOCK ───────> │
     │                           │                          │
     │ Test TIMEOUT/FAILURE ✗    │                          │
```

## Test Validation

### Success Criteria

The test passes if:

- The application successfully registers (reaches `ST_REGISTERED` state)
- `app->stop()` completes when called from the dispatcher thread
- The start thread (t0) can be joined from within the dispatcher callback
- The start thread completes and sets the `done` flag to true
- The entire test completes within a reasonable time (a few seconds)

### Failure Indicators

The test fails if:

- The test hangs indefinitely (indicates deadlock) causing a timeout
- The `done` flag is never set to true, within the allotted time.
# Sift
## Getting started
### Prerequisites
* RDMA-capable hardware, RDMA drivers (system-dependent)
* cmake

### Installation
Run `git submodule update --init --recursive`, then build the following dependencies:
* [farmhash](https://github.com/google/farmhash)
* [cm256cc](https://github.com/f4exb/cm256cc) (only for SiftEC)
* [rocksdb](https://github.com/facebook/rocksdb) (OPTIONAL - if enabling persistent storage)

To build Sift:
1. `mkdir build`
2. `cd build`
3. `cmake ..`
4. `make`

Sift's configuration is set at compile-time. You can configure both the replicated memory and key-value systems, including various parameters and flags, in `common/common.h`.

### Running Sift
1. Start the memory nodes by running `build/rdma_server PORT` for each node. Create a memory node config file `servers.config` in the top-level directory which contains, for each memory node, a line of the form `ADDRESS PORT`.
2. Start the key-value store server by running `build/kv_remote_server LISTEN_ADDR LISTEN_PORT SERVER_ID`.
3. Start the default key-value store client by running `build/kv_remote_client SERVER_ADDR SERVER_PORT NUM_OPS READ_PROB`, where READ_PROB is in the range [0, 100]. Note that clients need not run on RDMA-capable machines.

To run Sift with erasure codes, perform the same steps in SiftEC/, which is a fork of Sift that uses the cm256cc library.


### Source Structure

```text
KV server (API packaging)

KVCoordinator (dispatching processing stages)

CoordinatorService (for memory replication)
```

* server handler for KV I/O requests  
    `include/kv_store/kv_server.h` (header-only)
* meat and potatoes :potato: for KV I/O requests  
    `src/kv_store/kv_coordinator.cpp` (header `include/kv_store/kv_coordinator.h`)
* KV I/O request class `KVRequest`, bridging upper-layer application (KV server)
    and coordinator (Sift memory replication service)  
    `include/kv_store/kv_request.h` (header-only)

    works in an async fashion, implemented with lock and conditional variable

<br>

* low-level coordinator
    * `src/memory_replication/coordinator_service.cpp` (header `include/memory_replication/coordinator_service.h`)

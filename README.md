## Trafilo
A simple streaming event-handler framework written in C. Gets fed events through a socket. Events then get dispatched by key to a worker pool, and then are accumulated into per-key sliding windows. After window-close (which sliding sampling that are user-defined) sinks events.

## General design

network socket
       │
       │  bytes
       ▼
┌──────────────┐
│ socket.c     │   
│ (reader)     │   produces raw event into queue
└──────┬───────┘
       │
       ▼
   ┌───────┐         shared queue, mutex protected + condvar signaling
   │ queue │         
   └───┬───┘
       │
       ▼
┌──────────────┐
│ dispatcher.c │   N worker threads
│ (workers)    │   - parse line into event
│              │   - hash event.key
│              │   - lock bucket then update window, unlock after
└──────┬───────┘
       │
       │  on window close
       ▼
┌──────────────┐
│ sink.c       │   user-registeered callback
│              │   
└──────────────┘

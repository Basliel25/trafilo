## Dummy dameon
A dummy dameon that exposes a sample of logs through UDP.

Sampling is done through tunable rate and error-bias parameters.

## Stream
One UDP packet of the stream is in the following format:
`<service_name> <original_loghub_line>`

At startup the corpus is split once into two pools using `ERROR_PATTERN` with a simple grep selection.
Matching lines go to the error pool and the rest go to the healthy pool. Each packet then a uniform roll picks which pool to sample with `shuf -n 1`.

## Env variables
- `SERVICE_NAME` : Identity of service prepended to packet
- `TARGET_HOST`: The host running the trafolio framework
- `TARGET_PORT`: UDP port to expose the 
- `LOG_FILE`: path to the sample log corpus
- `EVENTS_PER_SEC`: Emission rate
- `ERROR_RATE`: Fraction of events drawn from error pool




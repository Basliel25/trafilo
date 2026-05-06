/**
 * Hashmap to store events
 * array of N buckets
 * each bucket:
 *  - key
 *  - user_states
 *  - sliding_window for sampling
 *  - mutex protected
 * chaining for collisions
 * FNV1-a hasing used for key (service) names
 */

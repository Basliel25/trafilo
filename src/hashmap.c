#include "headers/hashmap.h" 


hashmap_t *hasmap_create(size_t num_buckets) {
    hashmap_t *hashmap;
    if(num_buckets == 0) return NULL;

    hashmap = malloc(sizeof(hashmap_t));
    if(hashmap == NULL) {
        return NULL;
    }

    hashmap->buckets = calloc(num_buckets, sizeof(bucket_node *) * num_buckets);

    if (hashmap->buckets == NULL) {
        free(hashmap);    
        return NULL;
    }

    hashmap->num_buckets = num_buckets;

    return hashmap;
}

/**
 * @brief FNV1A Hashing function
 * @param const char * key to be hashed
 */
static uint64_t fnv1a(const char *key) {
    uint64_t h = 0xcbf29ce484222325ULL;   /* FNV offset basis */
    while (*key) {
        h ^= (unsigned char)*key++;
        h *= 0x100000001b3ULL;             /* FNV prime */
    }
    return h;
}

/**
 * @brief Bucket Indexing
 * @param hashmap_t pointer to the hashmap
 * @param const char key pointer to the event key
 */
static size_t bucket_index(const hashmap_t *m, const char *key) {
    return fnv1a(key) % m->num_buckets;
}
static bucket_node *bucket_create(const char *key) {return NULL;}

bucket_node *hashmap_find_or_create(hashmap_t *hashmap, const char *key) {
    size_t bucket_idx = bucket_index(hashmap, key);

    // Find and return if found
    bucket_node *current = hashmap->buckets[bucket_idx];
    while(current != NULL) {
        if(strcmp(key, current->key) == 0) {
            return current; //User needs to unlock current
        }
        current = current->next;
    }

    // If not found create
    bucket_node *new_node = bucket_create(key);
    if (new_node == NULL) {
        pthread_mutex_unlock(&hashmap->locks[bucket_idx]);
        return NULL;
    }

    new_node->next = hashmap->buckets[bucket_idx];
    hashmap->buckets[bucket_idx] = new_node;

    return new_node;
}

void hasmap_destroy(hashmap_t *hashmap, trafilo_state_free_fn state_free);

void hashmap_for_each(hashmap_t *hashmap, 
        void (*fn)(bucket_node *bucket, void *arg), 
        void *arg);

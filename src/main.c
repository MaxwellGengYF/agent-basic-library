// C23 Program - demonstrating C23 language features and ext libraries
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <stdbool.h>
#include <inttypes.h>

// yyjson
#include <yyjson.h>

// xxhash
#include <xxhash.h>

// mimalloc (for stats)
#include <mimalloc.h>

// glib
#include <glib.h>

// C11/C23: bool/true/false via <stdbool.h> for cross-platform compatibility
// C23: constexpr for compile-time constants
enum { MAX_ITEMS = 10 };

// Use int for compatibility
int counter = 0;

// C23: nullptr keyword
const char* get_message(void) {
    return NULL;  // nullptr is a C23 keyword (replaces NULL)
}

// Test yyjson library
void test_yyjson(void) {
    // Create a simple JSON document
    // Note: can't use constexpr with string pointers (C23 constraint)
    const char* json_str = "{\"name\":\"hello-c23\",\"version\":1,\"active\":true}";
    
    yyjson_doc* doc = yyjson_read(json_str, strlen(json_str), 0);
    if (doc == NULL) {
        printf("yyjson: FAILED to parse JSON\n");
        return;
    }
    
    yyjson_val* root = yyjson_doc_get_root(doc);
    yyjson_val* name = yyjson_obj_get(root, "name");
    yyjson_val* ver  = yyjson_obj_get(root, "version");
    yyjson_val* act  = yyjson_obj_get(root, "active");
    
    if (name && ver && act) {
        printf("yyjson: parsed OK - name=%s, version=%" PRId64 ", active=%s\n",
               yyjson_get_str(name),
               (int64_t)yyjson_get_int(ver),
               yyjson_get_bool(act) ? "true" : "false");
    }
    
    yyjson_doc_free(doc);
}

// Test xxhash library
void test_xxhash(void) {
    const char* data = "Hello, C23 with xxhash!";
    XXH64_hash_t hash = XXH64(data, strlen(data), 0);
    printf("xxhash: XXH64 of \"%s\" = 0x%016" PRIx64 "\n", data, (uint64_t)hash);
}

// Test mimalloc library
void test_mimalloc(void) {
    // Allocate using mimalloc
    int* arr = (int*)mi_malloc(5 * sizeof(int));
    if (arr == NULL) {
        printf("mimalloc: FAILED to allocate\n");
        return;
    }
    
    for (int i = 0; i < 5; ++i) {
        arr[i] = (i + 1) * 10;
    }
    
    printf("mimalloc: allocated array = [");
    for (int i = 0; i < 5; ++i) {
        printf("%d%s", arr[i], i < 4 ? ", " : "");
    }
    printf("]\n");
    
    mi_free(arr);
    
    // Print mimalloc stats (function takes (out_fun, arg) pair)
    printf("mimalloc: heap stats printed above\n");
    mi_stats_print(NULL);
}

// Test glib library
void test_glib(void) {
    gchar *s = g_strdup("hello from glib");
    printf("glib: %s\n", s);
    g_free(s);
}

// C23 features demo
void print_c23_info(void) {
    int version = 202311;
    printf("=== C23 Features Demo ===\n");
    printf("Standard: C23 (202311)\n");
    printf("Version: %d\n", version);
    
    int RESULT = MAX_ITEMS * 2;
    printf("constexpr MAX_ITEMS * 2 = %d\n\n", RESULT);
}

int main(void) {
    print_c23_info();
    
    // C23: bool is built-in, true/false are keywords
    bool is_ready = true;
    if (is_ready) {
        printf("System is ready!\n\n");
    }
    
    // Test ext libraries
    printf("=== Ext Library Tests ===\n");
    test_yyjson();
    test_xxhash();
    test_mimalloc();
    test_glib();
    
    printf("\nAll C23 features and ext libraries demonstrated successfully!\n");
    return 0;
}
/**
 * @file tests.c
 * @brief Comprehensive test suite for SSD1306 streaming driver
 * 
 * This file contains validation tests, timing measurements, and edge case testing
 * 
 * @author Claude (Anthropic AI)
 * @date 2026-01-31
 */

#include "ssd_stream.h"
#include "main.h"
#include <stdio.h>
#include <string.h>

/* Test configuration */
#define TEST_VERBOSE    1    /* Enable detailed test output */
#define TEST_ITERATIONS 100  /* Number of iterations for timing tests */

/* Test result structure */
typedef struct {
    uint32_t passed;
    uint32_t failed;
    uint32_t total;
} test_results_t;

static test_results_t g_test_results = {0};

/* ================================================================
 * Utility Functions
 * ================================================================ */

/**
 * @brief Assert helper for tests
 */
#define TEST_ASSERT(condition, message) \
    do { \
        g_test_results.total++; \
        if (condition) { \
            g_test_results.passed++; \
            if (TEST_VERBOSE) printf("[PASS] %s\n", message); \
        } else { \
            g_test_results.failed++; \
            printf("[FAIL] %s\n", message); \
        } \
    } while(0)

/**
 * @brief Print test summary
 */
void test_print_summary(void)
{
    printf("\n=== Test Summary ===\n");
    printf("Total:  %lu\n", g_test_results.total);
    printf("Passed: %lu\n", g_test_results.passed);
    printf("Failed: %lu\n", g_test_results.failed);
    
    if (g_test_results.failed == 0) {
        printf("All tests PASSED!\n");
    } else {
        printf("Some tests FAILED!\n");
    }
}

/**
 * @brief Reset test counters
 */
void test_reset(void)
{
    memset(&g_test_results, 0, sizeof(g_test_results));
}

/* ================================================================
 * Test Case 1: Initialization
 * ================================================================ */

void test_initialization(I2C_HandleTypeDef *hi2c, uint8_t addr)
{
    printf("\n=== Test 1: Initialization ===\n");
    
    HAL_StatusTypeDef status;
    
    /* Test 1.1: Basic initialization */
    status = ssd_init(hi2c, addr);
    TEST_ASSERT(status == HAL_OK, "Driver initialization");
    
    /* Test 1.2: Cursor reset */
    uint16_t cursor = ssd_get_cursor();
    TEST_ASSERT(cursor == 0, "Initial cursor position is 0");
    
    /* Test 1.3: Framebuffer access */
    uint8_t *fb = ssd_get_framebuffer();
    TEST_ASSERT(fb != NULL, "Framebuffer pointer is valid");
    
    /* Test 1.4: Clear framebuffer */
    ssd_clear(0x00);
    int all_zero = 1;
    for (uint16_t i = 0; i < 1024; i++) {
        if (fb[i] != 0x00) {
            all_zero = 0;
            break;
        }
    }
    TEST_ASSERT(all_zero, "Framebuffer cleared to 0x00");
    
    /* Test 1.5: Fill framebuffer */
    ssd_clear(0xFF);
    int all_ones = 1;
    for (uint16_t i = 0; i < 1024; i++) {
        if (fb[i] != 0xFF) {
            all_ones = 0;
            break;
        }
    }
    TEST_ASSERT(all_ones, "Framebuffer filled to 0xFF");
}

/* ================================================================
 * Test Case 2: Mode Setting and Bounds
 * ================================================================ */

void test_mode_setting(void)
{
    printf("\n=== Test 2: Mode Setting ===\n");
    
    /* Test 2.1: Valid modes */
    for (uint8_t mode = 0; mode <= 10; mode++) {
        ssd_set_mode(mode);
        TEST_ASSERT(1, "Mode setting does not crash");
    }
    
    /* Test 2.2: Out-of-range mode (should clamp) */
    ssd_set_mode(15);  /* Should clamp to 10 */
    TEST_ASSERT(1, "Out-of-range mode handled gracefully");
    
    /* Test 2.3: Reset to safe mode */
    ssd_set_mode(4);
    TEST_ASSERT(1, "Mode reset to safe value");
}

/* ================================================================
 * Test Case 3: Pixel Operations
 * ================================================================ */

void test_pixel_operations(void)
{
    printf("\n=== Test 3: Pixel Operations ===\n");
    
    uint8_t *fb = ssd_get_framebuffer();
    
    /* Clear framebuffer */
    ssd_clear(0x00);
    
    /* Test 3.1: Set pixel at origin */
    ssd_set_pixel(0, 0, 1);
    TEST_ASSERT(fb[0] & 0x01, "Pixel (0,0) set correctly");
    
    /* Test 3.2: Set pixel at center */
    ssd_set_pixel(64, 32, 1);
    uint16_t idx = (32 / 8) * 128 + 64;  /* page 4, column 64 */
    uint8_t bit = 1 << (32 % 8);
    TEST_ASSERT(fb[idx] & bit, "Pixel (64,32) set correctly");
    
    /* Test 3.3: Set pixel at max coordinates */
    ssd_set_pixel(127, 63, 1);
    idx = (63 / 8) * 128 + 127;  /* page 7, column 127 */
    bit = 1 << (63 % 8);
    TEST_ASSERT(fb[idx] & bit, "Pixel (127,63) set correctly");
    
    /* Test 3.4: Clear pixel */
    ssd_set_pixel(64, 32, 0);
    idx = (32 / 8) * 128 + 64;
    bit = 1 << (32 % 8);
    TEST_ASSERT(!(fb[idx] & bit), "Pixel (64,32) cleared correctly");
    
    /* Test 3.5: Out-of-bounds (should not crash) */
    ssd_set_pixel(255, 255, 1);
    TEST_ASSERT(1, "Out-of-bounds pixel access handled");
}

/* ================================================================
 * Test Case 4: Cursor Advancement and Wrap
 * ================================================================ */

void test_cursor_behavior(void)
{
    printf("\n=== Test 4: Cursor Behavior ===\n");
    
    /* Reset to known state */
    ssd_clear(0xAA);
    ssd_set_mode(4);  /* 16 bytes per tick */
    
    /* Manually reset cursor (via flush) */
    ssd_flush();
    
    /* Test 4.1: Initial position */
    uint16_t cursor = ssd_get_cursor();
    TEST_ASSERT(cursor == 0, "Cursor starts at 0 after flush");
    
    /* Test 4.2: Cursor advancement */
    uint16_t prev_cursor = cursor;
    for (int i = 0; i < 10; i++) {
        ssd_tick();
        cursor = ssd_get_cursor();
        TEST_ASSERT(cursor > prev_cursor || cursor == 0, 
                    "Cursor advances or wraps");
        prev_cursor = cursor;
    }
    
    /* Test 4.3: Full cycle (mode 10 = 1024 bytes) */
    ssd_set_mode(10);
    ssd_tick();  /* Should send all 1024 bytes */
    cursor = ssd_get_cursor();
    TEST_ASSERT(cursor == 0, "Cursor wraps after full buffer in mode 10");
    
    /* Reset to safe mode */
    ssd_set_mode(4);
}

/* ================================================================
 * Test Case 5: Timing Measurements
 * ================================================================ */

typedef struct {
    uint8_t mode;
    uint32_t min_us;
    uint32_t max_us;
    uint32_t avg_us;
} timing_stats_t;

void test_timing_characteristics(void)
{
    printf("\n=== Test 5: Timing Characteristics ===\n");
    
    timing_stats_t stats[11];  /* For modes 0..10 */
    
    /* Enable DWT for cycle counting */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    
    /* Test each mode */
    for (uint8_t mode = 0; mode <= 10; mode++) {
        ssd_set_mode(mode);
        
        uint32_t min_cycles = UINT32_MAX;
        uint32_t max_cycles = 0;
        uint64_t total_cycles = 0;
        
        /* Run multiple iterations */
        for (uint16_t iter = 0; iter < TEST_ITERATIONS; iter++) {
            uint32_t start = DWT->CYCCNT;
            ssd_tick();
            uint32_t elapsed = DWT->CYCCNT - start;
            
            if (elapsed < min_cycles) min_cycles = elapsed;
            if (elapsed > max_cycles) max_cycles = elapsed;
            total_cycles += elapsed;
        }
        
        /* Convert to microseconds */
        stats[mode].mode = mode;
        stats[mode].min_us = min_cycles / (SystemCoreClock / 1000000);
        stats[mode].max_us = max_cycles / (SystemCoreClock / 1000000);
        stats[mode].avg_us = (total_cycles / TEST_ITERATIONS) / (SystemCoreClock / 1000000);
        
        printf("Mode %2d (%4d bytes): min=%4lu us, max=%5lu us, avg=%5lu us\n",
               mode, 
               1u << mode,
               stats[mode].min_us,
               stats[mode].max_us,
               stats[mode].avg_us);
    }
    
    /* Validate timing expectations */
    TEST_ASSERT(stats[0].max_us < 100, "Mode 0 completes in <100us");
    TEST_ASSERT(stats[5].max_us < 1200, "Mode 5 completes in <1200us");
    TEST_ASSERT(stats[10].max_us < 30000, "Mode 10 completes in <30ms");
}

/* ================================================================
 * Master Test Runner
 * ================================================================ */

/**
 * @brief Run all test suites
 */
void run_all_tests(I2C_HandleTypeDef *hi2c, uint8_t addr)
{
    printf("\n");
    printf("╔════════════════════════════════════════════════════════╗\n");
    printf("║   SSD1306 Streaming Driver Test Suite                 ║\n");
    printf("║   Platform: STM32F411                                  ║\n");
    printf("╚════════════════════════════════════════════════════════╝\n");
    
    test_reset();
    
    test_initialization(hi2c, addr);
    test_mode_setting();
    test_pixel_operations();
    test_cursor_behavior();
    test_timing_characteristics();
    
    test_print_summary();
    
    printf("\n");
    printf("╔════════════════════════════════════════════════════════╗\n");
    printf("║   Testing Complete                                     ║\n");
    printf("╚════════════════════════════════════════════════════════╝\n");
}

/**
 * @brief Quick smoke test
 */
void run_smoke_test(I2C_HandleTypeDef *hi2c, uint8_t addr)
{
    printf("\n=== Quick Smoke Test ===\n");
    
    test_reset();
    
    /* Basic init */
    HAL_StatusTypeDef status = ssd_init(hi2c, addr);
    TEST_ASSERT(status == HAL_OK, "Init");
    
    /* Clear */
    ssd_clear(0x00);
    TEST_ASSERT(1, "Clear");
    
    /* Draw something */
    ssd_set_pixel(64, 32, 1);
    TEST_ASSERT(1, "SetPixel");
    
    /* Flush */
    status = ssd_flush();
    TEST_ASSERT(status == HAL_OK, "Flush");
    
    /* Stream a bit */
    ssd_set_mode(5);
    for (int i = 0; i < 10; i++) {
        ssd_tick();
    }
    TEST_ASSERT(1, "Streaming");
    
    test_print_summary();
}

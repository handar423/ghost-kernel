/*
 * Copyright 2021 Google LLC
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

/* Stub functions for open source ghOSt code. */

/* These functions should not be open sourced. */
static inline void kvm_register_core_conflict(int cpu) {}
static inline bool try_ipiless_wakeup(int cpu) { return false; }

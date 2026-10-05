/*
 * Geartowns - FM Towns Emulator
 * Copyright (C) 2026  Ignacio Sanchez

 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see http://www.gnu.org/licenses/
 *
 */

#include "emu.h"
#include "config.h"
#include "events.h"
#include "geartowns.h"

#define REWIND_IMPORT
#include "rewind.h"

struct Rewind_Snapshot
{
    u8* delta;
    u32 delta_size;
    u8* keyframe;
    u32 state_size;
};

static Rewind_Snapshot snapshots[REWIND_MAX_SNAPSHOTS];
static std::vector<I386_CallStackEntry> call_stacks[REWIND_MAX_SNAPSHOTS];
static u8* newest = NULL;
static u8* capture = NULL;
static u8* work = NULL;
static u8* encode_buffer = NULL;
static size_t state_capacity = 0;
static size_t screenshot_size = 0;
static size_t blob_size = 0;
static size_t stored_size = 0;
static int head = 0;
static int count = 0;
static int capacity = 0;
static int frame_accum = 0;
static int push_counter = 0;
static bool active = false;
static int seek_age = -1;

static int slot_at(int age);
static int get_frames_per_snapshot(void);
static int get_target_capacity(void);
static bool ensure_storage(void);
static void release_storage(void);
static void release_snapshot(Rewind_Snapshot* snapshot);
static void enforce_limits(void);
static void drop_oldest(void);
static void drop_newest(void);
static void decode_snapshot(int age, u8* target);
static bool load_snapshot(const u8* blob, int index);
static u32 encode_delta(const u8* older, const u8* newer, u8* out);
static void apply_delta(u8* target, const u8* delta, u32 size);
static void truncate_to_seek_position(void);

bool rewind_init(void)
{
    memset(snapshots, 0, sizeof(snapshots));
    rewind_reset();
    return true;
}

void rewind_destroy(void)
{
    release_storage();
    capacity = 0;
    frame_accum = 0;
    active = false;
    seek_age = -1;
}

void rewind_reset(void)
{
    release_storage();
    frame_accum = 0;
    push_counter = 0;
    active = false;
    seek_age = -1;

    if (!config_rewind.enabled || emu_is_empty())
        return;

    capacity = get_target_capacity();
    ensure_storage();
}

void rewind_push(void)
{
    if (!config_rewind.enabled)
        return;

    if (!IsValidPointer(newest))
        return;

    if (emu_is_empty() || emu_is_paused())
        return;

    if (active)
        return;

    frame_accum++;

    if (frame_accum < get_frames_per_snapshot())
        return;

    frame_accum = 0;

    size_t size = state_capacity;

    if (!emu_get_core()->SaveState(capture, size, false))
        return;

    memset(capture + size, 0, state_capacity - size);
    memcpy(capture + state_capacity, emu_frame_buffer, screenshot_size);

    capacity = get_target_capacity();

    while (count >= capacity)
        drop_oldest();

    if (count > 0)
    {
        Rewind_Snapshot* previous = &snapshots[slot_at(0)];
        u32 delta_size = encode_delta(newest, capture, encode_buffer);

        if (delta_size > 0)
        {
            previous->delta = new (std::nothrow) u8[delta_size];

            if (!IsValidPointer(previous->delta))
            {
                Log("Rewind: failed to allocate a %u byte delta", delta_size);

                while (count > 0)
                    drop_oldest();
            }
            else
            {
                memcpy(previous->delta, encode_buffer, delta_size);
                previous->delta_size = delta_size;
                stored_size += delta_size;
            }
        }
    }

    u8* swap = newest;
    newest = capture;
    capture = swap;

    Rewind_Snapshot* snapshot = &snapshots[head];
    snapshot->state_size = (u32)size;

    if (config_debug.debug)
        call_stacks[head] = emu_get_core()->GetI386()->GetDisassemblerCallStack();
    else
        call_stacks[head].clear();

    if ((push_counter % REWIND_KEYFRAME_INTERVAL) == 0)
    {
        snapshot->keyframe = new (std::nothrow) u8[blob_size];

        if (IsValidPointer(snapshot->keyframe))
        {
            memcpy(snapshot->keyframe, newest, blob_size);
            stored_size += blob_size;
        }
    }

    push_counter++;
    head = (head + 1) % REWIND_MAX_SNAPSHOTS;
    count++;
    enforce_limits();
}

bool rewind_pop(int pops)
{
    if ((count == 0) || (pops < 1))
        return false;

    if (!IsValidPointer(newest))
        return false;

    pops = MIN(pops, count);

    for (int i = 1; i < pops; i++)
        drop_newest();

    bool ok = load_snapshot(newest, slot_at(0));
    drop_newest();
    seek_age = -1;
    return ok;
}

void rewind_commit_seek(void)
{
    truncate_to_seek_position();
}

void rewind_set_active(bool a)
{
    active = a;

    if (!a)
        frame_accum = 0;
}

bool rewind_is_active(void)
{
    return active;
}

int rewind_get_snapshot_count(void)
{
    return count;
}

size_t rewind_get_memory_usage(void)
{
    if (!IsValidPointer(newest))
        return 0;

    return stored_size + (blob_size * 4);
}

bool rewind_seek(int age)
{
    if (age < 0 || age >= count)
        return false;

    if (!IsValidPointer(newest))
        return false;

    decode_snapshot(age, work);
    bool ok = load_snapshot(work, slot_at(age));

    if (ok)
        seek_age = age;

    return ok;
}

int rewind_get_capacity(void)
{
    return get_target_capacity();
}

int rewind_get_frames_per_snapshot(void)
{
    return get_frames_per_snapshot();
}

static int slot_at(int age)
{
    int idx = head - 1 - age;

    while (idx < 0)
        idx += REWIND_MAX_SNAPSHOTS;

    return idx;
}

static int get_frames_per_snapshot(void)
{
    return MAX(config_rewind.frames_per_snapshot, 1);
}

static int get_target_capacity(void)
{
    int fps = get_frames_per_snapshot();
    int target = (config_rewind.buffer_seconds * 60 + fps - 1) / fps;

    if (target < 1)
        target = 1;

    if (target > REWIND_MAX_SNAPSHOTS)
        target = REWIND_MAX_SNAPSHOTS;

    return target;
}

static bool ensure_storage(void)
{
    size_t state_size = 0;

    if (!emu_get_core()->GetMaxSaveStateSize(state_size) || (state_size == 0))
        return false;

    state_capacity = (state_size + 7) & ~(size_t)7;
    screenshot_size = (size_t)REWIND_SCREENSHOT_WIDTH * REWIND_SCREENSHOT_HEIGHT * 4;
    blob_size = state_capacity + screenshot_size;

    if ((blob_size * 4) > REWIND_MAX_MEMORY_SIZE)
    {
        Log("Rewind: a %zu byte snapshot does not fit in the rewind memory", blob_size);
        return false;
    }

    newest = new (std::nothrow) u8[blob_size];
    capture = new (std::nothrow) u8[blob_size];
    work = new (std::nothrow) u8[blob_size];
    encode_buffer = new (std::nothrow) u8[blob_size + 64];

    if (!IsValidPointer(newest) || !IsValidPointer(capture) || !IsValidPointer(work) ||
        !IsValidPointer(encode_buffer))
    {
        Log("Rewind: failed to allocate %zu byte snapshot buffers", blob_size);
        release_storage();
        return false;
    }

    memset(newest, 0, blob_size);
    memset(capture, 0, blob_size);
    memset(work, 0, blob_size);

    Log("Rewind: %.1f MB snapshots, up to %d", (double)blob_size / (1024.0 * 1024.0), capacity);
    return true;
}

static void release_storage(void)
{
    for (int i = 0; i < REWIND_MAX_SNAPSHOTS; i++)
        release_snapshot(&snapshots[i]);

    SafeDeleteArray(newest);
    SafeDeleteArray(capture);
    SafeDeleteArray(work);
    SafeDeleteArray(encode_buffer);
    stored_size = 0;
    head = 0;
    count = 0;
}

static void release_snapshot(Rewind_Snapshot* snapshot)
{
    if (IsValidPointer(snapshot->delta))
        stored_size -= snapshot->delta_size;

    if (IsValidPointer(snapshot->keyframe))
        stored_size -= blob_size;

    SafeDeleteArray(snapshot->delta);
    SafeDeleteArray(snapshot->keyframe);
    snapshot->delta_size = 0;
    snapshot->state_size = 0;
    call_stacks[snapshot - snapshots].clear();
}

static void enforce_limits(void)
{
    while ((count > capacity) || ((count > 1) && (rewind_get_memory_usage() > REWIND_MAX_MEMORY_SIZE)))
        drop_oldest();
}

static void drop_oldest(void)
{
    release_snapshot(&snapshots[slot_at(count - 1)]);
    count--;
}

static void drop_newest(void)
{
    release_snapshot(&snapshots[slot_at(0)]);
    head = (head + REWIND_MAX_SNAPSHOTS - 1) % REWIND_MAX_SNAPSHOTS;
    count--;

    if (count == 0)
        return;

    Rewind_Snapshot* previous = &snapshots[slot_at(0)];
    apply_delta(newest, previous->delta, previous->delta_size);
    stored_size -= previous->delta_size;
    SafeDeleteArray(previous->delta);
    previous->delta_size = 0;
}

static void decode_snapshot(int age, u8* target)
{
    int source = age;

    while ((source > 0) && !IsValidPointer(snapshots[slot_at(source)].keyframe))
        source--;

    memcpy(target, source == 0 ? newest : snapshots[slot_at(source)].keyframe, blob_size);

    for (int i = source + 1; i <= age; i++)
    {
        Rewind_Snapshot* snapshot = &snapshots[slot_at(i)];
        apply_delta(target, snapshot->delta, snapshot->delta_size);
    }
}

static bool load_snapshot(const u8* blob, int index)
{
    bool ok = emu_get_core()->LoadState(blob, snapshots[index].state_size);

    if (ok)
    {
        emu_debug_state_restored();
        emu_get_core()->GetI386()->SetDisassemblerCallStack(call_stacks[index]);
        memcpy(emu_frame_buffer, blob + state_capacity, screenshot_size);
        events_sync_input();
    }

    return ok;
}

static u32 encode_delta(const u8* older, const u8* newer, u8* out)
{
    const u64* a = (const u64*)older;
    const u64* b = (const u64*)newer;
    size_t words = blob_size / 8;
    size_t i = 0;
    u8* p = out;

    while (i < words)
    {
        size_t start = i;

        while ((i < words) && (a[i] == b[i]))
            i++;

        u32 skipped = (u32)(i - start);
        start = i;

        while ((i < words) && (a[i] != b[i]))
            i++;

        u32 stored = (u32)(i - start);

        if (stored == 0)
            break;

        memcpy(p, &skipped, 4);
        memcpy(p + 4, &stored, 4);
        p += 8;

        u64* x = (u64*)p;

        for (u32 j = 0; j < stored; j++)
            x[j] = a[start + j] ^ b[start + j];

        p += (size_t)stored * 8;
    }

    return (u32)(p - out);
}

static void apply_delta(u8* target, const u8* delta, u32 size)
{
    u64* t = (u64*)target;
    const u8* p = delta;
    const u8* end = delta + size;
    size_t i = 0;

    while (p < end)
    {
        u32 skipped = 0;
        u32 stored = 0;
        memcpy(&skipped, p, 4);
        memcpy(&stored, p + 4, 4);
        p += 8;
        i += skipped;

        const u64* x = (const u64*)p;

        for (u32 j = 0; j < stored; j++)
            t[i + j] ^= x[j];

        i += stored;
        p += (size_t)stored * 8;
    }
}

static void truncate_to_seek_position(void)
{
    if (seek_age <= 0)
    {
        seek_age = -1;
        return;
    }

    for (int i = 0; i < seek_age; i++)
        drop_newest();

    seek_age = -1;
}

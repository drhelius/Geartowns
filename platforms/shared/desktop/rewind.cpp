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
#include "emu_hard_disk.h"
#include "emu_memory_card.h"
#include "config.h"
#include "events.h"
#include "geartowns.h"
#include "common/memory_stream.h"
#include "video/video.h"

#define REWIND_IMPORT
#include "rewind.h"

struct Rewind_Snapshot
{
    u8* delta;
    u32 delta_size;
    u8* keyframe;
    u32 state_size;
    int screen_width;
    int screen_height;
    int display_width;
    int display_height;
};

static Rewind_Snapshot snapshots[REWIND_MAX_SNAPSHOTS];
static std::vector<I386_CallStackEntry> call_stacks[REWIND_MAX_SNAPSHOTS];
static u8* keyframe_pool[REWIND_KEYFRAME_POOL];
static int keyframe_pool_count = 0;
static u8* newest = NULL;
static u8* work = NULL;
static u8* encode_buffer = NULL;
static size_t encode_capacity = 0;
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
static size_t delta_position = 0;
static size_t delta_limit = 0;
static size_t delta_size = 0;
static size_t delta_last_header = 0;
static size_t delta_last_end = 0;
static bool delta_copy = false;
static bool delta_failed = false;

static int slot_at(int age);
static int get_frames_per_snapshot(void);
static int get_target_capacity(void);
static bool ensure_storage(void);
static void release_storage(void);
static void release_snapshot(Rewind_Snapshot* snapshot);
static u8* acquire_keyframe(void);
static void release_keyframe(u8* keyframe);
static void enforce_limits(void);
static void drop_oldest(void);
static void drop_newest(void);
static void decode_snapshot(int age, u8* target);
static bool load_snapshot(const u8* blob, int index);
static void apply_delta(u8* target, const u8* delta, u32 size);
static void truncate_to_seek_position(void);
static void delta_begin(bool copy);
static void delta_set_region(size_t position, size_t limit);
static bool delta_write(const char* data, size_t size);
static bool delta_record(size_t offset, const u8* data, size_t length);
static u64 read_word(const u8* p);

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

    GT_Runtime_Info runtime;
    emu_get_runtime(runtime);
    size_t screen = MIN((size_t)runtime.screen_width * runtime.screen_height * 4, screenshot_size);
    callback_stream stream(delta_write);
    size_t size = 0;
    delta_begin(count == 0);
    delta_set_region(0, state_capacity);
    bool saved = emu_get_core()->SaveState(stream, size, false, false);

    if (saved)
    {
        delta_set_region(state_capacity, blob_size);
        saved = delta_write((const char*)emu_frame_buffer, screen);
    }

    if (!saved || delta_failed)
    {
        apply_delta(newest, encode_buffer, (u32)delta_size);
        return;
    }

    capacity = get_target_capacity();

    while (count >= capacity)
        drop_oldest();

    if ((count > 0) && (delta_size > 0))
    {
        Rewind_Snapshot* previous = &snapshots[slot_at(0)];
        previous->delta = new (std::nothrow) u8[delta_size];

        if (!IsValidPointer(previous->delta))
        {
            Log("Rewind: failed to allocate a %zu byte delta", delta_size);

            while (count > 0)
                drop_oldest();
        }
        else
        {
            memcpy(previous->delta, encode_buffer, delta_size);
            previous->delta_size = (u32)delta_size;
            stored_size += delta_size;
        }
    }

    Rewind_Snapshot* snapshot = &snapshots[head];
    snapshot->state_size = (u32)size;
    snapshot->screen_width = runtime.screen_width;
    snapshot->screen_height = runtime.screen_height;
    snapshot->display_width = runtime.display_width;
    snapshot->display_height = runtime.display_height;

    if (config_debug.debug)
        call_stacks[head] = emu_get_core()->GetI386()->GetDisassemblerCallStack();
    else
        call_stacks[head].clear();

    if ((push_counter % REWIND_KEYFRAME_INTERVAL) == 0)
    {
        snapshot->keyframe = acquire_keyframe();

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

    return stored_size + (blob_size * (2 + keyframe_pool_count)) + encode_capacity;
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

    encode_capacity = blob_size + (blob_size / 32) + 65536;
    newest = new (std::nothrow) u8[blob_size];
    work = new (std::nothrow) u8[blob_size];
    encode_buffer = new (std::nothrow) u8[encode_capacity];

    if (!IsValidPointer(newest) || !IsValidPointer(work) || !IsValidPointer(encode_buffer))
    {
        Log("Rewind: failed to allocate %zu byte snapshot buffers", blob_size);
        release_storage();
        return false;
    }

    memset(newest, 0, blob_size);
    memset(work, 0, blob_size);

    Log("Rewind: %.1f MB snapshots, up to %d", (double)blob_size / (1024.0 * 1024.0), capacity);
    return true;
}

static void release_storage(void)
{
    for (int i = 0; i < REWIND_MAX_SNAPSHOTS; i++)
        release_snapshot(&snapshots[i]);

    for (int i = 0; i < keyframe_pool_count; i++)
        SafeDeleteArray(keyframe_pool[i]);

    keyframe_pool_count = 0;
    SafeDeleteArray(newest);
    SafeDeleteArray(work);
    SafeDeleteArray(encode_buffer);
    encode_capacity = 0;
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
    release_keyframe(snapshot->keyframe);
    snapshot->keyframe = NULL;
    snapshot->delta_size = 0;
    snapshot->state_size = 0;
    call_stacks[snapshot - snapshots].clear();
}

static u8* acquire_keyframe(void)
{
    if (keyframe_pool_count > 0)
        return keyframe_pool[--keyframe_pool_count];

    return new (std::nothrow) u8[blob_size];
}

static void release_keyframe(u8* keyframe)
{
    if (!IsValidPointer(keyframe))
        return;

    if (keyframe_pool_count < REWIND_KEYFRAME_POOL)
        keyframe_pool[keyframe_pool_count++] = keyframe;
    else
        SafeDeleteArray(keyframe);
}

static void enforce_limits(void)
{
    while (count > capacity)
        drop_oldest();

    while ((count > 1) && (rewind_get_memory_usage() > REWIND_MAX_MEMORY_SIZE))
    {
        if (keyframe_pool_count > 0)
        {
            keyframe_pool_count--;
            SafeDeleteArray(keyframe_pool[keyframe_pool_count]);
        }
        else
            drop_oldest();
    }
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
    Rewind_Snapshot* snapshot = &snapshots[index];
    bool ok = emu_get_core()->LoadState(blob, snapshot->state_size);

    if (ok)
    {
        size_t screen = MIN((size_t)snapshot->screen_width * snapshot->screen_height * 4, screenshot_size);
        emu_debug_state_restored();
        emu_hard_disk_state_loaded(true);
        emu_memory_card_state_loaded(true);
        emu_get_core()->GetI386()->SetDisassemblerCallStack(call_stacks[index]);
        emu_get_core()->GetVideo()->SetFrameSize(snapshot->screen_width, snapshot->screen_height,
            snapshot->display_width, snapshot->display_height);
        memcpy(emu_frame_buffer, blob + state_capacity, screen);
        events_sync_input();
    }

    return ok;
}

static void apply_delta(u8* target, const u8* delta, u32 size)
{
    const u8* p = delta;
    const u8* end = delta + size;

    while (p < end)
    {
        u32 offset = 0;
        u32 length = 0;
        memcpy(&offset, p, 4);
        memcpy(&length, p + 4, 4);
        p += 8;

        u8* t = target + offset;

        for (u32 i = 0; i < length; i++)
            t[i] ^= p[i];

        p += length;
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

static void delta_begin(bool copy)
{
    delta_position = 0;
    delta_limit = 0;
    delta_size = 0;
    delta_last_header = 0;
    delta_last_end = 0;
    delta_copy = copy;
    delta_failed = false;
}

static void delta_set_region(size_t position, size_t limit)
{
    delta_position = position;
    delta_limit = limit;
}

static bool delta_write(const char* data, size_t size)
{
    const u8* source = (const u8*)data;

    if (delta_failed || ((delta_position + size) > delta_limit))
    {
        delta_failed = true;
        return false;
    }

    if (delta_copy)
    {
        memcpy(newest + delta_position, source, size);
        delta_position += size;
        return true;
    }

    for (size_t i = 0; i < size; i += REWIND_DIFF_CHUNK)
    {
        size_t length = MIN(size - i, (size_t)REWIND_DIFF_CHUNK);
        const u8* reference = newest + delta_position + i;
        const u8* chunk = source + i;

        if (memcmp(reference, chunk, length) == 0)
            continue;

        size_t j = 0;

        while (j < length)
        {
            while (((j + 8) <= length) && (read_word(reference + j) == read_word(chunk + j)))
                j += 8;

            while ((j < length) && (reference[j] == chunk[j]))
                j++;

            if (j == length)
                break;

            size_t first = j++;

            while (((j + 8) <= length) && (read_word(reference + j) != read_word(chunk + j)))
                j += 8;

            if ((j + 8) > length)
                j = length;

            size_t last = j;

            while (reference[last - 1] == chunk[last - 1])
                last--;

            if (!delta_record(delta_position + i + first, chunk + first, last - first))
                return false;
        }
    }

    delta_position += size;
    return true;
}

static bool delta_record(size_t offset, const u8* data, size_t length)
{
    bool merge = (delta_size > 0) && (delta_last_end == offset);
    size_t needed = length + (merge ? 0 : 8);

    if ((delta_size + needed) > encode_capacity)
    {
        delta_failed = true;
        return false;
    }

    if (!merge)
    {
        u32 start = (u32)offset;
        delta_last_header = delta_size;
        memcpy(encode_buffer + delta_size, &start, 4);
        delta_size += 8;
    }

    u8* reference = newest + offset;
    u8* out = encode_buffer + delta_size;

    for (size_t i = 0; i < length; i++)
    {
        out[i] = reference[i] ^ data[i];
        reference[i] = data[i];
    }

    delta_size += length;
    delta_last_end = offset + length;
    u32 record_length = (u32)(delta_size - delta_last_header - 8);
    memcpy(encode_buffer + delta_last_header + 4, &record_length, 4);
    return true;
}

static u64 read_word(const u8* p)
{
    u64 value;
    memcpy(&value, p, 8);
    return value;
}

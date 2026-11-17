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

#include <SDL3/SDL.h>
#include "emu.h"
#include "emu_hard_disk.h"
#include "emu_memory_card.h"
#include "config.h"
#include "events.h"
#include "geartowns.h"
#include "common/memory_stream.h"
#include "system/scheduler.h"
#include "video/video.h"

#define REWIND_IMPORT
#include "rewind.h"

struct Rewind_Snapshot
{
    u8* delta;
    u32 delta_size;
    u8* keyframe;
    u32 keyframe_size;
    u32 state_size;
    u64 clocks;
    int screen_width;
    int screen_height;
    int display_width;
    int display_height;
#if defined(REWIND_CHECK)
    u64 state_hash;
    u64 screen_hash;
#endif
    Video::Video_Frame_Info frame_info;
};

static Rewind_Snapshot* snapshots = NULL;
static std::vector<I386_CallStackEntry>* call_stacks = NULL;
static u8* newest = NULL;
static u8* work = NULL;
static u8* encode_buffer = NULL;
static size_t encode_capacity = 0;
static size_t encode_limit = 0;
static size_t state_capacity = 0;
static size_t screenshot_size = 0;
static size_t screenshot_used = 0;
static size_t blob_size = 0;
static size_t stored_size = 0;
static size_t keyframe_debt = 0;
static size_t storage_failed_budget = 0;
static int slots = 0;
static int head = 0;
static int count = 0;
static int frame_accum = 0;
static int max_memory_mb = 0;
static int required_memory_mb = 0;
static bool active = false;
static int seek_age = -1;
static int work_age = -1;
static u64 pop_target = 0;
static size_t delta_position = 0;
static size_t delta_limit = 0;
static size_t delta_size = 0;
static size_t delta_last_header = 0;
static size_t delta_last_end = 0;
static bool delta_copy = false;
static bool delta_failed = false;
#if defined(REWIND_CHECK)
static u8* check_buffer = NULL;
static int check_passed = 0;
static int check_failed = 0;
#endif

static int slot_at(int age);
static int get_frames_per_snapshot(void);
static size_t get_used_size(void);
static bool ensure_storage(void);
static void release_storage(void);
static bool grow_slots(void);
static bool ensure_work(int age);
static void release_work(void);
static void release_snapshot(int index);
static void enforce_limits(void);
static void drop_oldest(void);
static void drop_newest(void);
static void update_keyframe_debt(void);
static size_t get_range_size(int from, int to);
static void decode_snapshot(int age);
static bool load_snapshot(const u8* blob, int age);
static void apply_delta(u8* target, const u8* delta, u32 size);
static void delta_begin(bool copy);
static void delta_set_region(size_t position, size_t limit);
static bool delta_write(const char* data, size_t size);
static bool delta_record(size_t offset, const u8* data, size_t length);
static bool grow_encode_buffer(size_t needed);
static u64 read_word(const u8* p);
#if defined(REWIND_CHECK)
static u64 check_hash(const u8* data, size_t size);
static void check_blob(const char* what, const u8* blob, int age);
static void check_round_trip(const u8* blob, int age);
#endif

bool rewind_init(void)
{
    rewind_reset();
    return true;
}

void rewind_destroy(void)
{
    release_storage();
    frame_accum = 0;
    active = false;
    seek_age = -1;
}

void rewind_reset(void)
{
    release_storage();
    frame_accum = 0;
    active = false;
    seek_age = -1;
    storage_failed_budget = 0;
    required_memory_mb = 0;

    if (!config_rewind.enabled || emu_is_empty())
        return;

    ensure_storage();
}

void rewind_push(bool frame_completed)
{
    if (!config_rewind.enabled)
        return;

    if (emu_is_empty() || emu_is_paused())
        return;

    if (active)
        return;

    if (!config_debug.debug)
    {
        if (!frame_completed)
            return;

        frame_accum++;

        if (frame_accum < get_frames_per_snapshot())
            return;
    }

    frame_accum = 0;

    if (!IsValidPointer(newest) && !ensure_storage())
        return;

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

    screenshot_used = MAX(screenshot_used, screen);

    if ((count == slots) && !grow_slots())
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
            keyframe_debt += delta_size;
        }
    }

    Rewind_Snapshot* snapshot = &snapshots[head];
    snapshot->state_size = (u32)size;
    snapshot->clocks = emu_get_core()->GetScheduler()->GetClocks();
    snapshot->screen_width = runtime.screen_width;
    snapshot->screen_height = runtime.screen_height;
    snapshot->display_width = runtime.display_width;
    snapshot->display_height = runtime.display_height;
    snapshot->frame_info = emu_get_core()->GetVideo()->GetFrameInfo();

    if (config_debug.debug)
        call_stacks[head] = emu_get_core()->GetI386()->GetDisassemblerCallStack();
    else
        call_stacks[head].clear();

#if defined(REWIND_CHECK)
    snapshot->state_hash = check_hash(newest, size);
    snapshot->screen_hash = check_hash(newest + state_capacity, screen);
#endif

    size_t used = get_used_size();

    if (config_debug.debug && (keyframe_debt >= (used * REWIND_KEYFRAME_STATES)))
    {
        snapshot->keyframe = new (std::nothrow) u8[used];

        if (IsValidPointer(snapshot->keyframe))
        {
            memcpy(snapshot->keyframe, newest, used);
            snapshot->keyframe_size = (u32)used;
            stored_size += used;
            keyframe_debt = 0;
        }
    }

    head = (head + 1) % slots;
    count++;
    enforce_limits();
}

bool rewind_pop(double seconds)
{
    if ((count == 0) || (seconds <= 0.0))
        return false;

    if (!IsValidPointer(newest))
        return false;

    release_work();
    seek_age = -1;

    u64 step = (u64)(seconds * (double)REWIND_CLOCK_RATE);
    pop_target = (step < pop_target) ? (pop_target - step) : 0;

    int pops = 0;

    while ((pops < count) && (snapshots[slot_at(pops)].clocks >= pop_target))
        pops++;

    if (pops == 0)
        return false;

    for (int i = 1; i < pops; i++)
        drop_newest();

#if defined(REWIND_CHECK)
    check_blob("Pop", newest, 0);
#endif

    bool ok = load_snapshot(newest, 0);
    drop_newest();
    update_keyframe_debt();
    return ok;
}

void rewind_commit_seek(void)
{
    if (seek_age > 0)
    {
        if (work_age == seek_age)
        {
            for (int i = 0; i < seek_age; i++)
            {
                release_snapshot(slot_at(0));
                head = (head + slots - 1) % slots;
                count--;
            }

            Rewind_Snapshot* current = &snapshots[slot_at(0)];
            stored_size -= current->delta_size;
            SafeDeleteArray(current->delta);
            current->delta_size = 0;

            u8* previous = newest;
            newest = work;
            work = previous;

#if defined(REWIND_CHECK)
            check_blob("Resume", newest, 0);
#endif
        }
        else
        {
            for (int i = 0; i < seek_age; i++)
                drop_newest();
        }

        update_keyframe_debt();
    }

    release_work();
    seek_age = -1;
}

void rewind_set_active(bool a)
{
    active = a;

    if (a)
        pop_target = emu_is_empty() ? 0 : emu_get_core()->GetScheduler()->GetClocks();
    else
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

int rewind_get_seek_age(void)
{
    return seek_age;
}

size_t rewind_get_memory_usage(void)
{
    if (!IsValidPointer(newest))
        return 0;

    size_t records = (size_t)slots * (sizeof(Rewind_Snapshot) + sizeof(std::vector<I386_CallStackEntry>));
    size_t usage = stored_size + blob_size + encode_capacity + records;

    if (IsValidPointer(work))
        usage += blob_size;

    return usage;
}

size_t rewind_get_memory_budget(void)
{
    int max = rewind_get_max_memory_mb();
    int mb = CLAMP(config_rewind.memory_mb, REWIND_MEMORY_STEP_MB, max);
    mb -= mb % REWIND_MEMORY_STEP_MB;
    return (size_t)mb * 1024 * 1024;
}

int rewind_get_max_memory_mb(void)
{
    if (max_memory_mb == 0)
    {
        int half = SDL_GetSystemRAM() / 2;
        max_memory_mb = MAX(half - (half % REWIND_MEMORY_STEP_MB), REWIND_MEMORY_STEP_MB);
    }

    return max_memory_mb;
}

int rewind_get_required_memory_mb(void)
{
    return required_memory_mb;
}

#if defined(REWIND_CHECK)
void rewind_get_check_counts(int* passed, int* failed)
{
    *passed = check_passed;
    *failed = check_failed;
}
#endif

bool rewind_seek(int age)
{
    if (age < 0 || age >= count)
        return false;

    if (!IsValidPointer(newest))
        return false;

    const u8* blob = newest;

    if (age > 0)
    {
        if (!ensure_work(age))
            return false;

        decode_snapshot(age);
        blob = work;
    }

#if defined(REWIND_CHECK)
    check_blob("Seek", blob, age);
#endif

    bool ok = load_snapshot(blob, age);

    if (ok)
        seek_age = age;

    return ok;
}

int rewind_get_frames_per_snapshot(void)
{
    return get_frames_per_snapshot();
}

double rewind_get_recorded_seconds(void)
{
    return rewind_get_age_seconds(count - 1);
}

double rewind_get_age_seconds(int age)
{
    if ((age <= 0) || (age >= count))
        return 0.0;

    u64 latest = snapshots[slot_at(0)].clocks;
    u64 clocks = snapshots[slot_at(age)].clocks;

    if (latest <= clocks)
        return 0.0;

    return (double)(latest - clocks) / (double)REWIND_CLOCK_RATE;
}

static int slot_at(int age)
{
    int index = head - 1 - age;

    if (index < 0)
        index += slots;

    return index;
}

static int get_frames_per_snapshot(void)
{
    if (config_debug.debug)
        return 1;

    return CLAMP(config_rewind.frames_per_snapshot, 1, REWIND_MAX_FRAMES_PER_SNAPSHOT);
}

static size_t get_used_size(void)
{
    return state_capacity + screenshot_used;
}

static bool ensure_storage(void)
{
    size_t budget = rewind_get_memory_budget();

    if (storage_failed_budget == budget)
        return false;

    size_t state_size = 0;

    if (!emu_get_core()->GetMaxSaveStateSize(state_size) || (state_size == 0))
    {
        storage_failed_budget = budget;
        return false;
    }

    state_capacity = (state_size + 7) & ~(size_t)7;
    screenshot_size = (size_t)REWIND_SCREENSHOT_WIDTH * REWIND_SCREENSHOT_HEIGHT * 4;
    blob_size = state_capacity + screenshot_size;

    if ((blob_size * 2) > budget)
    {
        size_t step = (size_t)REWIND_MEMORY_STEP_MB * 1024 * 1024;
        required_memory_mb = (int)(((blob_size * 2) + step - 1) / step) * REWIND_MEMORY_STEP_MB;
        storage_failed_budget = budget;
        Log("Rewind: %.1f MB snapshots need %d MB of memory", (double)blob_size / (1024.0 * 1024.0),
            required_memory_mb);
        return false;
    }

    required_memory_mb = 0;
    encode_limit = blob_size + (blob_size / 32) + 65536;
    encode_capacity = MIN((size_t)REWIND_ENCODE_INITIAL_SIZE, encode_limit);
    newest = new (std::nothrow) u8[blob_size];
    encode_buffer = new (std::nothrow) u8[encode_capacity];

    if (!IsValidPointer(newest) || !IsValidPointer(encode_buffer) || !grow_slots())
    {
        Log("Rewind: failed to allocate %zu byte snapshot buffers", blob_size);
        release_storage();
        storage_failed_budget = budget;
        return false;
    }

    memset(newest, 0, blob_size);

#if defined(REWIND_CHECK)
    check_buffer = new (std::nothrow) u8[state_capacity];

    if (!IsValidPointer(check_buffer))
    {
        Log("Rewind: failed to allocate the %zu byte check buffer", state_capacity);
        release_storage();
        storage_failed_budget = budget;
        return false;
    }
#endif

    Log("Rewind: %.1f MB snapshots, %d MB of memory", (double)blob_size / (1024.0 * 1024.0),
        (int)(budget / (1024 * 1024)));
    return true;
}

static void release_storage(void)
{
    for (int i = 0; i < slots; i++)
    {
        SafeDeleteArray(snapshots[i].delta);
        SafeDeleteArray(snapshots[i].keyframe);
    }

    SafeDeleteArray(snapshots);
    SafeDeleteArray(call_stacks);
    SafeDeleteArray(newest);
    SafeDeleteArray(work);
    SafeDeleteArray(encode_buffer);
#if defined(REWIND_CHECK)
    SafeDeleteArray(check_buffer);
#endif
    slots = 0;
    encode_capacity = 0;
    encode_limit = 0;
    stored_size = 0;
    keyframe_debt = 0;
    screenshot_used = 0;
    head = 0;
    count = 0;
    work_age = -1;
}

static bool grow_slots(void)
{
    int new_slots = (slots == 0) ? REWIND_INITIAL_SLOTS : (slots * 2);
    Rewind_Snapshot* new_snapshots = new (std::nothrow) Rewind_Snapshot[new_slots];
    std::vector<I386_CallStackEntry>* new_call_stacks = new (std::nothrow) std::vector<I386_CallStackEntry>[new_slots];

    if (!IsValidPointer(new_snapshots) || !IsValidPointer(new_call_stacks))
    {
        Log("Rewind: failed to allocate %d snapshot records", new_slots);
        SafeDeleteArray(new_snapshots);
        SafeDeleteArray(new_call_stacks);
        return false;
    }

    memset(new_snapshots, 0, sizeof(Rewind_Snapshot) * new_slots);

    for (int i = 0; i < count; i++)
    {
        int index = slot_at(count - 1 - i);
        new_snapshots[i] = snapshots[index];
        new_call_stacks[i].swap(call_stacks[index]);
    }

    SafeDeleteArray(snapshots);
    SafeDeleteArray(call_stacks);
    snapshots = new_snapshots;
    call_stacks = new_call_stacks;
    slots = new_slots;
    head = count;
    return true;
}

static bool ensure_work(int age)
{
    if (IsValidPointer(work))
        return true;

    work = new (std::nothrow) u8[blob_size];

    if (!IsValidPointer(work))
    {
        Log("Rewind: failed to allocate a %zu byte seek buffer", blob_size);
        return false;
    }

    size_t used = get_used_size();
    memset(work + used, 0, blob_size - used);
    work_age = -1;

    size_t budget = rewind_get_memory_budget();

    while (((count - 1) > age) && (rewind_get_memory_usage() > budget))
        drop_oldest();

    return true;
}

static void release_work(void)
{
    SafeDeleteArray(work);
    work_age = -1;
}

static void release_snapshot(int index)
{
    Rewind_Snapshot* snapshot = &snapshots[index];
    stored_size -= (size_t)snapshot->delta_size + snapshot->keyframe_size;
    SafeDeleteArray(snapshot->delta);
    SafeDeleteArray(snapshot->keyframe);
    snapshot->delta_size = 0;
    snapshot->keyframe_size = 0;
    snapshot->state_size = 0;
    call_stacks[index].clear();
}

static void enforce_limits(void)
{
    size_t budget = rewind_get_memory_budget();

    while ((count > 1) && (rewind_get_memory_usage() > budget))
        drop_oldest();
}

static void drop_oldest(void)
{
    release_snapshot(slot_at(count - 1));
    count--;

    if (work_age >= count)
        work_age = -1;
}

static void drop_newest(void)
{
    release_snapshot(slot_at(0));
    head = (head + slots - 1) % slots;
    count--;

    if (count == 0)
        return;

    Rewind_Snapshot* previous = &snapshots[slot_at(0)];
    apply_delta(newest, previous->delta, previous->delta_size);
    stored_size -= previous->delta_size;
    SafeDeleteArray(previous->delta);
    previous->delta_size = 0;

#if defined(REWIND_CHECK)
    check_blob("Drop", newest, 0);
#endif
}

static void update_keyframe_debt(void)
{
    keyframe_debt = 0;

    for (int age = 0; age < count; age++)
    {
        Rewind_Snapshot* snapshot = &snapshots[slot_at(age)];
        keyframe_debt += snapshot->delta_size;

        if (IsValidPointer(snapshot->keyframe))
            return;
    }
}

static size_t get_range_size(int from, int to)
{
    int first = MIN(from, to) + 1;
    int last = MAX(from, to);
    size_t size = 0;

    for (int age = first; age <= last; age++)
        size += snapshots[slot_at(age)].delta_size;

    return size;
}

static void decode_snapshot(int age)
{
    size_t used = get_used_size();
    int source = 0;
    bool from_work = false;
    size_t best = used + get_range_size(0, age);

    if (work_age >= 0)
    {
        size_t cost = get_range_size(work_age, age);

        if (cost <= best)
        {
            best = cost;
            source = work_age;
            from_work = true;
        }
    }

    size_t distance = 0;

    for (int k = age; (k > 0) && ((used + distance) < best); k--)
    {
        if (IsValidPointer(snapshots[slot_at(k)].keyframe))
        {
            best = used + distance;
            source = k;
            from_work = false;
            break;
        }

        distance += snapshots[slot_at(k)].delta_size;
    }

    distance = 0;

    for (int k = age + 1; k < count; k++)
    {
        distance += snapshots[slot_at(k)].delta_size;

        if ((used + distance) >= best)
            break;

        if (IsValidPointer(snapshots[slot_at(k)].keyframe))
        {
            best = used + distance;
            source = k;
            from_work = false;
            break;
        }
    }

    if (!from_work)
    {
        if (source == 0)
            memcpy(work, newest, used);
        else
        {
            Rewind_Snapshot* keyframe = &snapshots[slot_at(source)];
            memcpy(work, keyframe->keyframe, keyframe->keyframe_size);

            if (keyframe->keyframe_size < used)
                memset(work + keyframe->keyframe_size, 0, used - keyframe->keyframe_size);
        }
    }

    for (int i = source + 1; i <= age; i++)
    {
        Rewind_Snapshot* snapshot = &snapshots[slot_at(i)];
        apply_delta(work, snapshot->delta, snapshot->delta_size);
    }

    for (int i = source; i > age; i--)
    {
        Rewind_Snapshot* snapshot = &snapshots[slot_at(i)];
        apply_delta(work, snapshot->delta, snapshot->delta_size);
    }

    work_age = age;
}

static bool load_snapshot(const u8* blob, int age)
{
    int index = slot_at(age);
    Rewind_Snapshot* snapshot = &snapshots[index];
    bool ok = emu_get_core()->LoadState(blob, snapshot->state_size);

    if (ok)
    {
#if defined(REWIND_CHECK)
        check_round_trip(blob, age);
#endif

        size_t screen = MIN((size_t)snapshot->screen_width * snapshot->screen_height * 4, screenshot_size);
        emu_debug_state_restored();
        emu_hard_disk_state_loaded(true);
        emu_memory_card_state_loaded(true);
        emu_get_core()->GetI386()->SetDisassemblerCallStack(call_stacks[index]);
        emu_get_core()->GetVideo()->SetFrameSize(snapshot->screen_width, snapshot->screen_height,
            snapshot->display_width, snapshot->display_height);
        emu_get_core()->GetVideo()->SetFrameInfo(snapshot->frame_info);
        memcpy(emu_frame_buffer, blob + state_capacity, screen);
        events_sync_input();
        emu_reset_load_ffwd();
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
    size_t needed = delta_size + length + (merge ? 0 : 8);

    if ((needed > encode_capacity) && !grow_encode_buffer(needed))
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

static bool grow_encode_buffer(size_t needed)
{
    if (needed > encode_limit)
        return false;

    size_t capacity = MIN(MAX(encode_capacity * 2, needed), encode_limit);
    u8* buffer = new (std::nothrow) u8[capacity];

    if (!IsValidPointer(buffer))
    {
        Log("Rewind: failed to allocate a %zu byte delta buffer", capacity);
        return false;
    }

    memcpy(buffer, encode_buffer, delta_size);
    SafeDeleteArray(encode_buffer);
    encode_buffer = buffer;
    encode_capacity = capacity;
    return true;
}

static u64 read_word(const u8* p)
{
    u64 value;
    memcpy(&value, p, 8);
    return value;
}

#if defined(REWIND_CHECK)
static u64 check_hash(const u8* data, size_t size)
{
    u64 hash = 0xCBF29CE484222325ULL;
    size_t i = 0;

    for (; (i + 8) <= size; i += 8)
        hash = (hash ^ read_word(data + i)) * 0x100000001B3ULL;

    for (; i < size; i++)
        hash = (hash ^ data[i]) * 0x100000001B3ULL;

    return hash ^ size;
}

static void check_blob(const char* what, const u8* blob, int age)
{
    Rewind_Snapshot* snapshot = &snapshots[slot_at(age)];
    size_t screen = MIN((size_t)snapshot->screen_width * snapshot->screen_height * 4, screenshot_size);
    bool state = check_hash(blob, snapshot->state_size) == snapshot->state_hash;
    bool picture = check_hash(blob + state_capacity, screen) == snapshot->screen_hash;

    if (state && picture)
    {
        check_passed++;
        return;
    }

    check_failed++;
    Log("Rewind check: %s, snapshot %d of %d (%.4f s old): the %s differs", what, count - age, count,
        rewind_get_age_seconds(age), state ? "screen" : (picture ? "state" : "state and the screen"));
}

static void check_round_trip(const u8* blob, int age)
{
    Rewind_Snapshot* snapshot = &snapshots[slot_at(age)];
    size_t size = state_capacity;

    if (!emu_get_core()->SaveState(check_buffer, size, false, false) || (size != snapshot->state_size))
    {
        check_failed++;
        Log("Rewind check: snapshot %d of %d saves %zu bytes after loading, not %u", count - age, count, size,
            snapshot->state_size);
        return;
    }

    size_t compared = size - MIN(size, (size_t)REWIND_CHECK_HEADER_SIZE);

    if (memcmp(check_buffer, blob, compared) == 0)
    {
        check_passed++;
        return;
    }

    size_t offset = 0;

    while (check_buffer[offset] == blob[offset])
        offset++;

    check_failed++;
    Log("Rewind check: snapshot %d of %d saves differently after loading, first at byte %zu of %zu (%02X, was %02X)",
        count - age, count, offset, size, check_buffer[offset], blob[offset]);
}
#endif

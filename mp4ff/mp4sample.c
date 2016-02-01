#include <stdlib.h>
#include "mp4ffint.h"


//{{{
static int32_t mp4ff_chunk_of_sample(const mp4ff_t *f, const int32_t track, const int32_t sample,
                                     int32_t *chunk_sample, int32_t *chunk)
{
    int32_t total_entries = 0;
    int32_t chunk2entry;
    int32_t chunk1, chunk2, chunk1samples, range_samples, total = 0;

    if (f->track[track] == NULL)
    {
        return -1;
    }

    total_entries = f->track[track]->stsc_entry_count;

    chunk1 = 1;
    chunk1samples = 0;
    chunk2entry = 0;

    do
    {
        chunk2 = f->track[track]->stsc_first_chunk[chunk2entry];
        *chunk = chunk2 - chunk1;
        range_samples = *chunk * chunk1samples;

        if (sample < total + range_samples) break;

        chunk1samples = f->track[track]->stsc_samples_per_chunk[chunk2entry];
        chunk1 = chunk2;

        if(chunk2entry < total_entries)
        {
            chunk2entry++;
            total += range_samples;
        }
    } while (chunk2entry < total_entries);

    if (chunk1samples)
        *chunk = (sample - total) / chunk1samples + chunk1;
    else
        *chunk = 1;

    *chunk_sample = total + (*chunk - chunk1) * chunk1samples;

    return 0;
}
//}}}

//{{{
static int32_t mp4ff_chunk_to_offset(const mp4ff_t *f, const int32_t track, const int32_t chunk)
{
    const mp4ff_track_t * p_track = f->track[track];

    if (p_track->stco_entry_count && (chunk > p_track->stco_entry_count))
    {
        return p_track->stco_chunk_offset[p_track->stco_entry_count - 1];
    } else if (p_track->stco_entry_count) {
        return p_track->stco_chunk_offset[chunk - 1];
    } else {
        return 8;
    }

    return 0;
}
//}}}

//{{{
static int32_t mp4ff_sample_range_size(const mp4ff_t *f, const int32_t track,
                                       const int32_t chunk_sample, const int32_t sample)
{
    int32_t i, total;
    const mp4ff_track_t * p_track = f->track[track];

    if (p_track->stsz_sample_size)
    {
        return (sample - chunk_sample) * p_track->stsz_sample_size;
    }
    else
    {
        if (sample>=p_track->stsz_sample_count) return 0;//error

        for(i = chunk_sample, total = 0; i < sample; i++)
        {
            total += p_track->stsz_table[i];
        }
    }

    return total;
}
//}}}

//{{{
static int32_t mp4ff_sample_to_offset(const mp4ff_t *f, const int32_t track, const int32_t sample)
{
    int32_t chunk, chunk_sample, chunk_offset1, chunk_offset2;

    mp4ff_chunk_of_sample(f, track, sample, &chunk_sample, &chunk);

    chunk_offset1 = mp4ff_chunk_to_offset(f, track, chunk);
    chunk_offset2 = chunk_offset1 + mp4ff_sample_range_size(f, track, chunk_sample, sample);

    return chunk_offset2;
}
//}}}

//{{{
int32_t mp4ff_audio_frame_size(const mp4ff_t *f, const int32_t track, const int32_t sample)
{
    int32_t bytes;
    const mp4ff_track_t * p_track = f->track[track];

    if (p_track->stsz_sample_size)
    {
        bytes = p_track->stsz_sample_size;
    } else {
        bytes = p_track->stsz_table[sample];
    }

    return bytes;
}
//}}}

//{{{
int32_t mp4ff_set_sample_position(mp4ff_t *f, const int32_t track, const int32_t sample)
{
    int32_t offset;

    offset = mp4ff_sample_to_offset(f, track, sample);
    mp4ff_set_position(f, offset);

    return 0;
}
//}}}

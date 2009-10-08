/*
  Copyright (C) 2003-2009 Paul Brossier <piem@aubio.org>

  This file is part of aubio.

  aubio is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  aubio is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with aubio.  If not, see <http://www.gnu.org/licenses/>.

*/

#include "utils.h"

#ifdef HAVE_LASH
#include <lash/lash.h>
#include <pthread.h>
lash_client_t *aubio_lash_client;
lash_args_t *lash_args;
void *lash_thread_main (void *data);
int lash_main (void);
void save_data (void);
void restore_data (lash_config_t * lash_config);
pthread_t lash_thread;
#endif /* HAVE_LASH */

/* settings */
const char *output_filename = NULL;
const char *input_filename = NULL;
const char *onset_filename =
    AUBIO_PREFIX "/share/sounds/" PACKAGE "/woodblock.aiff";
int frames = 0;
int verbose = 0;
int usejack = 0;
int usedoubled = 1;
int frames_delay = 0;


/* energy,specdiff,hfc,complexdomain,phase */
aubio_onsetdetection_type type_onset = aubio_onset_kl;
aubio_onsetdetection_type type_onset2 = aubio_onset_complex;
smpl_t threshold = 0.3;
smpl_t silence = -90.;
uint_t buffer_size = 512;       //1024;
uint_t overlap_size = 256;      //512;
uint_t channels = 1;
uint_t samplerate = 44100;


aubio_sndfile_t *file = NULL;
aubio_sndfile_t *fileout = NULL;

aubio_pvoc_t *pv;
fvec_t *ibuf;
fvec_t *obuf;
fvec_t *pitch_obuf;
cvec_t *fftgrain;
fvec_t *woodblock;
aubio_onsetdetection_t *o;
aubio_onsetdetection_t *o2;
fvec_t *onset;
fvec_t *onset2;
smpl_t isonset = 0;
aubio_peakpicker_t *parms;


/* pitch objects */
smpl_t pitch = 0.;
aubio_pitchdetection_t *pitchdet;
aubio_pitchdetection_type type_pitch = aubio_pitch_yinfft;      // aubio_pitch_mcomb
aubio_pitchdetection_mode mode_pitch = aubio_pitchm_freq;
uint_t median = 6;

fvec_t *note_buffer = NULL;
fvec_t *note_buffer2 = NULL;
smpl_t curlevel = 0.;
smpl_t maxonset = 0.;

smpl_t curnote = 0.;
smpl_t newnote = 0.;
uint_t isready = 0;



/* badly redeclare some things */
aubio_onsetdetection_type type_onset;
smpl_t threshold;
smpl_t averaging;
const char *prog_name;

void
usage (FILE * stream, int exit_code)
{
  fprintf (stream, "usage: %s [ options ] \n", prog_name);
  fprintf (stream,
      "       -h      --help          Display this message.\n"
      "       -v      --verbose       Be verbose.\n"
      "       -j      --jack          Use Jack.\n"
      "       -o      --output        Output type.\n"
      "       -i      --input         Input type.\n"
      "       -O      --onset         Select onset detection algorithm.\n"
      "       -t      --threshold     Set onset detection threshold.\n"
      "       -s      --silence       Select silence threshold.\n"
      "       -p      --pitch         Select pitch detection algorithm.\n"
      "       -B      --bufsize       Set buffer size.\n"
      "       -H      --hopsize       Set hopsize.\n"
      "       -a      --averaging     Use averaging.\n");
  exit (exit_code);
}

int
parse_args (int argc, char **argv)
{
  const char *options = "hvjo:i:O:t:s:p:B:H:a";
  int next_option;
  struct option long_options[] = {
    {"help", 0, NULL, 'h'},
    {"verbose", 0, NULL, 'v'},
    {"jack", 0, NULL, 'j'},
    {"output", 1, NULL, 'o'},
    {"input", 1, NULL, 'i'},
    {"onset", 1, NULL, 'O'},
    {"threshold", 1, NULL, 't'},
    {"silence", 1, NULL, 's'},
    {"pitch", 1, NULL, 'p'},
    {"averaging", 0, NULL, 'a'},
    {"bufsize", 1, NULL, 'B'},
    {"hopsize", 1, NULL, 'H'},
    {NULL, 0, NULL, 0}
  };
#ifdef HAVE_LASH
  lash_args = lash_extract_args (&argc, &argv);
#endif /* HAVE_LASH */
  prog_name = argv[0];
  if (argc < 1) {
    usage (stderr, 1);
    return -1;
  }
  do {
    next_option = getopt_long (argc, argv, options, long_options, NULL);
    switch (next_option) {
      case 'o':
        output_filename = optarg;
        break;
      case 'i':
        input_filename = optarg;
        break;
      case 'h':                /* help */
        usage (stdout, 0);
        return -1;
      case 'v':                /* verbose */
        verbose = 1;
        break;
      case 'j':
        usejack = 1;
        break;
      case 'O':                /*onset type */
        if (strcmp (optarg, "energy") == 0)
          type_onset = aubio_onset_energy;
        else if (strcmp (optarg, "specdiff") == 0)
          type_onset = aubio_onset_specdiff;
        else if (strcmp (optarg, "hfc") == 0)
          type_onset = aubio_onset_hfc;
        else if (strcmp (optarg, "complexdomain") == 0)
          type_onset = aubio_onset_complex;
        else if (strcmp (optarg, "complex") == 0)
          type_onset = aubio_onset_complex;
        else if (strcmp (optarg, "phase") == 0)
          type_onset = aubio_onset_phase;
        else if (strcmp (optarg, "mkl") == 0)
          type_onset = aubio_onset_mkl;
        else if (strcmp (optarg, "kl") == 0)
          type_onset = aubio_onset_kl;
        else if (strcmp (optarg, "specflux") == 0)
          type_onset = aubio_onset_specflux;
        else {
          errmsg ("unknown onset type.\n");
          abort ();
        }
        usedoubled = 0;
        break;
      case 's':                /* threshold value for onset */
        silence = (smpl_t) atof (optarg);
        break;
      case 't':                /* threshold value for onset */
        threshold = (smpl_t) atof (optarg);
        /*
           if (!isfinite(threshold)) {
           debug("could not get threshold.\n");
           abort();
           }
         */
        break;
      case 'p':
        if (strcmp (optarg, "mcomb") == 0)
          type_pitch = aubio_pitch_mcomb;
        else if (strcmp (optarg, "yinfft") == 0)
          type_pitch = aubio_pitch_yin;
        else if (strcmp (optarg, "yin") == 0)
          type_pitch = aubio_pitch_yin;
        else if (strcmp (optarg, "schmitt") == 0)
          type_pitch = aubio_pitch_schmitt;
        else if (strcmp (optarg, "fcomb") == 0)
          type_pitch = aubio_pitch_fcomb;
        else {
          errmsg ("unknown pitch type.\n");
          abort ();
        }
        break;
      case 'a':
        averaging = 1;
        break;
      case 'B':
        buffer_size = atoi (optarg);
        break;
      case 'H':
        overlap_size = atoi (optarg);
        break;
      case '?':                /* unknown options */
        usage (stderr, 1);
        break;
      case -1:                 /* done with options */
        break;
      default:                 /*something else unexpected */
        fprintf (stderr, "Error parsing option '%c'\n", next_option);
        abort ();
    }
  }
  while (next_option != -1);

  if (input_filename != NULL) {
    debug ("Input file : %s\n", input_filename);
  } else if (input_filename != NULL && output_filename != NULL) {
    debug ("Input file : %s\n", input_filename);
    debug ("Output file : %s\n", output_filename);
  } else {
#if HAVE_JACK
    debug ("Jack input output\n");
    usejack = 1;
#else
    debug
        ("Error: Could not switch to jack mode\n   aubio was compiled without jack support\n");
    exit (1);
#endif
  }

  return 0;
}

void
examples_common_init (int argc, char **argv)
{


  aubio_sndfile_t *onsetfile = NULL;
  /* parse command line arguments */
  parse_args (argc, argv);

  woodblock = new_fvec (buffer_size, 1);
  if (output_filename || usejack) {
    /* dummy assignement to keep egcs happy */
    isonset = (onsetfile = new_aubio_sndfile_ro (onset_filename)) ||
        (onsetfile = new_aubio_sndfile_ro ("sounds/woodblock.aiff")) ||
        (onsetfile = new_aubio_sndfile_ro ("../sounds/woodblock.aiff"));
    if (onsetfile == NULL) {
      outmsg ("Could not find woodblock.aiff\n");
      exit (1);
    }
  }
  if (onsetfile) {
    /* read the output sound once */
    aubio_sndfile_read (onsetfile, overlap_size, woodblock);
  }

  if (!usejack) {
    debug ("Opening files ...\n");
    file = new_aubio_sndfile_ro (input_filename);
    if (file == NULL) {
      outmsg ("Could not open input file %s.\n", input_filename);
      exit (1);
    }
    if (verbose)
      aubio_sndfile_info (file);
    channels = aubio_sndfile_channels (file);
    samplerate = aubio_sndfile_samplerate (file);
    if (output_filename != NULL)
      fileout = new_aubio_sndfile_wo (file, output_filename);
  }
#ifdef HAVE_LASH
  else {
    aubio_lash_client = lash_init (lash_args, argv[0],
        LASH_Config_Data_Set | LASH_Terminal, LASH_PROTOCOL (2, 0));
    if (!aubio_lash_client) {
      fprintf (stderr, "%s: could not initialise lash\n", __FUNCTION__);
    }
    /* tell the lash server our client id */
    if (lash_enabled (aubio_lash_client)) {
      lash_event_t *event =
          (lash_event_t *) lash_event_new_with_type (LASH_Client_Name);
      lash_event_set_string (event, "aubio");
      lash_send_event (aubio_lash_client, event);
      pthread_create (&lash_thread, NULL, lash_thread_main, NULL);
    }
  }
#endif /* HAVE_LASH */

  ibuf = new_fvec (overlap_size, channels);
  obuf = new_fvec (overlap_size, channels);
  fftgrain = new_cvec (buffer_size, channels);

  if (usepitch) {
    pitchdet = new_aubio_pitchdetection (buffer_size * 4,
        overlap_size, channels, samplerate, type_pitch, mode_pitch);
    aubio_pitchdetection_set_tolerance (pitchdet, 0.7);
    pitch_obuf = new_fvec (1, channels);

    if (median) {
      note_buffer = new_fvec (median, 1);
      note_buffer2 = new_fvec (median, 1);
    }
  }
  /* phase vocoder */
  pv = new_aubio_pvoc (buffer_size, overlap_size, channels);
  /* onsets */
  parms = new_aubio_peakpicker (threshold);
  o = new_aubio_onsetdetection (type_onset, buffer_size, channels);
  onset = new_fvec (1, channels);
  if (usedoubled) {
    o2 = new_aubio_onsetdetection (type_onset2, buffer_size, channels);
    onset2 = new_fvec (1, channels);
  }

}


void
examples_common_del (void)
{
  if (usepitch) {
    send_noteon (curnote, 0);
    del_aubio_pitchdetection (pitchdet);
    if (median) {
      del_fvec (note_buffer);
      del_fvec (note_buffer2);
    }
    del_fvec (pitch_obuf);
  }
  if (usedoubled) {
    del_aubio_onsetdetection (o2);
    del_fvec (onset2);
  }
  del_aubio_onsetdetection (o);
  del_aubio_peakpicker (parms);
  del_aubio_pvoc (pv);
  del_fvec (obuf);
  del_fvec (ibuf);
  del_cvec (fftgrain);
  del_fvec (onset);
  del_fvec (woodblock);
  aubio_cleanup ();
}

#if HAVE_JACK
aubio_jack_t *jack_setup;
#endif

void
examples_common_process (aubio_process_func_t process_func,
    aubio_print_func_t print)
{
  if (usejack) {

#if HAVE_JACK
    debug ("Jack init ...\n");
    jack_setup = new_aubio_jack (channels, channels,
        0, 1, (aubio_process_func_t) process_func);
    debug ("Jack activation ...\n");
    aubio_jack_activate (jack_setup);
    debug ("Processing (Ctrl+C to quit) ...\n");
    pause ();
    aubio_jack_close (jack_setup);
#else
    usage (stderr, 1);
    outmsg ("Compiled without jack output, exiting.\n");
#endif

  } else {
    /* phasevoc */
    debug ("Processing ...\n");

    frames = 0;

    while ((signed) overlap_size == aubio_sndfile_read (file, overlap_size,
            ibuf)) {
      isonset = 0;
      process_func (ibuf->data, obuf->data, overlap_size);
      print ();
      if (output_filename != NULL) {
        aubio_sndfile_write (fileout, overlap_size, obuf);
      }
      frames++;
    }

    debug ("Processed %d frames of %d samples.\n", frames, buffer_size);

    flush_process (process_func, print);
    del_aubio_sndfile (file);

    if (output_filename != NULL)
      del_aubio_sndfile (fileout);

  }
}

void
flush_process (aubio_process_func_t process_func, aubio_print_func_t print)
{
  uint_t i;
  fvec_zeros(obuf);
  for (i = 0; (signed) i < frames_delay; i++) {
    process_func (ibuf->data, obuf->data, overlap_size);
    print ();
  }
}

void
send_noteon (int pitch, int velo)
{
  smpl_t mpitch = floor (aubio_freqtomidi (pitch) + .5);
#if HAVE_JACK
  jack_midi_event_t ev;
  ev.size = 3;
  ev.buffer = malloc (3 * sizeof (jack_midi_data_t)); // FIXME
  ev.time = 0;
  if (usejack) {
    ev.buffer[2] = velo;
    ev.buffer[1] = mpitch;
    if (velo == 0) {
      ev.buffer[0] = 0x80;      /* note off */
    } else {
      ev.buffer[0] = 0x90;      /* note on */
    }
    aubio_jack_midi_event_write (jack_setup, (jack_midi_event_t *) & ev);
  } else
#endif
  if (!verbose) {
    if (velo == 0) {
      outmsg ("%f\n", frames * overlap_size / (float) samplerate);
    } else {
      outmsg ("%f\t%f\t", mpitch, frames * overlap_size / (float) samplerate);
    }
  }
}


void
note_append (fvec_t * note_buffer, smpl_t curnote)
{
  uint_t i = 0;
  for (i = 0; i < note_buffer->length - 1; i++) {
    note_buffer->data[0][i] = note_buffer->data[0][i + 1];
  }
  note_buffer->data[0][note_buffer->length - 1] = curnote;
  return;
}

uint_t
get_note (fvec_t * note_buffer, fvec_t * note_buffer2)
{
  uint_t i = 0;
  for (i = 0; i < note_buffer->length; i++) {
    note_buffer2->data[0][i] = note_buffer->data[0][i];
  }
  return fvec_median (note_buffer2);
}

#if HAVE_LASH

void *
lash_thread_main (void *data __attribute__ ((unused)))
{
  printf ("LASH thread running\n");

  while (!lash_main ())
    usleep (1000);

  printf ("LASH thread finished\n");
  return NULL;
}

int
lash_main (void)
{
  lash_event_t *lash_event;
  lash_config_t *lash_config;

  while ((lash_event = lash_get_event (aubio_lash_client))) {
    switch (lash_event_get_type (lash_event)) {
      case LASH_Quit:
        lash_event_destroy (lash_event);
        exit (1);
        return 1;
      case LASH_Restore_Data_Set:
        lash_send_event (aubio_lash_client, lash_event);
        break;
      case LASH_Save_Data_Set:
        save_data ();
        lash_send_event (aubio_lash_client, lash_event);
        break;
      case LASH_Server_Lost:
        return 1;
      default:
        printf ("%s: received unknown LASH event of type %d",
            __FUNCTION__, lash_event_get_type (lash_event));
        lash_event_destroy (lash_event);
        break;
    }
  }

  while ((lash_config = lash_get_config (aubio_lash_client))) {
    restore_data (lash_config);
    lash_config_destroy (lash_config);
  }

  return 0;
}

void
save_data ()
{
  lash_config_t *lash_config;

  lash_config = lash_config_new_with_key ("threshold");
  lash_config_set_value_double (lash_config, threshold);
  lash_send_config (aubio_lash_client, lash_config);

}

void
restore_data (lash_config_t * lash_config)
{
  const char *lash_key;

  lash_key = lash_config_get_key (lash_config);

  if (strcmp (lash_key, "threshold") == 0) {
    threshold = lash_config_get_value_double (lash_config);
    return;
  }

}

#endif /* HAVE_LASH */

#include <stdio.h>
#include <time.h>
#include <cassert>
#include <math.h>
#include <stdlib.h>
#include <popt.h>
#include <sys/stat.h>
#include "handymath.h"
#include "vector3d.h"
#include "Monte-Carlo/square-well.h"

// ------------------------------------------------------------------------------
// Notes on conventions and definitions used
// ------------------------------------------------------------------------------
//
// All coordinates are cartesion.
//
// The coordinates x, y, z are always floating point numbers that refer to real
// locations within the cell.
//
// The coordinates x_i, y_i, z_i are always integers that refer to the bin number
// of the respective coordinate such that x_i refers to a bin of thickness dx
// centered at x.
//
// The symbols e, e_i, and de are used as general coordinates.
//
// Def: If two objects, a and b, are closer than a.R + b.R + neighbor_R + dn,
// then they are neighbors.
//
// Neighbors are used to drastically reduce the number of collision tests needed.
//
// Def: The neighborsphere of an object, a, is the sphere within which
// everything is a neighbor of a.
// Note that this sphere has a well defined center, but it does not have
// a well defined radius unless all obects are circumscribed by spheres of
// the same radius, but this does not affect anything.


// ------------------------------------------------------------------------------
// Global Constants
// ------------------------------------------------------------------------------

const int x = 0;
const int y = 1;
const int z = 2;

// ------------------------------------------------------------------------------
// Functions
// ------------------------------------------------------------------------------

// States how long it's been since last took call.
static void took(const char *name);

// Saves the locations of all balls to a file.
inline void save_locations(const ball *p, int N, const char *fname,
                           const double len[3], const char *comment="");

// The following functions only do anything if debug is true:

// Prints the location and radius of every ball
// As well as whether any overlap or are outside the cell.
inline void print_all(const ball *p, int N, double len[3]);

// Same as print_all, but only prints information for one ball,
// and does not do overlap tests.
inline void print_one(const ball &a, int id, const ball *p, int N,
                      double len[3], int walls);

// Only print those balls that overlap or are outside the cell
// Also prints those they overlap with
inline void print_bad(const ball *p, int N, double len[3], int walls);

// Checks to make sure that every ball is his neighbor's neighbor.
inline void check_neighbor_symmetry(const ball *p, int N);

int main(int argc, const char *argv[]) {
  took("Starting program");
  // ----------------------------------------------------------------------------
  // Define "Constants" -- set from arguments then unchanged
  // ----------------------------------------------------------------------------

  // NOTE: debug can slow things down VERY much
  int debug = false;

  int no_weights = false;
  double fix_kT = 0;
  int flat_histogram = false;
  int gaussian_fit = false;
  int walker_weights = false;
  int wang_landau = false;

  double wl_factor = 0.125;
  double wl_fmod = 2;
  double wl_threshold = 3;
  double wl_cutoff = 1e-6;
  double gaussian_cutoff = 0.25;

  sw_simulation sw;

  sw.len[0] = sw.len[1] = sw.len[2] = 1;
  sw.walls = 0;
  sw.N = 1000;
  sw.dr = 0.1;

  int wall_dim = 1;
  unsigned long int seed = 0;

  char *data_dir = new char[1024];
  sprintf(data_dir, "papers/square-well-liquid/data");
  char *filename = new char[1024];
  sprintf(filename, "default_filename");
  char *filename_suffix = new char[1024];
  sprintf(filename_suffix, "default_filename_suffix");
  long simulation_iterations = 2500000;
  long initialization_iterations = 500000;
  double acceptance_goal = .4;
  double R = 1;
  double well_width = 1.3;
  double ff = 0.0;
  double neighbor_scale = 2;
  double de_density = 0.1;
  double de_g = 0.05;
  double max_rdf_radius = 10;
  int totime = 0;
  // scale is not universally constant -- it is adjusted during initialization
  //  so that we have a reasonable acceptance rate
  double translation_scale = 0.05;

  poptContext optCon;
  // ----------------------------------------------------------------------------
  // Set values from parameters
  // ----------------------------------------------------------------------------
  poptOption optionsTable[] = {
    {"N", '\0', POPT_ARG_INT, &sw.N, 0, "Number of balls to simulate", "INT"},
    {"ww", '\0', POPT_ARG_DOUBLE | POPT_ARGFLAG_SHOW_DEFAULT, &well_width, 0,
     "Ratio of square well width to ball diameter", "DOUBLE"},
    {"ff", '\0', POPT_ARG_DOUBLE, &ff, 0, "Filling fraction. If specified, the "
     "cell dimensions are adjusted accordingly without changing the shape of "
     "the cell"},
    {"walls", '\0', POPT_ARG_INT | POPT_ARGFLAG_SHOW_DEFAULT, &sw.walls, 0,
     "Number of walled dimensions (dimension order: x,y,z)", "INT"},
    {"initialize", '\0', POPT_ARG_LONG | POPT_ARGFLAG_SHOW_DEFAULT,
     &initialization_iterations, 0,
     "Number of iterations to run for initialization", "INT"},
    {"iterations", '\0', POPT_ARG_LONG | POPT_ARGFLAG_SHOW_DEFAULT, &simulation_iterations,
     0, "Number of iterations for which to run the simulation", "INT"},
    {"de_g", '\0', POPT_ARG_DOUBLE | POPT_ARGFLAG_SHOW_DEFAULT, &de_g, 0,
     "Resolution of distribution functions", "DOUBLE"},
    {"dr", '\0', POPT_ARG_DOUBLE | POPT_ARGFLAG_SHOW_DEFAULT, &sw.dr, 0,
     "Differential radius change used in pressure calculation", "DOUBLE"},
    {"de_density", '\0', POPT_ARG_DOUBLE | POPT_ARGFLAG_SHOW_DEFAULT,
     &de_density, 0, "Resolution of density file", "DOUBLE"},
    {"max_rdf_radius", '\0', POPT_ARG_DOUBLE | POPT_ARGFLAG_SHOW_DEFAULT,
     &max_rdf_radius, 0, "Set maximum radius for RDF data collection", "DOUBLE"},
    {"lenx", '\0', POPT_ARG_DOUBLE, &sw.len[x], 0,
     "Relative cell size in x dimension", "DOUBLE"},
    {"leny", '\0', POPT_ARG_DOUBLE, &sw.len[y], 0,
     "Relative cell size in y dimension", "DOUBLE"},
    {"lenz", '\0', POPT_ARG_DOUBLE, &sw.len[z], 0,
     "Relative cell size in z dimension", "DOUBLE"},
    {"filename", '\0', POPT_ARG_STRING | POPT_ARGFLAG_SHOW_DEFAULT, &filename, 0,
     "Base of output file names", "STRING"},
    {"filename_suffix", '\0', POPT_ARG_STRING | POPT_ARGFLAG_SHOW_DEFAULT,
     &filename_suffix, 0, "Output file name suffix", "STRING"},
    {"data_dir", '\0', POPT_ARG_STRING | POPT_ARGFLAG_SHOW_DEFAULT, &data_dir, 0,
     "Directory in which to save data", "data_dir"},
    {"neighbor_scale", '\0', POPT_ARG_DOUBLE | POPT_ARGFLAG_SHOW_DEFAULT,
     &neighbor_scale, 0, "Ratio of neighbor sphere radius to interaction scale "
     "times ball radius. Drastically reduces collision detections","DOUBLE"},
    {"translation_scale", '\0', POPT_ARG_DOUBLE | POPT_ARGFLAG_SHOW_DEFAULT,
     &translation_scale, 0, "Standard deviation for translations of balls, "
     "relative to ball radius", "DOUBLE"},
    {"seed", '\0', POPT_ARG_INT | POPT_ARGFLAG_SHOW_DEFAULT, &seed, 0,
     "Seed for the random number generator", "INT"},
    {"acceptance_goal", '\0', POPT_ARG_DOUBLE | POPT_ARGFLAG_SHOW_DEFAULT,
     &acceptance_goal, 0, "Goal to set the acceptance rate", "DOUBLE"},
    {"nw", '\0', POPT_ARG_NONE, &no_weights, 0, "Don't use weighing method "
     "to get better statistics on low entropy states", "BOOLEAN"},
    {"kT", '\0', POPT_ARG_DOUBLE, &fix_kT, 0, "Use a fixed temperature of kT"
     " rather than adjusted weights", "DOUBLE"},
    {"flat", '\0', POPT_ARG_NONE, &flat_histogram, 0,
     "Use a flat histogram method", "BOOLEAN"},
    {"gaussian", '\0', POPT_ARG_NONE, &gaussian_fit, 0,
     "Use gaussian weights for flat histogram", "BOOLEAN"},
    {"walkers", '\0', POPT_ARG_NONE, &walker_weights, 0,
     "Use a walker optimization weight histogram method", "BOOLEAN"},
    {"wang_landau", '\0', POPT_ARG_NONE, &wang_landau, 0,
     "Use Wang-Landau histogram method", "BOOLEAN"},
    {"wl_factor", '\0', POPT_ARG_DOUBLE | POPT_ARGFLAG_SHOW_DEFAULT, &wl_factor,
     0, "Initial value of Wang-Landau factor", "DOUBLE"},
    {"wl_fmod", '\0', POPT_ARG_DOUBLE | POPT_ARGFLAG_SHOW_DEFAULT, &wl_fmod, 0,
     "Wang-Landau factor modifiction parameter", "DOUBLE"},
    {"wl_threshold", '\0', POPT_ARG_DOUBLE | POPT_ARGFLAG_SHOW_DEFAULT,
     &wl_threshold, 0, "Threhold for normalized standard deviation in "
     "energy histogram at which to adjust Wang-Landau factor", "DOUBLE"},
    {"wl_cutoff", '\0', POPT_ARG_DOUBLE | POPT_ARGFLAG_SHOW_DEFAULT,
     &wl_cutoff, 0, "Cutoff for Wang-Landau factor", "DOUBLE"},
    {"time", '\0', POPT_ARG_INT, &totime, 0,
     "Timing of display information (seconds)", "INT"},
    {"R", '\0', POPT_ARG_DOUBLE | POPT_ARGFLAG_SHOW_DEFAULT,
     &R, 0, "Ball radius (for testing purposes; should always be 1)", "DOUBLE"},
    {"debug", '\0', POPT_ARG_NONE, &debug, 0, "Debug mode", "BOOLEAN"},
    POPT_AUTOHELP
    POPT_TABLEEND
  };
  optCon = poptGetContext(NULL, argc, argv, optionsTable, 0);
  poptSetOtherOptionHelp(optCon, "[OPTION...]\nNumber of balls and filling "
                         "fraction or cell dimensions are required arguments.");

  int c = 0;
  // go through arguments, set them based on optionsTable
  while((c = poptGetNextOpt(optCon)) >= 0);
  if (c < -1) {
    fprintf(stderr, "\n%s: %s\n", poptBadOption(optCon, 0), poptStrerror(c));
    return 1;
  }
  poptFreeContext(optCon);

  // ----------------------------------------------------------------------------
  // Verify we have reasonable arguments and set secondary parameters
  // ----------------------------------------------------------------------------

  // check that only one method is used
  if(bool(no_weights) + bool(flat_histogram) + bool(gaussian_fit)
     + bool(wang_landau) + bool(walker_weights) + (fix_kT != 0) != 1){
    printf("Exactly one histigram method must be selected!");
    return 254;
  }

  if(sw.walls >= 2){
    printf("Code cannot currently handle walls in more than one dimension.\n");
    return 254;
  }
  if(sw.walls > 3){
    printf("You cannot have walls in more than three dimensions.\n");
    return 254;
  }
  if(well_width < 1){
    printf("Interaction scale should be greater than (or equal to) 1.\n");
    return 254;
  }


  if (ff != 0) {
    // The user specified a filling fraction, so we must make it so!
    const double volume = 4*M_PI/3*R*R*R*sw.N/ff;
    const double min_cell_width = 2*sqrt(2)*R; // minimum cell width
    const int numcells = (sw.N+3)/4; // number of unit cells we need
    const int max_cubic_width = pow(volume/min_cell_width/min_cell_width/min_cell_width, 1.0/3);
    if (max_cubic_width*max_cubic_width*max_cubic_width > numcells) {
      // We can get away with a cubic cell, so let's do so.  Cubic
      // cells are nice and comfortable!
      sw.len[x] = sw.len[y] = sw.len[z] = pow(volume, 1.0/3);
    } else {
      // A cubic cell won't work with our initialization routine, so
      // let's go with a lopsided cell that should give us something
      // that will work.
      int xcells = int( pow(numcells, 1.0/3) );
      int cellsleft = (numcells + xcells - 1)/xcells;
      int ycells = int( sqrt(cellsleft) );
      int zcells = (cellsleft + ycells - 1)/ycells;

      // The above should give a zcells that is largest, followed by
      // ycells and then xcells.  Thus we make the lenz just small
      // enough to fit these cells, and so on, to make the cell as
      // close to cubic as possible.
      sw.len[z] = zcells*min_cell_width;
      if (xcells == ycells) {
        sw.len[x] = sw.len[y] = sqrt(volume/sw.len[z]);
      } else {
        sw.len[y] = min_cell_width*ycells;
        sw.len[x] = volume/sw.len[y]/sw.len[z];
      }
      printf("Using lopsided %d x %d x %d cell (total goal %d)\n", xcells, ycells, zcells, numcells);
    }
  }

  printf("\nSetting cell dimensions to (%g, %g, %g).\n",
         sw.len[x], sw.len[y], sw.len[z]);
  if (sw.N <= 0 || initialization_iterations < 0 || simulation_iterations < 0 || R <= 0 ||
      neighbor_scale <= 0 || sw.dr <= 0 || translation_scale < 0 ||
      sw.len[x] < 0 || sw.len[y] < 0 || sw.len[z] < 0) {
    fprintf(stderr, "\nAll parameters must be positive.\n");
    return 1;
  }
  sw.dr *= R;

  const double eta = (double)sw.N*4.0/3.0*M_PI*R*R*R/(sw.len[x]*sw.len[y]*sw.len[z]);
  if (eta > 1) {
    fprintf(stderr, "\nYou're trying to cram too many balls into the cell. "
            "They will never fit. Filling fraction: %g\n", eta);
    return 7;
  }

  // If a filename was not selected, make a default
  if (strcmp(filename, "default_filename") == 0) {
    char *method_tag = new char[20];
    char *wall_tag = new char[10];
    if(sw.walls == 0) sprintf(wall_tag,"periodic");
    else if(sw.walls == 1) sprintf(wall_tag,"wall");
    else if(sw.walls == 2) sprintf(wall_tag,"tube");
    else if(sw.walls == 3) sprintf(wall_tag,"box");
    if (fix_kT) {
      sprintf(method_tag, "-kT%g", fix_kT);
    } else if (no_weights) {
      sprintf(method_tag, "-nw");
    } else if (flat_histogram) {
      sprintf(method_tag, "-flat");
    } else if (gaussian_fit) {
      sprintf(method_tag, "-gaussian");
    } else if (wang_landau) {
      sprintf(method_tag, "-wang_landau");
    } else if (walker_weights) {
      sprintf(method_tag, "-walkers");
    } else {
      method_tag[0] = 0; // set method_tag to the empty string
    }
    sprintf(filename, "%s-ww%04.2f-ff%04.2f-N%i%s",
            wall_tag, well_width, eta, sw.N, method_tag);
    printf("\nUsing default file name: ");
    delete[] method_tag;
    delete[] wall_tag;
  }
  else
    printf("\nUsing given file name: ");
  // If a filename suffix was specified, add it
  if (strcmp(filename_suffix, "default_filename_suffix") != 0)
    sprintf(filename, "%s-%s", filename, filename_suffix);
  printf("%s\n",filename);

  printf("------------------------------------------------------------------\n");
  printf("Running %s with parameters:\n", argv[0]);
  for(int i = 1; i < argc; i++) {
    if(argv[i][0] == '-') printf("\n");
    printf("%s ", argv[i]);
  }
  printf("\n");
  if (totime > 0) printf("Timing information will be displayed.\n");
  if (debug) printf("DEBUG MODE IS ENABLED!\n");
  else printf("Debug mode disabled\n");
  printf("------------------------------------------------------------------\n\n");

  // ----------------------------------------------------------------------------
  // Define sw_simulation variables
  // ----------------------------------------------------------------------------

  sw.iteration = 0; // start at zeroeth iteration
  sw.state_of_max_entropy = 0;
  sw.max_observed_interactions = 0;

  // translation distance should scale with ball radius
  sw.translation_distance = translation_scale*R;

  // neighbor radius should scale with radius and interaction scale
  sw.neighbor_R = neighbor_scale*R*well_width;

  // Find the upper limit to the maximum number of neighbors a ball could have
  sw.max_neighbors = max_balls_within(2+neighbor_scale*well_width);

  // Energy histogram
  sw.interaction_distance = 2*R*well_width;
  sw.energy_levels = sw.N/2*max_balls_within(sw.interaction_distance);
  sw.energy_histogram = new long[sw.energy_levels]();

  sw.seeking_energy = new bool[sw.energy_levels]();
  sw.round_trips = new long[sw.energy_levels]();

  // Walkers
  sw.walkers_up = new long[sw.energy_levels]();
  sw.walkers_total = new long[sw.energy_levels]();

  // Energy weights, state density
  int weight_updates = 0;
  sw.ln_energy_weights = new double[sw.energy_levels]();

  // Radial distribution function (RDF) histogram
  long *g_energy_histogram = new long[sw.energy_levels]();
  const int g_bins = round(min(min(min(sw.len[y],sw.len[z]),sw.len[x]),max_rdf_radius)
                           / de_g / 2);
  long **g_histogram = new long*[sw.energy_levels];
  for(int i = 0; i < sw.energy_levels; i++)
    g_histogram[i] = new long[g_bins]();

  // Density histogram
  const int density_bins = round(sw.len[wall_dim]/de_density);
  const double bin_volume = sw.len[x]*sw.len[y]*sw.len[z]/sw.len[wall_dim]*de_density;
  long **density_histogram = new long*[sw.energy_levels];
  for(int i = 0; i < sw.energy_levels; i++)
    density_histogram[i] = new long[density_bins]();

  printf("memory use estimate = %.2g G\n\n",
         8*double((6 + g_bins + density_bins)*sw.energy_levels)/1024/1024/1024);

  sw.balls = new ball[sw.N];

  if(totime < 0) totime = 10*sw.N;

  // a guess for the number of iterations for which to run histogram initialization
  int first_weight_update = sw.energy_levels;

  // Initialize the random number generator with our seed
  random::seed(seed);

  // ----------------------------------------------------------------------------
  // Set up the initial grid of balls
  // ----------------------------------------------------------------------------

  for(int i = 0; i < sw.N; i++) // initialize ball radii
    sw.balls[i].R = R;

  // Balls will be initially placed on a face centered cubic (fcc) grid
  // Note that the unit cells need not be actually "cubic", but the fcc grid will
  //   be stretched to cell dimensions
  const double min_cell_width = 2*sqrt(2)*R; // minimum cell width
  const int spots_per_cell = 4; // spots in each fcc periodic unit cell
  int cells[3]; // array to contain number of cells in x, y, and z dimensions
  for(int i = 0; i < 3; i++){
    cells[i] = int(sw.len[i]/min_cell_width); // max number of cells that will fit
  }

  // It is usefull to know our cell dimensions
  double cell_width[3];
  for(int i = 0; i < 3; i++) cell_width[i] = sw.len[i]/cells[i];

  // If we made our cells to small, return with error
  for(int i = 0; i < 3; i++){
    if(cell_width[i] < min_cell_width){
      printf("Placement cell size too small: (%g,  %g,  %g) coming from (%g, %g, %g)\n",
             cell_width[0],cell_width[1],cell_width[2],
             sw.len[0], sw.len[1], sw.len[2]);
      printf("Minimum allowed placement cell width: %g\n",min_cell_width);
      printf("Total simulation cell dimensions: (%g,  %g,  %g)\n",
             sw.len[0],sw.len[1],sw.len[2]);
      printf("Fixing the chosen ball number, filling fractoin, and relative\n"
             "  simulation cell dimensions simultaneously does not appear to be possible\n");
      return 176;
    }
  }

  // Define ball positions relative to cell position
  vector3d* offset = new vector3d[4]();
  offset[x] = vector3d(0,cell_width[y],cell_width[z])/2;
  offset[y] = vector3d(cell_width[x],0,cell_width[z])/2;
  offset[z] = vector3d(cell_width[x],cell_width[y],0)/2;

  // Reserve some spots at random to be vacant
  const int total_spots = spots_per_cell*cells[x]*cells[y]*cells[z];
  bool *spot_reserved = new bool[total_spots]();
  int p; // Index of reserved spot
  for(int i = 0; i < total_spots-sw.N; i++) {
    p = floor(random::ran()*total_spots); // Pick a random spot index
    if(spot_reserved[p] == false) // If it's not already reserved, reserve it
      spot_reserved[p] = true;
    else // Otherwise redo this index (look for a new spot)
      i--;
  }

  // Place all balls in remaining spots
  int b = 0;
  for(int i = 0; i < cells[x]; i++) {
    for(int j = 0; j < cells[y]; j++) {
      for(int k = 0; k < cells[z]; k++) {
        for(int l = 0; l < 4; l++) {
          if(!spot_reserved[i*(4*cells[z]*cells[y])+j*(4*cells[z])+k*4+l]) {
            sw.balls[b].pos = vector3d(i*cell_width[x],j*cell_width[y],
                                       k*cell_width[z]) + offset[l];
            b++;
          }
        }
      }
    }
  }
  delete[] offset;
  delete[] spot_reserved;
  took("Placement");

  // ----------------------------------------------------------------------------
  // Print info about the initial configuration for troubleshooting
  // ----------------------------------------------------------------------------

  {
    int most_neighbors =
      initialize_neighbor_tables(sw.balls, sw.N, sw.neighbor_R + 2*sw.dr,
                                 sw.max_neighbors, sw.len, sw.walls);
    if (most_neighbors < 0) {
      fprintf(stderr, "The guess of %i max neighbors was too low. Exiting.\n",
              sw.max_neighbors);
      return 1;
    }
    printf("Neighbor tables initialized.\n");
    printf("The most neighbors is %i, whereas the max allowed is %i.\n",
           most_neighbors, sw.max_neighbors);
  }

  // ----------------------------------------------------------------------------
  // Make sure initial placement is valid
  // ----------------------------------------------------------------------------

  bool error = false, error_cell = false;
  for(int i = 0; i < sw.N; i++) {
    if (!in_cell(sw.balls[i], sw.len, sw.walls, sw.dr)) {
      error_cell = true;
      error = true;
    }
    for(int j = 0; j < i; j++) {
      if (overlap(sw.balls[i], sw.balls[j], sw.len, sw.walls)) {
        error = true;
        break;
      }
    }
    if (error) break;
  }
  if (error){
    print_bad(sw.balls, sw.N, sw.len, sw.walls);
    printf("Error in initial placement: ");
    if(error_cell) printf("balls placed outside of cell.\n");
    else printf("balls are overlapping.\n");
    return 253;
  }

  fflush(stdout);

  // ----------------------------------------------------------------------------
  // Initialization of cell
  // ----------------------------------------------------------------------------

  double avg_neighbors = 0;
  sw.interactions =
    count_all_interactions(sw.balls, sw.N, sw.interaction_distance, sw.len, sw.walls);

  // First, let us figure out what the max entropy point is.
  sw.state_of_max_entropy = sw.initialize_max_entropy_and_translation_distance();

  if (gaussian_fit) {
    sw.initialize_gaussian(10);
  } else if (flat_histogram) {
    {
      sw.initialize_gaussian(log(1e40));
      sw.initialize_max_entropy_and_translation_distance();
    }
    const double scale = log(10);
    double width;
    double range;
    do {
      width = sw.initialize_gaussian(scale);
      range = sw.max_observed_interactions - sw.state_of_max_entropy;
      // Now shift to the max entropy state...
      sw.initialize_max_entropy_and_translation_distance();
      printf("***\n");
      printf("*** Gaussian has width %.1f compared to range %.0f (ratio %.2f)\n",
             width, range, width/range);
      printf("***\n");
    } while (width < gaussian_cutoff*range);
  } else if (fix_kT) {
    sw.initialize_canonical(fix_kT);
  } else if (wang_landau) {
    sw.initialize_wang_landau(wl_factor, wl_threshold, wl_cutoff);
  } else { // initialize optimized ensemble method
    while(sw.iteration <= initialization_iterations + first_weight_update) {
      // ---------------------------------------------------------------
      // Move each ball once
      // ---------------------------------------------------------------
      for(int i = 0; i < sw.N; i++){
        sw.move_a_ball();
      }
      assert(sw.interactions ==
             count_all_interactions(sw.balls, sw.N, sw.interaction_distance,
                                    sw.len, sw.walls));

      // ---------------------------------------------------------------
      // Update weights
      // ---------------------------------------------------------------
      if((sw.iteration > first_weight_update)
         && ((sw.iteration-first_weight_update)
             % int(first_weight_update*uipow(2,weight_updates)) == 0)) {

        printf("Weight update: %d.\n", int(uipow(2,weight_updates)));
        walker_hist(sw.energy_histogram, sw.ln_energy_weights, sw.energy_levels,
                    sw.walkers_up, sw.walkers_total, &sw.moves);
        weight_updates++;
      }
      // ---------------------------------------------------------------
      // Print out timing information if desired
      // ---------------------------------------------------------------
      if (totime > 0 && sw.iteration % totime == 0) {
        char *iter = new char[1024];
        sprintf(iter, "%i iterations", totime);
        took(iter);
        delete[] iter;
        printf("Iteration %li, acceptance rate of %g, translation_distance: %g.\n",
               sw.iteration, (double)sw.moves.working/sw.moves.total,
               sw.translation_distance);
        printf("We've had %g updates per kilomove and %g informs per kilomoves, "
               "for %g informs per update.\n",
               1000.0*sw.moves.updates/sw.moves.total,
               1000.0*sw.moves.informs/sw.moves.total,
               (double)sw.moves.informs/sw.moves.updates);
        const long checks_without_tables = sw.moves.total*sw.N;
        int total_neighbors = 0;
        int most_neighbors = 0;
        for(int i = 0; i < sw.N; i++) {
          total_neighbors += sw.balls[i].num_neighbors;
          most_neighbors = max(sw.balls[i].num_neighbors, most_neighbors);
        }
        avg_neighbors = double(total_neighbors)/sw.N;
        const long checks_with_tables = sw.moves.total*avg_neighbors
          + sw.N*sw.moves.updates;
        printf("We've done about %.3g%% of the distance calculations we would "
               "have done without tables.\n",
               100.0*checks_with_tables/checks_without_tables);
        printf("The max number of neighbors is %i, whereas the most we have is "
               "%i.\n", sw.max_neighbors, most_neighbors);
        printf("Neighbor scale is %g and avg. number of neighbors is %g.\n\n",
               neighbor_scale, avg_neighbors);
        fflush(stdout);
      }
    }
  }

  // ----------------------------------------------------------------------------
  // Generate info to put in save files
  // ----------------------------------------------------------------------------

  mkdir(data_dir, 0777); // create save directory

  char *headerinfo = new char[4096];
  sprintf(headerinfo,
          "# cell dimensions: (%g, %g, %g)\n"
          "# walls: %i\n"
          "# de_density: %g\n"
          "# de_g: %g\n"
          "# seed: %li\n"
          "# N: %i\n"
          "# R: %f\n"
          "# well_width: %g\n"
          "# translation_distance: %g\n"
          "# neighbor_scale: %g\n"
          "# dr: %g\n"
          "# energy_levels: %i\n\n",
          sw.len[0], sw.len[1], sw.len[2], sw.walls, de_density, de_g, seed, sw.N, R,
          well_width, sw.translation_distance, neighbor_scale, sw.dr, sw.energy_levels);

  char *e_fname = new char[1024];
  sprintf(e_fname, "%s/%s-E.dat", data_dir, filename);

  char *w_fname = new char[1024];
  sprintf(w_fname, "%s/%s-lnw.dat", data_dir, filename);

  char *rt_fname = new char[1024];
  sprintf(rt_fname, "%s/%s-rt.dat", data_dir, filename);

  char *density_fname = new char[1024];
  sprintf(density_fname, "%s/%s-density.dat", data_dir, filename);

  char *g_fname = new char[1024];
  sprintf(g_fname, "%s/%s-g.dat", data_dir, filename);

  // ----------------------------------------------------------------------------
  // Print initialization info
  // ----------------------------------------------------------------------------

  char *countinfo = new char[4096];
  sprintf(countinfo,
          "# iterations: %li\n"
          "# working moves: %li\n"
          "# total moves: %li\n"
          "# acceptance rate: %g\n\n",
          sw.iteration, sw.moves.working, sw.moves.total,
          double(sw.moves.working)/sw.moves.total);

  // Save weights histogram
  FILE *w_out = fopen((const char *)w_fname, "w");
  if (!w_out) {
    fprintf(stderr, "Unable to create %s!\n", w_fname);
    exit(1);
  }
  fprintf(w_out, "%s", headerinfo);
  fprintf(w_out, "%s", countinfo);
  fprintf(w_out, "# interactions\tln(weight)\n");
  for(int i = 0; i < sw.energy_levels; i++)
    fprintf(w_out, "%i  %g\n",i,sw.ln_energy_weights[i]);
  fclose(w_out);

  delete[] countinfo;

  // Now let's iterate to the point where we are at maximum
  // probability before we do the real simulation.
  sw.initialize_max_entropy_and_translation_distance();

  took("Initialization");

  // ----------------------------------------------------------------------------
  // MAIN PROGRAM LOOP
  // ----------------------------------------------------------------------------

  clock_t output_period = CLOCKS_PER_SEC; // start at outputting every minute
  // top out at one hour interval
  clock_t max_output_period = clock_t(CLOCKS_PER_SEC)*60*30;
  clock_t last_output = clock(); // when we last output data

  sw.moves.total = 0;
  sw.moves.working = 0;
  sw.iteration = 0;

  // Reset energy histogram and round trip counts
  for(int i = 0; i < sw.energy_levels; i++){
    sw.energy_histogram[i] = 0;
    sw.round_trips[i] = 0;
  }

  while(sw.iteration <= simulation_iterations) {
    // ---------------------------------------------------------------
    // Move each ball once, add to energy histogram
    // ---------------------------------------------------------------
    for(int i = 0; i < sw.N; i++)
      sw.move_a_ball();
    assert(sw.interactions ==
           count_all_interactions(sw.balls, sw.N, sw.interaction_distance, sw.len,
                                  sw.walls));
    // ---------------------------------------------------------------
    // Add data to density and RDF histograms
    // ---------------------------------------------------------------
    // Density histogram
    if(sw.walls){
      for(int i = 0; i < sw.N; i++){
        density_histogram[sw.interactions]
          [int(floor(sw.balls[i].pos[wall_dim]/de_density))] ++;
      }
    }

    // RDF
    if(!sw.walls){
      g_energy_histogram[sw.interactions]++;
      for(int i = 0; i < sw.N; i++){
        for(int j = 0; j < sw.N; j++){
          if(i != j){
            const vector3d r = periodic_diff(sw.balls[i].pos, sw.balls[j].pos, sw.len,
                                             sw.walls);
            const int r_i = floor(r.norm()/de_g);
            if(r_i < g_bins) g_histogram[sw.interactions][r_i]++;
          }
        }
      }
    }
    // ---------------------------------------------------------------
    // Save to file
    // ---------------------------------------------------------------

    const clock_t now = clock();
    if ((now - last_output > output_period) || sw.iteration == simulation_iterations) {
      last_output = now;
      assert(last_output);
      if (output_period < max_output_period/2) output_period *= 2;
      else if (output_period < max_output_period)
        output_period = max_output_period;
      const double secs_done = double(now)/CLOCKS_PER_SEC;
      const int seconds = int(secs_done) % 60;
      const int minutes = int(secs_done / 60) % 60;
      const int hours = int(secs_done / 3600) % 24;
      const int days = int(secs_done / 86400);
      printf("Saving data after %i days, %02i:%02i:%02i, %li iterations "
             "complete.\n", days, hours, minutes, seconds, sw.iteration);
      fflush(stdout);

      char *countinfo = new char[4096];
      sprintf(countinfo,
              "# iterations: %li\n"
              "# working moves: %li\n"
              "# total moves: %li\n"
              "# acceptance rate: %g\n\n",
              sw.iteration, sw.moves.working, sw.moves.total,
              double(sw.moves.working)/sw.moves.total);

      // Save energy histogram
      FILE *e_out = fopen((const char *)e_fname, "w");
      fprintf(e_out, "%s", headerinfo);
      fprintf(e_out, "%s", countinfo);
      fprintf(e_out, "# interactions   counts\n");
      for(int i = 0; i < sw.energy_levels; i++){
        if(sw.energy_histogram[i] != 0)
          fprintf(e_out, "%i  %ld\n",i,sw.energy_histogram[i]);
      }
      fclose(e_out);

      // Save round trip counts
      FILE *rt_out = fopen(rt_fname, "w");
      if (!rt_out) {
        fprintf(stderr, "Unable to create %s!\n", rt_fname);
        exit(1);
      }
      fprintf(rt_out, "%s", headerinfo);
      fprintf(rt_out, "%s", countinfo);
      fprintf(rt_out, "# interactions\tround trips\n");
      for(int i = 0; i < sw.energy_levels; i++)
        fprintf(rt_out, "%i  %li\n", i, sw.round_trips[i]);
      fclose(rt_out);

      // Save RDF
      if(!sw.walls){
        FILE *g_out = fopen((const char *)g_fname, "w");
        fprintf(g_out, "%s", headerinfo);
        fprintf(g_out, "%s", countinfo);
        fprintf(g_out, "# data table containing values of g "
                "(i.e. radial distribution function)\n"
                "# first column reserved for specifying energy level\n"
                "# column number r_n (starting from the second column, "
                "counting from zero) corresponds to radius r given by "
                "r = (r_n + 0.5) * de_g\n");
        const double density = sw.N/sw.len[x]/sw.len[y]/sw.len[z];
        const double total_vol = sw.len[x]*sw.len[y]*sw.len[z];
        for(int i = 0; i < sw.energy_levels; i++){
          if(g_histogram[i][g_bins-1] > 0){ // if we have RDF data at this energy
            fprintf(g_out, "\n%i",i);
            for(int r_i = 0; r_i < g_bins; r_i++) {
              const double probability = (double)g_histogram[i][r_i]
                / g_energy_histogram[i];
              const double r = (r_i + 0.5) * de_g;
              const double shell_vol =
                4.0/3.0*M_PI*(uipow(r+de_g/2, 3) - uipow(r-de_g/2, 3));
              const double n2 = probability/total_vol/shell_vol;
              const double g = n2/sqr(density);
              fprintf(g_out, " %8.5f", g);
            }
          }
        }
        fclose(g_out);
      }

      // Saving density data
      if(sw.walls){
        FILE *densityout = fopen((const char *)density_fname, "w");
        fprintf(densityout, "%s", headerinfo);
        fprintf(densityout, "%s", countinfo);
        fprintf(densityout, "\n# data table containing densities in slabs "
                "(bins) of thickness de_density away from a wall");
        fprintf(densityout, "\n# row number corresponds to energy level");
        fprintf(densityout, "\n# column number dn (counting from zero) "
                "corresponds to distance d from wall given by "
                "d = (dn + 0.5) * de_density");
        for(int i = 0; i < sw.energy_levels; i++){
          fprintf(densityout, "\n");
          for(int r_i = 0; r_i < density_bins; r_i++) {
            const double bin_density =
              (double)density_histogram[i][r_i]
              *sw.N/sw.energy_histogram[i]/bin_volume;
            fprintf(densityout, "%8.5f ", bin_density);
          }
        }
        fclose(densityout);
      }

      delete[] countinfo;
    }
  }
  // ----------------------------------------------------------------------------
  // END OF MAIN PROGRAM LOOP
  // ----------------------------------------------------------------------------

  for (int i=0; i<sw.N; i++) {
    delete[] sw.balls[i].neighbors;
  }
  delete[] sw.balls;
  delete[] sw.ln_energy_weights;
  delete[] sw.energy_histogram;

  delete[] sw.walkers_up;
  delete[] sw.walkers_total;

  delete[] sw.seeking_energy;
  delete[] sw.round_trips;

  for (int i = 0; i < sw.energy_levels; i++) {
    delete[] density_histogram[i];
    delete[] g_histogram[i];
  }
  delete[] g_histogram;
  delete[] density_histogram;
  delete[] g_energy_histogram;

  delete[] headerinfo;
  delete[] e_fname;
  delete[] w_fname;
  delete[] rt_fname;
  delete[] density_fname;
  delete[] g_fname;

  delete[] data_dir;
  delete[] filename;
  delete[] filename_suffix;

  return 0;
}
// ------------------------------------------------------------------------------
// END OF MAIN
// ------------------------------------------------------------------------------

inline void print_all(const ball *p, int N) {
  for (int i = 0; i < N; i++) {
    char *pos = new char[1024];
    p[i].pos.tostr(pos);
    printf("%4i: R: %4.2f, %i neighbors: ", i, p[i].R, p[i].num_neighbors);
    for(int j = 0; j < min(10, p[i].num_neighbors); j++)
      printf("%i ", p[i].neighbors[j]);
    if (p[i].num_neighbors > 10)
      printf("...");
    printf("\n      pos:          %s\n", pos);
    delete[] pos;
  }
  printf("\n");
  fflush(stdout);
}

inline void print_one(const ball &a, int id, const ball *p, int N,
                      double len[3], int walls) {
  char *pos = new char[1024];
  a.pos.tostr(pos);
  printf("%4i: R: %4.2f, %i neighbors: ", id, a.R, a.num_neighbors);
  for(int j=0; j<min(10, a.num_neighbors); j++)
    printf("%i ", a.neighbors[j]);
  if (a.num_neighbors > 10)
    printf("...");
  printf("\n      pos:          %s\n", pos);
  for (int j=0; j<N; j++) {
    if (j != id && overlap(a, p[j], len, walls)) {
      p[j].pos.tostr(pos);
      printf("\t  Overlaps with %i", j);
      printf(": %s\n", pos);
    }
  }
  delete[] pos;
  printf("\n");
  fflush(stdout);
}

inline void print_bad(const ball *p, int N, double len[3], int walls) {
  for (int i = 0; i < N; i++) {
    bool incell = in_cell(p[i], len, walls);
    bool overlaps = false;
    for (int j = 0; j < i; j++) {
      if (overlap(p[i], p[j], len, walls)) {
        overlaps = true;
        break;
      }
    }
    if (!incell || overlaps) {
      char *pos = new char[1024];
      p[i].pos.tostr(pos);
      printf("%4i: %s R: %4.2f\n", i, pos, p[i].R);
      if (!incell)
        printf("\t  Outside cell!\n");
      for (int j = 0; j < i; j++) {
        if (overlap(p[i], p[j], len, walls)) {
          p[j].pos.tostr(pos);
          printf("\t  Overlaps with %i", j);
          printf(": %s\n", pos);
        }
      }
      delete[] pos;
    }
  }
  fflush(stdout);
}

inline void check_neighbor_symmetry(const ball *p, int N) {
  for(int i = 0; i < N; i++) {
    for(int j = 0; j < p[i].num_neighbors; j++) {
      const int k = p[i].neighbors[j];
      bool is_neighbor = false;
      for (int l = 0; l < p[k].num_neighbors; l++) {
        if (p[k].neighbors[l] == i) {
          is_neighbor = true;
          break;
        }
      }
      if(!is_neighbor) {
        printf("NEIGHBOR TABLE ERROR: %i has %i as a neighbor, but %i does "
               "not reciprocate!!!\n", i, k, k);
      }
    }
  }
}

static void took(const char *name) {
  assert(name); // so it'll count as being used...
  static clock_t last_time = clock();
  clock_t t = clock();
  double seconds = (t-last_time)/double(CLOCKS_PER_SEC);
  if (seconds > 120) {
    printf("%s took %.0f minutes and %g seconds.\n", name, seconds/60,
           fmod(seconds,60));
  } else {
    printf("%s took %g seconds...\n", name, seconds);
  }
  fflush(stdout);
  last_time = t;
}

void save_locations(const ball *p, int N, const char *fname, const double len[3],
                    const char *comment) {
  FILE *out = fopen((const char *)fname, "w");
  fprintf(out, "# %s\n", comment);
  fprintf(out, "%g %g %g\n", len[x], len[y], len[z]);
  for(int i = 0; i < N; i++) {
    fprintf(out, "%6.2f %6.2f %6.2f ", p[i].pos[x], p[i].pos[y], p[i].pos[z]);
    fprintf(out, "\n");
  }
  fclose(out);
}

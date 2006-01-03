/**
 *
 *@file prune_bounds.c
 *
 * Prune stuff based on bounds
 *
 */

#include <limits.h>

#include "termlist.h"
#include "list.h"
#include "switches.h"

extern int attack_length;
extern int attack_leastcost;
extern Protocol INTRUDER;
extern int proofDepth;
extern int max_encryption_level;
extern int num_regular_runs;
extern int num_intruder_runs;

//! Prune determination for bounds
/**
 * When something is pruned here, the state space is not complete anymore.
 *
 *@returns true iff this state is invalid for some reason
 */
int
prune_bounds (const System sys)
{
  Termlist tl;
  List bl;

  /* prune for time */
  if (passed_time_limit ())
    {
      // Oh no, we ran out of time!
      if (switches.output == PROOF)
	{
	  indentPrint ();
	  eprintf ("Pruned: ran out of allowed time (-T %i switch)\n",
		   get_time_limit ());
	}
      // Pruned because of time bound!
      sys->current_claim->timebound = 1;
      return 1;
    }

  /* prune for proof depth */
  if (proofDepth > switches.maxproofdepth)
    {
      // Hardcoded limit on proof tree depth
      if (switches.output == PROOF)
	{
	  indentPrint ();
	  eprintf ("Pruned: proof tree too deep: %i (-d %i switch)\n",
		   proofDepth, switches.maxproofdepth);
	}
      return 1;
    }

  /* prune for trace length */
  if (switches.maxtracelength < INT_MAX)
    {
      int tracelength;
      int run;

      /* compute trace length of current semistate */
      tracelength = 0;
      run = 0;
      while (run < sys->maxruns)
	{
	  /* ignore intruder actions */
	  if (sys->runs[run].protocol != INTRUDER)
	    {
	      tracelength = tracelength + sys->runs[run].step;
	    }
	  run++;
	}
      /* test */
      if (tracelength > switches.maxtracelength)
	{
	  // Hardcoded limit on proof tree depth
	  if (switches.output == PROOF)
	    {
	      indentPrint ();
	      eprintf ("Pruned: trace too long: %i (-l %i switch)\n",
		       tracelength, switches.maxtracelength);
	    }
	  return 1;
	}
    }

  if (num_regular_runs > switches.runs)
    {
      // Hardcoded limit on runs
      if (switches.output == PROOF)
	{
	  indentPrint ();
	  eprintf ("Pruned: too many regular runs (%i).\n", num_regular_runs);
	}
      return 1;
    }

  // This needs some foundation. Probably * 2^max_encryption_level
  //!@todo Fix this bound
  if ((switches.match < 2)
      && (num_intruder_runs >
	  ((double) switches.runs * max_encryption_level * 8)))
    {
      // Hardcoded limit on iterations
      if (switches.output == PROOF)
	{
	  indentPrint ();
	  eprintf
	    ("Pruned: %i intruder runs is too much. (max encr. level %i)\n",
	     num_intruder_runs, max_encryption_level);
	}
      return 1;
    }

  // Limit on exceeding any attack length
  if (get_semitrace_length () >= attack_length)
    {
      if (switches.output == PROOF)
	{
	  indentPrint ();
	  eprintf ("Pruned: attack length %i.\n", attack_length);
	}
      return 1;
    }

  /* prune for cheaper */
  if (switches.prune == 2 && attack_leastcost <= attackCost (sys))
    {
      // We already had an attack at least this cheap.
      if (switches.output == PROOF)
	{
	  indentPrint ();
	  eprintf
	    ("Pruned: attack cost exceeds a previously found attack.\n");
	}
      return 1;
    }

  // Limit on attack count
  if (enoughAttacks (sys))
    return 1;

  // Pruning involving the number of intruder actions
  {
    // Count intruder actions
    int actioncount;

    actioncount = countIntruderActions ();

    // Limit intruder actions in any case
    if (!(switches.intruder) && actioncount > 0)
      {
	if (switches.output == PROOF)
	  {
	    indentPrint ();
	    eprintf
	      ("Pruned: no intruder allowed.\n", switches.maxIntruderActions);
	  }
	return 1;
      }

    // Limit on intruder events count
    if (actioncount > switches.maxIntruderActions)
      {
	if (switches.output == PROOF)
	  {
	    indentPrint ();
	    eprintf
	      ("Pruned: more than %i encrypt/decrypt events in the semitrace.\n",
	       switches.maxIntruderActions);
	  }
	return 1;
      }
  }

  // No pruning because of bounds
  return 0;
}
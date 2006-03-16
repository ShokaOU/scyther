#include <stdlib.h>
#include <limits.h>
#include "system.h"
#include "switches.h"
#include "arachne.h"
#include "binding.h"
#include "depend.h"
#include "debug.h"

extern Protocol INTRUDER;	// Pointers, to be set by the Init of arachne.c
extern Role I_M;		// Same here.
extern Role I_RRS;
extern Role I_RRSD;

#define INVALID		-1
#define isGoal(rd)	(rd->type == READ && !rd->internal)
#define isBound(rd)	(rd->bound)
#define length		step


#define CLAIMTEXTCOLOR "#ffffff"
#define CLAIMCOLOR "#000000"
#define GOODCOMMCOLOR "forestgreen"

#define INTRUDERCOLORH 18.0
#define INTRUDERCOLORL 0.65
#define INTRUDERCOLORS 0.9
#define RUNCOLORL1 0.90
#define RUNCOLORL2 0.65
#define RUNCOLORH1 (INTRUDERCOLORH + 360 - 10.0)
#define RUNCOLORH2 (INTRUDERCOLORH + 10.0)
#define RUNCOLORS1 0.8
#define RUNCOLORS2 0.6
#define RUNCOLORDELTA 0.2	// maximum hue delta between roles (0.2): smaller means role colors of a protocol become more similar.
#define RUNCOLORCONTRACT 0.8	// contract from protocol edges: smaller means more distinction between protocols.
#define UNTRUSTEDCOLORS 0.4

/*
 * Dot output
 *
 *
 * The algorithm itself is not very complicated; because the semi-bundles have
 * bindings etcetera, a graph can be draw quickly and efficiently.
 *
 * Interesting issues:
 *
 * Binding annotations are only drawn if they don't connect with regular
 * events, and when the item does not occur in any previous binding, it might
 * be connected to the initial intruder knowledge.
 *
 * Color management is quite involved. We draw identical protocols in similar
 * color schemes.  A color scheme is a gradient between two colors, evenly
 * spread over all the runs.
 */

static System sys = NULL;

/*
 * code
 */

void
printVisualRun (int rid)
{
  int run;
  int display;

  display = 1;
  for (run = 0; run < rid; run++)
    {
      if (sys->runs[run].protocol != INTRUDER)
	{
	  display++;
	}
    }
  eprintf ("#%i", display);
}

//! Remap term stuff
void
termPrintRemap (const Term t)
{
  termPrintCustom (t, "", "", "(", ")", "\\{ ", " \\}", printVisualRun);
}

//! Draw node
void
node (const System sys, const int run, const int index)
{
  if (sys->runs[run].protocol == INTRUDER)
    {
      if (sys->runs[run].role == I_M)
	{
	  eprintf ("intruder");
	}
      else
	{
	  eprintf ("ri%i", run);
	}
    }
  else
    {
      eprintf ("r%ii%i", run, index);
    }
}

//! Draw arrow
void
arrow (const System sys, Binding b)
{
  node (sys, b->run_from, b->ev_from);
  eprintf (" -> ");
  node (sys, b->run_to, b->ev_to);
}

//! Redirect node
void
redirNode (const System sys, Binding b)
{
  eprintf ("redir_");
  node (sys, b->run_from, b->ev_from);
  node (sys, b->run_to, b->ev_to);
}

//! Roledef draw
void
roledefDraw (Roledef rd)
{
  void optlabel (void)
  {
    Term label;

    label = rd->label;
    if (label != NULL)
      {
	if (realTermTuple (label))
	  {
	    label = TermOp2 (label);
	  }
	eprintf ("_");
	termPrintRemap (label);
      }
  }

  if (rd->type == READ)
    {
      eprintf ("read");
      optlabel ();
      eprintf (" from ");
      termPrintRemap (rd->from);
      eprintf ("\\n");
      termPrintRemap (rd->message);
    }
  if (rd->type == SEND)
    {
      eprintf ("send");
      optlabel ();
      eprintf (" to ");
      termPrintRemap (rd->to);
      eprintf ("\\n");
      termPrintRemap (rd->message);
    }
  if (rd->type == CLAIM)
    {
      eprintf ("claim");
      optlabel ();
      eprintf ("\\n");
      termPrintRemap (rd->to);
      if (rd->message != NULL)
	{
	  eprintf (" : ");
	  termPrintRemap (rd->message);
	}
    }
}

//! Choose term node
void
chooseTermNode (const Term t)
{
  eprintf ("CHOOSE");
  {
    char *rsbuf;

    rsbuf = RUNSEP;
    RUNSEP = "x";
    termPrint (t);
    RUNSEP = rsbuf;
  }
}

//! Value for hlsrgb conversion
static double
hlsValue (double n1, double n2, double hue)
{
  if (hue > 360.0)
    hue -= 360.0;
  else if (hue < 0.0)
    hue += 360.0;
  if (hue < 60.0)
    return n1 + (n2 - n1) * hue / 60.0;
  else if (hue < 180.0)
    return n2;
  else if (hue < 240.0)
    return n1 + (n2 - n1) * (240.0 - hue) / 60.0;
  else
    return n1;
}

//! hls to rgb conversion
void
hlsrgb (int *r, int *g, int *b, double h, double l, double s)
{
  double m1, m2;

  int bytedouble (double d)
  {
    double x;

    x = 255.0 * d;
    if (x <= 0)
      return 0;
    else if (x >= 255.0)
      return 255;
    else
      return (int) x;
  }

  while (h >= 360.0)
    h -= 360.0;
  while (h < 0)
    h += 360.0;
  m2 = (l <= 0.5) ? (l * (l + s)) : (l + s - l * s);
  m1 = 2.0 * l - m2;
  if (s == 0.0)
    {
      *r = *g = *b = bytedouble (l);
    }
  else
    {
      *r = bytedouble (hlsValue (m1, m2, h + 120.0));
      *g = bytedouble (hlsValue (m1, m2, h));
      *b = bytedouble (hlsValue (m1, m2, h - 120.0));
    }
}

//! print color from h,l,s triplet
void
printColor (double h, double l, double s)
{
  int r, g, b;

  hlsrgb (&r, &g, &b, h, l, s);
  eprintf ("#%02x%02x%02x", r, g, b);
}


//! Set local buffer with the correct color for this run.
/**
 * Determines number of protocols, shifts to the right color pair, and colors
 * the run within the current protocol in the fade between the color pair.
 *
 * This can be done much more efficiently by computing these colors once,
 * instead of each time again for each run. However, this is not a
 * speed-critical section so this will do just nicely.
 */
void
setRunColorBuf (const System sys, int run, char *colorbuf)
{
  int range;
  int index;
  double protoffset, protrange;
  double roleoffset, roledelta;
  double color;
  double h, l, s;
  int r, g, b;

  // help function: contract roleoffset, roledelta with a factor (<= 1.0)
  void contract (double factor)
  {
    roledelta = roledelta * factor;
    roleoffset = (roleoffset * factor) + ((1.0 - factor) / 2.0);
  }

  // determine #protocol, resulting in two colors
  {
    Termlist protocols;
    Term refprot;
    int r;
    int firstfound;

    protocols = NULL;
    refprot = sys->runs[run].protocol->nameterm;
    index = 0;
    range = 1;
    firstfound = false;
    for (r = 0; r < sys->maxruns; r++)
      {
	if (sys->runs[r].protocol != INTRUDER)
	  {
	    Term prot;

	    prot = sys->runs[r].protocol->nameterm;
	    if (!isTermEqual (prot, refprot))
	      {
		// Some 'other' protocol
		if (!inTermlist (protocols, prot))
		  {
		    // New other protocol
		    protocols = termlistAdd (protocols, prot);
		    range++;
		    if (!firstfound)
		      {
			index++;
		      }
		  }
	      }
	    else
	      {
		// Our protocol
		firstfound = true;
	      }
	  }
      }
    termlistDelete (protocols);
  }

  // Compute protocol offset [0.0 ... 1.0>
  protrange = 1.0 / range;
  protoffset = index * protrange;

  // We now now our range, and we can determine which role this one is.
  {
    Role rr;
    int done;

    range = 0;
    index = 0;
    done = false;

    for (rr = sys->runs[run].protocol->roles; rr != NULL; rr = rr->next)
      {
	if (sys->runs[run].role == rr)
	  {
	    done = true;
	  }
	else
	  {
	    if (!done)
	      {
		index++;
	      }
	  }
	range++;
      }
  }

  // Compute role offset [0.0 ... 1.0]
  if (range <= 1)
    {
      roledelta = 0.0;
      roleoffset = 0.5;
    }
  else
    {
      // range over 0..1
      roledelta = 1.0 / (range - 1);
      roleoffset = index * roledelta;
      // Now this can result in a delta that is too high (depending on protocolrange)
      if (protrange * roledelta > RUNCOLORDELTA)
	{
	  contract (RUNCOLORDELTA / (protrange * roledelta));
	}
    }

  // We slightly contract the colors (taking them away from protocol edges)
  contract (RUNCOLORCONTRACT);

  // Now we can convert this to a color
  color = protoffset + (protrange * roleoffset);
  h = RUNCOLORH1 + color * (RUNCOLORH2 - RUNCOLORH1);
  l = RUNCOLORL1 + color * (RUNCOLORL2 - RUNCOLORL1);
  s = RUNCOLORS1 + color * (RUNCOLORS2 - RUNCOLORS1);

  // If the run is not trusted, we lower the saturation significantly
  if (!isRunTrusted (sys, run))
    {
      s = UNTRUSTEDCOLORS;
    }

  // set to buffer
  hlsrgb (&r, &g, &b, h, l, s);
  sprintf (colorbuf, "#%02x%02x%02x", r, g, b);

  // compute second color (light version)
  /*
     l += 0.07;
     if (l > 1.0)
     l = 1.0;
   */
  hlsrgb (&r, &g, &b, h, l, s);
  sprintf (colorbuf + 8, "#%02x%02x%02x", r, g, b);
}

//! Communication status
int
isCommunicationExact (const System sys, Binding b)
{
  Roledef rd1, rd2;

  rd1 = eventRoledef (sys, b->run_from, b->ev_from);
  rd2 = eventRoledef (sys, b->run_to, b->ev_to);

  if (!isTermEqual (rd1->message, rd2->message))
    {
      return false;
    }
  if (!isTermEqual (rd1->from, rd2->from))
    {
      return false;
    }
  if (!isTermEqual (rd1->to, rd2->to))
    {
      return false;
    }
  if (!isTermEqual (rd1->label, rd2->label))
    {
      return false;
    }
  return true;
}

//! Determine ranks for all nodes
/**
 * Some crude algorithm I sketched on the blackboard.
 */
int
graph_ranks (int *ranks, int nodes)
{
  int i;
  int todo;
  int rank;

#ifdef DEBUG
  if (hasCycle ())
    {
      error ("Graph ranks tried, but a cycle exists!");
    }
#endif

  i = 0;
  while (i < nodes)
    {
      ranks[i] = INT_MAX;
      i++;
    }

  todo = nodes;
  rank = 0;
  while (todo > 0)
    {
      // There are still unassigned nodes
      int n;

      n = 0;
      while (n < nodes)
	{
	  if (ranks[n] == INT_MAX)
	    {
	      // Does this node have incoming stuff from stuff with equal rank or higher?
	      int refn;

	      refn = 0;
	      while (refn < nodes)
		{
		  if (ranks[refn] >= rank && getNode (refn, n))
		    refn = nodes + 1;
		  else
		    refn++;
		}
	      if (refn == nodes)
		{
		  ranks[n] = rank;
		  todo--;
		}
	    }
	  n++;
	}
      rank++;
    }
  return rank;
}


//! Iterate over events (in non-intruder runs) in which some value term occurs first.
// Function should return true for iteration to continue.
int
iterate_first_regular_occurrences (const System sys,
				   int (*func) (int run, int ev),
				   const Term t)
{
  int run;

  for (run = 0; run < sys->maxruns; run++)
    {
      if (sys->runs[run].protocol != INTRUDER)
	{
	  int ev;
	  Roledef rd;

	  rd = sys->runs[run].start;
	  for (ev = 0; ev < sys->runs[run].step; ev++)
	    {
	      if (termSubTerm (rd->from, t) ||
		  termSubTerm (rd->to, t) || termSubTerm (rd->message, t))
		{
		  // Allright, t occurs here in this run first
		  if (!func (run, ev))
		    {
		      return false;
		    }
		  break;
		}
	    }
	}
    }
  return true;
}

//! Does a term occur in a run?
int
termOccursInRun (Term t, int run)
{
  Roledef rd;
  int e;

  rd = sys->runs[run].start;
  e = 0;
  while (e < sys->runs[run].step)
    {
      if (roledefSubTerm (rd, t))
	{
	  return true;
	}
      e++;
      rd = rd->next;
    }
  return false;
}

//! Draw a class choice
/**
 * \rho classes are already dealt with in the headers, so we should ignore them.
 */
void
drawClass (const System sys, Binding b)
{
  Term varterm;

  // now check in previous things whether we saw that term already
  int notSameTerm (Binding b2)
  {
    return (!isTermEqual (varterm, b2->term));
  }

  varterm = deVar (b->term);

  // Variable?
  if (!isTermVariable (varterm))
    {
      return;
    }

  // Agent variable?
  {
    int run;

    run = TermRunid (varterm);
    if ((run >= 0) && (run < sys->maxruns))
      {
	if (inTermlist (sys->runs[run].rho, varterm))
	  {
	    return;
	  }
      }
  }

  // Seen before?
  if (!iterate_preceding_bindings (b->run_to, b->ev_to, notSameTerm))
    {
      // We saw the same term before. Exit.
      return;
    }




  // not seen before: choose class
  eprintf ("\t");
  chooseTermNode (varterm);
  eprintf (" [label=\"Any ");
  termPrintRemap (varterm);
  eprintf ("\"];\n");
  eprintf ("\t");
  chooseTermNode (varterm);
  eprintf (" -> ");
  node (sys, b->run_to, b->ev_to);
  eprintf (" [weight=\"2.0\",arrowhead=\"none\",style=\"dotted\"];\n");
}

//! Draw a single binding
void
drawBinding (const System sys, Binding b)
{
  int intr_to, intr_from;

  intr_from = (sys->runs[b->run_from].protocol == INTRUDER);
  intr_to = (sys->runs[b->run_to].protocol == INTRUDER);

  if (intr_from)
    {
      // from intruder
      /*
       * Because this can be generated many times, it seems
       * reasonable to not duplicate such arrows, especially when
       * they're from M_0. Maybe the others are still relevant.
       */
      if (1 == 1 || sys->runs[b->run_from].role == I_M)
	{
	  // now check in previous things whether we saw that term already
	  int notSameTerm (Binding b2)
	  {
	    return (!isTermEqual (b->term, b2->term));
	  }

	  if (!iterate_preceding_bindings (b->run_to, b->ev_to, notSameTerm))
	    {
	      // We saw the same term before. Exit.
	      return;
	    }
	}

      // normal from intruder (not M0)
      if (intr_to)
	{
	  // intr->intr
	  eprintf ("\t");
	  arrow (sys, b);
	  eprintf (" [label=\"");
	  termPrintRemap (b->term);
	  eprintf ("\"]");
	  eprintf (";\n");
	}
      else
	{
	  // intr->regular
	  eprintf ("\t");
	  arrow (sys, b);
	  eprintf (";\n");
	}
    }
  else
    {
      // not from intruder
      if (intr_to)
	{
	  // regular->intr
	  eprintf ("\t");
	  arrow (sys, b);
	  eprintf (";\n");
	}
      else
	{
	  // regular->regular
	  /*
	   * Has this been done *exactly* as we hoped?
	   */
	  if (isCommunicationExact (sys, b))
	    {
	      eprintf ("\t");
	      arrow (sys, b);
	      eprintf (" [style=bold,color=\"%s\"]", GOODCOMMCOLOR);
	      eprintf (";\n");
	    }
	  else
	    {
	      // Something was changed, so we call this a redirect
	      eprintf ("\t");
	      node (sys, b->run_from, b->ev_from);
	      eprintf (" -> ");
	      redirNode (sys, b);
	      eprintf (" -> ");
	      node (sys, b->run_to, b->ev_to);
	      eprintf (";\n");

	      eprintf ("\t");
	      redirNode (sys, b);
	      eprintf (" [style=filled,fillcolor=\"");
	      printColor (INTRUDERCOLORH, INTRUDERCOLORL, INTRUDERCOLORS);
	      eprintf ("\",label=\"redirect");
	      if (!isTermEqual
		  (eventRoledef (sys, b->run_from, b->ev_from)->message,
		   b->term))
		{
		  // Only explicitly mention redirect term when it differs from the sent term
		  eprintf ("\\n");
		  termPrintRemap (b->term);
		}
	      eprintf ("\"]");
	      eprintf (";\n");

	    }
	}
    }
}

//! Draw dependecies (including intruder!)
/**
 * Returns from_intruder_count
 */
int
drawAllBindings (const System sys)
{
  List bl;
  int fromintr;

  fromintr = 0;
  for (bl = sys->bindings; bl != NULL; bl = bl->next)
    {
      Binding b;

      b = (Binding) bl->data;
      if (!b->blocked)
	{
	  // if the binding is not done (class choice) we might
	  // still show it somewhere.
	  if (b->done)
	    {
	      // done, draw
	      drawBinding (sys, b);
	      // from intruder?
	      if (sys->runs[b->run_from].protocol == INTRUDER)
		{
		  fromintr++;
		}
	    }
	  else
	    {
	      drawClass (sys, b);
	    }
	}
    }
  return fromintr;
}

//! Nicely format the role and agents we think we're talking to.
void
agentsOfRunPrintOthers (const System sys, const int run)
{
  Term role = sys->runs[run].role->nameterm;
  Termlist roles = sys->runs[run].protocol->rolenames;

  while (roles != NULL)
    {
      if (!isTermEqual (role, roles->term))
	{
	  Term agent;

	  termPrintRemap (roles->term);
	  eprintf (" is ");
	  agent = agentOfRunRole (sys, run, roles->term);
	  if (isTermVariable (agent))
	    {
	      eprintf ("any ");
	    }
	  termPrintRemap (agent);
	  eprintf ("\\l");
	}
      roles = roles->next;
    }
}



//! Draw regular runs
void
drawRegularRuns (const System sys)
{
  int run;
  int rcnum;
  char *colorbuf;

  // two buffers, eight chars each
  colorbuf = malloc (16 * sizeof (char));

  rcnum = 0;
  for (run = 0; run < sys->maxruns; run++)
    {
      if (sys->runs[run].length > 0)
	{
	  if (sys->runs[run].protocol != INTRUDER)
	    {
	      Roledef rd;
	      int index;
	      int prevnode;

	      prevnode = 0;
	      index = 0;
	      rd = sys->runs[run].start;
	      // Regular run

	      /*
	         eprintf ("\tsubgraph cluster_run%i {\n", run);
	         eprintf ("\t\tlabel = \"");
	         eprintf ("#%i: ", run);
	         termPrintRemap (sys->runs[run].protocol->nameterm);
	         eprintf (", ");
	         agentsOfRunPrint (sys, run);
	         eprintf ("\\nTesting the second line\";\n", run);
	         if (run == 0)
	         {
	         eprintf ("\t\tcolor = red;\n");
	         }
	         else
	         {
	         eprintf ("\t\tcolor = blue;\n");
	         }
	       */

	      // set color
	      setRunColorBuf (sys, run, colorbuf);

	      // Display the respective events
	      while (index < sys->runs[run].length)
		{
		  int showthis;

		  showthis = true;
		  if (rd->type == CLAIM)
		    {
		      if (run != 0)
			{
			  showthis = false;
			}
		      else
			{
			  if (index != sys->current_claim->ev)
			    {
			      showthis = false;
			    }
			}
		    }
		  if (showthis)
		    {
		      // Print node itself
		      eprintf ("\t\t");
		      node (sys, run, index);
		      eprintf (" [");
		      if (run == 0 && index == sys->current_claim->ev)
			{
			  // The claim under scrutiny
			  eprintf
			    ("style=filled,fontcolor=\"%s\",fillcolor=\"%s\",shape=box,",
			     CLAIMTEXTCOLOR, CLAIMCOLOR);
			}
		      else
			{
			  eprintf ("shape=box,style=filled,");
			  // print color of this run
			  eprintf ("fillcolor=\"%s\",", colorbuf);
			}
		      eprintf ("label=\"");
		      //roledefPrintShort (rd);
		      roledefDraw (rd);
		      eprintf ("\"]");
		      eprintf (";\n");

		      // Print binding to previous node
		      if (index > sys->runs[run].firstReal)
			{
			  // index > 0
			  eprintf ("\t\t");
			  node (sys, run, prevnode);
			  eprintf (" -> ");
			  node (sys, run, index);
			  eprintf (" [style=\"bold\", weight=\"10.0\"]");
			  eprintf (";\n");
			  prevnode = index;
			}
		      else
			{
			  // index <= firstReal
			  if (index == sys->runs[run].firstReal)
			    {
			      // index == firstReal
			      Roledef rd;
			      int send_before_read;
			      int done;

			      // Determine if it is an active role or note
			      /**
			       *@todo note that this will probably become a standard function call for role.h
			       */
			      rd =
				roledef_shift (sys->runs[run].start,
					       sys->runs[run].firstReal);
			      done = 0;
			      send_before_read = 0;
			      while (!done && rd != NULL)
				{
				  if (rd->type == READ)
				    {
				      done = 1;
				    }
				  if (rd->type == SEND)
				    {
				      done = 1;
				      send_before_read = 1;
				    }
				  rd = rd->next;
				}
			      // Draw the first box (HEADER)
			      // This used to be drawn only if done && send_before_read, now we always draw it.
			      eprintf ("\t\ts%i [label=\"{ ", run);

			      // Print first line
			      {
				Term rolename;
				Term agentname;

				rolename = sys->runs[run].role->nameterm;
				agentname =
				  agentOfRunRole (sys, run, rolename);
				if (isTermVariable (agentname))
				  {
				    eprintf ("Any agent ");
				  }
				termPrintRemap (agentname);
				eprintf (" in role ");
				termPrintRemap (rolename);
				eprintf ("\\l");
			      }

			      // Second line
			      // Possible protocol (if more than one)
			      {
				int showprotocol;
				Protocol p;
				int morethanone;

				// Simple case: don't show
				showprotocol = false;

				// Check whether the protocol spec has more than one
				morethanone = false;
				for (p = sys->protocols; p != NULL;
				     p = p->next)
				  {
				    if (p != INTRUDER)
				      {
					if (p != sys->runs[run].protocol)
					  {
					    morethanone = true;
					    break;
					  }
				      }
				  }

				// More than one?
				if (morethanone)
				  {
				    if (run == 0)
				      {
					// If this is run 0 we report the protocol anyway, even is there is only a single one in the attack
					showprotocol = true;
				      }
				    else
				      {
					int r;
					// For other runs we only report when there are multiple protocols
					showprotocol = false;
					for (r = 0; r < sys->maxruns; r++)
					  {
					    if (sys->runs[r].protocol !=
						INTRUDER)
					      {
						if (sys->runs[r].protocol !=
						    sys->runs[run].protocol)
						  {
						    showprotocol = true;
						    break;
						  }
					      }
					  }
				      }
				  }

				// Use the result
				if (showprotocol)
				  {
				    eprintf ("Protocol ");
				    termPrintRemap (sys->runs[run].protocol->
						    nameterm);
				    eprintf ("\\l");
				  }
			      }
			      eprintf ("Run ");
			      printVisualRun (run);
			      eprintf ("\\l");


			      // rho, sigma, const
			      void showLocal (Term told, Term tnew)
			      {
				if (realTermVariable (tnew))
				  {
				    // Variables are mapped, maybe. But then we wonder whether they occur in reads.
				    termPrintRemap (told);
				    if (termOccursInRun (tnew, run))
				      {
					Term t;

					t = deVar (tnew);
					eprintf (" : ");
					if (realTermVariable (t))
					  {
					    eprintf ("any ");
					    termPrintRemap (t);
					    if ((!t->roleVar)
						&& switches.match == 0
						&& t->stype != NULL)
					      {
						Termlist tl;

						eprintf (" of type ");
						for (tl = t->stype;
						     tl != NULL;
						     tl = tl->next)
						  {
						    termPrintRemap (tl->term);
						    if (tl->next != NULL)
						      {
							eprintf (",");
						      }
						  }
					      }
					  }
					else
					  {
					    termPrintRemap (t);
					  }
				      }
				    else
				      {
					eprintf (" is not read");
				      }
				  }
				else
				  {
				    termPrintRemap (tnew);
				  }
				eprintf ("\\l");
			      }
			      void showLocals (Termlist tlold, Termlist tlnew,
					       Term tavoid)
			      {
				while (tlold != NULL && tlnew != NULL)
				  {
				    if (!isTermEqual (tlold->term, tavoid))
				      {
					showLocal (tlold->term, tlnew->term);
				      }
				    tlold = tlold->next;
				    tlnew = tlnew->next;
				  }
			      }

			      if (termlistLength (sys->runs[run].rho) > 1)
				{
				  eprintf ("|");
				  if (sys->runs[run].role->initiator)
				    {
				      eprintf ("Initiates with:\\l");
				    }
				  else
				    {
				      eprintf ("Responds to:\\l");
				    }
				  showLocals (sys->runs[run].protocol->
					      rolenames, sys->runs[run].rho,
					      sys->runs[run].role->nameterm);
				}

			      if (sys->runs[run].constants != NULL)
				{
				  eprintf ("|Creates:\\l");
				  showLocals (sys->runs[run].role->
					      declaredconsts,
					      sys->runs[run].constants, NULL);
				}
			      if (sys->runs[run].sigma != NULL)
				{
				  eprintf ("|Variables:\\l");
				  showLocals (sys->runs[run].role->
					      declaredvars,
					      sys->runs[run].sigma, NULL);
				}

			      // close up
			      eprintf ("}\", shape=record");
			      eprintf (",style=filled,fillcolor=\"%s\"",
				       colorbuf + 8);
			      eprintf ("];\n");
			      eprintf ("\t\ts%i -> ", run);
			      node (sys, run, index);
			      eprintf (" [style=bold, weight=\"10.0\"];\n");
			      prevnode = index;
			    }
			}
		    }
		  index++;
		  rd = rd->next;
		}
	      //eprintf ("\t}\n");

	    }
	}
    }
  free (colorbuf);
}

//! Draw intruder runs
void
drawIntruderRuns (const System sys)
{
  int run;

  /*
     eprintf ("\tsubgraph intruder {\n");
     eprintf ("\t\tlabel = \"Intruder\\nTesting the seconds.\";\n");
     eprintf ("\t\tcolor = red;\n");
   */
  for (run = 0; run < sys->maxruns; run++)
    {
      if (sys->runs[run].length > 0)
	{
	  if (sys->runs[run].protocol == INTRUDER)
	    {
	      // Intruder run
	      if (sys->runs[run].role != I_M)
		{
		  // Not an M_0 run, so we can draw it.
		  eprintf ("\t\t");
		  node (sys, run, 0);
		  eprintf (" [style=filled,fillcolor=\"");
		  printColor (INTRUDERCOLORH, INTRUDERCOLORL, INTRUDERCOLORS);
		  eprintf ("\",");
		  if (sys->runs[run].role == I_RRSD)
		    {
		      eprintf ("label=\"decrypt\"");
		    }
		  if (sys->runs[run].role == I_RRS)
		    {
		      // Distinguish function application
		      if (isTermFunctionName
			  (sys->runs[run].start->next->message))
			{
			  eprintf ("label=\"apply\"");
			}
		      else
			{
			  eprintf ("label=\"encrypt\"");
			}
		    }
		  eprintf ("];\n");
		}
	    }
	}
    }
  //eprintf ("\t}\n\n");
}

//! Show intruder choices
void
drawIntruderChoices (const System sys)
{
  Termlist shown;
  List bl;

  shown = NULL;
  for (bl = sys->bindings; bl != NULL; bl = bl->next)
    {
      Binding b;

      b = (Binding) bl->data;
      if ((!b->blocked) && (!b->done) && isTermVariable (b->term))
	{
	  /*
	   * If the binding is not blocked, but also not done,
	   * the intruder can apparently satisfy it at will.
	   */

	  if (!inTermlist (shown, b->term))
	    {
	      int firsthere (int run, int ev)
	      {
					/**
					 * @todo This is not very efficient,
					 * but maybe that is not really
					 * needed here.
					 */
		int notearlier (int run2, int ev2)
		{
		  if (sys->runs[run2].protocol != INTRUDER)
		    {
		      return (!roledefSubTerm
			      (eventRoledef (sys, run2, ev2), b->term));
		    }
		  else
		    {
		      return true;
		    }
		}

		if (iteratePrecedingEvents (sys, notearlier, run, ev))
		  {
		    eprintf ("\t");
		    chooseTermNode (b->term);
		    eprintf (" -> ");
		    node (sys, run, ev);
		    eprintf (" [color=\"darkgreen\"];\n");
		  }
		return true;
	      }

	      // If the first then we add a header
	      if (shown == NULL)
		{
		  eprintf ("// Showing intruder choices.\n");
		}
	      // Not in the list, show new node
	      shown = termlistAdd (shown, b->term);
	      eprintf ("\t");
	      chooseTermNode (b->term);
	      eprintf (" [label=\"Class: any ");
	      termPrintRemap (b->term);
	      eprintf ("\",color=\"darkgreen\"];\n");

	      iterate_first_regular_occurrences (sys, firsthere, b->term);
	    }
	}

    }

  if (shown != NULL)
    {
      eprintf ("\n");
    }
  termlistDelete (shown);
}


//! Display the current semistate using dot output format.
/**
 * This is not as nice as we would like it. Furthermore, the function is too big.
 */
void
dotSemiState (const System mysys)
{
  static int attack_number = 0;
  int run;
  Protocol p;
  int *ranks;
  int maxrank;
  int from_intruder_count;
  int nodes;

  sys = mysys;

  // Open graph
  attack_number++;
  eprintf ("digraph semiState%i {\n", attack_number);
  eprintf ("\tlabel = \"[Id %i] Protocol ", sys->attackid);
  p = (Protocol) sys->current_claim->protocol;
  termPrintRemap (p->nameterm);
  eprintf (", role ");
  termPrintRemap (sys->current_claim->rolename);
  eprintf (", claim type ");
  termPrintRemap (sys->current_claim->type);
  eprintf ("\";\n");

  // Globals
  eprintf ("\tfontname=Helvetica;\n");
  eprintf ("\tnode [fontname=Helvetica];\n");
  eprintf ("\tedge [fontname=Helvetica];\n");

  // Needed for the bindings later on: create graph

  nodes = nodeCount ();
  ranks = malloc (nodes * sizeof (int));
  maxrank = graph_ranks (ranks, nodes);	// determine ranks

#ifdef DEBUG
  // For debugging purposes, we also display an ASCII version of some stuff in the comments
  printSemiState ();
  // Even draw all dependencies for non-intruder runs
  // Real nice debugging :(
  {
    int run;

    run = 0;
    while (run < sys->maxruns)
      {
	int ev;

	ev = 0;
	while (ev < sys->runs[run].length)
	  {
	    int run2;
	    int notfirstrun;

	    eprintf ("// precedence: r%ii%i <- ", run, ev);
	    run2 = 0;
	    notfirstrun = 0;
	    while (run2 < sys->maxruns)
	      {
		int notfirstev;
		int ev2;

		notfirstev = 0;
		ev2 = 0;
		while (ev2 < sys->runs[run2].length)
		  {
		    if (isDependEvent (run2, ev2, run, ev))
		      {
			if (notfirstev)
			  eprintf (",");
			else
			  {
			    if (notfirstrun)
			      eprintf (" ");
			    eprintf ("r%i:", run2);
			  }
			eprintf ("%i", ev2);
			notfirstrun = 1;
			notfirstev = 1;
		      }
		    ev2++;
		  }
		run2++;
	      }
	    eprintf ("\n");
	    ev++;
	  }
	run++;
      }
  }
#endif

  // First, runs
  drawRegularRuns (sys);
  drawIntruderRuns (sys);
  from_intruder_count = drawAllBindings (sys);

  // Third, the intruder node (if needed)
  if (from_intruder_count > 0)
    {
      eprintf
	("\tintruder [label=\"Initial intruder knowledge\", style=filled,fillcolor=\"");
      printColor (INTRUDERCOLORH, INTRUDERCOLORL, INTRUDERCOLORS);
      eprintf ("\"];\n");
    }
  // eprintf ("\t};\n");

  // For debugging we might add more stuff: full dependencies
#ifdef DEBUG
  if (DEBUGL (3))
    {
      int r1;

      for (r1 = 0; r1 < sys->maxruns; r1++)
	{
	  if (sys->runs[r1].protocol != INTRUDER)
	    {
	      int e1;

	      for (e1 = 0; e1 < sys->runs[r1].step; e1++)
		{
		  int r2;

		  for (r2 = 0; r2 < sys->maxruns; r2++)
		    {
		      if (sys->runs[r2].protocol != INTRUDER)
			{
			  int e2;

			  for (e2 = 0; e2 < sys->runs[r2].step; e2++)
			    {
			      if (isDependEvent (r1, e1, r2, e2))
				{
				  eprintf
				    ("\tr%ii%i -> r%ii%i [color=grey];\n", r1,
				     e1, r2, e2);
				}
			    }
			}
		    }
		}
	    }
	}
    }
#endif

  // Intruder choices
  //drawIntruderChoices (sys);

  // Ranks
  //showRanks (sys, maxrank, ranks);

#ifdef DEBUG
  // Debug: print dependencies
  if (DEBUGL (3))
    {
      dependPrint ();
    }
#endif

  // clean memory
  free (ranks);			// ranks

  // close graph
  eprintf ("};\n\n");
}

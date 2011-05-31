/**
 * @file  C_mpmProg.cpp
 * @brief The internal 'program' API.
 *
 *  C_mpmProgs are overloaded classes that perform specific functions in the
 *  context of the Dijkstra system. The default contol system allows an
 *  external network-based program to drive the core dijkstra search. This
 *  process can be very slow on large-scale problems. To alleviate that, 
 *  mpm_programs can be written. These map directly to external dsh scripts
 *  but without any of the network overhead.
 */
/*
 * Original Author: Rudolph Pienaar
 * CVS Revision Info:
 *    $Author: rudolph $
 *    $Date: 2011/05/31 18:18:49 $
 *    $Revision: 1.16 $
 *
 * Copyright © 2011 The General Hospital Corporation (Boston, MA) "MGH"
 *
 * Terms and conditions for use, reproduction, distribution and contribution
 * are found in the 'FreeSurfer Software License Agreement' contained
 * in the file 'LICENSE' found in the FreeSurfer distribution, and here:
 *
 * https://surfer.nmr.mgh.harvard.edu/fswiki/FreeSurferSoftwareLicense
 *
 * Reporting: freesurfer@nmr.mgh.harvard.edu
 *
 */


#include "C_mpmProg.h"
#include "dijkstra.h"

#include "c_surface.h"
#include "c_label.h"
#include "c_vertex.h"
#include "unistd.h"

#include <sstream>

extern 	bool     	Gb_stdout;
extern	stringstream	Gsout;

//
//\\\---
// Base class issues --->>>
/////---
//

//-----------------------------------------------------------
// Base socket constructors/destructor, output dump routines
//-----------------------------------------------------------

void
C_mpmProg::debug_push(
    string      astr_currentProc)
{
    if (stackDepth_get() >= SSTACKDEPTH-1) {
        cout << "Current stackDepth:\t" << stackDepth_get()         << endl;
        cout << "stackdepth limit:\t"   << SSTACKDEPTH              << endl;
        for (int i=0; i<SSTACKDEPTH; i++)
            cout << "Stack depth: " << i << "\t" << str_proc_get(i) << endl;
        error("Out of str_proc stack depth");
    }
    stackDepth_set(stackDepth_get()+1);
    str_proc_set(stackDepth_get(), astr_currentProc);
}

void C_mpmProg::debug_pop() {
    mstackDepth--;
}

void
C_mpmProg::error(
    string      astr_msg        /* = ""  Error message */,
    int         code            /* = 1  Error ID  */)
{

    extern int errno;

    cerr << "\nFatal error encountered.\n";
    cerr << "\tC_mpmProg `" << mstr_name << "' (id: " << mid << ")\n";
    cerr << "\tCurrent function: " << mstr_obj << "::" << str_proc_get() << "\n";
    cerr << "\t" << astr_msg << "\n";
    perror("Error returned from system");
    print();
    cerr << "Throwing exception to (this) with code " << code << "\n\n";
    throw(this);
}

void
C_mpmProg::warn(
    string      astr_msg        /* = ""  Warning message */,
    int         code            /* = 1  Warning code  */)
{
    if (mwarnings) {
        cerr << "\nWarning.\n";
        cerr << "\tC_mpmProg `" << mstr_name << "' (id: " << mid << ")\n";
        cerr << "\tCurrent function: " << mstr_obj << "::" << str_proc_get() << "\n";
        cerr << "\t" << astr_msg << " (warning code: " << code << ")\n";
    }
}

void
C_mpmProg::function_trace(
    string      astr_msg        /* = ""  Trace message */,
    string      astr_separator  /* = ""  Separator message */)
{
    int                 i;
    string              str_tab                 = "";
    static string       str_objectName          = "";
    static string       str_funcName            = "";

    if (mverbosity>=mstackDepth) {
        cerr << astr_separator;
        for (i=0; i<mstackDepth; i++)
        str_tab += "    ";
        if (str_objectName != mstr_name) {
	    cerr << "\n" << mstr_obj << " `";
            cerr << str_name_get() << "' (id: " << mid << ")" << endl;
	}
        if (str_funcName != str_proc_get()) {
            cerr << "\n" << str_tab << "Current function: " << mstr_obj << "::";
            cerr << str_proc_get();
        }
        cerr << "\n" << str_tab << astr_msg << endl;
    }
    str_objectName      = str_name_get();
    str_funcName        = str_proc_get();
}

void
C_mpmProg::core_construct(
    string      astr_name       /* = "unnamed" */,
    int         a_id            /* = -1  */,
    int         a_verbosity     /* = 0  */,
    int         a_warnings      /* = 0  */,
    int         a_stackDepth    /* = 0  */,
    string      astr_proc       /* = "noproc" */)
{
    //
    // ARGS
    //  astr_name       in      name of object
    //  a_id            in      internal id number of object
    //  a_verbosity     in      verbosity level
    // a_warnings       in      warnings level
    // a_stackDepth     in      stack depth (debugging)
    // astr_proc        in      currently executing proc (debugging)
    //
    // DESC
    //  Common core statements for all constructors.
    //

    mstr_name   = astr_name;
    mid         = a_id;
    mverbosity  = a_verbosity;
    mwarnings   = a_warnings;
    mstackDepth = a_stackDepth;

    str_proc_set(stackDepth_get(), "no name");
    mstr_obj = "C_mpmProg";
}

C_mpmProg::C_mpmProg(
    s_env*      aps_env
){
    string      str_name        =       "Unnamed C_mpmProg";
    int         id              = 0;
    core_construct(str_name, id);
    mps_env                     = aps_env;
}

C_mpmProg::C_mpmProg(const C_mpmProg &C_mpmProg) {
    //
    // Copy constructor
    //

    debug_push("C_mpmProg (copy constructor)");
    error("Copy constructor not yet defined");
    debug_pop();
}

C_mpmProg & C_mpmProg::operator=(const C_mpmProg & C_mpmProg) {
    //
    // Overloaded (=) operator
    //

    debug_push("operator=");
    error("Overloaded operator= not yet defined");
    return *this;       // Never executed --
    debug_pop();
}

C_mpmProg::~C_mpmProg(void) {}

void
C_mpmProg::print() {
    cout << "object name:\t"    << mstr_name    << endl;
    cout << "object id:\t"      << mid          << endl;
    cout << "object type:\t"    << mstr_obj     << endl;
}

//
//\\\***
// C_mpmProg_NOP definitions ****>>>>
/////***
//

C_mpmProg_NOP::C_mpmProg_NOP(
    s_env*      aps_env) : C_mpmProg(aps_env)
{
    //
    // ARGS
    //
    // DESC
    // Basically a thin "fall-through" constructor to the base
    // class.
    //
    // PRECONDITIONS
    // o aps_env must be fully instantiated.
    //
    // HISTORY
    // 14 December 2009
    // o Initial design and coding.
    //

    debug_push("C_mpmProg_NOP");
    mstr_obj	= "C_mpmProg_NOP";

    msleepSeconds       = 5;

    debug_pop();
}

C_mpmProg_NOP::~C_mpmProg_NOP() {
    //
    // Destructor
    //

}

int
C_mpmProg_NOP::run() {
    //
    // DESC
    // Main entry to the actual 'run' core of the mpmProg
    //

    int         ret     = 1;

    debug_push("run");

    // Sleep for 'msleepSeconds'
    mps_env->pcsm_stdout->colprintf("Sleeping for interval (seconds)", "[ %d ]\n",
                                     msleepSeconds);
    sleep(msleepSeconds);

    debug_pop();
    return ret;
}

//
//\\\***
// C_mpmProg_pathFind definitions ****>>>>
/////***
//

C_mpmProg_pathFind::C_mpmProg_pathFind(
    s_env*      aps_env,
    int		amvertex_start,
    int		amvertex_end) : C_mpmProg(aps_env)
{
    //
    // ARGS
    //	aps_env			in		parent env structure
    //	amvertex_start		in		default start vertex
    //	amvertex_end		in		default end vertex -- if set to
    //						-1 the end vertex is assumed to
    //						mean the total number of
    //						vertices.
    //
    // DESC
    // Basically a thin "fall-through" constructor to the base
    // class.
    //
    // PRECONDITIONS
    // o aps_env must be fully instantiated.
    //
    // HISTORY
    // 08 July 2010
    // o Initial design and coding.
    //

    debug_push("C_mpmProg_pathFind");

    mstr_obj	= "C_mpmProg_pathFind";
    vertexStart_set(amvertex_start);
    mvertex_end                 = 0;
    mb_surfaceRipClear          = false;

    s_env_activeSurfaceSetIndex(mps_env, 0);
    mvertex_total		= mps_env->pMS_curvature->nvertices;

    if(	amvertex_start >= mvertex_total	||
        amvertex_start < 0 )
	vertexStart_set(0);
    else
	vertexStart_set(amvertex_start);

    if(	amvertex_end == -1 		|| 
        amvertex_end >= mvertex_total	|| 
        amvertex_end < 0)
	vertexEnd_set(mvertex_total-1);
    else
	vertexEnd_set(amvertex_end);

    debug_pop();
}

C_mpmProg_pathFind::~C_mpmProg_pathFind() {
    //
    // Destructor
    //

}

void
C_mpmProg_pathFind::print() {
  //
  // DESC
  // Simple info print method
  //

  C_mpmProg::print();
}

float
C_mpmProg_pathFind::cost_compute(
    int         a_start,
    int         a_end
) {
    //
    // ARGS
    // a_start          in              start vertex index
    // a_end            in              end vertex index
    //
    // DESC
    // Sets the main environment and then calls a dijkstra
    // computation.
    //

    int         ok;
    float       f_cost  = 0.0;
    static int  calls   = -1;
    char 	pch_buffer[65536];

    mps_env->startVertex = a_start;
    mps_env->endVertex   = a_end;

    calls++;
    ok = dijkstra(*mps_env);  // costs are written to vertex elements along
                              //+ the path in the MRIS structure.

    if(!ok) {
        fprintf(stderr, " fail ]\n");
        fprintf(stderr, "dijkstra failure, returning to system.\n");
        exit(1);
    }
    pULOUT(colsprintf(-50, 20, pch_buffer,
           "Marking (rip) path along vertices", "[ ok ]\n"));
//    s_env_activeSurfaceSetIndex(&st_env, (int) e_workingCurvature);
    f_cost      = surface_ripMark(*mps_env);
    if(mb_surfaceRipClear) {
        surface_ripClear(*mps_env, mb_surfaceRipClear);
    }
    return f_cost;
}

int
C_mpmProg_pathFind::run() {
    //
    // DESC
    // Main entry to the actual 'run' core of the pathFind module
    // -- for the most part, these innards comprise what used to be
    // the core of the original 'mris_pmake'.
    //

    float       f_cost  = 0.0;
    int         ret     = 1;
    string      str_optionsFQName       = "";
    string	str_patchFQName         = "";
    char 	pch_buffer[65536];

    debug_push("run");

    pSLOUT("PROCESSING: path\n");
    colsprintf(	mps_env->lw, mps_env->rw, pch_buffer, 
        	"Start->End vertices", "[ %d->%d ]\n",
        	mps_env->startVertex, mps_env->endVertex);
    if(Gb_stdout) printf("%s", pch_buffer); pULOUT(pch_buffer);

    f_cost            = cost_compute(mvertex_start, mvertex_end);

    colsprintf(	mps_env->lw, mps_env->rw, pch_buffer, 
        	"Total path cost", " [ %f ]\n", f_cost);
    if(Gb_stdout) printf("%s", pch_buffer); pULOUT(pch_buffer);
    pnRLOUT(lsprintf(mps_env->lw, pch_buffer, "%f", f_cost));

    if (mps_env->b_patchFile_save) {
        str_patchFQName =  mps_env->str_workingDir +
        		   mps_env->str_patchFileName;
        if (MRISwritePatch(mps_env->pMS_curvature,
                               (char*) str_patchFQName.c_str()) != NO_ERROR)
        	exit(1);
	pULOUT(colsprintf(mps_env->lw, mps_env->rw, pch_buffer, 
        	"Saving patch file...", " [ ok ]\n"));
    }

    if (mps_env->b_labelFile_save) {
        //label_save(st_env);
        void* pv_void = NULL;
        label_workingSurface_saveTo(*mps_env, vertex_ripFlagIsTrue, pv_void);
        if (mps_env->b_surfacesKeepInSync) {
            surface_workingToAux_ripTrueCopy(*mps_env);
            label_auxSurface_saveTo(*mps_env, vertex_ripFlagIsTrue, pv_void);
        }
	pULOUT(colsprintf(mps_env->lw, mps_env->rw, pch_buffer, 
        	"Labeling and saving all target vertices...", " [ ok ]\n"));
    }

    if (mps_env->b_surfacesClear) {
        s_env_activeSurfaceSetIndex(mps_env, (int) e_workingCurvature);
        surface_ripClear(*mps_env, true);
        if (mps_env->b_surfacesKeepInSync) {
            s_env_activeSurfaceSetIndex(mps_env, (int) e_auxillary);
            surface_ripClear(*mps_env, true);
            // NB!! Remember to set the "active" surface back
            // to the "working" surface. The dijkstra()
            // function operates on this "active" surface.
            s_env_activeSurfaceSetIndex(mps_env, (int) e_workingCurvature);
        }
	pULOUT(colsprintf(mps_env->lw, mps_env->rw, pch_buffer, 
        	"Clearing (rip) path along vertices...", " [ ok ]\n"));
    }
    fflush(stdout);
    debug_pop();
    return ret;
}

//
//\\\***
// C_mpmProg_autodijk definitions ****>>>>
/////***
//

C_mpmProg_autodijk::C_mpmProg_autodijk(
    s_env*      aps_env) : C_mpmProg(aps_env)
{
    //
    // ARGS
    //
    // DESC
    // Basically a thin "fall-through" constructor to the base
    // class.
    //
    // PRECONDITIONS
    // o aps_env must be fully instantiated.
    //
    // HISTORY
    // 17 November 2009
    // o Initial design and coding.
    //

    debug_push("C_mpmProg_autodijk");

    mstr_obj	= "C_mpmProg_autodijk";
    mvertex_polar               = 0;
    mvertex_start               = 0;
    mvertex_step                = 1;
    mvertex_end                 = 0;
    m_costFunctionIndex         = 0;
    mb_performExhaustive        = false;
    mb_surfaceRipClear          = false;
    mprogressIter               = 100;

    s_env_activeSurfaceSetIndex(mps_env, 0);
    mstr_costFileName   = mps_env->str_costCurvFile;
    mstr_costFullPath   = mps_env->str_workingDir + "/" + mstr_costFileName;
    mvertex_end         = mps_env->pMS_curvature->nvertices;
    mvertex_total       = mvertex_end;
    mpf_cost            = new float[mvertex_total];
    for(int i=0; i<mvertex_end; i++)
        mpf_cost[i]     = 0.0;

    debug_pop();
}

C_mpmProg_autodijk::~C_mpmProg_autodijk() {
    //
    // Destructor
    //

    delete [] mpf_cost;

}

/*!
  \fn C_mpmProg_autodijk::CURV_fileWrite()
  \brief Write a FreeSurfer curvature array to a file
  \return If curvature file is successfully written, return e_OK, else return e_WRITEACCESSERROR.
*/
e_FILEACCESS
C_mpmProg_autodijk::CURV_fileWrite()
{
  FILE* FP_curv;
  int   i;
  char  pch_readMessage[65536];

  if((FP_curv = fopen(mstr_costFullPath.c_str(), "w")) == NULL)
    return(e_WRITEACCESSERROR);
  fwrite3(NEW_VERSION_MAGIC_NUMBER, FP_curv);
  fwriteInt(mvertex_total, FP_curv);
  fwriteInt(mps_env->pMS_curvature->nfaces, FP_curv);
  fwriteInt((int)1, FP_curv);
  sprintf(pch_readMessage, "Writing %s", mstr_costFileName.c_str());
  for(i=0; i<mvertex_total; i++) {
    CURV_arrayProgress_print(mvertex_total, i, pch_readMessage);
    fwriteFloat(mpf_cost[i], FP_curv);
  }
  fclose(FP_curv);
  return(e_OK);
}


void
C_mpmProg_autodijk::print() {
  //
  // DESC
  // Simple info print method
  //

  C_mpmProg::print();
}

float
C_mpmProg_autodijk::cost_compute(
    int         a_start,
    int         a_end
) {
    //
    // ARGS
    // a_start          in              start vertex index
    // a_end            in              end vertex index
    //
    // DESC
    // Sets the main environment and then calls a dijkstra
    // computation.
    //

    int         ok;
    float       f_cost  = 0.0;
    static int  calls   = -1;

    mps_env->startVertex = a_start;
    mps_env->endVertex   = a_end;

    calls++;
    if(!(calls % mprogressIter)) {
        mps_env->pcsm_stdout->colprintf(
                            "path and dijkstra cost", "[ %d -> %d/%d = ",
                            mps_env->startVertex,
                            mps_env->endVertex,
                            mvertex_total);
    }
    ok = dijkstra(*mps_env);  // costs are written to vertex elements along
                              //+ the path in the MRIS structure.

    if(!ok) {
        fprintf(stderr, " fail ]\n");
        fprintf(stderr, "dijkstra failure, returning to system.\n");
        exit(1);
    }
    f_cost      = surface_ripMark(*mps_env);
    if(!(calls % mprogressIter)) {
        if(Gb_stdout) printf(" %f ]\n", f_cost);
    }
    if(mb_surfaceRipClear) {
        surface_ripClear(*mps_env, mb_surfaceRipClear);
    }
    return f_cost;
}

int
C_mpmProg_autodijk::run() {
    //
    // DESC
    // Main entry to the actual 'run' core of the mpmProg
    //

    float       f_cost  = 0.0;
    int         ret     = 1;

    debug_push("run");

    if(mb_performExhaustive) {
      // Calculate the costs from polar to every other vertex in
      // mesh *explicitly*. DO NOT USE -- ONLY FOR DEBUGGING!! A
      // single sweep of the dijkstra from polar->polar will
      // have the same result in seconds... the exhaustive search
      // can take multiple hours.
      for(int v = mvertex_start; v < mvertex_end; v+=mvertex_step) {
          f_cost        = cost_compute(mvertex_polar, v);
          mpf_cost[v]   = f_cost;
      }
    } else {
      f_cost            = cost_compute(mvertex_polar, mvertex_polar);
      for(int v = 0; v < mvertex_end; v++)
        mpf_cost[v]     = mps_env->pMS_active->vertices[v].val;
    }
    // Write the cost curv file
    if((ret=CURV_fileWrite()) != e_OK)
        error("I could not save the cost curv file.");

    debug_pop();
    return ret;
}

//
//\\\***
// C_mpmProg_autodijk_fast definitions ****>>>>
/////***
//

///
/// Constructor
/// \param apps_env Freesurfer environment
///
C_mpmProg_autodijk_fast::C_mpmProg_autodijk_fast(s_env* aps_env) :
    C_mpmProg_autodijk(aps_env)
{
    debug_push("C_mpmProg_autodijk_fast");
    mstr_obj	= "C_mpmProg_autodijk_fast";

    debug_pop();
}

///
/// Destructor
///
C_mpmProg_autodijk_fast::~C_mpmProg_autodijk_fast()
{
}

///
/// Convert the freesurfer environment MRI surface representation into
/// a representation that is formatted for consumption by the OpenCL
/// algorithm
/// \param graph Pointer to the graph data to generate
///
void C_mpmProg_autodijk_fast::genOpenCLGraphRepresentation(GraphData *graph)
{
    MRIS*           surf = mps_env->pMS_active;
//    s_iterInfo      st_iterInfo;
//    bool            b_relNextReference  = true;

    cout << "Converting graph to fast representation... " << endl;
    // Allocate memory for each of the vertices
    graph->vertexCount = surf->nvertices;
    graph->vertexArray = new int[surf->nvertices];

    // Determine the size of the edge and weight buffers
    int numEdges = 0;
    for(int i = 0; i < surf->nvertices; i++)
    {
        numEdges += surf->vertices[i].vnum;
    }

    // Allocate memory for the edge and weight arrays
    graph->edgeCount = numEdges;
    graph->edgeArray = new int[numEdges];
    graph->weightArray = new float[numEdges];

    // Now create the edge list and compute all of the edge weights
    int curEdgeIndex = 0;
    for(int i = 0; i < surf->nvertices; i++)
    {
        VERTEX *curVertex = &surf->vertices[i];

        // Assign the edge index for this vertex
        graph->vertexArray[i] = curEdgeIndex;

        for(int j = 0; j < curVertex->vnum; j++)
        {
	    int ij;
            // index for the neighbor
            graph->edgeArray[curEdgeIndex] = curVertex->v[j];
	    ij = curVertex->v[j];

            // Compute the weight for this edge
//            float cost = mps_env->costFunc_do(*mps_env, &st_iterInfo, i, j, b_relNextReference);
	    float cost = s_env_edgeCostFind(*mps_env, i, ij);	    
            graph->weightArray[curEdgeIndex] = cost;

            curEdgeIndex++;
        }
    }
}
///
/// Run method - the main entry to the 'run' core of the mpmProg
///
int C_mpmProg_autodijk_fast::run()
{
    int         ret     = 1;

    debug_push("run");

    GraphData graph;
    genOpenCLGraphRepresentation(&graph);

    // Create the list of source indices, here we only ever do one source
    int sourceVertices = mvertex_polar;

    // Allocate array for results (the algorithm will write out a result
    // for each vertex in the graph)
    float *results = new float[graph.vertexCount];

    // Perform a Dijkstra run
    cout << "Running Dijkstra's algorithm..." << endl;

#ifdef FS_OPENCL
    // We have implemented various flavors of the Dijkstra algorithm that runs with
    // OpenCL using either CPU-only, GPU+CPU, Multi GPU, or Multi GPU + CPU.  Based
    // on what set of devices is available, this function will choose which implementation
    // to use.  This version of Dijkstra selects which devices to use automatically.

    // If compiled with OpenCL support, run the OpenCL version of the algorithm
    runDijkstraOpenCL(&graph, &sourceVertices, results, 1);
#else
    // If not compiled with OpenCL, run the reference version of the algorithm
    runDijkstraRef(&graph, &sourceVertices, results, 1);
#endif

    cout << "Done." << endl;

    // Save back the resulting costs
    for(int v = mvertex_start; v < mvertex_end; v+=mvertex_step)
    {
        mpf_cost[v] = results[v];
    }

    // Free temporary results buffer
    delete [] results;

    // Free up memory from allocation of graph data
    delete [] graph.vertexArray;
    delete [] graph.edgeArray;
    delete [] graph.weightArray;

    // Write the cost curv file
    if((ret=CURV_fileWrite()) != e_OK)
        error("I could not save the cost curv file.");

    debug_pop();
    return ret;
}

//
//\\\***
// C_mpmProg_ROI definitions ****>>>>
/////***
//

C_mpmProg_ROI::C_mpmProg_ROI(
    s_env*      aps_env, 
    int 	amvertex_center, 
    float 	af_radius) : C_mpmProg(aps_env)
{
    //
    // ARGS
    //
    // DESC
    // Basically a thin "fall-through" constructor to the base
    // class.
    //
    // PRECONDITIONS
    // o aps_env must be fully instantiated.
    //
    // HISTORY
    // 05 May 2011
    // o Initial design and coding.
    //

    debug_push("C_mpmProg_ROI");
    mstr_obj	        = "C_mpmProg_ROI";

    mvertex_center      = amvertex_center;
    mf_radius           = af_radius;

    debug_pop();
}

C_mpmProg_ROI::~C_mpmProg_ROI() {
    //
    // Destructor
    //

}

int
C_mpmProg_ROI::run() {
    //
    // DESC
    // Main entry to the actual 'run' core of the mpmProg
    //

    int         ret     = 1;

    debug_push("run");


    debug_pop();
    return ret;
}

/* eof */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

#include <assert.h>

#include "macros.h"
#include "error.h"
#include "diag.h"
#include "proto.h"
#include "mrisurf.h"
#include "mri.h"
#include "macros.h"
#include "version.h"

#define		STRBUF		65536
#define 	MAX_FILES 	1000
#define 	CE( x )		fprintf(stdout, ( x ))
#define 	START_i  	3

// Calculations performed on the curvature surface
typedef enum _secondOrderType {
	e_Raw,			// "Raw" (native) curvature - no calcuation
	e_Gaussian, 		// Gaussian curvature
	e_Mean,			// Mean curvature
	e_Normal,		// Normalised curvature
	e_Scaled,		// "Raw" scaled curvature
	e_ScaledTrans		// Scaled and translated curvature
} e_secondOrderType;

// Output file prefixes
typedef enum _OFSP {
	e_None,			// No prefix
	e_Partial,		// A partial prefix (does not include the stem)
	e_Full,			// Full prefix - including stem
} e_OFSP;

static char vcid[] = 
	"$Id: mris_curvature_stats.c,v 1.15 2005/09/30 15:37:06 rudolph Exp $";

int 		main(int argc, char *argv[]) ;

static int  	get_option(int argc, char *argv[]) ;
static void 	usage_exit(void) ;
static void 	print_usage(void) ;
static void 	print_help(void) ;
static void 	print_version(void) ;
int  		comp(const void *p, const void *q);    // Compare p and q for qsort()
void		histogram_wrapper(
			MRIS*			amris,
			e_secondOrderType	aesot
		);
void		histogram_create(
			MRIS*			amris_curvature,
			float			af_minCurv,
			double			af_binSize,
			int			abins,
			float*			apf_histogram
		);
void		OFSP_create(
			char*			apch_prefix,
			char*			apch_suffix,
			char*			apch_curv,
			e_OFSP			ae_OFSP
		);
void 		secondOrderParams_print(
			MRIS*			apmris,
			e_secondOrderType	aesot,
			int			ai
		);
void		outputFileNames_create(
			char*			apch_curv
		);
void		outputFiles_open(void);
void		outputFiles_close(void);
int		MRISminMaxCurvatures(
			MRI_SURFACE*		apmris,
			float*			apf_min,
			float*			apf_max
		);
int		MRISminMaxVertIndices(
			MRI_SURFACE*		apmris,
			int*			ap_vertexMin,
			int*			ap_vertexMax,
			float			af_min,
			float			af_max
		);
int		MRISvertexCurvature_set(
			MRI_SURFACE*		apmris,
			int			aindex,
			float			af_val
		);

char*		Progname ;
char*		hemi;

static int 	navgs 			= 0 ;
static int 	normalize_flag 		= 0 ;
static int 	condition_no 		= 0 ;
static int 	stat_flag 		= 0 ;
static char*	label_name 		= NULL ;
static char*	output_fname 		= NULL ;
static char	surf_name[STRBUF];

// Additional global variables (prefixed by 'G') added by RP.
//	Flags are prefixed with Gb_ and are used to track
//	user spec'd command line flags.
static int 	Gb_minMaxShow		= 0;
static int	Gb_histogram		= 0;
static int	Gb_histogramPercent	= 0;
static int	Gb_binSizeOverride	= 0;
static double	Gf_binSize		= 0.;
static int	Gb_histStartOverride	= 0;
static float	Gf_histStart		= 0.;
static int	Gb_histEndOverride	= 0;
static float	Gf_histEnd		= 0.;
static int	Gb_gaussianAndMean	= 0;
static int	Gb_output2File		= 0;
static int	Gb_scale		= 0;
static int	Gb_scaleMin		= 0;
static int	Gb_scaleMax		= 0;
static int	Gb_zeroVertex		= 0;
static int	G_zeroVertex		= 0;
static int 	G_nbrs 			= 2;
static int	G_bins			= 1;
static int	Gb_maxUlps		= 0;
static int	G_maxUlps		= 0;

// All possible output file name and suffixes
static char	Gpch_log[STRBUF];
static char	Gpch_logS[]		= "log";
static FILE*	GpFILE_log		= NULL;
static char	Gpch_allLog[STRBUF];		
static char	Gpch_allLogS[]		= "log";
static FILE*	GpFILE_allLog		= NULL;
static char	Gpch_rawHist[STRBUF];
static char	Gpch_rawHistS[]		= "raw.hist";
static FILE*	GpFILE_rawHist		= NULL;
static char	Gpch_normCurv[STRBUF];
static char	Gpch_normHist[STRBUF];
static char	Gpch_normHistS[]	= "norm.hist";
static FILE*	GpFILE_normHist		= NULL;
static char	Gpch_normCurv[STRBUF];
static char	Gpch_normCurvS[]	= "norm.crv";
static char	Gpch_KHist[STRBUF];
static char	Gpch_KHistS[]		= "K.hist";
static FILE*	GpFILE_KHist		= NULL;
static char	Gpch_KCurv[STRBUF];
static char	Gpch_KCurvS[]		= "K.crv";
static char	Gpch_HHist[STRBUF];
static char	Gpch_HHistS[]		= "H.hist";
static FILE*	GpFILE_HHist		= NULL;
static char	Gpch_HCurv[STRBUF];
static char	Gpch_HCurvS[]		= "H.crv";
static char	Gpch_scaledHist[STRBUF];
static char	Gpch_scaledHistS[]	= "scaled.hist";
static FILE*	GpFILE_scaledHist	= NULL;
static char	Gpch_scaledCurv[STRBUF];
static char	Gpch_scaledCurvS[]	= "scaled.crv";
		
// These are used for tabular output
const int	G_leftCols		= 20;
const int	G_rightCols		= 20;

// Mean / sigma tracking and scaling
static double 	Gpf_means[MAX_FILES] ;
static double	Gf_mean			= 0.;
static double	Gf_sigma		= 0.;
static double	Gf_n			= 0.;
static double	Gf_total		= 0.;
static double	Gf_total_sq		= 0.;	
static double	Gf_scaleFactor		= 1.;
static double	Gf_scaleMin		= 0.;
static double	Gf_scaleMax		= 0.;

int
main(int argc, char *argv[])
{
  char         	**av, fname[STRBUF], *sdir ;
  char         	*subject_name, *curv_fname ;
  int          	ac, nargs, i ;
  MRI_SURFACE  	*mris ;

  char		pch_text[65536];
  int		vmin				= -1;
  int		vmax				= -1;

  /* rkt: check for and handle version tag */
  nargs = handle_version_option (argc, argv, 
	"$Id: mris_curvature_stats.c,v 1.15 2005/09/30 15:37:06 rudolph Exp $", "$Name:  $");
  if (nargs && argc - nargs == 1)
    exit (0);
  argc -= nargs;

  Progname = argv[0] ;
  ErrorInit(NULL, NULL, NULL) ;
  DiagInit(NULL, NULL, NULL) ;

  sdir = getenv("SUBJECTS_DIR") ;
  if (!sdir)
    ErrorExit(ERROR_BADPARM, "%s: no SUBJECTS_DIR in environment.\n",Progname);
  ac = argc ;
  av = argv ;
  strcpy(surf_name, "-x");
  for ( ; argc > 1 && ISOPTION(*argv[1]) ; argc--, argv++)
  {
    nargs = get_option(argc, argv) ;
    argc -= nargs ;
    argv += nargs ;
  }
  if(!strcmp(surf_name, "-x"))
    strcpy(surf_name, "orig") ;

  if (argc < 4)
    usage_exit() ;

  /* parameters are 
     1. subject name
     2. hemisphere
  */
  subject_name = argv[1] ; hemi = argv[2] ;
  sprintf(fname, "%s/%s/surf/%s.%s", sdir, subject_name, hemi, surf_name) ;
  printf("Reading surface from %s...\n", fname);
  mris = MRISread(fname) ;
  if (!mris)
    ErrorExit(ERROR_NOFILE, "%s: could not read surface file %s",
              Progname, fname) ;


  if (label_name)
  {
    LABEL *area ;
    area = LabelRead(subject_name, label_name) ;
    if (!area)
      ErrorExit(ERROR_NOFILE, "%s: could not read label file %s",
                Progname, label_name) ;
    LabelRipRestOfSurface(area, mris) ;
    LabelFree(&area) ;
  }
  if (label_name)
    fprintf(stdout, "%s: ", label_name) ;

  // Process all the command-line spec'd curvature files
  for (Gf_n= Gf_total_sq = Gf_total = 0.0, i = START_i ; i < argc ; i++)
  {
    curv_fname = argv[i] ;
    
    outputFiles_close();
    outputFileNames_create(curv_fname);
    outputFiles_open();

    if (MRISreadCurvatureFile(mris, curv_fname) != NO_ERROR)
      ErrorExit(ERROR_BADFILE,"%s: could not read curvature file %s.\n",
                Progname, curv_fname);

    if(Gb_zeroVertex)
	MRISvertexCurvature_set(mris, G_zeroVertex, 0);

    if(Gb_scale) {
	MRISscaleCurvature(mris, Gf_scaleFactor);
      	if(Gpch_scaledCurv) MRISwriteCurvature(mris, Gpch_scaledCurv);
    }

    if(Gb_scaleMin && Gb_scaleMax) {
	MRISscaleCurvatures(mris, Gf_scaleMin, Gf_scaleMax);
      	if(Gpch_scaledCurv) MRISwriteCurvature(mris, Gpch_scaledCurv);
    }

    if (normalize_flag) {
	MRISnormalizeCurvature(mris);
	if(Gpch_normCurv) MRISwriteCurvature(mris, Gpch_normCurv);
    }

    MRISaverageCurvatures(mris, navgs) ;
    Gf_mean = MRIScomputeAverageCurvature(mris, &Gf_sigma) ;
    sprintf(pch_text, "Raw Curvature (using '%s.%s'):", hemi, curv_fname);
    fprintf(stdout, "%-50s", pch_text);
    fprintf(stdout, "%20.4f +- %2.4f\n", Gf_mean, Gf_sigma) ;
    if(GpFILE_log) 
	fprintf(GpFILE_allLog, "mean/sigma = %20.4f +- %2.4f\n", Gf_mean, Gf_sigma);

    if(Gb_minMaxShow) {
	MRISminMaxVertIndices(	mris, &vmin, &vmax, 
				mris->min_curv, mris->max_curv);
	fprintf(stdout, 
	     	"%*s%20.6f\tvertex = %d\n%*s%20.6f\tvertex = %d\n",
	    	G_leftCols, "min = ", mris->min_curv, vmin,
	    	G_leftCols, "max = ", mris->max_curv, vmax);
	if(GpFILE_allLog)
	    fprintf(GpFILE_allLog, "min = %f\nmax = %f\n", 
		mris->min_curv, mris->max_curv);
	if(Gb_histogram) histogram_wrapper(mris, e_Raw);
    }

    Gpf_means[i-START_i] = Gf_mean ;
    Gf_total += Gf_mean ; Gf_total_sq += Gf_mean*Gf_mean ;
    Gf_n+= 1.0 ;
  }

    // Should we calculate Gaussian and mean? This is a surface-based
    //	calculation, and this does not depend on the curvature processing
    //	loop.
    if(Gb_gaussianAndMean) {
	fprintf(stdout, 
	"\tCalculating second fundamental form for Gaussian, Mean...\n");

	/* Gaussian and Mean curvature calculations */
	MRISsetNeighborhoodSize(mris, G_nbrs);
	MRIScomputeSecondFundamentalForm(mris);
	secondOrderParams_print(mris, e_Gaussian, i);
      	if(Gpch_KCurv) MRISwriteCurvature(mris, Gpch_KCurv);
	secondOrderParams_print(mris, e_Mean,	  i);
      	if(Gpch_HCurv) MRISwriteCurvature(mris, Gpch_HCurv);
    }


  if (Gf_n> 1.8)
  {
    Gf_mean = Gf_total / Gf_n;
    Gf_sigma = sqrt(Gf_total_sq/Gf_n- Gf_mean*Gf_mean) ;
    fprintf(stdout, "\nMean across %d curvatures: %8.4e +- %8.4e\n",
            (int) Gf_n, Gf_mean, Gf_sigma) ;
  }
  //else
  //  Gf_mean = Gf_sigma = 0.0 ;
  

  MRISfree(&mris) ;
  if (output_fname)
  {
    // This code dumps only the *final* mean/sigma values to the summary
    //	log file. For cases where only 1 curvature file has been processed,
    //	the mean/sigma of this file is written. For multiple files, the
    //	mean/sigma over *all* of the files is written.
    if (label_name)
      fprintf(GpFILE_log, "%s: ", label_name) ;
    fprintf(GpFILE_log, "%8.4e +- %8.4e\n", Gf_mean, Gf_sigma) ;
  }
  outputFiles_close();
  exit(0) ;
  return(0) ;  /* for ansi */
}

void secondOrderParams_print(
	MRIS*			apmris,
	e_secondOrderType	aesot,
	int			ai
) {
    //
    // PRECONDITIONS
    //	o amris must have been prepared with a call to
    //	  MRIScomputeSecondFundamentalForm(...)
    //
    // POSTCONDITIONS
    // 	o Depending on <esot>, data is printed to stdout (and
    //	  output file, if spec'd).
    //

    char	pch_out[65536];
    char	pch_text[65536];
    float	f_max			= 0.;
    float	f_min			= 0.;
    float	f_maxExplicit		= 0.;
    float	f_minExplicit		= 0.;
    int		vmax			= -1;
    int		vmin			= -1;

    if(Gb_zeroVertex)
	MRISvertexCurvature_set(apmris, G_zeroVertex, 0);
    switch(aesot) {
	case e_Gaussian:
    	    MRISuseGaussianCurvature(apmris);
	    sprintf(pch_out, "Gaussian");
	    f_min	= apmris->Kmin;
	    f_max	= apmris->Kmax;
	    MRISminMaxCurvatures(apmris, &f_minExplicit, &f_maxExplicit);
	    if(f_min != f_minExplicit) {
	    	printf("Lookup   min: %f\n", f_min);
	    	printf("Explicit min: %f\n", f_minExplicit);
		f_min = f_minExplicit;
		apmris->Kmin 		= f_minExplicit;
		apmris->min_curv	= f_minExplicit;
	    }
	    if(f_max != f_maxExplicit) {
	    	printf("Lookup   max: %f\n", f_max);
	    	printf("Explicit max: %f\n", f_maxExplicit);
		f_max = f_maxExplicit;
		apmris->Kmax 		= f_maxExplicit;
		apmris->max_curv	= f_maxExplicit;
	    }
    	    MRISminMaxVertIndices(apmris, &vmin, &vmax, f_min, f_max);
	break;
	case e_Mean:
	    MRISuseMeanCurvature(apmris);
	    sprintf(pch_out, "Mean");
	    f_min	= apmris->Hmin;
	    f_max	= apmris->Hmax;
	    MRISminMaxCurvatures(apmris, &f_minExplicit, &f_maxExplicit);
	    if(f_min != f_minExplicit) {
	    	printf("Lookup   min: %f\n", f_min);
	    	printf("Explicit min: %f\n", f_minExplicit);
		f_min = f_minExplicit;
		apmris->Hmin 		= f_minExplicit;
		apmris->min_curv	= f_minExplicit;
	    }
	    if(f_max != f_maxExplicit) {
	    	printf("Lookup   max: %f\n", f_max);
	    	printf("Explicit max: %f\n", f_maxExplicit);
		f_max = f_maxExplicit;
		apmris->Hmax 		= f_maxExplicit;
		apmris->max_curv	= f_maxExplicit;
	    }
	    MRISminMaxVertIndices(apmris, &vmin, &vmax, f_min, f_max);
	break;
	case e_Raw:
	case e_Normal:
	case e_Scaled:
	case e_ScaledTrans:
	break;
    }
    
    Gf_mean = MRIScomputeAverageCurvature(apmris, &Gf_sigma);
    sprintf(pch_text, "%s Curvature (using '%s.%s'):", 
			pch_out, hemi, surf_name);
    fprintf(stdout, "%-50s", pch_text);
    fprintf(stdout, "%20.4f +- %2.4f\n", Gf_mean, Gf_sigma);
    if(GpFILE_allLog)
	fprintf(GpFILE_allLog, "mean/sigma = %20.4f +- %2.4f\n", Gf_mean, Gf_sigma);
    if(Gb_minMaxShow) {
	fprintf(stdout, 
	     	"%*s%20.6f\tvertex = %d\n%*s%20.6f\tvertex = %d\n",
	G_leftCols, "min = ", f_min, vmin,
	G_leftCols, "max = ", f_max, vmax);
	if(GpFILE_allLog)
	    fprintf(GpFILE_allLog, "min = %f\nmax = %f\n",
			f_min, f_max);
    }

    if(Gb_histogram) histogram_wrapper(apmris, aesot);
    Gpf_means[ai-START_i] 	= Gf_mean;
    Gf_total 			+= Gf_mean; 
    Gf_total_sq 		+= Gf_mean*Gf_mean ;
    Gf_n			+= 1.0;

}

int
MRISvertexCurvature_set(
	MRI_SURFACE*		apmris,
	int			aindex,
	float			af_val
) {
    //
    // PRECONDITIONS
    //  o <af_val> is typically zero.
    //  o MRIScomputeSecondFundamentalForm() should have been called
    //	  if mean/gaussian values are important.
    //
    // POSTCONDITIONS
    //  o The curvature of the vertex at aindex is
    //    set to <af_val>. The Gaussian and Mean are also set
    //    to <af_val>.
    // 	o apmris max_curv and min_curv values are recomputed across
    //	  the "raw", gaussian, and mean curvatures.
    // 

    VERTEX*	pvertex;
    int		vno;
    float	f_maxCurv	= 0.;    float  f_minCurv	= 0.;
    float 	f_maxCurvK	= 0.; 	 float  f_minCurvK	= 0.;
    float 	f_maxCurvH	= 0.;    float  f_minCurvH	= 0.;

    if(aindex > apmris->nvertices)
	ErrorExit(ERROR_SIZE, "%s: target vertex is out of range.", Progname);

    apmris->vertices[aindex].curv	= af_val;
    apmris->vertices[aindex].K		= af_val;
    apmris->vertices[aindex].H		= af_val;

    f_maxCurv  = f_minCurv  = apmris->vertices[0].curv;
    f_maxCurvK = f_minCurvK = apmris->vertices[0].K;
    f_maxCurvH = f_maxCurvH = apmris->vertices[0].H;
    for (vno = 0 ; vno < apmris->nvertices ; vno++) {
	pvertex = &apmris->vertices[vno] ;
      	if (pvertex->ripflag)
            continue;
      	if(pvertex->curv > f_maxCurv)  f_maxCurv  = pvertex->curv;
	if(pvertex->curv < f_minCurv)  f_minCurv  = pvertex->curv;
      	if(pvertex->K    > f_maxCurvK) f_maxCurvK = pvertex->K;
	if(pvertex->K    < f_minCurvK) f_minCurvK = pvertex->K;
      	if(pvertex->H    > f_maxCurvH) f_maxCurvH = pvertex->H;
	if(pvertex->H    < f_minCurvH) f_minCurvH = pvertex->H;
    }
    apmris->max_curv	= f_maxCurv;
    apmris->min_curv	= f_minCurv;
    apmris->Kmax	= f_maxCurvK;
    apmris->Kmin	= f_minCurvK;
    apmris->Hmax	= f_maxCurvH;
    apmris->Hmin	= f_minCurvH;
    
    return(NO_ERROR);
}

int
MRISminMaxCurvatures(
	MRI_SURFACE*		apmris,
	float*			apf_min,
	float*			apf_max

) {
    //
    // PRECONDITIONS
    //  o apmris should have its curvature fields applicably set (i.e. Gaussian
    //	  and Mean usage has already been called).
    //
    // POSTCONDITIONS
    //	o Return the min and max curvatures in apf_min and apf_max respectively.
    //

    VERTEX*	pvertex;
    int		vno;

    float	f_min	= 1e6;
    float	f_max	= -1e6;

    for (vno = 0 ; vno < apmris->nvertices ; vno++) {
	pvertex = &apmris->vertices[vno] ;
	if(!vno) { f_min = f_max = pvertex->curv; }
      	if (pvertex->ripflag)
            continue;
      	if(pvertex->curv > f_max)
	    f_max = pvertex->curv;
	if(pvertex->curv < f_min)
	    f_min = pvertex->curv;
    }
    *apf_max = f_max; 
    *apf_min = f_min;
    return(NO_ERROR);
}

int
MRISminMaxVertIndices(
	MRI_SURFACE*		apmris,
	int*			ap_vertexMin,
	int*			ap_vertexMax,
	float			af_min,
	float			af_max
) {
    //
    // PRECONDITIONS
    //  o apmris should already have its max_curv and min_curv fields
    //	  defined.
    //  o for second order min/max, make sure that apmris has its Kmin/Kmax
    //	  and Hmin/Hmax fields defined.
    //
    // POSTCONDITIONS
    //	o Return the actual vertex index number corresponding to the
    //	  minimum and maximum curvature.
    //

    VERTEX*	pvertex;
    int		vno;

    for (vno = 0 ; vno < apmris->nvertices ; vno++) {
	pvertex = &apmris->vertices[vno] ;
      	if (pvertex->ripflag)
            continue;
      	if(pvertex->curv == af_min)
	    *ap_vertexMin = vno;
	if(pvertex->curv == af_max)
	    *ap_vertexMax = vno;
    }

//     printf("%f\n%f\n", 
// 		apmris->vertices[*ap_vertexMin].curv, 
// 		apmris->vertices[*ap_vertexMax].curv); 

    return(NO_ERROR);

}

int  
MRISscaleCurvature(
	MRI_SURFACE*	apmris, 
	float 		af_scale) {
    //
    // POSTCONDITIONS
    //	o Each curvature value in apmris is simply scaled by <af_scale>
    //

    VERTEX*	pvertex;
    int		vno;
    int		vtotal;
    double	f_mean;   

    for(f_mean = 0.0, vtotal = vno = 0 ; vno < apmris->nvertices ; vno++) {
      pvertex = &apmris->vertices[vno] ;
      if (pvertex->ripflag)
        continue ;
      vtotal++ ;
      f_mean += pvertex->curv ;
    }
    f_mean /= (double)vtotal ;

    for (vno = 0 ; vno < apmris->nvertices ; vno++) {
	pvertex = &apmris->vertices[vno] ;
      	if (pvertex->ripflag)
            continue;
      	pvertex->curv = (pvertex->curv - f_mean) * af_scale + f_mean ;
    }
    return(NO_ERROR);
}

void
OFSP_create(
	char*		apch_prefix,
	char*		apch_suffix,
	char*		apch_curv,
	e_OFSP		ae_OFSP
) {
    //
    // PRECONDITIONS
    //	o pch_prefix / pch_suffix must be large enough to contain
    //	  required text info.
    //
    // POSTCONDITIONS
    //  o a full pch_prefix is composed of:
    //		<output_fname>.<hemi>.<surface>.<pch_suffix>
    //  o a partial pch_prefix is compsed of:
    //		<output_fname>.<hemi>.<surface>.<pch_suffix>
    //

    switch(ae_OFSP) {
	case e_None:
    	    sprintf(apch_prefix, output_fname);	
	    break;
	case e_Partial:
    	    sprintf(apch_prefix, "%s.%s.%s.%s",
		hemi,
		surf_name,
		apch_curv,
		apch_suffix
	    );    
	    break;
	case e_Full:
    	    sprintf(apch_prefix, "%s.%s.%s.%s.%s",
		output_fname,
		hemi,
		surf_name,
		apch_curv,
		apch_suffix
	    );    
	    break;
    }
}

void
histogram_wrapper(
    	MRIS*			amris,
	e_secondOrderType	aesot
) {
    //
    // PRECONDITIONS
    //	o amris_curvature must be valid and populated with curvature values
    //	  of correct type (i.e. normalised, Gaussian/Mean, scaled)
    //	o apf_histogram memory must be handled (i.e. created/freed) by the
    //	  caller.
    //
    // POSTCONDITIONS
    // 	o max/min_curvatures and binSize are checked for validity.
    //	o histogram memory is created / freed.
    //  o histogram_create() is called.
    //

    int		i;
    float*	pf_histogram;
    float	f_maxCurv	= amris->max_curv;
    float	f_minCurv	= amris->min_curv;
    double 	f_binSize	= 0.;
    int		b_error		= 0.;
 
    if(Gb_binSizeOverride)
	f_binSize	= Gf_binSize;

    if(Gb_histStartOverride)
	f_minCurv	= Gf_histStart;

    if(f_minCurv < amris->min_curv)
	f_minCurv	= amris->min_curv;

    if(Gb_histEndOverride) 
	f_maxCurv	= Gf_histEnd;

    if(f_maxCurv > amris->max_curv)
	f_maxCurv	= amris->max_curv;

    f_binSize		= ((double)f_maxCurv - (double)f_minCurv) / (double)G_bins;
    if(f_maxCurv <= f_minCurv)
	ErrorExit(ERROR_SIZE, "%s: f_maxCurv < f_minCurv.",
			Progname);

    b_error =  	(long)(((double)f_minCurv+(double)G_bins*(double)f_binSize)*1e10) > 
		(long)(((double)f_maxCurv)*1e10);

    if(b_error)
	ErrorExit(ERROR_SIZE, "%s: Invalid <binSize> and <bins> combination",
			Progname);

    pf_histogram = calloc(G_bins, sizeof(float));
    histogram_create(	amris,
			f_minCurv,
			f_binSize,
			G_bins,
			pf_histogram);

    printf("%*s%*s%*s\n", G_leftCols, "bin start", 
			  G_leftCols, "bin end", 
			  G_leftCols, "count");
    for(i=0; i<G_bins; i++) {
	printf("%*.4f%*.4f%*.4f ",
		G_leftCols, (i*f_binSize)+f_minCurv, 
		G_leftCols, ((i+1)*f_binSize)+f_minCurv, 
		G_leftCols, pf_histogram[i]);
	printf("\n");
	switch(aesot) {
	    case e_Raw:
		if(GpFILE_rawHist!=NULL)
		fprintf(GpFILE_rawHist, "%f\t%f\t%f\n", 
				(i*f_binSize)+f_minCurv,
				((i+1)*f_binSize)+f_minCurv,
				pf_histogram[i]);
	    break;
	    case e_Gaussian:
		if(GpFILE_KHist!=NULL)
		fprintf(GpFILE_KHist,"%f\t%f\t%f\n", 
				(i*f_binSize)+f_minCurv,
				((i+1)*f_binSize)+f_minCurv,
				pf_histogram[i]);
	    break;
	    case e_Mean:
		if(GpFILE_HHist!=NULL)
		fprintf(GpFILE_HHist,"%f\t%f\t%f\n", 
				(i*f_binSize)+f_minCurv,
				((i+1)*f_binSize)+f_minCurv,
				pf_histogram[i]);
	    break;
	    case e_Normal:
		if(GpFILE_normHist!=NULL)
		fprintf(GpFILE_normHist, "%f\t%f\t%f\n", 
				(i*f_binSize)+f_minCurv,
				((i+1)*f_binSize)+f_minCurv,
				pf_histogram[i]);
	    break;
	    case e_ScaledTrans:
	    case e_Scaled:
		if(GpFILE_scaledHist!=NULL)
		fprintf(GpFILE_scaledHist,"%f\t%f\t%f\n", 
				(i*f_binSize)+f_minCurv,
				((i+1)*f_binSize)+f_minCurv,
				pf_histogram[i]);
	    break;
	}
    }
    free(pf_histogram);
}

// Usable AlmostEqual function
//	Bruce Dawson, see
//  http://www.cygnus-software.com/papers/comparingfloats/comparingfloats.htm
#define true  1
#define false 0
int AlmostEqual2sComplement(float A, float B, int maxUlps)
{
    int aInt;
    int bInt;
    int intDiff;

    // Make sure maxUlps is non-negative and small enough that the
    // default NAN won't compare as equal to anything.
    assert(maxUlps > 0 && maxUlps < 4 * 1024 * 1024);
    aInt = *(int*)&A;
    // Make aInt lexicographically ordered as a twos-complement int
    if (aInt < 0)
        aInt = 0x80000000 - aInt;
    // Make bInt lexicographically ordered as a twos-complement int
    bInt = *(int*)&B;
    if (bInt < 0)
        bInt = 0x80000000 - bInt;
    intDiff = abs(aInt - bInt);
    if (intDiff <= maxUlps)
        return true;
    return false;
}

void
histogram_create(
	MRIS*			amris_curvature,
	float			af_minCurv,
	double			af_binSize,
	int			abins,
	float*			apf_histogram
) {
    //
    // PRECONDITIONS
    //	o amris_curvature must be valid and populated with curvature values
    //	  of correct type (i.e. normalised, Gaussian/Mean, scaled)
    //	o apf_histogram memory must be handled (i.e. created/freed) by the
    //	  caller.
    //  o af_minCurv < af_maxCurv.
    //
    // POSTCONDITIONS
    //	o curvature values in amris_curvature will be sorted into abins.
    //
    // HISTORY
    // 12 September 2005
    //	o Initial design and coding.
    //

    //char*	pch_function	= "histogram_create()";

    int 	vno		= 0;
    float*	pf_curvature	= NULL;
    int		i		= 0;
    int		j		= 0;
    int		start		= 0;
    int		count		= 0;
    int		totalCount	= 0;
    int		nvertices	= 0;

    int		b_onLeftBound	= 0; // Don't trigger higher order float comp
    int		b_onRightBound	= 0; //	on left/right bounds

    double	l_curvature;	// These three variables
    double	l_leftBound;	//	are scaled up and truncated
    double	l_rightBound;	//	to minimise rounding errors

    nvertices		= amris_curvature->nvertices;
    fprintf(stdout, "\n%*s = %f\n",  
		G_leftCols, "bin size", af_binSize);
    fprintf(stdout, "%*s = %d\n",  
		G_leftCols, "surface vertices", nvertices);
    pf_curvature	= calloc(nvertices, sizeof(float));
    
    for(vno=0; vno < nvertices; vno++) 
	pf_curvature[vno]	= amris_curvature->vertices[vno].curv;

    qsort(pf_curvature, nvertices, sizeof(float), comp);

    for(i=0; i<abins; i++) {
	count	= 0;
	for(j=start; j<nvertices; j++) {
	    l_curvature		= (double)(pf_curvature[j]);
	    l_leftBound		= (double)((i*af_binSize+af_minCurv));
            l_rightBound	= (double)((((i+1)*af_binSize)+af_minCurv));
	    if(Gb_maxUlps) {
		b_onRightBound	= 
		    AlmostEqual2sComplement(l_curvature,l_rightBound,G_maxUlps);
	    	b_onLeftBound	=		
		    AlmostEqual2sComplement(l_curvature,l_leftBound,G_maxUlps);
	    }
	    if(	(l_curvature >= l_leftBound && l_curvature <= l_rightBound)
					||
			  b_onLeftBound || b_onRightBound ) {
		count++;
	        totalCount++;
	    }
	    if(l_curvature > l_rightBound && !b_onRightBound) {
		start = j;
		break;
	    }
	}
	apf_histogram[i]	= count;
	if(Gb_histogramPercent)
	    apf_histogram[i] /= nvertices;
	if(totalCount == nvertices)
	    break;
    }
    fprintf(stdout, "%*s = %d\n", 
		G_leftCols, "sorted vertices", totalCount);
    fprintf(stdout, "%*s = %f\n", 
		G_leftCols, "ratio", (float)totalCount / (float)nvertices);
    free(pf_curvature);
}

/*----------------------------------------------------------------------
            Parameters:

           Description:
----------------------------------------------------------------------*/
static int
get_option(int argc, char *argv[])
{
  int  		nargs 		= 0;
  char*		option;
  char*		pch_text;
  
  option = argv[1] + 1 ;            /* past '-' */

  switch (toupper(*option))
  {
  case 'O':
    output_fname 	= argv[2] ;
    Gb_output2File	= 1;
    nargs 		= 1 ;
    fprintf(stderr, "Outputting results using filestem '%s'...\n", output_fname) ;
    break ;
  case 'F':
    pch_text = argv[2];
    strcpy(surf_name, pch_text); 
    nargs = 1 ;
    fprintf(stderr, "Setting surface to '%s'...\n", surf_name);
    break;
  case 'L':
    label_name = argv[2] ;
    fprintf(stderr, "Using label '%s'...\n", label_name) ;
    nargs = 1 ;
    break ;
  case 'A':
    navgs = atoi(argv[2]) ;
    fprintf(stderr, "Averaging curvature %d times...\n", navgs);
    nargs = 1 ;
    break ;
  case 'C':
    Gb_scale		= 1;
    Gf_scaleFactor 	= atof(argv[2]) ;
    fprintf(stderr, "Setting raw scale factor to %f...\n", Gf_scaleFactor);
    nargs 		= 1 ;
    break ;
  case 'D':
    Gb_scaleMin		= 1;
    Gf_scaleMin		= atof(argv[2]);
    fprintf(stderr, "Setting scale min factor to %f...\n", Gf_scaleMin);
    nargs		= 1;
    break;
  case 'E':
    Gb_scaleMax		= 1;
    Gf_scaleMax		= atof(argv[2]);
    fprintf(stderr, "Setting scale max factor to %f...\n", Gf_scaleMax);
    nargs		= 1;
    break;
  case 'G':
    Gb_gaussianAndMean	= 1;
    break ;
  case 'S':   /* write out stats */
    stat_flag 		= 1 ;
    condition_no 	= atoi(argv[2]) ;
    nargs 		= 1 ;
    fprintf(stderr, "Setting out summary statistics as condition %d...\n",
            condition_no) ;
    break ;
  case 'H':   /* histogram */
    if(argc == 2)
	print_usage();
    Gb_histogram	= 1;
    Gb_histogramPercent	= 0;
    Gb_minMaxShow	= 1;
    G_bins 		= atoi(argv[2]);
    nargs 		= 1 ;
    fprintf(stderr, "Setting curvature histogram to %d bins...\n",
            G_bins);
    if(G_bins <=0 )
    	ErrorExit(ERROR_BADPARM, "%s: Invalid bin number.\n", Progname);
    break ;
  case 'P':   /* percentage histogram */
    Gb_histogram	= 1;
    Gb_histogramPercent	= 1;
    Gb_minMaxShow	= 1;
    G_bins 		= atoi(argv[2]);
    nargs 		= 1 ;
    fprintf(stderr, "Creating percentage curvature histogram with %d bins...\n",
            G_bins);
    break ;
  case 'B':   /* binSize override*/
    Gb_binSizeOverride	= 1;
    Gf_binSize 		= (double) atof(argv[2]);
    nargs 		= 1 ;
    fprintf(stderr, "Setting curvature histogram binSize to %f...\n",
            Gf_binSize);
    break;
  case 'I':   /* histogram start */
    Gb_histStartOverride	= 1;
    Gf_histStart 		= atof(argv[2]);
    nargs 			= 1 ;
    fprintf(stderr, "Setting histogram start point to %f...\n",
            Gf_histStart);
    break;
  case 'J':   /* histogram end */
    Gb_histEndOverride	= 1;
    Gf_histEnd 		= atof(argv[2]);
    nargs 		= 1 ;
    fprintf(stderr, "Setting histogram end point to %f...\n",
            Gf_histEnd);
    break;
  case 'Z':   /* zero a target vertex */
    Gb_zeroVertex	= 1;
    G_zeroVertex	= atoi(argv[2]);
    nargs 		= 1 ;
    fprintf(stderr, "Setting zero vertex index to %d...\n",
            G_zeroVertex);
    break;
  case 'Q':   /* set maxUlps */
    Gb_maxUlps		= 1;
    G_maxUlps		= atoi(argv[2]);
    nargs 		= 1 ;
    fprintf(stderr, "Setting maxUlps to %d...\n",
            G_zeroVertex);
    break;
  case 'N':
    normalize_flag 	= 1 ;
    fprintf(stderr, "Setting normalisation ON...\n");
    break ;
  case 'M':
    Gb_minMaxShow	= 1;
    break;
  case 'V':
    print_version() ;
    exit(1);
    break;
  case '?':
  case 'U':
    print_usage() ;
    exit(1) ;
    break ;
  default:
    fprintf(stderr, "unknown option %s\n", argv[1]) ;
    exit(1) ;
    break ;
  }
  return(nargs) ;
}

void
outputFileNames_create(
	char*		apch_curv
) {
    //
    // POSTCONDITIONS
    //	o All necessary (depending on user flags) file names are created.
    //
    OFSP_create(Gpch_log, 	Gpch_logS,	apch_curv,	e_None);
    OFSP_create(Gpch_allLog, 	Gpch_allLogS,	apch_curv,	e_Full);
    OFSP_create(Gpch_rawHist,	Gpch_rawHistS,	apch_curv,	e_Full);
    OFSP_create(Gpch_normHist,	Gpch_normHistS,	apch_curv,	e_Full);
    OFSP_create(Gpch_normCurv,	Gpch_normCurvS,	apch_curv,	e_Partial);
    OFSP_create(Gpch_KHist, 	Gpch_KHistS,	apch_curv,	e_Full);
    OFSP_create(Gpch_KCurv, 	Gpch_KCurvS,	apch_curv,	e_Partial);
    OFSP_create(Gpch_HHist,	Gpch_HHistS,	apch_curv,	e_Full);
    OFSP_create(Gpch_HCurv,	Gpch_HCurvS,	apch_curv,	e_Partial);
    OFSP_create(Gpch_scaledHist,Gpch_scaledHistS,apch_curv,	e_Full);
    OFSP_create(Gpch_scaledCurv,Gpch_scaledCurvS,apch_curv,	e_Partial);
}

void
outputFiles_open(void) {
    //
    // POSTCONDITIONS
    //	o All necessary (depending on user flags) files are opened.
    //

    if(Gb_output2File) {	
	CE("\n\tFiles processed for this curvature:\n");
  	printf("%*s\n", G_leftCols*2, Gpch_log);
	if((GpFILE_log=fopen(Gpch_log, "a"))==NULL)
	    ErrorExit(ERROR_NOFILE, "%s: Could not open file '%s' for apending.\n",
			Progname, Gpch_log);
  	printf("%*s\n", G_leftCols*2, Gpch_allLog);
	if((GpFILE_allLog=fopen(Gpch_allLog, "w"))==NULL)
	    ErrorExit(ERROR_NOFILE, "%s: Could not open file '%s' for writing.\n",
			Progname, Gpch_allLog);
	if(Gb_histogram) {
  	    printf("%*s\n", G_leftCols*2, Gpch_rawHist);
	    if((GpFILE_rawHist=fopen(Gpch_rawHist, "w"))==NULL)
	    	ErrorExit(ERROR_NOFILE, "%s: Could not open file '%s' for writing.\n",
			Progname, Gpch_rawHist);
	}
	if(normalize_flag) {
	    if(Gb_histogram) {
		printf("%*s\n", G_leftCols*2, Gpch_normHist);
	    	if((GpFILE_normHist=fopen(Gpch_normHist, "w"))==NULL)
	    		ErrorExit(ERROR_NOFILE, 
				"%s: Could not open file '%s' for writing.\n",
				Progname, Gpch_normHist);
	    }
	    printf("%*s\n", G_leftCols*2, Gpch_normCurv);
	}
	if(Gb_gaussianAndMean) {
	    if(Gb_histogram) {
	    	if((GpFILE_KHist=fopen(Gpch_KHist, "w"))==NULL) {
			printf("%*s\n", G_leftCols*2, Gpch_KHist);
	    		ErrorExit(ERROR_NOFILE, 
				"%s: Could not open file '%s' for writing.\n",
				Progname, Gpch_KHist);
		}
	    	if((GpFILE_HHist=fopen(Gpch_HHist, "w"))==NULL) {
			printf("%*s\n", G_leftCols*2,Gpch_HHist);
	    		ErrorExit(ERROR_NOFILE, 
				"%s: Could not open file '%s' for writing.\n",
				Progname, Gpch_HHist);
		}
	    }
	    printf("%*s\n", G_leftCols*2, Gpch_KCurv);
	    printf("%*s\n", G_leftCols*2, Gpch_HCurv);
        }
	if(Gb_scale || (Gb_scaleMin && Gb_scaleMax)) {
	    if(Gb_histogram) {
		printf("%*s\n", G_leftCols*2, Gpch_scaledHist);		
	    	if((GpFILE_scaledHist=fopen(Gpch_scaledHist, "w"))==NULL)
	    		ErrorExit(ERROR_NOFILE, 
				"%s: Could not open file '%s' for writing.\n",
				Progname, Gpch_scaledHist);
	    }
	    printf("%*s\n", G_leftCols*2, Gpch_scaledCurv);
	}	
    printf("\n");
  }
}

void
outputFiles_close(void) {
    //
    // POSTCONDITIONS
    //	o Checks for any open files and closes them.
    //
  
    if(GpFILE_log)	fclose(GpFILE_log);	GpFILE_log 	= NULL;
    if(GpFILE_allLog)	fclose(GpFILE_allLog);	GpFILE_allLog 	= NULL;	
    if(GpFILE_rawHist)	fclose(GpFILE_rawHist);	GpFILE_rawHist	= NULL;
    if(GpFILE_normHist)	fclose(GpFILE_normHist);GpFILE_normHist	= NULL;
    if(GpFILE_KHist)	fclose(GpFILE_KHist);	GpFILE_KHist	= NULL;
    if(GpFILE_HHist)	fclose(GpFILE_HHist);	GpFILE_HHist	= NULL;
    if(GpFILE_scaledHist)
			fclose(GpFILE_scaledHist); GpFILE_scaledHist = NULL;
}

static void
usage_exit(void)
{
  print_usage() ;
  exit(1) ;
}

static void
print_usage(void)
{
  print_help() ;
}

static void
print_help(void)
{
  char  pch_synopsis[65536];
  
  sprintf(pch_synopsis,"\n\
									\n\
NAME									\n\
									\n\
	mris_curvature_stats						\n\
									\n\
SYNOPSIS								\n\
									\n\
	mris_curvature_stats [options] \\				\n\
		<subjectName> <hemi> <curvFile1> [... <curvFileN]	\n\
									\n\
DESCRIPTION								\n\
									\n\
	'mris_curvature_stats' will primarily compute the mean and	\n\
	variances for a curvature file (or a set of curvature files).	\n\
	These files can be possibly constrained by a label.		\n\
									\n\
	Additionally, 'mris_curvature_stats' can report the max/min	\n\
	curvature values, and compute a simple histogram based on 	\n\
	these vales.							\n\
									\n\
	Curvatures can also be normalised and constrained to a given	\n\
	range before computation.					\n\
									\n\
	Gaussian and mean calculations on a surface structure can	\n\
	be performed.							\n\
									\n\
	Finally, all output to the console, as well as any new		\n\
	curvatures that result from the above calculations can be	\n\
	saved to a series of text and binary-curvature files.		\n\
									\n\
	Please see the following options for more details.		\n\
									\n\
OPTIONS									\n\
									\n\
    [-a <numberOfAverages]						\n\
									\n\
	Average the curvature <numberOfAverages> times.			\n\
									\n\
    [-o <outputFileStem>]						\n\
									\n\
	Save processing results to a series of files. This includes	\n\
	condensed text output, histogram files (MatLAB friendly)	\n\
	and curvature files.						\n\
									\n\
	The actual files that are saved depends on which additional	\n\
	calculation flags have been specified (i.e. normalisation,	\n\
	Gaussian / Mean, scaling).					\n\
									\n\
	In the case when a Gaussian/Mean calculation has been 		\n\
	performed, 'mris_curvature_stats' will act in a manner		\n\
	similar to 'mris_curvature -w'.	 Note though that though the	\n\
	name of the curvature files that are created are different, 	\n\
	the contents are identical to 'mris_curvature -w' created files.\n\
									\n\
	All possible files that can be saved are:			\n\
	(where  OHSC  = <outputFileStem>.<hemi>.<surface>.<curvFile>	\n\
		HSC   = <hemi>.<surface>.<curvFile>			\n\
									\n\
		<outputFileStem>	(Log only a single		\n\
					 mean+-sigma. If several	\n\
					 curvature files have been	\n\
					 specified, log the mean+-sigma \n\
					 across all the curvatures.	\n\
					 Note also that this file is	\n\
					 *appended* for each new run.)	\n\
		<OHSC>.log		(Full output, i.e the output of	\n\
					 each curvature file mean +-	\n\
					 sigma, as well as min/max	\n\
					 as it is processed.)  		\n\
		<OHS>.raw.hist		(Raw histogram file. By 'raw'   \n\
					 is implied that the curvature  \n\
					 has not been further processed \n\
					 in any manner.)		\n\
		<OHS>.norm.hist		(Normalised histogram file)	\n\
		<HS>.norm.crv 		(Normalised curv file)		\n\
		<OHS>.K.hist		(Gaussian histogram file)	\n\
		<HS>.K.crv 		(Gaussian curv file)		\n\
		<OHS>.H.hist		(Mean histogram file)		\n\
		<HS>.H.crv 		(Mean curv file)		\n\
		<OHS>.scaled.hist	(Scaled histogram file)		\n\
		<HS>.scaled.crv		(Scaled curv file)		\n\
									\n\
	Note that curvature files are saved to $SUBJECTS_DIR/surf	\n\
	and *not* to the current working directory.			\n\
									\n\
	Note also that file names can become quite long and somewhat	\n\
	unyielding.							\n\
									\n\
    [-h <numberOfBins>] [-p <numberOfBins]				\n\
									\n\
	If specified, prepare a histogram over the range of curvature	\n\
	values, using <numberOfBins> buckets. These are dumped to	\n\
	stdout.								\n\
									\n\
	If '-p' is used, then the histogram is expressed as a 		\n\
	percentage.							\n\
									\n\
	Note that this histogram, working off float values and float	\n\
	boundaries, can suffer from rounding errors! There might be	\n\
	instances when a very few (on average) curvature values might	\n\
	not be sorted.							\n\
									\n\
	The histogram behaviour can be further tuned with the 		\n\
	following:							\n\
									\n\
    [-b <binSize>] [-i <binStartCurvature] [-j <binEndCurvature]	\n\
									\n\
	These arguments are only processed iff a '-h <numberOfBins>'	\n\
	has also been specified. By default, <binSize> is defined as	\n\
									\n\
		(maxCurvature - minCurvature) / <numberOfBins>		\n\
									\n\
	The '-b' option allows the user to specify an arbitrary		\n\
	<binSize>. This is most useful when used in conjunction with	\n\
	the '-i <binStartCurvature>' option, which starts the histogram	\n\
	not at (minCurvature), but at <binStartCurvature>. So, if 	\n\
	a histogram reveals that most values seem confined to a very	\n\
	narrow range, the '-b' and '-i' allow the user to 'zoom in'	\n\
	to this range and expand.					\n\
									\n\
	If <binStartCurvature> < (minCurvature), then regardless	\n\
	of its current value, <binStartCurvature> = (minCurvature).	\n\
	Also, if (<binStartCurvature> + <binSize>*<numberOfBins> >)	\n\
	(maxCurvature), an error is raised and processing aborts.	\n\
									\n\
	The '-j' allows the user to specify an optional end		\n\
	value for the histogram. Using '-i' and '-j' together		\n\
	are the most convenient ways to zoom into a region of interest	\n\
	in a histogram.							\n\
									\n\
    [-l <labelFileName>]						\n\
									\n\
	Constrain statistics to the region defined in <labelFileName>.	\n\
									\n\
    [-m]								\n\
									\n\
	Output min / max information for the processed curvature.	\n\
									\n\
    [-n]								\n\
									\n\
	Normalise the curvature before computation. Normalisation	\n\
	takes precedence over scaling, so if '-n' is specified		\n\
	in conjunction with '-c' or '-smin'/'-smax' it will		\n\
	override the effects of the scaling.				\n\
									\n\
	If specified in conjunction with '-o <outputFileStem>'		\n\
	will also create a curvature file with these values.		\n\
									\n\
    [-s <summaryCondition>]						\n\
									\n\
	Write out stats as <summaryCondition>.				\n\
									\n\
    [-d <minCurvature> -e <maxCurvature>]				\n\
									\n\
	Scale curvature values between <minCurvature> and 		\n\
	<maxCurvature>. If the minimum curvature is greater		\n\
	than the maximum curvature, or if either is 			\n\
	unspecified, these flags are ignored.				\n\
									\n\
	This scale computation takes precedence over '-c' scaling.	\n\
									\n\
	Note also that the final scaling bounds might not correspond	\n\
	to <minCurvature>... <maxCurvature> since values are scaled	\n\
	across this range so as to preserve the original mean profile.	\n\
									\n\
	If specified in conjunction with '-o <outputFileStem>'		\n\
	will also create a curvature file with these values.		\n\
									\n\
    [-c <factor>]							\n\
									\n\
	Scale curvature values with <factor>. The mean of the 		\n\
	original curvature is conserved (and the sigma increases	\n\
	with <factor>).							\n\
									\n\
	If specified in conjunction with '-o <outputFileStem>'		\n\
	will also create a curvature file with these values.		\n\
									\n\
    [-version]								\n\
									\n\
	Print out version number.					\n\
									\n\
    [-z <vertexIndex>]							\n\
									\n\
	Sets the curvature values at that index to zero. The 		\n\
	'raw' curvature, as well as the Gaussian and Mean curvatures	\n\
	are set to zero, and min/max values are recomputed.		\n\
									\n\
	This is useful in cases when outliers in the data (particularly \n\
	evident in Gaussian calcuations) skew mean and sigma values.	\n\
									\n\
    [-q <maxUlps>]							\n\
									\n\
	The <maxUlps> is used to toggle a more rigorous floating point	\n\
	comparison operation in the histogram function. Comparing 	\n\
	float values for sorting into bins can at times fail due to	\n\
	number precision issues. If, over the range of comparison	\n\
	some curvature values are not sorted, add <maxUlps>.		\n\
									\n\
	This adds extra function calls to AlmostEqual2sComplement(..)	\n\
	for float comparisons and improves the general accuracy, at 	\n\
	a very slight performance penalty.				\n\
									\n\
	You will most likely never have to use this argument, and is 	\n\
	for advanced use only.						\n\
									\n\
NOTES									\n\
									\n\
	It is important to note that some combinations of the command	\n\
	line parameters are somewhat meaningless, such as normalising	\n\
	a 'sulc' curvature file (since it's normalised by definition).	\n\
									\n\
EXAMPLES								\n\
 									\n\
    mris_curvature_stats 801_recon rh curv				\n\
									\n\
	For subject '801_recon', determine the mean and sigma for	\n\
	the curvature file on the right hemisphere.			\n\
									\n\
    mris_curvature_stats -m 801_recon rh curv				\n\
									\n\
	Same as above, but print the min/max curvature values		\n\
	across the surface.						\n\
									\n\
    mris_curvature_stats -h 20 -m 801_recon rh curv			\n\
									\n\
	Same as above, and also print a histogram of curvature 		\n\
	values over the min/max range, using 20 bins. By replacing	\n\
	the '-h' with '-p', print the histogram as a percentage.	\n\
									\n\
    mris_curvature_stats -h 20 -b 0.01 -i 0.1 -m 801_recon rh curv	\n\
									\n\
	Same as above, but this time constrain the histogram to the 20  \n\
	bins from -0.1 to 0.1, with a bin size of 0.01.			\n\
									\n\
	Note that the count / percentage values are taken across the    \n\
	total curvature range and not the constrained window defined 	\n\
	by the '-i' and '-b' arguments.					\n\
									\n\
    mris_curvature_stats -G -m 801_recon rh curv			\n\
									\n\
	Print the min/max curvatures for 'curv', and also calculate	\n\
	the Gaussian and Mean curvatures (also printing the min/max     \n\
	for these).							\n\
									\n\
    mris_curvature_stats -G -F smoothwm -m 801_recon rh curv		\n\
									\n\
	By default, 'mris_curvature_stats' reads the 'orig' surface	\n\
	for the passed subject. This is not generally the best surface	\n\
	for Gaussian determination. The '-F' uses the 'smoothwm' 	\n\
	surface, which is a better choice.				\n\
									\n\
    mris_curvature_stats -h 10 -G -F smoothwm -m 801_recon rh curv	\n\
									\n\
	Same as above, with the addition of a histogram for the 	\n\
	Gaussian and Mean curvatures as well.				\n\
									\n\
    mris_curvature_stats -h 10 -G -F smoothwm -m -o foo \\ 		\n\
	801_recon rh curv sulc						\n\
									\n\
	Generate several output text files that capture the min/max	\n\
	and histograms for each curvature processed. Also create	\n\
	new Gaussian and Mean curvature files.				\n\
									\n\
	In this case, the curvature files created are called:		\n\
									\n\
		rh.smoothwm.curv.K.crv					\n\
		rh.smoothwm.curv.H.crv					\n\
		rh.smoothwm.sulc.K.crv					\n\
		rh.smoothwm.sulc.H.crv					\n\
									\n\
	and are saved to the $SUBJECTS_DIR/<subjid>/surf directory.	\n\
	These can be re-read by 'mris_curvature_stats' using 		\n\
									\n\
		mris_curvature_stats -m 801_recon rh \\			\n\
			smoothwm.curv.K.crv smoothwm.sulc.K.crv		\n\
									\n\
ADVANCED EXAMPLES							\n\
									\n\
	'mris_curvature_stats' can also provide some useful side 	\n\
	effects. Reading in a curvature, and applying any calculation	\n\
	to it (scaling, gaussian, etc.) can result in data that		\n\
	can be visualised quite well in a tool such as 'tksurfer'.	\n\
									\n\
	Consider the normal curvature display in 'tksurfer', which	\n\
	is usually quite dark due to the dynamic range of the display.	\n\
	We can considerably improve the brightness by scaling a 	\n\
	curvature file and rendering the resultant in 'tksurfer'.	\n\
									\n\
	First, take an arbitrary curvature, apply a scale factor,	\n\
	and an output filestem:						\n\
									\n\
    mris_curvature_stats -o foo -c 10 801_recon rh curv			\n\
									\n\
	This scales each curvature value by 10. A new curvature file	\n\
	is saved in							\n\
									\n\
		$SUBJECTS_DIR/801_recon/surf/rh.orig.curv.scaled.crv	\n\
									\n\
	Comparing the two curvatures in 'tksurfer' will clearly show	\n\
	the scaled file as much brighter.				\n\
									\n\
	Similarly, the Gaussian curvature can be processed, scaled, and	\n\
	displayed, yielding very useful visual information. First 	\n\
	create and save a Gaussian curvature file (remember that the	\n\
	smoothwm surface is a better choice than the default orig	\n\
	surface):							\n\
									\n\
    mris_curvature_stats -o foo -G -F smoothwm 801_recon rh curv	\n\
									\n\
	The 'foo' filestem is ignored when saving curvature files, but	\n\
	needs to be specified in order to trigger output saving. This	\n\
	command will create Gaussian and Mean curvature files in the	\n\
	$SUBJECTS_DIR/surf directory:					\n\
									\n\
		rh.smoothwm.curv.K.crv					\n\
		rh.smoothwm.curv.H.crv					\n\
									\n\
	Now, process the created Gaussian with the scaled curvature:	\n\
									\n\
    mris_curvature_stats -o foo -c 10 801_recon rh smoothwm.curv.K.crv	\n\
									\n\
	Again, the 'foo' filestem is ignored, but needs to be specified	\n\
	to trigger the save. The final scaled Gaussian curvature is 	\n\
	saved to (again in the $SUBJECTS_DIR/801_recon/surf directory):	\n\
									\n\
		rh.orig.smoothwm.curv.K.crv.scaled.crv			\n\
									\n\
	which is a much better candidate to view in 'tksurfer' than	\n\
	the original Gaussian curvature file.				\n\
									\n\
									\n\
									\n\
");

  CE(pch_synopsis);
  exit(1) ;
}

static void
print_version(void)
{
  fprintf(stderr, "%s\n", vcid) ;
  exit(1) ;
}

int  comp(const void *p, const void *q)
{
  float *ptr1 = (float *)(p);
  float *ptr2 = (float *)(q);

  if (*ptr1 < *ptr2) return -1;
  else if (*ptr1 == *ptr2) return 0;
  else return 1;
}

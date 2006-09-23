/*
  Name:    mri_surf2vol
  Author:  Douglas N. Greve
  email:   analysis-bugs@nmr.mgh.harvard.edu
  Date:    2/27/02
  Purpose: converts values on a surface to a volume
  $Id: mri_surf2vol.c,v 1.18 2006/09/23 18:47:13 greve Exp $
*/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include <string.h>

#include "macros.h"
#include "error.h"
#include "diag.h"
#include "proto.h"

#include "matrix.h"
#include "mri.h"
#include "mri2.h"
#include "mri_identify.h"
#include "MRIio_old.h"
#include "registerio.h"
#include "resample.h"
#include "version.h"
#include "fio.h"
#include "fsenv.h"

static int  parse_commandline(int argc, char **argv);
static void check_options(void);
static void print_usage(void) ;
static void usage_exit(void);
static void print_help(void) ;
static void print_version(void) ;
static void argnerr(char *option, int n);
static void dump_options(FILE *fp);
static int  singledash(char *flag);
static int isflag(char *flag);
static int nth_is_arg(int nargc, char **argv, int nth);
static int istringnmatch(char *str1, char *str2, int n);

int main(int argc, char *argv[]) ;

static char vcid[] =
"$Id: mri_surf2vol.c,v 1.18 2006/09/23 18:47:13 greve Exp $";
char *Progname = NULL;

int debug = 0, gdiagno = -1;

char *subjectsdir = NULL;
char *surfvalpath = NULL;
char *surfvalfmt = NULL;
int   surfvalfmtid = 0;
char *hemi = NULL;
char *surfname = "white";
char *srcsubject = NULL;
char *targsubject = NULL;
float projfrac = 0;
static int fillribbon = 0 ;

char *tempvolpath=NULL;
char *tempvolfmt;
int   tempvolfmtid = 0;
char *mergevolpath=NULL;
char *outvolpath;
char *outvolfmt;
int   outvolfmtid = 0;
char *vtxvolpath;
char *vtxvolfmt;
int   vtxvolfmtid = 0;
int   dim[3];
float res[3];
float xyz0[3];
float cdircos[3], rdircos[3], sdircos[3];
char *precision;

char *volregfile = NULL;

MRI *mritmp;
MRI *SurfVal;
MRI *RefAnat;
MRI *TempVol,*OutVol;
MRI *VtxVol;
MRI_SURFACE *SrcSurf;

MATRIX *Ma2vTKR;
MATRIX *Kvol, *invKvol;
MATRIX *Qa2v;

float reshapefactor;
int mksurfmask = 0;
int UseVolRegIdentity = 0;
FSENV *fsenv;
int fstalres;
char tmpstr[2000];

/*---------------------------------------------------------------*/
int main(int argc, char **argv)
{
  float ipr, bpr, intensity, v;
  int float2int, err, vtx, nhits, c,r,s,f;
  char fname[2000];
  int nargs;
  int n ;

  /* rkt: check for and handle version tag */
  nargs = handle_version_option
    (argc, argv,
     "$Id: mri_surf2vol.c,v 1.18 2006/09/23 18:47:13 greve Exp $",
     "$Name:  $");
  if (nargs && argc - nargs == 1)
    exit (0);
  argc -= nargs;

  Progname = argv[0] ;
  argc --;
  argv++;
  ErrorInit(NULL, NULL, NULL) ;
  DiagInit(NULL, NULL, NULL) ;

  if(argc == 0) usage_exit();

  fsenv = FSENVgetenv();

  parse_commandline(argc, argv);
  printf("gdiagno = %d\n",gdiagno);
  if(gdiagno > -1) Gdiag_no = gdiagno;

  check_options();

  if(UseVolRegIdentity){
    printf("Using identity matrix for registration\n");
    Ma2vTKR = MatrixIdentity(4,NULL);
  }
  else{
    /* Read in the tkregister registration */
    err = regio_read_register(volregfile, &srcsubject, &ipr, &bpr,
                              &intensity, &Ma2vTKR, &float2int);
    if(err) exit(1);
  }

  /* Read in the template volume header */
  TempVol = MRIreadHeader(tempvolpath,MRI_VOLUME_TYPE_UNKNOWN);
  if(TempVol == NULL){
    printf("mri_surf2vol ERROR: reading %s header\n",tempvolpath);
    exit(1);
  }

  /* Read in the anatomical reference volume header */
  sprintf(fname,"%s/%s/mri/orig.mgz",subjectsdir,srcsubject);
  if(fio_FileExistsReadable(fname))  RefAnat = MRIread(fname);
  if(!fio_FileExistsReadable(fname)) RefAnat = NULL;
  if(RefAnat == NULL){
    printf("Cannot find orig.mgz, trying orig/COR files instead...\n");
    sprintf(fname,"%s/%s/mri/orig",subjectsdir,srcsubject);
    RefAnat = MRIread(fname);
    if(RefAnat == NULL){
      printf("mri_surf2vol ERROR: reading %s header\n",fname);
      exit(1);
    }
  }

  /* Construct the matrix to map from Surface XYZ to vol */
  Kvol = MRIxfmCRS2XYZtkreg(TempVol); /* converts crs to xyz in vol */
  invKvol = MatrixInverse(Kvol,NULL); /* converts xyz to crs in vol */
  Qa2v = MatrixMultiply(invKvol,Ma2vTKR,NULL); /* conv xyz anat to crs vol */
  printf("Qa2v: SurfXYZ to VolCRS: ------------------------------\n");
  MatrixPrint(stdout,Qa2v);
  printf("--------------------------------------------------\n");

  /* Dump some info before staring the main program */
  dump_options(stdout);

  /* ---------- Load the surface for source subject -------------------*/
  sprintf(fname,"%s/%s/surf/%s.%s",subjectsdir,srcsubject,hemi,surfname);
  printf("Reading surface %s\n",fname);
  SrcSurf = MRISread(fname) ;
  if (!SrcSurf)
    ErrorExit(ERROR_NOFILE, "%s: could not read surface %s", Progname, fname) ;
  printf("Done reading source surface\n");
  fflush(stdout);

  /* Load the thickness for projection along the normal*/
  if((projfrac != 0) || (fillribbon != 0)){
    sprintf(fname,"%s/%s/surf/%s.%s",subjectsdir,srcsubject,hemi,"thickness");
    printf("Reading thickness %s\n",fname);
    MRISreadCurvatureFile(SrcSurf, fname);
    printf("Done\n");
  }

  /* ----------- Read in the surface values ------------------------- */
  if(! mksurfmask){
    if(istringnmatch(surfvalfmt,"paint",0)){
      printf("INFO: Reading %s as paint format\n",surfvalpath);
      MRISreadValues(SrcSurf,surfvalpath);
      SurfVal = MRIallocSequence(SrcSurf->nvertices, 1, 1,MRI_FLOAT,1);
      for(vtx = 0; vtx < SrcSurf->nvertices; vtx ++){
        MRIFseq_vox(SurfVal,vtx,0,0,0) = SrcSurf->vertices[vtx].val;
      }
    }
    else {
      printf("INFO: reading  %s as %s\n",surfvalpath,surfvalfmt);
      SurfVal =  MRIreadType(surfvalpath,surfvalfmtid);
      if(SurfVal == NULL){
        printf("ERROR: could not read %s as %s\n",surfvalpath,surfvalfmt);
        exit(1);
      }
      if(SurfVal->height != 1 || SurfVal->depth != 1){
        reshapefactor = SurfVal->height * SurfVal->depth;
        printf("INFO: Reshaping %f\n",reshapefactor);
        mritmp = mri_reshape(SurfVal, reshapefactor*SurfVal->width,
                             1, 1, SurfVal->nframes);
        MRIfree(&SurfVal);
        SurfVal = mritmp;
      }
      if(SurfVal->width != SrcSurf->nvertices){
        fprintf(stderr,"ERROR: dimesion inconsitency in source data\n");
        fprintf(stderr,"       Number of surface vertices = %d\n",
                SrcSurf->nvertices);
        fprintf(stderr,"      Number of value vertices = %d\n",SurfVal->width);
        exit(1);
      }
    }

    printf("Done loading source values (nvtxs = %d)\n",SrcSurf->nvertices);
  }
  else{
    /* Create a mask of the surface by filling the array with 1's */
    SurfVal =  MRIallocSequence(SrcSurf->nvertices,1,1,MRI_FLOAT,1);
    printf("surf nframes = %d\n",SurfVal->nframes);
    if(SurfVal == NULL){
      printf("ERROR: could not alloc SurfVal\n");
      exit(1);
    }
    MRIvalueFill(SurfVal, 1);
  }

  /*---------- Allocate the output volume ------------------*/
  OutVol = MRIallocSequence(TempVol->width, TempVol->height,
                            TempVol->depth,  MRI_FLOAT, SurfVal->nframes);
  if(OutVol == NULL){
    printf("ERROR: could not alloc output volume MRI\n");
    exit(1);
  }
  MRIcopyHeader(TempVol,OutVol);
  OutVol->nframes = SurfVal->nframes;

  printf("INFO: mapping vertices to closest voxel\n");
  if (fillribbon){   /* fill entire ribbon */
    for (nhits = 0, projfrac = 0.0 ; projfrac <= 1.0 ; projfrac += 0.05){
      VtxVol = MRImapSurf2VolClosest(SrcSurf, OutVol, Qa2v, projfrac);
      if(VtxVol == NULL){
	printf("ERROR: could not map vertices to voxels\n");
	exit(1);
      }

      n = MRIsurf2Vol(SurfVal, OutVol, VtxVol);
      printf("INFO: resampling surface to volume at projfrac=%2.2f, %d hits\n",
             projfrac, n);
      nhits += n ;
      MRIfree(&VtxVol) ;
    }
#if 0
      /* nhits is not valid yet for filling the ribbon.
         MRIsurf2Vol needs to be rewritten to take into
         account the fact that the same voxel get mapped
         many times
      */
      printf("INFO: sampled %d voxels in the volume\n",nhits);
#endif
  }
  else{  /* sample from one point */
    VtxVol = MRImapSurf2VolClosest(SrcSurf, OutVol, Qa2v, projfrac);
    if(VtxVol == NULL){
      printf("ERROR: could not map vertices to voxels\n");
      exit(1);
    }

    printf("INFO: resampling surface to volume\n");
    nhits = MRIsurf2Vol(SurfVal, OutVol, VtxVol);
    printf("INFO: sampled %d voxels in the volume\n",nhits);
    MRIcopyHeader(OutVol,VtxVol); // should fix MRIsurf2vol()
  }

  /* count the number of hits */

  //if(mksurfmask){ // This may not be necessary
  //  printf("INFO: binarizing output volume\n");
  //}

  /* Read in the merge volume */
  if(mergevolpath){
    TempVol = MRIread(mergevolpath);
    if(TempVol == NULL){
      printf("mri_surf2vol ERROR: reading %s\n",mergevolpath);
      exit(1);
    }
    for(c=0; c < OutVol->width; c++){
      for(r=0; r < OutVol->height; r++){
	for(s=0; s < OutVol->depth; s++){
	  v = MRIgetVoxVal(VtxVol,c,r,s,0);
	  if(v == -1){
	    // output is zero, replace with mergevol
	    for(f=0; f < OutVol->nframes; f++){
	      v = MRIgetVoxVal(TempVol,c,r,s,f);
	      MRIsetVoxVal(OutVol,c,r,s,f,v);
	    } //frame
	  } 
	} //slice
      } //row
    } //col
  }


  if(outvolpath != NULL){
    printf("INFO: writing output volume to %s\n",outvolpath);
    MRIwriteType(OutVol,outvolpath,outvolfmtid);
  }

  if(vtxvolpath != NULL){
    printf("INFO: writing closest vertex map to %s\n",vtxvolpath);
    MRIwrite(VtxVol,vtxvolpath);
  }

  printf("done\n");
  return(0);
}


/* ------------------------------------------------------------------ */
/* ------------------------------------------------------------------ */
/* ------------------------------------------------------------------ */
static int parse_commandline(int argc, char **argv)
{
  int  i, nargc , nargsused;
  char **pargv, *option ;

  if(argc < 1) usage_exit();

  nargc   = argc;
  pargv = argv;
  while(nargc > 0){

    option = pargv[0];
    if(debug) printf("%d %s\n",nargc,option);
    nargc -= 1;
    pargv += 1;

    nargsused = 0;

    if (!strcasecmp(option, "--help"))  print_help() ;
    else if (!strcasecmp(option, "--version")) print_version() ;
    else if (!strcasecmp(option, "--debug"))   debug = 1;
    else if (!strcasecmp(option, "--mkmask"))  mksurfmask = 1;

    else if ( !strcmp(option, "--gdiagno") ) {
      if(nargc < 1) argnerr(option,1);
      sscanf(pargv[0],"%d",&gdiagno);
      nargsused = 1;
    }

    else if (istringnmatch(option, "--sd",4)){
      if(nargc < 1) argnerr(option,1);
      subjectsdir = pargv[0]; nargsused = 1;
    }
    else if (istringnmatch(option, "--surfval",0) || istringnmatch(option, "--sval",0)){
      if(nargc < 1) argnerr(option,1);
      surfvalpath = pargv[0]; nargsused = 1;
      if(nth_is_arg(nargc, pargv, 1)){
        surfvalfmt = pargv[1]; nargsused ++;
        surfvalfmtid = string_to_type(surfvalfmt);
      }
    }
    else if (istringnmatch(option, "--srcsubject",9)){
      if(nargc < 1) argnerr(option,1);
      srcsubject = pargv[0]; nargsused = 1;
    }
    else if (istringnmatch(option, "--surf",9)){
      if(nargc < 1) argnerr(option,1);
      surfname = pargv[0]; nargsused = 1;
    }
    else if (istringnmatch(option, "--hemi",3)){
      if(nargc < 1) argnerr(option,1);
      hemi = pargv[0]; nargsused = 1;
      if(strcmp(hemi,"lh") && strcmp(hemi,"rh")){
        printf("ERROR: hemi = %s, must be lh or rh\n",hemi);
        exit(1);
      }
    }
    else if ( !strcmp(option, "--projfrac") ) {
      if(nargc < 1) argnerr(option,1);
      sscanf(pargv[0],"%f",&projfrac); nargsused = 1;
    }
    else if ( !strcmp(option, "--fillribbon") ) {
      if(nargc < 0) argnerr(option,1);
      nargsused = 0;
      fillribbon = 1 ;
    }
    else if (istringnmatch(option, "--volregidentity",16)){
      if(nargc < 1) argnerr(option,1);
      srcsubject = pargv[0]; nargsused = 1;
      UseVolRegIdentity = 1;
    }
    else if (istringnmatch(option, "--volreg",8)){
      if(nargc < 1) argnerr(option,1);
      volregfile = pargv[0]; nargsused = 1;
    }
    else if (istringnmatch(option, "--fstal",7)){
      if(nargc < 1) argnerr(option,1);
      sscanf(pargv[0],"%d",&fstalres); 
      sprintf(tmpstr,"%s/average/mni305.cor.subfov%d.reg",
	      fsenv->FREESURFER_HOME,fstalres);
      volregfile = strcpyalloc(tmpstr);
      sprintf(tmpstr,"%s/average/mni305.cor.subfov%d.mgz",
	      fsenv->FREESURFER_HOME,fstalres);
      tempvolpath = strcpyalloc(tmpstr);

      nargsused = 1;
    }
    else if (istringnmatch(option, "--outvol",0)){
      if(nargc < 1) argnerr(option,1);
      outvolpath = pargv[0]; nargsused = 1;
      if(nth_is_arg(nargc, pargv, 1)){
        outvolfmt = pargv[1]; nargsused ++;
        outvolfmtid = string_to_type(outvolfmt);
      }
    }
    else if (istringnmatch(option, "--vtxvol",0)){
      if(nargc < 1) argnerr(option,1);
      vtxvolpath = pargv[0]; nargsused = 1;
      if(nth_is_arg(nargc, pargv, 1)){
        vtxvolfmt = pargv[1]; nargsused ++;
        vtxvolfmtid = string_to_type(vtxvolfmt);
      }
    }
    else if (istringnmatch(option, "--template",6)){
      if(nargc < 1) argnerr(option,1);
      tempvolpath = pargv[0]; nargsused = 1;
      if(nth_is_arg(nargc, pargv, 1)){
        tempvolfmt = pargv[1]; nargsused ++;
        tempvolfmtid = string_to_type(tempvolfmt);
      }
    }
    else if (istringnmatch(option, "--merge",7)){
      if(nargc < 1) argnerr(option,1);
      mergevolpath = pargv[0]; nargsused = 1;
    }
    else if ( !strcmp(option, "--dim") ) {
      if(nargc < 3) argnerr(option,3);
      for(i=0;i<3;i++) sscanf(pargv[i],"%d",&dim[i]);
      nargsused = 3;
    }
    else if ( !strcmp(option, "--res") ) {
      if(nargc < 3) argnerr(option,3);
      for(i=0;i<3;i++) sscanf(pargv[i],"%f",&res[i]);
      nargsused = 3;
    }
    else if ( !strcmp(option, "--xyz0") ) {
      if(nargc < 3) argnerr(option,3);
      for(i=0;i<3;i++) sscanf(pargv[i],"%f",&xyz0[i]);
      nargsused = 3;
    }
    else if ( !strcmp(option, "--cdircos") ) {
      if(nargc < 3) argnerr(option,3);
      for(i=0;i<3;i++) sscanf(pargv[i],"%f",&cdircos[i]);
      nargsused = 3;
    }
    else if ( !strcmp(option, "--rdircos") ) {
      if(nargc < 3) argnerr(option,3);
      for(i=0;i<3;i++) sscanf(pargv[i],"%f",&rdircos[i]);
      nargsused = 3;
    }
    else if ( !strcmp(option, "--sdircos") ) {
      if(nargc < 3) argnerr(option,3);
      for(i=0;i<3;i++) sscanf(pargv[i],"%f",&sdircos[i]);
      nargsused = 3;
    }
    else if (!strcmp(option, "--precision")){
      if(nargc < 1) argnerr(option,1);
      precision = pargv[0];
      nargsused = 1;
    }
    else{
      fprintf(stderr,"ERROR: Option %s unknown\n",option);
      if(singledash(option))
        fprintf(stderr,"       Did you really mean -%s ?\n",option);
      exit(-1);
    }
    nargc -= nargsused;
    pargv += nargsused;
  }
  return(0);
}
/* ------------------------------------------------------ */
static void usage_exit(void)
{
  print_usage() ;
  exit(1) ;
}
/* --------------------------------------------- */
static void print_usage(void)
{
  printf("USAGE: %s \n",Progname) ;
  printf("\n");
  printf("  --surfval surfvalpath <fmt>\n");
  printf("  --mkmask : make a mask instead of loading surfval\n");
  printf("  --hemi hemisphere (lh or rh)\n");
  printf("  --surf surfname (default is white)\n");
  printf("  --projfrac thickness fraction \n");
  printf("  --fillribbon\n");
  printf("  --volreg volume registration file\n");
  printf("  --volregidentity subjid : use identity "
         "(must supply subject name)\n");
  printf("  --template vol : output like this volume\n");
  printf("  \n");
  printf("  --fstal res : use fs talairach registration\n");
  printf("  \n");
  printf("  --merge vol : merge with this vol (becomes template)\n");
  printf("  \n");
  printf("  --outvol <fmt>  output volume path id\n");
  printf("  --vtxvol <fmt>  vertex map volume path id\n");
  printf("  \n");
  printf("  --sd subjectsdir : FreeSurfer subjects' directory\n");
  printf("  --help    : hidden secrets of success\n");
  printf("  --gdiagno number : set diag level\n");
  printf("  --version : print version and exit\n");
  printf("  \n");
}
/* --------------------------------------------- */
static void print_help(void)
{
  print_usage() ;

  printf("\n%s\n\n",vcid);

  printf
    (
     "Resamples a surface into a volume. \n"
     "\n"
     "FLAGS AND ARGUMENTS\n"
     "\n"
     "--surfval surfvalpath <fmt>\n"
     "\n"
     "This is the source of the surface values, and, optionally,"
     " the format.\n"
     "If the format is not included, the format will be "
     "inferred from the\n"
     "path name. See FORMATS below. A mask can be "
     "created instead; see --mkmask.\n"
     "\n"
     "--mkmask \n"
     "\n"
     "Create a binary (ie, 1/0) mask of all the "
     "locations where a surface\n"
     "vertex intersects a volume voxel. This is done instead of mapping\n"
     "surface values to the volume.\n"
     "\n"
     "--hemi hemisphere\n"
     "\n"
     "Hemisphere (lh or rh) that the source surface values refer to.\n"
     "\n"
     "--surf surfname\n"
     "\n"
     "Surface to use as found in surf directory. The actual surface file\n"
     "name will be hemi.surfname. Default is white.\n"
     "\n"
     "--projfrac fraction\n"
     "\n"
     "When sampling into the volume, compute the XYZ of the vertex as the\n"
     "XYZ of the vertex on the white/gray boundary projected fraction of the\n"
     "cortical thickncess along the surface normal. For example, to place\n"
     "the vertex half way into the cortical sheet, set fraction = 0.5. The\n"
     "fraction can be any number (including negatives).\n"
     "\n"
     "--fillribbon\n"
     "\n"
     "Fill the entire ribbon (iterate projfrac from 0 to 1)\n"
     "\n"
     "--volreg volume registration file\n"
     "\n"
     "Contains the matrix that maps XYZ in the reference anatomical to XYZ\n"
     "in the functional volume. The format of this file is that as output by\n"
     "tkregister2 and includes the name of the subject. It will be assumed\n"
     "that the input surface values are sampled on the surface of this\n"
     "subject. Cannot be used with --volregidentity.\n"
     "\n"
     "--volregidentity subjid\n"
     "\n"
     "Use identity matrix for the registration between the surface and the\n"
     "template volume (ie, template volume is the anatomical ref). "
     "Must supply\n"
     "subjid (which is usually obtained from the volreg). "
     "Cannot be used with\n"
     "--volreg.\n"
     "\n"
     "--template template volume <fmt>\n"
     "\n"
     "This is the volume that will be used as a template for the output\n"
     "volume in terms of the field-of-view, geometry, and precision. If the\n"
     "format is not included, the format will be inferred from the path\n"
     "name. Not needed with --merge. See FORMATS below.\n"
     "\n"
     "--fstal res\n"
     "\n"
     "sets volreg to $FREESURFER_HOME/average/mni305.cor.subfov$res.reg \n"
     "and template to $FREESURFER_HOME/average/mni305.cor.subfov$res.mgz\n"
     "res can be 1 or 2.\n"
     "\n"
     "--merge mergevolume\n"
     "\n"
     "Merge the output volume with this volume. Ie, each voxel that is not\n"
     "assigned a value from the surface inherits its value from the merge\n"
     "volume. The merge volume becomes the template volume (ie, --template\n"
     "not needed or used).\n"
     "\n"
     "--outvol output volume <fmt>\n"
     "\n"
     "Path name of the output volume. If the format is not included, the\n"
     "format will be inferred from the path name. See FORMATS below.\n"
     "\n"
     "--vtxvol vertex output volume <fmt>\n"
     "\n"
     "Path name of the vertex output volume. The vertex volume is the the\n"
     "same as the output volume except that the voxel value is the number of\n"
     "the vertex that mapped to that voxel. If no vertex mapped to a voxel,\n"
     "the value is set to -1. This volume is mainly helpful for\n"
     "debugging. If the format is not included, the format will be inferred\n"
     "from the path name. See FORMATS below.\n"
     "\n"
     "--sd subjectsdir\n"
     "\n"
     "Use subjectsdir as the FreeSurfer subjects directory. If unspecified,\n"
     "the value of the environment variable SUBJECTS_DIR is used.\n"
     "\n"
     "--gdiagno diagnostic level\n"
     "\n"
     "Sets the diagnostic level (only good for debugging).\n"
     "\n"
     "--version\n"
     "\n"
     "Print out version string and exit.\n"
     "\n"
     "--help \n"
     "\n"
     "Prints out all this information.\n"
     "\n"
     "FORMATS\n"
     "\n"
     "Data file format can be specified implicitly (through the path name)\n"
     "or explicitly. All formats accepted by mri_convert can be used. "
     "In addition,\n"
     "the surface value file can be paint format. "
     "If paint format is used, make\n"
     "sure to put ./ in front of the pathname.\n"
     "\n"
     "BUGS\n"
     "\n"
     "If paint format is used, make sure to put ./ in front of the pathname.\n"
     "\n"
     "BUG REPORTING\n"
     "\n"
     "Report bugs to analysis-bugs@nmr.mgh.harvard.edu. "
     "Include the following \n"
     "formatted as a list as follows: (1) command-line, (2) directory where\n"
     "the program was run (for those in the MGH-NMR Center), (3) version, \n"
     "(4) text output, (5) description of the problem.\n"
     "\n"
     "SEE ALSO \n"
     "\n"
     "mri_convert, mri_vol2surf\n");

  exit(1) ;
}

/* --------------------------------------------- */
static void print_version(void)
{
  printf("%s\n", vcid) ;
  exit(1) ;
}

/* --------------------------------------------- */
static void argnerr(char *option, int n)
{
  if(n==1)
    fprintf(stderr,"ERROR: %s flag needs %d argument\n",option,n);
  else
    fprintf(stderr,"ERROR: %s flag needs %d arguments\n",option,n);
  exit(-1);
}

/* --------------------------------------------- */
static void check_options(void)
{
  if(! mksurfmask ) {
    if(surfvalpath == NULL){
      printf("A surface value path must be supplied\n");
      exit(1);
    }
    if(surfvalfmt == NULL){
      surfvalfmtid = mri_identify(surfvalpath);
      if(surfvalfmtid == MRI_VOLUME_TYPE_UNKNOWN){
        printf("ERROR: cannot recognize the type of %s\n",surfvalpath);
        exit(1);
      }
      surfvalfmt = type_to_string(surfvalfmtid);
    }
    else{
      if(! istringnmatch(surfvalfmt,"paint",0)){
        surfvalfmtid = string_to_type(surfvalfmt);
        if(surfvalfmtid == MRI_VOLUME_TYPE_UNKNOWN){
          printf("ERROR: cannot recognize format %s\n",surfvalfmt);
          exit(1);
        }
      }
    }
  }
  if(mksurfmask && surfvalpath != NULL){
    printf("ERROR: cannot make mask and spec surface value file\n");
    exit(1);
  }

  if(hemi == NULL){
    printf("A hemisphere must be supplied\n");
    exit(1);
  }
  if(volregfile != NULL && UseVolRegIdentity){
    printf("ERROR: cannot spec both --volreg file --volregidentity. \n");
    exit(1);
  }
  if(volregfile == NULL && !UseVolRegIdentity){
    printf("A volume registration file must be supplied\n");
    exit(1);
  }
  if(outvolpath == NULL && vtxvolpath == NULL){
    printf("ERROR: No output supplied.\n");
    exit(1);
  }
  if(mergevolpath != NULL){
    printf("Using merge volume as template\n");
    tempvolpath = mergevolpath;
  }

  if(tempvolpath == NULL){
    printf("A template volume path must be supplied\n");
    exit(1);
  }

  if(subjectsdir == NULL){
    subjectsdir = getenv("SUBJECTS_DIR");
    if(subjectsdir==NULL){
      fprintf(stderr,"ERROR: SUBJECTS_DIR not defined in environment\n");
      exit(1);
    }
  }

  if(outvolfmt == NULL){
    outvolfmtid = mri_identify(outvolpath);
    if(outvolfmtid == MRI_VOLUME_TYPE_UNKNOWN){
      printf("ERROR: cannot recognize the type of %s\n",outvolpath);
      exit(1);
    }
    outvolfmt = type_to_string(outvolfmtid);
  }
  else{
    outvolfmtid = string_to_type(outvolfmt);
    if(outvolfmtid == MRI_VOLUME_TYPE_UNKNOWN){
      printf("ERROR: cannot recognize format %s\n",outvolfmt);
      exit(1);
    }
  }

  if(vtxvolfmt != NULL){
    if(vtxvolfmt == NULL){
      vtxvolfmtid = mri_identify(vtxvolpath);
      if(vtxvolfmtid == MRI_VOLUME_TYPE_UNKNOWN){
        printf("ERROR: cannot recognize the type of %s\n",vtxvolpath);
        exit(1);
      }
      vtxvolfmt = type_to_string(vtxvolfmtid);
    }
    else{
      vtxvolfmtid = string_to_type(vtxvolfmt);
      if(vtxvolfmtid == MRI_VOLUME_TYPE_UNKNOWN){
        printf("ERROR: cannot recognize format %s\n",vtxvolfmt);
        exit(1);
      }
    }
  }

  if(tempvolfmt == NULL){
    tempvolfmtid = mri_identify(tempvolpath);
    if(tempvolfmtid == MRI_VOLUME_TYPE_UNKNOWN){
      printf("ERROR: cannot recognize the type of %s\n",tempvolpath);
      exit(1);
    }
    tempvolfmt = type_to_string(tempvolfmtid);
  }
  else{
    tempvolfmtid = string_to_type(tempvolfmt);
    if(tempvolfmtid == MRI_VOLUME_TYPE_UNKNOWN){
      printf("ERROR: cannot recognize format %s\n",tempvolfmt);
      exit(1);
    }
  }
  if(mergevolpath) tempvolpath = mergevolpath;

  return;
}

/* --------------------------------------------- */
static void dump_options(FILE *fp)
{
  MRI *mri;

  fprintf(fp,"subjects dir   %s\n",subjectsdir);
  if(!mksurfmask) fprintf(fp,"surface value path  %s\n",surfvalpath);
  fprintf(fp,"hemi           %s\n",hemi);
  fprintf(fp,"mksurfmask     %d\n",mksurfmask);
  fprintf(fp,"projfrac       %g\n",projfrac);
  if(volregfile) fprintf(fp,"volreg file    %s\n",volregfile);
  fprintf(fp,"outvol   path  %s\n",outvolpath);
  fprintf(fp,"template path  %s\n",tempvolpath);

  fprintf(fp,"------- Anat2Vol Registration (TkReg)----\n");
  MatrixPrint(fp,Ma2vTKR);
  fprintf(fp,"-----------------------------------------\n");

  mri = TempVol;
  fprintf(fp, "%6.6s = %d\n", "height", mri->height);
  fprintf(fp, "%6.6s = %d\n", "width", mri->width);
  fprintf(fp, "%6.6s = %d\n", "depth", mri->depth);
  fprintf(fp, "%6.6s = %f\n", "xsize", mri->xsize);
  fprintf(fp, "%6.6s = %f\n", "ysize", mri->ysize);
  fprintf(fp, "%6.6s = %f\n", "zsize", mri->zsize);
  fprintf(fp, "%6.6s = %f %f %f\n", "cdc ", mri->x_r, mri->x_a, mri->x_s);
  fprintf(fp, "%6.6s = %f %f %f\n", "rdc ", mri->y_r, mri->y_a, mri->y_s);
  fprintf(fp, "%6.6s = %f %f %f\n", "sdc ", mri->z_r, mri->z_a, mri->z_s);
  fprintf(fp, "%6.6s = %f %f %f\n", "xyz0", mri->c_r, mri->c_a, mri->c_s);

  fprintf(fp,"Gdiag_no  %d\n",Gdiag_no);

  return;
  fprintf(fp,"dim      %3d %3d %3d\n",
          dim[0],dim[1],dim[2]);
  fprintf(fp,"res      %6.4f %6.4f %6.4f\n",
          res[0],res[1],res[2]);
  fprintf(fp,"xyz0  %6.4f %6.4f %6.4f\n",
          xyz0[0], xyz0[1], xyz0[2]);
  fprintf(fp,"col   dircos  %6.4f %6.4f %6.4f\n",
          cdircos[0],cdircos[1],cdircos[2]);
  fprintf(fp,"row   dircos  %6.4f %6.4f %6.4f\n",
          rdircos[0],rdircos[1],rdircos[2]);
  fprintf(fp,"slice dircos  %6.4f %6.4f %6.4f\n",
          sdircos[0],sdircos[1],sdircos[2]);
  fprintf(fp,"precision %s\n",precision);

  return;
}

/*---------------------------------------------------------------*/
static int singledash(char *flag)
{
  int len;
  len = strlen(flag);
  if(len < 2) return(0);
  if(flag[0] == '-' && flag[1] != '-') return(1);
  return(0);
}

/*---------------------------------------------------------------*/
static int isflag(char *flag)
{
  int len;
  len = strlen(flag);
  if(len < 2) return(0);

  if(flag[0] == '-' && flag[1] == '-') return(1);
  return(0);
}

/*---------------------------------------------------------------*/
static int nth_is_arg(int nargc, char **argv, int nth)
{
  /* Checks that nth arg exists and is not a flag */
  /* nth is 0-based */

  /* check that there are enough args for nth to exist */
  if(nargc <= nth) return(0);

  /* check whether the nth arg is a flag */
  if(isflag(argv[nth])) return(0);

  return(1);
}

/*------------------------------------------------------------
  istringnmatch() - compare the first n characters of two strings,
  return a 1 if they match (ignoring case), a zero otherwise. If
  n=0, then do a full comparison.
  ------------------------------------------------------------*/
static int istringnmatch(char *str1, char *str2, int n)
{
  if(n > 0  && ! strncasecmp(str1,str2,n)) return(1);
  if(n <= 0 && ! strcasecmp(str1,str2)) return(1);
  return(0);
}

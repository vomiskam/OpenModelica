/*
 * This file is part of OpenModelica.
 *
 * Copyright (c) 1998-2014, Open Source Modelica Consortium (OSMC),
 * c/o Linköpings universitet, Department of Computer and Information Science,
 * SE-58183 Linköping, Sweden.
 *
 * All rights reserved.
 *
 * THIS PROGRAM IS PROVIDED UNDER THE TERMS OF THE BSD NEW LICENSE OR THE
 * GPL VERSION 3 LICENSE OR THE OSMC PUBLIC LICENSE (OSMC-PL) VERSION 1.2.
 * ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS PROGRAM CONSTITUTES
 * RECIPIENT'S ACCEPTANCE OF THE OSMC PUBLIC LICENSE OR THE GPL VERSION 3,
 * ACCORDING TO RECIPIENTS CHOICE.
 *
 * The OpenModelica software and the OSMC (Open Source Modelica Consortium)
 * Public License (OSMC-PL) are obtained from OSMC, either from the above
 * address, from the URLs: http://www.openmodelica.org or
 * http://www.ida.liu.se/projects/OpenModelica, and in the OpenModelica
 * distribution. GNU version 3 is obtained from:
 * http://www.gnu.org/copyleft/gpl.html. The New BSD License is obtained from:
 * http://www.opensource.org/licenses/BSD-3-Clause.
 *
 * This program is distributed WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE, EXCEPT AS
 * EXPRESSLY SET FORTH IN THE BY RECIPIENT SELECTED SUBSIDIARY LICENSE
 * CONDITIONS OF OSMC-PL.
 *
 */

#include <string.h>
#include <setjmp.h>

#include "openmodelica.h"
#include "openmodelica_func.h"
#include "simulation_data.h"

#include "util/omc_error.h"
#include "util/memory_pool.h"

#include "simulation/simulation_runtime.h"
#include "simulation/results/simulation_result.h"
#include "simulation/solver/solver_main.h"
#include "simulation/solver/model_help.h"
#include "simulation/solver/external_input.h"
#include "simulation/solver/epsilon.h"

#include "simulation/solver/dassl.h"
#include "meta_modelica.h"


static const char *dasslMethodStr[DASSL_MAX] = {"unknown",
                                                "dassl",
                                                "dasslwort",
                                                "dasslsteps",
                                                "dasslSymJac",
                                                "dasslNumJac",
                                                "dasslColorSymJac",
                                                "dasslInternalNumJac"};

static const char *dasslMethodStrDescStr[DASSL_MAX] = {"unknown",
                                                       "dassl with colored numerical jacobian, with interval root finding - default",
                                                       "dassl without internal root finding",
                                                       "dassl as default, but without consideration of numberOfintervals or stepSize. Output point are internal dassl steps.",
                                                       "dassl with symbolic jacobian",
                                                       "dassl with numerical jacobian",
                                                       "dassl with colored symbolic jacobian",
                                                       "dassl with internal numerical jacobian"};



/* provides a dummy Jacobian to be used with DASSL */
static int
dummy_Jacobian(double *t, double *y, double *yprime, double *deltaD,
    double *delta, double *cj, double *h, double *wt, double *rpar, int* ipar) {
  return 0;
}
static int
dummy_zeroCrossing(int *neqm, double *t, double *y, double *yp,
                   int *ng, double *gout, double *rpar, int* ipar) {
  return 0;
}

static int JacobianSymbolic(double *t, double *y, double *yprime,  double *deltaD, double *pd, double *cj, double *h, double *wt,
    double *rpar, int* ipar);
static int JacobianSymbolicColored(double *t, double *y, double *yprime, double *deltaD, double *pd, double *cj, double *h, double *wt,
    double *rpar, int* ipar);
static int JacobianOwnNum(double *t, double *y, double *yprime, double *deltaD, double *pd, double *cj, double *h, double *wt,
    double *rpar, int* ipar);
static int JacobianOwnNumColored(double *t, double *y, double *yprime, double *deltaD, double *pd, double *cj, double *h, double *wt,
    double *rpar, int* ipar);

void  DDASKR(
    int (*res) (double *t, double *y, double *yprime, double* cj, double *delta, int *ires, double *rpar, int* ipar),
    int *neq,
    double *t,
    double *y,
    double *yprime,
    double *tout,
    int *info,
    double *rtol,
    double *atol,
    int *idid,
    double *rwork,
    int *lrw,
    int *iwork,
    int *liw,
    double *rpar,
    int *ipar,
    int (*jac) (double *t, double *y, double *yprime, double *deltaD, double *delta, double *cj, double *h, double *wt, double *rpar, int* ipar),
    int (*psol) (int *neq, double *t, double *y, double *yprime, double *savr, double *pwk, double *cj, double *wt, double *wp, int *iwp, double *b, double eplin, int* ires, double *rpar, int* ipar),
    int (*g) (int *neqm, double *t, double *y, double *yp, int *ng, double *gout, double *rpar, int* ipar),
    int *ng,
    int *jroot
);

static int
dummy_precondition(int *neq, double *t, double *y, double *yprime, double *savr, double *pwk, double *cj, double *wt, double *wp, int *iwp, double *b, double eplin, int* ires, double *rpar, int* ipar){
    return 0;
}


static int continue_DASSL(int* idid, double* tolarence);



/* function for calculating state values on residual form */
static int functionODE_residual(double *t, double *x, double *xprime, double *cj, double *delta, int *ires, double *rpar, int* ipar);

/* function for calculating zeroCrossings */
static int function_ZeroCrossingsDASSL(int *neqm, double *t, double *y, double *yp,
        int *ng, double *gout, double *rpar, int* ipar);

int dassl_initial(DATA* data, SOLVER_INFO* solverInfo, DASSL_DATA *dasslData)
{
  /* work arrays for DASSL */
  int i;
  SIMULATION_INFO *simInfo = &(data->simulationInfo);

  TRACE_PUSH

  dasslData->dasslMethod = 0;

  for(i=1; i< DASSL_MAX;i++)
  {
    if(!strcmp((const char*)simInfo->solverMethod, dasslMethodStr[i]))
    {
      dasslData->dasslMethod = i;
      break;
    }
  }

  if(dasslData->dasslMethod == DASSL_UNKNOWN)
  {
    if (ACTIVE_WARNING_STREAM(LOG_SOLVER))
    {
      warningStreamPrint(LOG_SOLVER, 1, "unrecognized solver method %s, current options are:", simInfo->solverMethod);
      for(i=1; i < DASSL_MAX; ++i)
      {
        warningStreamPrint(LOG_SOLVER, 0, "  %-15s [%s]", dasslMethodStr[i], dasslMethodStrDescStr[i]);
      }
      messageClose(LOG_SOLVER);
    }
    throwStreamPrint(data->threadData,"unrecognized dassl solver method %s", simInfo->solverMethod);
  }
  else
  {
    infoStreamPrint(LOG_SOLVER, 0, "Use solver method: %s\t%s",dasslMethodStr[dasslData->dasslMethod],dasslMethodStrDescStr[dasslData->dasslMethod]);
  }


  dasslData->liw = 40 + data->modelData.nStates;
  dasslData->lrw = 60 + ((maxOrder + 4) * data->modelData.nStates) + (data->modelData.nStates * data->modelData.nStates)  + (3*data->modelData.nZeroCrossings);
  dasslData->rwork = (double*) calloc(dasslData->lrw, sizeof(double));
  assertStreamPrint(data->threadData, 0 != dasslData->rwork,"out of memory");
  dasslData->iwork = (int*)  calloc(dasslData->liw, sizeof(int));
  assertStreamPrint(data->threadData, 0 != dasslData->iwork,"out of memory");
  dasslData->ng = (int) data->modelData.nZeroCrossings;
  dasslData->ngdummy = (int) 0;
  dasslData->jroot = (int*)  calloc(data->modelData.nZeroCrossings, sizeof(int));
  dasslData->rpar = (double**) malloc(2*sizeof(double*));
  dasslData->ipar = (int*) malloc(sizeof(int));
  dasslData->ipar[0] = ACTIVE_STREAM(LOG_JAC);
  assertStreamPrint(data->threadData, 0 != dasslData->ipar,"out of memory");
  dasslData->atol = (double*) malloc(data->modelData.nStates*sizeof(double));
  dasslData->rtol = (double*) malloc(data->modelData.nStates*sizeof(double));
  dasslData->info = (int*) calloc(infoLength, sizeof(int));
  assertStreamPrint(data->threadData, 0 != dasslData->info,"out of memory");
  dasslData->dasslStatistics = (unsigned int*) calloc(numStatistics, sizeof(unsigned int));
  assertStreamPrint(data->threadData, 0 != dasslData->dasslStatistics,"out of memory");
  dasslData->dasslStatisticsTmp = (unsigned int*) calloc(numStatistics, sizeof(unsigned int));
  assertStreamPrint(data->threadData, 0 != dasslData->dasslStatisticsTmp,"out of memory");

  dasslData->idid = 0;

  dasslData->sqrteps = sqrt(DBL_EPSILON);
  dasslData->ysave = (double*) malloc(data->modelData.nStates*sizeof(double));
  dasslData->delta_hh = (double*) malloc(data->modelData.nStates*sizeof(double));
  dasslData->newdelta = (double*) malloc(data->modelData.nStates*sizeof(double));
  dasslData->stateDer = (double*) malloc(data->modelData.nStates*sizeof(double));

  dasslData->info[2] = 1;
  /*********************************************************************
   *info[3] = 1;  //go not past TSTOP
   *rwork[0] = stop;  //TSTOP
   *********************************************************************
   *info[6] = 1;  //prohibit code to decide max. stepsize on its own
   *rwork[1] = *step;  //define max. stepsize
   *********************************************************************/

  if(dasslData->dasslMethod == DASSL_SYMJAC ||
      dasslData->dasslMethod == DASSL_COLOREDSYMJAC ||
      dasslData->dasslMethod == DASSL_NUMJAC ||
      dasslData->dasslMethod == DASSL_WORT ||
      dasslData->dasslMethod == DASSL_RT ||
      dasslData->dasslMethod == DASSL_STEPS){
    if (data->callback->initialAnalyticJacobianA(data)){
      /* TODO: check that the one states is dummy */
      if(data->modelData.nStates == 1) {
        infoStreamPrint(LOG_SOLVER, 0, "No SparsePattern, since there are no states! Switch back to normal.");
      } else {
        infoStreamPrint(LOG_STDOUT, 0, "Jacobian or SparsePattern is not generated or failed to initialize! Switch back to normal.");
      }
      dasslData->dasslMethod = DASSL_INTERNALNUMJAC;
    }else{
      dasslData->info[4] = 1; /* use sub-routine JAC */
    }
  }

  if(dasslData->dasslMethod != DASSL_WORT)
    solverInfo->solverRootFinding = 1;

  /* Setup nominal values of the states
   * as relative tolerances */
  dasslData->info[1] = 1;
  for(i=0;i<data->modelData.nStates;++i)
  {
    dasslData->rtol[i] = data->simulationInfo.tolerance;
    dasslData->atol[i] = data->simulationInfo.tolerance * data->modelData.realVarsData[i].attribute.nominal;
  }

  TRACE_POP
  return 0;
}


int dassl_deinitial(DASSL_DATA *dasslData)
{
  TRACE_PUSH

  /* free work arrays for DASSL */
  free(dasslData->rwork);
  free(dasslData->iwork);
  free(dasslData->rpar);
  free(dasslData->ipar);
  free(dasslData->atol);
  free(dasslData->rtol);
  free(dasslData->info);
  free(dasslData->jroot);
  free(dasslData->ysave);
  free(dasslData->delta_hh);
  free(dasslData->newdelta);
  free(dasslData->stateDer);
  free(dasslData->dasslStatistics);
  free(dasslData->dasslStatisticsTmp);
  free(dasslData);

  TRACE_POP
  return 0;
}

/**********************************************************************************************
 * DASSL with synchronous treating of when equation
 *   - without integrated ZeroCrossing method.
 *   + ZeroCrossing are handled outside DASSL.
 *   + if no event occurs outside DASSL performs a warm-start
 **********************************************************************************************/
int dassl_step(DATA* data, SOLVER_INFO* solverInfo)
{
  double tout = 0;
  int i = 0;
  unsigned int ui = 0;
  int retVal = 0;

  SIMULATION_DATA *sData = (SIMULATION_DATA*) data->localData[0];
  SIMULATION_DATA *sDataOld = (SIMULATION_DATA*) data->localData[1];
  MODEL_DATA *mData = (MODEL_DATA*) &data->modelData;
  DASSL_DATA *dasslData = (DASSL_DATA*) solverInfo->solverData;
  modelica_real* stateDer = dasslData->stateDer;

  /* after rotation dassl expects the same states as before */
  memcpy(sData->realVars, sDataOld->realVars, sizeof(double)*data->modelData.nStates);
  memcpy(stateDer, sDataOld->realVars + data->modelData.nStates, sizeof(double)*data->modelData.nStates);

  dasslData->rpar[0] = (double*) (void*) data;
  dasslData->rpar[1] = (double*) (void*) dasslData;

  TRACE_PUSH

  assertStreamPrint(data->threadData, 0 != dasslData->rpar, "could not passed to DDASKR");

  /* If an event is triggered and processed restart dassl. */
  if(solverInfo->didEventStep)
  {
    debugStreamPrint(LOG_EVENTS_V, 0, "Event-management forced reset of DDASKR");
    /* obtain reset */
    dasslData->info[0] = 0;
    dasslData->idid = 0;
  }

  /* Calculate steps until TOUT is reached */
  /* If dasslsteps is selected, the dassl run to stopTime */
  if  (dasslData->dasslMethod == DASSL_STEPS){
    tout = data->simulationInfo.stopTime;
  }else {
    tout = solverInfo->currentTime + solverInfo->currentStepSize;
  }

  /* Check that tout is not less than timeValue
   * else will dassl get in trouble. If that is the case we skip the current step. */
  if (solverInfo->currentStepSize < DASSL_STEP_EPS)
  {
    infoStreamPrint(LOG_DASSL, 0, "Desired step to small try next one");
    infoStreamPrint(LOG_DASSL, 0, "Interpolate linear");

    /*euler step*/
    for(i = 0; i < data->modelData.nStates; i++)
    {
      sData->realVars[i] = sDataOld->realVars[i] + stateDer[i] * solverInfo->currentStepSize;
    }
    sData->timeValue = tout;
    data->callback->functionODE(data);
    solverInfo->currentTime = tout;

    TRACE_POP
    return retVal;
  }

  infoStreamPrint(LOG_DASSL, 0, "Calling DASSL from %.15g to %.15g", solverInfo->currentTime, tout);
  do
  {
    infoStreamPrint(LOG_SOLVER, 0, "new step: time=%.15g", solverInfo->currentTime);
    if(dasslData->idid == 1)
    {
      /* rotate RingBuffer before step is calculated */
      rotateRingBuffer(data->simulationData, 1, (void**) data->localData);
      sData = (SIMULATION_DATA*) data->localData[0];
      sDataOld = (SIMULATION_DATA*) data->localData[1];
      /* after rotation dassl expects the same states as before */
      memcpy(sData->realVars, sDataOld->realVars, sizeof(double)*data->modelData.nStates);
    }

    /* read input vars */
    externalInputUpdate(data);
    data->callback->input_function(data);

    if(dasslData->dasslMethod ==  DASSL_SYMJAC) {
      DDASKR(functionODE_residual, &mData->nStates,
          &solverInfo->currentTime, sData->realVars, stateDer, &tout,
          dasslData->info, dasslData->rtol, dasslData->atol, &dasslData->idid,
          dasslData->rwork, &dasslData->lrw, dasslData->iwork, &dasslData->liw,
          (double*) (void*) dasslData->rpar, dasslData->ipar, JacobianSymbolic, dummy_precondition,
          function_ZeroCrossingsDASSL, (int*) &dasslData->ng, dasslData->jroot);
    } else if(dasslData->dasslMethod ==  DASSL_NUMJAC) {
      DDASKR(functionODE_residual, &mData->nStates,
          &solverInfo->currentTime, sData->realVars, stateDer, &tout,
          dasslData->info, dasslData->rtol, dasslData->atol, &dasslData->idid,
          dasslData->rwork, &dasslData->lrw, dasslData->iwork, &dasslData->liw,
          (double*) (void*) dasslData->rpar, dasslData->ipar, JacobianOwnNum, dummy_precondition,
          function_ZeroCrossingsDASSL,(int*) &dasslData->ng, dasslData->jroot);
    } else if(dasslData->dasslMethod ==  DASSL_COLOREDSYMJAC) {
      DDASKR(functionODE_residual, &mData->nStates,
          &solverInfo->currentTime, sData->realVars, stateDer, &tout,
          dasslData->info, dasslData->rtol, dasslData->atol, &dasslData->idid,
          dasslData->rwork, &dasslData->lrw, dasslData->iwork, &dasslData->liw,
          (double*) (void*) dasslData->rpar, dasslData->ipar, JacobianSymbolicColored, dummy_precondition,
          function_ZeroCrossingsDASSL, (int*) &dasslData->ng, dasslData->jroot);
    } else if(dasslData->dasslMethod ==  DASSL_INTERNALNUMJAC) {
      DDASKR(functionODE_residual, &mData->nStates,
          &solverInfo->currentTime, sData->realVars, stateDer, &tout,
          dasslData->info, dasslData->rtol, dasslData->atol, &dasslData->idid,
          dasslData->rwork, &dasslData->lrw, dasslData->iwork, &dasslData->liw,
          (double*) (void*) dasslData->rpar, dasslData->ipar, dummy_Jacobian, dummy_precondition,
          function_ZeroCrossingsDASSL, (int*) &dasslData->ng, dasslData->jroot);
    } else if(dasslData->dasslMethod ==  DASSL_WORT) {
      DDASKR(functionODE_residual, &mData->nStates,
          &solverInfo->currentTime, sData->realVars, stateDer, &tout,
          dasslData->info, dasslData->rtol, dasslData->atol, &dasslData->idid,
          dasslData->rwork, &dasslData->lrw, dasslData->iwork, &dasslData->liw,
          (double*) (void*)dasslData->rpar, dasslData->ipar, JacobianOwnNumColored,  dummy_precondition,
          dummy_zeroCrossing, &dasslData->ngdummy, NULL);
    } else {
      DDASKR(functionODE_residual, (int*) &mData->nStates,
          &solverInfo->currentTime, sData->realVars, stateDer, &tout,
          dasslData->info, dasslData->rtol, dasslData->atol, &dasslData->idid,
          dasslData->rwork, &dasslData->lrw, dasslData->iwork, &dasslData->liw,
          (double*) (void*) dasslData->rpar, dasslData->ipar, JacobianOwnNumColored,  dummy_precondition,
          function_ZeroCrossingsDASSL, (int*) &dasslData->ng, dasslData->jroot);
    }
    /* set ringbuffer time to current time */ 
    sData->timeValue = solverInfo->currentTime;

    if(dasslData->idid == -1) {
      fflush(stderr);
      fflush(stdout);
      warningStreamPrint(LOG_DASSL, 0, "A large amount of work has been expended.(About 500 steps). Trying to continue ...");
      infoStreamPrint(LOG_DASSL, 0, "DASSL will try again...");
      dasslData->info[0] = 1; /* try again */
    } else if(dasslData->idid < 0) {
      fflush(stderr);
      fflush(stdout);
      retVal = continue_DASSL(&dasslData->idid, &data->simulationInfo.tolerance);
      /* read input vars */
      externalInputUpdate(data);
      data->callback->input_function(data);
      data->callback->functionODE(data);
      warningStreamPrint(LOG_STDOUT, 0, "can't continue. time = %f", sData->timeValue);
      TRACE_POP
      return retVal;
    } else if(dasslData->idid == 5) {
      data->threadData->currentErrorStage = ERROR_EVENTSEARCH;
    }

    /* emit step, if dasslsteps is selected */
    if (dasslData->dasslMethod == DASSL_STEPS){
      /*
       * to emit consistent value we need to update the whole
       * continuous system with algebraic variables.
       */
      updateContinuousSystem(data);
      sim_result.emit(&sim_result, data);

    } else if (dasslData->idid == 1){
      /* to be consistent we need to evaluate functionODE again,
       * since dassl does not evalute functionODE with the freshest states.
       */
      sData->timeValue = solverInfo->currentTime;
      data->callback->functionODE(data);
      data->callback->function_ZeroCrossingsEquations(data);
    }

  } while(dasslData->idid == 1 ||
          (dasslData->idid == -1 && solverInfo->currentTime <= data->simulationInfo.stopTime));


  if(ACTIVE_STREAM(LOG_DASSL)) {
    infoStreamPrint(LOG_DASSL, 1, "dassl call staistics: ");
    infoStreamPrint(LOG_DASSL, 0, "value of idid: %d", (int)dasslData->idid);
    infoStreamPrint(LOG_DASSL, 0, "current time value: %0.4g", solverInfo->currentTime);
    infoStreamPrint(LOG_DASSL, 0, "current integration time value: %0.4g", dasslData->rwork[3]);
    infoStreamPrint(LOG_DASSL, 0, "step size H to be attempted on next step: %0.4g", dasslData->rwork[2]);
    infoStreamPrint(LOG_DASSL, 0, "step size used on last successful step: %0.4g", dasslData->rwork[6]);
    infoStreamPrint(LOG_DASSL, 0, "number of steps taken so far: %d", (int)dasslData->iwork[10]);
    infoStreamPrint(LOG_DASSL, 0, "number of calls of functionODE() : %d", (int)dasslData->iwork[11]);
    infoStreamPrint(LOG_DASSL, 0, "number of calculation of jacobian : %d", (int)dasslData->iwork[12]);
    infoStreamPrint(LOG_DASSL, 0, "total number of convergence test failures: %d", (int)dasslData->iwork[13]);
    infoStreamPrint(LOG_DASSL, 0, "total number of error test failures: %d", (int)dasslData->iwork[14]);
    messageClose(LOG_DASSL);
  }

  /* save dassl stats */
  for(ui = 0; ui < numStatistics; ui++) {
    assert(10 + ui < dasslData->liw);
    dasslData->dasslStatisticsTmp[ui] = dasslData->iwork[10 + ui];
  }

  infoStreamPrint(LOG_DASSL, 0, "Finished DDASKR step.");

  TRACE_POP
  return retVal;
}

static int
continue_DASSL(int* idid, double* atol)
{
  int retValue = -1;

  TRACE_PUSH

  switch(*idid)
  {
  case 1:
  case 2:
  case 3:
    /* 1-4 means success */
    break;
  case -1:
    warningStreamPrint(LOG_DASSL, 0, "A large amount of work has been expended.(About 500 steps). Trying to continue ...");
    retValue = 1; /* adrpo: try to continue */
    break;
  case -2:
    warningStreamPrint(LOG_STDOUT, 0, "The error tolerances are too stringent");
    retValue = -2;
    break;
  case -3:
    /* wbraun: don't throw at this point let the solver handle it */
    /* throwStreamPrint("DDASKR: THE LAST STEP TERMINATED WITH A NEGATIVE IDID value"); */
    retValue = -3;
    break;
  case -6:
    warningStreamPrint(LOG_STDOUT, 0, "DDASSL had repeated error test failures on the last attempted step.");
    retValue = -6;
    break;
  case -7:
    warningStreamPrint(LOG_STDOUT, 0, "The corrector could not converge.");
    retValue = -7;
    break;
  case -8:
    warningStreamPrint(LOG_STDOUT, 0, "The matrix of partial derivatives is singular.");
    retValue = -8;
    break;
  case -9:
    warningStreamPrint(LOG_STDOUT, 0, "The corrector could not converge. There were repeated error test failures in this step.");
    retValue = -9;
    break;
  case -10:
    warningStreamPrint(LOG_STDOUT, 0, "A Modelica assert prevents the integrator to continue. For more information use -lv LOG_DASSL");
    retValue = -10;
    break;
  case -11:
    warningStreamPrint(LOG_STDOUT, 0, "IRES equal to -2 was encountered and control is being returned to the calling program.");
    retValue = -11;
    break;
  case -12:
    warningStreamPrint(LOG_STDOUT, 0, "DDASSL failed to compute the initial YPRIME.");
    retValue = -12;
    break;
  case -33:
    warningStreamPrint(LOG_STDOUT, 0, "The code has encountered trouble from which it cannot recover.");
    retValue = -33;
    break;
  }

  TRACE_POP
  return retValue;
}


int functionODE_residual(double *t, double *y, double *yd, double* cj, double *delta,
                    int *ires, double *rpar, int *ipar)
{
  DATA* data = (DATA*)(void*)((double**)rpar)[0];
  threadData_t *threadData = data->threadData;

  double timeBackup;
  long i;
  int saveJumpState;
  int success = 0;

  TRACE_PUSH

  timeBackup = data->localData[0]->timeValue;
  data->localData[0]->timeValue = *t;

  saveJumpState = data->threadData->currentErrorStage;
  data->threadData->currentErrorStage = ERROR_INTEGRATOR;

  /* try */
#if !defined(OMC_EMCC)
  MMC_TRY_INTERNAL(simulationJumpBuffer)
#endif

  /* read input vars */
  externalInputUpdate(data);
  data->callback->input_function(data);

  /* eval input vars */
  data->callback->functionODE(data);

    /* get the difference between the temp_xd(=localData->statesDerivatives)
       and xd(=statesDerivativesBackup) */
    for(i=0; i < data->modelData.nStates; i++)
    {
      delta[i] = data->localData[0]->realVars[data->modelData.nStates + i] - yd[i];
    }
    success = 1;
#if !defined(OMC_EMCC)
  MMC_CATCH_INTERNAL(simulationJumpBuffer)
#endif

  if (!success) {
    *ires = -1;
  }

  data->threadData->currentErrorStage = saveJumpState;

  data->localData[0]->timeValue = timeBackup;

  TRACE_POP
  return 0;
}

int function_ZeroCrossingsDASSL(int *neqm, double *t, double *y, double *yp,
        int *ng, double *gout, double *rpar, int* ipar)
{
  DATA* data = (DATA*)(void*)((double**)rpar)[0];

  double timeBackup;
  int saveJumpState;

  TRACE_PUSH

  saveJumpState = data->threadData->currentErrorStage;
  data->threadData->currentErrorStage = ERROR_EVENTSEARCH;

  timeBackup = data->localData[0]->timeValue;
  data->localData[0]->timeValue = *t;

  /* read input vars */
  externalInputUpdate(data);
  data->callback->input_function(data);
  /* eval needed equations*/
  data->callback->function_ZeroCrossingsEquations(data);

  data->callback->function_ZeroCrossings(data, gout);

  data->threadData->currentErrorStage = saveJumpState;
  data->localData[0]->timeValue = timeBackup;

  TRACE_POP
  return 0;
}


int functionJacAColored(DATA* data, double* jac)
{
  const int index = data->callback->INDEX_JAC_A;
  int i,j,l,k,ii;

  TRACE_PUSH

  for(i=0; i < data->simulationInfo.analyticJacobians[index].sparsePattern.maxColors; i++)
  {
    for(ii=0; ii < data->simulationInfo.analyticJacobians[index].sizeCols; ii++)
      if(data->simulationInfo.analyticJacobians[index].sparsePattern.colorCols[ii]-1 == i)
        data->simulationInfo.analyticJacobians[index].seedVars[ii] = 1;

    /*
    // debug output
    if(ACTIVE_STREAM((LOG_JAC | LOG_ENDJAC))){
      printf("Caluculate one col:\n");
      for(l=0;  l < data->simulationInfo.analyticJacobians[index].sizeCols;l++)
        infoStreamPrint((LOG_JAC | LOG_ENDJAC),"seed: data->simulationInfo.analyticJacobians[index].seedVars[%d]= %f",l,data->simulationInfo.analyticJacobians[index].seedVars[l]);
    }
    */

    data->callback->functionJacA_column(data);

    for(j = 0; j < data->simulationInfo.analyticJacobians[index].sizeCols; j++)
    {
      if(data->simulationInfo.analyticJacobians[index].seedVars[j] == 1)
      {
        if(j==0)
          ii = 0;
        else
          ii = data->simulationInfo.analyticJacobians[index].sparsePattern.leadindex[j-1];
        while(ii < data->simulationInfo.analyticJacobians[index].sparsePattern.leadindex[j])
        {
          l  = data->simulationInfo.analyticJacobians[index].sparsePattern.index[ii];
          k  = j*data->simulationInfo.analyticJacobians[index].sizeRows + l;
          jac[k] = data->simulationInfo.analyticJacobians[index].resultVars[l];
          /*infoStreamPrint((LOG_JAC | LOG_ENDJAC),"write %d. in jac[%d]-[%d,%d]=%f from col[%d]=%f",ii,k,l,j,jac[k],l,data->simulationInfo.analyticJacobians[index].resultVars[l]);*/
          ii++;
        };
      }
    }
    for(ii=0; ii < data->simulationInfo.analyticJacobians[index].sizeCols; ii++)
      if(data->simulationInfo.analyticJacobians[index].sparsePattern.colorCols[ii]-1 == i) data->simulationInfo.analyticJacobians[index].seedVars[ii] = 0;

    /*
   // debug output
    if(ACTIVE_STREAM((LOG_JAC | LOG_ENDJAC))){
      infoStreamPrint("Print jac:");
      for(l=0;  l < data->simulationInfo.analyticJacobians[index].sizeCols;l++)
      {
        for(k=0;  k < data->simulationInfo.analyticJacobians[index].sizeRows;k++)
          printf("% .5e ",jac[l+k*data->simulationInfo.analyticJacobians[index].sizeRows]);
        printf("\n");
      }
    }
    */
  }

  TRACE_POP
  return 0;
}


int functionJacASym(DATA* data, double* jac)
{
  const int index = data->callback->INDEX_JAC_A;
  unsigned int i,j,k;

  TRACE_PUSH

  k = 0;
  for(i=0; i < data->simulationInfo.analyticJacobians[index].sizeCols; i++)
  {
    data->simulationInfo.analyticJacobians[index].seedVars[i] = 1.0;

    /*
    // debug output
    if(ACTIVE_STREAM((LOG_JAC | LOG_ENDJAC)))
    {
      printf("Caluculate one col:\n");
      for(j=0;  j < data->simulationInfo.analyticJacobians[index].sizeCols;j++)
        infoStreamPrint((LOG_JAC | LOG_ENDJAC),"seed: data->simulationInfo.analyticJacobians[index].seedVars[%d]= %f",j,data->simulationInfo.analyticJacobians[index].seedVars[j]);
    }
    */

    data->callback->functionJacA_column(data);

    for(j = 0; j < data->simulationInfo.analyticJacobians[index].sizeRows; j++)
    {
      jac[k++] = data->simulationInfo.analyticJacobians[index].resultVars[j];
      /*infoStreamPrint((LOG_JAC | LOG_ENDJAC),"write in jac[%d]-[%d,%d]=%g from row[%d]=%g",k,i,j,jac[k-1],j,data->simulationInfo.analyticJacobians[index].resultVars[j]);*/
    }

    data->simulationInfo.analyticJacobians[index].seedVars[i] = 0.0;
  }
  // debug output; would be optimized away if the code compiled
  /* if(DEBUG_STREAM(LOG_DEBUG))
  {
    infoStreamPrint("Print jac:");
    for(i=0;  i < data->simulationInfo.analyticJacobians[index].sizeRows;i++)
    {
      for(j=0;  j < data->simulationInfo.analyticJacobians[index].sizeCols;j++)
        printf("% .5e ",jac[i+j*data->simulationInfo.analyticJacobians[index].sizeCols]);
      printf("\n");
    }
  } */

  TRACE_POP
  return 0;
}

/*
 * provides a analytical Jacobian to be used with DASSL
 */

static int JacobianSymbolicColored(double *t, double *y, double *yprime, double *deltaD, double *pd, double *cj, double *h, double *wt,
         double *rpar, int* ipar)
{
  DATA* data = (DATA*)(void*)((double**)rpar)[0];
  double* backupStates;
  double timeBackup;
  int i;
  int j;

  TRACE_PUSH

  backupStates = data->localData[0]->realVars;
  timeBackup = data->localData[0]->timeValue;


  data->localData[0]->timeValue = *t;
  data->localData[0]->realVars = y;
  /* read input vars */
  externalInputUpdate(data);
  data->callback->input_function(data);
  /* eval ode*/
  data->callback->functionODE(data);
  functionJacAColored(data, pd);

  /* add cj to the diagonal elements of the matrix */
  j = 0;
  for(i = 0; i < data->modelData.nStates; i++)
  {
    pd[j] -= (double) *cj;
    j += data->modelData.nStates + 1;
  }
  data->localData[0]->realVars = backupStates;
  data->localData[0]->timeValue = timeBackup;

  TRACE_POP
  return 0;
}

/*
 * provides a analytical Jacobian to be used with DASSL
 */
static int JacobianSymbolic(double *t, double *y, double *yprime, double *deltaD, double *pd, double *cj, double *h, double *wt,
         double *rpar, int* ipar)
{
  DATA* data = (DATA*)(void*)((double**)rpar)[0];
  double* backupStates;
  double timeBackup;
  int i;
  int j;

  TRACE_PUSH

  backupStates = data->localData[0]->realVars;
  timeBackup = data->localData[0]->timeValue;

  data->localData[0]->timeValue = *t;
  data->localData[0]->realVars = y;
  /* read input vars */
  externalInputUpdate(data);
  data->callback->input_function(data);
  /* eval ode*/
  data->callback->functionODE(data);
  functionJacASym(data, pd);

  /* add cj to the diagonal elements of the matrix */
  j = 0;
  for(i = 0; i < data->modelData.nStates; i++)
  {
    pd[j] -= (double) *cj;
    j += data->modelData.nStates + 1;
  }
  data->localData[0]->realVars = backupStates;
  data->localData[0]->timeValue = timeBackup;

  TRACE_POP
  return 0;
}

/*
 *  function calculates a jacobian matrix by
 *  numerical method finite differences
 */
int jacA_num(DATA* data, double *t, double *y, double *yprime, double *delta, double *matrixA, double *cj, double *h, double *wt, double *rpar, int *ipar)
{
  DASSL_DATA* dasslData = (DASSL_DATA*)(void*)((double**)rpar)[1];
  /* const int index = INDEX_JAC_A; */

  double delta_h = dasslData->sqrteps;
  double delta_hh,delta_hhh, deltaInv;
  double ysave;
  int ires;
  int i,j;

  TRACE_PUSH

  for(i=data->modelData.nStates-1; i >= 0; i--)
  {
    delta_hhh = *h * yprime[i];
    delta_hh = delta_h * fmax(fmax(abs(y[i]),abs(delta_hhh)),abs(1. / wt[i]));
    delta_hh = (delta_hhh >= 0 ? delta_hh : -delta_hh);
    delta_hh = y[i] + delta_hh - y[i];
    deltaInv = 1. / delta_hh;
    ysave = y[i];
    y[i] += delta_hh;

    /* internal dassl numerical jacobian is
     * calculated by adding cj to yprime.
     * This lead to numerical cancellations.
     */
    /*yprime[i] += *cj * delta_hh;*/

    functionODE_residual(t, y, yprime, cj, dasslData->newdelta, &ires, rpar, ipar);

    for(j = data->modelData.nStates-1; j >= 0 ; j--)
    {
      matrixA[i*data->modelData.nStates+j] = (dasslData->newdelta[j] - delta[j]) * deltaInv;
    }
    y[i] = ysave;
  }

  /*
   * Debug output
  if(ACTIVE_STREAM(LOG_JAC))
  {
    infoStreamPrint(LOG_SOLVER, "Print jac:");
    for(i=0;  i < data->simulationInfo.analyticJacobians[index].sizeRows;i++)
    {
      for(j=0;  j < data->simulationInfo.analyticJacobians[index].sizeCols;j++)
        printf("%.20e ",matrixA[i+j*data->simulationInfo.analyticJacobians[index].sizeCols]);
      printf("\n");
    }
  }
  */

  TRACE_POP
  return 0;
}

/*
 * provides a numerical Jacobian to be used with DASSL
 */
static int JacobianOwnNum(double *t, double *y, double *yprime, double *deltaD, double *pd, double *cj, double *h, double *wt,
    double *rpar, int* ipar)
{
  int i,j;
  DATA* data = (DATA*)(void*)((double**)rpar)[0];

  TRACE_PUSH

  if(jacA_num(data, t, y, yprime, deltaD, pd, cj, h, wt, rpar, ipar))
  {
    throwStreamPrint(data->threadData, "Error, can not get Matrix A ");
    TRACE_POP
    return 1;
  }
  j = 0;
  /* add cj to diagonal elements and store in pd */
  for(i = 0; i < data->modelData.nStates; i++)
  {
    pd[j] -= (double) *cj;
    j += data->modelData.nStates + 1;
  }

  TRACE_POP
  return 0;
}


/*
 *  function calculates a jacobian matrix by
 *  numerical method finite differences
 */
int jacA_numColored(DATA* data, double *t, double *y, double *yprime, double *delta, double *matrixA, double *cj, double *h, double *wt, double *rpar, int *ipar)
{
  const int index = data->callback->INDEX_JAC_A;
  DASSL_DATA* dasslData = (DASSL_DATA*)(void*)((double**)rpar)[1];
  double delta_h = dasslData->sqrteps;
  double delta_hhh;
  int ires;
  double* delta_hh = dasslData->delta_hh;
  double* ysave = dasslData->ysave;

  int i,j,l,k,ii;

  TRACE_PUSH

  for(i = 0; i < data->simulationInfo.analyticJacobians[index].sparsePattern.maxColors; i++)
  {
    for(ii=0; ii < data->simulationInfo.analyticJacobians[index].sizeCols; ii++)
    {
      if(data->simulationInfo.analyticJacobians[index].sparsePattern.colorCols[ii]-1 == i)
      {
        delta_hhh = *h * yprime[ii];
        delta_hh[ii] = delta_h * fmax(fmax(abs(y[ii]),abs(delta_hhh)),abs(1./wt[ii]));
        delta_hh[ii] = (delta_hhh >= 0 ? delta_hh[ii] : -delta_hh[ii]);
        delta_hh[ii] = y[ii] + delta_hh[ii] - y[ii];

        ysave[ii] = y[ii];
        y[ii] += delta_hh[ii];

        delta_hh[ii] = 1. / delta_hh[ii];
      }
    }

    functionODE_residual(t, y, yprime, cj, dasslData->newdelta, &ires, rpar, ipar);

    for(ii = 0; ii < data->simulationInfo.analyticJacobians[index].sizeCols; ii++)
    {
      if(data->simulationInfo.analyticJacobians[index].sparsePattern.colorCols[ii]-1 == i)
      {
        if(ii==0)
          j = 0;
        else
          j = data->simulationInfo.analyticJacobians[index].sparsePattern.leadindex[ii-1];
        while(j < data->simulationInfo.analyticJacobians[index].sparsePattern.leadindex[ii])
        {
          l  =  data->simulationInfo.analyticJacobians[index].sparsePattern.index[j];
          k  = l + ii*data->simulationInfo.analyticJacobians[index].sizeRows;
          matrixA[k] = (dasslData->newdelta[l] - delta[l]) * delta_hh[ii];
          /*infoStreamPrint(ACTIVE_STREAM(LOG_JAC),"write %d. in jac[%d]-[%d,%d]=%e",ii,k,j,l,matrixA[k]);*/
          j++;
        };
        y[ii] = ysave[ii];
      }
    }
  }

  /*
   * Debug output
  if(ACTIVE_STREAM(LOG_JAC))
  {
    infoStreamPrint(LOG_SOLVER, "Print jac:");
    for(i=0;  i < data->simulationInfo.analyticJacobians[index].sizeRows;i++)
    {
      for(j=0;  j < data->simulationInfo.analyticJacobians[index].sizeCols;j++)
        printf("%.20e ",matrixA[i+j*data->simulationInfo.analyticJacobians[index].sizeCols]);
      printf("\n");
    }
  }
  */

  TRACE_POP
  return 0;
}

/*
 * provides a numerical Jacobian to be used with DASSL
 */
static int JacobianOwnNumColored(double *t, double *y, double *yprime, double *deltaD, double *pd, double *cj, double *h, double *wt,
   double *rpar, int* ipar)
{
  DATA* data = (DATA*)(void*)((double**)rpar)[0];
  int i,j;

  TRACE_PUSH

  if(jacA_numColored(data, t, y, yprime, deltaD, pd, cj, h, wt, rpar, ipar))
  {
    throwStreamPrint(data->threadData, "Error, can not get Matrix A ");
    TRACE_POP
    return 1;
  }

  /* add cj to diagonal elements and store in pd */
  j = 0;
  for(i = 0; i < data->modelData.nStates; i++)
  {
    pd[j] -= (double) *cj;
    j += data->modelData.nStates + 1;
  }

  TRACE_POP
  return 0;
}


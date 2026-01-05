/*
 * Example: background job + spinner + progress (pseudo-code).
 *
 * Design goals:
 * - UI thread never blocks on I/O
 * - progress is reported to a model
 * - spinner animates on a low-cost tick (e.g. 25fps)
 */

typedef struct Job Job;
struct Job{
  char *id;
  int known;   // 0 spinner / 1 progressbar
  int a, b;    // progress a/b
  char *status;
  int running;
};

typedef struct Spinner Spinner;
struct Spinner{ int phase; int running; };

void workerproc(void *arg){
  Job *j = arg;
  // do work; periodically send progress
  // uiSendMsg(PROGRESS, j->id, a, b, nil);
  // uiSendMsg(DONE, j->id, b, b, "ok");
}

UiNode *JobRow(Job *j, Spinner *sp){
  // pseudo tree
  return uiRow(
    uiHStack(10,
      uiBadge(j->running ? "running" : "idle"),
      uiText(j->id),
      uiSpacer(),
      j->known ? uiProgress(j->a, j->b) : uiSpinner(sp->phase),
      uiButton("Logs", uiCmd("open logs", j->id)),
      uiButton("Cancel", uiCmd("cancel job", j->id))
    )
  );
}

void tick(void){
  // called by scheduler
  sp.phase = (sp.phase + 1) % 12;
  uiInvalidate(sp);
}

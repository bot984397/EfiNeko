#ifndef __EFI_NEKO_ANIM_H__
#define __EFI_NEKO_ANIM_H__

#define FRAME_COUNT(anim) (sizeof(anim) / sizeof((anim)[0]))

#define NEKO_FRAME_FLAG_LOOP_BEGIN  0x01
#define NEKO_FRAME_FLAG_LOOP_END    0x02

#define NEKO_ANIM_FLAG_INTERRUPT_OK 0x01

// a frame marked with NEKO_FRAME_FLAG_LOOP_BEGIN must specify a number of
// loop iterations. 0 loop iterations translates to an infinite loop.
typedef struct {
   UINT8 SpriteSheetX;     // horizontal position in the spritesheet
   UINT8 SpriteSheetY;     // vertical position in the spritesheet
   UINT8 Duration;         // number of ticks for this frame
   UINT8 Flags;            // frame flags
   UINT8 LoopIterations;   // number of loop iterations if loop is specified
   UINT8 LoopIndex;        // index into the current loop
} AnimationFrame;

typedef struct {
   AnimationFrame *Frames;
   UINT8 FrameCount;
} AnimationSequence;

typedef enum {
   NEKO_ANIM_IDLE,
   NEKO_ANIM_STARTLED,
   NEKO_ANIM_RUN_UP,
   NEKO_ANIM_RUN_UP_RIGHT,
   NEKO_ANIM_RUN_RIGHT,
   NEKO_ANIM_RUN_DOWN_RIGHT,
   NEKO_ANIM_RUN_DOWN,
   NEKO_ANIM_RUN_DOWN_LEFT,
   NEKO_ANIM_RUN_LEFT,
   NEKO_ANIM_RUN_UP_LEFT,
   NEKO_ANIM_SCRATCH_UP,
   NEKO_ANIM_SCRATCH_RIGHT,
   NEKO_ANIM_SCRATCH_DOWN,
   NEKO_ANIM_SCRATCH_LEFT,
} NekoAnimationType;

static AnimationFrame IdleFrames[] = {
   { 0, 0, 8, 0, 0, 0 },                           // Idle
   { 1, 0, 8, 0, 0, 0 },                           // Scratch 1
   { 2, 0, 8, 0, 0, 0 },                           // Scratch 2
   { 3, 0, 8, 0, 0, 0 },                           // Scratch 3
   { 4, 0, 8, 0, 0, 0 },                           // Yawn
   { 5, 0, 8, NEKO_FRAME_FLAG_LOOP_BEGIN, 0, 0 },  // Sleep 1
   { 6, 0, 8, NEKO_FRAME_FLAG_LOOP_END, 0, 0 }     // Sleep 2
};

static AnimationFrame StartledFrames[] = {
   { 7, 0, 8, 0, 0, 0 }                            // Startled
};

static AnimationFrame RunUpFrames[] = {
   { 0, 2, 8, 0, 0, 0 },                           // RunUp 1
   { 1, 2, 8, 0, 0, 0 }                            // RunUp 2
};

static AnimationFrame RunUpRightFrames[] = {
   { 6, 1, 8, 0, 0, 0 },                           // RunUpRight 1
   { 7, 1, 8, 0, 0, 0 }                            // RunUpRight 2
};

static AnimationFrame RunRightFrames[] = {
   { 4, 1, 8, 0, 0, 0 },                           // RunRight 1
   { 5, 1, 8, 0, 0, 0 }                            // RunRight 2
};

static AnimationFrame RunDownRightFrames[] = {
   { 2, 1, 8, 0, 0, 0 },                           // RunDownRight 1
   { 3, 1, 8, 0, 0, 0 }                            // RunDownRight 2
};

static AnimationFrame RunDownFrames[] = {
   { 0, 1, 8, 0, 0, 0 },                           // RunDown 1
   { 1, 1, 8, 0, 0, 0 }                            // RunDown 2
};

static AnimationFrame RunDownLeftFrames[] = {
   { 6, 2, 8, 0, 0, 0 },                           // RunDownLeft 1
   { 7, 2, 8, 0, 0, 0 }                            // RunDownLeft 2
};

static AnimationFrame RunLeftFrames[] = {
   { 4, 2, 8, 0, 0, 0 },                           // RunLeft 1
   { 5, 2, 8, 0, 0, 0 }                            // RunLeft 2
};

static AnimationFrame RunUpLeftFrames[] = {
   { 2, 2, 8, 0, 0, 0 },                           // RunUpLeft 1
   { 3, 2, 8, 0, 0, 0 }                            // RunUpLeft 2
};

static const AnimationSequence AnimationSequences[] = {
   [NEKO_ANIM_IDLE] = {
      .Frames = IdleFrames,
      .FrameCount = FRAME_COUNT(IdleFrames)
   },
   [NEKO_ANIM_STARTLED] = {
      .Frames = StartledFrames,
      .FrameCount = FRAME_COUNT(StartledFrames)
   },
   [NEKO_ANIM_RUN_UP] = {
      .Frames = RunUpFrames,
      .FrameCount = FRAME_COUNT(RunUpFrames)
   },
   [NEKO_ANIM_RUN_UP_RIGHT] = {
      .Frames = RunUpRightFrames,
      .FrameCount = FRAME_COUNT(RunUpRightFrames)
   },
   [NEKO_ANIM_RUN_RIGHT] = {
      .Frames = RunRightFrames,
      .FrameCount = FRAME_COUNT(RunRightFrames)
   },
   [NEKO_ANIM_RUN_DOWN_RIGHT] = {
      .Frames = RunDownRightFrames,
      .FrameCount = FRAME_COUNT(RunDownRightFrames)
   },
   [NEKO_ANIM_RUN_DOWN] = {
      .Frames = RunDownFrames,
      .FrameCount = FRAME_COUNT(RunDownFrames)
   },
   [NEKO_ANIM_RUN_DOWN_LEFT] = {
      .Frames = RunDownLeftFrames,
      .FrameCount = FRAME_COUNT(RunDownLeftFrames)
   },
   [NEKO_ANIM_RUN_LEFT] = {
      .Frames = RunLeftFrames,
      .FrameCount = FRAME_COUNT(RunLeftFrames)
   },
   [NEKO_ANIM_RUN_UP_LEFT] = {
      .Frames = RunUpLeftFrames,
      .FrameCount = FRAME_COUNT(RunUpLeftFrames)
   }
};

#endif // __EFI_NEKO_ANIM_H__

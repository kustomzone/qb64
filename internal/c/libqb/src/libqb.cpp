#include "common.h"
#include "libqb.h"

#include "list.h"
#include "rounding.h"
#include "memblocks.h"
#include "gfs.h"
#include "qbs.h"
#include "printer.h"
#include "icon.h"

#ifdef QB64_GUI
 #include "parts/core/glew/src/glew.c"
#endif

#ifdef QB64_WINDOWS
#include <fcntl.h>
#include <shellapi.h>
#endif

#ifdef QB64_MACOSX
#include <ApplicationServices/ApplicationServices.h>

#include "Cocoa/Cocoa.h"
#include <CoreFoundation/CoreFoundation.h>
#include <objc/objc.h>
#include <objc/objc-runtime.h>

#include <mach-o/dyld.h> //required for _NSGetExecutablePath

#endif


    int32 disableEvents=0;

    //This next block used to be in common.cpp; put here until I can find a better
    //place for it (LC, 2018-01-05)
#ifndef QB64_WINDOWS
    void Sleep(uint32 milliseconds){
        static uint64 sec,nsec;
        sec=milliseconds/1000;
        nsec=(milliseconds%1000)*1000000;
        static timespec ts;
        ts.tv_sec = sec;
        ts.tv_nsec = nsec;
        nanosleep (&ts, NULL);
}

uint32 _lrotl(uint32 word,uint32 shift){
  return (word << shift) | (word >> (32 - shift));
}

void ZeroMemory(void *ptr,int64 bytes){
  memset(ptr,0,bytes);
}
#endif
//bit-array access functions (note: used to be included through 'bit.cpp')
uint64 getubits(uint32 bsize,uint8 *base,ptrszint i){
  int64 bmask;
  bmask=~(-(((int64)1)<<bsize));
  i*=bsize;
  return ((*(uint64*)(base+(i>>3)))>>(i&7))&bmask;
}
int64 getbits(uint32 bsize,uint8 *base,ptrszint i){
  int64 bmask, bval64;
  bmask=~(-(((int64)1)<<bsize));
  i*=bsize;
  bval64=((*(uint64*)(base+(i>>3)))>>(i&7))&bmask;
  if (bval64&(((int64)1)<<(bsize-1))) return bval64|(~bmask);
  return bval64;
}
void setbits(uint32 bsize,uint8 *base,ptrszint i,int64 val){
  int64 bmask;
  uint64 *bptr64;
  bmask=(((uint64)1)<<bsize)-1;
  i*=bsize;
  bptr64=(uint64*)(base+(i>>3));
  *bptr64=(*bptr64&( ( (bmask<<(i&7)) ^-1)  )) | ((val&bmask)<<(i&7));
}


#ifdef QB64_UNIX
#include <pthread.h>
#include <libgen.h> //required for dirname()
#endif

#ifdef QB64_LINUX
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
Display *X11_display=NULL;
Window X11_window;
#endif

int32 x11_locked=0;
int32 x11_lock_request=0;
void x11_lock(){
  x11_lock_request=1; while (x11_locked==0) Sleep(1);
}
void x11_unlock(){
  x11_locked=0;
}



/*
Logging for QB64 developers (when an alert() just isn't enough)
        1) Temporarily set allow_logging=1
        2) Call log with a string or number:
                log_event("this is a char* string");
                log_event(12345);
        3) 'log.txt' is created in the same folder as your executable
        * 'log.txt' is truncated every time your program runs on the first call to log_event(...)
*/
int32 allow_logging=1;
std::ofstream log_file;
int32 log_file_opened=0;
void open_log_file(){
  if (log_file_opened==0){
    log_file.open("log.txt", std::ios_base::out|std::ios_base::trunc);
    log_file_opened=1;    
  }
}
void log_event(char *x){
  open_log_file();
  log_file << x;  
}
void log_event(int32 x){
  open_log_file();
  char str[1000];
  memset(&str[0],0,1000);
  sprintf(str, "%d", x);
  log_file << &str[0];
}


//GUI notification variables
int32 force_display_update=0;

void sub__delay(double seconds);

void *generic_window_handle=NULL;
#ifdef QB64_WINDOWS
HWND window_handle=NULL;
#endif
//...

extern "C" void QB64_Window_Handle(void *handle){
  generic_window_handle=handle;
#ifdef QB64_WINDOWS
  window_handle=(HWND)handle; 
#endif
  //...
}



//forward references
void set_view(int32 new_mode);
void set_render_source(int32 new_handle);
void set_render_dest(int32 new_handle);
void reinit_glut_callbacks();
void showErrorOnScreen(char *errorMessage, int32 errorNumber, int32 lineNumber);//display error message on screen and enter infinite loop

int32 framebufferobjects_supported=0;

int32 environment_2d__screen_width=0; //the size of the software SCREEN
int32 environment_2d__screen_height=0;
int32 environment__window_width=0; //window may be larger or smaller than the SCREEN
int32 environment__window_height=0;
int32 environment_2d__screen_x1=0; //offsets of 'screen' within the window
int32 environment_2d__screen_y1=0;
int32 environment_2d__screen_x2=0;
int32 environment_2d__screen_y2=0;
int32 environment_2d__screen_scaled_width=640;//inital values prevent _SCALEDWIDTH/_SCALEDHEIGHT returning 0
int32 environment_2d__screen_scaled_height=400;
float environment_2d__screen_x_scale=1.0f;
float environment_2d__screen_y_scale=1.0f;
int32 environment_2d__screen_smooth=0;//1(LINEAR) or 0(NEAREST)
int32 environment_2d__letterbox=0;//1=vertical black stripes required, 2=horizontal black stripes required




int32 qloud_next_input_index=1;

int32 window_exists=0;
int32 create_window=0;
int32 window_focused=0; //Not used on Windows
uint8 *window_title=NULL;

double max_fps=60;//60 is the default
int32 auto_fps=0;//set to 1 to make QB64 auto-adjust fps based on load

int32 os_resize_event=0;

int32 resize_auto=0;//1=_STRETCH, 2=_SMOOTH
float resize_auto_ideal_aspect=640.0/400.0;
float resize_auto_accept_aspect=640.0/400.0;

int32 fullscreen_smooth=0;
int32 fullscreen_width=0;
int32 fullscreen_height=0;
int32 screen_scale=0;
int32 resize_pending=1;
int32 resize_snapback=1;
int32 resize_snapback_x=640;
int32 resize_snapback_y=400;
int32 resize_event=0;
int32 resize_event_x=0;
int32 resize_event_y=0;

int32 ScreenResizeScale=0;
int32 ScreenResize=0;
extern "C" int QB64_Resizable(){
  return ScreenResize;
}


int32 sub_gl_called=0;

extern void evnt(uint32 linenumber, uint32 inclinenumber = 0, const char* incfilename = NULL);

extern "C" int qb64_custom_event(int event,int v1,int v2,int v3,int v4,int v5,int v6,int v7,int v8,void *p1,void *p2);
#ifdef QB64_WINDOWS
  extern "C" LRESULT qb64_os_event_windows(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, int *qb64_os_event_info);
#endif

#if defined(QB64_LINUX) && defined(QB64_GUI)
  extern "C" void qb64_os_event_linux(XEvent *event, Display *display, int *qb64_os_event_info);
#endif

#define QB64_EVENT_CLOSE 1
#define QB64_EVENT_KEY 2
#define QB64_EVENT_RELATIVE_MOUSE_MOVEMENT 3
#define QB64_EVENT_KEY_PAUSE 1000


static int32 display_x=640;
static int32 display_y=400;
int32 display_x_prev=640,display_y_prev=400;

static int32 display_required_x=640;
static int32 display_required_y=400;

int32 dont_call_sub_gl=0;

void GLUT_DISPLAY_REQUEST();

void timerCB(int millisec)//not currently being used
{
#ifdef QB64_GLUT
  glutPostRedisplay();
  glutTimerFunc(millisec, timerCB, millisec);
#endif
}


struct display_frame_struct{
  int32 state;
  int64 order;
  uint32 *bgra;
  int32 w;
  int32 h;
  int32 bytes;//w*h*4
};
display_frame_struct display_frame[3];
int64 display_frame_order_next=1;
#define DISPLAY_FRAME_STATE__EMPTY 1
#define DISPLAY_FRAME_STATE__BUILDING 2
#define DISPLAY_FRAME_STATE__READY 3
#define DISPLAY_FRAME_STATE__DISPLAYING 4

//when a new software frame is not required by display(), hardware content may exist,
//and if it does then this variable is used to determine to highest order index to render
int64 last_rendered_hardware_display_frame_order=0;
int64 last_hardware_display_frame_order=0;



//Special Handle system
//---------------------
//Purpose: Manage handles to custom QB64 interfaces alongside the standard QB file handle indexing
//Method:  Uses negative values for custom interface handles
struct special_handle_struct{
  int8 type;
  //0=Invalid handle (was previously being used but was closed)
  //1=Stream (eg. COM, Network[TCP/IP])
  //2=Host Listener (used by _OPENCONNECTION)
  ptrszint index;//an index or pointer to the type's object
};
list *special_handles=NULL;

//Stream system
//-------------
//Purpose: Unify access to the input and/or output of streamed data
struct stream_struct{
  uint8 *in;
  ptrszint in_size;//current size in bytes
  ptrszint in_limit;//size before reallocation of buffer is required
  int8 eof;//user attempted to read past end of stream
  //Note: 'out' is unrequired because data can be sent directly to the interface
  //-----------------------------------------
  int8 type;
  //1=Network (TCP)
  ptrszint index;//an index or pointer to the type's object
};
list *stream_handles=NULL;

void stream_free(stream_struct *st){
  if (st->in_limit) free(st->in);
  list_remove(stream_handles,list_get_index(stream_handles,st));
}

void stream_update(stream_struct *stream);
void stream_out(stream_struct *st,void *offset,ptrszint bytes);

void connection_close(ptrszint i);


/********** Render State **********/
/*
Apart from 'glTexParameter' based settings (with are texture specific)
all other OpenGL states are global.
This means when switching between dest FBOs a complete state change is inevitable.
*/
struct RENDER_STATE_DEST{ //could be the primary render target or a FBO
        int32 ignore;//at present no relevant states appear to be FBO specific
};
struct RENDER_STATE_SOURCE{ //texture states
        int32 smooth_stretched;
        int32 smooth_shrunk;
        int32 texture_wrap;
        int32 PO2_fix;
};
struct RENDER_STATE_GLOBAL{ //settings not bound to specific source/target
        RENDER_STATE_DEST *dest;
        RENDER_STATE_SOURCE *source;
        int32 dest_handle;        
        int32 source_handle;
        int32 view_mode;
        int32 use_alpha;
        int32 depthbuffer_mode;
        int32 cull_mode;
};
RENDER_STATE_GLOBAL render_state;
RENDER_STATE_DEST dest_render_state0;
#define VIEW_MODE__UNKNOWN 0
#define VIEW_MODE__2D 1
#define VIEW_MODE__3D 2
#define VIEW_MODE__RESET 3
#define ALPHA_MODE__UNKNOWN -1
#define ALPHA_MODE__DONT_BLEND 0
#define ALPHA_MODE__BLEND 1
#define TEXTURE_WRAP_MODE__UNKNOWN -1
#define TEXTURE_WRAP_MODE__DONT_WRAP 0
#define TEXTURE_WRAP_MODE__WRAP 1
#define SMOOTH_MODE__UNKNOWN -1
#define SMOOTH_MODE__DONT_SMOOTH 0
#define SMOOTH_MODE__SMOOTH 1
#define PO2_FIX__OFF 0
#define PO2_FIX__EXPANDED 1
#define PO2_FIX__MIPMAPPED 2


#define DEPTHBUFFER_MODE__UNKNOWN -1
#define DEPTHBUFFER_MODE__OFF 0
#define DEPTHBUFFER_MODE__ON 1
#define DEPTHBUFFER_MODE__LOCKED 2
#define DEPTHBUFFER_MODE__CLEAR 3
#define CULL_MODE__UNKNOWN -1
#define CULL_MODE__NONE 0
#define CULL_MODE__CLOCKWISE_ONLY 1
#define CULL_MODE__ANTICLOCKWISE_ONLY 2
/********** Render State **********/

int32 depthbuffer_mode0=DEPTHBUFFER_MODE__ON;
int32 depthbuffer_mode1=DEPTHBUFFER_MODE__ON;

#define INVALID_HARDWARE_HANDLE -1

struct hardware_img_struct{
  int32 w;
  int32 h;
  int32 texture_handle;//if 0, imports from software_pixel_buffer automatically
  int32 dest_context_handle;//used when rendering other images onto this image
  int32 depthbuffer_handle;//generated when 3D commands are called
  int32 pending_commands;//incremented with each command, decremented after command is processed
  int32 remove;//if =1, free immediately after all pending commands are processed
  uint32 *software_pixel_buffer;//if NULL, generates a blank texture
  int32 alpha_disabled;//changed by _BLEND/_DONTBLEND commands
  int32 depthbuffer_mode;//changed by _DEPTHBUFFER
  int32 valid;
  RENDER_STATE_SOURCE source_state;
  RENDER_STATE_DEST dest_state;  
  int32 PO2_w;//if PO2_FIX__EXPANDED/MIPMAPPED, these are the texture size
  int32 PO2_h;
};
list *hardware_img_handles=NULL;

int32 first_hardware_command=0;//only set once
int32 last_hardware_command_added=0;//0 if none exist
int32 last_hardware_command_rendered=0;//0 if all have been processed
int32 next_hardware_command_to_remove=0;//0 if all have been processed

struct hardware_graphics_command_struct{
  int64 order;//which _DISPLAY event to bind the operation to
  int32 next_command;//the handle of the next hardware_graphics_command of the same display-order, of 0 if last
  int64 command;//the command type, actually a set of bit flags
  //Bit 00: Decimal value 000001: _PUTIMAGE
  union{
    int32 option;
    int32 src_img; //MUST be a hardware handle
  };
  union{
    int32 dst_img; //MUST be a hardware handle or 0 for the default 2D rendering context  
    int32 target;
  };
  float src_x1;
  float src_y1;
  float src_x2;
  float src_y2;
  float src_x3;
  float src_y3;
  float dst_x1;
  float dst_y1;
  float dst_z1;
  float dst_x2;
  float dst_y2;
  float dst_z2;
  float dst_x3;
  float dst_y3;
  float dst_z3;
  int32 smooth;//0 or 1 (whether to apply texture filtering)
  int32 cull_mode;
  int32 depthbuffer_mode;
  int32 use_alpha;//0 or 1 (whether to refer to the alpha component of pixel values)
  int32 remove;
};
list *hardware_graphics_command_handles=NULL;
#define HARDWARE_GRAPHICS_COMMAND__PUTIMAGE 1
#define HARDWARE_GRAPHICS_COMMAND__FREEIMAGE_REQUEST 2
#define HARDWARE_GRAPHICS_COMMAND__FREEIMAGE 3
#define HARDWARE_GRAPHICS_COMMAND__MAPTRIANGLE 4
#define HARDWARE_GRAPHICS_COMMAND__MAPTRIANGLE3D 5
#define HARDWARE_GRAPHICS_COMMAND__CLEAR_DEPTHBUFFER 6

int32 SOFTWARE_IMG_HANDLE_MIN=-8388608;
int32 HARDWARE_IMG_HANDLE_OFFSET=-16777216;//added to all hardware image handles to avoid collision
                                           //the lowest integer value a single precision number can exactly represent,
                                           //because users put handles in SINGLEs
#define NEW_HARDWARE_IMG__BUFFER_CONTENT 1
#define NEW_HARDWARE_IMG__DUPLICATE_PROVIDED_BUFFER 2

#include "libqb/gui.h"

  //these lock values increment
  int64 display_lock_request=0;
  int64 display_lock_confirmed=0;
  int64 display_lock_released=0;

//note: only to be used by user functions, not internal functions
hardware_img_struct *get_hardware_img(int32 handle){
  hardware_img_struct *img;
  if (handle<HARDWARE_IMG_HANDLE_OFFSET||handle>=SOFTWARE_IMG_HANDLE_MIN) return NULL;
  img=(hardware_img_struct*)list_get(hardware_img_handles,handle-HARDWARE_IMG_HANDLE_OFFSET);
  if (img==NULL) return NULL;
  if (!img->valid) return NULL;
  return img;
}
int32 get_hardware_img_index(int32 handle){
  return handle-HARDWARE_IMG_HANDLE_OFFSET;
}

static uint16 codepage437_to_unicode16[] = {
  0x0020,0x263A,0x263B,0x2665,0x2666,0x2663,0x2660,0x2022,0x25D8,0x25CB,0x25D9,0x2642,0x2640,0x266A,0x266B,0x263C,
  0x25BA,0x25C4,0x2195,0x203C,0x00B6,0x00A7,0x25AC,0x21A8,0x2191,0x2193,0x2192,0x2190,0x221F,0x2194,0x25B2,0x25BC,
  0x0020,0x0021,0x0022,0x0023,0x0024,0x0025,0x0026,0x0027,0x0028,0x0029,0x002A,0x002B,0x002C,0x002D,0x002E,0x002F,
  0x0030,0x0031,0x0032,0x0033,0x0034,0x0035,0x0036,0x0037,0x0038,0x0039,0x003A,0x003B,0x003C,0x003D,0x003E,0x003F,
  0x0040,0x0041,0x0042,0x0043,0x0044,0x0045,0x0046,0x0047,0x0048,0x0049,0x004A,0x004B,0x004C,0x004D,0x004E,0x004F,
  0x0050,0x0051,0x0052,0x0053,0x0054,0x0055,0x0056,0x0057,0x0058,0x0059,0x005A,0x005B,0x005C,0x005D,0x005E,0x005F,
  0x0060,0x0061,0x0062,0x0063,0x0064,0x0065,0x0066,0x0067,0x0068,0x0069,0x006A,0x006B,0x006C,0x006D,0x006E,0x006F,
  0x0070,0x0071,0x0072,0x0073,0x0074,0x0075,0x0076,0x0077,0x0078,0x0079,0x007A,0x007B,0x007C,0x007D,0x007E,0x2302,
  0x00C7,0x00FC,0x00E9,0x00E2,0x00E4,0x00E0,0x00E5,0x00E7,0x00EA,0x00EB,0x00E8,0x00EF,0x00EE,0x00EC,0x00C4,0x00C5,
  0x00C9,0x00E6,0x00C6,0x00F4,0x00F6,0x00F2,0x00FB,0x00F9,0x00FF,0x00D6,0x00DC,0x00A2,0x00A3,0x00A5,0x20A7,0x0192,
  0x00E1,0x00ED,0x00F3,0x00FA,0x00F1,0x00D1,0x00AA,0x00BA,0x00BF,0x2310,0x00AC,0x00BD,0x00BC,0x00A1,0x00AB,0x00BB,
  0x2591,0x2592,0x2593,0x2502,0x2524,0x2561,0x2562,0x2556,0x2555,0x2563,0x2551,0x2557,0x255D,0x255C,0x255B,0x2510,
  0x2514,0x2534,0x252C,0x251C,0x2500,0x253C,0x255E,0x255F,0x255A,0x2554,0x2569,0x2566,0x2560,0x2550,0x256C,0x2567,
  0x2568,0x2564,0x2565,0x2559,0x2558,0x2552,0x2553,0x256B,0x256A,0x2518,0x250C,0x2588,0x2584,0x258C,0x2590,0x2580,
  0x03B1,0x00DF,0x0393,0x03C0,0x03A3,0x03C3,0x00B5,0x03C4,0x03A6,0x0398,0x03A9,0x03B4,0x221E,0x03C6,0x03B5,0x2229,
  0x2261,0x00B1,0x2265,0x2264,0x2320,0x2321,0x00F7,0x2248,0x00B0,0x2219,0x00B7,0x221A,0x207F,0x00B2,0x25A0,0x0020
};


#ifdef QB64_BACKSLASH_FILESYSTEM
#include "parts\\video\\font\\ttf\\src.c"
#else
#include "parts/video/font/ttf/src.c"
#endif

#ifdef QB64_MACOSX
#include <mach/mach_time.h>
#define ORWL_NANO (+1.0E-9)
#define ORWL_GIGA UINT64_C(1000000000)
static double orwl_timebase = 0.0;
static uint64_t orwl_timestart = 0;
int64 orwl_gettime(void) {
        if (!orwl_timestart) {
                mach_timebase_info_data_t tb = { 0 };
                mach_timebase_info(&tb);
                orwl_timebase = tb.numer;
                orwl_timebase /= tb.denom;
                orwl_timestart = mach_absolute_time();
        }
        struct timespec t;
        double diff = (mach_absolute_time() - orwl_timestart) * orwl_timebase;
        t.tv_sec = diff * ORWL_NANO;
        t.tv_nsec = diff - (t.tv_sec * ORWL_GIGA);                
    return t.tv_sec * 1000 + t.tv_nsec / 1000000;                
}
#endif

#ifdef QB64_LINUX
int64 GetTicks(){
  struct timespec tp;
  clock_gettime(CLOCK_MONOTONIC, &tp);
  return tp.tv_sec * 1000 + tp.tv_nsec / 1000000;
}
#elif defined QB64_MACOSX
int64 GetTicks(){
    return orwl_gettime();
}
#else
int64 GetTicks(){
        return ( ( ((int64)clock()) * ((int64)1000) ) / ((int64)CLOCKS_PER_SEC) );        
}
#endif


#define QBK 200000
#define VK 100000
#define UC 1073741824
/* QBK codes:
   200000-200010: Numpad keys with Num-Lock off
   NO_NUMLOCK_KP0=INSERT
   NO_NUMLOCK_KP1=END
   NO_NUMLOCK_KP2=DOWN
   NO_NUMLOCK_KP3=PGDOWN
   NO_NUMLOCK_KP4...
   NO_NUMLOCK_KP5
   NO_NUMLOCK_KP6
   NO_NUMLOCK_KP7
   NO_NUMLOCK_KP8
   NO_NUMLOCK_KP9
   NO_NUMLOCK_KP_PERIOD=DEL
   200011: SCROLL_LOCK_ON
   200012: INSERT_MODE_ON
*/
#define QBK_SCROLL_LOCK_MODE 11
#define QBK_INSERT_MODE 12
#define QBK_CHR0 13
typedef enum {
  QBVK_UNKNOWN    = 0,
  QBVK_FIRST      = 0,
  QBVK_BACKSPACE  = 8,
  QBVK_TAB        = 9,
  QBVK_CLEAR      = 12,
  QBVK_RETURN     = 13,
  QBVK_PAUSE      = 19,
  QBVK_ESCAPE     = 27,
  QBVK_SPACE      = 32,
  QBVK_EXCLAIM    = 33,
  QBVK_QUOTEDBL   = 34,
  QBVK_HASH       = 35,
  QBVK_DOLLAR     = 36,
  QBVK_AMPERSAND  = 38,
  QBVK_QUOTE      = 39,
  QBVK_LEFTPAREN  = 40,
  QBVK_RIGHTPAREN = 41,
  QBVK_ASTERISK   = 42,
  QBVK_PLUS       = 43,
  QBVK_COMMA      = 44,
  QBVK_MINUS      = 45,
  QBVK_PERIOD     = 46,
  QBVK_SLASH      = 47,
  QBVK_0          = 48,
  QBVK_1          = 49,
  QBVK_2          = 50,
  QBVK_3          = 51,
  QBVK_4          = 52,
  QBVK_5          = 53,
  QBVK_6          = 54,
  QBVK_7          = 55,
  QBVK_8          = 56,
  QBVK_9          = 57,
  QBVK_COLON      = 58,
  QBVK_SEMICOLON  = 59,
  QBVK_LESS       = 60,
  QBVK_EQUALS     = 61,
  QBVK_GREATER    = 62,
  QBVK_QUESTION   = 63,
  QBVK_AT         = 64,
  //Skip uppercase letters
  QBVK_LEFTBRACKET= 91,
  QBVK_BACKSLASH  = 92,
  QBVK_RIGHTBRACKET = 93,
  QBVK_CARET      = 94,
  QBVK_UNDERSCORE = 95,
  QBVK_BACKQUOTE  = 96,
  QBVK_a          = 97,
  QBVK_b          = 98,
  QBVK_c          = 99,
  QBVK_d          = 100,
  QBVK_e          = 101,
  QBVK_f          = 102,
  QBVK_g          = 103,
  QBVK_h          = 104,
  QBVK_i          = 105,
  QBVK_j          = 106,
  QBVK_k          = 107,
  QBVK_l          = 108,
  QBVK_m          = 109,
  QBVK_n          = 110,
  QBVK_o          = 111,
  QBVK_p          = 112,
  QBVK_q          = 113,
  QBVK_r          = 114,
  QBVK_s          = 115,
  QBVK_t          = 116,
  QBVK_u          = 117,
  QBVK_v          = 118,
  QBVK_w          = 119,
  QBVK_x          = 120,
  QBVK_y          = 121,
  QBVK_z          = 122,
  QBVK_DELETE     = 127,
  //End of ASCII mapped QBVKs
  //International QBVKs
  QBVK_WORLD_0        = 160,      /* 0xA0 */
  QBVK_WORLD_1        = 161,
  QBVK_WORLD_2        = 162,
  QBVK_WORLD_3        = 163,
  QBVK_WORLD_4        = 164,
  QBVK_WORLD_5        = 165,
  QBVK_WORLD_6        = 166,
  QBVK_WORLD_7        = 167,
  QBVK_WORLD_8        = 168,
  QBVK_WORLD_9        = 169,
  QBVK_WORLD_10       = 170,
  QBVK_WORLD_11       = 171,
  QBVK_WORLD_12       = 172,
  QBVK_WORLD_13       = 173,
  QBVK_WORLD_14       = 174,
  QBVK_WORLD_15       = 175,
  QBVK_WORLD_16       = 176,
  QBVK_WORLD_17       = 177,
  QBVK_WORLD_18       = 178,
  QBVK_WORLD_19       = 179,
  QBVK_WORLD_20       = 180,
  QBVK_WORLD_21       = 181,
  QBVK_WORLD_22       = 182,
  QBVK_WORLD_23       = 183,
  QBVK_WORLD_24       = 184,
  QBVK_WORLD_25       = 185,
  QBVK_WORLD_26       = 186,
  QBVK_WORLD_27       = 187,
  QBVK_WORLD_28       = 188,
  QBVK_WORLD_29       = 189,
  QBVK_WORLD_30       = 190,
  QBVK_WORLD_31       = 191,
  QBVK_WORLD_32       = 192,
  QBVK_WORLD_33       = 193,
  QBVK_WORLD_34       = 194,
  QBVK_WORLD_35       = 195,
  QBVK_WORLD_36       = 196,
  QBVK_WORLD_37       = 197,
  QBVK_WORLD_38       = 198,
  QBVK_WORLD_39       = 199,
  QBVK_WORLD_40       = 200,
  QBVK_WORLD_41       = 201,
  QBVK_WORLD_42       = 202,
  QBVK_WORLD_43       = 203,
  QBVK_WORLD_44       = 204,
  QBVK_WORLD_45       = 205,
  QBVK_WORLD_46       = 206,
  QBVK_WORLD_47       = 207,
  QBVK_WORLD_48       = 208,
  QBVK_WORLD_49       = 209,
  QBVK_WORLD_50       = 210,
  QBVK_WORLD_51       = 211,
  QBVK_WORLD_52       = 212,
  QBVK_WORLD_53       = 213,
  QBVK_WORLD_54       = 214,
  QBVK_WORLD_55       = 215,
  QBVK_WORLD_56       = 216,
  QBVK_WORLD_57       = 217,
  QBVK_WORLD_58       = 218,
  QBVK_WORLD_59       = 219,
  QBVK_WORLD_60       = 220,
  QBVK_WORLD_61       = 221,
  QBVK_WORLD_62       = 222,
  QBVK_WORLD_63       = 223,
  QBVK_WORLD_64       = 224,
  QBVK_WORLD_65       = 225,
  QBVK_WORLD_66       = 226,
  QBVK_WORLD_67       = 227,
  QBVK_WORLD_68       = 228,
  QBVK_WORLD_69       = 229,
  QBVK_WORLD_70       = 230,
  QBVK_WORLD_71       = 231,
  QBVK_WORLD_72       = 232,
  QBVK_WORLD_73       = 233,
  QBVK_WORLD_74       = 234,
  QBVK_WORLD_75       = 235,
  QBVK_WORLD_76       = 236,
  QBVK_WORLD_77       = 237,
  QBVK_WORLD_78       = 238,
  QBVK_WORLD_79       = 239,
  QBVK_WORLD_80       = 240,
  QBVK_WORLD_81       = 241,
  QBVK_WORLD_82       = 242,
  QBVK_WORLD_83       = 243,
  QBVK_WORLD_84       = 244,
  QBVK_WORLD_85       = 245,
  QBVK_WORLD_86       = 246,
  QBVK_WORLD_87       = 247,
  QBVK_WORLD_88       = 248,
  QBVK_WORLD_89       = 249,
  QBVK_WORLD_90       = 250,
  QBVK_WORLD_91       = 251,
  QBVK_WORLD_92       = 252,
  QBVK_WORLD_93       = 253,
  QBVK_WORLD_94       = 254,
  QBVK_WORLD_95       = 255,      /* 0xFF */
  //Numeric keypad
  QBVK_KP0            = 256,
  QBVK_KP1            = 257,
  QBVK_KP2            = 258,
  QBVK_KP3            = 259,
  QBVK_KP4            = 260,
  QBVK_KP5            = 261,
  QBVK_KP6            = 262,
  QBVK_KP7            = 263,
  QBVK_KP8            = 264,
  QBVK_KP9            = 265,
  QBVK_KP_PERIOD      = 266,
  QBVK_KP_DIVIDE      = 267,
  QBVK_KP_MULTIPLY    = 268,
  QBVK_KP_MINUS       = 269,
  QBVK_KP_PLUS        = 270,
  QBVK_KP_ENTER       = 271,
  QBVK_KP_EQUALS      = 272,
  //Arrows + Home/End pad
  QBVK_UP         = 273,
  QBVK_DOWN       = 274,
  QBVK_RIGHT      = 275,
  QBVK_LEFT       = 276,
  QBVK_INSERT     = 277,
  QBVK_HOME       = 278,
  QBVK_END        = 279,
  QBVK_PAGEUP     = 280,
  QBVK_PAGEDOWN   = 281,
  //Function keys
  QBVK_F1         = 282,
  QBVK_F2         = 283,
  QBVK_F3         = 284,
  QBVK_F4         = 285,
  QBVK_F5         = 286,
  QBVK_F6         = 287,
  QBVK_F7         = 288,
  QBVK_F8         = 289,
  QBVK_F9         = 290,
  QBVK_F10        = 291,
  QBVK_F11        = 292,
  QBVK_F12        = 293,
  QBVK_F13        = 294,
  QBVK_F14        = 295,
  QBVK_F15        = 296,
  //Key state modifier keys
  QBVK_NUMLOCK    = 300,
  QBVK_CAPSLOCK   = 301,
  QBVK_SCROLLOCK  = 302,
  //If more modifiers are added, the window defocus code in qb64_os_event_linux must be altered
  QBVK_RSHIFT     = 303,
  QBVK_LSHIFT     = 304,
  QBVK_RCTRL      = 305,
  QBVK_LCTRL      = 306,
  QBVK_RALT       = 307,
  QBVK_LALT       = 308,
  QBVK_RMETA      = 309,
  QBVK_LMETA      = 310,
  QBVK_LSUPER     = 311,      /* Left "Windows" key */
  QBVK_RSUPER     = 312,      /* Right "Windows" key */
  QBVK_MODE       = 313,      /* "Alt Gr" key */
  QBVK_COMPOSE        = 314,      /* Multi-key compose key */
  //Miscellaneous function keys
  QBVK_HELP       = 315,
  QBVK_PRINT      = 316,
  QBVK_SYSREQ     = 317,
  QBVK_BREAK      = 318,
  QBVK_MENU       = 319,
  QBVK_POWER      = 320,      /* Power Macintosh power key */
  QBVK_EURO       = 321,      /* Some european keyboards */
  QBVK_UNDO       = 322,      /* Atari keyboard has Undo */
  QBVK_LAST
}QBVKs;
//Enumeration of valid key mods (possibly OR'd together)
typedef enum {
  KMOD_NONE  = 0x0000,
  KMOD_LSHIFT= 0x0001,
  KMOD_RSHIFT= 0x0002,
  KMOD_LCTRL = 0x0040,
  KMOD_RCTRL = 0x0080,
  KMOD_LALT  = 0x0100,
  KMOD_RALT  = 0x0200,
  KMOD_LMETA = 0x0400,
  KMOD_RMETA = 0x0800,
  KMOD_NUM   = 0x1000,
  KMOD_CAPS  = 0x2000,
  KMOD_MODE  = 0x4000,
  KMOD_RESERVED = 0x8000
}KMODs;
#define KMOD_CTRL   (KMOD_LCTRL|KMOD_RCTRL)
#define KMOD_SHIFT  (KMOD_LSHIFT|KMOD_RSHIFT)
#define KMOD_ALT    (KMOD_LALT|KMOD_RALT)
#define KMOD_META   (KMOD_LMETA|KMOD_RMETA)



extern int32 cloud_app;
int32 cloud_chdir_complete=0;
int32 cloud_port[8];

/* Restricted Functionality: (Security focused approach, does not include restricting sound etc)

   Block while compiling: (ONLY things that cannot be caught at runtime)
   - $CHECKING:OFF [X]
   - _MEM(x,y) [X]
   - DECLARE LIBRARY [X]

   Block at runtime:
   - paths [fixdir]
   - MKDIR [sub_mkdir]
   - SHELL(subs/functions)
   [func__shellhide,
   func__shell,
   sub_shell,
   sub_shell2,
   sub_shell3,
   sub_shell4]
   - RUN "filename" [sub_run]
   - CHAIN [sub_chain]
   - SCREENPRINT [sub__screenprint]
   - SCREENCLICK [sub__screenclick]
   - SCREENIMAGE (returns a blank 1024x768 image)[func__screenimage]
   - ENVIRON [func_environ(num&str), sub_environ]

   Reference notes:
   - KILL calls fixdir()

   Ports:
   - Client connections are unrestricted
   - Host ports values are either 1 or 2, but the default is port 1 for out of range values

*/


/*
  int32 allocated_bytes=0;
  void *malloc2(int x){
  allocated_bytes+=x;
  return malloc(x);
  }
  void *realloc2(void *x, int y){
  allocated_bytes+=y;
  return realloc(x,y);
  }
  void *calloc2(int x, int y){
  allocated_bytes+=(x*y);
  return calloc(x,y);
  }
  #define malloc(x) malloc2(x)
  #define calloc(x,y) calloc2(x,y)
  #define realloc(x,y) realloc2(x,y)
*/

int64 device_event_index=0;
int32 device_mouse_relative=0;

int32 lock_mainloop=0;//0=unlocked, 1=lock requested, 2=locked
 
int32 lpos=1;
int32 width_lprint=80;

//forward refs
void sub_shell4(qbs*,int32);//_DONTWAIT & _HIDE
int32 func__source();
int32 func_pos(int32 ignore);

double func_timer(double accuracy,int32 passed);
int32 func__newimage(int32 x,int32 y,int32 bpp,int32 passed);
void display();
void validatepage(int32);
void sub__dest(int32);
void sub__source(int32);
int32 func__printwidth(qbs*,int32,int32);
void sub_cls(int32,uint32,int32);
void qbs_print(qbs*,int32);
int32 func__copyimage(int32 i,int32 mode,int32 passed);
int32 func__dest();
int32 func__display();
void qbg_sub_view_print(int32,int32,int32);
qbs *qbs_new(int32,uint8);
qbs *qbs_new_txt(const char*);
qbs *qbs_add(qbs*,qbs*);
qbs *qbs_set(qbs*,qbs*);
void qbg_sub_window(float,float,float,float,int32);
int32 autodisplay=1;
extern qbs *qbs_str(int64 value);
extern qbs *qbs_str(int32 value);
extern qbs *qbs_str(int16 value);
extern qbs *qbs_str(int8 value);
extern qbs *qbs_str(uint64 value);
extern qbs *qbs_str(uint32 value);
extern qbs *qbs_str(uint16 value);
extern qbs *qbs_str(uint8 value);
extern qbs *qbs_str(float value);
extern qbs *qbs_str(double value);
extern qbs *qbs_str(long double value);
void key_update();
int32 key_display_state=0;
int32 key_display=0;
int32 key_display_redraw=0;

extern int32 device_last;
extern int32 device_max;
extern device_struct *devices;

extern uint8 getDeviceEventButtonValue(device_struct *device, int32 eventIndex, int32 objectIndex);
extern void setDeviceEventButtonValue(device_struct *device, int32 eventIndex, int32 objectIndex, uint8 value);
extern float getDeviceEventAxisValue(device_struct *device, int32 eventIndex, int32 objectIndex);
extern void setDeviceEventAxisValue(device_struct *device, int32 eventIndex, int32 objectIndex, float value);
extern float getDeviceEventWheelValue(device_struct *device, int32 eventIndex, int32 objectIndex);
extern void setDeviceEventWheelValue(device_struct *device, int32 eventIndex, int32 objectIndex, float value);
extern void setupDevice(device_struct *device);
extern int32 createDeviceEvent(device_struct *device);
extern void commitDeviceEvent(device_struct *device);



extern ontimer_struct *ontimer;
extern onkey_struct *onkey;
extern int32 onkey_inprogress;
extern onstrig_struct *onstrig;
extern int32 onstrig_inprogress;

extern uint32 qbevent;

#ifdef DEPENDENCY_DEVICEINPUT
#include "parts/input/game_controller/src.c"
#endif

extern int32 console;
extern int32 screen_hide_startup;
//...

int64 exit_code=0;

int32 console_active=1;
int32 console_child=0;//set if console is only being used by this program
int32 console_image=-1;
int32 screen_hide=0;

//format:[deadkey's symbol in UTF16],[ASCII code of alphabet letter],[resulting UTF16 character]...0
static uint16 deadchar_lookup[] = {
  96,97,224,96,65,192,180,97,225,180,65,193,94,97,226,94,65,194,126,97,227,126,65,195,168,97,228,168,65,196,730,97,229,730,65,197,180,99,263,94,99,265,96,101,232,96,69,200,180,101,233,180,69,201,94,101,234,94,69,202,126,101,7869,168,101,235,168,69,203,180,103,501,94,103,285,94,104,293,168,104,7719,96,105,236,96,73,204,180,105,237,180,73,205,94,105,238,94,73,206,126,105,297,168,105,239,168,73,207,94,106,309,180,107,7729,180,108,314,180,109,7743,96,110,505,180,110,324,126,110,241,126,78,209,96,111,242,96,79,210,180,111,243,180,79,211,94,111,244,94,79,212,126,111,245,126,79,213,168,111,246,168,79,214,180,112,7765,180,114,341,180,115,347,94,115,349,168,116,7831,96,117,249,96,85,217,180,117,250,180,85,218,94,117,251,94,85,219,126,117,361,168,117,252,168,85,220,730,117,367,126,118,7805,96,119,7809,180,119,7811,94,119,373,168,119,7813,730,119,7832,168,120,7821,96,121,7923,180,121,253,180,89,221,94,121,375,126,121,7929,168,121,255,168,89,376,730,121,7833,180,122,378,94,122,7825
  ,0
};

int32 keydown_glyph=0;

int32 convert_unicode(int32 src_fmt,void *src_buf,int32 src_size,int32 dest_fmt,void *dest_buf){
  /*
    important: to ensure enough space is available for the conversion, dest_buf must be at least src_size*4+4 in length
    returns: the number of bytes written to dest_buf
    fmt values:
    1=ASCII(CP437)
    8=UTF8
    16=UTF16
    32=UTF32
  */

  static int32 dest_size;
  dest_size=0;

  //setup source
  uint8 *src_uint8p=NULL;
  if (src_fmt==1){
    src_uint8p=(uint8*)src_buf;
  }
  uint16 *src_uint16p=NULL;
  if (src_fmt==16){
    src_uint16p=(uint16*)src_buf;
    src_size=src_size-(src_size&1);//cull trailing bytes
  }
  uint32 *src_uint32p=NULL;
  if (src_fmt==32){
    src_uint32p=(uint32*)src_buf;
    src_size=src_size-(src_size&3);//cull trailing bytes
  }

  //setup dest
  uint16 *dest_uint16p=NULL;
  if (dest_fmt==16){
    dest_uint16p=(uint16*)dest_buf;
  }
  uint32 *dest_uint32p=NULL;
  if (dest_fmt==32){
    dest_uint32p=(uint32*)dest_buf;
  }

  uint32 x;//scalar

  while(src_size){

    //convert src to scalar UNICODE value 'x'

    if (src_fmt==1){//CP437
      x=*src_uint8p++;
      src_size--;
      x=codepage437_to_unicode16[x];
    }
    if (src_fmt==16){//UTF16
      src_size-=2;
      x=*src_uint16p++;
      //note: does not handle surrogate pairs yet
    }
    if (src_fmt==32){//UTF32
      src_size-=4;
      x=*src_uint32p++;
    }

    //convert scalar UNICODE value 'x' to dest

    if (dest_fmt==16){//UTF16
      *dest_uint16p++=x;
      dest_size+=2;
      //note: does not handle surrogate pairs yet
    }
    if (dest_fmt==32){//UTF32
      *dest_uint32p++=x;
      dest_size+=4;
    }

  }//loop

  //add NULL terminator (does not change the size in bytes returned)
  if (dest_fmt==16) *dest_uint16p=0;
  if (dest_fmt==32) *dest_uint32p=0;

  return dest_size;
}





#ifdef QB64_WINDOWS
void showvalue(__int64);
#endif
void sub_beep();











int32 func__loadfont(qbs *filename,int32 size,qbs *requirements,int32 passed);



int32 lastfont=48;
int32 *font=(int32*)calloc(4*(48+1),1);//NULL=unused index
int32 *fontheight=(int32*)calloc(4*(48+1),1);
int32 *fontwidth=(int32*)calloc(4*(48+1),1);
int32 *fontflags=(int32*)calloc(4*(48+1),1);

//keyhit cyclic buffer
int64 keyhit[8192];
//    keyhit specific internal flags: (stored in high 32-bits)
//    &4294967296->numpad was used
int32 keyhit_nextfree=0;
int32 keyhit_next=0;
//note: if full, the oldest message is discarded to make way for the new message




void update_shift_state();




uint32 bindkey=0;

void scancodedown(uint8 scancode);
void scancodeup(uint8 scancode);







/*
  QB64 Mapping of audio control keyboard keys:
  MEDIA_PLAY_PAUSE 0x2200
  MEDIA_STOP 0x2400
  MEDIA_NEXT_TRACK 0x1900
  MEDIA_PREV_TRACK 0x1000
*/

/*
  compatibility upgrade:
  0-255
  num & caps versions are totally ignored
  shift is USUALLY the same as ascii but not always
  0 anywhere else means a key cannot be entered
*/


static const int32 scancode_lookup[]={
  //DESCRIPTION OFFSET  SCANCODE      ASCII      SHIFT       CTRL        ALT        NUM       CAPS SHIFT+CAPS  SHIFT+NUM
  /* ?       */    0 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */    1 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */    2 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */    3 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */    4 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */    5 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */    6 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */    7 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* Bksp    */    8 ,      0x0E,      0x08,      0x08,      0x7F,    0x0E00,         0,         0,         0,         0,
  /* Tab     */    9 ,      0x0F,      0x09,    0x0F00,    0x9400,         0,         0,         0,         0,         0,
  /* ?       */   10 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */   11 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */   12 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* enter   */   13 ,      0x1C,      0x0D,      0x0D,      0x0A,         0,         0,         0,         0,         0,
  /* ?       */   14 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */   15 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */   16 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */   17 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */   18 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */   19 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */   20 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */   21 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */   22 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */   23 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */   24 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */   25 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */   26 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* Esc     */   27 ,      0x01,      0x1B,      0x1B,      0x1B,         0,         0,         0,         0,         0,
  /* ?       */   28 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */   29 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */   30 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */   31 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* space   */   32 ,      0x39,      0x20,      0x20,      0x20,         0,         0,         0,         0,         0,
  /* 1 !     */   33 ,      0x02,      0x21,      0x21,         0,    0x7800,         0,         0,         0,         0,
  /* \91 \93     */   34 ,      0x28,      0x22,      0x22,         0,    0x2800,         0,         0,         0,         0,
  /* 3 #     */   35 ,      0x04,      0x23,      0x23,         0,    0x7A00,         0,         0,         0,         0,
  /* 4 $     */   36 ,      0x05,      0x24,      0x24,         0,    0x7B00,         0,         0,         0,         0,
  /* 5 %     */   37 ,      0x06,      0x25,      0x25,         0,    0x7C00,         0,         0,         0,         0,
  /* 7 &     */   38 ,      0x08,      0x26,      0x26,         0,    0x7E00,         0,         0,         0,         0,
  /* \91 \93     */   39 ,      0x28,      0x27,      0x27,         0,    0x2800,         0,         0,         0,         0,
  /* 9 (     */   40 ,      0x0A,      0x28,      0x28,         0,    0x8000,         0,         0,         0,         0,
  /* 0 )     */   41 ,      0x0B,      0x29,      0x29,         0,    0x8100,         0,         0,         0,         0,
  /* 8 *     */   42 ,      0x09,      0x2A,      0x2A,         0,    0x7F00,         0,         0,         0,         0,
  /* = +     */   43 ,      0x0D,      0x2B,      0x2B,         0,    0x8300,         0,         0,         0,         0,
  /* , <     */   44 ,      0x33,      0x2C,      0x2C,         0,    0x3300,         0,         0,         0,         0,
  /* - _     */   45 ,      0x0C,      0x2D,      0x2D,      0x1F,    0x8200,         0,         0,         0,         0,
  /* . >     */   46 ,      0x34,      0x2E,      0x2E,         0,    0x3400,         0,         0,         0,         0,
  /* / ?     */   47 ,      0x35,      0x2F,      0x2F,         0,    0x3500,         0,         0,         0,         0,
  /* 0 )     */   48 ,      0x0B,      0x30,      0x30,         0,    0x8100,         0,         0,         0,         0,
  /* 1 !     */   49 ,      0x02,      0x31,      0x31,         0,    0x7800,         0,         0,         0,         0,
  /* 2 @     */   50 ,      0x03,      0x32,      0x32,    0x0300,    0x7900,         0,         0,         0,         0,
  /* 3 #     */   51 ,      0x04,      0x33,      0x33,         0,    0x7A00,         0,         0,         0,         0,
  /* 4 $     */   52 ,      0x05,      0x34,      0x34,         0,    0x7B00,         0,         0,         0,         0,
  /* 5 %     */   53 ,      0x06,      0x35,      0x35,         0,    0x7C00,         0,         0,         0,         0,
  /* 6 ^     */   54 ,      0x07,      0x36,      0x36,      0x1E,    0x7D00,         0,         0,         0,         0,
  /* 7 &     */   55 ,      0x08,      0x37,      0x37,         0,    0x7E00,         0,         0,         0,         0,
  /* 8 *     */   56 ,      0x09,      0x38,      0x38,         0,    0x7F00,         0,         0,         0,         0,
  /* 9 (     */   57 ,      0x0A,      0x39,      0x39,         0,    0x8000,         0,         0,         0,         0,
  /* ; :     */   58 ,      0x27,      0x3A,      0x3A,         0,    0x2700,         0,         0,         0,         0,
  /* ; :     */   59 ,      0x27,      0x3B,      0x3B,         0,    0x2700,         0,         0,         0,         0,
  /* , <     */   60 ,      0x33,      0x3C,      0x3C,         0,    0x3300,         0,         0,         0,         0,
  /* = +     */   61 ,      0x0D,      0x3D,      0x3D,         0,    0x8300,         0,         0,         0,         0,
  /* . >     */   62 ,      0x34,      0x3E,      0x3E,         0,    0x3400,         0,         0,         0,         0,
  /* / ?     */   63 ,      0x35,      0x3F,      0x3F,         0,    0x3500,         0,         0,         0,         0,
  /* 2 @     */   64 ,      0x03,      0x40,      0x40,    0x0300,    0x7900,         0,         0,         0,         0,
  /* A       */   97 ,      0x1E,      0x41,      0x41,      0x01,    0x1E00,         0,         0,         0,         0,
  /* B       */   98 ,      0x30,      0x42,      0x42,      0x02,    0x3000,         0,         0,         0,         0,
  /* C       */   99 ,      0x2E,      0x43,      0x43,      0x03,    0x2E00,         0,         0,         0,         0,
  /* D       */  100 ,      0x20,      0x44,      0x44,      0x04,    0x2000,         0,         0,         0,         0,
  /* E       */  101 ,      0x12,      0x45,      0x45,      0x05,    0x1200,         0,         0,         0,         0,
  /* F       */  102 ,      0x21,      0x46,      0x46,      0x06,    0x2100,         0,         0,         0,         0,
  /* G       */  103 ,      0x22,      0x47,      0x47,      0x07,    0x2200,         0,         0,         0,         0,
  /* H       */  104 ,      0x23,      0x48,      0x48,      0x08,    0x2300,         0,         0,         0,         0,
  /* I       */  105 ,      0x17,      0x49,      0x49,      0x09,    0x1700,         0,         0,         0,         0,
  /* J       */  106 ,      0x24,      0x4A,      0x4A,      0x0A,    0x2400,         0,         0,         0,         0,
  /* K       */  107 ,      0x25,      0x4B,      0x4B,      0x0B,    0x2500,         0,         0,         0,         0,
  /* L       */  108 ,      0x26,      0x4C,      0x4C,      0x0C,    0x2600,         0,         0,         0,         0,
  /* M       */  109 ,      0x32,      0x4D,      0x4D,      0x0D,    0x3200,         0,         0,         0,         0,
  /* N       */  110 ,      0x31,      0x4E,      0x4E,      0x0E,    0x3100,         0,         0,         0,         0,
  /* O       */  111 ,      0x18,      0x4F,      0x4F,      0x0F,    0x1800,         0,         0,         0,         0,
  /* P       */  112 ,      0x19,      0x50,      0x50,      0x10,    0x1900,         0,         0,         0,         0,
  /* Q       */  113 ,      0x10,      0x51,      0x51,      0x11,    0x1000,         0,         0,         0,         0,
  /* R       */  114 ,      0x13,      0x52,      0x52,      0x12,    0x1300,         0,         0,         0,         0,
  /* S       */  115 ,      0x1F,      0x53,      0x53,      0x13,    0x1F00,         0,         0,         0,         0,
  /* T       */  116 ,      0x14,      0x54,      0x54,      0x14,    0x1400,         0,         0,         0,         0,
  /* U       */  117 ,      0x16,      0x55,      0x55,      0x15,    0x1600,         0,         0,         0,         0,
  /* V       */  118 ,      0x2F,      0x56,      0x56,      0x16,    0x2F00,         0,         0,         0,         0,
  /* W       */  119 ,      0x11,      0x57,      0x57,      0x17,    0x1100,         0,         0,         0,         0,
  /* X       */  120 ,      0x2D,      0x58,      0x58,      0x18,    0x2D00,         0,         0,         0,         0,
  /* Y       */  121 ,      0x15,      0x59,      0x59,      0x19,    0x1500,         0,         0,         0,         0,
  /* Z       */  122 ,      0x2C,      0x5A,      0x5A,      0x1A,    0x2C00,         0,         0,         0,         0,
  /* [ {     */   91 ,      0x1A,      0x5B,      0x5B,      0x1B,    0x1A00,         0,         0,         0,         0,
  /* \ |     */   92 ,      0x2B,      0x5C,      0x5C,      0x1C,    0x2B00,         0,         0,         0,         0,
  /* ] }     */   93 ,      0x1B,      0x5D,      0x5D,      0x1D,    0x1B00,         0,         0,         0,         0,
  /* 6 ^     */   94 ,      0x07,      0x5E,      0x5E,      0x1E,    0x7D00,         0,         0,         0,         0,
  /* - _     */   95 ,      0x0C,      0x5F,      0x5F,      0x1F,    0x8200,         0,         0,         0,         0,
  /* ` ~     */   96 ,      0x29,      0x60,      0x60,         0,    0x2900,         0,         0,         0,         0,
  /* A       */   97 ,      0x1E,      0x61,      0x61,      0x01,    0x1E00,         0,         0,         0,         0,
  /* B       */   98 ,      0x30,      0x62,      0x62,      0x02,    0x3000,         0,         0,         0,         0,
  /* C       */   99 ,      0x2E,      0x63,      0x63,      0x03,    0x2E00,         0,         0,         0,         0,
  /* D       */  100 ,      0x20,      0x64,      0x64,      0x04,    0x2000,         0,         0,         0,         0,
  /* E       */  101 ,      0x12,      0x65,      0x65,      0x05,    0x1200,         0,         0,         0,         0,
  /* F       */  102 ,      0x21,      0x66,      0x66,      0x06,    0x2100,         0,         0,         0,         0,
  /* G       */  103 ,      0x22,      0x67,      0x67,      0x07,    0x2200,         0,         0,         0,         0,
  /* H       */  104 ,      0x23,      0x68,      0x68,      0x08,    0x2300,         0,         0,         0,         0,
  /* I       */  105 ,      0x17,      0x69,      0x69,      0x09,    0x1700,         0,         0,         0,         0,
  /* J       */  106 ,      0x24,      0x6A,      0x6A,      0x0A,    0x2400,         0,         0,         0,         0,
  /* K       */  107 ,      0x25,      0x6B,      0x6B,      0x0B,    0x2500,         0,         0,         0,         0,
  /* L       */  108 ,      0x26,      0x6C,      0x6C,      0x0C,    0x2600,         0,         0,         0,         0,
  /* M       */  109 ,      0x32,      0x6D,      0x6D,      0x0D,    0x3200,         0,         0,         0,         0,
  /* N       */  110 ,      0x31,      0x6E,      0x6E,      0x0E,    0x3100,         0,         0,         0,         0,
  /* O       */  111 ,      0x18,      0x6F,      0x6F,      0x0F,    0x1800,         0,         0,         0,         0,
  /* P       */  112 ,      0x19,      0x70,      0x70,      0x10,    0x1900,         0,         0,         0,         0,
  /* Q       */  113 ,      0x10,      0x71,      0x71,      0x11,    0x1000,         0,         0,         0,         0,
  /* R       */  114 ,      0x13,      0x72,      0x72,      0x12,    0x1300,         0,         0,         0,         0,
  /* S       */  115 ,      0x1F,      0x73,      0x73,      0x13,    0x1F00,         0,         0,         0,         0,
  /* T       */  116 ,      0x14,      0x74,      0x74,      0x14,    0x1400,         0,         0,         0,         0,
  /* U       */  117 ,      0x16,      0x75,      0x75,      0x15,    0x1600,         0,         0,         0,         0,
  /* V       */  118 ,      0x2F,      0x76,      0x76,      0x16,    0x2F00,         0,         0,         0,         0,
  /* W       */  119 ,      0x11,      0x77,      0x77,      0x17,    0x1100,         0,         0,         0,         0,
  /* X       */  120 ,      0x2D,      0x78,      0x78,      0x18,    0x2D00,         0,         0,         0,         0,
  /* Y       */  121 ,      0x15,      0x79,      0x79,      0x19,    0x1500,         0,         0,         0,         0,
  /* Z       */  122 ,      0x2C,      0x7A,      0x7A,      0x1A,    0x2C00,         0,         0,         0,         0,
  /* [ {     */  123 ,      0x1A,      0x7B,      0x7B,      0x1B,    0x1A00,         0,         0,         0,         0,
  /* \ |     */  124 ,      0x2B,      0x7C,      0x7C,      0x1C,    0x2B00,         0,         0,         0,         0,
  /* ] }     */  125 ,      0x1B,      0x7D,      0x7D,      0x1D,    0x1B00,         0,         0,         0,         0,
  /* ` ~     */  126 ,      0x29,      0x7E,      0x7E,         0,    0x2900,         0,         0,         0,         0,
  /* ?       */  127 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  128 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  129 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  130 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  131 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  132 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  133 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  134 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  135 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  136 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  137 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  138 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  139 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  140 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  141 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  142 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  143 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  144 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  145 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  146 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  147 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  148 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  149 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  150 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  151 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  152 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  153 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  154 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  155 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  156 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  157 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  158 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  159 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  160 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  161 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  162 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  163 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  164 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  165 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  166 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  167 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  168 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  169 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  170 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  171 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  172 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  173 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  174 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  175 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  176 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  177 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  178 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  179 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  180 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  181 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  182 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  183 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  184 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  185 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  186 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  187 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  188 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  189 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  190 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  191 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  192 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  193 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,

  /* ?       */  194 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  195 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  196 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  197 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  198 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  199 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  200 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  201 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  202 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  203 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  204 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  205 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  206 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  207 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  208 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  209 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  210 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  211 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  212 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  213 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  214 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  215 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  216 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  217 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  218 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  219 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  220 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  221 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  222 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  223 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  224 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  225 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  226 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  227 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  228 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  229 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  230 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  231 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  232 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  233 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  234 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  235 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  236 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  237 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  238 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  239 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  240 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  241 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  242 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  243 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  244 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  245 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  246 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  247 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  248 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  249 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  250 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  251 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  252 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  253 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  254 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  255 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */    0 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */    1 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */    2 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */    3 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */    4 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */    5 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */    6 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */    7 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */    8 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */    9 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */   10 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */   11 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */   12 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */   13 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */   14 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */   15 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */   16 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */   17 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */   18 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */   19 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */   20 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */   21 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */   22 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */   23 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */   24 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */   25 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */   26 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */   27 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */   28 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */   29 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */   30 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */   31 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */   32 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */   33 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */   34 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */   35 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */   36 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */   37 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */   38 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */   39 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */   40 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */   41 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */   42 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */   43 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */   44 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */   45 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */   46 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */   47 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */   48 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */   49 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */   50 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */   51 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */   52 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */   53 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */   54 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */   55 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */   56 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */   57 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */   58 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* F1      */   59 ,      0x3B,    0x3B00,    0x5400,    0x5E00,    0x6800,         0,         0,         0,         0,
  /* F2      */   60 ,      0x3C,    0x3C00,    0x5500,    0x5F00,    0x6900,         0,         0,         0,         0,
  /* F3      */   61 ,      0x3D,    0x3D00,    0x5600,    0x6000,    0x6A00,         0,         0,         0,         0,
  /* F4      */   62 ,      0x3E,    0x3E00,    0x5700,    0x6100,    0x6B00,         0,         0,         0,         0,
  /* F5      */   63 ,      0x3F,    0x3F00,    0x5800,    0x6200,    0x6C00,         0,         0,         0,         0,
  /* F6      */   64 ,      0x40,    0x4000,    0x5900,    0x6300,    0x6D00,         0,         0,         0,         0,
  /* F7      */   65 ,      0x41,    0x4100,    0x5A00,    0x6400,    0x6E00,         0,         0,         0,         0,
  /* F8      */   66 ,      0x42,    0x4200,    0x5B00,    0x6500,    0x6F00,         0,         0,         0,         0,
  /* F9      */   67 ,      0x43,    0x4300,    0x5C00,    0x6600,    0x7000,         0,         0,         0,         0,
  /* F10     */   68 ,      0x44,    0x4400,    0x5D00,    0x6700,    0x7100,         0,         0,         0,         0,
  /* ?       */   69 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */   70 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* home *X */   71 ,      0x47,    0x4700,    0x4700,    0x7700,    0x9700,         0,         0,         0,         0, //note: X=not on NUMPAD
  /* up *X   */   72 ,      0x48,    0x4800,    0x4800,    0x8D00,    0x9800,         0,         0,         0,         0,
  /* pgup *X */   73 ,      0x49,    0x4900,    0x4900,    0x8400,    0x9900,         0,         0,         0,         0,
  /* ?       */   74 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* left *X */   75 ,      0x4B,    0x4B00,    0x4B00,    0x7300,    0x9B00,         0,         0,         0,         0,
  /* (center)*/   76 ,         0,         0,         0,         0,         0,         0,         0,         0,         0, //note: not used
  /* right *X*/   77 ,      0x4D,    0x4D00,    0x4D00,    0x7400,    0x9D00,         0,         0,         0,         0,
  /* ?       */   78 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* end  *X */   79 ,      0x4F,    0x4F00,    0x4F00,    0x7500,    0x9F00,         0,         0,         0,         0,
  /* down *X */   80 ,      0x50,    0x5000,    0x5000,    0x9100,    0xA000,         0,         0,         0,         0,
  /* pgdn *X */   81 ,      0x51,    0x5100,    0x5100,    0x7600,    0xA100,         0,         0,         0,         0,
  /* ins *X  */   82 ,      0x52,    0x5200,    0x5200,    0x9200,    0xA200,         0,         0,         0,         0,
  /* del *X  */   83 ,      0x53,    0x5300,    0x5300,    0x9300,    0xA300,         0,         0,         0,         0,
  /* ?       */   84 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */   85 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */   86 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */   87 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */   88 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */   89 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */   90 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */   91 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */   92 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */   93 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */   94 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */   95 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */   96 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */   97 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */   98 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */   99 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  100 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  101 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  102 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  103 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  104 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  105 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  106 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  107 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  108 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  109 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  110 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  111 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  112 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  113 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  114 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  115 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  116 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  117 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  118 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  119 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  120 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  121 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  122 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  123 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  124 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  125 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  126 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  127 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  128 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  129 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  130 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  131 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  132 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* F11     */  133 ,      0x57,    0x8500,    0x8500,    0x8900,    0x8B00,         0,         0,         0,         0,
  /* F12     */  134 ,      0x58,    0x8600,    0x8600,    0x8A00,    0x8C00,         0,         0,         0,         0,
  /* ?       */  135 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  136 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  137 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  138 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  139 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  140 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  141 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  142 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  143 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  144 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  145 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  146 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  147 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  148 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  149 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  150 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  151 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  152 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  153 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  154 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  155 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  156 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  157 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  158 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  159 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  160 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  161 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  162 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  163 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  164 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  165 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  166 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  167 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  168 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  169 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  170 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  171 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  172 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  173 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  174 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  175 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  176 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  177 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  178 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  179 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  180 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  181 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  182 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  183 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  184 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  185 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  186 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  187 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  188 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  189 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  190 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  191 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  192 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  193 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  194 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  195 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  196 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  197 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  198 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  199 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  200 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  201 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  202 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  203 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  204 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  205 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  206 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,


  /* ?       */  207 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  208 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  209 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  210 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  211 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  212 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  213 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  214 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  215 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  216 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  217 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  218 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  219 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  220 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  221 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  222 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  223 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  224 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  225 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  226 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  227 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  228 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  229 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  230 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  231 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  232 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  233 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  234 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  235 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  236 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  237 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  238 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  239 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  240 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  241 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  242 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  243 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  244 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  245 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  246 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  247 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  248 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  249 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  250 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  251 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  252 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  253 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  254 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  /* ?       */  255 ,         0,         0,         0,         0,         0,         0,         0,         0,         0,
  //NUMPAD keys in VK order
  /* ins     */  512 ,      0x52,    0x5200,      0x30,    0x9200,         0,      0x30,         0,         0,    0x5200,
  /* end     */  513 ,      0x4F,    0x4F00,      0x31,    0x7500,         0,      0x31,         0,         0,    0x4F00,
  /* down    */  514 ,      0x50,    0x5000,      0x32,    0x9100,         0,      0x32,         0,         0,    0x5000,
  /* pgdn    */  515 ,      0x51,    0x5100,      0x33,    0x7600,         0,      0x33,         0,         0,    0x5100,
  /* left    */  516 ,      0x4B,    0x4B00,      0x34,    0x7300,         0,      0x34,         0,         0,    0x4B00,
  /* center  */  517 ,      0x4C,    0x4C00,      0x35,    0x8F00,         0,      0x35,         0,         0,    0x4C00,
  /* right   */  518 ,      0x4D,    0x4D00,      0x36,    0x7400,         0,      0x36,         0,         0,    0x4D00,
  /* home    */  519 ,      0x47,    0x4700,      0x37,    0x7700,         0,      0x37,         0,         0,    0x4700,
  /* up      */  520 ,      0x48,    0x4800,      0x38,    0x8D00,         0,      0x38,         0,         0,    0x4800,
  /* pgup    */  521 ,      0x49,    0x4900,      0x39,    0x8400,         0,      0x39,         0,         0,    0x4900,
  /* del     */  522 ,      0x53,    0x5300,      0x2E,    0x9300,         0,      0x2E,         0,         0,    0x5300,
  /* divide  */  523 ,      0x35,      0x2F,      0x2F,    0x9500,    0xA400,      0x2F,         0,         0,      0x2F,
  /* mult    */  524 ,      0x37,      0x2A,      0x2A,    0x9600,    0x3700,      0x2A,         0,         0,      0x2A,
  /* -d      */  525 ,      0x4A,      0x2D,      0x2D,    0x8E00,    0x4A00,      0x2D,         0,         0,      0x2D,
  /* +e      */  526 ,      0x4E,      0x2B,      0x2B,    0x9000,    0x4E00,      0x2B,         0,         0,      0x2B,
  /* enter   */  527 ,      0x1C,      0x0D,      0x0D,      0x0A,    0xA600,      0x0D,         0,         0,      0x0D,

  /* ctrl    */  000 ,         0,         0,         0,         0,         0,         0,         0,         0,      0x1D,
  /* Lshift  */  000 ,      0x2A,         0,         0,         0,         0,         0,         0,         0,         0,
  /* Rshift  */  000 ,      0x36,         0,         0,         0,         0,         0,         0,         0,         0,
  /* alt     */  000 ,      0x38,         0,         0,         0,         0,         0,         0,         0,         0,
  /* caps    */  000 ,      0x3A,         0,         0,         0,         0,         0,         0,         0,         0,
  /* num     */  000 ,      0x45,         0,         0,         0,         0,         0,         0,         0,         0,
  /* scrl    */  000 ,      0x46,         0,         0,         0,         0,         0,         0,         0,         0,
  /* * PrtSc */  000 ,      0x37,      0x2A,         0,      0x10,         0,      0x2A,      0x2A,         0,         0,
};


void keydown(uint32 x);
void keyup(uint32 x);

uint32 unicode_to_cp437(uint32 x){
  static int32 i;
  for (i=0;i<=255;i++){
    if (x==codepage437_to_unicode16[i]) return i;
  }
  return 0;
}

uint32 *keyheld_buffer=(uint32*)malloc(1);
uint32 *keyheld_bind_buffer=(uint32*)malloc(1);
int32 keyheld_n=0;
int32 keyheld_size=0;

int32 keyheld(uint32 x){
  static int32 i;
  for (i=0;i<keyheld_n;i++){
    if (keyheld_buffer[i]==x) return 1;
  }
  //check multimapped NUMPAD keys
  if ((x>=42)&&(x<=57)){
    if ((x>=48)&&(x<=57)) return keyheld(VK+QBVK_KP0+(x-48));//0-9
    if (x==46) return keyheld(VK+QBVK_KP_PERIOD);
    if (x==47) return keyheld(VK+QBVK_KP_DIVIDE);
    if (x==42) return keyheld(VK+QBVK_KP_MULTIPLY);
    if (x==45) return keyheld(VK+QBVK_KP_MINUS);
    if (x==43) return keyheld(VK+QBVK_KP_PLUS);
  }
  if (x==13) return keyheld(VK+QBVK_KP_ENTER);
  if (x&0xFF00){
    static uint32 x2;
    x2=(x>>8)&255;
    if ((x2>=71)&&(x2<=83)){
      if (x2==82) return keyheld(QBK+QBVK_KP0-QBVK_KP0);
      if (x2==79) return keyheld(QBK+QBVK_KP1-QBVK_KP0);
      if (x2==80) return keyheld(QBK+QBVK_KP2-QBVK_KP0);
      if (x2==81) return keyheld(QBK+QBVK_KP3-QBVK_KP0);
      if (x2==75) return keyheld(QBK+QBVK_KP4-QBVK_KP0);
      if (x2==76) return keyheld(QBK+QBVK_KP5-QBVK_KP0);
      if (x2==77) return keyheld(QBK+QBVK_KP6-QBVK_KP0);
      if (x2==71) return keyheld(QBK+QBVK_KP7-QBVK_KP0);
      if (x2==72) return keyheld(QBK+QBVK_KP8-QBVK_KP0);
      if (x2==73) return keyheld(QBK+QBVK_KP9-QBVK_KP0);
      if (x2==83) return keyheld(QBK+QBVK_KP_PERIOD-QBVK_KP0);
    }
  }
  return 0;
}


void keyheld_add(uint32 x){
  static int32 i; for (i=0;i<keyheld_n;i++){if (keyheld_buffer[i]==x) return;}//already in buffer
  if (keyheld_n==keyheld_size){keyheld_size++; keyheld_buffer=(uint32*)realloc(keyheld_buffer,keyheld_size*4); keyheld_bind_buffer=(uint32*)realloc(keyheld_bind_buffer,keyheld_size*4);}//expand buffer
  keyheld_buffer[keyheld_n]=x;//add entry
  keyheld_bind_buffer[keyheld_n]=bindkey; bindkey=0;//add binded key (0=none)
  keyheld_n++;//note: inc. must occur after setting entry (threading reasons)
}
void keyheld_remove(uint32 x){
  static int32 i;
  for (i=0;i<keyheld_n;i++){
    if (keyheld_buffer[i]==x){//exists
      memmove(&keyheld_buffer[i],&keyheld_buffer[i+1],(keyheld_n-i-1)*4);
      memmove(&keyheld_bind_buffer[i],&keyheld_bind_buffer[i+1],(keyheld_n-i-1)*4);
      keyheld_n--;//note: dec. must occur after memmove (threading reasons)
      return;
    }
  }
}
void keyheld_unbind(uint32 x){
  static int32 i;
  for (i=0;i<keyheld_n;i++){
    if (keyheld_bind_buffer[i]==x){//exists
      keyup(keyheld_buffer[i]);
      return;
    }
  }
}


void keydown_ascii(uint32 x){
  keydown(x);
}
void keydown_unicode(uint32 x){
  keydown_glyph=1;
  //note: UNICODE 0-127 map directly to ASCII 0-127
  if (x<=127){keydown_ascii(x); return;}
  //note: some UNICODE values map directly to CP437 values found in the extended ASCII set
  static uint32 x2; if (x2=unicode_to_cp437(x)){keydown_ascii(x2); return;}
  //note: full width latin characters will be mapped to their normal width equivalents
  //Wikipedia note: Range U+FF01\96FF5E reproduces the characters of ASCII 21 to 7E as fullwidth forms, that is, a fixed width form used in CJK computing. This is useful for typesetting Latin characters in a CJK  environment. U+FF00 does not correspond to a fullwith ASCII 20 (space character), since that role is already fulfilled by U+3000 "ideographic space."
  if ((x>=0x0000FF01)&&(x<=0x0000FF5E)){keydown_ascii(x-0x0000FF01+0x21); return;}
  if (x==0x3000){keydown_ascii(32); return;}
  x|=UC;
  keydown(x);
}
void keydown_vk(uint32 x){
  keydown(x);
}
void keyup_ascii(uint32 x){
  keyup(x);
}
void keyup_unicode(uint32 x){
  //note: UNICODE 0-127 map directly to ASCII 0-127
  if (x<=127){keyup_ascii(x); return;}
  //note: some UNICODE values map directly to CP437 values found in the extended ASCII set
  static uint32 x2; if (x2=unicode_to_cp437(x)){keyup_ascii(x2); return;}
  //note: full width latin characters will be mapped to their normal width equivalents
  //Wikipedia note: Range U+FF01\96FF5E reproduces the characters of ASCII 21 to 7E as fullwidth forms, that is, a fixed width form used in CJK computing. This is useful for typesetting Latin characters in a CJK  environment. U+FF00 does not correspond to a fullwith ASCII 20 (space character), since that role is already fulfilled by U+3000 "ideographic space."
  if ((x>=0x0000FF01)&&(x<=0x0000FF5E)){keyup_ascii(x-0x0000FF01+0x21); return;}
  if (x==0x3000){keyup_ascii(32); return;}
  x|=UC;
  keyup(x);
}
void keyup_vk(uint32 x){
  keyup(x);
}


int32 exit_ok=0;

//substitute Windows functionality
#ifndef QB64_WINDOWS
//MessageBox defines
#define IDOK                1
#define IDCANCEL            2
#define IDABORT             3
#define IDRETRY             4
#define IDIGNORE            5
#define IDYES               6
#define IDNO                7
#define MB_OK                       0x00000000L
#define MB_OKCANCEL                 0x00000001L
#define MB_ABORTRETRYIGNORE         0x00000002L
#define MB_YESNOCANCEL              0x00000003L
#define MB_YESNO                    0x00000004L
#define MB_RETRYCANCEL              0x00000005L
#define MB_SYSTEMMODAL              0x00001000L
//alternate implementations of MessageBox
#ifdef QB64_MACOSX
int MessageBox(int ignore,char* message, char* header, int type )
{


  CFStringRef header_ref      = CFStringCreateWithCString( NULL, header,     kCFStringEncodingASCII );
  CFStringRef message_ref  = CFStringCreateWithCString( NULL, message, kCFStringEncodingASCII );
  CFOptionFlags result;
  if (type&MB_SYSTEMMODAL) type-=MB_SYSTEMMODAL;
  if (type==MB_YESNO){ 
    CFUserNotificationDisplayAlert(
                   0, // no timeout
                   kCFUserNotificationNoteAlertLevel,
                   NULL,
                   NULL, 
                   NULL,
                   header_ref,
                   message_ref,
                   CFSTR("No"),
                   CFSTR("Yes"),
                   NULL,
                   &result
                   );
    CFRelease( header_ref );
    CFRelease( message_ref );
    if( result == kCFUserNotificationDefaultResponse )
      return IDNO;
    else
      return IDYES;
  }
  if (type==MB_OK){ 
    CFUserNotificationDisplayAlert(
                   0, // no timeout
                   kCFUserNotificationNoteAlertLevel,
                   NULL,
                   NULL, 
                   NULL,
                   header_ref,
                   message_ref,
                   CFSTR("OK"),
                   NULL,
                   NULL,
                   &result
                   );
    CFRelease( header_ref );
    CFRelease( message_ref );
    return IDOK;
  }
  return IDCANCEL;
}
#else
int MessageBox(int ignore,char* message,char* title,int type){
  static qbs *s=NULL; if (!s) s=qbs_new(0,0);
  if (type&MB_SYSTEMMODAL) type-=MB_SYSTEMMODAL;
  if (type==MB_YESNO){
    qbs_set(s,qbs_new_txt("xmessage -center -title "));
    qbs_set(s,qbs_add(s,qbs_new_txt("?"))); s->chr[s->len-1]=34;
    qbs_set(s,qbs_add(s,qbs_new_txt(title)));
    qbs_set(s,qbs_add(s,qbs_new_txt("?"))); s->chr[s->len-1]=34;
    qbs_set(s,qbs_add(s,qbs_new_txt(" -buttons Yes:2,No:1 ")));
    qbs_set(s,qbs_add(s,qbs_new_txt("?"))); s->chr[s->len-1]=34;
    qbs_set(s,qbs_add(s,qbs_new_txt(message)));
    qbs_set(s,qbs_add(s,qbs_new_txt("                         ")));
    qbs_set(s,qbs_add(s,qbs_new_txt("?"))); s->chr[s->len-1]=34;
    qbs_set(s,qbs_add(s,qbs_new_txt("?"))); s->chr[s->len-1]=0;
    static int status;
    status=system((char*)s->chr);
    if (-1!=status){
      static int32 r;
      r=WEXITSTATUS(status);
      if (r==2) return IDYES;
      if (r==1) return IDNO;
    }
    return IDNO;
  }//MB_YESNO
  if (type==MB_OK){
    qbs_set(s,qbs_new_txt("xmessage -center -title "));
    qbs_set(s,qbs_add(s,qbs_new_txt("?"))); s->chr[s->len-1]=34;
    qbs_set(s,qbs_add(s,qbs_new_txt(title)));
    qbs_set(s,qbs_add(s,qbs_new_txt("?"))); s->chr[s->len-1]=34;
    qbs_set(s,qbs_add(s,qbs_new_txt(" -buttons OK:1 ")));
    qbs_set(s,qbs_add(s,qbs_new_txt("?"))); s->chr[s->len-1]=34;
    qbs_set(s,qbs_add(s,qbs_new_txt(message)));
    qbs_set(s,qbs_add(s,qbs_new_txt("                         ")));
    qbs_set(s,qbs_add(s,qbs_new_txt("?"))); s->chr[s->len-1]=34;
    qbs_set(s,qbs_add(s,qbs_new_txt("?"))); s->chr[s->len-1]=0;
    system((char*)s->chr);
    return IDOK;
  }//MB_OK
  return IDCANCEL;
}
#endif

void AllocConsole(){
  return;
}
void FreeConsole(){
  return;
}
#endif

int MessageBox2(int ignore,char* message,char* title,int type){

  if (cloud_app){
    FILE *f = fopen("..\\final.txt", "w");
    if (f != NULL)
      {
    fprintf(f, "%s", title);
    fprintf(f, "\n");
    fprintf(f, "%s", message);
    fclose(f);
      }
    exit(0);//should log error
  }

  #ifdef QB64_ANDROID
    showErrorOnScreen(message, 0, 0);//display error message on screen and enter infinite loop
  #endif

  #ifdef QB64_WINDOWS
    return MessageBox(window_handle,message,title,type);
  #else
    return MessageBox(NULL,message,title,type);
  #endif
}



void alert(int32 x){
  static char str[100];
  memset(&str[0],0,100);
  sprintf(str, "%d", x);
  MessageBox(0,&str[0], "Alert", MB_OK );
}

void alert(char *x){
  MessageBox(0,x, "Alert", MB_OK );
}





//vc->project->properties->configuration properties->general->configuration type->application(.exe)
//vc->project->properties->configuration properties->general->configuration type->static library(.lib)


extern void QBMAIN(void *);
extern void TIMERTHREAD();
void MAIN_LOOP();

#ifdef QB64_WINDOWS
extern void QBMAIN_WINDOWS(void *);
extern void TIMERTHREAD_WINDOWS(void *);
void MAIN_LOOP_WINDOWS(void *);
#else
extern void *QBMAIN_LINUX(void *);
extern void *TIMERTHREAD_LINUX(void *);
void *MAIN_LOOP_LINUX(void *);
#endif

void GLUT_MAINLOOP_THREAD(void *);
void GLUT_DISPLAY_REQUEST();


extern qbs* WHATISMYIP();

//directory access defines
#define EPERM           1
#define ENOENT          2
#define ESRCH           3
#define EINTR           4
#define EIO             5
#define ENXIO           6
#define E2BIG           7
#define ENOEXEC         8
#define EBADF           9
#define ECHILD          10
#define EAGAIN          11
#define ENOMEM          12
#define EACCES          13
#define EFAULT          14
#define EBUSY           16
#define EEXIST          17
#define EXDEV           18
#define ENODEV          19
#define ENOTDIR         20
#define EISDIR          21
#define EINVAL          22
#define ENFILE          23
#define EMFILE          24
#define ENOTTY          25
#define EFBIG           27
#define ENOSPC          28
#define ESPIPE          29
#define EROFS           30
#define EMLINK          31
#define EPIPE           32
#define EDOM            33
#define ERANGE          34
#define EDEADLK         36
#define ENAMETOOint32    38
#define ENOLCK          39
#define ENOSYS          40
#define ENOTEMPTY       41
#define EILSEQ          42


int32 lprint=0;//set to 1 during LPRINT operations
int32 lprint_image=0;
double lprint_last=0;//TIMER(0.001) value at last time LPRINT was used
int32 lprint_buffered=0;//set to 1 if content is pending to print
int32 lprint_locked=0;//set to 1 to deny access by QB64 program


static int32 file_charset8_raw_len=16384;
static const uint8 file_charset8_raw[]={
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,0,255,0,0,0,0,0,0,255,255,0,255,0,0,255,0,255,255,0,0,0,0,0,0,255,255,0,255,255,255,255,0,255,255,0,0,255,255,0,0,255,255,0,0,0,0,0,0,255,0,255,255,255,255,255,255,0,0,255,255,255,255,255,255,0,255,255,255,255,255,255,255,255,255,255,0,255,255,0,255,255,255,255,255,255,255,255,255,255,255,255,0,0,0,0,255,255,255,255,255,0,0,255,255,255,255,255,255,255,255,255,255,255,0,255,255,255,255,255,255,0,0,255,255,0,255,255,0,0,255,255,255,255,255,255,255,0,255,255,255,255,255,255,255,0,255,255,255,255,255,255,255,0,0,255,255,255,255,255,0,0,0,0,255,255,255,0,0,0,0,0,0,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,0,0,0,0,0,0,255,255,255,0,0,0,0,255,255,255,255,255,0,0,255,255,255,255,255,255,255,0,0,255,255,255,255,255,0,0,0,0,255,255,255,0,0,0,0,0,0,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,0,0,0,0,255,255,255,255,255,0,0,0,0,255,255,255,0,0,0,255,
  255,255,255,255,255,255,0,255,255,255,255,255,255,255,0,0,255,255,255,255,255,0,0,0,0,255,255,255,0,0,0,0,255,255,255,255,255,0,0,0,0,0,255,0,0,0,0,0,0,0,255,0,0,0,0,0,0,255,255,255,0,0,0,0,255,255,255,255,255,0,0,255,255,255,255,255,255,255,0,0,255,255,255,255,255,0,0,0,0,255,255,255,0,0,0,0,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,255,255,0,0,0,0,255,255,255,255,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,0,0,255,255,255,255,255,0,0,0,0,255,255,255,255,0,0,0,0,255,255,255,255,255,0,0,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,255,255,255,255,0,0,0,255,255,0,0,255,255,0,0,255,0,0,0,0,255,0,0,255,0,0,0,0,255,0,0,255,255,0,0,255,255,0,0,0,255,255,255,255,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,255,255,255,255,0,0,0,0,255,255,255,0,0,255,255,0,0,255,255,0,255,255,255,255,0,255,255,0,255,255,255,255,
  0,255,255,0,0,255,255,0,0,255,255,255,0,0,0,0,255,255,255,255,255,255,255,255,255,255,0,0,0,0,255,255,255,255,0,0,0,0,0,255,255,255,0,0,0,0,255,255,255,255,0,255,255,255,255,255,0,255,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,0,255,255,255,255,0,0,0,0,0,255,255,255,255,0,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,0,255,255,255,255,0,0,0,0,0,255,255,0,0,0,0,255,255,255,255,255,255,0,0,0,0,255,255,0,0,0,0,0,255,255,255,255,255,255,0,0,255,255,0,0,255,255,0,0,255,255,255,255,255,255,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,255,0,0,0,0,255,255,255,255,0,0,0,0,255,255,255,0,0,0,0,0,0,255,255,255,255,255,255,255,0,255,255,0,0,0,255,255,0,255,255,255,255,255,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,255,255,255,255,255,255,0,0,255,255,0,255,255,0,0,0,0,0,0,255,0,0,255,255,0,0,255,0,255,0,255,255,0,255,0,0,0,255,255,255,255,0,0,255,255,255,0,0,255,255,255,255,255,255,0,0,255,255,255,0,0,255,255,
  255,255,0,0,0,255,0,255,255,0,255,0,255,0,0,255,255,0,0,255,255,0,0,0,0,0,0,0,255,255,255,0,0,0,0,0,255,255,255,255,255,0,0,0,255,255,255,255,255,255,255,0,255,255,255,255,255,0,0,0,255,255,255,0,0,0,0,0,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,0,0,0,0,0,255,255,255,0,0,0,255,255,255,255,255,0,255,255,255,255,255,255,255,0,0,0,255,255,255,255,255,0,0,0,0,0,255,255,255,0,0,0,0,0,0,0,255,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,255,255,0,0,0,255,255,255,255,255,255,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,255,255,255,255,255,255,0,0,0,255,255,255,255,0,0,0,0,0,255,255,0,0,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,0,0,0,0,0,0,0,0,255,255,0,0,255,255,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,255,255,255,0,255,255,0,255,255,255,255,0,255,255,0,255,255,0,255,255,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,0,0,0,0,0,0,0,255,255,255,255,
  255,0,0,255,255,0,0,0,255,255,0,0,255,255,255,0,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,0,255,255,255,0,0,0,255,255,0,0,255,255,0,0,0,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,0,0,255,255,255,255,255,255,0,0,255,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,255,255,0,0,0,255,255,255,255,255,255,0,0,0,0,255,255,0,0,0,0,255,255,255,255,255,255,0,0,0,255,255,255,255,0,0,0,0,0,255,255,0,0,0,255,255,255,255,255,255,255,255,0,0,0,255,255,0,0,0,0,0,255,255,255,255,0,0,0,255,255,255,255,255,255,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,255,255,255,255,255,255,0,0,0,255,255,255,255,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,255,255,0,0,255,255,255,255,255,255,255,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,255,255,255,255,255,255,255,0,0,255,255,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,0,0,255,0,0,0,255,255,0,0,255,255,0,255,255,255,255,255,255,255,255,0,255,255,0,0,255,255,0,0,0,255,0,0,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,255,255,0,0,0,255,255,255,255,255,255,0,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,0,255,255,255,255,255,255,0,0,0,255,255,255,255,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,255,255,0,0,0,0,0,255,255,255,255,0,0,0,0,255,255,255,255,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,255,255,255,255,255,255,255,0,0,255,255,0,255,255,0,0,255,255,255,255,255,255,255,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,255,255,255,0,0,255,255,0,0,0,0,0,0,0,255,255,255,255,0,0,0,0,0,0,0,255,255,0,0,255,255,255,255,255,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,255,255,0,255,255,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,255,255,0,255,255,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,255,255,255,0,0,0,0,255,255,0,255,255,0,0,0,0,255,255,255,0,0,0,0,255,255,255,0,255,255,0,255,255,0,255,255,255,0,0,255,255,0,0,255,255,0,0,0,
  255,255,255,0,255,255,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,255,255,0,0,0,255,255,255,255,0,0,255,255,255,255,255,255,255,255,0,0,255,255,255,255,0,0,0,255,255,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,255,255,255,255,255,255,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,255,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,0,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,0,0,255,255,0,0,0,255,255,0,255,255,0,0,255,255,255,0,255,255,0,255,255,255,255,0,255,255,255,255,0,255,255,0,255,255,255,0,0,255,255,0,0,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,255,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,0,0,0,255,255,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,255,255,255,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,255,255,0,0,255,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,0,0,0,255,255,0,0,255,255,0,0,0,0,0,0,255,
  255,0,0,0,0,255,255,255,0,0,0,0,0,0,0,255,255,0,0,255,255,0,0,255,255,0,0,0,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,0,0,0,0,255,255,255,255,0,0,0,255,255,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,255,255,255,255,255,0,0,0,0,0,255,255,0,0,0,0,0,255,255,255,255,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,0,0,255,255,0,0,0,0,0,0,255,255,255,255,255,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,255,255,0,0,255,255,0,0,0,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,255,255,255,0,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,0,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,0,0,255,255,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,0,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,0,255,255,255,255,0,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,0,255,255,255,255,0,0,0,0,0,0,0,0,
  0,0,0,0,255,255,255,255,0,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,0,255,255,255,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,0,0,0,255,255,0,0,255,255,0,0,0,0,0,0,255,
  255,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,0,0,255,255,0,0,0,255,255,0,255,255,0,255,255,255,255,0,255,255,0,255,255,255,255,0,255,255,0,255,255,255,255,0,255,255,0,0,0,0,0,0,0,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,255,255,0,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,255,255,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,0,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,255,255,255,0,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,255,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,0,0,0,255,255,0,0,255,255,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,255,255,0,0,255,255,0,0,0,255,255,255,255,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,0,0,0,0,255,255,0,255,255,0,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,255,255,0,0,255,255,
  255,255,255,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,255,0,0,255,255,0,0,0,255,0,0,255,255,0,255,0,0,0,0,255,255,255,255,0,0,0,0,255,255,0,255,0,0,0,0,255,255,0,0,0,255,0,255,255,255,255,255,255,255,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,255,0,0,255,255,0,0,0,255,0,0,255,255,0,255,0,0,0,0,255,255,255,255,0,0,0,0,255,255,0,255,0,0,0,0,255,255,0,0,0,0,0,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,0,0,0,255,255,0,0,255,255,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,255,255,255,0,0,255,255,0,0,255,255,0,0,0,255,255,255,255,255,0,0,0,0,0,0,0,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,255,255,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,
  0,0,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,0,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,255,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,255,255,0,0,0,255,255,255,255,0,0,0,0,255,255,0,255,255,0,0,0,255,255,0,0,255,255,0,255,255,255,0,0,255,255,0,0,0,0,0,0,0,0,0,255,255,255,255,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,255,0,0,255,255,0,0,255,255,0,255,255,255,255,255,255,255,0,0,0,0,0,0,0,0,0,255,255,0,0,0,255,255,0,255,255,255,0,255,255,255,0,255,255,255,255,255,255,255,0,255,255,255,255,255,255,255,0,255,255,0,255,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,0,0,0,0,0,0,0,0,255,255,0,0,0,255,255,0,255,255,255,0,0,255,255,0,255,255,255,255,0,255,255,0,255,255,0,255,255,255,255,0,255,255,0,0,255,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,255,255,255,0,0,0,0,255,255,0,255,255,0,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,0,255,255,
  0,255,255,0,0,0,0,255,255,255,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,0,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,255,255,255,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,0,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,255,255,255,0,0,0,255,255,255,255,0,0,0,0,0,0,255,255,255,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,0,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,255,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,0,255,255,0,255,255,255,0,0,255,255,0,0,0,0,0,0,0,0,0,0,255,255,255,255,0,0,0,255,255,0,0,255,255,0,0,255,255,255,0,0,0,0,0,0,255,255,255,0,0,0,0,0,0,0,255,255,255,0,0,255,255,0,0,255,255,0,0,0,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,0,0,255,0,255,255,0,255,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,
  255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,0,255,255,255,255,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,255,0,255,255,0,255,255,255,255,255,255,255,0,255,255,255,0,255,255,255,0,255,255,0,0,0,255,255,0,0,0,0,0,0,0,0,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,0,255,255,0,255,255,0,0,0,0,255,255,255,0,0,0,0,0,255,255,255,0,0,0,0,255,255,0,255,255,0,0,255,255,0,0,0,255,255,0,0,0,0,0,0,0,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,0,255,255,255,255,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,255,0,255,255,0,0,0,255,255,0,255,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,255,0,
  0,255,255,0,0,255,255,0,255,255,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,255,255,255,255,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,255,0,0,0,0,0,0,0,0,0,0,255,255,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,0,0,0,0,0,0,255,255,255,0,0,0,0,255,255,0,255,255,0,0,255,255,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,255,255,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,0,0,0,0,0,0,0,255,255,0,0,0,255,255,255,255,255,0,0,255,255,0,0,255,255,0,0,0,255,255,255,0,255,255,0,0,0,0,0,0,0,0,0,255,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,255,255,255,0,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,255,255,0,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,0,0,0,255,255,0,0,255,255,0,0,255,255,0,0,0,0,0,0,255,255,0,0,255,255,0,0,0,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,255,255,255,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,0,255,255,255,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,0,0,0,255,255,0,0,255,255,0,0,255,255,255,255,255,255,0,0,255,255,0,0,0,0,0,0,0,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,0,0,0,0,255,255,0,255,255,0,0,0,255,255,0,0,0,0,0,255,255,255,255,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,255,255,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,0,255,255,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,0,255,255,255,255,255,0,0,0,0,0,0,255,255,0,0,255,255,255,255,255,0,0,0,255,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,255,255,0,0,0,255,255,255,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,255,255,255,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,0,255,255,255,255,0,0,0,255,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,255,255,0,0,255,255,0,255,255,0,0,0,255,255,255,255,0,0,0,0,255,255,0,255,255,0,0,255,255,255,0,0,255,255,0,0,0,0,0,0,0,0,0,0,255,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,
  0,0,0,0,0,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,255,255,0,0,255,255,255,255,255,255,255,0,255,255,255,255,255,255,255,0,255,255,0,255,0,255,255,0,255,255,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,0,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,0,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,0,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,255,255,255,0,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,255,255,255,0,0,0,255,255,0,0,0,0,0,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,0,255,255,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,0,255,255,255,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,255,255,255,0,0,0,255,255,255,0,255,
  255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,0,0,0,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,0,0,255,255,0,0,0,0,0,0,0,255,255,255,255,0,0,0,0,0,0,0,255,255,0,0,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,255,255,255,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,255,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,0,255,255,255,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,0,255,255,255,255,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,255,255,0,255,255,0,255,0,255,255,0,255,255,255,255,255,255,255,0,255,255,255,255,255,255,255,0,0,255,255,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,255,255,0,0,255,255,
  0,255,255,0,0,0,0,255,255,255,0,0,0,0,255,255,0,255,255,0,0,255,255,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,0,255,255,255,255,255,0,0,0,0,0,0,255,255,0,0,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,0,0,255,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,255,0,0,255,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,255,255,255,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,255,255,255,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,255,255,255,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,0,255,255,0,255,255,0,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,0,0,0,0,0,0,255,255,255,0,0,0,0,255,255,0,255,255,0,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,255,255,255,255,0,0,0,255,255,0,0,255,255,0,0,255,255,0,0,0,0,0,0,255,255,0,0,255,255,0,0,0,255,255,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,255,255,0,0,0,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,255,255,0,0,0,0,0,0,0,0,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,0,255,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,0,0,0,255,255,0,0,255,255,0,0,255,255,255,255,255,255,0,0,255,255,0,0,0,0,0,0,0,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,0,255,255,0,0,0,0,255,255,0,0,255,255,255,255,0,0,0,0,0,0,0,255,255,0,0,0,255,255,255,255,255,0,0,255,255,0,0,255,255,0,0,0,255,255,255,255,255,255,0,0,0,0,0,0,0,0,255,255,0,0,255,255,0,0,0,0,0,0,0,0,0,
  0,0,255,255,255,255,0,0,0,0,0,0,0,255,255,0,0,0,255,255,255,255,255,0,0,255,255,0,0,255,255,0,0,0,255,255,255,255,255,255,0,0,0,0,0,0,0,0,0,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,0,0,0,0,0,0,0,255,255,0,0,0,255,255,255,255,255,0,0,255,255,0,0,255,255,0,0,0,255,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,255,255,0,0,0,0,0,0,0,255,255,0,0,0,255,255,255,255,255,0,0,255,255,0,0,255,255,0,0,0,255,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,255,255,255,255,0,0,0,0,0,0,0,255,255,0,0,0,0,255,255,255,0,0,0,0,255,255,255,255,255,255,0,255,255,0,0,0,0,255,255,0,0,255,255,255,255,0,0,0,255,255,0,0,255,255,0,0,255,255,255,255,255,255,0,0,255,255,0,0,0,0,0,0,0,255,255,255,255,0,0,0,0,0,0,0,0,0,0,255,255,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,0,0,0,255,255,0,0,255,255,0,0,255,255,255,255,255,255,0,0,255,255,0,0,0,0,0,0,0,255,255,255,255,
  0,0,0,0,0,0,0,0,0,0,0,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,0,0,0,255,255,0,0,255,255,0,0,255,255,255,255,255,255,0,0,255,255,0,0,0,0,0,0,0,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,255,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,0,0,255,255,0,0,0,255,255,0,0,0,255,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,255,255,0,0,0,0,0,0,0,0,0,0,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,255,255,0,0,0,255,255,255,0,0,0,0,255,255,0,255,255,0,0,255,255,0,0,0,255,255,0,255,255,255,255,255,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,0,0,0,255,255,0,0,255,255,0,0,255,255,255,255,
  255,255,0,0,255,255,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,0,0,0,255,255,0,0,0,0,0,0,255,255,255,255,0,0,0,0,255,255,0,0,0,0,0,255,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,255,0,0,0,0,255,255,0,0,0,255,255,255,255,255,255,255,255,255,0,0,255,255,0,0,0,255,255,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,0,0,255,255,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,255,255,255,255,255,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,255,0,0,0,0,0,0,0,0,0,0,255,255,255,255,0,0,0,255,255,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,0,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,0,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,0,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,0,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,255,255,255,255,0,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,0,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,0,0,0,255,255,0,0,255,255,0,0,0,0,0,0,0,0,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,0,255,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,0,255,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,255,255,0,0,0,0,0,0,0,0,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,0,255,255,255,255,255,0,0,0,0,0,0,255,255,0,0,255,255,255,255,255,0,0,0,255,255,0,0,0,0,255,255,0,0,0,255,255,0,0,0,0,0,255,255,255,255,0,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,0,255,255,255,255,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,255,255,0,0,0,0,0,0,0,0,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,0,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  255,255,0,0,0,0,0,0,255,255,0,0,0,0,255,255,255,255,255,255,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,255,255,255,255,255,255,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,255,0,0,0,0,255,255,0,255,255,0,0,0,255,255,0,0,255,0,0,255,255,255,255,0,0,0,0,0,255,255,0,0,0,0,0,255,255,255,0,0,255,255,0,255,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,0,255,255,255,255,0,0,0,255,255,255,255,255,255,0,0,0,0,255,255,0,0,0,0,255,255,255,255,255,255,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,255,255,255,255,255,0,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,255,255,255,0,255,0,255,255,0,0,0,255,255,0,255,255,0,0,255,255,255,255,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,255,0,0,0,0,255,255,255,0,0,0,0,255,255,0,255,255,0,0,0,255,255,0,0,0,0,0,255,255,255,255,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,255,255,0,255,255,0,0,0,0,255,255,255,0,0,0,0,0,0,0,255,255,255,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,0,0,0,0,0,0,0,255,255,
  0,0,0,255,255,255,255,255,0,0,255,255,0,0,255,255,0,0,0,255,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,0,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,0,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,0,0,0,0,0,0,0,0,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,0,255,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,0,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,255,255,0,0,255,255,0,0,255,255,255,0,255,255,0,0,255,255,255,255,255,255,0,0,255,255,0,255,255,255,0,0,255,255,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,0,0,0,255,255,0,255,255,
  0,0,0,255,255,0,255,255,0,0,0,0,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,0,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,0,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,255,255,0,0,0,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,255,255,255,255,0,0,0,255,255,0,255,255,0,0,255,255,0,0,255,255,0,255,255,255,255,0,0,0,255,255,0,0,255,255,0,255,255,0,0,255,255,0,255,255,0,0,255,255,0,0,0,0,0,0,255,255,255,255,255,255,0,0,0,0,255,255,255,255,0,0,0,255,255,0,255,
  255,0,0,255,255,0,0,255,255,0,255,255,0,255,255,0,0,255,255,0,255,255,255,0,255,255,0,255,255,255,255,255,255,0,0,255,255,255,255,0,0,0,0,0,0,255,255,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,255,255,0,255,255,0,0,255,255,0,255,255,0,0,255,255,0,0,0,255,255,0,0,255,255,0,0,0,255,255,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,255,255,0,0,0,255,255,0,0,255,255,0,0,0,255,255,0,0,255,255,0,255,255,0,0,255,255,0,255,255,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,0,0,0,255,0,255,0,0,0,255,0,0,0,0,0,255,0,0,0,255,0,255,0,0,0,255,0,0,0,0,0,255,0,0,0,255,0,255,0,0,0,255,0,0,0,0,0,255,0,0,0,255,0,255,0,0,0,255,0,0,0,0,255,0,255,0,255,0,255,255,0,255,0,255,0,255,0,0,255,0,255,0,255,0,255,255,0,255,0,255,0,255,0,0,255,0,255,0,255,0,255,255,0,255,0,255,0,255,0,0,255,0,255,0,255,0,255,255,0,255,0,255,0,255,0,255,255,
  0,255,255,0,255,255,0,255,255,255,0,255,255,255,255,255,0,255,255,0,255,255,255,255,255,0,255,255,255,0,255,255,0,255,255,0,255,255,0,255,255,255,0,255,255,255,255,255,0,255,255,0,255,255,255,255,255,0,255,255,255,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,255,255,255,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,255,255,255,255,255,0,0,0,0,0,0,255,255,0,0,0,255,255,255,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,255,255,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,
  255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,0,0,0,0,0,0,255,255,0,0,0,255,255,255,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,255,255,255,255,0,255,255,0,0,0,0,0,0,255,255,0,255,255,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,255,0,0,0,0,0,0,255,255,0,255,255,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,255,255,255,255,0,255,255,0,0,0,0,0,0,255,255,0,255,255,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,
  0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,255,255,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,255,255,255,255,255,0,0,0,0,0,0,255,255,0,0,0,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,255,255,255,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,255,255,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,
  0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,255,255,255,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,255,255,255,255,255,255,255,255,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,255,255,255,0,0,0,255,255,0,0,0,0,0,0,255,255,255,255,255,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,255,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,255,0,0,255,255,0,0,0,0,0,0,255,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  255,255,255,255,255,255,0,0,255,255,0,0,0,0,0,0,255,255,0,255,255,255,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,255,255,255,255,0,255,255,255,0,0,0,0,0,0,0,0,255,255,255,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,255,255,0,0,0,0,0,0,0,0,255,255,255,255,0,255,255,255,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,255,0,0,255,255,0,0,0,0,0,0,255,255,0,255,255,255,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,255,255,0,0,0,0,0,0,0,0,255,255,255,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,255,255,255,255,0,255,255,255,0,0,0,0,0,0,0,0,255,255,255,255,0,255,255,255,0,0,255,255,0,255,255,0,
  0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,255,255,255,255,255,255,255,255,0,0,0,0,0,0,0,0,255,255,255,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,255,255,255,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,255,255,0,0,0,0,0,0,0,0,255,255,255,255,255,255,255,255,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,255,255,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,255,255,255,0,0,0,255,255,0,0,0,0,0,0,255,
  255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,0,0,0,255,255,0,0,0,0,0,0,255,255,255,255,255,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,255,255,255,255,255,255,255,255,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,255,255,255,255,255,255,255,255,0,0,0,255,255,0,0,0,255,255,255,255,255,255,255,255,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,255,255,255,255,255,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,0,0,0,0,255,255,255,255,0,0,0,0,255,255,255,255,0,0,0,0,255,255,255,255,0,0,0,0,255,255,255,255,0,0,0,0,255,255,255,255,0,0,0,0,255,255,255,255,0,0,0,0,255,255,255,255,0,0,0,0,0,0,0,0,255,255,255,255,0,0,0,0,255,255,255,255,0,0,0,0,255,255,255,255,0,0,0,0,255,255,255,255,0,0,0,0,255,255,255,255,0,0,0,0,255,255,255,255,0,0,0,0,255,255,255,255,0,0,0,0,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,0,255,255,0,255,255,0,255,255,255,0,0,255,255,0,0,255,0,0,0,255,255,0,255,255,255,0,0,0,255,255,255,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,0,0,0,255,255,0,0,255,255,0,0,255,255,255,255,255,0,0,0,255,255,0,0,255,255,0,0,255,255,255,255,255,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,255,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,0,0,255,255,0,0,255,255,0,0,0,255,255,0,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,255,255,0,0,255,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,0,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,255,255,255,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,0,255,255,0,255,255,0,255,255,255,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,0,0,0,0,255,255,0,0,0,0,0,255,255,255,255,0,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,0,255,255,255,255,0,0,0,0,0,255,255,0,0,0,0,255,255,255,255,255,255,0,0,0,0,255,255,255,0,0,0,0,255,255,0,255,255,0,0,255,255,0,0,0,255,255,0,255,255,255,255,255,255,255,0,255,255,0,0,0,255,255,0,0,255,255,0,255,255,0,0,0,0,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,0,0,0,0,255,255,0,255,255,0,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,0,255,255,0,255,255,0,0,
  0,255,255,0,255,255,0,0,255,255,255,0,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,0,0,0,0,255,255,0,0,0,0,0,0,0,255,255,0,0,0,0,255,255,255,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,0,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,0,255,255,0,255,255,0,255,255,255,255,0,255,255,0,255,255,0,255,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,255,255,255,255,255,255,0,255,255,0,255,255,0,255,255,255,255,0,255,255,0,255,255,0,255,255,255,255,255,255,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,255,255,255,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,255,255,255,0,0,0,255,255,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,0,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,
  255,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,255,255,255,255,255,255,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,0,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,0,255,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,
  0,0,0,0,0,0,0,255,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,0,255,255,0,255,255,0,255,255,255,0,0,0,0,0,0,0,0,0,0,0,255,255,255,0,255,255,0,255,255,0,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,0,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,0,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,255,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,0,255,255,255,255,0,0,0,0,0,255,255,255,0,0,0,255,255,255,255,0,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,
  255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,0,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,0,0,0,0,255,255,255,255,0,0,0,0,255,255,255,255,0,0,0,0,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

static int32 file_chrset16_raw_len=32768;
static const uint8 file_chrset16_raw[]={
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,0,255,0,0,0,0,0,0,255,255,0,255,0,0,255,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,255,255,255,255,0,255,255,0,0,255,255,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,0,255,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,0,255,255,255,255,255,255,255,255,255,255,0,255,255,0,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,0,0,0,0,255,255,255,255,255,0,0,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,0,255,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,255,255,0,0,255,255,255,255,255,255,255,0,255,255,255,255,255,255,255,0,255,255,255,255,255,255,255,0,255,255,255,255,255,255,255,0,0,255,255,255,255,255,0,0,0,0,255,255,255,0,0,0,0,0,0,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,0,0,0,0,0,0,255,255,255,0,0,0,0,255,255,255,255,255,0,0,255,255,255,255,255,255,255,0,0,255,255,255,255,255,0,0,0,0,255,255,255,0,0,0,0,0,0,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,255,255,0,0,0,0,255,255,255,255,0,0,255,255,255,0,0,255,255,255,255,255,255,0,0,255,255,255,255,255,255,0,0,255,255,255,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,255,255,0,0,0,0,0,255,255,255,255,0,0,0,255,255,255,255,255,255,0,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,0,255,255,255,255,255,255,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,255,255,0,0,0,0,255,255,255,255,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,0,0,255,255,255,255,255,0,0,0,0,255,255,255,255,0,0,0,0,255,255,255,255,255,0,0,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,0,0,0,255,255,0,0,255,255,0,0,255,0,0,0,0,255,0,0,255,0,0,0,0,255,0,0,255,255,0,0,255,255,0,0,0,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,0,0,0,0,255,255,255,0,0,255,255,0,0,255,255,0,255,255,255,255,0,255,255,0,255,255,255,255,0,255,255,0,0,255,255,0,0,255,255,255,0,0,0,0,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,0,0,0,0,0,255,255,255,0,0,0,0,255,255,0,255,0,0,0,255,255,0,0,255,0,0,255,255,255,255,0,0,0,255,255,0,0,255,255,0,0,
  255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,0,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,0,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,0,255,255,255,255,0,0,0,0,0,255,255,0,0,0,0,255,255,255,255,255,255,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,0,0,255,255,0,0,255,255,0,0,255,255,255,255,255,255,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,255,0,0,0,0,255,255,255,255,0,0,0,0,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,255,0,255,255,0,0,0,255,255,0,255,255,255,255,255,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,
  0,255,255,0,0,0,255,255,0,255,255,0,0,255,255,255,255,255,255,0,0,255,255,255,255,255,255,0,0,255,255,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,255,255,0,255,255,0,255,255,0,0,255,255,255,255,0,0,255,255,255,0,0,255,255,255,0,0,255,255,255,255,0,0,255,255,0,255,255,0,255,255,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,255,0,0,0,0,0,255,255,255,255,0,0,0,0,255,255,255,255,255,0,0,0,255,255,255,255,255,255,255,0,255,255,255,255,255,0,0,0,255,255,255,255,0,0,0,0,255,255,255,0,0,0,0,0,255,255,0,0,0,0,0,0,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,255,0,0,0,0,255,255,255,255,0,0,0,255,255,255,255,255,0,255,255,255,255,255,255,255,0,0,0,255,255,255,
  255,255,0,0,0,0,255,255,255,255,0,0,0,0,0,255,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,255,255,0,0,0,255,255,255,255,255,255,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,255,255,255,255,255,255,0,0,0,255,255,255,255,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,0,0,0,0,0,0,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,255,255,255,0,255,255,0,255,255,255,255,0,255,255,0,255,255,255,255,0,255,255,0,255,255,0,255,255,255,255,0,255,255,0,0,0,255,255,0,255,255,
  0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,0,0,255,255,0,0,0,255,255,0,0,255,255,0,0,0,0,0,0,0,255,255,255,0,0,0,0,255,255,0,255,255,0,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,0,255,255,0,255,255,0,0,0,0,255,255,255,0,0,0,0,0,0,0,255,255,0,0,255,255,0,0,0,255,255,0,0,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,255,0,255,255,255,255,255,255,255,0,255,255,255,255,255,255,255,0,255,255,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,255,255,0,0,0,255,255,255,255,255,255,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,255,255,255,255,
  255,255,0,0,0,255,255,255,255,0,0,0,0,0,255,255,0,0,0,0,255,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,255,255,0,0,0,255,255,255,255,255,255,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,255,255,255,255,255,255,0,0,0,255,255,255,255,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,255,255,0,0,255,255,255,255,255,255,255,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,255,255,255,255,255,255,255,0,0,255,255,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,0,255,0,0,0,0,255,255,0,255,255,0,0,255,255,255,255,255,255,255,0,0,255,255,0,255,255,0,0,0,0,255,0,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,0,0,0,0,0,0,
  255,255,255,0,0,0,0,0,255,255,255,0,0,0,0,255,255,255,255,255,0,0,0,255,255,255,255,255,0,0,255,255,255,255,255,255,255,0,255,255,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,255,0,255,255,255,255,255,255,255,0,0,255,255,255,255,255,0,0,0,255,255,255,255,255,0,0,0,0,255,255,255,0,0,0,0,0,255,255,255,0,0,0,0,0,0,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,255,255,0,0,0,0,255,255,255,255,0,0,0,0,255,255,255,255,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,0,255,0,0,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,255,255,255,255,255,255,255,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,255,255,255,255,255,255,255,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,255,255,255,255,255,0,0,255,255,0,0,0,255,255,0,255,255,0,0,0,0,255,0,255,255,0,0,0,0,0,0,0,255,255,255,255,255,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,255,0,0,0,0,255,255,0,255,255,0,0,0,255,255,0,0,255,255,255,255,
  255,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,255,0,255,255,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,255,255,0,255,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,0,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,0,255,255,255,0,0,0,0,255,255,255,0,255,255,0,255,255,0,255,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,0,255,255,255,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,255,255,0,0,0,255,255,255,255,0,0,255,255,255,255,255,255,255,255,0,0,255,255,255,255,0,0,0,255,255,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,
  255,0,0,0,0,255,255,255,255,255,255,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,0,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,0,0,0,0,255,255,0,255,255,0,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,255,0,255,255,0,255,255,0,255,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,0,255,255,0,255,255,0,0,0,0,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,255,0,0,0,0,255,255,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,255,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,0,0,255,255,0,0,0,255,255,0,0,0,
  0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,255,255,0,255,255,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,0,0,255,255,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,255,255,255,255,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,255,255,0,0,0,255,255,0,0,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,255,0,0,0,0,255,255,255,255,0,0,0,255,255,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,255,255,255,255,255,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,255,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,
  0,255,255,255,255,255,255,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,255,255,0,0,0,255,255,0,0,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,255,255,255,255,0,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,0,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,255,0,255,255,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,0,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,0,255,255,255,255,255,0,0,
  255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,0,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,0,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,0,255,255,255,255,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,0,0,255,255,0,0,0,
  255,255,0,255,255,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,0,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,255,255,255,255,0,255,255,0,255,255,255,255,0,255,255,0,255,255,255,255,0,255,255,0,255,255,255,0,0,255,255,0,0,0,0,0,0,0,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,0,0,0,0,0,0,255,255,255,0,0,0,0,255,255,0,255,255,0,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,255,255,255,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,0,0,0,255,255,0,0,255,255,0,0,255,
  255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,255,255,255,0,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,255,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,0,0,0,255,255,0,0,255,255,0,255,255,0,0,0,0,255,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,255,0,0,255,255,0,0,255,255,0,0,0,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,0,0,0,0,255,255,0,255,255,0,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,255,255,0,0,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,255,0,0,255,255,0,0,255,255,0,
  0,255,255,0,0,0,255,0,0,255,255,0,255,0,0,0,0,255,255,255,255,0,0,0,0,255,255,0,255,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,255,0,0,255,255,0,0,255,255,0,255,255,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,0,255,0,0,255,255,0,255,0,0,0,0,255,255,255,255,0,0,0,0,255,255,0,255,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,0,0,0,255,255,0,0,255,255,0,255,255,0,0,0,0,255,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,255,255,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,0,255,255,0,0,255,255,0,0,0,255,255,255,0,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,
  255,255,0,255,255,0,0,0,255,255,0,255,255,255,255,255,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,0,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,255,255,0,0,
  0,255,255,255,255,0,0,0,0,255,255,255,255,0,0,0,0,255,255,0,255,255,0,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,255,255,255,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,255,0,0,255,255,0,0,255,255,0,255,255,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,255,255,0,255,255,255,0,255,255,255,0,255,255,255,255,255,255,255,0,255,255,255,255,255,255,255,0,255,255,0,255,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,255,255,0,255,255,255,0,0,255,255,0,255,255,255,255,0,255,255,0,255,255,
  255,255,255,255,255,0,255,255,0,255,255,255,255,0,255,255,0,0,255,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,0,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,0,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,0,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,255,255,255,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,0,0,255,255,0,0,0,255,255,0,255,255,0,0,0,
  255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,255,0,255,255,0,255,255,0,255,255,255,255,0,0,255,255,255,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,0,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,255,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,255,255,255,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,0,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,0,255,255,0,0,0,0,0,0,0,255,255,255,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,0,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,0,0,255,255,
  255,255,255,255,0,0,255,0,255,255,0,255,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,0,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,0,255,255,0,255,255,0,0,0,0,255,255,255,0,0,0,0,0,0,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,
  255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,255,0,255,255,0,255,255,0,255,0,255,255,0,255,255,0,255,0,255,255,0,255,255,255,255,255,255,255,0,255,255,255,0,255,255,255,0,0,255,255,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,0,255,255,0,255,255,0,0,0,255,255,255,255,255,0,0,0,0,255,255,255,0,0,0,0,0,255,255,255,0,0,0,0,255,255,255,255,255,0,0,0,255,255,0,255,255,0,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,0,255,255,255,255,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,255,0,255,255,0,0,
  0,255,255,0,255,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,255,0,255,255,0,0,0,255,255,0,255,255,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,255,0,0,0,0,0,0,255,255,255,0,0,0,0,0,0,255,255,255,0,0,0,0,0,0,255,255,255,0,0,0,0,0,0,255,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,
  0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,0,0,0,0,0,0,255,255,255,0,0,0,0,255,255,0,255,255,0,0,255,255,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,0,0,0,0,0,0,0,255,255,0,0,0,255,255,255,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,0,255,255,255,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,255,255,0,0,0,0,255,255,0,255,255,0,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,0,0,255,255,0,0,0,255,255,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,255,255,0,0,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,0,0,0,0,0,0,255,255,
  0,0,0,0,0,0,255,255,0,0,0,0,255,255,255,255,0,0,0,255,255,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,0,255,255,255,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,0,0,255,255,0,0,0,255,255,0,255,255,255,255,255,255,255,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,255,255,0,0,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,0,0,0,0,255,255,0,255,255,0,0,0,255,255,0,0,255,0,0,0,255,255,0,0,0,0,0,255,255,255,255,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,0,255,255,0,255,255,
  0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,0,255,255,255,255,255,0,0,0,0,0,0,255,255,0,0,255,255,0,0,255,255,0,0,0,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,255,255,0,0,0,255,255,255,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,255,255,255,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,
  0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,0,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,255,255,0,0,255,255,0,255,255,0,0,0,255,255,255,255,0,0,0,0,255,255,255,255,0,0,0,0,255,255,0,255,255,0,0,0,255,255,0,0,255,255,0,255,255,255,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,0,255,255,0,0,255,255,255,255,255,255,255,0,255,255,0,255,0,255,255,0,255,255,0,255,0,255,255,0,255,255,0,
  255,0,255,255,0,255,255,0,255,0,255,255,0,255,255,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,255,255,255,0,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,0,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,0,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,255,255,255,0,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,255,
  255,255,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,0,255,255,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,0,255,255,255,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,255,255,255,0,0,0,255,255,255,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,0,0,255,255,0,0,0,255,255,0,0,255,255,0,0,0,0,0,0,0,255,255,255,0,0,0,0,0,0,0,255,255,0,0,255,255,0,0,0,255,255,0,0,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,255,255,255,255,255,255,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,255,255,0,0,0,0,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,0,255,255,255,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,0,255,255,255,255,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,255,0,255,255,0,255,255,0,255,0,255,255,0,255,255,0,255,0,255,255,0,255,255,255,255,255,255,255,0,0,255,255,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,255,255,0,0,255,255,0,255,255,0,0,0,0,255,255,255,0,0,0,0,0,255,255,255,0,0,0,0,0,255,255,255,0,0,0,0,255,255,0,255,255,0,0,255,255,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,0,255,255,255,255,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,255,255,255,255,255,255,255,0,255,255,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,255,255,0,255,255,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,255,255,255,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,255,255,255,0,0,0,0,255,255,0,0,0,0,
  0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,0,255,255,0,255,255,0,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,0,0,0,0,0,0,255,255,255,0,0,0,0,255,255,0,255,255,0,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,0,0,0,255,255,0,0,255,255,0,255,255,0,0,0,0,255,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,255,0,0,255,255,0,0,255,255,0,0,0,255,255,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,255,255,
  0,0,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,0,255,255,255,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,0,0,255,255,0,0,0,255,255,0,255,255,255,255,255,255,255,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,255,255,0,0,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,0,0,0,0,0,0,255,255,255,0,0,0,0,255,255,0,255,255,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,0,0,0,0,0,0,0,255,255,0,0,0,255,255,255,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,0,255,255,255,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,0,0,0,0,0,0,0,255,255,0,0,0,255,255,255,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,0,255,255,255,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,0,0,0,0,0,0,0,255,255,0,0,0,255,255,255,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,0,255,255,255,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,0,0,0,0,255,255,0,255,255,0,0,0,0,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,0,0,0,0,0,0,0,255,255,0,0,0,255,255,255,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,0,255,255,255,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,0,0,0,255,255,0,0,255,255,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,255,255,0,0,0,255,255,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,255,255,0,0,0,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,0,0,0,0,0,0,255,255,255,0,0,0,0,255,255,0,255,255,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,0,0,255,255,0,0,0,255,255,0,255,255,255,255,255,255,255,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,255,255,0,0,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,0,0,255,255,0,0,0,255,255,0,255,255,255,255,255,255,255,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,255,255,0,0,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  255,255,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,0,0,255,255,0,0,0,255,255,0,255,255,255,255,255,255,255,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,255,255,0,0,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,255,255,0,0,0,255,255,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,255,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,
  0,0,0,0,0,0,0,255,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,255,0,0,0,0,0,0,255,255,255,0,0,0,0,255,255,0,255,255,0,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,255,255,255,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,0,0,0,0,255,255,0,255,255,0,0,0,0,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,0,0,0,0,255,255,0,255,255,0,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,255,255,255,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,
  255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,0,0,0,0,255,255,255,255,255,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,255,255,0,255,255,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,255,255,0,0,0,255,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,255,255,255,255,255,255,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,0,255,255,0,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,0,0,255,255,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,255,255,255,255,255,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,0,0,0,0,0,0,255,255,255,0,0,0,0,255,255,0,255,255,0,0,0,0,0,0,0,0,0,0,0,
  255,255,255,255,255,0,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,0,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,0,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,0,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,0,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,0,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,255,255,0,0,0,255,255,0,0,255,255,0,0,0,0,0,0,0,0,0,0,255,255,0,0,
  255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,0,255,255,255,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,0,255,255,255,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,0,255,255,255,255,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,0,0,255,255,0,0,0,255,255,0,255,
  255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,0,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,255,255,0,0,0,0,0,0,0,0,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,0,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,255,255,0,0,0,255,255,0,0,255,255,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,255,255,0,0,0,255,255,255,255,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,0,0,0,0,255,255,0,255,255,0,0,0,255,255,0,0,255,0,0,0,255,255,0,0,0,0,0,255,255,255,
  255,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,255,0,0,255,255,0,255,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,0,255,255,255,255,0,0,0,0,0,255,255,0,0,0,0,255,255,255,255,255,255,0,0,0,0,255,255,0,0,0,0,255,255,255,255,255,255,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,0,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,255,255,255,0,0,0,255,255,0,0,0,255,0,0,255,255,0,0,255,255,0,0,255,255,0,255,255,255,255,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,0,0,0,0,255,255,0,255,255,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,
  0,0,0,0,255,255,255,255,255,255,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,255,255,0,255,255,0,0,0,0,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,0,0,0,0,0,0,0,255,255,0,0,0,255,255,255,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,0,255,255,255,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,0,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,
  255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,0,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,0,255,255,255,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,0,255,255,0,255,255,0,255,255,255,0,0,0,0,0,0,0,0,0,0,255,255,0,255,255,255,0,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,0,255,255,0,255,255,0,255,255,255,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,255,255,0,255,255,255,0,0,255,255,0,255,255,255,255,0,255,255,0,255,255,255,255,255,255,
  255,0,255,255,0,255,255,255,255,0,255,255,0,0,255,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,0,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,0,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,0,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,255,255,0,255,255,0,0,0,

  255,255,0,0,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,255,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,255,0,255,255,0,0,0,255,255,0,255,255,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,255,255,255,0,0,255,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,255,0,255,255,0,0,0,255,255,0,255,255,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,255,255,0,255,255,0,0,255,255,255,0,255,0,0,255,255,255,255,0,0,0,255,255,255,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,255,255,0,0,0,0,255,255,255,255,0,0,0,0,255,255,255,255,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,255,255,0,0,255,255,0,255,255,0,0,255,255,0,255,255,0,0,0,0,255,255,0,255,255,0,0,0,0,255,255,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,255,255,0,0,0,0,255,255,0,255,255,0,0,0,0,255,255,0,255,255,0,0,255,255,0,255,255,0,0,255,255,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,0,0,0,255,0,255,0,0,0,255,0,0,0,0,0,255,0,0,0,255,0,255,0,0,0,255,0,0,0,0,0,255,0,0,0,255,0,255,0,0,0,255,0,0,0,0,0,255,0,0,0,255,0,255,0,0,0,255,0,0,0,0,0,255,0,0,0,255,0,255,0,0,0,255,0,0,0,0,0,255,0,0,0,255,0,255,0,0,0,255,0,0,0,0,0,255,0,0,0,255,0,255,0,0,0,255,0,0,0,0,0,255,0,0,0,255,0,255,0,0,0,255,0,0,0,255,0,255,0,255,0,255,255,0,255,0,255,0,255,0,0,255,0,255,0,255,0,255,255,0,255,0,255,0,255,0,0,255,0,255,0,255,0,255,255,0,255,0,255,0,255,0,0,255,0,255,0,255,0,255,255,0,255,0,255,0,255,0,0,255,0,255,0,255,0,255,255,0,255,0,255,0,255,0,0,255,0,255,0,255,0,255,255,0,255,0,255,0,255,0,0,255,0,255,0,255,0,255,255,0,255,0,255,0,255,0,0,255,0,255,0,255,0,255,255,0,255,0,255,0,255,0,255,255,0,255,255,255,0,255,0,255,255,255,0,255,255,
  255,255,255,0,255,255,255,0,255,0,255,255,255,0,255,255,255,255,255,0,255,255,255,0,255,0,255,255,255,0,255,255,255,255,255,0,255,255,255,0,255,0,255,255,255,0,255,255,255,255,255,0,255,255,255,0,255,0,255,255,255,0,255,255,255,255,255,0,255,255,255,0,255,0,255,255,255,0,255,255,255,255,255,0,255,255,255,0,255,0,255,255,255,0,255,255,255,255,255,0,255,255,255,0,255,0,255,255,255,0,255,255,255,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,255,255,255,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,
  0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,255,255,255,255,255,0,0,0,0,0,0,255,255,0,0,0,255,255,255,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,255,255,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,
  0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,0,0,0,0,0,0,255,255,0,0,0,255,255,255,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,255,255,255,255,0,255,255,0,0,0,0,0,0,255,255,0,255,255,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,
  255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,255,0,0,0,0,0,0,255,255,0,255,255,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,255,255,255,255,0,255,255,0,0,0,0,0,0,255,255,0,255,255,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,
  255,0,0,0,255,255,0,255,255,0,255,255,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,255,255,255,255,255,0,0,0,0,0,0,255,255,0,0,0,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,255,255,255,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,255,255,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,255,255,255,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,
  255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,255,255,255,255,255,255,255,255,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,255,255,255,0,0,0,255,255,0,0,0,0,0,0,255,255,255,255,255,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,
  0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,255,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,255,0,0,255,255,0,0,0,0,0,0,255,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,0,0,255,255,0,0,0,0,0,0,255,255,0,255,255,255,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,
  255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,255,255,255,255,0,255,255,255,0,0,0,0,0,0,0,0,255,255,255,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,255,255,0,0,0,0,0,0,0,0,255,255,255,255,0,255,255,255,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,255,0,0,255,255,0,0,0,0,0,0,255,255,0,255,255,255,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,
  255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,255,255,0,0,0,0,0,0,0,0,255,255,255,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,255,255,255,255,0,255,255,255,0,0,0,0,0,0,0,0,255,255,255,255,0,255,255,255,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,255,255,255,255,255,255,255,255,0,0,0,0,0,0,0,0,255,255,255,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,255,255,255,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,255,255,0,0,0,0,0,0,0,0,255,255,255,255,255,255,255,255,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,255,255,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,
  255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,255,255,255,0,0,0,255,255,0,0,0,0,0,0,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,0,0,0,255,255,0,0,0,0,0,0,255,255,255,255,255,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,255,255,255,255,255,255,255,255,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,255,255,255,255,255,255,255,255,0,0,0,255,255,0,0,0,255,255,255,255,255,255,255,255,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,
  0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,0,0,0,0,255,255,255,255,0,0,0,0,255,255,255,255,0,0,0,0,255,255,255,255,0,0,0,0,255,255,255,255,0,0,0,0,255,255,255,255,0,0,0,0,255,255,255,255,0,0,0,0,255,255,255,255,0,0,0,0,255,255,255,255,0,0,0,0,255,255,255,255,0,0,0,0,255,255,255,255,0,0,0,0,255,255,255,255,0,0,0,0,255,255,255,255,0,0,0,0,255,255,255,255,0,0,0,0,255,255,255,255,0,0,0,0,255,255,255,255,0,0,0,0,0,0,0,0,255,255,255,255,0,0,0,0,255,255,255,255,0,0,0,0,255,255,
  255,255,0,0,0,0,255,255,255,255,0,0,0,0,255,255,255,255,0,0,0,0,255,255,255,255,0,0,0,0,255,255,255,255,0,0,0,0,255,255,255,255,0,0,0,0,255,255,255,255,0,0,0,0,255,255,255,255,0,0,0,0,255,255,255,255,0,0,0,0,255,255,255,255,0,0,0,0,255,255,255,255,0,0,0,0,255,255,255,255,0,0,0,0,255,255,255,255,0,0,0,0,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,0,255,255,0,255,255,0,255,255,255,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,255,0,0,0,255,255,255,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,0,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,255,255,0,0,0,255,255,0,0,255,255,0,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,255,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,255,0,255,255,0,0,0,255,255,0,0,255,255,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,255,255,0,255,255,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,0,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,255,255,255,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,255,255,255,0,255,255,0,255,255,0,255,255,255,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,0,0,0,0,255,255,0,0,0,0,0,255,255,255,255,0,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,0,255,255,255,255,0,0,0,0,0,255,255,0,0,0,0,255,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,0,0,0,0,255,255,0,255,255,0,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,255,255,255,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,0,255,255,0,255,255,0,0,0,0,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,0,0,0,0,255,255,0,255,255,0,0,255,255,0,0,0,255,255,
  0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,255,255,255,0,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,0,0,0,255,255,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,255,255,0,0,0,0,255,255,255,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,0,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,0,255,255,0,255,255,0,255,255,255,255,0,255,255,0,255,255,255,255,0,255,255,0,255,255,0,255,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,255,255,255,255,255,255,0,255,
  255,0,255,255,0,255,255,255,255,0,255,255,0,255,255,255,255,255,255,0,0,255,255,0,255,255,255,255,255,255,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,255,255,255,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,0,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,
  255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,255,255,255,255,255,255,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,0,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,0,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,0,255,255,0,255,255,0,255,255,255,0,0,0,0,0,0,0,0,0,0,0,255,255,255,0,255,255,0,255,255,0,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,0,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,0,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,0,0,0,0,255,255,0,0,255,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,0,255,255,255,255,0,0,0,0,0,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,0,255,255,0,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,255,255,0,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,0,0,0,0,255,255,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,0,0,0,255,255,0,0,255,0,0,0,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255,255,0,0,0,255,255,255,255,255,0,0,0,255,255,255,255,255,0,0,0,255,255,255,255,255,0,0,0,255,255,255,255,255,0,0,0,255,255,255,255,255,0,0,0,255,255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

static int32 file_qb64_pal_len=1024;
static const uint8 file_qb64_pal[]={
  0,0,0,0,170,0,0,0,0,170,0,0,170,170,0,0,0,0,170,0,170,0,170,0,0,85,170,0,170,170,170,0,85,85,85,0,255,85,85,0,85,255,85,0,255,255,85,0,85,85,255,0,255,85,255,0,85,255,255,0,255,255,255,0,0,0,0,0,20,20,20,0,32,32,32,0,45,45,45,0,57,57,57,0,69,69,69,0,81,81,81,0,97,97,97,0,113,113,113,0,130,130,130,0,146,146,146,0,162,162,162,0,182,182,182,0,202,202,202,0,227,227,227,0,255,255,255,0,255,0,0,0,255,0,65,0,255,0,125,0,255,0,190,0,255,0,255,0,190,0,255,0,125,0,255,0,65,0,255,0,0,0,255,0,0,65,255,0,0,125,255,0,0,190,255,0,0,255,255,0,0,255,190,0,0,255,125,0,0,255,65,0,0,255,0,0,65,255,0,0,125,255,0,0,190,255,0,0,255,255,0,0,255,190,0,0,255,125,0,0,255,65,0,0,255,125,125,0,255,125,158,0,255,125,190,0,255,125,223,0,255,125,255,0,223,125,255,0,190,125,255,0,158,125,255,0,125,125,255,0,125,158,255,0,125,190,255,0,125,223,255,0,125,255,255,0,125,255,223,0,125,255,190,0,125,255,158,0,125,255,125,0,158,255,125,0,190,255,125,0,223,255,125,0,255,255,125,0,255,223,125,0,255,190,125,0,255,158,125,0,255,
  182,182,0,255,182,198,0,255,182,219,0,255,182,235,0,255,182,255,0,235,182,255,0,219,182,255,0,198,182,255,0,182,182,255,0,182,198,255,0,182,219,255,0,182,235,255,0,182,255,255,0,182,255,235,0,182,255,219,0,182,255,198,0,182,255,182,0,198,255,182,0,219,255,182,0,235,255,182,0,255,255,182,0,255,235,182,0,255,219,182,0,255,198,182,0,113,0,0,0,113,0,28,0,113,0,57,0,113,0,85,0,113,0,113,0,85,0,113,0,57,0,113,0,28,0,113,0,0,0,113,0,0,28,113,0,0,57,113,0,0,85,113,0,0,113,113,0,0,113,85,0,0,113,57,0,0,113,28,0,0,113,0,0,28,113,0,0,57,113,0,0,85,113,0,0,113,113,0,0,113,85,0,0,113,57,0,0,113,28,0,0,113,57,57,0,113,57,69,0,113,57,85,0,113,57,97,0,113,57,113,0,97,57,113,0,85,57,113,0,69,57,113,0,57,57,113,0,57,69,113,0,57,85,113,0,57,97,113,0,57,113,113,0,57,113,97,0,57,113,85,0,57,113,69,0,57,113,57,0,69,113,57,0,85,113,57,0,97,113,57,0,113,113,57,0,113,97,57,0,113,85,57,0,113,69,57,0,113,81,81,0,113,81,89,0,113,81,97,0,113,81,105,0,113,81,113,0,105,81,113,0,97,81,113,0,89,81,113,0,81,81,113,0,
  81,89,113,0,81,97,113,0,81,105,113,0,81,113,113,0,81,113,105,0,81,113,97,0,81,113,89,0,81,113,81,0,89,113,81,0,97,113,81,0,105,113,81,0,113,113,81,0,113,105,81,0,113,97,81,0,113,89,81,0,65,0,0,0,65,0,16,0,65,0,32,0,65,0,49,0,65,0,65,0,49,0,65,0,32,0,65,0,16,0,65,0,0,0,65,0,0,16,65,0,0,32,65,0,0,49,65,0,0,65,65,0,0,65,49,0,0,65,32,0,0,65,16,0,0,65,0,0,16,65,0,0,32,65,0,0,49,65,0,0,65,65,0,0,65,49,0,0,65,32,0,0,65,16,0,0,65,32,32,0,65,32,40,0,65,32,49,0,65,32,57,0,65,32,65,0,57,32,65,0,49,32,65,0,40,32,65,0,32,32,65,0,32,40,65,0,32,49,65,0,32,57,65,0,32,65,65,0,32,65,57,0,32,65,49,0,32,65,40,0,32,65,32,0,40,65,32,0,49,65,32,0,57,65,32,0,65,65,32,0,65,57,32,0,65,49,32,0,65,40,32,0,65,45,45,0,65,45,49,0,65,45,53,0,65,45,61,0,65,45,65,0,61,45,65,0,53,45,65,0,49,45,65,0,45,45,65,0,45,49,65,0,45,53,65,0,45,61,65,0,45,65,65,0,45,65,61,0,45,65,53,0,45,65,49,0,45,65,45,0,49,65,45,0,53,65,45,0,61,65,45,0,65,65,45,0,65,61,45,0,65,53,45,0,65,49,45,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0
};

static int32 file_qb64ega_pal_len=256;
static const uint8 file_qb64ega_pal[]={
  0,0,0,0,170,0,0,0,0,170,0,0,170,170,0,0,0,0,170,0,170,0,170,0,0,170,170,0,170,170,170,0,85,0,0,0,255,0,0,0,85,170,0,0,255,170,0,0,85,0,170,0,255,0,170,0,85,170,170,0,255,170,170,0,0,85,0,0,170,85,0,0,0,255,0,0,170,255,0,0,0,85,170,0,170,85,170,0,0,255,170,0,170,255,170,0,85,85,0,0,255,85,0,0,85,255,0,0,255,255,0,0,85,85,170,0,255,85,170,0,85,255,170,0,255,255,170,0,0,0,85,0,170,0,85,0,0,170,85,0,170,170,85,0,0,0,255,0,170,0,255,0,0,170,255,0,170,170,255,0,85,0,85,0,255,0,85,0,85,170,85,0,255,170,85,0,85,0,255,0,255,0,255,0,85,170,255,0,255,170,255,0,0,85,85,0,170,85,85,0,0,255,85,0,170,255,85,0,0,85,255,0,170,85,255,0,0,255,255,0,170,255,255,0,85,85,85,0,255,85,85,0,85,255,85,0,255,255,85,0,85,85,255,0,255,85,255,0,85,255,255,0,255,255,255,0
};

uint16 *unicode16_buf=(uint16*)malloc(1);
int32 unicode16_buf_size=1;
void convert_text_to_utf16(int32 fonthandle,void *buf,int32 size){
  //expand buffer if necessary
  if (unicode16_buf_size<(size*4+4)){
    unicode16_buf_size=size*4+4;
    free(unicode16_buf);
    unicode16_buf=(uint16*)malloc(unicode16_buf_size);
  }
  //convert text
  if ((fontflags[fonthandle]&32)&&(fonthandle!=NULL)){//unicode font
    if (size==1) size=4;
    convert_unicode(32,buf,size,16,unicode16_buf);
  }else{
    convert_unicode(1,buf,size,16,unicode16_buf);
  }
}



qbs *unknown_opcode_mess;

extern uint32 ercl;
extern uint32 inclercl;
extern char* includedfilename;

int32 exit_blocked=0;
int32 exit_value=0;
//1=X-button
//2=CTRL-BREAK
//3=X-button and CTRL-BREAK

//MLP
//int32 qbshlp1=0;

char *fixdir(qbs *filename){
  //note: changes the slashes in a filename to make it compatible with the OS
  //applied to QB commands: open, bload/bsave, loadfont, loadimage, sndopen/sndplayfile
  static int32 i;

  if (cloud_app){
    for (i=0;i<filename->len;i++){
      if ((filename->chr[i]>=48)&&(filename->chr[i]<=57)) goto ok;
      if ((filename->chr[i]>=65)&&(filename->chr[i]<=90)){filename->chr[i]+=32; goto ok;}//force lowercase
      if ((filename->chr[i]>=97)&&(filename->chr[i]<=122)) goto ok;
      if (filename->chr[i]==95) goto ok;//underscore
      if (filename->chr[i]==46){
    if (i!=0) goto ok;//period cannot be the first character
      }
      if (filename->chr[i]==0){
    if (i==(filename->len-1)) goto ok;//NULL terminator
      }
      error(263);//"Paths/Filename illegal in QLOUD"
    ok:;
    }
  }

  for (i=0;i<filename->len;i++){
#ifdef QB64_WINDOWS
    if (filename->chr[i]==47) filename->chr[i]=92;
#else
    if (filename->chr[i]==92) filename->chr[i]=47;
#endif
  }
  return (char*)filename->chr;
}


int32 width8050switch=1;//if set, can automatically switch to WIDTH 80,50 if LOCATE'ing beyond row 26

uint32 pal[256];

extern qbs* nothingstring;

static uint32 sdl_shiftstate=0;

static uint32 sdl_scroll_lock=0;
static uint32 sdl_insert=0;
static uint32 sdl_scroll_lock_prepared=1;
static uint32 sdl_insert_prepared=1;

int32 sub_screen_height_in_characters=-1;//-1=undefined
int32 sub_screen_width_in_characters=-1;//-1=undefined
int32 sub_screen_font=-1;//-1=undefined
int32 sub_screen_keep_page0=0;

int32 key_repeat_on=0;




uint32 palette_256[256];
uint32 palette_64[64];

//QB64 2D PROTOTYPE 1.0

int32 pages=1;
int32 *page=(int32*)calloc(1,4);

#define IMG_BUFFERSIZE 4096
img_struct *img=(img_struct*)malloc(IMG_BUFFERSIZE*sizeof(img_struct));
int32 nimg=IMG_BUFFERSIZE;
int32 nextimg=0;

uint32 *fimg=(uint32*)malloc(IMG_BUFFERSIZE*4);//a list to recover freed indexes
int32 nfimg=IMG_BUFFERSIZE;
int32 lastfimg=-1;//-1=no freed indexes exist

uint8 *cblend=NULL;
uint8 *ablend=NULL;
uint8 *ablend127;
uint8 *ablend128;
//to save 16MB of RAM, software blend tables are only allocated if a 32-bit image is created
void init_blend(){
  uint8 *cp;
  int32 i,x2,x3,i2,z;
  float f,f2,f3;
  cblend=(uint8*)malloc(16777216);
  cp=cblend;
  for (i=0;i<256;i++){//source alpha
    for (x2=0;x2<256;x2++){//source
      for (x3=0;x3<256;x3++){//dest
    f=i;
    f2=x2;
    f3=x3;
    f/=255.0;//0.0-1.0
    *cp++=qbr_float_to_long((f*f2)+((1.0-f)*f3));//CINT(0.0-255.0)
      }}}
  /*
    "60%+60%=84%" formula
    imagine a 60% opaque lens, you can see 40% of whats behind
    now put another 60% opaque lens on top of it
    you can now see 40% of the previous lens of which 40% is of the original scene
    40% of 40% is 16%
    100%-16%=84%
    V1=60, V2=60
    v1=V1/100, v2=V2/100
    iv1=1-v1, iv2=1-v2
    iv3=iv1*iv2
    v3=1-iv3
    V3=v3*100
  */
  ablend=(uint8*)malloc(65536);
  cp=ablend;
  for (i=0;i<256;i++){//first alpha value
    for (i2=0;i2<256;i2++){//second alpha value
      f=i; f2=i2;
      f/=255.0; f2/=255.0;
      f=1.0-f; f2=1.0-f2;
      f3=f*f2;
      z=qbr_float_to_long((1.0-f3)*255.0);
      *cp++=z;
    }}
  ablend127=ablend+(127<<8);
  ablend128=ablend+(128<<8);
}


uint32 display_page_index=0;
uint32 write_page_index=0;
uint32 read_page_index=0;
//use of non-indexed forms assumes valid indexes (may not be suitable for all commands)
img_struct *write_page=NULL;
img_struct *read_page=NULL;
img_struct *display_page=NULL;
uint32 *display_surface_offset=0;

void restorepalette(img_struct* im){
  static uint32 *pal;
  if (im->bytes_per_pixel==4) return;
  pal=im->pal;

  switch(im->compatible_mode){

  case 1:
    /*
      SCREEN Mode 1 Syntax:  COLOR [background][,palette]
      \A6 background is the screen color (range = 0-15)
      \A6 palette is a three-color palette (range = 0-1)
      0 = green, red, and brown         1 = cyan, magenta, and bright white
      Note: option 1 is the default, palette can override these though
      OPTION 1:*DEFAULT*
      0=black(color 0)
      1=cyan(color 3)
      2=purple(color 5)
      3=light grey(color 7)
      OPTION 0:
      0=black(color 0)
      1=green(color 2)
      2=red(color 4)
      3=brown(color 6)
    */
    pal[0]=palette_256[0];
    pal[1]=palette_256[3];
    pal[2]=palette_256[5];
    pal[3]=palette_256[7];
    return;
    break;

  case 2://black/white 2 color palette
    pal[0]=0;
    pal[1]=0xFFFFFF;
    return;
    break;

  case 9://16 colors selected from 64 possibilities
    pal[0]=palette_64[0];
    pal[1]=palette_64[1];
    pal[2]=palette_64[2];
    pal[3]=palette_64[3];
    pal[4]=palette_64[4];
    pal[5]=palette_64[5];
    pal[6]=palette_64[20];
    pal[7]=palette_64[7];
    pal[8]=palette_64[56];
    pal[9]=palette_64[57];
    pal[10]=palette_64[58];
    pal[11]=palette_64[59];
    pal[12]=palette_64[60];
    pal[13]=palette_64[61];
    pal[14]=palette_64[62];
    pal[15]=palette_64[63];
    return;
    break;

  case 10://4 colors selected from 9 possibilities (indexes held in array pal[4-7])
    pal[4]=0;
    pal[5]=4;
    pal[6]=6;
    pal[7]=8;
    return;
    break;

  case 11://black/white 2 color palette
    pal[0]=0;
    pal[1]=0xFFFFFF;
    return;
    break;

  case 13:
    memcpy(pal,palette_256,1024);
    return;
    break;

  case 256:
    memcpy(pal,palette_256,1024);
    return;
    break;

  default:
    //default 16 color palette
    memcpy(pal,palette_256,64);

  };//switch

}//restorepalette




void pset(int32 x,int32 y,uint32 col){
  static uint8 *cp;
  static uint32 *o32;
  static uint32 destcol;
  if (write_page->bytes_per_pixel==1){
    write_page->offset[y*write_page->width+x]=col&write_page->mask;
    return;
  }else{
    if (write_page->alpha_disabled){
      write_page->offset32[y*write_page->width+x]=col;
      return;
    }
    switch(col&0xFF000000){
    case 0xFF000000://100% alpha, so regular pset (fast)
      write_page->offset32[y*write_page->width+x]=col;
      return;
      break;
    case 0x0://0%(0) alpha, so no pset (very fast)
      return;
      break;
    case 0x80000000://~50% alpha (optomized)

      o32=write_page->offset32+(y*write_page->width+x);
      *o32=(((*o32&0xFEFEFE)+(col&0xFEFEFE))>>1)+(ablend128[*o32>>24]<<24);
      return;
      break; 
    case 0x7F000000://~50% alpha (optomized)
      o32=write_page->offset32+(y*write_page->width+x);
      *o32=(((*o32&0xFEFEFE)+(col&0xFEFEFE))>>1)+(ablend127[*o32>>24]<<24);
      return;
      break;
    default://other alpha values (uses a lookup table)
      o32=write_page->offset32+(y*write_page->width+x);
      destcol=*o32;
      cp=cblend+(col>>24<<16);
      *o32=
    cp[(col<<8&0xFF00)+(destcol&255)    ]
    +(cp[(col&0xFF00)   +(destcol>>8&255) ]<<8)
    +(cp[(col>>8&0xFF00)+(destcol>>16&255)]<<16)
    +(ablend[(col>>24)+(destcol>>16&0xFF00)]<<24);
    };
  }
}



/*
  img_struct *img=(img_struct*)malloc(1024*sizeof(img_struct));
  uint32 nimg=1024;
  uint32 nextimg=0;//-1=none have been assigned

  uint32 *freeimg=(uint32*)malloc(1024*4);//a list to recover freed indexes
  uint32 nfreeimg=1024;
  uint32 lastfreeimg=-1;//-1=no freed indexes exist
*/

//returns an index to free img structure
uint32 newimg(){
  static int32 i;
  if (lastfimg!=-1){
    i=fimg[lastfimg--];
    goto gotindex;
  }
  if (nextimg<nimg){
    i=nextimg++;
    goto gotindex;
  }
  img=(img_struct*)realloc(img,(nimg+IMG_BUFFERSIZE)*sizeof(img_struct));
  if (!img) error(502);
  //update existing img pointers to new locations
  display_page=&img[display_page_index];
  write_page=&img[write_page_index];
  read_page=&img[read_page_index];
  memset(&img[nimg],0,IMG_BUFFERSIZE*sizeof(img_struct));
  nimg+=IMG_BUFFERSIZE;
  i=nextimg++;
 gotindex:
  img[i].valid=1;
  return i;
}

int32 freeimg(uint32 i){
  //returns: 0=failed, 1=success
  if (i>=nimg) return 0;
  if (!img[i].valid) return 0;
  if (lastfimg>=(nfimg-1)){//extend
    fimg=(uint32*)realloc(fimg,(nfimg+IMG_BUFFERSIZE)*4);
    if (!fimg) error(503);
    nfimg+=IMG_BUFFERSIZE;
  }
  if (img[i].lock_id){
    free_mem_lock((mem_lock*)img[i].lock_offset);//untag
  }
  memset(&img[i],0,sizeof(img_struct));
  lastfimg++;
  fimg[lastfimg]=i;
  return 1;
}


void imgrevert(int32 i){
  static int32 bpp;
  static img_struct *im;

  im=&img[i];
  bpp=im->compatible_mode;

  //revert to assumed default values
  im->bytes_per_pixel=1;
  im->font=16;
  im->color=15;
  im->print_mode=3;
  im->background_color=0;
  im->draw_ta=0.0; im->draw_scale=1.0;

  //revert to mode's set values
  switch (bpp){
  case 0:
    im->bits_per_pixel=16; im->bytes_per_pixel=2;
    im->color=7;
    im->text=1;
    im->cursor_show=0; im->cursor_firstvalue=4; im->cursor_lastvalue=4;
    break;
  case 1:
    im->bits_per_pixel=2;
    im->font=8;
    im->color=3;
    break;
  case 2:
    im->bits_per_pixel=1; 
    im->font=8;//it gets stretched from 8 to 16 later
    im->color=1;
    break;
  case 7:
    im->bits_per_pixel=4;
    im->font=8;
    break;
  case 8:
    im->bits_per_pixel=4;
    im->font=8;
    break;
  case 9:
    im->bits_per_pixel=4;
    im->font=14;
    break;
  case 10:
    im->bits_per_pixel=2;
    im->font=14;
    im->color=3;
    break;
  case 11:
    im->bits_per_pixel=1;
    im->color=1;
    break;
  case 12:
    im->bits_per_pixel=4;
    break;
  case 13:
    im->bits_per_pixel=8;
    im->font=8;
    break;
  case 256:
    im->bits_per_pixel=8;
    break;
  case 32:
    im->bits_per_pixel=32; im->bytes_per_pixel=4;
    im->color=0xFFFFFFFF;
    im->background_color=0xFF000000;
    break;
  };
  im->draw_color=im->color;

  //revert palette
  if (bpp!=32){
    restorepalette(im);
    im->transparent_color=-1;
  }

  //revert calculatable values
  if (im->bits_per_pixel<32) im->mask=(1<<im->bits_per_pixel)-1; else im->mask=0xFFFFFFFF;
  //text
  im->cursor_x=1; im->cursor_y=1;
  im->top_row=1;
  if (bpp) im->bottom_row=(im->height/im->font); else im->bottom_row=im->height;
  im->bottom_row--; if (im->bottom_row<=0) im->bottom_row=1;
  if (!bpp) return;
  //graphics
  //clipping/scaling
  im->x=((double)im->width)/2.0; im->y=((double)im->height)/2.0;
  im->view_x2=im->width-1; im->view_y2=im->height-1;
  im->scaling_x=1; im->scaling_y=1;
  im->window_x2=im->view_x2; im->window_y2=im->view_y2;

  //clear
  if (bpp){//graphics
    memset(im->offset,0,im->width*im->height*im->bytes_per_pixel);
  }else{//text
    static int32 i2,i3;
    static uint16 *sp;
    i3=im->width*im->height; sp=(uint16*)im->offset; for (i2=0;i2<i3;i2++){*sp++=0x0720;}
  }

}//imgrevert

int32 imgframe(uint8 *o,int32 x,int32 y,int32 bpp){
  static int32 i;
  static img_struct *im;
  if (x<=0||y<=0) return 0;
  i=newimg();
  im=&img[i];
  im->offset=o;
  im->width=x; im->height=y;

  //assume default values
  im->bytes_per_pixel=1;
  im->font=16;
  im->color=15;
  im->compatible_mode=bpp;
  im->print_mode=3;
  im->draw_ta=0.0; im->draw_scale=1.0;

  //set values
  switch (bpp){
  case 0:
    im->bits_per_pixel=16; im->bytes_per_pixel=2;
    im->color=7;
    im->text=1;
    im->cursor_show=0; im->cursor_firstvalue=4; im->cursor_lastvalue=4;
    break;
  case 1:
    im->bits_per_pixel=2;
    im->font=8;
    im->color=3;
    break;
  case 2:
    im->bits_per_pixel=1; 
    im->font=8;//it gets stretched from 8 to 16 later
    im->color=1;
    break;
  case 7:
    im->bits_per_pixel=4;
    im->font=8;
    break;
  case 8:
    im->bits_per_pixel=4;
    im->font=8;
    break;
  case 9:
    im->bits_per_pixel=4;
    im->font=14;
    break;
  case 10:
    im->bits_per_pixel=2;
    im->font=14;
    im->color=3;
    break;
  case 11:
    im->bits_per_pixel=1;
    im->color=1;
    break;
  case 12:
    im->bits_per_pixel=4;
    break;
  case 13:
    im->bits_per_pixel=8;
    im->font=8;
    break;
  case 256:
    im->bits_per_pixel=8;
    break;
  case 32:
    im->bits_per_pixel=32; im->bytes_per_pixel=4;
    im->color=0xFFFFFFFF;
    im->background_color=0xFF000000;
    break;
  default:
    return 0;
  };
  im->draw_color=im->color;

  //attach palette
  if (bpp!=32){
    im->pal=(uint32*)calloc(256,4);
    if (!im->pal){
      freeimg(i);
      return 0;
    }
    im->flags|=IMG_FREEPAL;
    restorepalette(im);
    im->transparent_color=-1;
  }

  //set calculatable values
  if (im->bits_per_pixel<32) im->mask=(1<<im->bits_per_pixel)-1; else im->mask=0xFFFFFFFF;
  //text
  im->cursor_x=1; im->cursor_y=1;
  im->top_row=1;
  if (bpp) im->bottom_row=(im->height/im->font); else im->bottom_row=im->height;
  im->bottom_row--; if (im->bottom_row<=0) im->bottom_row=1;
  if (!bpp) return i;
  //graphics
  //clipping/scaling
  im->x=((double)im->width)/2.0; im->y=((double)im->height)/2.0;
  im->view_x2=im->width-1; im->view_y2=im->height-1;
  im->scaling_x=1; im->scaling_y=1;
  im->window_x2=im->view_x2; im->window_y2=im->view_y2;

  return i;
}

void sub__freeimage(int32 i,int32 passed);//forward ref

int32 imgnew(int32 x,int32 y,int32 bpp){
  static int32 i,i2,i3;
  static img_struct *im;
  static uint16 *sp;
  static uint32 *lp;
  i=imgframe(NULL,x,y,bpp);
  if (!i) return 0;
  im=&img[i];
  if (bpp){//graphics
    if (bpp==32){
      if (!cblend) init_blend();
      im->offset=(uint8*)calloc(x*y,4);
      if (!im->offset){sub__freeimage(-i,1); return 0;}
      //i3=x*y; lp=im->offset32; for (i2=0;i2<i3;i2++){*lp++=0xFF000000;}
    }else{
      im->offset=(uint8*)calloc(x*y*im->bytes_per_pixel,1);
      if (!im->offset){sub__freeimage(-i,1); return 0;}
    }
  }else{//text
    im->offset=(uint8*)malloc(x*y*im->bytes_per_pixel);
    if (!im->offset){sub__freeimage(-i,1); return 0;}
    i3=x*y; sp=(uint16*)im->offset; for (i2=0;i2<i3;i2++){*sp++=0x0720;}
  }
  im->flags|=IMG_FREEMEM;
  return i;
}

void sub__font(int32 f,int32 i,int32 passed);//foward def

void flush_old_hardware_commands(){
  static int32 old_command;
  static int32 command_to_remove;
  static hardware_graphics_command_struct* last_rendered_hgc;
  static hardware_graphics_command_struct* old_hgc;
  static hardware_graphics_command_struct* next_hgc;

  if (next_hardware_command_to_remove&&last_hardware_command_rendered){

    last_rendered_hgc=(hardware_graphics_command_struct*)list_get(hardware_graphics_command_handles,last_hardware_command_rendered);
 
    old_command=next_hardware_command_to_remove;
    old_hgc=(hardware_graphics_command_struct*)list_get(hardware_graphics_command_handles,old_command);

  remove_next_hgc:

    if (old_hgc->next_command==0) goto cant_remove;
    next_hgc=(hardware_graphics_command_struct*)list_get(hardware_graphics_command_handles,old_hgc->next_command);
    if (next_hgc->order>=last_rendered_hgc->order) goto cant_remove;


    command_to_remove=old_command;

    if (old_hgc->command==HARDWARE_GRAPHICS_COMMAND__FREEIMAGE_REQUEST){    
      static hardware_img_struct *himg;
      himg=(hardware_img_struct*)list_get(hardware_img_handles,old_hgc->src_img);
      //alert("HARDWARE_GRAPHICS_COMMAND__FREEIMAGE_REQUEST");
      //alert(old_hgc->src_img);
      //add command to free image
      //create new command handle & structure
      int32 hgch=list_add(hardware_graphics_command_handles);
      hardware_graphics_command_struct* hgc=(hardware_graphics_command_struct*)list_get(hardware_graphics_command_handles,hgch);
      hgc->remove=0;
      //set command values
      hgc->command=HARDWARE_GRAPHICS_COMMAND__FREEIMAGE;
      hgc->src_img=old_hgc->src_img;
      //queue the command
      hgc->next_command=0;
      hgc->order=display_frame_order_next;
      if (last_hardware_command_added){
    hardware_graphics_command_struct* hgc2=(hardware_graphics_command_struct*)list_get(hardware_graphics_command_handles,last_hardware_command_added);
    hgc2->next_command=hgch;
      }
      last_hardware_command_added=hgch;
      if (first_hardware_command==0) first_hardware_command=hgch;    
    }

    

    old_command=old_hgc->next_command;
    next_hardware_command_to_remove=old_command;
    old_hgc=(hardware_graphics_command_struct*)list_get(hardware_graphics_command_handles,old_command);
    list_remove(hardware_graphics_command_handles,command_to_remove);

    goto remove_next_hgc;

  cant_remove:;


  }//next_hardware_command_to_remove&&last_hardware_command_rendered
}//flush_old_hardware_commands


void sub__putimage(double f_dx1,double f_dy1,double f_dx2,double f_dy2,int32 src,int32 dst,double f_sx1,double f_sy1,double f_sx2,double f_sy2,int32 passed){

  /* 
     Format & passed bits: (needs updating)
     [[{STEP}](?,?)[-[{STEP}](?,?)]][,[?][,[?][,[[{STEP}](?,?)[-[{STEP}](?,?)]][,{_SMOOTH}]]]]
     2?     1              4?       8                                 512      128
  */

  static int32 w,h,sskip,dskip,x,y,xx,yy,z,x2,y2,dbpp,sbpp;
  static img_struct *s,*d;
  static uint32 *soff32,*doff32,col,clearcol,destcol;
  static uint8 *soff,*doff;
  static uint8 *cp;
  static int32 xdir,ydir,no_stretch,no_clip,no_reverse,flip,mirror;
  static double mx,my,fx,fy,fsx1,fsy1,fsx2,fsy2,dv,dv2;
  static int32 sx1,sy1,sx2,sy2,dx1,dy1,dx2,dy2;
  static int32 sw,sh,dw,dh;
  static uint32 *pal;
  static uint32 *ulp;

  no_stretch=0; no_clip=0; no_reverse=1;
  flip=0; mirror=0;

  static int32 use_hardware;
  static img_struct s_emu,d_emu;//used to emulate a source/dest image for calculation purposes
  use_hardware=0;

  //is source a hardware handle?
hardware_img_struct* dst_himg=NULL;
hardware_img_struct* src_himg=NULL;
  if (src){
    src_himg=(hardware_img_struct*)list_get(hardware_img_handles,src-HARDWARE_IMG_HANDLE_OFFSET);
    if (src_himg!=NULL){//source is hardware image
      src-=HARDWARE_IMG_HANDLE_OFFSET;

      flush_old_hardware_commands();

      s_emu.width=src_himg->w;
      s_emu.height=src_himg->h;
      s_emu.clipping_or_scaling=0;
      s_emu.alpha_disabled=src_himg->alpha_disabled;
      s=&s_emu;

      //check dst      
      if (dst<0){
    dst_himg=(hardware_img_struct*)list_get(hardware_img_handles,dst-HARDWARE_IMG_HANDLE_OFFSET);
    if (dst_himg==NULL){error(258); return;}
    dst-=HARDWARE_IMG_HANDLE_OFFSET;

    d_emu.width=dst_himg->w;
    d_emu.height=dst_himg->h;
    d_emu.clipping_or_scaling=0;
    d_emu.alpha_disabled=dst_himg->alpha_disabled;
    d=&d_emu;

      }else{
    if (dst>1) {error(5); return;}
    dst=-dst;

    d=display_page;//use parameters from display page

      }

      sbpp=4; dbpp=4;
      use_hardware=1;

      goto resolve_coordinates;

    }//source is hardware image
  }//src passed



  if (passed&8){//src
    //validate
    if (src>=0){
      validatepage(src); s=&img[page[src]];
    }else{
      src=-src;
      if (src>=nextimg){error(258); return;}
      s=&img[src];
      if (!s->valid){error(258); return;}
    }
  }else{
    s=read_page;
  }//src
  if (s->text){error(5); return;}
  sbpp=s->bytes_per_pixel;

  if (passed&32){//dst
    //validate
    if (dst>=0){
      validatepage(dst); d=&img[page[dst]];
    }else{
      dst=-dst;
      if (dst>=nextimg){error(258); return;}
      d=&img[dst];
      if (!d->valid){error(258); return;}
    }
  }else{
    d=write_page;
  }//dst
  if (d->text){error(5); return;}
  dbpp=d->bytes_per_pixel;
  if ((sbpp==4)&&(dbpp==1)){error(5); return;}
  if (s==d){error(5); return;}//cannot put source onto itself!


 resolve_coordinates:



  //quick references
  sw=s->width; sh=s->height; dw=d->width; dh=d->height;



  //change coordinates according to step
  if (passed&2){f_dx1=d->x+f_dx1; f_dy1=d->y+f_dy1;}
  if (passed&16){f_dx2=f_dx1+f_dx2; f_dy2=f_dy1+f_dy2;}
  if (passed&256){f_sx1=s->x+f_sx1; f_sy1=s->y+f_sy1;}
  if (passed&1024){f_sx2=f_sx1+f_sx2; f_sy2=f_sy1+f_sy2;}

  //Here we calculate what our final point is going to be and put that value into the _DEST x/y so we can get STEP back correctly on the next call.
  //or something like that...  I have no idea how to explain what the heck I'm gdoing here!
  //Basically I'm just trying to update the x/y point that we last plot to on our screen so we can pick it back up and use it again...
  if (passed&4){
          //we entered both dest numbers.  Our last point plotted should be f_dx2/f_dy2
          d->x=f_dx2; 
          d->y=f_dy2;
  }
  else{
          if (passed&1){
                  //we only sent it the first dest value.  We want to put our rectangle on a portion of the screen starting at this point
                  if (passed&512) {
                          //we have all the source values.  We want to put that rectangle over to dest starting at that point
                          d->x=f_dx1+abs(f_sx2-f_sx1); 
                      d->y=f_dy1+abs(f_sy2-f_sy1);
              }
              else{
                          //we want to go from f_sx1,F_sx2 to the edge of the screen and put it over to dest starting at that point
                      d->x=f_dx1+abs(sw-f_sx1); 
                      d->y=f_dy1+abs(sh-f_sy1);
              }
          }
          else{
                  //we never sent the first source value.  We want to put the image over the whole screen.
                  d->x=dw; 
                  d->y=dh;
          }
  }

  //And here we update our source page information so the STEP will work properly there as well.
  //This seems a little simpler logic

  if (passed&512){
          //we sent it the stop coordinate of where we're reading from
          s->x = f_sx2;
          s->y = f_sy2;
  }
  else{
          //we didn't and we need to have it copy from wherever the starting point is to the bottom right of the screen.
          //so our final point read will be the source width/height
          s->x = sw;
          s->y = sh;
  }


  //resolve coordinates
  if (passed&1){//dx1,dy1
    if (d->clipping_or_scaling){
      if (d->clipping_or_scaling==2){
    dx1=qbr_float_to_long(f_dx1*d->scaling_x+d->scaling_offset_x)+d->view_offset_x;
    dy1=qbr_float_to_long(f_dy1*d->scaling_y+d->scaling_offset_y)+d->view_offset_y;
      }else{
    dx1=qbr_float_to_long(f_dx1)+d->view_offset_x; dy1=qbr_float_to_long(f_dy1)+d->view_offset_y;
      }
    }else{
      dx1=qbr_float_to_long(f_dx1); dy1=qbr_float_to_long(f_dy1);
    }
    //note: dx2 & dy2 cannot be passed if dx1 & dy1 weren't passed
    if (passed&4){//dx2,dy2
      if (d->clipping_or_scaling){
    if (d->clipping_or_scaling==2){
      dx2=qbr_float_to_long(f_dx2*d->scaling_x+d->scaling_offset_x)+d->view_offset_x;
      dy2=qbr_float_to_long(f_dy2*d->scaling_y+d->scaling_offset_y)+d->view_offset_y;
    }else{
      dx2=qbr_float_to_long(f_dx2)+d->view_offset_x; dy2=qbr_float_to_long(f_dy2)+d->view_offset_y;
    }
      }else{
    dx2=qbr_float_to_long(f_dx2); dy2=qbr_float_to_long(f_dy2);
      }
    }else{//dx2,dy2
      dx2=0; dy2=0;
    }//dx2,dy2
  }else{//dx1,dy1
    dx1=0; dy1=0; dx2=0; dy2=0;
  }//dx1,dy1

  if (passed&64){//sx1,sy1
    if (s->clipping_or_scaling){

      if (s->clipping_or_scaling==2){
    sx1=qbr_float_to_long(f_sx1*s->scaling_x+s->scaling_offset_x)+s->view_offset_x;
    sy1=qbr_float_to_long(f_sy1*s->scaling_y+s->scaling_offset_y)+s->view_offset_y;
      }else{
    sx1=qbr_float_to_long(f_sx1)+s->view_offset_x; sy1=qbr_float_to_long(f_sy1)+s->view_offset_y;
      }
    }else{
      sx1=qbr_float_to_long(f_sx1); sy1=qbr_float_to_long(f_sy1);
    }
    //note: sx2 & sy2 cannot be passed if sx1 & sy1 weren't passed
    if (passed&512){//sx2,sy2
      if (s->clipping_or_scaling){
    if (s->clipping_or_scaling==2){
      sx2=qbr_float_to_long(f_sx2*s->scaling_x+s->scaling_offset_x)+s->view_offset_x;
      sy2=qbr_float_to_long(f_sy2*s->scaling_y+s->scaling_offset_y)+s->view_offset_y;
    }else{
      sx2=qbr_float_to_long(f_sx2)+s->view_offset_x; sy2=qbr_float_to_long(f_sy2)+s->view_offset_y;
    }
      }else{
    sx2=qbr_float_to_long(f_sx2); sy2=qbr_float_to_long(f_sy2);
      }
    }else{//sx2,sy2
      sx2=0; sy2=0;
    }//sx2,sy2
  }else{//sx1,sy1
    sx1=0; sy1=0; sx2=0; sy2=0;
  }//sx1,sy1

  //all co-ordinates resolved (but omitted co-ordinates are set to 0!)

  if (use_hardware){
    //calculate omitted co-ordinates
    if ((passed&4)&&(passed&512)) goto got_hw_coord;//all passed
    if (passed&4){//(dx1,dy1)-(dx2,dy2)...
      if (passed&64){//(dx1,dy1)-(dx2,dy2),...,(sx1,sy1)
    sx2=sx1+abs(dx2-dx1); sy2=sy1+abs(dy2-dy1);
    goto got_hw_coord;
      }else{//(dx1,dy1)-(dx2,dy2)
    sx2=sw-1; sy2=sh-1;
    goto got_hw_coord;
      }
    }
    if (passed&512){//...(sx1,sy1)-(sx2,sy2)
      if (passed&1){//(dx1,dy1),,(sx1,sy1)-(sx2,sy2)
    dx2=dx1+abs(sx2-sx1); dy2=dy1+abs(sy2-sy1);
    goto got_hw_coord;
      }else{//(sx1,sy1)-(sx2,sy2)
    dx2=dw-1; dy2=dh-1;
    goto got_hw_coord;
      }
    }
    if (passed&64){error(5); return;}//Invalid: NULL-NULL,?,?,(sx1,sy1)-NULL
    if (passed&1){//(dx1,dy1)
      sx2=s->width-1; sy2=s->height-1;
      dx2=dx1+sx2; dy2=dy1+sy2;
      goto got_hw_coord;
    }
    //no coords provided
    sx2=s->width-1; sy2=s->height-1;
    dx2=d->width-1; dy2=d->height-1;
  got_hw_coord:

    //create new command handle & structure
    int32 hgch=list_add(hardware_graphics_command_handles);
    hardware_graphics_command_struct* hgc=(hardware_graphics_command_struct*)list_get(hardware_graphics_command_handles,hgch);
 
    hgc->remove=0;
 
    //set command values
    hgc->command=HARDWARE_GRAPHICS_COMMAND__PUTIMAGE;
 
    hgc->src_img=src;
    hgc->src_x1=sx1;
    hgc->src_y1=sy1;
    hgc->src_x2=sx2;
    hgc->src_y2=sy2;

    hgc->dst_img=dst;
    hgc->dst_x1=dx1;
    hgc->dst_y1=dy1;
    hgc->dst_x2=dx2;
    hgc->dst_y2=dy2;

    hgc->smooth=0;//unless specified, no filtering will be applied
    if (passed&128) hgc->smooth=1;

    hgc->use_alpha=1;
    if (s->alpha_disabled) hgc->use_alpha=0;
    //only consider dest alpha setting if it is a hardware image    
    if (dst_himg){
        if (d->alpha_disabled) hgc->use_alpha=0;
    }

    //queue the command
    hgc->next_command=0;
    hgc->order=display_frame_order_next;

    if (last_hardware_command_added){
      hardware_graphics_command_struct* hgc2=(hardware_graphics_command_struct*)list_get(hardware_graphics_command_handles,last_hardware_command_added);
      hgc2->next_command=hgch;
    }
    last_hardware_command_added=hgch;
    if (first_hardware_command==0) first_hardware_command=hgch;

    return;

  }//use hardware

  //(decided not to throw error, QB64 will use linear filtering if/when available)
  //if (passed&128){error(5); return;}//software surfaces do not support pixel _SMOOTHing yet

  if ((passed&4)&&(passed&512)){//all co-ords given
    //could be stretched
    if ( (abs(dx2-dx1)==abs(sx2-sx1)) && (abs(dy2-dy1)==abs(sy2-sy1)) ){//non-stretched
      //could be flipped/reversed
      //could need clipping
      goto reverse;
    }
    goto stretch;
  }

  if (passed&4){//(dx1,dy1)-(dx2,dy2)...
    if (passed&64){//(dx1,dy1)-(dx2,dy2),...,(sx1,sy1)
      sx2=sx1+abs(dx2-dx1); sy2=sy1+abs(dy2-dy1);
      //can't be stretched
      //could be flipped/reversed
      //could need clipping
      goto reverse;
    }else{//(dx1,dy1)-(dx2,dy2)
      sx2=sw-1; sy2=sh-1;
      //could be stretched
      if ( ((abs(dx2-dx1)+1)==sw) && ((abs(dy2-dy1)+1)==sh) ){//non-stretched
    //could be flipped/reversed
    //could need clipping
    goto reverse;
      }
      goto stretch;
    }//16
  }//2

  if (passed&512){//...(sx1,sy1)-(sx2,sy2)
    if (passed&1){//(dx1,dy1),,(sx1,sy1)-(sx2,sy2)
      dx2=dx1+abs(sx2-sx1); dy2=dy1+abs(sy2-sy1);
      //can't be stretched
      //could be flipped/reversed
      //could need clipping
      goto reverse;
    }else{//(sx1,sy1)-(sx2,sy2)
      dx2=dw-1; dy2=dh-1;
      //could be stretched
      if ( ((abs(sx2-sx1)+1)==dw) && ((abs(sy2-sy1)+1)==dh) ){//non-stretched
    //could be flipped/reversed
    //could need clipping
    goto reverse;
      }
      goto stretch;
    }//1
  }//32

  if (passed&64){error(5); return;}//Invalid: NULL-NULL,?,?,(sx1,sy1)-NULL

  if (passed&1){//(dx1,dy1)
    sx2=s->width-1; sy2=s->height-1;
    dx2=dx1+sx2; dy2=dy1+sy2;
    goto clip;
  }

  //no co-ords given
  sx2=s->width-1; sy2=s->height-1;
  dx2=d->width-1; dy2=d->height-1;
  if ((sx2==dx2)&&(sy2==dy2)){//non-stretched
    //note: because 0-size image is illegal, no null size check is necessary
    goto noflip;//cannot be reversed
  }
  //precalculate required values
  w=dx2-dx1; h=dy2-dy1;
  fsx1=sx1; fsy1=sy1; fsx2=sx2; fsy2=sy2;
  //"pull" corners so all source pixels are evenly represented in dest rect
  if (fsx1<=fsx2){fsx1-=0.499999; fsx2+=0.499999;}else{fsx1+=0.499999; fsx2-=0.499999;}
  if (fsy1<=fsy2){fsy1-=0.499999; fsy2+=0.499999;}else{fsy1+=0.499999; fsy2-=0.499999;}
  //calc source gradients
  if (w) mx=(fsx2-fsx1)/((double)w); else mx=0.0;
  if (h) my=(fsy2-fsy1)/((double)h); else my=0.0;
  //note: mx & my represent the amount of change per dest pixel
  goto stretch_noreverse_noclip;

 stretch:
  //stretch is required

  //mirror?
  if (dx2<dx1){
    if (sx2>sx1) mirror=1;
  }
  if (sx2<sx1){
    if (dx2>dx1) mirror=1;
  }
  if (dx2<dx1){x=dx1; dx1=dx2; dx2=x;}
  if (sx2<sx1){x=sx1; sx1=sx2; sx2=x;}
  //flip?
  if (dy2<dy1){
    if (sy2>sy1) flip=1;
  }
  if (sy2<sy1){
    if (dy2>dy1) flip=1;
  }
  if (dy2<dy1){y=dy1; dy1=dy2; dy2=y;}
  if (sy2<sy1){y=sy1; sy1=sy2; sy2=y;}

  w=dx2-dx1; h=dy2-dy1;
  fsx1=sx1; fsy1=sy1; fsx2=sx2; fsy2=sy2;
  //"pull" corners so all source pixels are evenly represented in dest rect
  if (fsx1<=fsx2){fsx1-=0.499999; fsx2+=0.499999;}else{fsx1+=0.499999; fsx2-=0.499999;}
  if (fsy1<=fsy2){fsy1-=0.499999; fsy2+=0.499999;}else{fsy1+=0.499999; fsy2-=0.499999;}
  //calc source gradients
  if (w) mx=(fsx2-fsx1)/((double)w); else mx=0.0;
  if (h) my=(fsy2-fsy1)/((double)h); else my=0.0;
  //note: mx & my represent the amount of change per dest pixel

  //crop dest offscreen pixels
  if (dx1<0){
    if (mirror) fsx2+=((double)dx1)*mx; else fsx1-=((double)dx1)*mx;
    dx1=0;
  }
  if (dy1<0){
    if (flip) fsy2+=((double)dy1)*my; else fsy1-=((double)dy1)*my;
    dy1=0;
  }
  if (dx2>=dw){
    if (mirror) fsx1+=((double)(dx2-dw+1))*mx; else fsx2-=((double)(dx2-dw+1))*mx;
    dx2=dw-1;
  }
  if (dy2>=dh){
    if (flip) fsy1+=((double)(dy2-dh+1))*my; else fsy2-=((double)(dy2-dh+1))*my;
    dy2=dh-1;
  }
  //crop source offscreen pixels
  if (w){//gradient cannot be 0
    if (fsx1<-0.4999999){
      x=(-fsx1-0.499999)/mx+1.0;
      if (mirror) dx2-=x; else dx1+=x;
      fsx1+=((double)x)*mx;
    }
    if (fsx2>(((double)sw)-0.5000001)){
      x=(fsx2-(((double)sw)-0.500001))/mx+1.0;
      if (mirror) dx1+=x; else dx2-=x;
      fsx2-=(((double)x)*mx);
    }
  }//w
  if (h){//gradient cannot be 0
    if (fsy1<-0.4999999){
      y=(-fsy1-0.499999)/my+1.0;
      if (flip) dy2-=y; else dy1+=y;
      fsy1+=((double)y)*my;
    }
    if (fsy2>(((double)sh)-0.5000001)){
      y=(fsy2-(((double)sh)-0.500001))/my+1.0;
      if (flip) dy1+=y; else dy2-=y;
      fsy2-=(((double)y)*my);
    }
  }//h
  //<0-size/offscreen?
  //note: <0-size will cause reversal of dest
  //      offscreen values will result in reversal of dest
  if (dx1>dx2) return;
  if (dy1>dy2) return;
  //all values are now within the boundries of the source & dest

 stretch_noreverse_noclip:
  w=dx2-dx1+1; h=dy2-dy1+1;//recalculate based on actual number of pixels

  if (sbpp==4){
    if (s->alpha_disabled||d->alpha_disabled) goto put_32_noalpha_stretch;
    goto put_32_stretch;
  }
  if (dbpp==1){
    if (s->transparent_color==-1) goto put_8_stretch;
    goto put_8_clear_stretch;
  }
  if (s->transparent_color==-1) goto put_8_32_stretch;
  goto put_8_32_clear_stretch;

 put_32_stretch:
  //calc. starting points & change values
  if (flip){
    if (mirror){
      doff32=d->offset32+(dy2*dw+dx2);
      dskip=-dw+w;
    }else{
      doff32=d->offset32+(dy2*dw+dx1);
      dskip=-dw-w;
    }
  }else{
    if (mirror){
      doff32=d->offset32+(dy1*dw+dx2);
      dskip=dw+w;
    }else{
      doff32=d->offset32+(dy1*dw+dx1);
      dskip=dw-w;
    }
  }
  if (mirror) xdir=-1; else xdir=1;
  //plot rect
  yy=h;
  fy=fsy1;
  fsx1-=mx;//prev value is moved on from
  do{
    xx=w;
    ulp=s->offset32+sw*qbr_double_to_long(fy);
    fx=fsx1;
    do{
      //--------plot pixel--------
      switch((col=*(ulp+qbr_double_to_long(fx+=mx)))&0xFF000000){
      case 0xFF000000:
    *doff32=col;
    break;
      case 0x0:
    break;
      case 0x80000000:
    *doff32=(((*doff32&0xFEFEFE)+(col&0xFEFEFE))>>1)+(ablend128[*doff32>>24]<<24);
    break; 
      case 0x7F000000:
    *doff32=(((*doff32&0xFEFEFE)+(col&0xFEFEFE))>>1)+(ablend127[*doff32>>24]<<24);
    break;
      default:
    destcol=*doff32;
    cp=cblend+(col>>24<<16);
    *doff32=
      cp[(col<<8&0xFF00)+(destcol&255)    ]
      +(cp[(col&0xFF00)   +(destcol>>8&255) ]<<8)
      +(cp[(col>>8&0xFF00)+(destcol>>16&255)]<<16)
      +(ablend[(col>>24)+(destcol>>16&0xFF00)]<<24);
      };//switch
      //--------done plot pixel--------
      doff32+=xdir;
    }while(--xx);
    doff32+=dskip;
    fy+=my;
  }while(--yy);
  return;

 put_32_noalpha_stretch:
  //calc. starting points & change values
  if (flip){
    if (mirror){
      doff32=d->offset32+(dy2*dw+dx2);
      dskip=-dw+w;
    }else{
      doff32=d->offset32+(dy2*dw+dx1);
      dskip=-dw-w;
    }
  }else{
    if (mirror){
      doff32=d->offset32+(dy1*dw+dx2);
      dskip=dw+w;
    }else{
      doff32=d->offset32+(dy1*dw+dx1);
      dskip=dw-w;
    }
  }
  if (mirror) xdir=-1; else xdir=1;
  //plot rect
  yy=h;
  fy=fsy1;
  fsx1-=mx;//prev value is moved on from
  doff32-=xdir;
  do{
    xx=w;
    ulp=s->offset32+sw*qbr_double_to_long(fy);
    fx=fsx1;
    do{
      *(doff32+=xdir)=*(ulp+qbr_double_to_long(fx+=mx));
    }while(--xx);
    doff32+=dskip;
    fy+=my;
  }while(--yy);
  return;

 put_8_stretch:
  //calc. starting points & change values
  if (flip){
    if (mirror){
      doff=d->offset+(dy2*dw+dx2);
      dskip=-dw+w;
    }else{
      doff=d->offset+(dy2*dw+dx1);
      dskip=-dw-w;
    }
  }else{
    if (mirror){
      doff=d->offset+(dy1*dw+dx2);
      dskip=dw+w;
    }else{
      doff=d->offset+(dy1*dw+dx1);
      dskip=dw-w;
    }
  }
  if (mirror) xdir=-1; else xdir=1;
  //plot rect
  yy=h;
  fy=fsy1;
  fsx1-=mx;//prev value is moved on from
  doff-=xdir;
  do{
    xx=w;
    cp=s->offset+sw*qbr_double_to_long(fy);
    fx=fsx1;
    do{
      *(doff+=xdir)=*(cp+qbr_double_to_long(fx+=mx));
    }while(--xx);
    doff+=dskip;
    fy+=my;
  }while(--yy);
  return;

 put_8_clear_stretch:
  clearcol=s->transparent_color;
  //calc. starting points & change values
  if (flip){
    if (mirror){
      doff=d->offset+(dy2*dw+dx2);
      dskip=-dw+w;
    }else{
      doff=d->offset+(dy2*dw+dx1);
      dskip=-dw-w;
    }
  }else{
    if (mirror){
      doff=d->offset+(dy1*dw+dx2);
      dskip=dw+w;
    }else{
      doff=d->offset+(dy1*dw+dx1);
      dskip=dw-w;
    }
  }
  if (mirror) xdir=-1; else xdir=1;
  //plot rect
  yy=h;
  fy=fsy1;
  fsx1-=mx;//prev value is moved on from
  do{
    xx=w;
    cp=s->offset+sw*qbr_double_to_long(fy);
    fx=fsx1;
    do{
      if ((col=*(cp+qbr_double_to_long(fx+=mx)))!=clearcol){
    *doff=col;
      }
      doff+=xdir;
    }while(--xx);
    doff+=dskip;
    fy+=my;
  }while(--yy);
  return;

 put_8_32_stretch:
  pal=s->pal;
  //calc. starting points & change values
  if (flip){
    if (mirror){
      doff32=d->offset32+(dy2*dw+dx2);
      dskip=-dw+w;
    }else{
      doff32=d->offset32+(dy2*dw+dx1);
      dskip=-dw-w;
    }
  }else{
    if (mirror){
      doff32=d->offset32+(dy1*dw+dx2);
      dskip=dw+w;
    }else{
      doff32=d->offset32+(dy1*dw+dx1);
      dskip=dw-w;
    }
  }
  if (mirror) xdir=-1; else xdir=1;
  //plot rect
  yy=h;
  fy=fsy1;
  fsx1-=mx;//prev value is moved on from
  doff32-=xdir;
  do{
    xx=w;
    cp=s->offset+sw*qbr_double_to_long(fy);
    fx=fsx1;
    do{
      *(doff32+=xdir)=pal[*(cp+qbr_double_to_long(fx+=mx))];
    }while(--xx);
    doff32+=dskip;
    fy+=my;
  }while(--yy);
  return;

 put_8_32_clear_stretch:
  clearcol=s->transparent_color;
  pal=s->pal;
  //calc. starting points & change values
  if (flip){
    if (mirror){
      doff32=d->offset32+(dy2*dw+dx2);
      dskip=-dw+w;
    }else{
      doff32=d->offset32+(dy2*dw+dx1);
      dskip=-dw-w;
    }
  }else{
    if (mirror){
      doff32=d->offset32+(dy1*dw+dx2);
      dskip=dw+w;
    }else{
      doff32=d->offset32+(dy1*dw+dx1);
      dskip=dw-w;
    }
  }
  if (mirror) xdir=-1; else xdir=1;
  //plot rect
  yy=h;
  fy=fsy1;
  fsx1-=mx;//prev value is moved on from
  do{
    xx=w;
    cp=s->offset+sw*qbr_double_to_long(fy);
    fx=fsx1;
    do{
      if ((col=*(cp+qbr_double_to_long(fx+=mx)))!=clearcol){
    *doff32=pal[col];
      }
      doff32+=xdir;
    }while(--xx);
    doff32+=dskip;
    fy+=my;
  }while(--yy);
  return;

 reverse:
  //mirror?
  if (dx2<dx1){
    if (sx2>sx1) mirror=1;
  }
  if (sx2<sx1){
    if (dx2>dx1) mirror=1;
  }
  if (dx2<dx1){x=dx1; dx1=dx2; dx2=x;}
  if (sx2<sx1){x=sx1; sx1=sx2; sx2=x;}
  //flip?
  if (dy2<dy1){
    if (sy2>sy1) flip=1;
  }
  if (sy2<sy1){
    if (dy2>dy1) flip=1;
  }
  if (dy2<dy1){y=dy1; dy1=dy2; dy2=y;}
  if (sy2<sy1){y=sy1; sy1=sy2; sy2=y;}

 clip:
  //crop dest offscreen pixels
  if (dx1<0){
    if (mirror) sx2+=dx1; else sx1-=dx1;
    dx1=0;
  }
  if (dy1<0){
    if (flip) sy2+=dy1; else sy1-=dy1;
    dy1=0;
  }
  if (dx2>=dw){
    if (mirror) sx1+=(dx2-dw+1); else sx2-=(dx2-dw+1);
    dx2=dw-1;
  }
  if (dy2>=dh){
    if (flip) sy1+=(dy2-dh+1); else sy2-=(dy2-dh+1);
    dy2=dh-1;
  }
  //crop source offscreen pixels
  if (sx1<0){
    if (mirror) dx2+=sx1; else dx1-=sx1;
    sx1=0;
  }
  if (sy1<0){
    if (flip) dy2+=sy1; else dy1-=sy1;
    sy1=0;
  }
  if (sx2>=sw){
    if (mirror) dx1+=(sx2-sw+1); else dx2-=(sx2-sw+1);
    sx2=sw-1;
  }
  if (sy2>=sh){
    if (flip) dy1+=(sy2-sh+1); else dy2-=(sy2-sh+1);
    sy2=sh-1;
  }
  //<0-size/offscreen?
  //note: <0-size will cause reversal of dest
  //      offscreen values will result in reversal of dest
  if (dx1>dx2) return;
  if (dy1>dy2) return;
  //all values are now within the boundries of the source & dest

  //mirror put
  if (mirror){
    if (sbpp==4){
      if (s->alpha_disabled||d->alpha_disabled) goto put_32_noalpha_mirror;
      goto put_32_mirror;
    }
    if (dbpp==1){
      if (s->transparent_color==-1) goto put_8_mirror;
      goto put_8_clear_mirror;
    }
    if (s->transparent_color==-1) goto put_8_32_mirror;
    goto put_8_32_clear_mirror;
  }//mirror put

 noflip:
  if (sbpp==4){
    if (s->alpha_disabled||d->alpha_disabled) goto put_32_noalpha;
    goto put_32;
  }
  if (dbpp==1){
    if (s->transparent_color==-1) goto put_8;
    goto put_8_clear;
  }
  if (s->transparent_color==-1) goto put_8_32;
  goto put_8_32_clear;

 put_32:
  w=dx2-dx1+1;
  doff32=d->offset32+(dy1*dw+dx1);
  dskip=dw-w;
  if (flip){
    soff32=s->offset32+(sy2*sw+sx1);
    sskip=-w-sw;
  }else{
    soff32=s->offset32+(sy1*sw+sx1);
    sskip=sw-w;
  }
  //plot rect
  h=dy2-dy1+1;
  do{
    xx=w;
    do{
      //--------plot pixel--------
      switch((col=*soff32++)&0xFF000000){
      case 0xFF000000:
    *doff32++=col;
    break;
      case 0x0:
    doff32++;
    break;
      case 0x80000000:
    *doff32++=(((*doff32&0xFEFEFE)+(col&0xFEFEFE))>>1)+(ablend128[*doff32>>24]<<24);
    break; 
      case 0x7F000000:
    *doff32++=(((*doff32&0xFEFEFE)+(col&0xFEFEFE))>>1)+(ablend127[*doff32>>24]<<24);
    break;
      default:
    destcol=*doff32;
    cp=cblend+(col>>24<<16);
    *doff32++=
      cp[(col<<8&0xFF00)+(destcol&255)    ]
      +(cp[(col&0xFF00)   +(destcol>>8&255) ]<<8)
      +(cp[(col>>8&0xFF00)+(destcol>>16&255)]<<16)
      +(ablend[(col>>24)+(destcol>>16&0xFF00)]<<24);
      };//switch
      //--------done plot pixel--------
    }while(--xx);
    soff32+=sskip; doff32+=dskip;
  }while(--h);
  return;

 put_32_noalpha:
  doff32=d->offset32+(dy1*dw+dx1);
  if (flip){
    soff32=s->offset32+(sy2*sw+sx1);
    sskip=-sw;
  }else{
    soff32=s->offset32+(sy1*sw+sx1);
    sskip=sw;
  }
  h=dy2-dy1+1;
  w=(dx2-dx1+1)*4;
  while(h--){
    memcpy(doff32,soff32,w);
    soff32+=sskip; doff32+=dw;
  }
  return;

 put_8:
  doff=d->offset+(dy1*dw+dx1);
  if (flip){
    soff=s->offset+(sy2*sw+sx1);
    sskip=-sw;
  }else{
    soff=s->offset+(sy1*sw+sx1);
    sskip=sw;
  }
  h=dy2-dy1+1;
  w=dx2-dx1+1;
  while(h--){
    memcpy(doff,soff,w);
    soff+=sskip; doff+=dw;
  }
  return;

 put_8_clear:
  clearcol=s->transparent_color;
  w=dx2-dx1+1;
  doff=d->offset+(dy1*dw+dx1);
  dskip=dw-w;
  if (flip){
    soff=s->offset+(sy2*sw+sx1);
    sskip=-w-sw;
  }else{
    soff=s->offset+(sy1*sw+sx1);
    sskip=sw-w;
  }
  //plot rect
  h=dy2-dy1+1;
  do{
    xx=w;
    do{
      if ((col=*soff++)!=clearcol){
    *doff=col;
      }
      doff++;
    }while(--xx);
    soff+=sskip; doff+=dskip;
  }while(--h);
  return;

 put_8_32:
  pal=s->pal;
  w=dx2-dx1+1;
  doff32=d->offset32+(dy1*dw+dx1);
  dskip=dw-w;
  if (flip){
    soff=s->offset+(sy2*sw+sx1);
    sskip=-w-sw;
  }else{
    soff=s->offset+(sy1*sw+sx1);
    sskip=sw-w;
  }
  //plot rect
  h=dy2-dy1+1;
  do{
    xx=w;
    do{
      *doff32++=pal[*soff++];
    }while(--xx);
    soff+=sskip; doff32+=dskip;
  }while(--h);
  return;

 put_8_32_clear:
  pal=s->pal;
  clearcol=s->transparent_color;
  w=dx2-dx1+1;
  doff32=d->offset32+(dy1*dw+dx1);
  dskip=dw-w;
  if (flip){
    soff=s->offset+(sy2*sw+sx1);
    sskip=-w-sw;
  }else{
    soff=s->offset+(sy1*sw+sx1);
    sskip=sw-w;
  }
  //plot rect
  h=dy2-dy1+1;
  do{
    xx=w;
    do{
      if ((col=*soff++)!=clearcol){
    *doff32=pal[col];
      }
      doff32++;
    }while(--xx);
    soff+=sskip; doff32+=dskip;
  }while(--h);
  return;

 put_32_mirror:
  w=dx2-dx1+1;
  doff32=d->offset32+(dy1*dw+dx1);
  dskip=dw-w;
  if (flip){
    soff32=s->offset32+(sy2*sw+sx2);
    sskip=-sw+w;
  }else{
    soff32=s->offset32+(sy1*sw+sx2);
    sskip=w+sw;
  }
  //plot rect
  h=dy2-dy1+1;
  do{
    xx=w;
    do{
      //--------plot pixel--------
      switch((col=*soff32--)&0xFF000000){
      case 0xFF000000:
    *doff32++=col;
    break;
      case 0x0:
    doff32++;
    break;
      case 0x80000000:
    *doff32++=(((*doff32&0xFEFEFE)+(col&0xFEFEFE))>>1)+(ablend128[*doff32>>24]<<24);
    break; 
      case 0x7F000000:
    *doff32++=(((*doff32&0xFEFEFE)+(col&0xFEFEFE))>>1)+(ablend127[*doff32>>24]<<24);
    break;
      default:
    destcol=*doff32;
    cp=cblend+(col>>24<<16);
    *doff32++=
      cp[(col<<8&0xFF00)+(destcol&255)    ]
      +(cp[(col&0xFF00)   +(destcol>>8&255) ]<<8)
      +(cp[(col>>8&0xFF00)+(destcol>>16&255)]<<16)
      +(ablend[(col>>24)+(destcol>>16&0xFF00)]<<24);
      };//switch
      //--------done plot pixel--------
    }while(--xx);
    soff32+=sskip; doff32+=dskip;
  }while(--h);
  return;

 put_32_noalpha_mirror:
  w=dx2-dx1+1;
  doff32=d->offset32+(dy1*dw+dx1);
  dskip=dw-w;
  if (flip){
    soff32=s->offset32+(sy2*sw+sx2);
    sskip=-sw+w;
  }else{
    soff32=s->offset32+(sy1*sw+sx2);
    sskip=w+sw;
  }
  //plot rect
  h=dy2-dy1+1;
  do{
    xx=w;
    do{
      *doff32++=*soff32--;
    }while(--xx);
    soff32+=sskip; doff32+=dskip;
  }while(--h);
  return;

 put_8_mirror:
  w=dx2-dx1+1;
  doff=d->offset+(dy1*dw+dx1);
  dskip=dw-w;
  if (flip){
    soff=s->offset+(sy2*sw+sx2);
    sskip=-sw+w;
  }else{
    soff=s->offset+(sy1*sw+sx2);
    sskip=w+sw;
  }
  //plot rect
  h=dy2-dy1+1;
  do{
    xx=w;
    do{
      *doff++=*soff--;
    }while(--xx);
    soff+=sskip; doff+=dskip;
  }while(--h);
  return;

 put_8_clear_mirror:
  clearcol=s->transparent_color;
  w=dx2-dx1+1;
  doff=d->offset+(dy1*dw+dx1);
  dskip=dw-w;
  if (flip){
    soff=s->offset+(sy2*sw+sx2);
    sskip=-sw+w;
  }else{
    soff=s->offset+(sy1*sw+sx2);
    sskip=w+sw;
  }
  //plot rect
  h=dy2-dy1+1;
  do{
    xx=w;
    do{
      if ((col=*soff--)!=clearcol){
    *doff=col;
      }
      doff++;
    }while(--xx);
    soff+=sskip; doff+=dskip;
  }while(--h);
  return;

 put_8_32_mirror:
  pal=s->pal;
  w=dx2-dx1+1;
  doff32=d->offset32+(dy1*dw+dx1);
  dskip=dw-w;
  if (flip){
    soff=s->offset+(sy2*sw+sx2);
    sskip=-sw+w;
  }else{
    soff=s->offset+(sy1*sw+sx2);
    sskip=w+sw;
  }
  //plot rect
  h=dy2-dy1+1;
  do{
    xx=w;
    do{
      *doff32++=pal[*soff--];
    }while(--xx);
    soff+=sskip; doff32+=dskip;
  }while(--h);
  return;

 put_8_32_clear_mirror:
  pal=s->pal;
  clearcol=s->transparent_color;
  w=dx2-dx1+1;
  doff32=d->offset32+(dy1*dw+dx1);
  dskip=dw-w;
  if (flip){
    soff=s->offset+(sy2*sw+sx2);
    sskip=-sw+w;
  }else{
    soff=s->offset+(sy1*sw+sx2);
    sskip=w+sw;
  }
  //plot rect
  h=dy2-dy1+1;
  do{
    xx=w;
    do{
      if ((col=*soff--)!=clearcol){
    *doff32=pal[col];
      }
      doff32++;
    }while(--xx);
    soff+=sskip; doff32+=dskip;
  }while(--h);
  return;

}//_putimage


int32 selectfont(int32 f,img_struct *im){
  im->font=f;
  im->cursor_x=1; im->cursor_y=1;
  im->top_row=1;
  if (im->compatible_mode) im->bottom_row=im->height/fontheight[f]; else im->bottom_row=im->height;
  im->bottom_row--; if (im->bottom_row<=0) im->bottom_row=1;
  return 1;//success
}


int32 nmodes=0;
int32 anymode=0;

float x_scale=1,y_scale=1;
int32 x_offset=0,y_offset=0;
int32 x_limit=0,y_limit=0;



int32 x_monitor=0,y_monitor=0;


int32 conversion_required=0;
uint32 *conversion_layer=(uint32*)malloc(8);

#define AUDIO_CHANNELS 256

#define sndqueue_lastindex 9999
uint32 sndqueue[sndqueue_lastindex+1];
int32 sndqueue_next=0;
int32 sndqueue_first=0;
int32 sndqueue_wait=-1;
int32 sndqueue_played=0;


void call_int(int32 i);

uint32 frame=0;


extern uint8 cmem[1114099];//16*65535+65535+3 (enough for highest referencable dword in conv memory)


int32 mouse_hideshow_called=0;

struct mouse_message{
  int16 x;
  int16 y;
  uint32 buttons;
  int16 movementx;
  int16 movementy;
};

/*
mouse_message mouse_messages[65536];//a circular buffer of mouse messages
int32 last_mouse_message=0;
int32 current_mouse_message=0;
*/

//Mouse message queue system
//--------------------------
struct mouse_message_queue_struct{
        mouse_message *queue;
        int32 lastIndex;
        int32 current;
        int32 first;
        int32 last;
        int32 child;
        int32 parent;
};
list *mouse_message_queue_handles=NULL;
int32 mouse_message_queue_first; //the first queue to populate from input source
int32 mouse_message_queue_default;//the default queue (for int33h and default _MOUSEINPUT operations)






//x86 Virtual CMEM emulation
//Note: x86 CPU emulation is still experimental and is not available in QB64 yet.
struct cpu_struct{
  //al,ah,ax,eax (unsigned & signed)
  union{
    struct{
      union{
    uint8 al;
    int8 al_signed;
      };
      union{
    uint8 ah;
    int8 ah_signed;
      };
    };
    uint16 ax;
    int16 ax_signed;
    uint32 eax;
    int32 eax_signed;
  };
  //bl,bh,bx,ebx (unsigned & signed)
  union{
    struct{
      union{
    uint8 bl;
    int8 bl_signed;
      };
      union{
    uint8 bh;
    int8 bh_signed;
      };
    };
    uint16 bx;
    int16 bx_signed;
    uint32 ebx;
    int32 ebx_signed;
  };
  //cl,ch,cx,ecx (unsigned & signed)
  union{
    struct{
      union{
    uint8 cl;
    int8 cl_signed;
      };
      union{
    uint8 ch;
    int8 ch_signed;
      };
    };
    uint16 cx;
    int16 cx_signed;
    uint32 ecx;
    int32 ecx_signed;
  };
  //dl,dh,dx,edx (unsigned & signed)
  union{
    struct{
      union{
    uint8 dl;
    int8 dl_signed;
      };
      union{
    uint8 dh;
    int8 dh_signed;
      };
    };
    uint16 dx;
    int16 dx_signed;
    uint32 edx;
    int32 edx_signed;
  };
  //si,esi (unsigned & signed)
  union{
    uint16 si;
    int16 si_signed;
    uint32 esi;
    int32 esi_signed;
  };
  //di,edi (unsigned & signed)
  union{
    uint16 di;
    int16 di_signed;
    uint32 edi;
    int32 edi_signed;
  };
  //bp,ebp (unsigned & signed)
  union{
    uint16 bp;
    int16 bp_signed;
    uint32 ebp;
    int32 ebp_signed;
  };
  //sp,esp (unsigned & signed)
  union{
    uint16 sp;
    int16 sp_signed;
    uint32 esp;
    int32 esp_signed;
  };
  //cs,ss,ds,es,fs,gs (unsigned & signed)
  union{
    uint16 cs;
    uint16 cs_signed;
  };
  union{
    uint16 ss;
    uint16 ss_signed;
  };
  union{
    uint16 ds;
    uint16 ds_signed;
  };
  union{
    uint16 es;
    uint16 es_signed;
  };
  union{
    uint16 fs;
    uint16 fs_signed;
  };
  union{
    uint16 gs;
    uint16 gs_signed;
  };
  //ip,eip (unsigned & signed)
  union{
    uint16 ip;
    uint16 ip_signed;
    uint32 eip;
    uint32 eip_signed;
  };
  //flags
  uint8 overflow_flag;
  uint8 direction_flag;
  uint8 interrupt_flag;
  uint8 trap_flag;
  uint8 sign_flag;
  uint8 zero_flag;
  uint8 auxiliary_flag;
  uint8 parity_flag;
  uint8 carry_flag;
};
cpu_struct cpu;

uint8 *ip;
uint8 *seg;//default segment (DS unless overridden)
uint8 *seg_bp;//the segment bp will be accessed from (SS unless overridden)

uint8 *reg8[8];
uint16 *reg16[8];
uint32 *reg32[8];
uint16 *segreg[8];

int32 a32;
int32 b32;//size of data to read/write in bits is 32


uint32 sib(){
  static uint32 i;//sib byte
  i=*ip++;
  switch(i>>6){
  case 0:
    return *reg32[i&7]+*reg32[i>>3&7];
    break;
  case 1:
    return *reg32[i&7]+(*reg32[i>>3&7]<<1);
    break;
  case 2:
    return *reg32[i&7]+(*reg32[i>>3&7]<<2);
    break;
  case 3:
    return *reg32[i&7]+(*reg32[i>>3&7]<<3);
    break;
  }
}

uint32 sib_mod0(){
  //Note: Called when top 2 bits of rm byte before sib byte were 0, base register is ignored
  //      and replaced with an int32 following the sib byte
  static uint32 i;//sib byte
  i=*ip++;
  if ((i&7)==5){
    switch(i>>6){
    case 0:
      return (*(uint32*)((ip+=4)-4))+*reg32[i>>3&7];
      break;
    case 1:
      return (*(uint32*)((ip+=4)-4))+(*reg32[i>>3&7]<<1);
      break;
    case 2:
      return (*(uint32*)((ip+=4)-4))+(*reg32[i>>3&7]<<2);
      break;
    case 3:
      return (*(uint32*)((ip+=4)-4))+(*reg32[i>>3&7]<<3);
      break;
    }
  }
  switch(i>>6){
  case 0:
    return *reg32[i&7]+*reg32[i>>3&7];
    break;
  case 1:
    return *reg32[i&7]+(*reg32[i>>3&7]<<1);
    break;
  case 2:
    return *reg32[i&7]+(*reg32[i>>3&7]<<2);
    break;
  case 3:
    return *reg32[i&7]+(*reg32[i>>3&7]<<3);
    break;
  }
}

uint8 *rm8(){
  static uint32 i;//r/m byte
  i=*ip++;
  switch(i>>6){
  case 3:
    return reg8[i&7];
    break;
  case 0:
    if (a32){
      switch(i&7){
      case 0: return seg+cpu.ax; break;
      case 1: return seg+cpu.cx; break;
      case 2: return seg+cpu.dx; break;
      case 3: return seg+cpu.bx; break;
      case 4: return seg+(uint16)sib_mod0(); break;
      case 5: return seg+(*(uint16*)((ip+=4)-4)); break;
      case 6: return seg+cpu.si; break;
      case 7: return seg+cpu.di; break;
      }
    }else{
      switch(i&7){
      case 0: return seg+((uint16)(cpu.bx+cpu.si)); break;
      case 1: return seg+((uint16)(cpu.bx+cpu.di)); break;
      case 2: return seg_bp+((uint16)(cpu.bp+cpu.si)); break;
      case 3: return seg_bp+((uint16)(cpu.bp+cpu.di)); break;
      case 4: return seg+cpu.si; break;
      case 5: return seg+cpu.di; break;
      case 6: return seg+(*(uint16*)((ip+=2)-2)); break;
      case 7: return seg+cpu.bx; break;
      }
    }
    break;
  case 1:
    if (a32){
      switch(i&7){
      case 0: return seg+((uint16)(cpu.eax+*(int8*)ip++)); break;
      case 1: return seg+((uint16)(cpu.ecx+*(int8*)ip++)); break;
      case 2: return seg+((uint16)(cpu.edx+*(int8*)ip++)); break;
      case 3: return seg+((uint16)(cpu.ebx+*(int8*)ip++)); break;
      case 4: i=sib(); return seg+((uint16)(i+*(int8*)ip++)); break;
      case 5: return seg_bp+((uint16)(cpu.ebp+*(int8*)ip++)); break;
      case 6: return seg+((uint16)(cpu.esi+*(int8*)ip++)); break;
      case 7: return seg+((uint16)(cpu.edi+*(int8*)ip++)); break;
      }
    }else{
      switch(i&7){
      case 0: return seg+((uint16)(cpu.bx+cpu.si+*(int8*)ip++)); break;
      case 1: return seg+((uint16)(cpu.bx+cpu.di+*(int8*)ip++)); break;
      case 2: return seg_bp+((uint16)(cpu.bp+cpu.si+*(int8*)ip++)); break;
      case 3: return seg_bp+((uint16)(cpu.bp+cpu.di+*(int8*)ip++)); break;
      case 4: return seg+((uint16)(cpu.si+*(int8*)ip++)); break;
      case 5: return seg+((uint16)(cpu.di+*(int8*)ip++)); break;
      case 6: return seg_bp+((uint16)(cpu.bp+*(int8*)ip++)); break;
      case 7: return seg+((uint16)(cpu.bx+*(int8*)ip++)); break;
      }
    }
    break;
  case 2:
    if (a32){ 
      switch(i&7){
      case 0: return seg+((uint16)(cpu.eax+*(uint32*)((ip+=4)-4))); break;
      case 1: return seg+((uint16)(cpu.ecx+*(uint32*)((ip+=4)-4))); break;
      case 2: return seg+((uint16)(cpu.edx+*(uint32*)((ip+=4)-4))); break;
      case 3: return seg+((uint16)(cpu.ebx+*(uint32*)((ip+=4)-4))); break;
      case 4: i=sib(); return seg+((uint16)(i+*(uint32*)((ip+=4)-4))); break;
      case 5: return seg_bp+((uint16)(cpu.ebp+*(uint32*)((ip+=4)-4))); break;
      case 6: return seg+((uint16)(cpu.esi+*(uint32*)((ip+=4)-4))); break;
      case 7: return seg+((uint16)(cpu.edi+*(uint32*)((ip+=4)-4))); break;
      }
    }else{
      switch(i&7){
      case 0: return seg+((uint16)(cpu.bx+cpu.si+*(uint16*)((ip+=2)-2))); break;
      case 1: return seg+((uint16)(cpu.bx+cpu.di+*(uint16*)((ip+=2)-2))); break;
      case 2: return seg_bp+((uint16)(cpu.bp+cpu.si+*(uint16*)((ip+=2)-2))); break;
      case 3: return seg_bp+((uint16)(cpu.bp+cpu.di+*(uint16*)((ip+=2)-2))); break;
      case 4: return seg+((uint16)(cpu.si+*(uint16*)((ip+=2)-2))); break;
      case 5: return seg+((uint16)(cpu.di+*(uint16*)((ip+=2)-2))); break;
      case 6: return seg_bp+((uint16)(cpu.bp+*(uint16*)((ip+=2)-2))); break;
      case 7: return seg+((uint16)(cpu.bx+*(uint16*)((ip+=2)-2))); break;
      }
    }
    break;
  }
}

uint16 *rm16(){
  static int32 i;//r/m byte
  i=*ip;
  switch(i>>6){
  case 3:
    ip++; 
    return reg16[i&7];
    break;
  case 0:
    ip++;
    if (a32){
      switch(i&7){
      case 0: return (uint16*)(seg+cpu.ax); break;
      case 1: return (uint16*)(seg+cpu.cx); break;
      case 2: return (uint16*)(seg+cpu.dx); break;
      case 3: return (uint16*)(seg+cpu.bx); break;   
      case 4: return (uint16*)(seg+(uint16)sib_mod0()); break;
      case 5: return (uint16*)(seg+(*(uint16*)((ip+=4)-4))); break;
      case 6: return (uint16*)(seg+cpu.si); break;
      case 7: return (uint16*)(seg+cpu.di); break;
      }
    }else{
      switch(i&7){
      case 0: return (uint16*)(seg+((uint16)(cpu.bx+cpu.si))); break;
      case 1: return (uint16*)(seg+((uint16)(cpu.bx+cpu.di))); break;
      case 2: return (uint16*)(seg_bp+((uint16)(cpu.bp+cpu.si))); break;
      case 3: return (uint16*)(seg_bp+((uint16)(cpu.bp+cpu.di))); break;
      case 4: return (uint16*)(seg+cpu.si); break;
      case 5: return (uint16*)(seg+cpu.di); break;
      case 6: return (uint16*)(seg+(*(uint16*)((ip+=2)-2))); break;
      case 7: return (uint16*)(seg+cpu.bx); break;
      }
    }
    break;
  case 1:
    ip++;
    if (a32){ 
      switch(i&7){
      case 0: return (uint16*)(seg+((uint16)(cpu.eax+*(int8*)ip++))); break;
      case 1: return (uint16*)(seg+((uint16)(cpu.ecx+*(int8*)ip++))); break;
      case 2: return (uint16*)(seg+((uint16)(cpu.edx+*(int8*)ip++))); break;
      case 3: return (uint16*)(seg+((uint16)(cpu.ebx+*(int8*)ip++))); break;
      case 4: i=sib(); return (uint16*)(seg+((uint16)(i+*(int8*)ip++))); break;
      case 5: return (uint16*)(seg_bp+((uint16)(cpu.ebp+*(int8*)ip++))); break;
      case 6: return (uint16*)(seg+((uint16)(cpu.esi+*(int8*)ip++))); break;
      case 7: return (uint16*)(seg+((uint16)(cpu.edi+*(int8*)ip++))); break;
      }
    }else{
      switch(i&7){
      case 0: return (uint16*)(seg+((uint16)(cpu.bx+cpu.si+*(int8*)ip++))); break;
      case 1: return (uint16*)(seg+((uint16)(cpu.bx+cpu.di+*(int8*)ip++))); break;
      case 2: return (uint16*)(seg_bp+((uint16)(cpu.bp+cpu.si+*(int8*)ip++))); break;
      case 3: return (uint16*)(seg_bp+((uint16)(cpu.bp+cpu.di+*(int8*)ip++))); break;
      case 4: return (uint16*)(seg+((uint16)(cpu.si+*(int8*)ip++))); break;
      case 5: return (uint16*)(seg+((uint16)(cpu.di+*(int8*)ip++))); break;
      case 6: return (uint16*)(seg_bp+((uint16)(cpu.bp+*(int8*)ip++))); break;
      case 7: return (uint16*)(seg+((uint16)(cpu.bx+*(int8*)ip++))); break;
      }
    }
    break;
  case 2:
    ip++;
    if (a32){ 
      switch(i&7){
      case 0: return (uint16*)(seg+((uint16)(cpu.eax+*(uint32*)((ip+=4)-4)))); break;
      case 1: return (uint16*)(seg+((uint16)(cpu.ecx+*(uint32*)((ip+=4)-4)))); break;
      case 2: return (uint16*)(seg+((uint16)(cpu.edx+*(uint32*)((ip+=4)-4)))); break;
      case 3: return (uint16*)(seg+((uint16)(cpu.ebx+*(uint32*)((ip+=4)-4)))); break;
      case 4: i=sib(); return (uint16*)(seg+((uint16)(i+*(uint32*)((ip+=4)-4)))); break;
      case 5: return (uint16*)(seg_bp+((uint16)(cpu.ebp+*(uint32*)((ip+=4)-4)))); break;
      case 6: return (uint16*)(seg+((uint16)(cpu.esi+*(uint32*)((ip+=4)-4)))); break;
      case 7: return (uint16*)(seg+((uint16)(cpu.edi+*(uint32*)((ip+=4)-4)))); break;
      }
    }else{
      switch(i&7){
      case 0: return (uint16*)(seg+((uint16)(cpu.bx+cpu.si+*(uint16*)((ip+=2)-2)))); break;
      case 1: return (uint16*)(seg+((uint16)(cpu.bx+cpu.di+*(uint16*)((ip+=2)-2)))); break;
      case 2: return (uint16*)(seg_bp+((uint16)(cpu.bp+cpu.si+*(uint16*)((ip+=2)-2)))); break;
      case 3: return (uint16*)(seg_bp+((uint16)(cpu.bp+cpu.di+*(uint16*)((ip+=2)-2)))); break;
      case 4: return (uint16*)(seg+((uint16)(cpu.si+*(uint16*)((ip+=2)-2)))); break;
      case 5: return (uint16*)(seg+((uint16)(cpu.di+*(uint16*)((ip+=2)-2)))); break;
      case 6: return (uint16*)(seg_bp+((uint16)(cpu.bp+*(uint16*)((ip+=2)-2)))); break;
      case 7: return (uint16*)(seg+((uint16)(cpu.bx+*(uint16*)((ip+=2)-2)))); break;
      }
    }
    break;
  }
}

uint32 *rm32(){
  static int32 i;//r/m byte
  i=*ip;
  switch(i>>6){
  case 3:
    ip++; 
    return reg32[i&7];
    break;
  case 0:
    ip++;
    if (a32){
      switch(i&7){
      case 0: return (uint32*)(seg+cpu.ax); break;
      case 1: return (uint32*)(seg+cpu.cx); break;
      case 2: return (uint32*)(seg+cpu.dx); break;
      case 3: return (uint32*)(seg+cpu.bx); break;
      case 4: return (uint32*)(seg+(uint16)sib_mod0()); break;
      case 5: return (uint32*)(seg+(*(uint16*)((ip+=4)-4))); break;
      case 6: return (uint32*)(seg+cpu.si); break;
      case 7: return (uint32*)(seg+cpu.di); break;
      }
    }else{
      switch(i&7){
      case 0: return (uint32*)(seg+((uint16)(cpu.bx+cpu.si))); break;
      case 1: return (uint32*)(seg+((uint16)(cpu.bx+cpu.di))); break;
      case 2: return (uint32*)(seg_bp+((uint16)(cpu.bp+cpu.si))); break;
      case 3: return (uint32*)(seg_bp+((uint16)(cpu.bp+cpu.di))); break;
      case 4: return (uint32*)(seg+cpu.si); break;
      case 5: return (uint32*)(seg+cpu.di); break;
      case 6: return (uint32*)(seg+(*(uint16*)((ip+=2)-2))); break;
      case 7: return (uint32*)(seg+cpu.bx); break;
      }
    }
    break;
  case 1:
    ip++;
    if (a32){ 
      switch(i&7){
      case 0: return (uint32*)(seg+((uint16)(cpu.eax+*(int8*)ip++))); break;
      case 1: return (uint32*)(seg+((uint16)(cpu.ecx+*(int8*)ip++))); break;
      case 2: return (uint32*)(seg+((uint16)(cpu.edx+*(int8*)ip++))); break;
      case 3: return (uint32*)(seg+((uint16)(cpu.ebx+*(int8*)ip++))); break;
      case 4: i=sib(); return (uint32*)(seg+((uint16)(i+*(int8*)ip++))); break;
      case 5: return (uint32*)(seg_bp+((uint16)(cpu.ebp+*(int8*)ip++))); break;
      case 6: return (uint32*)(seg+((uint16)(cpu.esi+*(int8*)ip++))); break;
      case 7: return (uint32*)(seg+((uint16)(cpu.edi+*(int8*)ip++))); break;
      }
    }else{
      switch(i&7){
      case 0: return (uint32*)(seg+((uint16)(cpu.bx+cpu.si+*(int8*)ip++))); break;
      case 1: return (uint32*)(seg+((uint16)(cpu.bx+cpu.di+*(int8*)ip++))); break;
      case 2: return (uint32*)(seg_bp+((uint16)(cpu.bp+cpu.si+*(int8*)ip++))); break;
      case 3: return (uint32*)(seg_bp+((uint16)(cpu.bp+cpu.di+*(int8*)ip++))); break;
      case 4: return (uint32*)(seg+((uint16)(cpu.si+*(int8*)ip++))); break;
      case 5: return (uint32*)(seg+((uint16)(cpu.di+*(int8*)ip++))); break;
      case 6: return (uint32*)(seg_bp+((uint16)(cpu.bp+*(int8*)ip++))); break;
      case 7: return (uint32*)(seg+((uint16)(cpu.bx+*(int8*)ip++))); break;
      }
    }
    break;
  case 2:
    ip++;
    if (a32){ 
      switch(i&7){
      case 0: return (uint32*)(seg+((uint16)(cpu.eax+*(uint32*)((ip+=4)-4)))); break;
      case 1: return (uint32*)(seg+((uint16)(cpu.ecx+*(uint32*)((ip+=4)-4)))); break;
      case 2: return (uint32*)(seg+((uint16)(cpu.edx+*(uint32*)((ip+=4)-4)))); break;
      case 3: return (uint32*)(seg+((uint16)(cpu.ebx+*(uint32*)((ip+=4)-4)))); break;
      case 4: i=sib(); return (uint32*)(seg+((uint16)(i+*(uint32*)((ip+=4)-4)))); break;
      case 5: return (uint32*)(seg_bp+((uint16)(cpu.ebp+*(uint32*)((ip+=4)-4)))); break;
      case 6: return (uint32*)(seg+((uint16)(cpu.esi+*(uint32*)((ip+=4)-4)))); break;
      case 7: return (uint32*)(seg+((uint16)(cpu.edi+*(uint32*)((ip+=4)-4)))); break;
      }
    }else{
      switch(i&7){
      case 0: return (uint32*)(seg+((uint16)(cpu.bx+cpu.si+*(uint16*)((ip+=2)-2)))); break;
      case 1: return (uint32*)(seg+((uint16)(cpu.bx+cpu.di+*(uint16*)((ip+=2)-2)))); break;
      case 2: return (uint32*)(seg_bp+((uint16)(cpu.bp+cpu.si+*(uint16*)((ip+=2)-2)))); break;
      case 3: return (uint32*)(seg_bp+((uint16)(cpu.bp+cpu.di+*(uint16*)((ip+=2)-2)))); break;
      case 4: return (uint32*)(seg+((uint16)(cpu.si+*(uint16*)((ip+=2)-2)))); break;
      case 5: return (uint32*)(seg+((uint16)(cpu.di+*(uint16*)((ip+=2)-2)))); break;
      case 6: return (uint32*)(seg_bp+((uint16)(cpu.bp+*(uint16*)((ip+=2)-2)))); break;
      case 7: return (uint32*)(seg+((uint16)(cpu.bx+*(uint16*)((ip+=2)-2)))); break;
      }
    }
    break;
  }
}

uint8* seg_es_ptr;
uint8* seg_cs_ptr;
uint8* seg_ss_ptr;
uint8* seg_ds_ptr;
uint8* seg_fs_ptr;
uint8* seg_gs_ptr;

#define seg_es 0
#define seg_cs 1
#define seg_ss 2
#define seg_ds 3
#define seg_fs 4
#define seg_gs 5


#define op_r i&7
void cpu_call(){

  static int32 i,i2,i3,x,x2,x3,y,y2,y3;
  static uint8 b,b2,b3;
  static uint8 *uint8p;
  static uint16 *uint16p;
  static uint32 *uint32p;
  static uint8* dseg;
  static int32 r;
  ip=(uint8*)&cmem[cpu.cs*16+cpu.ip];

  seg_es_ptr=(uint8*)cmem+cpu.es*16;
  seg_cs_ptr=(uint8*)cmem+cpu.cs*16;
  seg_ss_ptr=(uint8*)cmem+cpu.ss*16;
  seg_ds_ptr=(uint8*)cmem+cpu.ds*16;
  seg_fs_ptr=(uint8*)cmem+cpu.fs*16;
  seg_gs_ptr=(uint8*)cmem+cpu.gs*16;

 next_opcode:
  b32=0; a32=0; seg=seg_ds_ptr; seg_bp=seg_ss_ptr;

  i=*ip++;

  //read any prefixes
  if (i==0x66){b32=1; i=*ip++;}
  if (i==0x26){seg_bp=seg=seg_es_ptr; i=*ip++;}
  if (i==0x2E){seg_bp=seg=seg_cs_ptr; i=*ip++;}
  if (i==0x36){seg=seg_ss_ptr; i=*ip++;}
  if (i==0x3E){seg_bp=seg_ds_ptr; i=*ip++;}
  if (i==0x64){seg_bp=seg=seg_fs_ptr; i=*ip++;}
  if (i==0x65){seg_bp=seg=seg_gs_ptr; i=*ip++;}
  if (i==0x67){a32=1; i=*ip++;}

  if (i==0x0F) goto opcode_0F;

  r=*ip>>3&7;

  //mov
  if (i!=0x8D){
    if (i>=0x88&&i<=0x8E){
      switch(i){
      case 0x88:// /r r/m8,r8
    *rm8()=*reg8[r];
    break;
      case 0x89:// /r r/m16(32),r16(32)
    if (b32) *rm32()=*reg32[r]; else *rm16()=*reg16[r];
    break;
      case 0x8A:// /r r8,r/m8
    *reg8[r]=*rm8();
    break;
      case 0x8B:// /r r16(32),r/m16(32)
    if (b32) *reg32[r]=*rm32(); else *reg16[r]=*rm16();
    break;
      case 0x8C:// /r r/m16,Sreg
    *rm16()=*segreg[r];
    break;
      case 0x8E:// /r Sreg,r/m16
    *segreg[r]=*rm16();
    if (r==0) seg_es_ptr=(uint8*)cmem+*segreg[r]*16;
    //CS (r==1) cannot be set
    if (r==2) seg_ss_ptr=(uint8*)cmem+*segreg[r]*16;
    if (r==3) seg_ds_ptr=(uint8*)cmem+*segreg[r]*16;
    if (r==4) seg_fs_ptr=(uint8*)cmem+*segreg[r]*16;
    if (r==5) seg_gs_ptr=(uint8*)cmem+*segreg[r]*16;
    break;
      }
      goto done;
    }
  }
  if (i>=0xA0&&i<=0xA3){
    switch(i){
    case 0xA0:// al,moffs8
      cpu.al=*(seg+*(uint16*)ip); ip+=2;
      break;
    case 0xA1:// (e)ax,moffs16(32)
      if (b32){cpu.eax=*(uint32*)(seg+*(uint16*)ip); ip+=2;}else{cpu.ax=*(uint16*)(seg+*(uint16*)ip); ip+=2;}
      break;
    case 0xA2:// moffs8,al
      *(seg+*(uint16*)ip)=cpu.al; ip+=2;
      break;
    case 0xA3:// moffs16(32),(e)ax
      if (b32){*(uint32*)(seg+*(uint16*)ip)=cpu.eax; ip+=2;}else{*(uint16*)(seg+*(uint16*)ip)=cpu.ax; ip+=2;}
      break;
    }
    goto done;
  }
  if (i>=0xB0&&i<=0xB7){// +rb reg8,imm8
    *reg8[op_r]=*ip++;
    goto done;
  }
  if (i>=0xB8&&i<=0xBF){// +rw(rd) reg16(32),imm16(32)
    if (b32){*reg32[op_r]=*(uint32*)ip; ip+=4;}else{*reg16[op_r]=*(uint16*)ip; ip+=2;}
    goto done;
  }
  if (i==0xC6){// r/m8,imm8
    uint8p=rm8(); *uint8p=*ip++;
    goto done;
  }
  if (i==0xC7){// r/m16(32),imm16(32)
    if (b32){uint32p=rm32(); *uint32p=*(uint32*)ip; ip+=4;}else{uint16p=rm16(); *uint16p=*(uint16*)ip; ip+=2;}
    goto done;
  }

  //ret (todo)
  if (i==0xCB){//(far)
    //assume return control (revise later)
    return;
  }
  if (i==0xCA){//imm16 (far)
    //assume return control (revise later)
    return;
  }

  //int (todo)
  if (i==0xCD){
    call_int(*ip++);//assume interrupt table is 0xFFFF
    goto done;
  }

  //push
  if (i==0xFF){
    if (b32){*((uint32*)(seg_ss_ptr+(cpu.sp-=4)))=*rm32();}else{*((uint16*)(seg_ss_ptr+(cpu.sp-=2)))=*rm16();}
    goto done;
  }
  if (i>=0x50&&i<=0x57){//+ /r r16(32)
    if (b32){*((uint32*)(seg_ss_ptr+(cpu.sp-=4)))=*reg32[op_r];}else{*((uint16*)(seg_ss_ptr+(cpu.sp-=2)))=*reg16[op_r];}
    goto done;
  }
  if (i==0x6A){//imm8 (sign extended to 16 bits)
    *((uint16*)(seg_ss_ptr+(cpu.sp-=2)))=((int8)*ip++);
    goto done;
  }
  if (i==0x68){//imm16(32)
    if (b32){*((uint32*)(seg_ss_ptr+(cpu.sp-=4)))=*(uint32*)ip; ip+=4;}else{*((uint16*)(seg_ss_ptr+(cpu.sp-=2)))=*(uint16*)ip; ip+=2;}
    goto done;
  }
  if (i==0x0E){//CS
    *((uint16*)(seg_ss_ptr+(cpu.sp-=2)))=*segreg[seg_cs];
    goto done;
  }
  if (i==0x16){//SS
    *((uint16*)(seg_ss_ptr+(cpu.sp-=2)))=*segreg[seg_ss];
    goto done;
  }
  if (i==0x1E){//DS
    *((uint16*)(seg_ss_ptr+(cpu.sp-=2)))=*segreg[seg_ds];
    goto done;
  }
  if (i==0x06){//ES
    *((uint16*)(seg_ss_ptr+(cpu.sp-=2)))=*segreg[seg_es];
    goto done;
  }

  //pop
  if (i==0x8F){
    if (b32){*rm32()=*((uint32*)(seg_ss_ptr-4+(cpu.sp+=4)));}else{*rm16()=*((uint16*)(seg_ss_ptr-2+(cpu.sp+=2)));}
    goto done;
  }
  if (i>=0x58&&i<=0x5F){//+rw(d) r16(32)
    if (b32){*reg32[op_r]=*((uint32*)(seg_ss_ptr-4+(cpu.sp+=4)));}else{*reg16[op_r]=*((uint16*)(seg_ss_ptr-2+(cpu.sp+=2)));}
    goto done;
  }
  if (i==0x1F){//DS
    *segreg[seg_ds]=*((uint16*)(seg_ss_ptr-2+(cpu.sp+=2)));
    goto done;
  }
  if (i==0x07){//ES

    *segreg[seg_es]=*((uint16*)(seg_ss_ptr-2+(cpu.sp+=2)));
    goto done;
  }
  if (i==0x17){//SS
    *segreg[seg_ss]=*((uint16*)(seg_ss_ptr-2+(cpu.sp+=2)));
    goto done;
  }

  goto skip_0F_opcodes;
 opcode_0F:
  i=*ip++;
  r=*ip>>3&7; //required???

  //push
  if (i==0xA0){
    *((uint16*)(seg_ss_ptr+(cpu.sp-=2)))=*segreg[seg_fs];
    goto done;
  }
  if (i==0xA8){
    *((uint16*)(seg_ss_ptr+(cpu.sp-=2)))=*segreg[seg_gs];
    goto done;
  }

  //pop
  if (i==0xA1){//FS
    *segreg[seg_fs]=*((uint16*)(seg_ss_ptr-2+(cpu.sp+=2)));
    goto done;
  }
  if (i==0xA9){//GS
    *segreg[seg_gs]=*((uint16*)(seg_ss_ptr-2+(cpu.sp+=2)));
    goto done;
  }

 skip_0F_opcodes:

  i2=((i>>4)&15); if (i2<=9) i2+=48; else i2=i2-10+65;
  unknown_opcode_mess->chr[16]=i2;
  i2=i&15; if (i2<=9) i2+=48; else i2=i2-10+65;
  unknown_opcode_mess->chr[17]=i2;
  MessageBox2(NULL,(char*)unknown_opcode_mess->chr,"X86 Error",MB_OK|MB_SYSTEMMODAL);
  exit(86);
 done:
  if (*ip) goto next_opcode;

  exit(cmem[0]);

}



int32 screen_last_valid=0;
uint8 *screen_last=(uint8*)malloc(1);
uint32 screen_last_size=1;


uint64 asciicode_value=0;
int32 asciicode_reading=0;



int32 lock_display=0;
int32 lock_display_required=0;

//cost delay, made obselete by managing thread priorities (consider removal)
#define cost_limit 10000
#define cost_delay 0
uint32 cost=0;

#include "msbin.c"

//#include "time64.c"
//#include "time64.h"


int64 build_int64(uint32 val2,uint32 val1){
  static int64 val;
  val=val2;
  val<<=32;
  val|=val1;
  return val;
}

uint64 build_uint64(uint32 val2,uint32 val1){
  static uint64 val;
  val=val2;
  val<<=32;
  val|=val1;
  return val;
}

//nb. abreviations are used in variable names to save typing, here are some of the expansions
//cmem=conventional memory
//qbs=qbick basic string (refers to the emulation of quick basic strings)
//sp=stack pointer
//dblock=a 64K memory block in conventional memory holding single variables and strings
uint8 *cmem_static_base=&cmem[0]+1280+65536;
//[1280][DBLOCK][STATIC-><-DYNAMIC][A000-]

uint32 qbs_cmem_descriptor_space=256; //enough for 64 strings before expansion

uint32 qb64_firsttimervalue;//based on time of day
uint32 clock_firsttimervalue;//based on program launch time




uint8 wait_needed=1;

int32 full_screen=0;//0,1(stretched/closest),2(1:1)
int32 full_screen_toggle=0;//increments each time ALT+ENTER is pressed
int32 full_screen_set=-1;//0(windowed),1(stretched/closest),2(1:1)


int32 vertical_retrace_in_progress=0;
int32 vertical_retrace_happened=0;





static const char *arrow[] = {
  /* width height num_colors chars_per_pixel */
  "    32    32        3            1",
  /* colors */
  "X c #000000",
  ". c #ffffff",
  "  c None",
  /* pixels */
  "X                               ",
  "XX                              ",
  "X.X                             ",
  "X..X                            ",
  "X...X                           ",
  "X....X                          ",
  "X.....X                         ",
  "X......X                        ",
  "X.......X                       ",
  "X........X                      ",
  "X.........X                     ",
  "X......XXXXX                    ",
  "X...X..X                        ",
  "X..XX..X                        ",
  "X.X  X..X                       ",
  "XX   X..X                       ",
  "X     X..X                      ",
  "      X..X                      ",
  "       X..X                     ",
  "       X..X                     ",
  "        XX                      ",
  "                                ",
  "                                ",
  "                                ",
  "                                ",
  "                                ",
  "                                ",
  "                                ",
  "                                ",
  "                                ",
  "                                ",
  "                                ",
  "0,0"
};


uint8 lock_subsystem=0;

extern uint8 close_program; //=0;
uint8 program_wait=0;

extern uint8 suspend_program;
extern uint8 stop_program;


int32 global_counter=0;
extern double last_line;
void end(void);

void fix_error(){
  char *errtitle = NULL, *errmess = NULL, *cp;
  int prevent_handling = 0, len, v;
  if ((new_error >= 300) && (new_error <= 313)) prevent_handling = 1;
  if (!error_goto_line || error_handling || prevent_handling) {
    switch (new_error) {
      case 1: cp="NEXT without FOR"; break;
      case 2: cp="Syntax error"; break;
      case 3: cp="RETURN without GOSUB"; break;
      case 4: cp="Out of DATA"; break;
      case 5: cp="Illegal function call"; break;
      case 6: cp="Overflow"; break;
      case 7: cp="Out of memory"; break;
      case 8: cp="Label not defined"; break;
      case 9: cp="Subscript out of range"; break;
      case 10: cp="Duplicate definition"; break;
      case 12: cp="Illegal in direct mode"; break;
      case 13: cp="Type mismatch"; break;
      case 14: cp="Out of string space"; break;
        //error 15 undefined
      case 16: cp="String formula too complex"; break;
      case 17: cp="Cannot continue"; break;
      case 18: cp="Function not defined"; break;
      case 19: cp="No RESUME"; break;
      case 20: cp="RESUME without error"; break;
        //error 21-23 undefined
      case 24: cp="Device timeout"; break;
      case 25: cp="Device fault"; break;
      case 26: cp="FOR without NEXT"; break;
      case 27: cp="Out of paper"; break;
        //error 28 undefined
      case 29: cp="WHILE without WEND"; break;
      case 30: cp="WEND without WHILE"; break;
        //error 31-32 undefined
      case 33: cp="Duplicate label"; break;
        //error 34 undefined
      case 35: cp="Subprogram not defined"; break;
        //error 36 undefined
      case 37: cp="Argument-count mismatch"; break;
      case 38: cp="Array not defined"; break;
      case 40: cp="Variable required"; break;
      case 50: cp="FIELD overflow"; break;
      case 51: cp="Internal error"; break;
      case 52: cp="Bad file name or number"; break;
      case 53: cp="File not found"; break;
      case 54: cp="Bad file mode"; break;
      case 55: cp="File already open"; break;
      case 56: cp="FIELD statement active"; break;
      case 57: cp="Device I/O error"; break;
      case 58: cp="File already exists"; break;
      case 59: cp="Bad record length"; break;
      case 61: cp="Disk full"; break;
      case 62: cp="Input past end of file"; break;
      case 63: cp="Bad record number"; break;
      case 64: cp="Bad file name"; break;
      case 67: cp="Too many files"; break;
      case 68: cp="Device unavailable"; break;
      case 69: cp="Communication-buffer overflow"; break;
      case 70: cp="Permission denied"; break;
      case 71: cp="Disk not ready"; break;
      case 72: cp="Disk-media error"; break;
      case 73: cp="Feature unavailable"; break;
      case 74: cp="Rename across disks"; break;
      case 75: cp="Path/File access error"; break;
      case 76: cp="Path not found"; break;
      case 258: cp="Invalid handle"; break;

      case 300: cp="Memory region out of range"; break;
      case 301: cp="Invalid size"; break;
      case 302: cp="Source memory region out of range"; break;
      case 303: cp="Destination memory region out of range"; break;
      case 304: cp="Source and destination memory regions out of range"; break;
      case 305: cp="Source memory has been freed"; break;
      case 306: cp="Destination memory has been freed"; break;
      case 307: cp="Memory already freed"; break;
      case 308: cp="Memory has been freed"; break;
      case 309: cp="Memory not initialized"; break;
      case 310: cp="Source memory not initialized"; break;
      case 311: cp="Destination memory not initialized"; break;
      case 312: cp="Source and destination memory not initialized"; break;
      case 313: cp="Source and destination memory have been freed"; break;
      default: cp="Unprintable error"; break;
    }

#define FIXERRMSG_TITLE "%s%u"
#define FIXERRMSG_BODY "Line: %u (in %s)\n%s%s"
#define FIXERRMSG_MAINFILE "main module"
#define FIXERRMSG_CONT "\nContinue?"
#define FIXERRMSG_UNHAND "Unhandled Error #"
#define FIXERRMSG_CRIT "Critical Error #"

    len = snprintf(errmess, 0, FIXERRMSG_BODY, (inclercl ? inclercl : ercl), (inclercl ? includedfilename : FIXERRMSG_MAINFILE), cp, (!prevent_handling ? FIXERRMSG_CONT : ""));
    errmess = (char*)malloc(len + 1);
    if (!errmess) exit(0); //At this point we just give up
    snprintf(errmess, len + 1, FIXERRMSG_BODY, (inclercl ? inclercl : ercl), (inclercl ? includedfilename : FIXERRMSG_MAINFILE), cp, (!prevent_handling ? FIXERRMSG_CONT : ""));

    len = snprintf(errtitle, 0, FIXERRMSG_TITLE, (!prevent_handling ? FIXERRMSG_UNHAND : FIXERRMSG_CRIT), new_error);
    errtitle = (char*)malloc(len + 1);
    if (!errtitle) exit(0); //At this point we just give up
    snprintf(errtitle, len + 1, FIXERRMSG_TITLE, (!prevent_handling ? FIXERRMSG_UNHAND : FIXERRMSG_CRIT), new_error);

//Android cannot halt threads, so the easiest compromise is to just display the error
#ifdef QB64_ANDROID
        showErrorOnScreen(cp, new_error, ercl);
#endif

    if (prevent_handling){
      v=MessageBox2(NULL,errmess,errtitle,MB_OK);
      exit(0);
    }else{
      v=MessageBox2(NULL,errmess,errtitle,MB_YESNO|MB_SYSTEMMODAL);
    }

    if ((v==IDNO)||(v==IDOK)){close_program=1; end();}
    new_error=0;
    return;
  }
  error_err=new_error;
  new_error=0;
  error_erl=last_line;
  error_occurred=1;
  QBMAIN (NULL);
  return;
}



void error(int32 error_number){

  //critical errors:

  //out of memory errors
  if (error_number==257){MessageBox2(NULL,"Out of memory","Critical Error #1",MB_OK|MB_SYSTEMMODAL); exit(0);}//generic "Out of memory" error
  //tracable "Out of memory" errors
  if (error_number==502){MessageBox2(NULL,"Out of memory","Critical Error #2",MB_OK|MB_SYSTEMMODAL); exit(0);}
  if (error_number==503){MessageBox2(NULL,"Out of memory","Critical Error #3",MB_OK|MB_SYSTEMMODAL); exit(0);}
  if (error_number==504){MessageBox2(NULL,"Out of memory","Critical Error #4",MB_OK|MB_SYSTEMMODAL); exit(0);}
  if (error_number==505){MessageBox2(NULL,"Out of memory","Critical Error #5",MB_OK|MB_SYSTEMMODAL); exit(0);}
  if (error_number==506){MessageBox2(NULL,"Out of memory","Critical Error #6",MB_OK|MB_SYSTEMMODAL); exit(0);}
  if (error_number==507){MessageBox2(NULL,"Out of memory","Critical Error #7",MB_OK|MB_SYSTEMMODAL); exit(0);}
  if (error_number==508){MessageBox2(NULL,"Out of memory","Critical Error #8",MB_OK|MB_SYSTEMMODAL); exit(0);}
  if (error_number==509){MessageBox2(NULL,"Out of memory","Critical Error #9",MB_OK|MB_SYSTEMMODAL); exit(0);}
  if (error_number==510){MessageBox2(NULL,"Out of memory","Critical Error #10",MB_OK|MB_SYSTEMMODAL); exit(0);}
  if (error_number==511){MessageBox2(NULL,"Out of memory","Critical Error #11",MB_OK|MB_SYSTEMMODAL); exit(0);}
  if (error_number==512){MessageBox2(NULL,"Out of memory","Critical Error #12",MB_OK|MB_SYSTEMMODAL); exit(0);}
  if (error_number==513){MessageBox2(NULL,"Out of memory","Critical Error #13",MB_OK|MB_SYSTEMMODAL); exit(0);}
  if (error_number==514){MessageBox2(NULL,"Out of memory","Critical Error #14",MB_OK|MB_SYSTEMMODAL); exit(0);}
  if (error_number==515){MessageBox2(NULL,"Out of memory","Critical Error #15",MB_OK|MB_SYSTEMMODAL); exit(0);}
  if (error_number==516){MessageBox2(NULL,"Out of memory","Critical Error #16",MB_OK|MB_SYSTEMMODAL); exit(0);}
  if (error_number==517){MessageBox2(NULL,"Out of memory","Critical Error #17",MB_OK|MB_SYSTEMMODAL); exit(0);}
  if (error_number==518){MessageBox2(NULL,"Out of memory","Critical Error #18",MB_OK|MB_SYSTEMMODAL); exit(0);}

  //other critical errors
  if (error_number==11){MessageBox2(NULL,"Division by zero","Critical Error",MB_OK|MB_SYSTEMMODAL); exit(0);}
  if (error_number==256){MessageBox2(NULL,"Out of stack space","Critical Error",MB_OK|MB_SYSTEMMODAL); exit(0);}
  if (error_number==259){MessageBox2(NULL,"Cannot find dynamic library file","Critical Error",MB_OK|MB_SYSTEMMODAL); exit(0);}
  if (error_number==260){MessageBox2(NULL,"Sub/Function does not exist in dynamic library","Critical Error",MB_OK|MB_SYSTEMMODAL); exit(0);}
  if (error_number==261){MessageBox2(NULL,"Sub/Function does not exist in dynamic library","Critical Error",MB_OK|MB_SYSTEMMODAL); exit(0);}
  if (error_number==262){MessageBox2(NULL,"Function unavailable in QLOUD","Critical Error",MB_OK|MB_SYSTEMMODAL); exit(0);}
  if (error_number==263){MessageBox2(NULL,"Paths/Filename illegal in QLOUD","Critical Error",MB_OK|MB_SYSTEMMODAL); exit(0);}

  if (error_number==270){MessageBox2(NULL,"_GL command called outside of SUB _GL's scope","Critical Error",MB_OK|MB_SYSTEMMODAL); exit(0);}
  if (error_number==271){MessageBox2(NULL,"END/SYSTEM called within SUB _GL's scope","Critical Error",MB_OK|MB_SYSTEMMODAL); exit(0);}


  if (!new_error){
    if ((new_error==256)||(new_error==257)) fix_error();//critical error!
    if (error_number<=0) error_number=5;//Illegal function call
    new_error=error_number;
    qbevent=1;
  }
}

double get_error_erl(){
  return error_erl;
}

uint32 get_error_err(){
  return error_err;
}

void end(){
  dont_call_sub_gl=1;
  exit_ok|=1;
  while(!stop_program) Sleep(16);
  while(1) Sleep(16);
}



//MEM_STATIC memory manager
/*
  mem_static uses a pointer called mem_static_pointer to allocate linear memory.
  It can also change mem_static_pointer back to a previous location, effectively erasing
  any memory after that point.
  Because memory cannot be guaranteed to be allocated in exactly the same location
  after realloc which QB64 requires to keep functionality of previous pointers when
  the current block of memory is full QB64 creates an entirely new block, much larger
  than the previous block (at least 2x), and "writes-off" the previous block as un-
  reclaimable memory. This tradeoff is worth the speed it recovers.
  This allocation strategy can be shown as follows: (X=1MB)
  X
  XX
  XXXX
  XXXXXXXX
  XXXXXXXXXXXXXXXX
  etc.
*/
uint32 mem_static_size;
extern uint8 *mem_static;
extern uint8 *mem_static_pointer;
extern uint8 *mem_static_limit;

uint8 *mem_static_malloc(uint32 size){
  size+=7; size-=(size&7);//align to 8 byte boundry
  if ((mem_static_pointer+=size)<mem_static_limit) return mem_static_pointer-size;
  mem_static_size=(mem_static_size<<1)+size;
  mem_static=(uint8*)malloc(mem_static_size);
  if (!mem_static) error(504);
  mem_static_pointer=mem_static+size;
  mem_static_limit=mem_static+mem_static_size;
  return mem_static_pointer-size;
}
void mem_static_restore(uint8* restore_point){
  if ((restore_point>=mem_static)&&(restore_point<=mem_static_limit)){
    mem_static_pointer=restore_point;
  }else{
    //if restore_point is not in the current block, use t=start of current block as a new base
    mem_static_pointer=mem_static;
  }
}

//CMEM_FAR_DYNAMIC memory manager
/*
  (uses a custom "links" based memory manager)
*/
//           &HA000    DBLOCK SIZE        DBLOCK OFFSET
//           655360 - (65536            + 1280         )=588544 links possible
//links limited to 588544/4=147136 (do not have enough links if avg. block size less than 4 bytes)
//stores blocks, not free memory, because blocks are easier to identify
//always scanned from beginning to end, so prev. pointer is unnecessary
struct cmem_dynamic_link_type{
  uint8 *offset;
  uint8 *top;
  uint32 size;
  uint32 i;
  cmem_dynamic_link_type *next;
};
cmem_dynamic_link_type cmem_dynamic_link[147136+1]; //+1 is added because array is used from index 1

//i=cmem_dynamic_next_link++; if (i>=147136) error(257);//not enough blocks
//newlink=(cmem_dynamic_link_type*)&cmem_dynamic_link[i];

cmem_dynamic_link_type *cmem_dynamic_link_first=NULL;
int32 cmem_dynamic_next_link=0;
int32 cmem_dynamic_free_link=0;
uint32 cmem_dynamic_free_list[147136];
uint8 *cmem_dynamic_malloc(uint32 size){
  static int32 i;
  static uint8 *top;
  static cmem_dynamic_link_type *link;
  static cmem_dynamic_link_type *newlink;
  static cmem_dynamic_link_type *prev_link;
  if (size>65536) error(505);//>64K
  //to avoid mismatches between offsets, all 0-byte blocks are given the special offset A000h (the top of the heap)
  if (!size) return(&cmem[0]+655360);//top of heap
  //forces blocks to be multiples of 16 bytes so they align with segment boundaries
  if (size&15) size=size-(size&15)+16;
  //is a space large enough between existing blocks available?
  //(if not, memory will be allocated at bottom of heap)
  top=&cmem[0]+655360;//top is the base of the higher block
  prev_link=NULL;
  if (link=cmem_dynamic_link_first){
  cmem_dynamic_findspace:
    if ((top-link->top)>=size){ //gpf
      //found free space
      goto cmem_dynamic_make_new_link;
    }
    prev_link=link; top=link->offset;//set top to the base of current block for future comparisons
    if (link=link->next) goto cmem_dynamic_findspace;
  }
  //no space between existing blocks is large enough, alloc below 'top'
  if ((top-cmem_static_pointer)<size) error(506);//a large enough block cannot be created!
  cmem_dynamic_base=top-size;
  //get a new link index
 cmem_dynamic_make_new_link:
  if (cmem_dynamic_free_link){
    i=cmem_dynamic_free_list[cmem_dynamic_free_link--];
  }else{
    i=cmem_dynamic_next_link++; if (i>=147136) error(507);//not enough blocks
  }
  newlink=(cmem_dynamic_link_type*)&cmem_dynamic_link[i];
  //set link info
  newlink->i=i;
  newlink->offset=top-size;
  newlink->size=size;
  newlink->top=top;
  //attach below prev_link
  if (prev_link){
    newlink->next=prev_link->next;//NULL if none
    prev_link->next=newlink;
  }else{
    newlink->next=cmem_dynamic_link_first;//NULL if none
    cmem_dynamic_link_first=newlink;
  }
  return newlink->offset;
}

void cmem_dynamic_free(uint8 *block){
  static cmem_dynamic_link_type *link;
  static cmem_dynamic_link_type *prev_link;
  if (!cmem_dynamic_link_first) return;
  if (!block) return;
  if (block==(&cmem[0]+655360)) return;//to avoid mismatches between offsets, all 0-byte blocks are given the special offset A000h (the top of the heap)
  prev_link=NULL;
  link=cmem_dynamic_link_first;
 check_next:
  if (link->offset==block){
    //unlink
    if (prev_link){
      prev_link->next=link->next;
    }else{
      cmem_dynamic_link_first=link->next;
    }
    //free link
    cmem_dynamic_free_link++;
    cmem_dynamic_free_list[cmem_dynamic_free_link]=link->i;
    //memory freed successfully!
    return;
  }
  prev_link=link;
  if (link=link->next) goto check_next;
  return;
}

uint8 *defseg=&cmem[1280];//set to base of DBLOCK

void sub_defseg(int32 segment,int32 passed){
  if (new_error) return;
  if (!passed){
    defseg=&cmem[1280];
    return;
  }

  if ((segment<-65536)||(segment>65535)){//same range as QB checks
    error(6);
  }else{
    defseg=&cmem[0]+((uint16)segment)*16;
  }
}

int32 func_peek(int32 offset){
  if ((offset<-65536)||(offset>65535)){//same range as QB checks
    error(6);
    return 0;
  }
  return defseg[(uint16)offset];
}

void sub_poke(int32 offset,int32 value){
  if (new_error) return;
  if ((offset<-65536)||(offset>65535)){//same range as QB checks
    error(6);
    return;
  }
  defseg[(uint16)offset]=value;
}

int32 array_ok=1;//kept to compile legacy versions

//gosub-return handling
extern uint32 next_return_point; //=0;
extern uint32 *return_point; //=(uint32*)malloc(4*16384);
extern uint32 return_points; //=16384;
void more_return_points(){
  if (return_points>2147483647) error(256);
  return_points*=2;
  return_point=(uint32*)realloc(return_point,return_points*4);
  if (return_point==NULL) error(256);
}





uint8 keyon[65536];

qbs* singlespace;

qbs *func_mksmbf(float val){
  static qbs *tqbs;
  tqbs=qbs_new(4,1);
  if (_fieeetomsbin(&val,(float*)tqbs->chr)==1) {error(5); tqbs->len=0;}
  return tqbs;
}
qbs *func_mkdmbf(double val){
  static qbs *tqbs;
  tqbs=qbs_new(8,1);
  if (_dieeetomsbin(&val,(double*)tqbs->chr)==1) {error(5); tqbs->len=0;}
  return tqbs;
}

float func_cvsmbf(qbs *str){
  static float val;
  if (str->len<4) {error(5); return 0;}
  if (_fmsbintoieee((float*)str->chr,&val)==1) {error(5); return 0;}
  return val;
}
double func_cvdmbf(qbs *str){
  static double val;
  if (str->len<8) {error(5); return 0;}
  if (_dmsbintoieee((double*)str->chr,&val)==1) {error(5); return 0;}
  return val;
}

qbs *b2string(char v){ static qbs *tqbs; tqbs=qbs_new(1,1); *((char*)(tqbs->chr))=v; return tqbs;}
qbs *ub2string(char v){ static qbs *tqbs; tqbs=qbs_new(1,1); *((uint8*)(tqbs->chr))=v; return tqbs;}
qbs *i2string(int16 v){ static qbs *tqbs; tqbs=qbs_new(2,1); *((int16*)(tqbs->chr))=v; return tqbs;}
qbs *ui2string(int16 v){ static qbs *tqbs; tqbs=qbs_new(2,1); *((uint16*)(tqbs->chr))=v; return tqbs;}
qbs *l2string(int32 v){ static qbs *tqbs; tqbs=qbs_new(4,1); *((int32*)(tqbs->chr))=v; return tqbs;}
qbs *ul2string(uint32 v){ static qbs *tqbs; tqbs=qbs_new(4,1); *((uint32*)(tqbs->chr))=v; return tqbs;}
qbs *i642string(int64 v){ static qbs *tqbs; tqbs=qbs_new(8,1); *((int64*)(tqbs->chr))=v; return tqbs;}
qbs *ui642string(uint64 v){ static qbs *tqbs; tqbs=qbs_new(8,1); *((uint64*)(tqbs->chr))=v; return tqbs;}
qbs *s2string(float v){ static qbs *tqbs; tqbs=qbs_new(4,1); *((float*)(tqbs->chr))=v; return tqbs;}
qbs *d2string(double v){ static qbs *tqbs; tqbs=qbs_new(8,1); *((double*)(tqbs->chr))=v; return tqbs;}
qbs *f2string(long double v){ static qbs *tqbs; tqbs=qbs_new(32,1); memset(tqbs->chr,0,32); *((long double*)(tqbs->chr))=v; return tqbs;}
qbs *bit2string(uint32 bsize,int64 v){
  static qbs* tqbs;
  tqbs=qbs_new(8,1);
  int64 bmask;
  bmask=~(-(((int64)1)<<bsize));
  *((int64*)(tqbs->chr))=v&bmask;
  tqbs->len=(bsize+7)>>3;
  return tqbs;
}
qbs *ubit2string(uint32 bsize,uint64 v){
  static qbs* tqbs;
  int64 bmask;
  tqbs=qbs_new(8,1);
  bmask=~(-(((int64)1)<<bsize));
  *((uint64*)(tqbs->chr))=v&bmask;
  tqbs->len=(bsize+7)>>3;
  return tqbs;
}

char string2b(qbs*str){ if (str->len<1) {error(5); return 0;} else {return *((char*)str->chr);} }
uint8 string2ub(qbs*str){ if (str->len<1) {error(5); return 0;} else {return *((uint8*)str->chr);} }
int16 string2i(qbs*str){ if (str->len<2) {error(5); return 0;} else {return *((int16*)str->chr);} }
uint16 string2ui(qbs*str){ if (str->len<2) {error(5); return 0;} else {return *((uint16*)str->chr);} }
int32 string2l(qbs*str){ if (str->len<4) {error(5); return 0;} else {return *((int32*)str->chr);} }
uint32 string2ul(qbs*str){ if (str->len<4) {error(5); return 0;} else {return *((uint32*)str->chr);} }
int64 string2i64(qbs*str){ if (str->len<8) {error(5); return 0;} else {return *((int64*)str->chr);} }
uint64 string2ui64(qbs*str){ if (str->len<8) {error(5); return 0;} else {return *((uint64*)str->chr);} }
float string2s(qbs*str){ if (str->len<4) {error(5); return 0;} else {return *((float*)str->chr);} }
double string2d(qbs*str){ if (str->len<8) {error(5); return 0;} else {return *((double*)str->chr);} }
long double string2f(qbs*str){ if (str->len<32) {error(5); return 0;} else {return *((long double*)str->chr);} }
uint64 string2ubit(qbs*str,uint32 bsize){
  int64 bmask;
  if (str->len<((bsize+7)>>3)) {error(5); return 0;}
  bmask=~(-(((int64)1)<<bsize));
  return (*(uint64*)str->chr)&bmask;
}
int64 string2bit(qbs*str,uint32 bsize){
  int64 bmask, bval64;
  if (str->len<((bsize+7)>>3)) {error(5); return 0;}
  bmask=~(-(((int64)1)<<bsize));
  bval64=(*(uint64*)str->chr)&bmask;
  if (bval64&(((int64)1)<<(bsize-1))) return (bval64|(~bmask));
  return bval64;
}

void lrset_field(qbs *str);

void sub_lset(qbs *dest,qbs *source){
  if (new_error) return;
  if (source->len>=dest->len){
    if (dest->len) memcpy(dest->chr,source->chr,dest->len);
    goto field_check;
  }
  if (source->len) memcpy(dest->chr,source->chr,source->len);
  memset(dest->chr+source->len,32,dest->len-source->len);
 field_check:
  if (dest->field) lrset_field(dest);
}

void sub_rset(qbs *dest,qbs *source){
  if (new_error) return;
  if (source->len>=dest->len){
    if (dest->len) memcpy(dest->chr,source->chr,dest->len);
    goto field_check;
  }
  if (source->len) memcpy(dest->chr+dest->len-source->len,source->chr,source->len);
  memset(dest->chr,32,dest->len-source->len);
 field_check:
  if (dest->field) lrset_field(dest);
}




qbs *func_space(int32 spaces){
  static qbs *tqbs;
  if (spaces<0) spaces=0;
  tqbs=qbs_new(spaces,1);
  if (spaces) memset(tqbs->chr,32,spaces);
  return tqbs;
}

qbs *func_string(int32 characters,int32 asciivalue){
  static qbs *tqbs;
  if (characters<0) characters=0;
  tqbs=qbs_new(characters,1);
  if (characters) memset(tqbs->chr,asciivalue&0xFF,characters);
  return tqbs;
}

int32 func_instr(int32 start,qbs *str,qbs *substr,int32 passed){
  //QB64 difference: start can be 0 or negative
  //justification-start could be larger than the length of string to search in QBASIC
  static uint8 *limit,*base;
  static uint8 firstc;
  if (!passed) start=1;
  if (!str->len) return 0;
  if (start<1){
    start=1;
    if (!substr->len) return 0;
  }
  if (start>str->len) return 0;
  if (!substr->len) return start;
  if ((start+substr->len-1)>str->len) return 0;
  limit=str->chr+str->len;
  firstc=substr->chr[0];
  base=str->chr+start-1;
 nextchar:
  base=(uint8*)memchr(base,firstc,limit-base);
  if (!base) return 0;
  if ((base+substr->len)>limit) return 0;
  if (!memcmp(base,substr->chr,substr->len)) return base-str->chr+1;
  base++;
  if ((base+substr->len)>limit) return 0;
  goto nextchar;
}

void sub_mid(qbs *dest,int32 start,int32 l,qbs* src,int32 passed){
  if (new_error) return;
  static int32 src_offset;
  if (!passed) l=src->len;
  src_offset=0;
  if (dest==nothingstring) return;//quiet exit, error has already been reported!
  if (start<1){
    l=l+start-1;
    src_offset=-start+1;//src_offset is a byte offset with base 0!
    start=1;
  }
  if (l<=0) return;
  if (start>dest->len) return;
  if ((start+l-1)>dest->len) l=dest->len-start+1;
  //start and l are now reflect a valid region within dest
  if (src_offset>=src->len) return;
  if (l>(src->len-src_offset)) l=src->len-src_offset;
  //src_offset and l now reflect a valid region within src
  if (dest==src){
    if ((start-1)!=src_offset) memmove(dest->chr+start-1,src->chr+src_offset,l);
  }else{
    memcpy(dest->chr+start-1,src->chr+src_offset,l);
  }
}

qbs *func_mid(qbs *str,int32 start,int32 l,int32 passed){
  static qbs *tqbs;
  if (passed){
    if (start<1) {l=l-1+start; start=1;}
    if ((l>=1)&&(start<=str->len)){
      if ((start+l)>str->len) l=str->len-start+1;
    }else{
      l=0; start=1;//nothing!
    }
  }else{
    if (start<1) start=1;
    l=str->len-start+1;
    if (l<1){
      l=0; start=1;//nothing!
    }
  }
  if ((start==1)&&(l==str->len)) return str;//pass on
  if (str->tmp){ if (!str->fixed){ if (!str->readonly){ if (!str->in_cmem){//acquire
      str->chr=str->chr+(start-1);
      str->len=l;
      return str;
    }}}}
  tqbs=qbs_new(l,1);
  if (l) memcpy(tqbs->chr,str->chr+start-1,l);
  if (str->tmp) qbs_free(str);
  return tqbs;
}

qbs *qbs_ltrim(qbs *str){
  if (!str->len) return str;//pass on
  if (*str->chr!=32) return str;//pass on
  if (str->tmp){ if (!str->fixed){ if (!str->readonly){ if (!str->in_cmem){//acquire?
    qbs_ltrim_nextchar:
      if (*str->chr==32){
        str->chr++;
        if (--str->len) goto qbs_ltrim_nextchar;
      }
      return str;
    }}}}
  int32 i;
  i=0;
 qbs_ltrim_nextchar2: if (str->chr[i]==32) {i++; if (i<str->len) goto qbs_ltrim_nextchar2;}
  qbs *tqbs;
  tqbs=qbs_new(str->len-i,1);
  if (tqbs->len) memcpy(tqbs->chr,str->chr+i,tqbs->len);
  if (str->tmp) qbs_free(str);
  return tqbs;
}

qbs *qbs_rtrim(qbs *str){
  if (!str->len) return str;//pass on
  if (str->chr[str->len-1]!=32) return str;//pass on
  if (str->tmp){ if (!str->fixed){ if (!str->readonly){ if (!str->in_cmem){//acquire?
    qbs_rtrim_nextchar:
      if (str->chr[str->len-1]==32){
        if (--str->len) goto qbs_rtrim_nextchar;
      }
      return str;
    }}}}
  int32 i;
  i=str->len;
 qbs_rtrim_nextchar2: if (str->chr[i-1]==32) {i--; if (i) goto qbs_rtrim_nextchar2;}
  //i is the number of characters to keep
  qbs *tqbs;
  tqbs=qbs_new(i,1);
  if (i) memcpy(tqbs->chr,str->chr,i);
  if (str->tmp) qbs_free(str);
  return tqbs;
}

int32 func__str_nc_compare(qbs *s1, qbs *s2) {
  int32 limit, l1, l2;
  int32 v1, v2;
  unsigned char *c1=s1->chr, *c2=s2->chr;
  
  l1 = s1->len; l2 = s2->len;  //no need to get the length of these strings multiple times.
  if (!l1) {   
    if (l2) return -1; else return 0;  //if one is a null string we known the answer already.
  }
  if (!l2) return 1;
  if (l1<=l2) limit = l1; else limit = l2; //our limit is going to be the length of the smallest string.

  for (int32 i=0;i<limit; i++) {  //check the length of our string
    v1=*c1;v2=*c2;
    if ((v1>64)&&(v1<91)) v1=v1|32;
    if ((v2>64)&&(v2<91)) v2=v2|32;
    if (v1<v2) return -1;
    if (v1>v2) return 1;
       c1++;
     c2++;
    }
      
    if (l1<l2) return -1; 
  if (l2>l1) return 1;
  return 0;
}

int32 func__str_compare(qbs *s1, qbs *s2) {
  int32 i, limit, l1, l2;
    l1 = s1->len; l2 = s2->len;  //no need to get the length of these strings multiple times.
  if (!l1) {   
    if (l2) return -1; else return 0;  //if one is a null string we known the answer already.
  }
  if (!l2) return 1;
  if (l1<=l2) limit = l1; else limit = l2; 
    i=memcmp(s1->chr,s2->chr,limit); 
    if (i<0) return -1;
    if (i>0) return 1; 
    if (l1<l2) return -1; 
    if (l1>l2) return 1;
    return 0;
}

qbs *qbs_inkey(){
  if (new_error) return qbs_new(0,1);
  qbs *tqbs;
  if (cloud_app){
    Sleep(20);
  }else{
    Sleep(0);
  }
  tqbs=qbs_new(2,1);
  if (cmem[0x41a]!=cmem[0x41c]){
    tqbs->chr[0]=cmem[0x400+cmem[0x41a]];
    tqbs->chr[1]=cmem[0x400+cmem[0x41a]+1];
    if (tqbs->chr[0]){
      tqbs->len=1;
    }else{
      if (tqbs->chr[1]==0) tqbs->len=1;
    }
    cmem[0x41a]+=2;
    if (cmem[0x41a]==62) cmem[0x41a]=30;
  }else{
    tqbs->len=0;
  }
  return tqbs;
}

void sub__keyclear(int32 buf, int32 passed) {
  if (new_error) return;
  if (passed && (buf > 3 || buf < 1)) error(5);
  //  Sleep(10);
  if ((buf == 1 && passed) || !passed) {
    //INKEY$ buffer
    cmem[0x41a]=30; cmem[0x41b]=0; //head
    cmem[0x41c]=30; cmem[0x41d]=0; //tail
  }
  if ((buf == 2 && passed) || !passed) {
    //_KEYHIT buffer
    keyhit_nextfree = 0;
    keyhit_next = 0;
  }
  if ((buf == 3 && passed) || !passed) {
    //INP(&H60) buffer
    port60h_events = 0;
  }
}

//STR() functions
//singed integers
qbs *qbs_str(int64 value){
  qbs *tqbs;
  tqbs=qbs_new(20,1);
#ifdef QB64_WINDOWS
  tqbs->len=sprintf((char*)tqbs->chr,"% I64i",value);
#else
  tqbs->len=sprintf((char*)tqbs->chr,"% lli",value);
#endif
  return tqbs;
}
qbs *qbs_str(int32 value){
  qbs *tqbs;
  tqbs=qbs_new(11,1);
  tqbs->len=sprintf((char*)tqbs->chr,"% i",value);
  return tqbs;
}
qbs *qbs_str(int16 value){
  qbs *tqbs;
  tqbs=qbs_new(6,1);
  tqbs->len=sprintf((char*)tqbs->chr,"% i",value);
  return tqbs;
}
qbs *qbs_str(int8 value){
  qbs *tqbs;
  tqbs=qbs_new(4,1);
  tqbs->len=sprintf((char*)tqbs->chr,"% i",value);
  return tqbs;
}
//unsigned integers
qbs *qbs_str(uint64 value){
  qbs *tqbs;
  tqbs=qbs_new(21,1);
#ifdef QB64_WINDOWS
  tqbs->len=sprintf((char*)tqbs->chr," %I64u",value);
#else
  tqbs->len=sprintf((char*)tqbs->chr," %llu",value);
#endif
  return tqbs;
}
qbs *qbs_str(uint32 value){
  qbs *tqbs;
  tqbs=qbs_new(11,1);
  tqbs->len=sprintf((char*)tqbs->chr," %u",value);
  return tqbs;
}
qbs *qbs_str(uint16 value){
  qbs *tqbs;
  tqbs=qbs_new(6,1);
  tqbs->len=sprintf((char*)tqbs->chr," %u",value);
  return tqbs;
}
qbs *qbs_str(uint8 value){
  qbs *tqbs;
  tqbs=qbs_new(4,1);
  tqbs->len=sprintf((char*)tqbs->chr," %u",value);
  return tqbs;
}



uint8 func_str_fmt[7];
uint8 qbs_str_buffer[32];
uint8 qbs_str_buffer2[32];

qbs *qbs_str(float value){
  static qbs *tqbs;
  tqbs=qbs_new(16,1);
  static int32 l,i,i2,i3,digits,exponent;
  l=sprintf((char*)&qbs_str_buffer,"% .6E",value);
  //IMPORTANT: assumed l==14
  if (l==13){memmove(&qbs_str_buffer[12],&qbs_str_buffer[11],2); qbs_str_buffer[11]=48; l=14;}

  digits=7;
  for (i=8;i>=1;i--){
    if (qbs_str_buffer[i]==48){
      digits--;
    }else{
      if (qbs_str_buffer[i]!=46) break;
    }
  }//i
  //no significant digits? simply return 0
  if (digits==0){
    tqbs->len=2; tqbs->chr[0]=32; tqbs->chr[1]=48;//tqbs=[space][0]
    return tqbs;
  }
  //calculate exponent
  exponent=(qbs_str_buffer[11]-48)*100+(qbs_str_buffer[12]-48)*10+(qbs_str_buffer[13]-48);
  if (qbs_str_buffer[10]==45) exponent=-exponent;
  if ((exponent<=6)&&((exponent-digits)>=-8)) goto asdecimal;
  //fix up exponent to conform to QBASIC standards
  //i. cull trailing 0's after decimal point (use digits to help)
  //ii. cull leading 0's of exponent

  i3=0;
  i2=digits+2;
  if (digits==1) i2--;//don't include decimal point
  for (i=0;i<i2;i++) {tqbs->chr[i3]=qbs_str_buffer[i]; i3++;}
  for (i=9;i<=10;i++) {tqbs->chr[i3]=qbs_str_buffer[i]; i3++;}
  exponent=abs(exponent);
  //i2=13;
  //if (exponent>9) i2=12;
  i2=12;//override: if exponent is less than 10 still display a leading 0
  if (exponent>99) i2=11;
  for (i=i2;i<=13;i++) {tqbs->chr[i3]=qbs_str_buffer[i]; i3++;}
  tqbs->len=i3;
  return tqbs;
  /////////////////////
 asdecimal:
  //calculate digits after decimal point in var. i
  i=-(exponent-digits+1);
  if (i<0) i=0;
  func_str_fmt[0]=37;//"%"
  func_str_fmt[1]=32;//" "
  func_str_fmt[2]=46;//"."
  func_str_fmt[3]=i+48;
  func_str_fmt[4]=102;//"f"
  func_str_fmt[5]=0;
  tqbs->len=sprintf((char*)tqbs->chr,(const char*)&func_str_fmt,value);
  if (tqbs->chr[1]==48){//must manually cull leading 0
    memmove(tqbs->chr+1,tqbs->chr+2,tqbs->len-2);
    tqbs->len--;
  }
  return tqbs;
}

qbs *qbs_str(double value){
  static qbs *tqbs;
  tqbs=qbs_new(32,1);
  static int32 l,i,i2,i3,digits,exponent;

  l=sprintf((char*)&qbs_str_buffer,"% .15E",value);
  //IMPORTANT: assumed l==23
  if (l==22){memmove(&qbs_str_buffer[21],&qbs_str_buffer[20],2); qbs_str_buffer[20]=48; l=23;}

  //check if the 16th significant digit is 9, if it is round to 15 significant digits
  if (qbs_str_buffer[17]==57){
    sprintf((char*)&qbs_str_buffer2,"% .14E",value);
    memmove(&qbs_str_buffer,&qbs_str_buffer2,17);
    qbs_str_buffer[17]=48;
  }
  qbs_str_buffer[18]=68; //change E to D (QBASIC standard)
  digits=16;
  for (i=17;i>=1;i--){
    if (qbs_str_buffer[i]==48){
      digits--;
    }else{
      if (qbs_str_buffer[i]!=46) break;
    }
  }//i
  //no significant digits? simply return 0
  if (digits==0){
    tqbs->len=2; tqbs->chr[0]=32; tqbs->chr[1]=48;//tqbs=[space][0]
    return tqbs;
  }
  //calculate exponent
  exponent=(qbs_str_buffer[20]-48)*100+(qbs_str_buffer[21]-48)*10+(qbs_str_buffer[22]-48);
  if (qbs_str_buffer[19]==45) exponent=-exponent;
  //OLD if ((exponent<=15)&&((exponent-digits)>=-16)) goto asdecimal;
  if ((exponent<=15)&&((exponent-digits)>=-17)) goto asdecimal;
  //fix up exponent to conform to QBASIC standards
  //i. cull trailing 0's after decimal point (use digits to help)
  //ii. cull leading 0's of exponent
  i3=0;
  i2=digits+2;
  if (digits==1) i2--;//don't include decimal point
  for (i=0;i<i2;i++) {tqbs->chr[i3]=qbs_str_buffer[i]; i3++;}
  for (i=18;i<=19;i++) {tqbs->chr[i3]=qbs_str_buffer[i]; i3++;}
  exponent=abs(exponent);
  //i2=22;
  //if (exponent>9) i2=21;
  i2=21;//override: if exponent is less than 10 still display a leading 0
  if (exponent>99) i2=20;
  for (i=i2;i<=22;i++) {tqbs->chr[i3]=qbs_str_buffer[i]; i3++;}
  tqbs->len=i3;
  return tqbs;
  /////////////////////
 asdecimal:
  //calculate digits after decimal point in var. i
  i=-(exponent-digits+1);
  if (i<0) i=0;
  func_str_fmt[0]=37;//"%"
  func_str_fmt[1]=32;//" "
  func_str_fmt[2]=46;//"."
  if (i>9){
    func_str_fmt[3]=49;//"1"
    func_str_fmt[4]=(i-10)+48;
  }else{
    func_str_fmt[3]=48;//"0"
    func_str_fmt[4]=i+48;
  }
  func_str_fmt[5]=102;//"f"
  func_str_fmt[6]=0;
  tqbs->len=sprintf((char*)tqbs->chr,(const char*)&func_str_fmt,value);
  if (tqbs->chr[1]==48){//must manually cull leading 0
    memmove(tqbs->chr+1,tqbs->chr+2,tqbs->len-2);
    tqbs->len--;
  }
  return tqbs;
}

qbs *qbs_str(long double value){
  //not fully implemented
  return qbs_str((double)value);
}


int32 qbs_equal(qbs *str1,qbs *str2){
  if (str1->len!=str2->len) return 0;
  if (memcmp(str1->chr,str2->chr,str1->len)==0) return -1;
  return 0;
}
int32 qbs_notequal(qbs *str1,qbs *str2){
  if (str1->len!=str2->len) return -1;
  if (memcmp(str1->chr,str2->chr,str1->len)==0) return 0;
  return -1;
}
int32 qbs_greaterthan(qbs *str2,qbs *str1){
//same process as for lessthan; we just reverse the string order
        int32 i, limit, l1, l2;
    l1 = str1->len; l2 = str2->len;  
        if (!l1) if (l2) return -1; else return 0;
        if (l1<=l2) limit = l1; else limit = l2; 
    i=memcmp(str1->chr,str2->chr,limit); 
        if (i<0) return -1;
        if (i>0) return 0; 
        if (l1<l2) return -1;   
    return 0;
}
int32 qbs_lessthan(qbs *str1,qbs *str2){
  int32 i, limit, l1, l2;
    l1 = str1->len; l2 = str2->len;  //no need to get the length of these strings multiple times.
        if (!l1) if (l2) return -1; else return 0;  //if one is a null string we known the answer already.
        if (l1<=l2) limit = l1; else limit = l2; //our limit is going to be the length of the smallest string.
    i=memcmp(str1->chr,str2->chr,limit); //check only to the length of the shortest string
        if (i<0) return -1; //if the number is smaller by this point, say so
        if (i>0) return 0; // if it's larger by this point, say so
        //if the number is the same at this point, compare length.
        //if the length of the first one is smaller, then the string is smaller. Otherwise the second one is the same string, or longer.
        if (l1<l2) return -1;   
    return 0;
}
int32 qbs_lessorequal(qbs *str1,qbs *str2){
  //same process as lessthan, but we check to see if the lengths are equal here also.
  int32 i, limit, l1, l2;
    l1 = str1->len; l2 = str2->len; 
        if (!l1) return -1;  //if the first string has no length then it HAS to be smaller or equal to the second
        if (l1<=l2) limit = l1; else limit = l2;
    i=memcmp(str1->chr,str2->chr,limit); 
        if (i<0) return -1;
        if (i>0) return 0; 
        if (l1<=l2) return -1;  
    return 0;
}
int32 qbs_greaterorequal(qbs *str2,qbs *str1){
  //same process as for lessorequal; we just reverse the string order
  int32 i, limit, l1, l2;
    l1 = str1->len; l2 = str2->len; 
        if (!l1) return -1;
        if (l1<=l2) limit = l1; else limit = l2;
    i=memcmp(str1->chr,str2->chr,limit); 
        if (i<0) return -1;
        if (i>0) return 0; 
        if (l1<=l2) return -1;  
    return 0;
}

int32 qbs_asc(qbs *str,uint32 i){//uint32 speeds up checking for negative
  i--;
  if (i<str->len){
    return str->chr[i];
  }
  error(5);
  return 0;
}


int32 qbs_asc(qbs *str){
  if (str->len) return str->chr[0];
  error(5);
  return 0;
}

int32 qbs_len(qbs *str){
  return str->len;
}


//QBG BLOCK
int32 qbg_mode=-1;//-1 means not initialized!
int32 qbg_text_only;
//text & graphics modes
int32 qbg_height_in_characters, qbg_width_in_characters;
int32 qbg_top_row, qbg_bottom_row;
int32 qbg_cursor_x, qbg_cursor_y;
int32 qbg_character_height, qbg_character_width;
uint32 qbg_color, qbg_background_color;
//text mode ONLY
int32 qbg_cursor_show;
int32 qbg_cursor_firstvalue, qbg_cursor_lastvalue;//these values need revision
//graphics modes ONLY
int32 qbg_width, qbg_height;
float qbg_x, qbg_y;
int32 qbg_bits_per_pixel, qbg_pixel_mask; //for monochrome modes 1b, for 16 color 1111b, for 256 color 11111111b
int32 qbg_bytes_per_pixel;
int32 qbg_clipping_or_scaling;//1=clipping, 2=clipping and scaling
int32 qbg_view_x1, qbg_view_y1, qbg_view_x2, qbg_view_y2;
int32 qbg_view_offset_x, qbg_view_offset_y;
float qbg_scaling_x, qbg_scaling_y;
float qbg_scaling_offset_x, qbg_scaling_offset_y;
float qbg_window_x1, qbg_window_y1, qbg_window_x2, qbg_window_y2;
int32 qbg_pages;
uint32 *qbg_pageoffsets;
int32 *qbg_cursor_x_previous; //used to recover old cursor position
int32 *qbg_cursor_y_previous;
int32 qbg_active_page;
uint8 *qbg_active_page_offset;
int32 qbg_visual_page;
uint8 *qbg_visual_page_offset;
int32 qbg_color_assign[256];//for modes with quasi palettes!
uint32 pal_mode10[2][9];













uint8 charset8x8[256][8][8];
uint8 charset8x16[256][16][8];

int32 lineclip_draw;//1=draw, 0=don't draw
int32 lineclip_x1,lineclip_y1,lineclip_x2,lineclip_y2;
int32 lineclip_skippixels;//the number of pixels from x1,y1 which won't be drawn

void lineclip(int32 x1,int32 y1,int32 x2,int32 y2,int32 xmin,int32 ymin,int32 xmax,int32 ymax){
  static double mx,my,y,x,d;
  static int32 xdis,ydis;
  lineclip_skippixels=0;


  if (x1>=xmin){ if (x1<=xmax){ if (y1>=ymin){ if (y1<=ymax){//(x1,y1) onscreen?
      if (x1==x2) if (y1==y2) goto singlepoint;//is it a single point? (needed to avoid "division by 0" errors)
      goto gotx1y1;
    }}}}

  //(x1,y1) offscreen...

  if (x1==x2) if (y1==y2){lineclip_draw=0; return;}//offscreen single point

  //ignore entirely offscreen lines requiring no further calculations
  if (x1<xmin) if (x2<xmin){lineclip_draw=0; return;}
  if (x1>xmax) if (x2>xmax){lineclip_draw=0; return;}
  if (y1<ymin) if (y2<ymin){lineclip_draw=0; return;}
  if (y1>ymax) if (y2>ymax){lineclip_draw=0; return;}

  mx=(x2-x1)/fabs((double)(y2-y1)); my=(y2-y1)/fabs((double)(x2-x1));
  //right wall from right
  if (x1>xmax){
    if (mx<0){
      y=(double)y1+((double)x1-(double)xmax)*my;
      if (y>=ymin){ if (y<=ymax){
      //double space indented values calculate pixels to skip
      xdis=x1; ydis=y1;
      x1=xmax; y1=qbr_float_to_long(y);
      xdis=abs(xdis-x1); ydis=abs(ydis-y1);
      if (xdis>=ydis) lineclip_skippixels=xdis; else lineclip_skippixels=ydis;
      goto gotx1y1;
    }}
    }
  }
  //left wall from left
  if (x1<xmin){
    if (mx>0){
      y=(double)y1+((double)xmin-(double)x1)*my;
      if (y>=ymin){ if (y<=ymax){
      //double space indented values calculate pixels to skip
      xdis=x1; ydis=y1;
      x1=xmin; y1=qbr_float_to_long(y);
      xdis=abs(xdis-x1); ydis=abs(ydis-y1);

      if (xdis>=ydis) lineclip_skippixels=xdis; else lineclip_skippixels=ydis;
      goto gotx1y1;
    }}
    }
  }
  //top wall from top
  if (y1<ymin){
    if (my>0){
      x=(double)x1+((double)ymin-(double)y1)*mx;
      if (x>=xmin){ if (x<=xmax){
      //double space indented values calculate pixels to skip
      xdis=x1; ydis=y1;
      x1=qbr_float_to_long(x); y1=ymin;
      xdis=abs(xdis-x1); ydis=abs(ydis-y1);
      if (xdis>=ydis) lineclip_skippixels=xdis; else lineclip_skippixels=ydis;
      goto gotx1y1;
    }}
    }
  }
  //bottom wall from bottom
  if (y1>ymax){
    if (my<0){
      x=(double)x1+((double)y1-(double)ymax)*mx;
      if (x>=xmin){ if (x<=xmax){
      //double space indented values calculate pixels to skip
      xdis=x1; ydis=y1;
      x1=qbr_float_to_long(x); y1=ymax;
      xdis=abs(xdis-x1); ydis=abs(ydis-y1);
      if (xdis>=ydis) lineclip_skippixels=xdis; else lineclip_skippixels=ydis;
      goto gotx1y1;
    }}
    }
  }
  lineclip_draw=0;
  return;
 gotx1y1:

  if (x2>=xmin){ if (x2<=xmax){ if (y2>=ymin){ if (y2<=ymax){
      goto gotx2y2;
    }}}}


  mx=(x1-x2)/fabs((double)(y1-y2)); my=(y1-y2)/fabs((double)(x1-x2));
  //right wall from right
  if (x2>xmax){
    if (mx<0){
      y=(double)y2+((double)x2-(double)xmax)*my;
      if (y>=ymin){ if (y<=ymax){
      x2=xmax; y2=qbr_float_to_long(y);
      goto gotx2y2;
    }}
    }
  }
  //left wall from left
  if (x2<xmin){
    if (mx>0){
      y=(double)y2+((double)xmin-(double)x2)*my;
      if (y>=ymin){ if (y<=ymax){
      x2=xmin; y2=qbr_float_to_long(y);
      goto gotx2y2;
    }}
    }
  }
  //top wall from top
  if (y2<ymin){
    if (my>0){
      x=(double)x2+((double)ymin-(double)y2)*mx;
      if (x>=xmin){ if (x<=xmax){
      x2=qbr_float_to_long(x); y2=ymin;
      goto gotx2y2;
    }}
    }
  }
  //bottom wall from bottom
  if (y2>ymax){
    if (my<0){
      x=(double)x2+((double)y2-(double)ymax)*mx;
      if (x>=xmin){ if (x<=xmax){
      x2=qbr_float_to_long(x); y2=ymax;
      goto gotx2y2;
    }}
    }
  }
  lineclip_draw=0;
  return;
 gotx2y2:
 singlepoint:
  lineclip_draw=1;
  lineclip_x1=x1; lineclip_y1=y1; lineclip_x2=x2; lineclip_y2=y2;


  return;
}

void qbg_palette(uint32 attribute,uint32 col,int32 passed){
  static int32 r,g,b;
  if (new_error) return;
  if (!passed){restorepalette(write_page); return;}

  //32-bit
  if (write_page->bytes_per_pixel==4) goto error;

  attribute&=255;//patch to support QBASIC overflow "bug"

  if ((write_page->compatible_mode==13)||(write_page->compatible_mode==256)){
    if (col&0xFFC0C0C0) goto error;//11111111110000001100000011000000b
    r=col&63; g=(col>>8)&63; b=(col>>16)&63;
    r=qbr((double)r*4.063492f-0.4999999f); g=qbr((double)g*4.063492f-0.4999999f); b=qbr((double)b*4.063492f-0.4999999f);
    write_page->pal[attribute]=b+g*256+r*65536;
    //Upgraded from (((col<<2)&0xFF)<<16)+(((col>>6)&0xFF)<<8)+((col>>14)&0xFF)
    return;
  }

  if (write_page->compatible_mode==12){
    if (attribute>15) goto error;
    if (col&0xFFC0C0C0) goto error;//11111111110000001100000011000000b
    r=col&63; g=(col>>8)&63; b=(col>>16)&63;
    r=qbr((double)r*4.063492f-0.4999999f); g=qbr((double)g*4.063492f-0.4999999f); b=qbr((double)b*4.063492f-0.4999999f);
    write_page->pal[attribute]=b+g*256+r*65536;
    return;
  }

  if (write_page->compatible_mode==11){
    if (attribute>1) goto error;
    if (col&0xFFC0C0C0) goto error;//11111111110000001100000011000000b
    r=col&63; g=(col>>8)&63; b=(col>>16)&63;
    r=qbr((double)r*4.063492f-0.4999999f); g=qbr((double)g*4.063492f-0.4999999f); b=qbr((double)b*4.063492f-0.4999999f);
    write_page->pal[attribute]=b+g*256+r*65536;
    return;
  }

  if (write_page->compatible_mode==10){
    if (attribute>3) goto error;
    if ((col<0)||(col>8)) goto error;
    write_page->pal[attribute+4]=col;
    return;
  }

  if (write_page->compatible_mode==9){
    if (attribute>15) goto error;
    if ((col<0)||(col>63)) goto error;
    write_page->pal[attribute]=palette_64[col];
    return;
  }

  if (write_page->compatible_mode==8){
    if (attribute>15) goto error;
    if ((col<0)||(col>15)) goto error;
    write_page->pal[attribute]=palette_256[col];
    return;
  }

  if (write_page->compatible_mode==7){
    if (attribute>15) goto error;
    if ((col<0)||(col>15)) goto error;
    write_page->pal[attribute]=palette_256[col];
    return;
  }

  if (write_page->compatible_mode==2){
    if (attribute>1) goto error;
    if ((col<0)||(col>15)) goto error;
    write_page->pal[attribute]=palette_256[col];
    return;
  }

  if (write_page->compatible_mode==1){
    if (attribute>15) goto error;
    if ((col<0)||(col>15)) goto error;
    write_page->pal[attribute]=palette_256[col];
    return;
  }

  if (write_page->compatible_mode==0){
    if (attribute>15) goto error;
    if ((col<0)||(col>63)) goto error;
    write_page->pal[attribute]=palette_64[col];
    return;
  }

 error:
  error(5);
  return;

}




void qbg_sub_color(uint32 col1,uint32 col2,uint32 bordercolor,int32 passed){
  if (new_error) return;
  if (!passed){
    //performs no action if nothing passed (as in QBASIC for some modes)
    return;
  }

  if (write_page->compatible_mode==32){
    if (passed&4) goto error;
    if (passed&1) write_page->color=col1;
    if (passed&2) write_page->background_color=col2;
    return;
  }
  if (write_page->compatible_mode==256){
    if (passed&4) goto error;
    if (passed&1) if (col1>255) goto error;
    if (passed&2) if (col2>255) goto error;
    if (passed&1) write_page->color=col1;
    if (passed&2) write_page->background_color=col2;
    return;
  }
  if (write_page->compatible_mode==13){
    //if (passed&6) goto error;
    //if (col1>255) goto error;
    //write_page->color=col1;
    if (passed&4) goto error;
    if (passed&1) if (col1>255) goto error;
    if (passed&2) if (col2>255) goto error;
    if (passed&1) write_page->color=col1;
    if (passed&2) write_page->background_color=col2;
    return;
  }
  if (write_page->compatible_mode==12){
    //if (passed&6) goto error;
    //if (col1>15) goto error;
    //write_page->color=col1;
    if (passed&4) goto error;
    if (passed&1) if (col1>15) goto error;
    if (passed&2) if (col2>15) goto error;
    if (passed&1) write_page->color=col1;
    if (passed&2) write_page->background_color=col2;
    return;
  }
  if (write_page->compatible_mode==11){
    //if (passed&6) goto error;
    //if (col1>1) goto error;
    //write_page->color=col1;
    if (passed&4) goto error;
    if (passed&1) if (col1>1) goto error;
    if (passed&2) if (col2>1) goto error;
    if (passed&1) write_page->color=col1;
    if (passed&2) write_page->background_color=col2;
    return;
  }
  if (write_page->compatible_mode==10){
    if (passed&4) goto error;
    if (passed&1) if (col1>3) goto error;
    if (passed&2) if (col2>8) goto error;
    if (passed&1) write_page->color=col1;
    //if (passed&2) ..._color_assign[0]=col2;
    if (passed&2) write_page->pal[4]=col2;
    return;
  }
  if (write_page->compatible_mode==9){
    if (passed&4) goto error;
    if (passed&1) if (col1>15) goto error;
    if (passed&2) if (col2>63) goto error;
    if (passed&1) write_page->color=col1;
    if (passed&2) write_page->pal[0]=palette_64[col2];
    return;
  }
  if (write_page->compatible_mode==8){
    if (passed&4) goto error;
    if (passed&1) if (col1>15) goto error;
    if (passed&2) if (col2>15) goto error;
    if (passed&1) write_page->color=col1;
    if (passed&2) write_page->pal[0]=palette_256[col2];
    return;
  }
  if (write_page->compatible_mode==7){
    if (passed&4) goto error;
    if (passed&1) if (col1>15) goto error;
    if (passed&2) if (col2>15) goto error;
    if (passed&1) write_page->color=col1;
    if (passed&2) write_page->pal[0]=palette_256[col2];
    return;
  }
  if (write_page->compatible_mode==2){
    if (passed&4) goto error;
    if (passed&1) if (col1>1) goto error;
    if (passed&2) if (col2>15) goto error;
    if (passed&1) write_page->color=col1;
    if (passed&2) write_page->pal[0]=palette_256[col2];
    return;
  }
  if (write_page->compatible_mode==1){
    if (passed&4) goto error;
    if (passed&1){
      if (col1>15) goto error;
      write_page->pal[0]=palette_256[col1];
    }
    if (passed&2){
      if (col2&1){
    write_page->pal[1]=palette_256[3];
    write_page->pal[2]=palette_256[5];
    write_page->pal[3]=palette_256[7];
      }else{
    write_page->pal[1]=palette_256[2];
    write_page->pal[2]=palette_256[4];
    write_page->pal[3]=palette_256[6];
      }
    }
    return;
  }
  if (write_page->compatible_mode==0){
    if (passed&1) if (col1>31) goto error;
    if (passed&2) if (col2>15) goto error;
    if (passed&1) write_page->color=col1;
    if (passed&2) write_page->background_color=col2&7;
    return;
  }
 error:
  error(5);
  return;
}

void defaultcolors(){
  write_page->color=15; write_page->background_color=0;
  if (write_page->compatible_mode==0){write_page->color=7; write_page->background_color=0;}
  if (write_page->compatible_mode==1){write_page->color=3; write_page->background_color=0;}
  if (write_page->compatible_mode==2){write_page->color=1; write_page->background_color=0;}
  if (write_page->compatible_mode==10){write_page->color=3; write_page->background_color=0;}
  if (write_page->compatible_mode==11){write_page->color=1; write_page->background_color=0;}
  if (write_page->compatible_mode==32){write_page->color=0xFFFFFFFF; write_page->background_color=0xFF000000;}
  write_page->draw_color=write_page->color;
  return;
}

//Note: Cannot be used to setup page 0, just to validate it
void validatepage(int32 n){
  static int32 i,i2;
  //add new page indexes if necessary
  if (n>=pages){
    i=n+1;
    page=(int32*)realloc(page,i*4);
    memset(page+pages,0,(i-pages)*4);
    pages=i;
  }
  //create page at index n if none exists
  if (!page[n]){
    //graphical (assumed)
    i=page[0];
    i2=imgnew(img[i].width,img[i].height,img[i].compatible_mode);
    //modify based on page 0's attributes
    //i. link palette to page 0's palette (if necessary)
    if (img[i2].bytes_per_pixel!=4){
      free(img[i2].pal); img[i2].flags^=IMG_FREEPAL;
      img[i2].pal=img[i].pal;
    }
    //ii. set flags
    img[i2].flags|=IMG_SCREEN;
    //iii. inherit font
    selectfont(img[i].font,&img[i2]);
    //text
    //...
    page[n]=i2;
  }
  return;
}//validate_page



void qbg_screen(int32 mode,int32 color_switch,int32 active_page,int32 visual_page,int32 refresh,int32 passed){
  if (new_error) return;

  if (width8050switch){
    if ((passed!=1)||mode) width8050switch=0;
  }

  static int32 i,i2,i3,x,y,f,p;
  static img_struct *im;
  static int32 prev_width_in_characters,prev_height_in_characters;

  static int32 last_active_page=0;//used for active page settings migration

  i=0;//update flags
  //1=mode change required
  //2=page change required (used only to see if an early exit without locking is possible)

  i2=page[0];
  if (passed&1){//mode
    if (mode<0){//custom screen
      i3=-mode;
      if (i3>=nextimg){error(258); return;}//within valid range?
      if (!img[i3].valid){error(258); return;}//valid? 
      if (i3!=i2) i=1; //is mode changing?
    }else{
      if (mode==3) goto error;
      if (mode==4) goto error;
      if (mode==5) goto error;
      if (mode==6) goto error;
      if (mode>13) goto error;
      //is mode changing?
      if (i2){
       if (img[i2].compatible_mode!=mode) i=1;
      }else i=1;
      //force update if special parameters passed
      //(at present, only SCREEN 0 is ever called with these overrides, so handling
      // of these is done only in the SCREEN 0 section of the SCREEN sub)
      if ((sub_screen_width_in_characters!=-1)||(sub_screen_height_in_characters!=-1)||(sub_screen_font!=-1)) i=1;
    }
  }

  if (passed&4){//active page
    if (active_page<0) goto error;
    if (!(passed&8)){//if visual page not specified, set it to the active page
      passed|=8;
      visual_page=active_page;
    }
    if (!(i&1)){//mode not changing
      //validate the passed active page, then see if it is the currently selected page
      validatepage(active_page); i2=page[active_page];
      if ((i2!=read_page_index)||(i2!=write_page_index)) i|=2;
    }
  }//passed&4

  if (passed&8){//visual page
    i3=visual_page;
    if (i3<0) goto error;
    if (!(i&1)){//mode not changing
      validatepage(visual_page); i2=page[visual_page];
      if (i2!=display_page_index) i|=2;
    }
  }//passed&8

  //if no changes need to be made exit before locking
  if (!i) return;

  if (autodisplay){
    if (lock_display_required){//on init of main (), attempting a lock would create an infinite loop
      if (i&1){//avoid locking when only changing the screen page
    if (lock_display==0) lock_display=1;//request lock
    while (lock_display!=2){
      Sleep(0);
    }
      }
    }
  }

  screen_last_valid=0;//ignore cache used to update the screen on next update

  if (passed&1){//mode
    if (i&1){//mode change necessary

      //calculate previous width & height if possible
      prev_width_in_characters=0; prev_height_in_characters=0; 
      if (i=page[0]){//currently in a screen mode?
    im=&img[i];
    if (!im->compatible_mode){
      prev_width_in_characters=im->width; prev_height_in_characters=im->height;
    }else{
      x=fontwidth[im->font]; if (!x) x=1;
      prev_width_in_characters=im->width/x;
      prev_height_in_characters=im->height/fontheight[im->font];
    }
      }//currently in a screen mode


      //free any previously allocated surfaces
      //free pages in reverse order
      if (page[0]){//currently in a screen mode?
    for (i=1;i<pages;i++){
      if(i2=page[i]){
        //manual delete, freeing video pages is usually illegal
        if (img[i2].flags&IMG_FREEMEM) free(img[i2].offset);//free pixel data
        freeimg(i2);
      }//i2
    }//i
    i=page[0];
    if (sub_screen_keep_page0){
      img[i].flags^=IMG_SCREEN;
    }else{
      if (img[i].flags&IMG_FREEMEM) free(img[i].offset);//free pixel data
      if (img[i].flags&IMG_FREEPAL) free(img[i].pal);//free palette
      freeimg(i);
    }
      }//currently in a screen mode
      sub_screen_keep_page0=0;//reset to default status

      pages=1; page[0]=0;

      if (mode<0){//custom screen
    i=-mode;
    page[0]=i; img[i].flags|=IMG_SCREEN; display_page_index=i; display_page=&img[i]; write_page_index=i; write_page=&img[i]; read_page_index=i; read_page=&img[i];
    sub_screen_keep_page0=1;
      }

      //320 x 200 graphics
      //40 x 25 text format, character box size of 8 x 8
      //Assignment of up to 256K colors to up to 256 attributes
      if (mode==13){
    i=imgframe(&cmem[655360],320,200,13);
    memset(img[i].offset,0,320*200);
    page[0]=i; img[i].flags|=IMG_SCREEN; display_page_index=i; display_page=&img[i]; write_page_index=i; write_page=&img[i]; read_page_index=i; read_page=&img[i];
      }//13

      //640 x 480 graphics
      //80 x 30 or 80 x 60 text format, character box size of 8 x 16 or 8 x 8
      //Assignment of up to 256K colors to 16 attributes
      if (mode==12){
    i=imgnew(640,480,12);
    if ((prev_width_in_characters==80)&&(prev_height_in_characters==60)) selectfont(8,&img[i]);//override default font
    page[0]=i; img[i].flags|=IMG_SCREEN; display_page_index=i; display_page=&img[i]; write_page_index=i; write_page=&img[i]; read_page_index=i; read_page=&img[i];
      }//12

      /*
    Screen 11
    \A6 640 x 480 graphics
    \A6 80 x 30 or 80 x 60 text format, character box size of 8 x 16 or 8 x 8
    \A6 Assignment of up to 256K colors to 2 attributes
      */
      if (mode==11){
    i=imgnew(640,480,11);
    if ((prev_width_in_characters==80)&&(prev_height_in_characters==60)) selectfont(8,&img[i]);//override default font
    page[0]=i; img[i].flags|=IMG_SCREEN; display_page_index=i; display_page=&img[i]; write_page_index=i; write_page=&img[i]; read_page_index=i; read_page=&img[i];
      }//11

      //SCREEN 10: 640 x 350 graphics, monochrome monitor only
      //  \A6 80 x 25 or 80 x 43 text format, 8 x 14 or 8 x 8 character box size
      //  \A6 128K page size, page range is 0 (128K) or 0-1 (256K)
      //  \A6 Up to 9 pseudocolors assigned to 4 attributes
      /*
    'colors swap every half second!
    'using PALETTE does NOT swap color indexes
    '0 black-black
    '1 black-grey
    '2 black-white
    '3 grey-black
    '4 grey-grey
    '5 grey-white
    '6 white-black
    '7 white-grey
    '8 white-white
    '*IMPORTANT* QB sets initial values up different to default palette!
    '0 block-black(0)
    '1 grey-grey(4)
    '2 white-black(6)
    '3 white-white(8)
      */
      if (mode==10){
    i=imgnew(640,350,10);
    if ((prev_width_in_characters==80)&&(prev_height_in_characters==43)) selectfont(8,&img[i]);//override default font
    page[0]=i; img[i].flags|=IMG_SCREEN; display_page_index=i; display_page=&img[i]; write_page_index=i; write_page=&img[i]; read_page_index=i; read_page=&img[i];

      }//10

      /*
    SCREEN 9: 640 x 350 graphics
    \A6 80 x 25 or 80 x 43 text format, 8 x 14 or 8 x 8 character box size
    \A6 64K page size, page range is 0 (64K);
    128K page size, page range is 0 (128K) or 0-1 (256K)
    \A6 16 colors assigned to 4 attributes (64K adapter memory), or
    64 colors assigned to 16 attributes (more than 64K adapter memory)
      */
      if (mode==9){
    i=imgnew(640,350,9);
    if ((prev_width_in_characters==80)&&(prev_height_in_characters==43)) selectfont(8,&img[i]);//override default font
    page[0]=i; img[i].flags|=IMG_SCREEN; display_page_index=i; display_page=&img[i]; write_page_index=i; write_page=&img[i]; read_page_index=i; read_page=&img[i];
      }//9

      /*
    SCREEN 8: 640 x 200 graphics
    \A6 80 x 25 text format, 8 x 8 character box
    \A6 64K page size, page ranges are 0 (64K), 0-1 (128K), or 0-3 (246K)
    \A6 Assignment of 16 colors to any of 16 attributes
      */
      if (mode==8){
    i=imgnew(640,200,8);
    page[0]=i; img[i].flags|=IMG_SCREEN; display_page_index=i; display_page=&img[i]; write_page_index=i; write_page=&img[i]; read_page_index=i; read_page=&img[i];
      }//8

      /*
    SCREEN 7: 320 x 200 graphics
    \A6 40 x 25 text format, character box size 8 x 8
    \A6 32K page size, page ranges are 0-1 (64K), 0-3 (128K), or 0-7 (256K)
    \A6 Assignment of 16 colors to any of 16 attributes
      */
      if (mode==7){
    i=imgnew(320,200,7);
    page[0]=i; img[i].flags|=IMG_SCREEN; display_page_index=i; display_page=&img[i]; write_page_index=i; write_page=&img[i]; read_page_index=i; read_page=&img[i];
      }//7

      /*
    SCREEN 4:
    \A6 Supports Olivetti (R) Personal Computers models M24, M240, M28,
    M280, M380, M380/C, M380/T and AT&T (R) Personal Computers 6300
    series
    \A6 640 x 400 graphics
    \A6 80 x 25 text format, 8 x 16 character box
    \A6 1 of 16 colors assigned as the foreground color (selected by the
    COLOR statement); background is fixed at black.
      */
      //Note: QB64 will not support SCREEN 4

      /*
    SCREEN 3: Hercules adapter required, monochrome monitor only
    \A6 720 x 348 graphics
    \A6 80 x 25 text format, 9 x 14 character box
    \A6 2 screen pages (1 only if a second display adapter is installed)
    \A6 PALETTE statement not supported
      */
      //Note: QB64 will not support SCREEN 3

      /*
    SCREEN 2: 640 x 200 graphics
    \A6 80 x 25 text format with character box size of 8 x 8
    \A6 16 colors assigned to 2 attributes with EGA or VGA
      */
      if (mode==2){
    i=imgnew(640,200,2);
    page[0]=i; img[i].flags|=IMG_SCREEN; display_page_index=i; display_page=&img[i]; write_page_index=i; write_page=&img[i]; read_page_index=i; read_page=&img[i];
      }//2

      /*
    SCREEN 1: 320 x 200 graphics
    \A6 40 x 25 text format, 8 x 8 character box
    \A6 16 background colors and one of two sets of 3 foreground colors assigned
    using COLOR statement with CGA
    \A6 16 colors assigned to 4 attributes with EGA or VGA
      */
      if (mode==1){
    i=imgnew(320,200,1);
    page[0]=i; img[i].flags|=IMG_SCREEN; display_page_index=i; display_page=&img[i]; write_page_index=i; write_page=&img[i]; read_page_index=i; read_page=&img[i];
      }//1

      /*
    MDPA, CGA, EGA, or VGA Adapter Boards
    SCREEN 0: Text mode only
    \A6 Either 40 x 25, 40 x 43, 40 x 50, 80 x 25, 80 x 43, or 80 x 50 text format
    with 8 x 8 character box size (8 x 14, 9 x 14, or 9 x 16 with EGA or VGA)
    \A6 16 colors assigned to 2 attributes
    \A6 16 colors assigned to any of 16 attributes (with CGA or EGA)
    \A6 64 colors assigned to any of 16 attributes (with EGA or VGA)
      */
      /*
    granularity from &HB800
    4096 in 80x25
    2048 in 40x25
    6880 in 80x43 (80x43x2=6880)
    3440 in 40x43 (40x43x2=3440)
    8000 in 80x50 (80x50x2=8000)
    4000 in 40x50 (40x50x2=4000)
      */
      if (mode==0){

    if ((sub_screen_width_in_characters!=-1)&&(sub_screen_height_in_characters!=-1)&&(sub_screen_font!=-1)){
      x=sub_screen_width_in_characters; y=sub_screen_height_in_characters; f=sub_screen_font;
      sub_screen_width_in_characters=-1; sub_screen_height_in_characters=-1; sub_screen_font=-1;
      goto gotwidth;
    }
    if (sub_screen_width_in_characters!=-1){
      x=sub_screen_width_in_characters; sub_screen_width_in_characters=-1;
      y=25; f=16;//default
      if (prev_height_in_characters==43){y=43; f=14;}
      if (prev_height_in_characters==50){y=50; f=8;}
      if (x==40) f++;
      goto gotwidth;
    }
    if (sub_screen_height_in_characters!=-1){
      y=sub_screen_height_in_characters; sub_screen_height_in_characters=-1;
      f=16;//default
      if (y==43) f=14;
      if (y==50) f=8;
      x=80;//default
      if (prev_width_in_characters==40){f++; x=40;}
      goto gotwidth;
    }

    if ((prev_width_in_characters==80)&&(prev_height_in_characters==50)){
      x=80; y=50; f=8; goto gotwidth;
    }
    if ((prev_width_in_characters==40)&&(prev_height_in_characters==50)){
      x=40; y=50; f=8+1; goto gotwidth;
    }
    if ((prev_width_in_characters==80)&&(prev_height_in_characters==43)){
      x=80; y=43; f=8; goto gotwidth;
    }
    if ((prev_width_in_characters==40)&&(prev_height_in_characters==43)){
      x=40; y=43; f=8+1; goto gotwidth;
    }
    if ((prev_width_in_characters==40)&&(prev_height_in_characters==25)){
      x=40; y=25; f=16+1; goto gotwidth;
    }
    x=80; y=25; f=16;
      gotwidth:;
    i2=x*y*2;//default granularity
    //specific granularities which cannot be calculated
    if ((x==40)&&(y==25)&&(f=(16+1))) i2=2048;
    if ((x==80)&&(y==25)&&(f=16)) i2=4096;
    p=65536/i2;//number of pages to allocate in cmem
    if (p>8) p=8;//limit cmem pages to 8
    //make sure 8 page indexes exist
    if (7>=pages){
      i=7+1;
      page=(int32*)realloc(page,i*4);
      memset(page+pages,0,(i-pages)*4);
      pages=i;
    }
    for (i3=0;i3<8;i3++){
      if (i3<p){
        i=imgframe(&cmem[753664+i2*i3],x,y,0);
      }else{
        i=imgnew(x,y,0);
      }
      selectfont(f,&img[i]);
      img[i].flags|=IMG_SCREEN;
      page[i3]=i;
    }
    //text-clear 64K after seg. &HB800
    for (i=0;i<65536;i+=2){cmem[753664+i]=32; cmem[753664+i+1]=7;}//init. 64K of memory after B800
    i=page[0];
    display_page_index=i; display_page=&img[i]; write_page_index=i; write_page=&img[i]; read_page_index=i; read_page=&img[i];
      }//0

      write_page->draw_ta=0.0; write_page->draw_scale=1.0; //reset DRAW attributes (of write_page)

      last_active_page=0;

      key_display_redraw=1; key_update();

    }//setmode
  }//passed MODE

  //note: changing the active or visual page reselects the default colors!
  if (passed&4){//SCREEN ?,?,X,? (active_page)
    i=active_page; validatepage(i); i=page[i];
    if ((write_page_index!=i)||(read_page_index!=i)){
      write_page_index=i; write_page=&img[i]; read_page_index=i; read_page=&img[i];
      defaultcolors();
      //reset VIEW PRINT state
      write_page->top_row=1;
      if (!write_page->text) write_page->bottom_row=(write_page->height/write_page->font); else write_page->bottom_row=write_page->height;
      if (write_page->cursor_y>write_page->bottom_row) write_page->cursor_y=1;
      write_page->bottom_row--; if (write_page->bottom_row<=0) write_page->bottom_row=1;

      //active page migration
      //note: transfers any screen settings which are maintained during a QBASIC active page switch
      if (last_active_page!=active_page){
    static img_struct *old_page;
    old_page=&img[page[last_active_page]];
    //WINDOW settings
    /*
      SCREEN 7
      WINDOW (0, 0)-(1, 1)
      SCREEN 7, , 1, 1
      PSET (.5, .5), 14
    */
    //VIEW settings
    /*
      SCREEN 7
      VIEW SCREEN (50, 50)-(100, 100)
      SCREEN 7, , 1, 1
      LINE (0, 0)-(1000, 1000), 1, BF
    */
    //GRAPHICS CURSOR LOCATION
    //(proven)
    //NOT MAINTAINED:
    // X color settings (for both text and graphics)
    // X text cursor location
    // X draw color (reset, as in QBASIC, by defaultcolors())
    if (!write_page->text){
      memcpy(&write_page->apm_p1,&old_page->apm_p1,(uint32)(&write_page->apm_p2-&write_page->apm_p1));
    }
    last_active_page=active_page;
      }//active page migration

    }

  }//passed&4

  if (passed&8){//SCREEN ?,?,?,X (visual_page)
    i=visual_page; validatepage(i); i=page[i];
    if (display_page_index!=i){
      display_page_index=i; display_page=&img[i];
      defaultcolors();
      //reset VIEW PRINT state
      write_page->top_row=1;
      if (!write_page->text) write_page->bottom_row=(write_page->height/write_page->font); else write_page->bottom_row=write_page->height;
      if (write_page->cursor_y>write_page->bottom_row) write_page->cursor_y=1;
      write_page->bottom_row--; if (write_page->bottom_row<=0) write_page->bottom_row=1;

    }
  }//passed&8

  if (autodisplay){
    if (lock_display_required) lock_display=0;//release lock
  }

  return;
 error:
  error(5);
  return;
}//screen (end)

void sub_pcopy(int32 src,int32 dst){
  if (new_error) return;
  static img_struct *s,*d;
  //validate
  if (src>=0){
    validatepage(src); s=&img[page[src]];
  }else{
    src=-src;
    if (src>=nextimg) goto error;
    s=&img[src];
    if (!s->valid) goto error;
  }
  if (dst>=0){
    validatepage(dst); d=&img[page[dst]];
  }else{
    dst=-dst;
    if (dst>=nextimg) goto error;
    d=&img[dst];
    if (!d->valid) goto error;
  }
  if (s==d) return;
  if (s->bytes_per_pixel!=d->bytes_per_pixel) goto error;
  if ((s->height!=d->height)||(s->width!=d->width)) goto error;
  if (s->bytes_per_pixel==1){
    if (d->mask<s->mask) goto error;//cannot copy onto a palette image with less colors
  }
  memcpy(d->offset,s->offset,d->width*d->height*d->bytes_per_pixel);
  return;
 error:

  error(5);
  return;
}

void qbsub_width(int32 option,int32 value1,int32 value2,int32 passed){
  //[{#|LPRINT}][?],[?]
  static int32 i,i2;

  if (new_error) return;

  if (option==0){//WIDTH [?][,?]

    width8050switch=0;

    static uint32 col,col2;

    //used to restore scaling after simple font changes
    //QBASIC/4.5/7.1: PMAP uses old scaling values after WIDTH change
    static float window_x1,window_y1,window_x2,window_y2;

    //Specifics:
    //MODE 0: Changes the resolution based on the desired width
    //        Horizontal width of 1 to 40 uses a double width font
    //        Heights from 1 to 42 use font height 16 pixels
    //        Heights from 43 to 49 use font height 14 pixels
    //        Heights from 50 to ? use font height 8 pixels
    //MODES 1-13: The resolution IS NOT CHANGED
    //            The font is changed to a font usually available for that screen
    //            mode, if available, that fits the given dimensions EXACTLY
    //            If not possible, it may jump back to SCREEN 0 in some instances
    //            just as it did in QBASIC
    //256/32 BIT MODES: The font is unchanged
    //                  The resolution is changed using the currently selected font
    //note:
    //COLOR selection is kept, all other values are lost (if staying in same "mode")
    static int32 f,f2,width,height;

    if ((!(passed&1))&&(!(passed&2))) goto error;//cannot omit both arguments

    width=value1; height=value2;

    if ((write_page->compatible_mode==32)||(write_page->compatible_mode==256)){

      if (!(passed&1)){//width ommited
    width=write_page->width;
      }else{
    if (width<=0) goto error;
    i=fontwidth[write_page->font]; if (!i) i=1;
    width*=i;
      }

      if (!(passed&2)){//height ommited
    height=write_page->height;
      }else{
    if (height<=0) goto error;
    height*=fontheight[write_page->font];
      }

      //width & height are now the desired dimensions

      if ((width==write_page->width)&&(height==write_page->height)) return;//no change required

      if (write_page->flags&IMG_SCREEN){
    //delete pages 1-?
    for(i=1;i<pages;i++){
      if(i2=page[i]){
        if (display_page_index==i2){display_page_index=page[0]; display_page=&img[display_page_index];}
        if (read_page_index==i2){read_page_index=display_page_index; read_page=display_page;}
        if (write_page_index==i2){write_page_index=display_page_index; write_page=display_page;}
        //manual delete, freeing video pages is usually illegal
        if (img[i2].flags&IMG_FREEMEM) free(img[i2].offset);//free pixel data
        freeimg(i2);
      }
    }//i
      }

      if (autodisplay){
    if (write_page->flags&IMG_SCREEN){
      if (lock_display_required){
        if (lock_display==0) lock_display=1;
        while (lock_display!=2){
          Sleep(0);
        }
      }
    }
      }

      col=write_page->color; col2=write_page->background_color;
      f=write_page->font;
      //change resolution
      write_page->width=width; write_page->height=height;
      if (write_page->flags&IMG_FREEMEM){
    free(write_page->offset);//free pixel data
    write_page->offset=(uint8*)calloc(width*height*write_page->bytes_per_pixel,1);
      }else{//frame?
    memset(write_page->offset,0,width*height*write_page->bytes_per_pixel);
      }
      imgrevert(write_page_index);
      write_page->color=col; write_page->background_color=col2;
      selectfont(f,write_page);

      if (autodisplay){
    if (write_page->flags&IMG_SCREEN){
      if (lock_display_required) lock_display=0;//release lock
    }
      }

      return;

    }//32/256

    if (!(passed&1)){//width ommited
      if (height<=0) goto error;

      if (!write_page->compatible_mode){//0
    f=8;
    if (height<=49) f=14;
    if (height<=42) f=16;
    width=write_page->width;
    if (width<=40) f++;
    if ((write_page->font==f)&&(write_page->height==height)) return;//no change
    sub_screen_height_in_characters=height; sub_screen_width_in_characters=width;
    sub_screen_font=f;
    qbg_screen(0,0,0,0,0,1);
    return;
      }//0

      if (((write_page->compatible_mode>=1)&&(write_page->compatible_mode<=8))||(write_page->compatible_mode==13)){
    if (write_page->height==height*8){//correct resolution
      if (write_page->font==8) return;//correct font, no change required
      if (write_page->flags&IMG_SCREEN){
        //delete pages 1-?
        for(i=1;i<pages;i++){
          if(i2=page[i]){
        if (display_page_index==i2){display_page_index=page[0]; display_page=&img[display_page_index];}
        if (read_page_index==i2){read_page_index=display_page_index; read_page=display_page;}
        if (write_page_index==i2){write_page_index=display_page_index; write_page=display_page;}
        //manual delete, freeing video pages is usually illegal
        if (img[i2].flags&IMG_FREEMEM) free(img[i2].offset);//free pixel data
        freeimg(i2);
          }
        }//i
      }
      col=write_page->color; col2=write_page->background_color;
      imgrevert(write_page_index);
      write_page->color=col; write_page->background_color=col2;
      selectfont(8,write_page);
      return;
    }//correct resolution
    //fall through
      }//modes 1-8

      /*
    SCREEN 9: 640 x 350 graphics
    \A6 80 x 25 or 80 x 43 text format, 8 x 14 or 8 x 8 character box size
    \A6 64K page size, page range is 0 (64K);
    128K page size, page range is 0 (128K) or 0-1 (256K)
    \A6 16 colors assigned to 4 attributes (64K adapter memory), or
    64 colors assigned to 16 attributes (more than 64K adapter memory)
    SCREEN 10: 640 x 350 graphics, monochrome monitor only
    \A6 80 x 25 or 80 x 43 text format, 8 x 14 or 8 x 8 character box size
    \A6 128K page size, page range is 0 (128K) or 0-1 (256K)
    \A6 Up to 9 pseudocolors assigned to 4 attributes
      */
      if ((write_page->compatible_mode>=9)&&(write_page->compatible_mode<=10)){
    f=0;
    if(write_page->height==height*8) f=8;
    if(write_page->height==height*14) f=14;
    if((height==43)&&(write_page->height==350)) f=8;//?x350,8x8
    if(f){//correct resolution
      if (write_page->font==f) return;//correct font, no change required
      if (write_page->flags&IMG_SCREEN){
        //delete pages 1-?
        for(i=1;i<pages;i++){
          if(i2=page[i]){
        if (display_page_index==i2){display_page_index=page[0]; display_page=&img[display_page_index];}
        if (read_page_index==i2){read_page_index=display_page_index; read_page=display_page;}
        if (write_page_index==i2){write_page_index=display_page_index; write_page=display_page;}
        //manual delete, freeing video pages is usually illegal
        if (img[i2].flags&IMG_FREEMEM) free(img[i2].offset);//free pixel data
        freeimg(i2);
          }
        }//i
      }
      col=write_page->color; col2=write_page->background_color;
      window_x1=write_page->window_x1; window_x2=write_page->window_x2; window_y1=write_page->window_y1; window_y2=write_page->window_y2;
      imgrevert(write_page_index);
      qbg_sub_window(window_x1,window_y1,window_x2,window_y2,1+2); write_page->clipping_or_scaling=0;
      write_page->color=col; write_page->background_color=col2;
      selectfont(f,write_page);
      return;
    }//correct resolution
    //fall through
      }//modes 9,10

      if ((write_page->compatible_mode>=11)&&(write_page->compatible_mode<=12)){
    f=0;
    if(write_page->height==height*8) f=8;
    if(write_page->height==height*16) f=16;
    if(f){//correct resolution
      if (write_page->font==f) return;//correct font, no change required
      if (write_page->flags&IMG_SCREEN){
        //delete pages 1-?
        for(i=1;i<pages;i++){
          if(i2=page[i]){
        if (display_page_index==i2){display_page_index=page[0]; display_page=&img[display_page_index];}
        if (read_page_index==i2){read_page_index=display_page_index; read_page=display_page;}
        if (write_page_index==i2){write_page_index=display_page_index; write_page=display_page;}
        //manual delete, freeing video pages is usually illegal
        if (img[i2].flags&IMG_FREEMEM) free(img[i2].offset);//free pixel data
        freeimg(i2);
          }
        }//i
      }
      col=write_page->color; col2=write_page->background_color;
      window_x1=write_page->window_x1; window_x2=write_page->window_x2; window_y1=write_page->window_y1; window_y2=write_page->window_y2;
      imgrevert(write_page_index);
      qbg_sub_window(window_x1,window_y1,window_x2,window_y2,1+2); write_page->clipping_or_scaling=0;
      write_page->color=col; write_page->background_color=col2;
      selectfont(f,write_page);
      return;
    }//correct resolution
    //fall through
      }//modes 11,12

      //fall through:
      if ((height==25)||(height==50)||(height==43)){
    sub_screen_height_in_characters=height; qbg_screen(0,0,0,0,0,1);
    return;
      }

      goto error;

    }//width omitted

    if (!(passed&2)){//height omitted

      if (width<=0) goto error;

      if (!write_page->compatible_mode){//0
    height=write_page->height;
    f=8;
    if (height<=49) f=14;
    if (height<=42) f=16;
    if (width<=40) f++;
    if ((write_page->font==f)&&(write_page->width==width)) return;//no change
    sub_screen_height_in_characters=height; sub_screen_width_in_characters=width;
    sub_screen_font=f;
    qbg_screen(0,0,0,0,0,1);
    return;
      }//0

      if (((write_page->compatible_mode>=1)&&(write_page->compatible_mode<=8))||(write_page->compatible_mode==13)){
    if (write_page->width==width*8){//correct resolution
      if (write_page->font==8) return;//correct font, no change required
      if (write_page->flags&IMG_SCREEN){
        //delete pages 1-?
        for(i=1;i<pages;i++){
          if(i2=page[i]){
        if (display_page_index==i2){display_page_index=page[0]; display_page=&img[display_page_index];}
        if (read_page_index==i2){read_page_index=display_page_index; read_page=display_page;}
        if (write_page_index==i2){write_page_index=display_page_index; write_page=display_page;}
        //manual delete, freeing video pages is usually illegal
        if (img[i2].flags&IMG_FREEMEM) free(img[i2].offset);//free pixel data
        freeimg(i2);
          }
        }//i
      }
      col=write_page->color; col2=write_page->background_color;
      imgrevert(write_page_index);
      write_page->color=col; write_page->background_color=col2;
      selectfont(8,write_page);
      return;
    }//correct resolution
    //fall through
      }//modes 1-8

      /*
    SCREEN 9: 640 x 350 graphics
    \A6 80 x 25 or 80 x 43 text format, 8 x 14 or 8 x 8 character box size
    \A6 64K page size, page range is 0 (64K);
    128K page size, page range is 0 (128K) or 0-1 (256K)
    \A6 16 colors assigned to 4 attributes (64K adapter memory), or
    64 colors assigned to 16 attributes (more than 64K adapter memory)
    SCREEN 10: 640 x 350 graphics, monochrome monitor only
    \A6 80 x 25 or 80 x 43 text format, 8 x 14 or 8 x 8 character box size
    \A6 128K page size, page range is 0 (128K) or 0-1 (256K)
    \A6 Up to 9 pseudocolors assigned to 4 attributes
      */
      if ((write_page->compatible_mode>=9)&&(write_page->compatible_mode<=10)){
    f=0;
    if (write_page->width==width*8) f=8;
    if (f){//correct resolution
      f2=fontheight[write_page->font]; if (f2>8) f=14;
      if (write_page->font==f) return;//correct font, no change required
      if (write_page->flags&IMG_SCREEN){
        //delete pages 1-?
        for(i=1;i<pages;i++){
          if(i2=page[i]){
        if (display_page_index==i2){display_page_index=page[0]; display_page=&img[display_page_index];}
        if (read_page_index==i2){read_page_index=display_page_index; read_page=display_page;}
        if (write_page_index==i2){write_page_index=display_page_index; write_page=display_page;}
        //manual delete, freeing video pages is usually illegal
        if (img[i2].flags&IMG_FREEMEM) free(img[i2].offset);//free pixel data
        freeimg(i2);
          }
        }//i
      }
      col=write_page->color; col2=write_page->background_color;
      window_x1=write_page->window_x1; window_x2=write_page->window_x2; window_y1=write_page->window_y1; window_y2=write_page->window_y2;
      imgrevert(write_page_index);
      qbg_sub_window(window_x1,window_y1,window_x2,window_y2,1+2); write_page->clipping_or_scaling=0;
      write_page->color=col; write_page->background_color=col2;
      selectfont(f,write_page);
      return;
    }//correct resolution
    //fall through
      }//modes 9,10

      if ((write_page->compatible_mode>=11)&&(write_page->compatible_mode<=12)){
    f=0;
    if (write_page->width==width*8) f=8;
    if (f){//correct resolution
      f2=fontheight[write_page->font]; if (f2>8) f=16;
      if (write_page->font==f) return;//correct font, no change required
      if (write_page->flags&IMG_SCREEN){
        //delete pages 1-?
        for(i=1;i<pages;i++){
          if(i2=page[i]){
        if (display_page_index==i2){display_page_index=page[0]; display_page=&img[display_page_index];}
        if (read_page_index==i2){read_page_index=display_page_index; read_page=display_page;}
        if (write_page_index==i2){write_page_index=display_page_index; write_page=display_page;}
        //manual delete, freeing video pages is usually illegal
        if (img[i2].flags&IMG_FREEMEM) free(img[i2].offset);//free pixel data
        freeimg(i2);
          }
        }//i
      }
      col=write_page->color; col2=write_page->background_color;
      window_x1=write_page->window_x1; window_x2=write_page->window_x2; window_y1=write_page->window_y1; window_y2=write_page->window_y2;
      imgrevert(write_page_index);
      qbg_sub_window(window_x1,window_y1,window_x2,window_y2,1+2); write_page->clipping_or_scaling=0;
      write_page->color=col; write_page->background_color=col2;
      selectfont(f,write_page);
      return;
    }//correct resolution
    //fall through
      }//modes 11,12

      //fall through:
      if ((width==40)||(width==80)){
    sub_screen_width_in_characters=width;
    qbg_screen(0,0,0,0,0,1);
    return;
      }

      goto error;

    }//height omitted

    //both height & width passed

    if ((width<=0)||(height<=0)) goto error;

    if (!write_page->compatible_mode){//0
      f=8;
      if (height<=49) f=14;
      if (height<=42) f=16;
      if (width<=40) f++;
      if ((write_page->font==f)&&(write_page->width==width)&&(write_page->height==height)) return;//no change
      sub_screen_height_in_characters=height; sub_screen_width_in_characters=width;
      sub_screen_font=f;
      qbg_screen(0,0,0,0,0,1);
      return;
    }//0

    if (((write_page->compatible_mode>=1)&&(write_page->compatible_mode<=8))||(write_page->compatible_mode==13)){
      if ((write_page->width==width*8)&&(write_page->height==height*8)){//correct resolution
    if (write_page->font==8) return;//correct font, no change required
    if (write_page->flags&IMG_SCREEN){
      //delete pages 1-?
      for(i=1;i<pages;i++){
        if(i2=page[i]){
          if (display_page_index==i2){display_page_index=page[0]; display_page=&img[display_page_index];}
          if (read_page_index==i2){read_page_index=display_page_index; read_page=display_page;}
          if (write_page_index==i2){write_page_index=display_page_index; write_page=display_page;}
          //manual delete, freeing video pages is usually illegal
          if (img[i2].flags&IMG_FREEMEM) free(img[i2].offset);//free pixel data
          freeimg(i2);
        }
      }//i
    }
    col=write_page->color; col2=write_page->background_color;
    imgrevert(write_page_index);
    write_page->color=col; write_page->background_color=col2;
    selectfont(8,write_page);
    return;
      }//correct resolution
      //fall through
    }//modes 1-8

    /*
      SCREEN 9: 640 x 350 graphics
      \A6 80 x 25 or 80 x 43 text format, 8 x 14 or 8 x 8 character box size
      \A6 64K page size, page range is 0 (64K);
      128K page size, page range is 0 (128K) or 0-1 (256K)
      \A6 16 colors assigned to 4 attributes (64K adapter memory), or
      64 colors assigned to 16 attributes (more than 64K adapter memory)
      SCREEN 10: 640 x 350 graphics, monochrome monitor only
      \A6 80 x 25 or 80 x 43 text format, 8 x 14 or 8 x 8 character box size
      \A6 128K page size, page range is 0 (128K) or 0-1 (256K)
      \A6 Up to 9 pseudocolors assigned to 4 attributes
    */
    if ((write_page->compatible_mode>=9)&&(write_page->compatible_mode<=10)){
      f=0;
      if (write_page->width==width*8){
    if (write_page->height==height*8) f=8;
    if (write_page->height==height*14) f=14;
    if ((height==43)&&(write_page->height==350)) f=8;//?x350,8x8
      }
      if (f){//correct resolution
    if (write_page->font==f) return;//correct font, no change required
    if (write_page->flags&IMG_SCREEN){
      //delete pages 1-?
      for(i=1;i<pages;i++){
        if(i2=page[i]){
          if (display_page_index==i2){display_page_index=page[0]; display_page=&img[display_page_index];}
          if (read_page_index==i2){read_page_index=display_page_index; read_page=display_page;}
          if (write_page_index==i2){write_page_index=display_page_index; write_page=display_page;}
          //manual delete, freeing video pages is usually illegal
          if (img[i2].flags&IMG_FREEMEM) free(img[i2].offset);//free pixel data
          freeimg(i2);
        }
      }//i
    }
    col=write_page->color; col2=write_page->background_color;
    window_x1=write_page->window_x1; window_x2=write_page->window_x2; window_y1=write_page->window_y1; window_y2=write_page->window_y2;
    imgrevert(write_page_index);
    qbg_sub_window(window_x1,window_y1,window_x2,window_y2,1+2); write_page->clipping_or_scaling=0;
    write_page->color=col; write_page->background_color=col2;
    selectfont(f,write_page);
    return;
      }//correct resolution
      //fall through
    }//modes 9,10

    if ((write_page->compatible_mode>=11)&&(write_page->compatible_mode<=12)){
      f=0;
      if (write_page->width==width*8){

    if (write_page->height==height*8) f=8;

    if (write_page->height==height*16) f=16;
      }
      if (f){//correct resolution
    if (write_page->font==f) return;//correct font, no change required
    if (write_page->flags&IMG_SCREEN){
      //delete pages 1-?
      for(i=1;i<pages;i++){
        if(i2=page[i]){
          if (display_page_index==i2){display_page_index=page[0]; display_page=&img[display_page_index];}
          if (read_page_index==i2){read_page_index=display_page_index; read_page=display_page;}
          if (write_page_index==i2){write_page_index=display_page_index; write_page=display_page;}
          //manual delete, freeing video pages is usually illegal
          if (img[i2].flags&IMG_FREEMEM) free(img[i2].offset);//free pixel data
          freeimg(i2);
        }
      }//i
    }
    col=write_page->color; col2=write_page->background_color;
    window_x1=write_page->window_x1; window_x2=write_page->window_x2; window_y1=write_page->window_y1; window_y2=write_page->window_y2;
    imgrevert(write_page_index);
    qbg_sub_window(window_x1,window_y1,window_x2,window_y2,1+2); write_page->clipping_or_scaling=0;
    write_page->color=col; write_page->background_color=col2;
    selectfont(f,write_page);
    return;
      }//correct resolution
      //fall through
    }//modes 11,12

    //fall through:
    if ((width==40)||(width==80)){
      if ((height==25)||(height==50)||(height==43)){
    sub_screen_width_in_characters=width; sub_screen_height_in_characters=height;
    f=16;
    if (height==43) f=14;
    if (height==50) f=8;
    if (width==40) f++;
    sub_screen_font=f;
    qbg_screen(0,0,0,0,0,1);
    return;
      }

      goto error;

    }//WIDTH [?][,?]

  }//option==0

  if (option==2){//LPRINT
    if (passed!=1) goto error;
    if ((value1<1)||(value1>255)) goto error;
    width_lprint=value1;
    return;
  }//option==2

  //file/device?
  //...

 error:
  error(5);
  return;
}

void pset_and_clip(int32 x,int32 y,uint32 col){

  if ((x>=write_page->view_x1)&&(x<=write_page->view_x2)&&(y>=write_page->view_y1)&&(y<=write_page->view_y2)){

    static uint8 *cp;
    static uint32 *o32;
    static uint32 destcol;
    if (write_page->bytes_per_pixel==1){
      write_page->offset[y*write_page->width+x]=col&write_page->mask;
      return;
    }else{

      if (write_page->alpha_disabled){
    write_page->offset32[y*write_page->width+x]=col;
    return;
      }
      switch(col&0xFF000000){
      case 0xFF000000://100% alpha, so regular pset (fast)
    write_page->offset32[y*write_page->width+x]=col;
    return;
    break;
      case 0x0://0%(0) alpha, so no pset (very fast)
    return;
    break;
      case 0x80000000://~50% alpha (optomized)
    o32=write_page->offset32+(y*write_page->width+x);
    *o32=(((*o32&0xFEFEFE)+(col&0xFEFEFE))>>1)+(ablend128[*o32>>24]<<24);
    return;
    break; 
      case 0x7F000000://~50% alpha (optomized)
    o32=write_page->offset32+(y*write_page->width+x);
    *o32=(((*o32&0xFEFEFE)+(col&0xFEFEFE))>>1)+(ablend127[*o32>>24]<<24);
    return;
    break;
      default://other alpha values (uses a lookup table)
    o32=write_page->offset32+(y*write_page->width+x);
    destcol=*o32;
    cp=cblend+(col>>24<<16);
    *o32=
      cp[(col<<8&0xFF00)+(destcol&255)    ]
      +(cp[(col&0xFF00)   +(destcol>>8&255) ]<<8)
      +(cp[(col>>8&0xFF00)+(destcol>>16&255)]<<16)
      +(ablend[(col>>24)+(destcol>>16&0xFF00)]<<24);
      };
    }

  }//within viewport
  return;
}

void qb32_boxfill(float x1f,float y1f,float x2f,float y2f,uint32 col){
  static int32 x1,y1,x2,y2,i,width,img_width,x,y,d_width,a,a2,v1,v2,v3;
  static uint8 *p,*cp,*cp2,*cp3;
  static uint32 *lp,*lp_last,*lp_first;
  static uint32 *doff32,destcol;

  //resolve coordinates
  if (write_page->clipping_or_scaling){
    if (write_page->clipping_or_scaling==2){
      x1=qbr_float_to_long(x1f*write_page->scaling_x+write_page->scaling_offset_x)+write_page->view_offset_x;
      y1=qbr_float_to_long(y1f*write_page->scaling_y+write_page->scaling_offset_y)+write_page->view_offset_y;
      x2=qbr_float_to_long(x2f*write_page->scaling_x+write_page->scaling_offset_x)+write_page->view_offset_x;
      y2=qbr_float_to_long(y2f*write_page->scaling_y+write_page->scaling_offset_y)+write_page->view_offset_y;
    }else{
      x1=qbr_float_to_long(x1f)+write_page->view_offset_x; y1=qbr_float_to_long(y1f)+write_page->view_offset_y;
      x2=qbr_float_to_long(x2f)+write_page->view_offset_x; y2=qbr_float_to_long(y2f)+write_page->view_offset_y;
    }
  }else{
    x1=qbr_float_to_long(x1f); y1=qbr_float_to_long(y1f);
    x2=qbr_float_to_long(x2f); y2=qbr_float_to_long(y2f);
  }

  //swap coordinates (if necessary)
  if (x1>x2){i=x1; x1=x2; x2=i;}
  if (y1>y2){i=y1; y1=y2; y2=i;}

  //exit without rendering if necessary
  if (x2<write_page->view_x1) return;
  if (x1>write_page->view_x2) return;
  if (y2<write_page->view_y1) return;
  if (y1>write_page->view_y2) return;

  //crop coordinates
  if (x1<write_page->view_x1) x1=write_page->view_x1;
  if (y1<write_page->view_y1) y1=write_page->view_y1;
  if (x1>write_page->view_x2) x1=write_page->view_x2;
  if (y1>write_page->view_y2) y1=write_page->view_y2;
  if (x2<write_page->view_x1) x2=write_page->view_x1;
  if (y2<write_page->view_y1) y2=write_page->view_y1;
  if (x2>write_page->view_x2) x2=write_page->view_x2;
  if (y2>write_page->view_y2) y2=write_page->view_y2;

  if (write_page->bytes_per_pixel==1){
    col&=write_page->mask;
    width=x2-x1+1;
    img_width=write_page->width;
    p=write_page->offset+y1*write_page->width+x1;
    i=y2-y1+1;
  loop:
    memset(p,col,width);
    p+=img_width;
    if (--i) goto loop;
    return;
  }//1

  //assume 32-bit
  //optomized
  //alpha disabled or full alpha?
  a=col>>24;
  if ((write_page->alpha_disabled)||(a==255)){
    width=x2-x1+1;
    y=y2-y1+1;
    img_width=write_page->width;
    //build first line pixel by pixel
    lp_first=write_page->offset32+y1*img_width+x1;
    lp=lp_first-1; lp_last=lp+width;
    while (lp++<lp_last) *lp=col;
    //copy remaining lines
    lp=lp_first;
    width*=4;
    while(y--){
      memcpy(lp,lp_first,width);
      lp+=img_width;
    }
    return;
  }
  //no alpha?
  if (!a) return;
  //half alpha?
  img_width=write_page->width;
  doff32=write_page->offset32+y1*img_width+x1;
  width=x2-x1+1;
  d_width=img_width-width;
  if (a==128){
    col&=0xFEFEFE;
    y=y2-y1+1;
    while(y--){
      x=width;
      while(x--){
    *doff32++=(((*doff32&0xFEFEFE)+col)>>1)+(ablend128[*doff32>>24]<<24);
      }
      doff32+=d_width;
    }
    return;
  }
  if (a==127){
    col&=0xFEFEFE;
    y=y2-y1+1;
    while(y--){
      x=width;
      while(x--){
    *doff32++=(((*doff32&0xFEFEFE)+col)>>1)+(ablend127[*doff32>>24]<<24);
      }
      doff32+=d_width;
    }
    return;
  }
  //ranged alpha
  cp=cblend+(a<<16);
  a2=a<<8;
  cp3=cp+(col>>8&0xFF00);
  cp2=cp+(col&0xFF00);
  cp+=(col<<8&0xFF00);
  y=y2-y1+1;
  while(y--){
    x=width;
    while(x--){
      destcol=*doff32;
      *doff32++=
    cp[destcol&255]
    +(cp2[destcol>>8&255]<<8)
    +(cp3[destcol>>16&255]<<16)
    +(ablend[(destcol>>24)+a2]<<24);
    }
    doff32+=d_width;
  }
  return;
}


void fast_boxfill(int32 x1,int32 y1,int32 x2,int32 y2,uint32 col){
  //assumes:
  //actual coordinates passed
  //left->right, top->bottom order
  //on-screen
  static int32 i,width,img_width,x,y,d_width,a,a2,v1,v2,v3;
  static uint8 *p,*cp,*cp2,*cp3;
  static uint32 *lp,*lp_last,*lp_first;
  static uint32 *doff32,destcol;

  if (write_page->bytes_per_pixel==1){
    col&=write_page->mask;
    width=x2-x1+1;
    img_width=write_page->width;
    p=write_page->offset+y1*write_page->width+x1;
    i=y2-y1+1;
  loop:
    memset(p,col,width);
    p+=img_width;
    if (--i) goto loop;
    return;
  }//1

  //assume 32-bit
  //optomized
  //alpha disabled or full alpha?
  a=col>>24;
  if ((write_page->alpha_disabled)||(a==255)){

    width=x2-x1+1;
    y=y2-y1+1;
    img_width=write_page->width;
    //build first line pixel by pixel
    lp_first=write_page->offset32+y1*img_width+x1;
    lp=lp_first-1; lp_last=lp+width;
    while (lp++<lp_last) *lp=col;
    //copy remaining lines
    lp=lp_first;
    width*=4;
    while(y--){
      memcpy(lp,lp_first,width);
      lp+=img_width;
    }
    return;
  }
  //no alpha?
  if (!a) return;
  //half alpha?
  img_width=write_page->width;
  doff32=write_page->offset32+y1*img_width+x1;
  width=x2-x1+1;
  d_width=img_width-width;
  if (a==128){
    col&=0xFEFEFE;
    y=y2-y1+1;
    while(y--){
      x=width;
      while(x--){
    *doff32++=(((*doff32&0xFEFEFE)+col)>>1)+(ablend128[*doff32>>24]<<24);
      }
      doff32+=d_width;
    }
    return;
  }
  if (a==127){
    col&=0xFEFEFE;
    y=y2-y1+1;
    while(y--){
      x=width;
      while(x--){
    *doff32++=(((*doff32&0xFEFEFE)+col)>>1)+(ablend127[*doff32>>24]<<24);
      }
      doff32+=d_width;
    }
    return;
  }
  //ranged alpha
  cp=cblend+(a<<16);
  a2=a<<8;
  cp3=cp+(col>>8&0xFF00);
  cp2=cp+(col&0xFF00);
  cp+=(col<<8&0xFF00);
  y=y2-y1+1;
  while(y--){
    x=width;
    while(x--){
      destcol=*doff32;
      *doff32++=
    cp[destcol&255]
    +(cp2[destcol>>8&255]<<8)
    +(cp3[destcol>>16&255]<<16)
    +(ablend[(destcol>>24)+a2]<<24);
    }
    doff32+=d_width;
  }
  return;
}






//copied from qb32_line with the following modifications
//i. pre-WINDOW'd & VIEWPORT'd int32 co-ordinates
//ii. all references to style & lineclip_skippixels commented
//iii. declaration of x1,y1,x2,y2,x1f,y1f changed, some declarations removed
void fast_line(int32 x1,int32 y1,int32 x2,int32 y2,uint32 col){
  static int32 l,l2,mi;
  static float m,x1f,y1f;

  lineclip(x1,y1,x2,y2,write_page->view_x1,write_page->view_y1,write_page->view_x2,write_page->view_y2);

  //style=(style&65535)+(style<<16);
  //lineclip_skippixels&=15;
  //style=_lrotl(style,lineclip_skippixels);

  if (lineclip_draw){
    l=abs(lineclip_x1-lineclip_x2);
    l2=abs(lineclip_y1-lineclip_y2);
    if (l>l2){

      //x-axis distance is larger
      y1f=lineclip_y1;
      if (l){//following only applies if drawing more than one pixel
    m=((float)lineclip_y2-(float)lineclip_y1)/(float)l;
    if (lineclip_x2>=lineclip_x1) mi=1; else mi=-1;//direction of change
      }
      l++;
      while (l--){
    if (y1f<0) lineclip_y1=y1f-0.5f; else lineclip_y1=y1f+0.5f;

    //if ((style=_lrotl(style,1))&1){
    pset(lineclip_x1,lineclip_y1,col);
    //}

    lineclip_x1+=mi;
    y1f+=m;
      }

    }else{

      //y-axis distance is larger
      x1f=lineclip_x1;
      if (l2){//following only applies if drawing more than one pixel
    m=((float)lineclip_x2-(float)lineclip_x1)/(float)l2;
    if (lineclip_y2>=lineclip_y1) mi=1; else mi=-1;//direction of change
      }
      l2++;
      while (l2--){
    if (x1f<0) lineclip_x1=x1f-0.5f; else lineclip_x1=x1f+0.5f;
    //if ((style=_lrotl(style,1))&1){
    pset(lineclip_x1,lineclip_y1,col);
    //}
    lineclip_y1+=mi;
    x1f+=m;
      }

    }

  }//lineclip_draw
  return;
}























void qb32_line(float x1f,float y1f,float x2f,float y2f,uint32 col,uint32 style){
  static int32 x1,y1,x2,y2,l,l2,mi;
  static float m;

  //resolve coordinates
  if (write_page->clipping_or_scaling){
    if (write_page->clipping_or_scaling==2){
      x1=qbr_float_to_long(x1f*write_page->scaling_x+write_page->scaling_offset_x)+write_page->view_offset_x;
      y1=qbr_float_to_long(y1f*write_page->scaling_y+write_page->scaling_offset_y)+write_page->view_offset_y;
      x2=qbr_float_to_long(x2f*write_page->scaling_x+write_page->scaling_offset_x)+write_page->view_offset_x;
      y2=qbr_float_to_long(y2f*write_page->scaling_y+write_page->scaling_offset_y)+write_page->view_offset_y;
    }else{
      x1=qbr_float_to_long(x1f)+write_page->view_offset_x; y1=qbr_float_to_long(y1f)+write_page->view_offset_y;
      x2=qbr_float_to_long(x2f)+write_page->view_offset_x; y2=qbr_float_to_long(y2f)+write_page->view_offset_y;
    }
  }else{
    x1=qbr_float_to_long(x1f); y1=qbr_float_to_long(y1f);
    x2=qbr_float_to_long(x2f); y2=qbr_float_to_long(y2f);
  }

  lineclip(x1,y1,x2,y2,write_page->view_x1,write_page->view_y1,write_page->view_x2,write_page->view_y2);

  style=(style&65535)+(style<<16);
  lineclip_skippixels&=15;
  style=_lrotl(style,lineclip_skippixels);

  if (lineclip_draw){
    l=abs(lineclip_x1-lineclip_x2);
    l2=abs(lineclip_y1-lineclip_y2);
    if (l>l2){

      //x-axis distance is larger
      y1f=lineclip_y1;
      if (l){//following only applies if drawing more than one pixel
    m=((float)lineclip_y2-(float)lineclip_y1)/(float)l;
    if (lineclip_x2>=lineclip_x1) mi=1; else mi=-1;//direction of change
      }
      l++;
      while (l--){
    if (y1f<0) lineclip_y1=y1f-0.5f; else lineclip_y1=y1f+0.5f;

    if ((style=_lrotl(style,1))&1){
      pset(lineclip_x1,lineclip_y1,col);
    }

    lineclip_x1+=mi;
    y1f+=m;
      }

    }else{

      //y-axis distance is larger
      x1f=lineclip_x1;
      if (l2){//following only applies if drawing more than one pixel
    m=((float)lineclip_x2-(float)lineclip_x1)/(float)l2;
    if (lineclip_y2>=lineclip_y1) mi=1; else mi=-1;//direction of change
      }
      l2++;
      while (l2--){
    if (x1f<0) lineclip_x1=x1f-0.5f; else lineclip_x1=x1f+0.5f;
    if ((style=_lrotl(style,1))&1){
      pset(lineclip_x1,lineclip_y1,col);
    }
    lineclip_y1+=mi;
    x1f+=m;
      }

    }

  }//lineclip_draw
  return;
}



void sub_line(float x1,float y1,float x2,float y2,uint32 col,int32 bf,uint32 style,int32 passed){
  if (new_error) return;
  if (write_page->text){error(5); return;}
  /*
    '"[[{STEP}](?,?)]-[{STEP}](?,?)[,[?][,[{B|BF}][,?]]]"
    LINE -(10, 10) 'flags: 0
    LINE (0, 0)-(10, 10) 'flags: 1
    LINE -STEP(10, 10) 'flags: 2
    LINE STEP(0, 0)-(10, 10) 'flags: 1+4
  */

  //adjust coordinates and qb graphics cursor position based on STEP
  if (passed&1){
    if (passed&4){x1=write_page->x+x1; y1=write_page->y+y1;}
    write_page->x=x1; write_page->y=y1;
  }else{
    x1=write_page->x; y1=write_page->y;
  }
  if (passed&2){x2=write_page->x+x2; y2=write_page->y+y2;}
  write_page->x=x2; write_page->y=y2;

  if (bf==0){//line
    if ((passed&16)==0) style=0xFFFF;
    if ((passed&8)==0) col=write_page->color;
    write_page->draw_color=col;
    qb32_line(x1,y1,x2,y2,col,style);
    return;
  }

  if (bf==1){//rectangle
    if ((passed&16)==0) style=0xFFFF;
    if ((passed&8)==0) col=write_page->color;
    write_page->draw_color=col;
    qb32_line(x1,y1,x2,y1,col,style);
    qb32_line(x2,y1,x2,y2,col,style);
    qb32_line(x2,y2,x1,y2,col,style);
    qb32_line(x1,y2,x1,y1,col,style);
    return;
  }

  if (bf==2){//filled box
    if ((passed&8)==0) col=write_page->color;
    write_page->draw_color=col;
    qb32_boxfill(x1,y1,x2,y2,col);
    return;
  }

}//sub_line

































//3 paint routines exist for color (not textured) filling
//i) 8-bit
//ii) 32-bit no-alpha
//iii) 32-bit
//simple comparisons are used, the alpha value is part of that comparison in all cases
//even if blending is disabled (a fixed color is likely to have a fixed alpha value anyway),
//and this allows for filling alpha regions

//32-bit WITH BENDING
void sub_paint32(float x,float y,uint32 fillcol,uint32 bordercol,int32 passed){

  //uses 2 buffers, a and b, and swaps between them for reading and creating
  static uint32 a_n=0;
  static uint16 *a_x=(uint16*)malloc(2*65536),*a_y=(uint16*)malloc(2*65536);
  static uint8 *a_t=(uint8*)malloc(65536);
  static uint32 b_n=0;
  static uint16 *b_x=(uint16*)malloc(2*65536),*b_y=(uint16*)malloc(2*65536);
  static uint8 *b_t=(uint8*)malloc(65536);
  static uint8 *done=(uint8*)calloc(640*480,1);
  static int32 ix,iy,i,t,x2,y2;
  static uint32 offset;
  static uint8 *cp;
  static uint16 *sp;
  //overrides
  static int32 done_size=640*480;
  static uint32 *qbg_active_page_offset;//override
  static int32 qbg_width,qbg_view_x1,qbg_view_y1,qbg_view_x2,qbg_view_y2;//override
  static uint32 *doff32,destcol;

  if ((passed&2)==0) fillcol=write_page->color;
  if ((passed&4)==0) bordercol=fillcol;
  write_page->draw_color=fillcol;

  if (passed&1){write_page->x+=x; write_page->y+=y;}else{write_page->x=x; write_page->y=y;}

  if (write_page->clipping_or_scaling){
    if (write_page->clipping_or_scaling==2){
      ix=qbr_float_to_long(write_page->x*write_page->scaling_x+write_page->scaling_offset_x)+write_page->view_offset_x;
      iy=qbr_float_to_long(write_page->y*write_page->scaling_y+write_page->scaling_offset_y)+write_page->view_offset_y;
    }else{
      ix=qbr_float_to_long(write_page->x)+write_page->view_offset_x; iy=qbr_float_to_long(write_page->y)+write_page->view_offset_y;
    }
  }else{
    ix=qbr_float_to_long(write_page->x); iy=qbr_float_to_long(write_page->y);
  }

  //return if offscreen
  if ((ix<write_page->view_x1)||(iy<write_page->view_y1)||(ix>write_page->view_x2)||(iy>write_page->view_y2)){
    return;
  }

  //overrides
  qbg_active_page_offset=write_page->offset32;
  qbg_width=write_page->width;
  qbg_view_x1=write_page->view_x1;
  qbg_view_y1=write_page->view_y1;
  qbg_view_x2=write_page->view_x2;
  qbg_view_y2=write_page->view_y2;
  i=write_page->width*write_page->height;
  if (i>done_size){
    free(done);
    done=(uint8*)calloc(i,1);
  }

  //return if first point is the bordercolor
  if (qbg_active_page_offset[iy*qbg_width+ix]==bordercol) return;

  //create first node
  a_x[0]=ix; a_y[0]=iy;
  a_t[0]=15;
  //types:
  //&1=check left
  //&2=check right
  //&4=check above
  //&8=check below


  a_n=1;
  //qbg_active_page_offset[iy*qbg_width+ix]=fillcol;
  offset=iy*qbg_width+ix;
  //--------plot pixel--------
  doff32=qbg_active_page_offset+offset;
  switch(fillcol&0xFF000000){
  case 0xFF000000:
    *doff32=fillcol;
    break;
  case 0x0:
    doff32;
    break;
  case 0x80000000:
    *doff32=(((*doff32&0xFEFEFE)+(fillcol&0xFEFEFE))>>1)+(ablend128[*doff32>>24]<<24);
    break; 
  case 0x7F000000:
    *doff32=(((*doff32&0xFEFEFE)+(fillcol&0xFEFEFE))>>1)+(ablend127[*doff32>>24]<<24);
    break;
  default:
    destcol=*doff32;
    cp=cblend+(fillcol>>24<<16);
    *doff32=
      cp[(fillcol<<8&0xFF00)+(destcol&255)    ]
      +(cp[(fillcol&0xFF00)   +(destcol>>8&255) ]<<8)
      +(cp[(fillcol>>8&0xFF00)+(destcol>>16&255)]<<16)
      +(ablend[(fillcol>>24)+(destcol>>16&0xFF00)]<<24);
  };//switch
  //--------done plot pixel--------
  done[iy*qbg_width+ix]=1;

 nextpass:
  b_n=0;
  for (i=0;i<a_n;i++){
    t=a_t[i]; ix=a_x[i]; iy=a_y[i];

    //left
    if (t&1){
      x2=ix-1; y2=iy;
      if (x2>=qbg_view_x1){
    offset=y2*qbg_width+x2;
    if (!done[offset]){
      done[offset]=1;
      if (qbg_active_page_offset[offset]!=bordercol){
        //--------plot pixel--------
        doff32=qbg_active_page_offset+offset;
        switch(fillcol&0xFF000000){
        case 0xFF000000:
          *doff32=fillcol;
          break;
        case 0x0:
          doff32;
          break;
        case 0x80000000:
          *doff32=(((*doff32&0xFEFEFE)+(fillcol&0xFEFEFE))>>1)+(ablend128[*doff32>>24]<<24);
          break; 
        case 0x7F000000:
          *doff32=(((*doff32&0xFEFEFE)+(fillcol&0xFEFEFE))>>1)+(ablend127[*doff32>>24]<<24);
          break;
        default:
          destcol=*doff32;
          cp=cblend+(fillcol>>24<<16);
          *doff32=
        cp[(fillcol<<8&0xFF00)+(destcol&255)    ]
        +(cp[(fillcol&0xFF00)   +(destcol>>8&255) ]<<8)
        +(cp[(fillcol>>8&0xFF00)+(destcol>>16&255)]<<16)
        +(ablend[(fillcol>>24)+(destcol>>16&0xFF00)]<<24);
        };//switch
        //--------done plot pixel--------
        b_t[b_n]=13; b_x[b_n]=x2; b_y[b_n]=y2; b_n++;//add new node
      }}}}

    //right
    if (t&2){
      x2=ix+1; y2=iy;
      if (x2<=qbg_view_x2){
    offset=y2*qbg_width+x2;
    if (!done[offset]){
      done[offset]=1;
      if (qbg_active_page_offset[offset]!=bordercol){
        //--------plot pixel--------
        doff32=qbg_active_page_offset+offset;
        switch(fillcol&0xFF000000){
        case 0xFF000000:
          *doff32=fillcol;
          break;
        case 0x0:
          doff32;
          break;
        case 0x80000000:
          *doff32=(((*doff32&0xFEFEFE)+(fillcol&0xFEFEFE))>>1)+(ablend128[*doff32>>24]<<24);
          break; 
        case 0x7F000000:
          *doff32=(((*doff32&0xFEFEFE)+(fillcol&0xFEFEFE))>>1)+(ablend127[*doff32>>24]<<24);
          break;
        default:
          destcol=*doff32;
          cp=cblend+(fillcol>>24<<16);
          *doff32=
        cp[(fillcol<<8&0xFF00)+(destcol&255)    ]
        +(cp[(fillcol&0xFF00)   +(destcol>>8&255) ]<<8)
        +(cp[(fillcol>>8&0xFF00)+(destcol>>16&255)]<<16)
        +(ablend[(fillcol>>24)+(destcol>>16&0xFF00)]<<24);
        };//switch
        //--------done plot pixel--------
        b_t[b_n]=14; b_x[b_n]=x2; b_y[b_n]=y2; b_n++;//add new node
      }}}}

    //above
    if (t&4){
      x2=ix; y2=iy-1;
      if (y2>=qbg_view_y1){
    offset=y2*qbg_width+x2;
    if (!done[offset]){
      done[offset]=1;
      if (qbg_active_page_offset[offset]!=bordercol){
        //--------plot pixel--------
        doff32=qbg_active_page_offset+offset;
        switch(fillcol&0xFF000000){
        case 0xFF000000:
          *doff32=fillcol;
          break;
        case 0x0:
          doff32;
          break;
        case 0x80000000:
          *doff32=(((*doff32&0xFEFEFE)+(fillcol&0xFEFEFE))>>1)+(ablend128[*doff32>>24]<<24);
          break; 
        case 0x7F000000:
          *doff32=(((*doff32&0xFEFEFE)+(fillcol&0xFEFEFE))>>1)+(ablend127[*doff32>>24]<<24);
          break;
        default:
          destcol=*doff32;
          cp=cblend+(fillcol>>24<<16);
          *doff32=
        cp[(fillcol<<8&0xFF00)+(destcol&255)    ]
        +(cp[(fillcol&0xFF00)   +(destcol>>8&255) ]<<8)
        +(cp[(fillcol>>8&0xFF00)+(destcol>>16&255)]<<16)
        +(ablend[(fillcol>>24)+(destcol>>16&0xFF00)]<<24);
        };//switch
        //--------done plot pixel--------
        b_t[b_n]=7; b_x[b_n]=x2; b_y[b_n]=y2; b_n++;//add new node
      }}}}

    //below
    if (t&8){
      x2=ix; y2=iy+1;
      if (y2<=qbg_view_y2){
    offset=y2*qbg_width+x2;
    if (!done[offset]){
      done[offset]=1;
      if (qbg_active_page_offset[offset]!=bordercol){
        //--------plot pixel--------
        doff32=qbg_active_page_offset+offset;
        switch(fillcol&0xFF000000){
        case 0xFF000000:
          *doff32=fillcol;
          break;
        case 0x0:
          doff32;
          break;
        case 0x80000000:
          *doff32=(((*doff32&0xFEFEFE)+(fillcol&0xFEFEFE))>>1)+(ablend128[*doff32>>24]<<24);
          break; 
        case 0x7F000000:
          *doff32=(((*doff32&0xFEFEFE)+(fillcol&0xFEFEFE))>>1)+(ablend127[*doff32>>24]<<24);
          break;
        default:
          destcol=*doff32;
          cp=cblend+(fillcol>>24<<16);
          *doff32=
        cp[(fillcol<<8&0xFF00)+(destcol&255)    ]
        +(cp[(fillcol&0xFF00)   +(destcol>>8&255) ]<<8)
        +(cp[(fillcol>>8&0xFF00)+(destcol>>16&255)]<<16)
        +(ablend[(fillcol>>24)+(destcol>>16&0xFF00)]<<24);
        };//switch
        //--------done plot pixel--------
        b_t[b_n]=11; b_x[b_n]=x2; b_y[b_n]=y2; b_n++;//add new node
      }}}}

  }//i

  //no new nodes?
  if (b_n==0){
    memset(done,0,write_page->width*write_page->height);//cleanup
    return;//finished!
  }

  //swap a & b arrays
  sp=a_x; a_x=b_x; b_x=sp;
  sp=a_y; a_y=b_y; b_y=sp;
  cp=a_t; a_t=b_t; b_t=cp;
  a_n=b_n;

  goto nextpass;
}


//32-bit NO ALPHA BENDING
void sub_paint32x(float x,float y,uint32 fillcol,uint32 bordercol,int32 passed){

  //uses 2 buffers, a and b, and swaps between them for reading and creating
  static uint32 a_n=0;
  static uint16 *a_x=(uint16*)malloc(2*65536),*a_y=(uint16*)malloc(2*65536);
  static uint8 *a_t=(uint8*)malloc(65536);
  static uint32 b_n=0;
  static uint16 *b_x=(uint16*)malloc(2*65536),*b_y=(uint16*)malloc(2*65536);
  static uint8 *b_t=(uint8*)malloc(65536);
  static uint8 *done=(uint8*)calloc(640*480,1);
  static int32 ix,iy,i,t,x2,y2;
  static uint32 offset;
  static uint8 *cp;
  static uint16 *sp;
  //overrides
  static int32 done_size=640*480;
  static uint32 *qbg_active_page_offset;//override
  static int32 qbg_width,qbg_view_x1,qbg_view_y1,qbg_view_x2,qbg_view_y2;//override

  if ((passed&2)==0) fillcol=write_page->color;
  if ((passed&4)==0) bordercol=fillcol;
  write_page->draw_color=fillcol;

  if (passed&1){write_page->x+=x; write_page->y+=y;}else{write_page->x=x; write_page->y=y;}

  if (write_page->clipping_or_scaling){
    if (write_page->clipping_or_scaling==2){
      ix=qbr_float_to_long(write_page->x*write_page->scaling_x+write_page->scaling_offset_x)+write_page->view_offset_x;
      iy=qbr_float_to_long(write_page->y*write_page->scaling_y+write_page->scaling_offset_y)+write_page->view_offset_y;
    }else{
      ix=qbr_float_to_long(write_page->x)+write_page->view_offset_x; iy=qbr_float_to_long(write_page->y)+write_page->view_offset_y;
    }
  }else{
    ix=qbr_float_to_long(write_page->x); iy=qbr_float_to_long(write_page->y);
  }

  //return if offscreen
  if ((ix<write_page->view_x1)||(iy<write_page->view_y1)||(ix>write_page->view_x2)||(iy>write_page->view_y2)){
    return;
  }

  //overrides
  qbg_active_page_offset=write_page->offset32;
  qbg_width=write_page->width;
  qbg_view_x1=write_page->view_x1;
  qbg_view_y1=write_page->view_y1;
  qbg_view_x2=write_page->view_x2;
  qbg_view_y2=write_page->view_y2;
  i=write_page->width*write_page->height;
  if (i>done_size){
    free(done);
    done=(uint8*)calloc(i,1);
  }

  //return if first point is the bordercolor
  if (qbg_active_page_offset[iy*qbg_width+ix]==bordercol) return;

  //create first node
  a_x[0]=ix; a_y[0]=iy;
  a_t[0]=15;
  //types:
  //&1=check left
  //&2=check right
  //&4=check above
  //&8=check below

  a_n=1;
  qbg_active_page_offset[iy*qbg_width+ix]=fillcol;
  done[iy*qbg_width+ix]=1;

 nextpass:
  b_n=0;
  for (i=0;i<a_n;i++){
    t=a_t[i]; ix=a_x[i]; iy=a_y[i];

    //left
    if (t&1){
      x2=ix-1; y2=iy;
      if (x2>=qbg_view_x1){
    offset=y2*qbg_width+x2;
    if (!done[offset]){
      done[offset]=1;
      if (qbg_active_page_offset[offset]!=bordercol){
        qbg_active_page_offset[offset]=fillcol;
        b_t[b_n]=13; b_x[b_n]=x2; b_y[b_n]=y2; b_n++;//add new node
      }}}}

    //right
    if (t&2){
      x2=ix+1; y2=iy;
      if (x2<=qbg_view_x2){
    offset=y2*qbg_width+x2;
    if (!done[offset]){
      done[offset]=1;
      if (qbg_active_page_offset[offset]!=bordercol){
        qbg_active_page_offset[offset]=fillcol;
        b_t[b_n]=14; b_x[b_n]=x2; b_y[b_n]=y2; b_n++;//add new node
      }}}}

    //above
    if (t&4){
      x2=ix; y2=iy-1;
      if (y2>=qbg_view_y1){
    offset=y2*qbg_width+x2;
    if (!done[offset]){
      done[offset]=1;
      if (qbg_active_page_offset[offset]!=bordercol){
        qbg_active_page_offset[offset]=fillcol;
        b_t[b_n]=7; b_x[b_n]=x2; b_y[b_n]=y2; b_n++;//add new node
      }}}}

    //below
    if (t&8){
      x2=ix; y2=iy+1;
      if (y2<=qbg_view_y2){
    offset=y2*qbg_width+x2;
    if (!done[offset]){
      done[offset]=1;
      if (qbg_active_page_offset[offset]!=bordercol){
        qbg_active_page_offset[offset]=fillcol;
        b_t[b_n]=11; b_x[b_n]=x2; b_y[b_n]=y2; b_n++;//add new node
      }}}}

  }//i

  //no new nodes?
  if (b_n==0){
    memset(done,0,write_page->width*write_page->height);//cleanup
    return;//finished!
  }

  //swap a & b arrays
  sp=a_x; a_x=b_x; b_x=sp;
  sp=a_y; a_y=b_y; b_y=sp;
  cp=a_t; a_t=b_t; b_t=cp;
  a_n=b_n;

  goto nextpass;
}



//8-bit (default entry point)
void sub_paint(float x,float y,uint32 fillcol,uint32 bordercol,qbs *backgroundstr,int32 passed){
  if (new_error) return;
  if (write_page->text){error(5); return;}
  if (passed&8){error(5); return;}

  if (write_page->bytes_per_pixel==4){
    if (write_page->alpha_disabled){
      sub_paint32x(x,y,fillcol,bordercol,passed);
      return;
    }else{
      sub_paint32(x,y,fillcol,bordercol,passed);
      return;
    }
  }

  //uses 2 buffers, a and b, and swaps between them for reading and creating
  static uint32 a_n=0;
  static uint16 *a_x=(uint16*)malloc(2*65536),*a_y=(uint16*)malloc(2*65536);
  static uint8 *a_t=(uint8*)malloc(65536);
  static uint32 b_n=0;
  static uint16 *b_x=(uint16*)malloc(2*65536),*b_y=(uint16*)malloc(2*65536);
  static uint8 *b_t=(uint8*)malloc(65536);
  static uint8 *done=(uint8*)calloc(640*480,1);
  static int32 ix,iy,i,t,x2,y2;
  static uint32 offset;
  static uint8 *cp;
  static uint16 *sp;
  //overrides
  static int32 done_size=640*480;
  static uint8 *qbg_active_page_offset;//override
  static int32 qbg_width,qbg_view_x1,qbg_view_y1,qbg_view_x2,qbg_view_y2;//override

  if ((passed&2)==0) fillcol=write_page->color;
  if ((passed&4)==0) bordercol=fillcol;
  fillcol&=write_page->mask;
  bordercol&=write_page->mask;
  write_page->draw_color=fillcol;

  if (passed&1){write_page->x+=x; write_page->y+=y;}else{write_page->x=x; write_page->y=y;}

  if (write_page->clipping_or_scaling){
    if (write_page->clipping_or_scaling==2){
      ix=qbr_float_to_long(write_page->x*write_page->scaling_x+write_page->scaling_offset_x)+write_page->view_offset_x;
      iy=qbr_float_to_long(write_page->y*write_page->scaling_y+write_page->scaling_offset_y)+write_page->view_offset_y;
    }else{
      ix=qbr_float_to_long(write_page->x)+write_page->view_offset_x; iy=qbr_float_to_long(write_page->y)+write_page->view_offset_y;
    }
  }else{
    ix=qbr_float_to_long(write_page->x); iy=qbr_float_to_long(write_page->y);
  }

  //return if offscreen
  if ((ix<write_page->view_x1)||(iy<write_page->view_y1)||(ix>write_page->view_x2)||(iy>write_page->view_y2)){
    return;
  }

  //overrides
  qbg_active_page_offset=write_page->offset;
  qbg_width=write_page->width;
  qbg_view_x1=write_page->view_x1;
  qbg_view_y1=write_page->view_y1;
  qbg_view_x2=write_page->view_x2;
  qbg_view_y2=write_page->view_y2;
  i=write_page->width*write_page->height;
  if (i>done_size){
    free(done);
    done=(uint8*)calloc(i,1);
  }

  //return if first point is the bordercolor
  if (qbg_active_page_offset[iy*qbg_width+ix]==bordercol) return;

  //create first node
  a_x[0]=ix; a_y[0]=iy;
  a_t[0]=15;
  //types:
  //&1=check left
  //&2=check right
  //&4=check above
  //&8=check below

  a_n=1;
  qbg_active_page_offset[iy*qbg_width+ix]=fillcol;
  done[iy*qbg_width+ix]=1;

 nextpass:
  b_n=0;
  for (i=0;i<a_n;i++){
    t=a_t[i]; ix=a_x[i]; iy=a_y[i];

    //left
    if (t&1){
      x2=ix-1; y2=iy;
      if (x2>=qbg_view_x1){
    offset=y2*qbg_width+x2;
    if (!done[offset]){
      done[offset]=1;
      if (qbg_active_page_offset[offset]!=bordercol){
        qbg_active_page_offset[offset]=fillcol;
        b_t[b_n]=13; b_x[b_n]=x2; b_y[b_n]=y2; b_n++;//add new node
      }}}}

    //right
    if (t&2){
      x2=ix+1; y2=iy;
      if (x2<=qbg_view_x2){
    offset=y2*qbg_width+x2;
    if (!done[offset]){
      done[offset]=1;
      if (qbg_active_page_offset[offset]!=bordercol){
        qbg_active_page_offset[offset]=fillcol;
        b_t[b_n]=14; b_x[b_n]=x2; b_y[b_n]=y2; b_n++;//add new node
      }}}}

    //above
    if (t&4){
      x2=ix; y2=iy-1;
      if (y2>=qbg_view_y1){
    offset=y2*qbg_width+x2;
    if (!done[offset]){
      done[offset]=1;
      if (qbg_active_page_offset[offset]!=bordercol){
        qbg_active_page_offset[offset]=fillcol;
        b_t[b_n]=7; b_x[b_n]=x2; b_y[b_n]=y2; b_n++;//add new node
      }}}}

    //below
    if (t&8){
      x2=ix; y2=iy+1;
      if (y2<=qbg_view_y2){
    offset=y2*qbg_width+x2;
    if (!done[offset]){
      done[offset]=1;
      if (qbg_active_page_offset[offset]!=bordercol){
        qbg_active_page_offset[offset]=fillcol;
        b_t[b_n]=11; b_x[b_n]=x2; b_y[b_n]=y2; b_n++;//add new node
      }}}}

  }//i

  //no new nodes?
  if (b_n==0){
    memset(done,0,write_page->width*write_page->height);//cleanup
    return;//finished!
  }

  //swap a & b arrays
  sp=a_x; a_x=b_x; b_x=sp;
  sp=a_y; a_y=b_y; b_y=sp;
  cp=a_t; a_t=b_t; b_t=cp;
  a_n=b_n;

  goto nextpass;

}






void sub_paint(float x,float y,qbs *fillstr,uint32 bordercol,qbs *backgroundstr,int32 passed){
  if (new_error) return;

  //uses 2 buffers, a and b, and swaps between them for reading and creating
  static uint32 fillcol=0;//stub
  static uint32 a_n=0;
  static uint16 *a_x=(uint16*)malloc(2*65536),*a_y=(uint16*)malloc(2*65536);
  static uint8 *a_t=(uint8*)malloc(65536);
  static uint32 b_n=0;
  static uint16 *b_x=(uint16*)malloc(2*65536),*b_y=(uint16*)malloc(2*65536);
  static uint8 *b_t=(uint8*)malloc(65536);
  static uint8 *done=(uint8*)calloc(640*480,1);
  static int32 ix,iy,i,t,x2,y2;
  static uint32 offset;
  static uint8 *cp;
  static uint16 *sp;
  static uint32 backgroundcol;

  if (qbg_text_only){error(5); return;}
  if ((passed&2)==0){error(5); return;}//must be called with this parameter!

  //STEP 1: create the tile in a buffer (tile) using the source string
  static uint8 tilestr[256];
  static uint8 tile[8][64];
  static int32 sx,sy;
  static int32 bytesperrow;
  static int32 row2offset;
  static int32 row3offset;
  static int32 row4offset;
  static int32 byte;
  static int32 bitvalue;
  static int32 c;
  if (fillstr->len==0){error(5); return;}
  if (qbg_bits_per_pixel==4){
    if (fillstr->len>256){error(5); return;}
  }else{
    if (fillstr->len>64){error(5); return;}
  }
  memset(&tilestr[0],0,256);
  memcpy(&tilestr[0],fillstr->chr,fillstr->len);
  sx=8; sy=fillstr->len; //defaults
  if (qbg_bits_per_pixel==8) sx=1;
  if (qbg_bits_per_pixel==4){
    if (fillstr->len&3){
      sy=(fillstr->len-(fillstr->len&3)+4)>>2;
    }else{
      sy=fillstr->len>>2;
    }
    bytesperrow=sx>>3; if (sx&7) bytesperrow++;
    row2offset=bytesperrow;
    row3offset=bytesperrow*2;
    row4offset=bytesperrow*3;
  }
  if (qbg_bits_per_pixel==2) sx=4;
  //use modified "PUT" routine to create the tile
  cp=&tilestr[0];
  {//layer
    static int32 x,y;
    for (y=0;y<sy;y++){
      if (qbg_bits_per_pixel==4){
    bitvalue=128;
    byte=0;
      }
      for (x=0;x<sx;x++){
    //get colour
    if (qbg_bits_per_pixel==8){
      c=*cp;
      cp++;
    }
    if (qbg_bits_per_pixel==4){
      byte=x>>3;
      c=0;
      if (cp[byte]&bitvalue) c|=1;
      if (cp[row2offset+byte]&bitvalue) c|=2;
      if (cp[row3offset+byte]&bitvalue) c|=4;
      if (cp[row4offset+byte]&bitvalue) c|=8;
      bitvalue>>=1; if (bitvalue==0) bitvalue=128;
    }
    if (qbg_bits_per_pixel==1){
      if (!(x&7)){
        byte=*cp;
        cp++;
      }
      c=(byte&128)>>7; byte<<=1;
    }
    if (qbg_bits_per_pixel==2){
      if (!(x&3)){
        byte=*cp;
        cp++;
      }
      c=(byte&192)>>6; byte<<=2;
    }
    //"pset" color
    tile[x][y]=c;
      }//x
      if (qbg_bits_per_pixel==4) cp+=(bytesperrow*4);
      if (qbg_bits_per_pixel==1){
    if (sx&7) cp++;
      }
      if (qbg_bits_per_pixel==2){
    if (sx&3) cp++;
      }
    }//y
  }//unlayer
  //tile created!

  //STEP 2: establish border and background colors
  if ((passed&4)==0) bordercol=qbg_color;
  bordercol&=qbg_pixel_mask;

  backgroundcol=0;//default
  if (passed&8){
    if (backgroundstr->len==0){error(5); return;}
    if (backgroundstr->len>255){error(5); return;}
    if (qbg_bits_per_pixel==1){
      c=backgroundstr->chr[0];
      if ((c>0)&&(c<255)) backgroundcol=-1;//unclear definition
      if (c==255) backgroundcol=1;
    }
    if (qbg_bits_per_pixel==2){
      backgroundcol=-1;//unclear definition
      x2=backgroundstr->chr[0];
      y2=x2&3;
      x2>>=2; if ((x2&3)!=y2) goto uncleardef;
      x2>>=2; if ((x2&3)!=y2) goto uncleardef;

      x2>>=2; if ((x2&3)!=y2) goto uncleardef;

      backgroundcol=y2;
    }
    if (qbg_bits_per_pixel==4){
      backgroundcol=-1;//unclear definition
      y2=0;
      x2=4; if (backgroundstr->len<4) x2=backgroundstr->len;
      c=0; memcpy(&c,backgroundstr->chr,x2);
      x2=c&255; c>>=8; if ((x2!=0)&&(x2!=255)) goto uncleardef;
      y2|=(x2&1);
      x2=c&255; c>>=8; if ((x2!=0)&&(x2!=255)) goto uncleardef;
      y2|=((x2&1)<<1);
      x2=c&255; c>>=8; if ((x2!=0)&&(x2!=255)) goto uncleardef;
      y2|=((x2&1)<<2);
      x2=c&255; c>>=8; if ((x2!=0)&&(x2!=255)) goto uncleardef;
      y2|=((x2&1)<<3);
      backgroundcol=y2;
    }
    if (qbg_bits_per_pixel==8){
      backgroundcol=backgroundstr->chr[0];
    }
  }
 uncleardef:

  //STEP 3: perform tile'd fill
  if (passed&1){qbg_x+=x; qbg_y+=y;}else{qbg_x=x; qbg_y=y;}
  if (qbg_clipping_or_scaling){
    if (qbg_clipping_or_scaling==2){
      ix=qbr_float_to_long(qbg_x*qbg_scaling_x+qbg_scaling_offset_x)+qbg_view_offset_x;
      iy=qbr_float_to_long(qbg_y*qbg_scaling_y+qbg_scaling_offset_y)+qbg_view_offset_y;
    }else{
      ix=qbr_float_to_long(qbg_x)+qbg_view_offset_x; iy=qbr_float_to_long(qbg_y)+qbg_view_offset_y;
    }
  }else{
    ix=qbr_float_to_long(qbg_x); iy=qbr_float_to_long(qbg_y);
  }

  //return if offscreen
  if ((ix<qbg_view_x1)||(iy<qbg_view_y1)||(ix>qbg_view_x2)||(iy>qbg_view_y2)){
    return;
  }

  offset=iy*qbg_width+ix;

  //return if first point is the bordercolor
  if (qbg_active_page_offset[offset]==bordercol) return;

  //return if first point is the same as the tile color used and is not the background color
  fillcol=tile[ix%sx][iy%sy];
  if ((fillcol==qbg_active_page_offset[offset])&&(fillcol!=backgroundcol)) return;
  qbg_active_page_offset[offset]=fillcol;




  //create first node
  a_x[0]=ix; a_y[0]=iy;
  a_t[0]=15;
  //types:
  //&1=check left
  //&2=check right
  //&4=check above
  //&8=check below

  a_n=1;
  qbg_active_page_offset[iy*qbg_width+ix]=fillcol;
  done[iy*qbg_width+ix]=1;

 nextpass:
  b_n=0;
  for (i=0;i<a_n;i++){
    t=a_t[i]; ix=a_x[i]; iy=a_y[i];

    //left
    if (t&1){
      x2=ix-1; y2=iy;
      if (x2>=qbg_view_x1){
    offset=y2*qbg_width+x2;
    if (!done[offset]){
      done[offset]=1;
      if (qbg_active_page_offset[offset]!=bordercol){
        fillcol=tile[x2%sx][y2%sy];
        //no tile check required when moving horizontally!
        qbg_active_page_offset[offset]=fillcol;
        b_t[b_n]=13; b_x[b_n]=x2; b_y[b_n]=y2; b_n++;//add new node
      }}}}

    //right
    if (t&2){
      x2=ix+1; y2=iy;
      if (x2<=qbg_view_x2){
    offset=y2*qbg_width+x2;
    if (!done[offset]){
      done[offset]=1;
      if (qbg_active_page_offset[offset]!=bordercol){
        fillcol=tile[x2%sx][y2%sy];
        //no tile check required when moving horizontally!
        qbg_active_page_offset[offset]=fillcol;
        b_t[b_n]=14; b_x[b_n]=x2; b_y[b_n]=y2; b_n++;//add new node
      }}}}

    //above
    if (t&4){
      x2=ix; y2=iy-1;
      if (y2>=qbg_view_y1){
    offset=y2*qbg_width+x2;
    if (!done[offset]){
      done[offset]=1;
      if (qbg_active_page_offset[offset]!=bordercol){
        fillcol=tile[x2%sx][y2%sy];
        if ((fillcol!=qbg_active_page_offset[offset])||(fillcol==backgroundcol)){
          qbg_active_page_offset[offset]=fillcol;
          b_t[b_n]=7; b_x[b_n]=x2; b_y[b_n]=y2; b_n++;//add new node
        }
      }}}}

    //below
    if (t&8){
      x2=ix; y2=iy+1;
      if (y2<=qbg_view_y2){
    offset=y2*qbg_width+x2;
    if (!done[offset]){
      done[offset]=1;
      if (qbg_active_page_offset[offset]!=bordercol){
        fillcol=tile[x2%sx][y2%sy];
        if ((fillcol!=qbg_active_page_offset[offset])||(fillcol==backgroundcol)){
          qbg_active_page_offset[offset]=fillcol;
          b_t[b_n]=11; b_x[b_n]=x2; b_y[b_n]=y2; b_n++;//add new node
        }
      }}}}

  }//i

  //no new nodes?
  if (b_n==0){
    memset(done,0,qbg_width*qbg_height);//cleanup
    return;//finished!
  }

  //swap a & b arrays
  sp=a_x; a_x=b_x; b_x=sp;
  sp=a_y; a_y=b_y; b_y=sp;
  cp=a_t; a_t=b_t; b_t=cp;
  a_n=b_n;

  goto nextpass;

}










void sub_circle(double x,double y,double r,uint32 col,double start,double end,double aspect,int32 passed){
  //                                                &2         &4           &8         &16
  //[{STEP}](?,?),?[,[?][,[?][,[?][,?]]]]
  if (new_error) return;



  //data
  static double pi= 3.1415926535897932,pi2=6.2831853071795865;
  static int32 line_to_start,line_from_end;
  static int32 ix,iy;//integer screen co-ordinates of circle's centre
  static double xspan,yspan;
  static double c;//circumference
  static double px,py;
  static double sinb,cosb;//second angle used in double-angle-formula
  static int32 pixels;
  static double tmp;
  static int32 tmpi;
  static int32 i;
  static int32 exclusive;
  static double arc1,arc2,arc3,arc4,arcinc;
  static double px2,py2;
  static int32 x2,y2;
  static int32 x3,y3;
  static int32 lastplotted_x2,lastplotted_y2;
  static int32 lastchecked_x2,lastchecked_y2;

  if (write_page->text){error(5); return;}

  //lines to & from centre
  if (!(passed&4)) start=0;
  if (!(passed&8)) end=pi2;
  line_to_start=0; if (start<0) {line_to_start=1; start=-start;}
  line_from_end=0; if (end<0) {line_from_end=1; end=-end;}

  //error checking
  if (start>pi2){error(5); return;}
  if (end>pi2){error(5); return;}

  //when end<start, the arc of the circle that wouldn't have been drawn if start & end
  //were swapped is drawn
  exclusive=0;
  if (end<start){
    tmp=start; start=end; end=tmp;
    tmpi=line_to_start; line_to_start=line_from_end; line_from_end=tmpi;
    exclusive=1;
  }

  //calc. centre
  if (passed&1){x=write_page->x+x; y=write_page->y+y;}
  write_page->x=x; write_page->y=y;//set graphics cursor position to circle's centre



  r=x+r;//the differece between x & x+r in pixels will be the radius in pixels
  //resolve coordinates (but keep as floats)
  if (write_page->clipping_or_scaling){
    if (write_page->clipping_or_scaling==2){
      x=x*write_page->scaling_x+write_page->scaling_offset_x+write_page->view_offset_x;
      y=y*write_page->scaling_y+write_page->scaling_offset_y+write_page->view_offset_y;
      r=r*write_page->scaling_x+write_page->scaling_offset_x+write_page->view_offset_x;
    }else{
      x=x+write_page->view_offset_x;
      y=y+write_page->view_offset_y;
      r=r+write_page->view_offset_x;
    }
  }
  if (x<0) ix=x-0.5; else ix=x+0.5;
  if (y<0) iy=y-0.5; else iy=y+0.5;
  r=fabs(r-x);//r is now a radius in pixels



  //adjust vertical and horizontal span of the circle based on aspect ratio
  xspan=r; yspan=r;
  if (!(passed&16)){
    aspect=1;//Note: default aspect ratio is 1:1 for QB64 specific modes (256/32)
    if (write_page->compatible_mode==1) aspect=4.0*(200.0/320.0)/3.0;
    if (write_page->compatible_mode==2) aspect=4.0*(200.0/640.0)/3.0;
    if (write_page->compatible_mode==7) aspect=4.0*(200.0/320.0)/3.0;
    if (write_page->compatible_mode==8) aspect=4.0*(200.0/640.0)/3.0;
    if (write_page->compatible_mode==9) aspect=4.0*(350.0/640.0)/3.0;
    if (write_page->compatible_mode==10) aspect=4.0*(350.0/640.0)/3.0;
    if (write_page->compatible_mode==11) aspect=4.0*(480.0/640.0)/3.0;
    if (write_page->compatible_mode==12) aspect=4.0*(480.0/640.0)/3.0;
    if (write_page->compatible_mode==13) aspect=4.0*(200.0/320.0)/3.0;
    //Old method: aspect=4.0*((double)write_page->height/(double)write_page->width)/3.0;
  }
  if (aspect>=0){
    if (aspect<1){
      //aspect: 0 to 1
      yspan*=aspect;
    }
    if (aspect>1){
      //aspect: 1 to infinity
      xspan/=aspect;
    }
  }else{
    if (aspect>-1){
      //aspect: -1 to 0
      yspan*=(1+aspect);
    }
    //if aspect<-1 no change is required
  }

  //skip everything if none of the circle is inside current viwport
  if ((x+xspan+0.5)<write_page->view_x1) return;
  if ((y+yspan+0.5)<write_page->view_y1) return;
  if ((x-xspan-0.5)>write_page->view_x2) return;
  if ((y-yspan-0.5)>write_page->view_y2) return;

  if (!(passed&2)) col=write_page->color;
  write_page->draw_color=col;

  //pre-set/pre-calculate values
  c=pi2*r;
  pixels=c/4.0+0.5;
  arc1=0;
  arc2=pi;
  arc3=pi;
  arc4=pi2;
  arcinc=(pi/2)/(double)pixels;
  sinb=sin(arcinc);
  cosb=cos(arcinc);
  lastplotted_x2=-1;
  lastchecked_x2=-1;
  i=0;

  if (line_to_start){
    px=cos(start); py=sin(start);
    x2=px*xspan+0.5; y2=py*yspan-0.5;
    fast_line(ix,iy,ix+x2,iy-y2,col);
  }

  px=1;
  py=0;

 drawcircle:
  x2=px*xspan+0.5;
  y2=py*yspan-0.5;

  if (i==0) {lastchecked_x2=x2; lastchecked_y2=y2; goto plot;}

  if ( (abs(x2-lastplotted_x2)>=2)||(abs(y2-lastplotted_y2)>=2) ){
  plot:
    if (exclusive){
      if ((arc1<=start)||(arc1>=end)){pset_and_clip(ix+lastchecked_x2,iy+lastchecked_y2,col);}
      if ((arc2<=start)||(arc2>=end)){pset_and_clip(ix-lastchecked_x2,iy+lastchecked_y2,col);}
      if ((arc3<=start)||(arc3>=end)){pset_and_clip(ix-lastchecked_x2,iy-lastchecked_y2,col);}
      if ((arc4<=start)||(arc4>=end)){pset_and_clip(ix+lastchecked_x2,iy-lastchecked_y2,col);}
    }else{//inclusive
      if ((arc1>=start)&&(arc1<=end)){pset_and_clip(ix+lastchecked_x2,iy+lastchecked_y2,col);}
      if ((arc2>=start)&&(arc2<=end)){pset_and_clip(ix-lastchecked_x2,iy+lastchecked_y2,col);}
      if ((arc3>=start)&&(arc3<=end)){pset_and_clip(ix-lastchecked_x2,iy-lastchecked_y2,col);}
      if ((arc4>=start)&&(arc4<=end)){pset_and_clip(ix+lastchecked_x2,iy-lastchecked_y2,col);}
    }
    if (i>pixels) goto allplotted;
    lastplotted_x2=lastchecked_x2; lastplotted_y2=lastchecked_y2;
  }
  lastchecked_x2=x2; lastchecked_y2=y2;

  if (i<=pixels){
    i++;
    if (i>pixels) goto plot;
    px2=px*cosb+py*sinb;
    py=py*cosb-px*sinb;
    px=px2;
    if (i) {arc1+=arcinc; arc2-=arcinc; arc3+=arcinc; arc4-=arcinc;}
    goto drawcircle;
  }
 allplotted:

  if (line_from_end){
    px=cos(end); py=sin(end);
    x2=px*xspan+0.5; y2=py*yspan-0.5;
    fast_line(ix,iy,ix+x2,iy-y2,col);
  }

}//sub_circle



uint32 point(int32 x,int32 y){//does not clip!
  if (read_page->bytes_per_pixel==1){
    return read_page->offset[y*read_page->width+x]&read_page->mask;
  }else{
    return read_page->offset32[y*read_page->width+x];
  }
  return NULL;
}




double func_point(float x,float y,int32 passed){
  static int32 x2,y2,i;

  if (!passed){
    if (write_page->text){error(5); return 0;}
    i=qbr_float_to_long(x);
    if ((i<0)||(i>3)){error(5); return 0;}
    switch(i){
    case 0:
      if (write_page->clipping_or_scaling==2){
    return qbr_float_to_long(write_page->x*write_page->scaling_x+write_page->scaling_offset_x);
      }
      return qbr_float_to_long(write_page->x);
      break;
    case 1:
      if (write_page->clipping_or_scaling==2){
    return qbr_float_to_long(write_page->y*write_page->scaling_y+write_page->scaling_offset_y);
      }
      return qbr_float_to_long(write_page->y);
      break;
    case 2:
      return write_page->x;
      break;
    case 3:
      return write_page->y;
      break;
    default:
      error(5);
      return 0;
    }
  }//!passed

  if (read_page->text){error(5); return 0;}
  if (read_page->clipping_or_scaling){
    if (read_page->clipping_or_scaling==2){
      x2=qbr_float_to_long(x*read_page->scaling_x+read_page->scaling_offset_x)+read_page->view_offset_x;
      y2=qbr_float_to_long(y*read_page->scaling_y+read_page->scaling_offset_y)+read_page->view_offset_y;
    }else{
      x2=qbr_float_to_long(x)+read_page->view_offset_x; y2=qbr_float_to_long(y)+read_page->view_offset_y;
    }
  }else{
    x2=qbr_float_to_long(x); y2=qbr_float_to_long(y);
  }
  if (x2>=read_page->view_x1){ 
    if (x2<=read_page->view_x2){
      if (y2>=read_page->view_y1){ 
    if (y2<=read_page->view_y2){
      return point(x2,y2);
    }
      }
    }
  }
  return -1;
}








void sub_pset(float x,float y,uint32 col,int32 passed){
  if (new_error) return;
  static int32 x2,y2;
  if (!write_page->compatible_mode){error(5); return;}
  //Special Format: [{STEP}](?,?),[?]
  if (passed&1){write_page->x+=x; write_page->y+=y;}else{write_page->x=x; write_page->y=y;}
  if (!(passed&2)) col=write_page->color;
  write_page->draw_color=col;
  if (write_page->clipping_or_scaling){
    if (write_page->clipping_or_scaling==2){
      x2=qbr(write_page->x*write_page->scaling_x+write_page->scaling_offset_x)+write_page->view_offset_x;
      y2=qbr(write_page->y*write_page->scaling_y+write_page->scaling_offset_y)+write_page->view_offset_y;
    }else{
      x2=qbr(write_page->x)+write_page->view_offset_x; y2=qbr(write_page->y)+write_page->view_offset_y;
    }
    if (x2>=write_page->view_x1){ if (x2<=write_page->view_x2){
    if (y2>=write_page->view_y1){ if (y2<=write_page->view_y2){
        pset(x2,y2,col);
      }}}}
    return;
  }else{
    x2=qbr(write_page->x); if (x2>=0){ if (x2<write_page->width){
    y2=qbr(write_page->y); if (y2>=0){ if (y2<write_page->height){
        pset(x2,y2,col);
      }}}}
  }
  return;
}

void sub_preset(float x,float y,uint32 col,int32 passed){
  if (new_error) return;
  if (!(passed&2)){
    col=write_page->background_color;
    passed|=2;
  }
  sub_pset(x,y,col,passed);
  return;
}


int32 img_printchr=0;
int32 img_printchr_i;
int32 img_printchr_x;
int32 img_printchr_y;
char *img_printchr_offset;

void printchr(int32 character){
  static uint32 x,x2,y,y2,w,h,z,z2,z3,a,a2,a3,color,background_color,f;
  static uint32 *lp;
  static uint8 *cp;
  static img_struct *im;

  im=write_page;
  color=im->color;
  background_color=im->background_color;


  if (im->text){
    im->offset[(((im->cursor_y-1)*im->width+im->cursor_x-1))<<1]=character;
    im->offset[((((im->cursor_y-1)*im->width+im->cursor_x-1))<<1)+1]=(color&0xF)+background_color*16+(color&16)*8;
    return;
  }

  //precalculations

  f=im->font;
  x=fontwidth[f]; if (x) x*=(im->cursor_x-1); else x=im->cursor_x-1;
  y=(im->cursor_y-1)*fontheight[f];
  h=fontheight[f];
  if ((fontflags[f]&32)==0) character&=255;//unicodefontsupport

  //if (mode==1) img[i].print_mode=3;//fill
  //if (mode==2) img[i].print_mode=1;//keep
  //if (mode==3) img[i].print_mode=2;//only

  if (f>=32){//custom font

    //8-bit / alpha-disabled 32-bit / dont-blend(alpha may still be applied)
    if ((im->bytes_per_pixel==1)||((im->bytes_per_pixel==4)&&(im->alpha_disabled))||(fontflags[f]&8)){

      //render character
      static int32 ok;
      static uint8 *rt_data;
      static int32 rt_w,rt_h,rt_pre_x,rt_post_x;
      //int32 FontRenderTextASCII(int32 i,uint8*codepoint,int32 codepoints,int32 options,
      //                          uint8**out_data,int32*out_x,int32 *out_y,int32*out_x_pre_increment,int32*out_x_post_increment){
      ok=FontRenderTextASCII(font[f],(uint8*)&character,1,1,
                 &rt_data,&rt_w,&rt_h,&rt_pre_x,&rt_post_x);
      if (!ok) return;

      w=rt_w;

      switch(im->print_mode){
      case 3:
    for (y2=0;y2<h;y2++){
      cp=rt_data+y2*w;
      for (x2=0;x2<w;x2++){
        if (*cp++) pset(x+x2,y+y2,color); else pset(x+x2,y+y2,background_color);
      }}
    break;
      case 1:
    for (y2=0;y2<h;y2++){
      cp=rt_data+y2*w;
      for (x2=0;x2<w;x2++){
        if (*cp++) pset(x+x2,y+y2,color);
      }}
    break;
      case 2:
    for (y2=0;y2<h;y2++){
      cp=rt_data+y2*w;
      for (x2=0;x2<w;x2++){
        if (!(*cp++)) pset(x+x2,y+y2,background_color);
      }}
    break;
      default:
    break;
      }

      free(rt_data);
      return;
    }//1-8 bit
    //assume 32-bit blended

    a=(color>>24)+1; a2=(background_color>>24)+1;
    z=color&0xFFFFFF; z2=background_color&0xFFFFFF;

    //render character
    static int32 ok;
    static uint8 *rt_data;
    static int32 rt_w,rt_h,rt_pre_x,rt_post_x;
    //int32 FontRenderTextASCII(int32 i,uint8*codepoint,int32 codepoints,int32 options,
    //                          uint8**out_data,int32*out_x,int32 *out_y,int32*out_x_pre_increment,int32*out_x_post_increment){
    ok=FontRenderTextASCII(font[f],(uint8*)&character,1,0,
               &rt_data,&rt_w,&rt_h,&rt_pre_x,&rt_post_x);
    if (!ok) return;

    w=rt_w;

    switch(im->print_mode){
    case 3:

      static float r1,g1,b1,alpha1,r2,g2,b2,alpha2;
      alpha1=(color>>24)&255; r1=(color>>16)&255; g1=(color>>8)&255; b1=color&255;
      alpha2=(background_color>>24)&255; r2=(background_color>>16)&255; g2=(background_color>>8)&255; b2=background_color&255;
      static float dr,dg,db,da;
      dr=r2-r1;
      dg=g2-g1;
      db=b2-b1;
      da=alpha2-alpha1;
      static float cw;//color weight multiplier, avoids seeing black when transitioning from RGBA(?,?,?,255) to RGBA(0,0,0,0)
      if (alpha1) cw=alpha2/alpha1; else cw=100000;
      static float d;
 
      for (y2=0;y2<h;y2++){
    cp=rt_data+y2*w;
    for (x2=0;x2<w;x2++){

      d=*cp++;
      d=255-d;
      d/=255.0;
      static float r3,g3,b3,alpha3;
      alpha3=alpha1+da*d;
      d*=cw; if (d>1.0) d=1.0;
      r3=r1+dr*d;
      g3=g1+dg*d;
      b3=b1+db*d;
      static int32 r4,g4,b4,alpha4;
      r4=qbr_float_to_long(r3);
      g4=qbr_float_to_long(g3);
      b4=qbr_float_to_long(b3);
      alpha4=qbr_float_to_long(alpha3);
      pset(x+x2,y+y2,b4+(g4<<8)+(r4<<16)+(alpha4<<24));

    }}
      break;
    case 1:
      for (y2=0;y2<h;y2++){
    cp=rt_data+y2*w;
    for (x2=0;x2<w;x2++){
      z3=*cp++;
      if (z3) pset(x+x2,y+y2,((z3*a)>>8<<24)+z);
    }}
      break;
    case 2:
      for (y2=0;y2<h;y2++){
    cp=rt_data+y2*w;
    for (x2=0;x2<w;x2++){
      z3=*cp++;
      if (z3!=255) pset(x+x2,y+y2,(((255-z3)*a2)>>8<<24)+z2);
    }}
      break;
    default:
      break;
    }
    free(rt_data);
    return;
  }//custom font


  //default fonts
  if (im->font==8) cp=&charset8x8[character][0][0];
  if (im->font==14) cp=&charset8x16[character][1][0];
  if (im->font==16) cp=&charset8x16[character][0][0];
  switch(im->print_mode){
  case 3:
    for (y2=0;y2<h;y2++){ for (x2=0;x2<8;x2++){
    if (*cp++) pset(x+x2,y+y2,color); else pset(x+x2,y+y2,background_color);
      }}
    break;
  case 1:
    for (y2=0;y2<h;y2++){ for (x2=0;x2<8;x2++){
    if (*cp++) pset(x+x2,y+y2,color);
      }}
    break;
  case 2:
    for (y2=0;y2<h;y2++){ for (x2=0;x2<8;x2++){
    if (!(*cp++)) pset(x+x2,y+y2,background_color);
      }}
    break;
  default:
    break;
  }//z
  return;

}//printchr





int32 chrwidth(uint32 character){
  //Note: Only called by qbs_print()
  //      Supports "UNICODE" _LOADFONT option
  static int32 w;
  static img_struct *im;
  im=write_page;
  if (w=fontwidth[im->font]) return w;

  //Custom font
  static int32 render_option,f;
  static int32 ok;
  static uint8 *rt_data;
  static int32 rt_w,rt_h,rt_pre_x,rt_post_x;

  f=im->font;

  render_option=0;
  //8-bit / alpha-disabled 32-bit / dont-blend(alpha may still be applied)
  if ((im->bytes_per_pixel==1)||((im->bytes_per_pixel==4)&&(im->alpha_disabled))||(fontflags[f]&8)){
    render_option=1;
  }

  if ((fontflags[f]&32)){//UNICODE character
    ok=FontRenderTextUTF32(font[f],(uint32*)&character,1,render_option,
               &rt_data,&rt_w,&rt_h,&rt_pre_x,&rt_post_x);
  }else{//ASCII character
    character&=255;
    ok=FontRenderTextASCII(font[f],(uint8*)&character,1,render_option,
               &rt_data,&rt_w,&rt_h,&rt_pre_x,&rt_post_x);
  }
  if (!ok) return 0;
  free(rt_data);
  return rt_w;

}//chrwidth


void newline(){
  static uint32 *lp;
  static uint16 *sp;
  static int32 z,z2;

  //move cursor to new line
  write_page->cursor_y++; write_page->cursor_x=1;

  //scroll up screen if necessary
  if (write_page->cursor_y>write_page->bottom_row){

    if (lprint){
      sub__printimage(lprint_image);
      sub_cls(NULL,15,2);
      lprint_buffered=0;
      return;
    }

    if (write_page->text){
      //text
      //move lines up
      memmove(
          write_page->offset+(write_page->top_row-1)*2*write_page->width,
          write_page->offset+ write_page->top_row   *2*write_page->width,
          (write_page->bottom_row-write_page->top_row)*2*write_page->width
          );
      //erase bottom line
      z2=(write_page->color&0xF)+(write_page->background_color&7)*16+(write_page->color&16)*8;
      z2<<=8;
      z2+=32;
      sp=((uint16*)(write_page->offset+(write_page->bottom_row-1)*2*write_page->width));
      z=write_page->width;
      while(z--) *sp++=z2;
    }else{
      //graphics
      //move lines up
      memmove(
          write_page->offset+(write_page->top_row-1)*write_page->bytes_per_pixel*write_page->width*fontheight[write_page->font],
          write_page->offset+ write_page->top_row   *write_page->bytes_per_pixel*write_page->width*fontheight[write_page->font],
          (write_page->bottom_row-write_page->top_row)*write_page->bytes_per_pixel*write_page->width*fontheight[write_page->font]
          );
      //erase bottom line
      if (write_page->bytes_per_pixel==1){
    memset(write_page->offset+(write_page->bottom_row-1)*write_page->width*fontheight[write_page->font],write_page->background_color,write_page->width*fontheight[write_page->font]);
      }else{
    //assume 32-bit
    z2=write_page->background_color;
    lp=write_page->offset32+(write_page->bottom_row-1)*write_page->width*fontheight[write_page->font];
    z=write_page->width*fontheight[write_page->font];
    while(z--) *lp++=z2;
      }
    }//graphics
    write_page->cursor_y=write_page->bottom_row;
  }//scroll up

}//newline

void makefit(qbs *text){
  static int32 w,x,x2,x3;
  if (write_page->holding_cursor) return;
  if (write_page->cursor_x!=1){//if already at left-most, nothing more can be done
    if (write_page->text){
      if ((write_page->cursor_x+text->len-1)>write_page->width) newline();
    }else{
      w=func__printwidth(text,NULL,NULL);
      x=fontwidth[write_page->font]; if (!x) x=1; x=x*(write_page->cursor_x-1);
      if ((x+w)>write_page->width) newline();
    }
  }
}

void lprint_makefit(qbs *text){
  //stub
  //makefit(text);
}

void tab(){
  static int32 x,x2,w;

  //tab() on a held-cursor only sets the cursor to the left hand position of the next line
  if (write_page->holding_cursor){
    newline(); write_page->holding_cursor=0;
    return;
  }

  //text
  if (write_page->text){
    qbs_print(singlespace,0);
  text:
    if (write_page->cursor_x!=1){
      if (((write_page->cursor_x-1)%14)||(write_page->cursor_x>(write_page->width-13))){
    if (write_page->cursor_x<write_page->width){qbs_print(singlespace,0); goto text;}
      }
    }//!=1
    return;
  }

  x=fontwidth[write_page->font]; 
  if (!x){

    //variable width
    x=write_page->cursor_x-1;
    x2=(x/112+1)*112;//next position
    if (x2>=write_page->width){//it doesn't fit on line
      //box fill x to end of line with background color
      fast_boxfill(x,(write_page->cursor_y-1)*fontheight[write_page->font],write_page->width-1,write_page->cursor_y*fontheight[write_page->font]-1,write_page->background_color);
      newline();
    }else{//fits on line
      //box fill x to x2-1 with background color
      fast_boxfill(x,(write_page->cursor_y-1)*fontheight[write_page->font],x2-1,write_page->cursor_y*fontheight[write_page->font]-1,write_page->background_color);
      write_page->cursor_x=x2;
    }


  }else{

    //fixed width
    w=write_page->width/x;

    qbs_print(singlespace,0);
  fixwid:
    if (write_page->cursor_x!=1){
      if (((write_page->cursor_x-1)%14)||(write_page->cursor_x>(w-13))){
    if (write_page->cursor_x<w){qbs_print(singlespace,0); goto fixwid;}
      }
    }//!=1

  }
  return;
}

int32 func_lpos(int32 lpt){
  //lpt values: 0 = LPT1, 1 = LPT1, 2 = LPT2, 3 = LPT3
  if ((lpt<0)||(lpt>3)){error(5); return 0;}
  return lpos;
}

void qbs_lprint(qbs* str,int32 finish_on_new_line){
  static int32 old_dest;
  while (lprint_locked) Sleep(64);
  lprint=1;
  old_dest=func__dest();
  if (!lprint_image){
    lprint_image=func__newimage(640,960,13,1);
    sub__dest(lprint_image);
    sub_cls(NULL,15,2);
    sub__font(16,NULL,0);
    qbg_sub_color(0,15,NULL,3);
    qbg_sub_view_print(1,60,1);
  }else{
    sub__dest(lprint_image);
  }
  lprint_buffered=1;
  lprint_last=func_timer(0.001,1);
  qbs_print(str,finish_on_new_line);
  sub__dest(old_dest);
  lprint=0;
}

int32 no_control_characters=0;
int32 no_control_characters2=0;

void sub__controlchr(int32 onoff){
  if (onoff==2) no_control_characters2=1; else no_control_characters2=0;
}

int32 func__controlchr () {
  return -no_control_characters2;
}

void qbs_print(qbs* str,int32 finish_on_new_line){
  if (new_error) return;
  int32 i,i2,entered_new_line,x,x2,y,y2,z,z2,w;
  entered_new_line=0;
  static uint32 character;

  if (write_page->console){
    static qbs* strz; if (!strz) strz=qbs_new(0,0);
    qbs_set(strz,qbs_add(str,qbs_new_txt_len("\0",1)));
    if (finish_on_new_line) cout<<(char*)strz->chr<<endl; else cout<<(char*)strz->chr;
    return;
  }

  /*
    if (!str->len){
    if (!newline) return;//no action required
    if (write_page->holding_cursor){//extra CR required before return
    write_page->holding_cursor=0;
    i=-1;
    write_page->cursor_x++;
    goto print_unhold_cursor;
    }
    }

    if (!str->len) goto null_length;

    if (write_page->holding_cursor){
    write_page->holding_cursor=0;
    i=-1;
    write_page->cursor_x++;
    goto print_unhold_cursor;
    }
  */

  //holding cursor?
  if (write_page->holding_cursor){
    if (str->len){
      write_page->holding_cursor=0;
      newline();
    }else{
      //null length print string
      if (finish_on_new_line) write_page->holding_cursor=0;//new line will be entered automatically
    }
  }


  for (i=0;i<str->len;i++){

    character=str->chr[i];

    if (fontflags[write_page->font]&32){//unicode font
      if (i>(str->len-4)) break;//not enough data for a utf32 encoding
      character=*((int32*)(&str->chr[i]));
      i+=3;
    }

    if (lprint) lprint_buffered=1;
    entered_new_line=0;//beginning a new line was the last action (so don't add a new one)



    //###special characters

    if (no_control_characters||no_control_characters2) goto skip_control_characters;

    if (character==28){
      //advance one cursor position
      if (lprint){
    if (lpos<width_lprint) lpos++;
      }
      //can cursor advance?
      if (write_page->cursor_y>=write_page->bottom_row){
    if (write_page->text){
      if (write_page->cursor_x>=write_page->width) goto skip;
    }else{
      if (fontwidth[write_page->font]){
        if (write_page->cursor_x>=(write_page->width/fontwidth[write_page->font])) goto skip;
      }else{
        if (write_page->cursor_x>=write_page->width) goto skip;
      }
    } 
      }
      write_page->cursor_x++;
      if (write_page->text){
    if (write_page->cursor_x>write_page->width){write_page->cursor_y++; write_page->cursor_x=1;}
      }else{
    if (fontwidth[write_page->font]){
      if (write_page->cursor_x>(write_page->width/fontwidth[write_page->font])){write_page->cursor_y++; write_page->cursor_x=1;}
    }else{
      if (write_page->cursor_x>write_page->width){write_page->cursor_y++; write_page->cursor_x=1;}
    }
      } 
      goto skip;
    }

    if (character==29){
      //go back one cursor position
      if (lprint){
    if (lpos>1) lpos--;
      }
      //can cursor go back?
      if ((write_page->cursor_y==write_page->top_row)||(write_page->cursor_y>write_page->bottom_row)){
    if (write_page->cursor_x==1) goto skip;
      }
      write_page->cursor_x--;
      if (write_page->cursor_x<1){
    write_page->cursor_y--;
    if (write_page->text){
      write_page->cursor_x=write_page->width;
    }else{
      if (fontwidth[write_page->font]){
        write_page->cursor_x=write_page->width/fontwidth[write_page->font];
      }else{
        write_page->cursor_x=write_page->width;
      }
    } 
      }
      goto skip;
    }

    if (character==30){
      //previous row, same column
      //no change if cursor not within view print boundries
      if ((write_page->cursor_y>write_page->top_row)&&(write_page->cursor_y<=write_page->bottom_row)){
    write_page->cursor_y--;
      }
      goto skip;
    }

    if (character==31){
      //next row, same column
      //no change if cursor not within view print boundries
      if ((write_page->cursor_y>=write_page->top_row)&&(write_page->cursor_y<write_page->bottom_row)){
    write_page->cursor_y++;
      }
      goto skip;
    }

    if (character==12){//aka form feed
      if (lprint){sub__printimage(lprint_image); lprint_buffered=0;}
      //clears text viewport
      //clears bottom row
      //moves cursor to top-left of text viewport
      sub_cls(NULL,NULL,0);
      if (lprint) lpos=1;
      goto skip;
    }

    if (character==11){
      write_page->cursor_x=1; write_page->cursor_y=write_page->top_row;
      if (lprint) lpos=1;
      goto skip;
    }

    if (character==9){
      //moves to next multiple of 8 (always advances at least one space)
      if (!fontwidth[write_page->font]){
    //variable width!
    x=write_page->cursor_x-1;
    x2=(x/64+1)*64;//next position
    if (x2>=write_page->width){//it doesn't fit on line
      //box fill x to end of line with background color
      fast_boxfill(x,(write_page->cursor_y-1)*fontheight[write_page->font],write_page->width-1,write_page->cursor_y*fontheight[write_page->font]-1,write_page->background_color);
      newline();
      entered_new_line=1;
    }else{//fits on line
      //box fill x to x2-1 with background color
      fast_boxfill(x,(write_page->cursor_y-1)*fontheight[write_page->font],x2-1,write_page->cursor_y*fontheight[write_page->font]-1,write_page->background_color);
      write_page->cursor_x=x2;
    }
    goto skip;
      }else{
    if (write_page->cursor_x%8){//next cursor position not a multiple of 8
      i--;//more spaces will be required
    }
    character=32;//override character 9
      }
    }//9

    if (character==7){
      //qb64_generatesound(783.99,0.2,0);
      Sleep(250);
      goto skip;
    }

    if ((character==10)||(character==13)){
      newline();
      if (lprint) lpos=1;
      //note: entered_new_line not set because these carriage returns compound on each other
      goto skip;
    }

  skip_control_characters:

    //###check if character fits on line, if not move to next line
    //(only applies to non-fixed width fonts)
    if (!fontwidth[write_page->font]){//unpredictable width
      w=chrwidth(character);
      if ((write_page->cursor_x+w)>write_page->width){
    newline();
    //entered_new_line not set, a character will follow
      }
    }

    //###print the character
    printchr(character);

    //###advance lpos, begin new line if necessary
    if (lprint){
      lpos++;
      if (lpos>width_lprint){
    newline();
    entered_new_line=1;
    lpos=1;
    goto skip;//skip cursor advancement and checking because new line entered
      }
    }

    //###advance cursor
    if (fontwidth[write_page->font]){
      write_page->cursor_x++;
    }else{
      write_page->cursor_x+=w;
    }

    //###check if another character could fit at cursor_x's location
    if (write_page->compatible_mode){//graphics
      x=fontwidth[write_page->font]; if (!x) x=1;
      x2=x*(write_page->cursor_x-1);
      if (x2>(write_page->width-x)){
    if (!finish_on_new_line){
      if (i==(str->len-1)){//last character
        //move horizontal cursor back to right-most valid position
        write_page->cursor_x=write_page->width/x;
        write_page->holding_cursor=1;
        goto held_cursor;
      }
    }
    newline();
    entered_new_line=1;
      }
    }else{//text
      if (write_page->cursor_x>write_page->width){
    if (!finish_on_new_line){
      if (i==(str->len-1)){//last character
        write_page->cursor_x--;//move horizontal cursor back to right-most valid position
        write_page->holding_cursor=1;
        goto held_cursor;
      }
    }
    newline();
    entered_new_line=1;
      }
    }
  held_cursor:

  skip:;

    /*
      tabbing1:


      write_page->cursor_x++;



      //hold cursor?
      if (write_page->cursor_x>qbg_width_in_characters){//past last x position
      if (!newline){//don't need a new line
      if (i==(str->len-1)){//last character
      write_page->cursor_x--;
      write_page->holding_cursor=1;
      goto hold_cursor;
      }
      }
      }




      qbs_print_skipchar:;

      print_unhold_cursor:

      if (write_page->cursor_x>qbg_width_in_characters){
      qbs_print_newline:
      newlineadded=1;

      if (write_page->cursor_y==qbg_height_in_characters) write_page->cursor_y=qbg_bottom_row;

      write_page->cursor_y++;
      write_page->cursor_x=1;



      if (write_page->cursor_y>qbg_bottom_row){
      //move screen space within view print up 1 row
      //if (qbg_mode==13){

      ///memmove(&cmem[655360+(qbg_top_row-1)*2560],&cmem[655360+qbg_top_row*2560],(qbg_bottom_row-qbg_top_row)*2560);
      ///memset(&cmem[655360+(qbg_bottom_row-1)*2560],0,2560);
      if (qbg_text_only){

      memmove(qbg_active_page_offset+(qbg_top_row-1)*qbg_width_in_characters*2,
      qbg_active_page_offset+(qbg_top_row)*qbg_width_in_characters*2,
      (qbg_bottom_row-qbg_top_row)*qbg_width_in_characters*2);
      for (i2=0;i2<qbg_width_in_characters;i2++){
      qbg_active_page_offset[(qbg_bottom_row-1)*qbg_width_in_characters*2+i2*2]=32;
      qbg_active_page_offset[(qbg_bottom_row-1)*qbg_width_in_characters*2+i2*2+1]=7;

      }

      }else{
      memmove(qbg_active_page_offset+(qbg_top_row-1)*qbg_bytes_per_pixel*qbg_width*qbg_character_height,
      qbg_active_page_offset+qbg_top_row*qbg_bytes_per_pixel*qbg_width*qbg_character_height,
      (qbg_bottom_row-qbg_top_row)*qbg_bytes_per_pixel*qbg_width*qbg_character_height);
      memset(qbg_active_page_offset+(qbg_bottom_row-1)*qbg_bytes_per_pixel*qbg_width*qbg_character_height,0,qbg_bytes_per_pixel*qbg_width*qbg_character_height);
      }




      write_page->cursor_y=qbg_bottom_row;
      }



      }
    */

  }//i

 null_length:
  if (finish_on_new_line&&(!entered_new_line)) newline();
  if (lprint){
    if (finish_on_new_line) lpos=1;
  }


  /*
    null_length:

    //begin new line?
    if (newline&&(!newlineadded)) {newline=0; goto qbs_print_newline;}

    //hold cursor
    hold_cursor:
  */

  return;

}

template <typename T> static T qbs_cleanup(uint32 base,T passvalue){ 
      while (qbs_tmp_list_nexti>base) { qbs_tmp_list_nexti--; if(qbs_tmp_list[qbs_tmp_list_nexti]!=-1)qbs_free((qbs*)qbs_tmp_list[qbs_tmp_list_nexti]); }//clear any temp. strings created
            return passvalue;
}

void qbg_sub_window(float x1,float y1,float x2,float y2,int32 passed){
  //                  &1
  //(passed&2)->SCREEN
  if (new_error) return;
  static float i;
  static float old_x,old_y;

  if (write_page->text) goto qbg_sub_window_error;
  if ((!(passed&1))&&(passed&2)) goto qbg_sub_window_error;//SCREEEN passed without any other arguements!

  //backup current qbg_x & qbg_y coordinates relative to viewport, not window
  if (write_page->clipping_or_scaling==2){
    old_x=write_page->x*write_page->scaling_x+write_page->scaling_offset_x;
    old_y=write_page->y*write_page->scaling_y+write_page->scaling_offset_y;
  }else{
    old_x=write_page->x;
    old_y=write_page->y;
  }


  if (passed&1){
    if (x1==x2) goto qbg_sub_window_error;
    if (y1==y2) goto qbg_sub_window_error;
    //sort so x1 & y1 contain the lower values
    if (x1>x2){i=x1; x1=x2; x2=i;}
    if (y1>y2){i=y1; y1=y2; y2=i;}
    if (!(passed&2)){
      i=y1; y1=y2; y2=i;
    }
    //Note: Window's coordinates are not based on prev. WINDOW values
    write_page->clipping_or_scaling=2;
    write_page->scaling_x=((float)(write_page->view_x2-write_page->view_x1))/(x2-x1);
    write_page->scaling_y=((float)(write_page->view_y2-write_page->view_y1))/(y2-y1);
    write_page->scaling_offset_x=-x1*write_page->scaling_x; //scaling offset should be applied before scaling
    write_page->scaling_offset_y=-y1*write_page->scaling_y;
    if (!(passed&2)){
      write_page->scaling_offset_y=-y2*write_page->scaling_y+(write_page->view_y2-write_page->view_y1);
    }
    write_page->window_x1=x1; write_page->window_x2=x2; write_page->window_y1=y1; write_page->window_y2=y2;


    if (x1==0){ if (y1==0){ if (x2==(write_page->width-1)){ if (y2==(write_page->height-1)){
        if ((write_page->scaling_x==1)&&(write_page->scaling_y==1)){
          if ((write_page->scaling_offset_x==0)&&(write_page->scaling_offset_y==0)){
        goto qbg_sub_window_restore_default;
          }
        }
      }}}}

    //adjust qbg_x & qbg_y according to new window
    write_page->x=(old_x-write_page->scaling_offset_x)/write_page->scaling_x;
    write_page->y=(old_y-write_page->scaling_offset_y)/write_page->scaling_y;

    return;
  }else{
    //restore default WINDOW coordinates
  qbg_sub_window_restore_default:
    write_page->clipping_or_scaling=1;
    write_page->scaling_x=1;
    write_page->scaling_y=1;
    write_page->scaling_offset_x=0;
    write_page->scaling_offset_y=0;
    write_page->window_x1=0; write_page->window_x2=write_page->width-1; write_page->window_y1=0; write_page->window_y2=write_page->height-1;
    if (write_page->view_x1==0){ if (write_page->view_y1==0){ if (write_page->view_x2==(write_page->width-1)){ if (write_page->view_y2==(write_page->height-1)){
        if (write_page->view_offset_x==0){ if (write_page->view_offset_y==0){
        write_page->clipping_or_scaling=0;
          }}
      }}}}

    //adjust qbg_x & qbg_y according to new window
    write_page->x=old_x;
    write_page->y=old_y;

    return;
  }
 qbg_sub_window_error:
  error(5);
  return;
}



void qbg_sub_view_print(int32 topline,int32 bottomline,int32 passed){
  if (new_error) return;

  static int32 maxrows;
  maxrows=write_page->height; if (!write_page->text) maxrows/=fontheight[write_page->font];

  if (!passed){//topline and bottomline not passed
    write_page->top_row=1; write_page->bottom_row=maxrows;
    write_page->cursor_y=1; write_page->cursor_x=1;
    write_page->holding_cursor=0;
    return;
  }

  if (topline<=0) goto error;
  if (topline>maxrows) goto error;
  if (bottomline<topline) goto error;
  if (bottomline>maxrows) goto error;

  write_page->top_row=topline;
  write_page->bottom_row=bottomline;
  write_page->cursor_y=write_page->top_row;
  write_page->cursor_x=1;
  write_page->holding_cursor=0;
  return;

 error:
  error(5);
  return;
}

void qbg_sub_view(int32 x1,int32 y1,int32 x2,int32 y2,int32 fillcolor,int32 bordercolor,int32 passed){
  //   &1                                   &4              &8
  //    (passed&2)->coords_relative_to_screen
  if (new_error) return;
  //format: [{SCREEN}][(?,?)-(?,?)],[?],[?]
  //bordercolor draws a line AROUND THE OUTSIDE of the specified viewport
  //the current WINDOW settings do not affect inputted x,y values
  //the current VIEW settings do not affect inputted x,y values
  //REMEMBER! Recalculate WINDOW values based on new viewport dimensions
  int32 i;

  //PRE-ERROR CHECKING
  if (passed&1){
    if (x1<0) goto error;
    if (x1>=write_page->width) goto error;
    if (y1<0) goto error;
    if (y1>=write_page->height) goto error;
    if (x2<0) goto error;
    if (x2>=write_page->width) goto error;
    if (y2<0) goto error;
    if (y2>=write_page->height) goto error;
  }else{
    if (passed&2) goto error;
    if (passed&4) goto error;
    if (passed&8) goto error;
  }

  //reset DRAW attributes
  write_page->draw_ta=0.0; write_page->draw_scale=1.0;

  if (passed&1){
    //force x1,y1 to be the top left corner
    if (x2<x1){i=x1;x1=x2;x2=i;}
    if (y2<y1){i=y1;y1=y2;y2=i;}

    write_page->view_x1=x1; write_page->view_y1=y1; write_page->view_x2=x2; write_page->view_y2=y2;
    if ((passed&2)==0){
      write_page->view_offset_x=x1; write_page->view_offset_y=y1;
    }else{
      write_page->view_offset_x=0; write_page->view_offset_y=0;
    }
    if (!write_page->clipping_or_scaling) write_page->clipping_or_scaling=1;
  }else{
    //no argurments passed
    write_page->view_x1=0; write_page->view_y1=0; write_page->view_x2=write_page->width-1; write_page->view_y2=write_page->height-1;
    write_page->view_offset_x=0; write_page->view_offset_y=0;
    if (write_page->clipping_or_scaling==1) write_page->clipping_or_scaling=0;
  }

  //recalculate window values based on new viewport (if necessary)
  if (write_page->clipping_or_scaling==2){//WINDOW'ing in use
    write_page->scaling_x=((float)(write_page->view_x2-write_page->view_x1))/(write_page->window_x2-write_page->window_x1);
    write_page->scaling_y=((float)(write_page->view_y2-write_page->view_y1))/(write_page->window_y2-write_page->window_y1);
    write_page->scaling_offset_x=-write_page->window_x1*write_page->scaling_x;
    write_page->scaling_offset_y=-write_page->window_y1*write_page->scaling_y;
    if (write_page->window_y2<write_page->window_y1) write_page->scaling_offset_y=-write_page->window_y2*write_page->scaling_y+write_page->view_y2;
  }

  if (passed&4){//fillcolor
    qb32_boxfill(write_page->window_x1,write_page->window_y1,write_page->window_x2,write_page->window_y2,fillcolor);
  }

  if (passed&8){//bordercolor
    static int32 bx,by;
    by=write_page->view_y1-1;
    if ((by>=0)&&(by<write_page->height)){
      for (bx=write_page->view_x1-1;bx<=write_page->view_x2;bx++){
    if ((bx>=0)&&(bx<write_page->width)){
      pset(bx,by,bordercolor);
    }}}
    by=write_page->view_y2+1;
    if ((by>=0)&&(by<write_page->height)){
      for (bx=write_page->view_x1-1;bx<=write_page->view_x2;bx++){
    if ((bx>=0)&&(bx<write_page->width)){
      pset(bx,by,bordercolor);
    }}}
    bx=write_page->view_x1-1;
    if ((bx>=0)&&(bx<write_page->width)){
      for (by=write_page->view_y1-1;by<=write_page->view_y2;by++){
    if ((by>=0)&&(by<write_page->height)){
      pset(bx,by,bordercolor);
    }}}
    bx=write_page->view_x2+1;
    if ((bx>=0)&&(bx<write_page->width)){
      for (by=write_page->view_y1-1;by<=(write_page->view_y2+1);by++){
    if ((by>=0)&&(by<write_page->height)){
      pset(bx,by,bordercolor);
    }}}
  }

  return;
 error:
  error(5);
  return;
}



void sub_cls(int32 method,uint32 use_color,int32 passed){
  if (new_error) return;
  static int32 characters,i;
  static uint16 *sp;
  static uint16 clearvalue;

  //validate
  if (passed&2){
    if (write_page->bytes_per_pixel!=4){
      if (use_color>write_page->mask) goto error;
    }
  }else{
    use_color=write_page->background_color;
  }

  if (passed&1){
    if ((method>2)||(method<0)) goto error;
  }



  //all CLS methods reset the cursor position
  write_page->cursor_y=write_page->top_row;
  write_page->cursor_x=1;

  //all CLS methods reset DRAW attributes
  write_page->draw_ta=0.0; write_page->draw_scale=1.0;

  if (write_page->text){
    //precalculate a (int16) value which can be used to clear the screen
    clearvalue=(write_page->color&0xF)+(use_color&7)*16+(write_page->color&16)*8;
    clearvalue<<=8;
    clearvalue+=32;
  }

  if ((passed&1)==0){//no method specified
    //video mode: clear only graphics viewport
    //text mode: clear text view port AND the bottom line
    if (write_page->text){
      //text view port
      characters=write_page->width*(write_page->bottom_row-write_page->top_row+1);
      sp=(uint16*)&write_page->offset[(write_page->top_row-1)*write_page->width*2];
      for (i=0;i<characters;i++){sp[i]=clearvalue;}
      //bottom line
      characters=write_page->width;
      sp=(uint16*)&write_page->offset[(write_page->height-1)*write_page->width*2];
      for (i=0;i<characters;i++){sp[i]=clearvalue;}
      key_display_redraw=1; key_update();
      return;
    }else{//graphics
      //graphics view port
      if (write_page->bytes_per_pixel==1){//8-bit
    if (write_page->clipping_or_scaling){
      qb32_boxfill(write_page->window_x1,write_page->window_y1,write_page->window_x2,write_page->window_y2,use_color);
    }else{//fast method (no clipping/scaling)
      memset(write_page->offset,use_color,write_page->width*write_page->height);
    }
      }else{//32-bit
    i=write_page->alpha_disabled; write_page->alpha_disabled=1;  
    if (write_page->clipping_or_scaling){
      qb32_boxfill(write_page->window_x1,write_page->window_y1,write_page->window_x2,write_page->window_y2,use_color);
    }else{//fast method (no clipping/scaling)
      fast_boxfill(0,0,write_page->width-1,write_page->height-1,use_color);
    }
    write_page->alpha_disabled=i;
      }
    }

    if (write_page->clipping_or_scaling==2){
      write_page->x=((float)(write_page->view_x2-write_page->view_x1+1))/write_page->scaling_x/2.0f+write_page->scaling_offset_x;
      write_page->y=((float)(write_page->view_y2-write_page->view_y1+1))/write_page->scaling_y/2.0f+write_page->scaling_offset_y;
    }else{
      write_page->x=((float)(write_page->view_x2-write_page->view_x1+1))/2.0f;
      write_page->y=((float)(write_page->view_y2-write_page->view_y1+1))/2.0f;
    }

    key_display_redraw=1; key_update();
    return;
  }

  if (method==0){//clear everything
    if (write_page->text){
      characters=write_page->height*write_page->width;
      sp=(uint16*)write_page->offset;
      for (i=0;i<characters;i++){sp[i]=clearvalue;}
      key_display_redraw=1; key_update();
      return;
    }else{
      if (write_page->bytes_per_pixel==1){
    memset(write_page->offset,use_color,write_page->width*write_page->height);
      }else{ 
    i=write_page->alpha_disabled; write_page->alpha_disabled=1;  
    fast_boxfill(0,0,write_page->width-1,write_page->height-1,use_color);
    write_page->alpha_disabled=i;
      }
    }

    if (write_page->clipping_or_scaling==2){
      write_page->x=((float)(write_page->view_x2-write_page->view_x1+1))/write_page->scaling_x/2.0f+write_page->scaling_offset_x;
      write_page->y=((float)(write_page->view_y2-write_page->view_y1+1))/write_page->scaling_y/2.0f+write_page->scaling_offset_y;
    }else{
      write_page->x=((float)(write_page->view_x2-write_page->view_x1+1))/2.0f;
      write_page->y=((float)(write_page->view_y2-write_page->view_y1+1))/2.0f;
    }

    key_display_redraw=1; key_update();
    return;
  }

  if (method==1){//ONLY clear the graphics viewport
    if (write_page->text) return;
    //graphics view port
    if (write_page->bytes_per_pixel==1){//8-bit
      if (write_page->clipping_or_scaling){
    qb32_boxfill(write_page->window_x1,write_page->window_y1,write_page->window_x2,write_page->window_y2,use_color);
      }else{//fast method (no clipping/scaling)
    memset(write_page->offset,use_color,write_page->width*write_page->height);
      }
    }else{//32-bit
      i=write_page->alpha_disabled; write_page->alpha_disabled=1;  
      if (write_page->clipping_or_scaling){
    qb32_boxfill(write_page->window_x1,write_page->window_y1,write_page->window_x2,write_page->window_y2,use_color);
      }else{//fast method (no clipping/scaling)
    fast_boxfill(0,0,write_page->width-1,write_page->height-1,use_color);
      }
      write_page->alpha_disabled=i;
    }

    if (write_page->clipping_or_scaling==2){
      write_page->x=((float)(write_page->view_x2-write_page->view_x1+1))/write_page->scaling_x/2.0f+write_page->scaling_offset_x;
      write_page->y=((float)(write_page->view_y2-write_page->view_y1+1))/write_page->scaling_y/2.0f+write_page->scaling_offset_y;
    }else{
      write_page->x=((float)(write_page->view_x2-write_page->view_x1+1))/2.0f;
      write_page->y=((float)(write_page->view_y2-write_page->view_y1+1))/2.0f;
    }

    key_display_redraw=1; key_update();
    return;
  }

  if (method==2){//ONLY clear the VIEW PRINT range text viewport
    if (write_page->text){
      //text viewport
      characters=write_page->width*(write_page->bottom_row-write_page->top_row+1);
      sp=(uint16*)&write_page->offset[(write_page->top_row-1)*write_page->width*2];
      for (i=0;i<characters;i++){sp[i]=clearvalue;}
      //should not and does not redraw KEY bar
      return;
    }else{
      //text viewport
      if (write_page->bytes_per_pixel==1){//8-bit
    memset(&write_page->offset[write_page->width*fontheight[write_page->font]*(write_page->top_row-1)],use_color,write_page->width*fontheight[write_page->font]*(write_page->bottom_row-write_page->top_row+1));
      }else{//32-bit
    i=write_page->alpha_disabled; write_page->alpha_disabled=1;  
    fast_boxfill(0,fontheight[write_page->font]*(write_page->top_row-1),write_page->width-1,fontheight[write_page->font]*write_page->bottom_row-1,use_color);
    write_page->alpha_disabled=i;
      }
      //should not and does not redraw KEY bar
      return;
    }
  }

  return;
 error:
  error(5);
  return;
}



void qbg_sub_locate(int32 row,int32 column,int32 cursor,int32 start,int32 stop,int32 passed){
  static int32 h,w,i;
  if (new_error) return;

  //calculate height & width in characters
  if (write_page->compatible_mode){
    h=write_page->height/fontheight[write_page->font];
    if (fontwidth[write_page->font]){
      w=write_page->width/fontwidth[write_page->font];
    }else{
      w=write_page->width;
    }
  }else{
    h=write_page->height;
    w=write_page->width;
  }

  //PRE-ERROR CHECKING
  if (passed&1){
    if (row<write_page->top_row) goto error;
    if ((row!=h)&&(row>write_page->bottom_row)){
      if (width8050switch){//note: can assume WIDTH 80,25 as no SCREEN or WIDTH statements have been called
    width8050switch=0;
    if (row<=50){
      if (passed&2){
        if (column<1) goto error;
        if (column>w) goto error;
      }
      char *buffer;
      uint32 c,c2;
      buffer=(char*)malloc(80*25*2);
      c=write_page->color; c2=write_page->background_color;
      memcpy(buffer,&cmem[0xB8000],80*25*2);
      qbsub_width(0,80,50,3);
      memcpy(&cmem[0xB8000],buffer,80*25*2);
      write_page->color=c; write_page->background_color=c2;
      free(buffer);
      goto width8050switch_done;
    }
      }
      goto error;
    }
  }
 width8050switch_done:
  if (passed&2){
    if (column<1) goto error;
    if (column>w) goto error;
  }
  if (passed&4){
    if (cursor<0) goto error;
    if (cursor>1) goto error;
  }
  if (passed&8){
    if (start<0) goto error;
    if (start>31) goto error;
  }
  if (passed&16){
    if (stop<0) goto error;
    if (stop>31) goto error;
  }

  if (passed&1) {write_page->cursor_y=row; write_page->holding_cursor=0;}
  if (passed&2) {write_page->cursor_x=column; write_page->holding_cursor=0;}
  if ((passed&3)==0){
    if (write_page->holding_cursor) write_page->holding_cursor=2;//special case[forum/index.php?topic=5255.0]
  }


  if (passed&4){
    if (cursor) cursor=1;
    write_page->cursor_show=cursor;
    if (write_page->flags&IMG_SCREEN){//page-linked attribute
      for (i=0;i<pages;i++){
    if (page[i]) img[i].cursor_show=cursor;
      }
    }//IMG_SCREEN
  }//passed&4


  if (passed&8){
    write_page->cursor_firstvalue=start;
  }else{
    start=write_page->cursor_firstvalue;
  }
  if (passed&16){
    write_page->cursor_lastvalue=stop;
  }else{
    stop=write_page->cursor_lastvalue;
  }
  if (passed&(8+16)){
    if (write_page->flags&IMG_SCREEN){//page-linked attribute
      for (i=0;i<pages;i++){
    if (page[i]){
      img[i].cursor_firstvalue=start;
      img[i].cursor_lastvalue=stop;
    }
      }//i
    }//IMG_SCREEN
  }

  return;

 error:
  error(5);
  return;
}









//input helper functions:
uint64 hexoct2uint64_value;
int32 hexoct2uint64(qbs* h){
  //returns 0=failed
  //        1=HEX value (default if unspecified)
  //        2=OCT value
  static int32 i,i2;
  static uint64 result;
  result=0;
  static int32 type;
  type=0;
  hexoct2uint64_value=0;
  if (!h->len) return 1;
  if (h->chr[0]!=38) return 0;//not "&"
  if (h->len==1) return 1;//& received, but awaiting further input
  i=h->chr[1];
  if ((i==72)||(i==104)) type=1;//"H"or"h"
  if ((i==79)||(i==111)) type=2;//"O"or"o"
  if (!type) return 0;
  if (h->len==2) return type;

  if (type==1){
    if (h->len>18) return 0;//larger than int64
    for (i=2;i<h->len;i++){
      result<<=4;
      i2=h->chr[i];
      //          0  -      9             A  -      F             a  -      f
      if ( ((i2>=48)&&(i2<=57)) || ((i2>=65)&&(i2<=70)) || ((i2>=97)&&(i2<=102)) ){
    if (i2>=97) i2-=32;
    if (i2>=65) i2-=7;
    i2-=48;
    //i2 is now a values between 0 and 15
    result+=i2;
      }else return 0;//invalid character
    }//i
    hexoct2uint64_value=result;
    return 1;
  }//type==1

  if (type==2){
    //unsigned _int64 max=18446744073709551615 (decimal, 20 chars)
    //                   =1777777777777777777777 (octal, 22 chars)
    //                   =FFFFFFFFFFFFFFFF (hex, 16 chars)
    if (h->len>24) return 0;//larger than int64
    if (h->len==24){
      if ((h->chr[2]!=48)&&(h->chr[2]!=49)) return 0;//larger than int64
    }
    for (i=2;i<h->len;i++){
      result<<=3;
      i2=h->chr[i];
      if ((i2>=48)&&(i2<=55)){//0-7
    i2-=48;
    result+=i2;
      }else return 0;//invalid character
    }//i
    hexoct2uint64_value=result;
    return 2;
  }//type==2

}


extern void SUB_VKUPDATE();

//input method (complex, calls other qbs functions)
const char *uint64_max[] =    {"18446744073709551615"};
const char *int64_max[] =     {"9223372036854775807"};
const char *int64_max_neg[] = {"9223372036854775808"};
const char *single_max[] = {"3402823"};
const char *single_max_neg[] = {"1401298"};
const char *double_max[] = {"17976931"};
const char *double_max_neg[] = {"4940656"};
uint8 significant_digits[1024];
int32 num_significant_digits;

extern void *qbs_input_variableoffsets[257];
extern int32 qbs_input_variabletypes[257];
qbs *qbs_input_arguements[257];
int32 cursor_show_last;

void qbs_input(int32 numvariables,uint8 newline){
  if (new_error) return;
  int32 i,i2,i3,i4,i5,i6,chr;

  static int32 autodisplay_backup;
  autodisplay_backup=autodisplay;
  autodisplay=1;

  static int32 source_backup;
  source_backup=func__source();
  sub__source(func__dest());

  //duplicate dest image so any changes can be reverted
  static int32 dest_image,dest_image_temp,dest_image_holding_cursor;
  dest_image=func__copyimage(func__dest(),NULL,NULL);
  if (dest_image==-1) error(516);//out of memory
  dest_image_temp=func__copyimage(func__dest(),NULL,NULL);
  if (dest_image_temp==-1) error(517);//out of memory
  static int32 dest_image_cursor_x,dest_image_cursor_y;
  dest_image_cursor_x=write_page->cursor_x;
  dest_image_cursor_y=write_page->cursor_y;
  dest_image_holding_cursor=write_page->holding_cursor;

  uint32 qbs_tmp_base=qbs_tmp_list_nexti;

  static int32 lineinput;
  lineinput=0;
  if (qbs_input_variabletypes[1]&ISSTRING){
    if (qbs_input_variabletypes[1]&512){
      qbs_input_variabletypes[1]=-512;
      lineinput=1;
    }}

  cursor_show_last=write_page->cursor_show;
  write_page->cursor_show=1;

  int32 addspaces;
  addspaces=0;
  qbs* inpstr=qbs_new(0,0);//not temp so must be freed
  qbs* inpstr2=qbs_new(0,0);//not temp so must be freed
  qbs* key=qbs_new(0,0);//not temp so must be freed
  qbs* c=qbs_new(1,0);//not temp so must be freed

  for (i=1;i<=numvariables;i++) qbs_input_arguements[i]=qbs_new(0,0);

  //init all passed variables to 0 or ""
  for (i=1;i<=numvariables;i++){

    if (qbs_input_variabletypes[i]&ISSTRING){//STRING
      if (((qbs*)qbs_input_variableoffsets[i])->fixed){
    memset(((qbs*)qbs_input_variableoffsets[i])->chr,32,((qbs*)qbs_input_variableoffsets[i])->len);
      }else{
    ((qbs*)qbs_input_variableoffsets[i])->len=0;
      }
    }

    if ((qbs_input_variabletypes[i]&ISOFFSETINBITS)==0){//reg. numeric variable
      memset(qbs_input_variableoffsets[i],0,(qbs_input_variabletypes[i]&511)>>3);
    }

    //bit referenced?

  }//i




 qbs_input_next:

  int32 argn,firstchr,toomany;
  toomany=0;
  argn=1;
  i=0;
  i2=0;
  qbs_input_arguements[1]->len=0;
  firstchr=1;
 qbs_input_sep_arg:

  if (i<inpstr->len){

    if (inpstr->chr[i]==44){//","
      if (i2!=1){//not in the middle of a string
    if (!lineinput){
      i2=0;
      argn=argn+1;
      if (argn>numvariables){toomany=1; goto qbs_input_sep_arg_done;}
      qbs_input_arguements[argn]->len=0;
      firstchr=1;
      goto qbs_input_next_arg;
    }
      }
    }

    if (inpstr->chr[i]==34){//"
      if (firstchr){
    if (!lineinput){
      i2=1;//requires closure
      firstchr=0;
      goto qbs_input_next_arg;
    }
      }
      if (i2==1){
    i2=2;
    goto qbs_input_next_arg;
      }
    }

    if (i2==2){
      goto backspace;//INVALID! Cannot have any characters after a closed "..."
    }

    c->chr[0]=inpstr->chr[i];
    qbs_set(qbs_input_arguements[argn],qbs_add(qbs_input_arguements[argn],c));

    firstchr=0;
  qbs_input_next_arg:;
    i++;
    goto qbs_input_sep_arg;
  }
 qbs_input_sep_arg_done:
  if (toomany) goto backspace;

  //validate current arguements
  //ASSUME LEADING & TRALING SPACES REMOVED!
  uint8 valid;
  uint8 neg;
  int32 completewith;
  int32 l;
  uint8 *cp,*cp2;
  uint64 max,max_neg,multiple,value;
  uint64 hexvalue;

  completewith=-1;
  valid=1;
  l=qbs_input_arguements[argn]->len;
  cp=qbs_input_arguements[argn]->chr;
  neg=0;

  if ((qbs_input_variabletypes[argn]&ISSTRING)==0){
    if ((qbs_input_variabletypes[argn]&ISFLOAT)==0){
      if ((qbs_input_variabletypes[argn]&511)<=32){//cannot handle INTEGER64 variables using this method!
    int64 finalvalue;
    //it's an integer variable!
    finalvalue=0;
    if (l==0){completewith=48; goto typechecked_integer;}
    //calculate max & max_neg (i4 used to store number of bits)
    i4=qbs_input_variabletypes[argn]&511;
    max=1;
    max<<=i4;
    max--;

    //check for hex/oct
    if (i3=hexoct2uint64(qbs_input_arguements[argn])){
      hexvalue=hexoct2uint64_value;
      if (hexvalue>max){valid=0; goto typechecked;}
      //i. check max num of "digits" required to represent a value, if more exist cull excess
      //ii. set completewith value (if necessary)
      if (i3==1){
        value=max;
        i=0;
        for (i2=1;i2<=16;i2++){
          if (value&0xF) i=i2;
          value>>=4;
        }
        if (l>(2+i)){valid=0; goto typechecked;}
        if (l==1) completewith=72;//"H"
        if (l==2) completewith=48;//"0"
      }
      if (i3==2){
        value=max;
        i=0;
        for (i2=1;i2<=22;i2++){
          if (value&0x7) i=i2;
          value>>=3;
        }
        if (l>(2+i)){valid=0; goto typechecked;}
        if (l==1) completewith=111;//"O"
        if (l==2) completewith=48;//"0"
      }
      finalvalue=hexvalue;
      goto typechecked_integer;
    }

    //max currently contains the largest UNSIGNED value possible, adjust as necessary
    if (qbs_input_variabletypes[argn]&ISUNSIGNED){ 
      max_neg=0;
    }else{
      max>>=1;
      max_neg=max+1;
    }
    //check for - sign
    i2=0;
    if ((qbs_input_variabletypes[argn]&ISUNSIGNED)==0){ 
      if (cp[i2]==45){//"-"
        if (l==1) {completewith=48; goto typechecked_integer;}
        i2++; neg=1;
      }
    }
    //after a leading 0 no other digits are possible, return an error if this is the case
    if (cp[i2]==48){
      if (l>(i2+1)){valid=0; goto typechecked;}
    }
    //scan the "number"...
    multiple=1;
    value=0;
    for (i=l-1;i>=i2;i--){
      i3=cp[i]-48;
      if ((i3>=0)&&(i3<=9)){
        value+=multiple*i3;
        if (qbs_input_variabletypes[argn]&ISUNSIGNED){ 
          if (value>max){valid=0; goto typechecked;}
        }else{
          if (neg){
        if (value>max_neg){valid=0; goto typechecked;}
          }else{
        if (value>max){valid=0; goto typechecked;}
          }
        }
      }else{valid=0; goto typechecked;}
      multiple*=10;
    }//next i
    if (neg) finalvalue=-value; else finalvalue=value;
      typechecked_integer:
    //set variable to finalvalue
    if ((qbs_input_variabletypes[argn]&ISOFFSETINBITS)==0){//reg. numeric variable
      memcpy(qbs_input_variableoffsets[argn],&finalvalue,(qbs_input_variabletypes[argn]&511)>>3);
    }
    goto typechecked;
      }
    }
  }

  if (qbs_input_variabletypes[argn]&ISSTRING){
    if (((qbs*)qbs_input_variableoffsets[argn])->fixed){
      if (l>((qbs*)qbs_input_variableoffsets[argn])->len) {valid=0; goto typechecked;}
    }
    qbs_set((qbs*)qbs_input_variableoffsets[argn],qbs_input_arguements[argn]);
    goto typechecked;
  }

  //INTEGER64 type
  //int64 range:          \969223372036854775808 to  9223372036854775807
  //uint64 range: 0                    to 18446744073709551615
  if ((qbs_input_variabletypes[argn]&ISSTRING)==0){
    if ((qbs_input_variabletypes[argn]&ISFLOAT)==0){
      if ((qbs_input_variabletypes[argn]&511)==64){
    if (l==0){completewith=48; *(int64*)qbs_input_variableoffsets[argn]=0; goto typechecked;}

    //check for hex/oct
    if (i3=hexoct2uint64(qbs_input_arguements[argn])){
      hexvalue=hexoct2uint64_value;
      if (hexvalue>max){valid=0; goto typechecked;}
      //set completewith value (if necessary)
      if (i3==1) if (l==1) completewith=72;//"H"
      if (i3==2) if (l==1) completewith=111;//"O"
      if (l==2) completewith=48;//"0"
      *(uint64*)qbs_input_variableoffsets[argn]=hexvalue;
      goto typechecked;
    }

    //check for - sign
    i2=0;
    if ((qbs_input_variabletypes[argn]&ISUNSIGNED)==0){ 
      if (cp[i2]==45){//"-"
        if (l==1) {completewith=48; *(int64*)qbs_input_variableoffsets[argn]=0; goto typechecked;}
        i2++; neg=1;
      }
    }
    //after a leading 0 no other digits are possible, return an error if this is the case
    if (cp[i2]==48){
      if (l>(i2+1)){valid=0; goto typechecked;}
    }
    //count how many digits are in the number
    i4=0;
    for (i=l-1;i>=i2;i--){
      i3=cp[i]-48;
      if ((i3<0)||(i3>9)) {valid=0; goto typechecked;}
      i4++;
    }//i
    if (qbs_input_variabletypes[argn]&ISUNSIGNED){
      if (i4<20) goto typechecked_int64;
      if (i4>20) {valid=0; goto typechecked;}

      cp2=(uint8*)uint64_max[0];
    }else{
      if (i4<19) goto typechecked_int64;
      if (i4>19) {valid=0; goto typechecked;}
      if (neg) cp2=(uint8*)int64_max_neg[0]; else cp2=(uint8*)int64_max[0];
    }
    //number of digits valid, but exact value requires checking
    cp=qbs_input_arguements[argn]->chr;
    for (i=0;i<i4;i++){
      if (cp[i+i2]<cp2[i]) goto typechecked_int64;
      if (cp[i+i2]>cp2[i]) {valid=0; goto typechecked;}
    }
      typechecked_int64:
    //add character 0 to end to make it a null terminated string
    c->chr[0]=0; qbs_set(qbs_input_arguements[argn],qbs_add(qbs_input_arguements[argn],c));
    if (qbs_input_variabletypes[argn]&ISUNSIGNED){
#ifdef QB64_WINDOWS
      sscanf((char*)qbs_input_arguements[argn]->chr,"%I64u",(uint64*)qbs_input_variableoffsets[argn]);
#else
      sscanf((char*)qbs_input_arguements[argn]->chr,"%llu",(uint64*)qbs_input_variableoffsets[argn]);
#endif
    }else{
#ifdef QB64_WINDOWS
      sscanf((char*)qbs_input_arguements[argn]->chr,"%I64i",(int64*)qbs_input_variableoffsets[argn]);
#else
      sscanf((char*)qbs_input_arguements[argn]->chr,"%lli",(int64*)qbs_input_variableoffsets[argn]);
#endif
    }
    goto typechecked;
      }
    }
  }

  //check ISFLOAT type?
  //[-]9999[.]9999[E/D][+/-]99999
  if (qbs_input_variabletypes[argn]&ISFLOAT){
    static int32 digits_before_point;
    static int32 digits_after_point;
    static int32 zeros_after_point;
    static int32 neg_power;
    digits_before_point=0;
    digits_after_point=0;
    neg_power=0;
    value=0;
    zeros_after_point=0;
    num_significant_digits=0;

    //set variable to 0
    if ((qbs_input_variabletypes[argn]&511)==32) *(float*)qbs_input_variableoffsets[argn]=0;
    if ((qbs_input_variabletypes[argn]&511)==64) *(double*)qbs_input_variableoffsets[argn]=0;
    if ((qbs_input_variabletypes[argn]&511)==256) *(long double*)qbs_input_variableoffsets[argn]=0;

    //begin with a generic assessment, regardless of whether it is single, double or float
    if (l==0){completewith=48; goto typechecked;}

    //check for hex/oct
    if (i3=hexoct2uint64(qbs_input_arguements[argn])){
      hexvalue=hexoct2uint64_value;
      //set completewith value (if necessary)
      if (i3==1) if (l==1) completewith=72;//"H"
      if (i3==2) if (l==1) completewith=111;//"O"
      if (l==2) completewith=48;//"0"
      //nb. because VC6 didn't support...
      //error C2520: conversion from uint64 to double not implemented, use signed int64
      //I've implemented a work-around so correct values will be returned
      static int64 transfer;
      transfer=0x7FFFFFFF;
      transfer<<=32;
      transfer|=0xFFFFFFFF;
      while(hexvalue>transfer){
    hexvalue-=transfer;
    if ((qbs_input_variabletypes[argn]&511)==32) *(float*)qbs_input_variableoffsets[argn]+=transfer;
    if ((qbs_input_variabletypes[argn]&511)==64) *(double*)qbs_input_variableoffsets[argn]+=transfer;
    if ((qbs_input_variabletypes[argn]&511)==256) *(long double*)qbs_input_variableoffsets[argn]+=transfer;
      }
      transfer=hexvalue;
      if ((qbs_input_variabletypes[argn]&511)==32) *(float*)qbs_input_variableoffsets[argn]+=transfer;
      if ((qbs_input_variabletypes[argn]&511)==64) *(double*)qbs_input_variableoffsets[argn]+=transfer;
      if ((qbs_input_variabletypes[argn]&511)==256) *(long double*)qbs_input_variableoffsets[argn]+=transfer;
      goto typechecked;
    }

    //check for - sign
    i2=0;
    if (cp[i2]==45){//"-"
      if (l==1) {completewith=48; goto typechecked;}
      i2++; neg=1;
    }
    //if it starts with 0, it may only have one leading 0
    if (cp[i2]==48){
      if (l>(i2+1)){
    i2++;
    if (cp[i2]==46) goto decimal_point;
    valid=0; goto typechecked;//expected a decimal point
    //nb. of course, user could have typed D or E BUT there is no point
    //    calculating 0 to the power of anything!
      }else goto typechecked;//validate, as no other data is required
    }
    //scan digits before decimal place
    for (i=i2;i<l;i++){
      i3=cp[i];
      if ((i3==68)||(i3==(68+32))||(i3==69)||(i3==(69+32))){//d,D,e,E?
    if (i==i2){valid=0; goto typechecked;}//cannot begin with d,D,e,E!
    i2=i;
    goto exponent;
      }
      if (i3==46){i2=i; goto decimal_point;}//nb. it can begin with a decimal point!
      i3-=48;
      if ((i3<0)||(i3>9)){valid=0; goto typechecked;}
      digits_before_point++;
      //nb. because leading 0 is handled differently, all digits are significant
      significant_digits[num_significant_digits]=i3+48; num_significant_digits++;
    }
    goto assess_float;
    ////////////////////////////////
  decimal_point:;
    i4=1;
    if (i2==(l-1)) {completewith=48; goto assess_float;}
    i2++;
    for (i=i2;i<l;i++){
      i3=cp[i];
      if ((i3==68)||(i3==(68+32))||(i3==69)||(i3==(69+32))){//d,D,e,E?
    if (num_significant_digits){
      if (i==i2){valid=0; goto typechecked;}//cannot begin with d,D,e,E just after a decimal point!
      i2=i;
      goto exponent;
    }
      }
      i3-=48;
      if ((i3<0)||(i3>9)){valid=0; goto typechecked;}
      if (i3) i4=0;
      if (i4) zeros_after_point++;
      digits_after_point++;
      if ((num_significant_digits)||i3){
    significant_digits[num_significant_digits]=i3+48; num_significant_digits++;
      }
    }//i
    goto assess_float;
    ////////////////////////////////
  exponent:;

    //ban d/D for SINGLE precision input
    if ((qbs_input_variabletypes[argn]&511)==32){//SINGLE
      i3=cp[i2];
      if ((i3==68)||(i3==(68+32))){//d/D
    valid=0; goto typechecked;
      }
    }
    //correct "D" notation for c++ scanf
    i3=cp[i2];
    if ((i3==68)||(i3==(68+32))){//d/D
      cp[i2]=69;//"E"
    }
    if (i2==(l-1)) {completewith=48; goto assess_float;}
    i2++;
    //check for optional + or -
    i3=cp[i2];
    if (i3==45){//"-"
      if (i2==(l-1)) {completewith=48; goto assess_float;}
      neg_power=1;
      i2++;
    }
    if (i3==43){//"+"
      if (i2==(l-1)) {completewith=48; goto assess_float;}
      i2++;
    }
    //nothing valid after a leading 0
    if (cp[i2]==48){//0
      if (l>(i2+1)) {valid=0; goto typechecked;}
    }
    multiple=1;
    value=0;
    for (i=l-1;i>=i2;i--){
      i3=cp[i]-48;
      if ((i3>=0)&&(i3<=9)){

    value+=multiple*i3;
      }else{
    valid=0; goto typechecked;
      }
      multiple*=10;
    }//i
    //////////////////////////
  assess_float:;
    //nb. 0.???? means digits_before_point==0

    if ((qbs_input_variabletypes[argn]&511)==32){//SINGLE
      //QB:           \B13.402823    E+38 to \B11.401298    E-45
      //WIKIPEDIA:    \B13.4028234   E+38 to ?
      //OTHER SOURCE: \B13.402823466 E+38 to \B11.175494351 E-38
      if (neg_power) value=-value;
      //special case->single 0 after point
      if ((zeros_after_point==1)&&(digits_after_point==1)){
    digits_after_point=0;
    zeros_after_point=0;
      }
      //upper overflow check
      //i. check that value doesn't consist solely of 0's
      if (zeros_after_point>43){valid=0; goto typechecked;}//cannot go any further without reversal by exponent
      if ((digits_before_point==0)&&(digits_after_point==zeros_after_point)) goto nooverflow_float;
      //ii. calculate the position of the first WHOLE digit (in i)
      i=digits_before_point;
      if (!i) i=-zeros_after_point;
      /*EXAMPLES:
    1.0         i=1
    12.0        i=2
    0.1         i=0
    0.01        i=-1
      */
      i=i+value;//apply exponent
      if (i>39){valid=0; goto typechecked;}
      //nb. the above blocks the ability to type a long-int32 number and use a neg exponent
      //    to validate it
      //********IMPORTANT: if i==39 then the first 7 digits MUST be scanned!!!
      if (i==39){
    cp2=(uint8*)single_max[0];
    i2=num_significant_digits;
    if (i2>7) i2=7;
    for (i3=0;i3<i2;i3++){
      if (significant_digits[i3]>*cp2){valid=0; goto typechecked;}
      if (significant_digits[i3]<*cp2) break;
      cp2++;
    }
      }
      //check for pointless levels of precision (eg. 1.21351273512653625116212!)
      if (digits_after_point){
    if (digits_before_point){
      if ((digits_after_point+digits_before_point)>8){valid=0; goto typechecked;}
    }else{
      if ((digits_after_point-zeros_after_point)>8){valid=0; goto typechecked;}
    }
      }
      //check for "under-flow"
      if (i<-44){valid=0; goto typechecked;}
      //********IMPORTANT: if i==-44 then the first 7 digits MUST be scanned!!!
      if (i==-44){
    cp2=(uint8*)single_max_neg[0];
    i2=num_significant_digits;
    if (i2>7) i2=7;
    for (i3=0;i3<i2;i3++){
      if (significant_digits[i3]<*cp2){valid=0; goto typechecked;}
      if (significant_digits[i3]>*cp2) break;
      cp2++;
    }
      }
    nooverflow_float:;
      c->chr[0]=0; qbs_set(qbs_input_arguements[argn],qbs_add(qbs_input_arguements[argn],c));
      sscanf((char*)qbs_input_arguements[argn]->chr,"%f",(float*)qbs_input_variableoffsets[argn]);
      goto typechecked;
    }

    if ((qbs_input_variabletypes[argn]&511)==64){//DOUBLE
      //QB: Double (15-digit) precision \B11.7976931 D+308 to \B14.940656 D-324
      //WIKIPEDIA:    \B11.7976931348623157 D+308 to ???
      //OTHER SOURCE: \B11.7976931348623157 D+308 to \B12.2250738585072014E-308



      if (neg_power) value=-value;
      //special case->single 0 after point
      if ((zeros_after_point==1)&&(digits_after_point==1)){
    digits_after_point=0;
    zeros_after_point=0;
      }
      //upper overflow check
      //i. check that value doesn't consist solely of 0's
      if (zeros_after_point>322){valid=0; goto typechecked;}//cannot go any further without reversal by exponent
      if ((digits_before_point==0)&&(digits_after_point==zeros_after_point)) goto nooverflow_double;
      //ii. calculate the position of the first WHOLE digit (in i)
      i=digits_before_point;
      if (!i) i=-zeros_after_point;
      i=i+value;//apply exponent
      if (i>309){valid=0; goto typechecked;}
      //nb. the above blocks the ability to type a long-int32 number and use a neg exponent
      //    to validate it
      //********IMPORTANT: if i==309 then the first 8 digits MUST be scanned!!!
      if (i==309){
    cp2=(uint8*)double_max[0];
    i2=num_significant_digits;
    if (i2>8) i2=8;
    for (i3=0;i3<i2;i3++){
      if (significant_digits[i3]>*cp2){valid=0; goto typechecked;}
      if (significant_digits[i3]<*cp2) break;
      cp2++;
    }
      }
      //check for pointless levels of precision (eg. 1.21351273512653625116212!)
      if (digits_after_point){
    if (digits_before_point){
      if ((digits_after_point+digits_before_point)>16){valid=0; goto typechecked;}
    }else{
      if ((digits_after_point-zeros_after_point)>16){valid=0; goto typechecked;}
    }
      }
      //check for "under-flow"
      if (i<-323){valid=0; goto typechecked;}
      //********IMPORTANT: if i==-323 then the first 7 digits MUST be scanned!!!
      if (i==-323){
    cp2=(uint8*)double_max_neg[0];
    i2=num_significant_digits;
    if (i2>7) i2=7;
    for (i3=0;i3<i2;i3++){
      if (significant_digits[i3]<*cp2){valid=0; goto typechecked;}
      if (significant_digits[i3]>*cp2) break;
      cp2++;
    }
      }
    nooverflow_double:;
      c->chr[0]=0; qbs_set(qbs_input_arguements[argn],qbs_add(qbs_input_arguements[argn],c));
      sscanf((char*)qbs_input_arguements[argn]->chr,"%lf",(double*)qbs_input_variableoffsets[argn]);
      goto typechecked;
    }

    if ((qbs_input_variabletypes[argn]&511)==256){//FLOAT
      //at present, there is no defined limit for FLOAT type numbers, so no restrictions
      //are applied!
      c->chr[0]=0; qbs_set(qbs_input_arguements[argn],qbs_add(qbs_input_arguements[argn],c));

      //sscanf((char*)qbs_input_arguements[argn]->chr,"%lf",(long double*)qbs_input_variableoffsets[argn]);
      static double sscanf_fix;
      sscanf((char*)qbs_input_arguements[argn]->chr,"%lf",&sscanf_fix);
      *(long double*)qbs_input_variableoffsets[argn]=sscanf_fix;

    }


  }//ISFLOAT

  //undefined/uncheckable types fall through as valid!
 typechecked:;
  if (!valid) goto backspace;



  qbs_set(inpstr2,inpstr);


  //input a key



 qbs_input_invalidinput:

  static int32 showing_cursor;
  showing_cursor=0;

 qbs_input_wait_for_key:

  //toggle box cursor
  if (!write_page->text){
    i=1;
    if ((write_page->font>=32)||(write_page->compatible_mode==256)||(write_page->compatible_mode==32)){
      if (GetTicks()&512) i=0;
    }
    if (i!=showing_cursor){
      showing_cursor^=1;
      static int32 x,y,x2,y2,fx,fy,alpha,cw;
      static uint32 c;
      alpha=write_page->alpha_disabled; write_page->alpha_disabled=1;
      fy=fontheight[write_page->font];
      fx=fontwidth[write_page->font]; if (!fx) fx=1;
      cw=fx; if ((write_page->font>=32)||(write_page->compatible_mode==256)||(write_page->compatible_mode==32)) cw=1;
      y2=(write_page->cursor_y-1)*fy;
      for (y=0;y<fy;y++){
    x2=(write_page->cursor_x-1)*fx;
    for (x=0;x<cw;x++){
      pset (x2,y2,point(x2,y2)^write_page->color);
      x2++;
    }
    y2++;
      }
      write_page->alpha_disabled=alpha;
    }
  }//!write_page->text

  if (addspaces){
    addspaces--;
    c->chr[0]=32; qbs_set(key,c);
  }else{

    if (write_page->console){
      qbs_set(key,qbs_new_txt(""));
      chr=fgetc(stdin);
      if (chr!=EOF){
    if (chr=='\n') chr=13;
    qbs_set(key,qbs_new_txt(" "));  
    key->chr[0]=chr;  
      }else{Sleep(10);}
    }else{
      Sleep(10);
      qbs_set(key,qbs_inkey());
      
      disableEvents=1;//we don't want the ON TIMER bound version of VKUPDATE to fire during a call to itself!
      SUB_VKUPDATE();
      disableEvents=0;

    }

    qbs_cleanup(qbs_tmp_base,0);
  }
  if (stop_program) return;
  if (key->len!=1) goto qbs_input_wait_for_key;

  //remove box cursor
  if (!write_page->text){
    if (showing_cursor){
      showing_cursor=0;
      static int32 x,y,x2,y2,fx,fy,cw,alpha;
      static uint32 c;
      alpha=write_page->alpha_disabled; write_page->alpha_disabled=1;
      fy=fontheight[write_page->font];
      fx=fontwidth[write_page->font]; if (!fx) fx=1;
      cw=fx; if ((write_page->font>=32)||(write_page->compatible_mode==256)||(write_page->compatible_mode==32)) cw=1;
      y2=(write_page->cursor_y-1)*fy;
      for (y=0;y<fy;y++){
    x2=(write_page->cursor_x-1)*fx;
    for (x=0;x<cw;x++){
      pset (x2,y2,point(x2,y2)^write_page->color);
      x2++;
    }
    y2++;
      }
      write_page->alpha_disabled=alpha;
    }
  }//!write_page->text

  //input should disallow certain characters
  if (key->chr[0]==7) {qbs_print(key,0); goto qbs_input_next;}//beep!
  if (key->chr[0]==10) goto qbs_input_next;//linefeed
  if (key->chr[0]==9){//tab
    i=8-(inpstr2->len&7);
    addspaces=i;
    goto qbs_input_next;
  }
  //other ASCII chars that cannot be printed
  if (key->chr[0]==11) goto qbs_input_next;
  if (key->chr[0]==12) goto qbs_input_next;
  if (key->chr[0]==28) goto qbs_input_next;
  if (key->chr[0]==29) goto qbs_input_next;
  if (key->chr[0]==30) goto qbs_input_next;
  if (key->chr[0]==31) goto qbs_input_next;

  if (key->chr[0]==13){
    //assume input is valid

    //autofinish (if necessary)

    //assume all parts entered

    for (i=1;i<=numvariables;i++){
      qbs_free(qbs_input_arguements[i]);
    }

    if (newline){
      c->len=0;
      if (!write_page->console) qbs_print(c,1);
    }
    qbs_free(c);
    qbs_free(inpstr);
    qbs_free(inpstr2);
    qbs_free(key);

    write_page->cursor_show=cursor_show_last;

    sub__freeimage(dest_image,1); sub__freeimage(dest_image_temp,1);

    if (autodisplay_backup==0){
      autodisplay=-1;//toggle request
      while(autodisplay) Sleep(1);
    }

    sub__source(source_backup);

    return;
  }

  if (key->chr[0]==8){//backspace
  backspace:
    if (!inpstr->len) goto qbs_input_invalidinput;
    inpstr->len--;
    i2=func__dest();//backup current dest
    sub_pcopy(dest_image,dest_image_temp);//copy original background to temp
    //write characters to temp
    sub__dest(dest_image_temp);
    write_page->cursor_x=dest_image_cursor_x; write_page->cursor_y=dest_image_cursor_y; write_page->holding_cursor=dest_image_holding_cursor;
    for (i=0;i<inpstr->len;i++){key->chr[0]=inpstr->chr[i]; qbs_print(key,0);}
    sub__dest(i2);
    //copy temp to dest
    sub_pcopy(dest_image_temp,i2);
    //update cursor
    write_page->cursor_x=img[-dest_image_temp].cursor_x; write_page->cursor_y=img[-dest_image_temp].cursor_y;
    goto qbs_input_next;
  }

  if (inpstr2->len>=255) goto qbs_input_next;

  //affect inpstr2 with key
  qbs_set(inpstr2,qbs_add(inpstr2,key));

  //perform actual update
  if (!write_page->console) qbs_print(key,0);

  qbs_set(inpstr,inpstr2);

  goto qbs_input_next;

}//qbs_input

long double func_val(qbs *s){
  char c;
  static int32 i,i2,step,num_significant_digits,most_significant_digit_position;
  static int32 num_exponent_digits;
  static int32 negate,negate_exponent;
  static uint8 significant_digits[256];
  static int64 exponent_value;
  static uint8 built_number[256];
  static long double return_value;
  static int64 value;
  static int64 hex_value;
  static int32 hex_digits;
  if (!s->len) return 0;
  value=0;
  negate_exponent=0;
  num_exponent_digits=0;
  num_significant_digits=0;
  most_significant_digit_position=0;
  step=0;
  exponent_value=0;
  negate=0;

  i=0;
  for (i=0;i<s->len;i++){
    c=(char)s->chr[i];
    switch (c) {
    case ' ':
    case '\t':
      goto whitespace;
      break;

    case '&':
      if (step==0) goto hex;
      goto finish;
      break;

    case '-':
      if (step==0) {
        negate = 1;
        step = 1;
        goto checked;
      }
      else if (step==3) {
        negate_exponent = 1;
        step = 4;
        goto checked;
      }
      goto finish;
      break;

    case '+':
      if (step==0 || step==3) {
        step++;
        goto checked;
      }
      goto finish;
      break;

    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
      if (step<=1){//before decimal point
        step=1;
        if ((num_significant_digits)||(c>48)){
          most_significant_digit_position++;
          significant_digits[num_significant_digits]=c;
          num_significant_digits++;
          value=value*10+c-48;
        }
      }      
      else if (step==2){//after decimal point
        if ((num_significant_digits==0)&&(c==48)) most_significant_digit_position--;
        if ((num_significant_digits)||(c>48)){
          significant_digits[num_significant_digits]=c;
          num_significant_digits++;
        }
      }
      
      else if (step>=3){//exponent
        step=4;
        if ((num_exponent_digits)||(c>48)){
          if (num_exponent_digits>=18) goto finish;
          exponent_value*=10; exponent_value=exponent_value+c-48;//precalculate
          num_exponent_digits++;
        }
      }
      goto checked;
      break;

    case '.':
      if (step>1) goto finish;
      step=2; goto checked;
      break;
      
    case 'D':
    case 'E':
    case 'd':
    case 'e':
      if (step>2) goto finish;
      step=3; goto checked;
      break;

    default:
    goto finish;//invalid character
    break;
    }
    
  checked:
  whitespace:;
  }
 finish:;

  //Check for all-zero mantissa
  if (num_significant_digits==0) return 0;

  //If no exponent (or E0) and no decimal part and no chance of overflowing value, return straight away
  if (exponent_value==0 && num_significant_digits==most_significant_digit_position && num_significant_digits < 19){
    return negate ? -value : value;
  }
  
  //normalise number (change 123.456E2 to 1.23456E4, or 123.456 to 1.23456E2)
  exponent_value=exponent_value+most_significant_digit_position-1;

  if (negate_exponent) exponent_value=-exponent_value;
  i=0;
  //we are now building a floating point number in ascii characters
  if (negate) {built_number[i]=45; i++;}//-
  if (num_significant_digits){
    //build nomalised mantissa 
    for (i2=0;i2<num_significant_digits;i2++){
      if (i2==1){
        built_number[i]=46;
        i++;
      }
      built_number[i]=significant_digits[i2]; i++;
    }
    built_number[i]=69; i++;//E
    //add exponent
#ifdef QB64_WINDOWS
    i2=sprintf((char*)&built_number[i],"%I64i",exponent_value);
#else
    i2=sprintf((char*)&built_number[i],"%lli",exponent_value);
#endif
    i=i+i2;
  }else{
    built_number[i]=48; i++;//0
  }
  built_number[i]=0;//NULL terminate

#ifdef QB64_MINGW
  __mingw_sscanf((char*)&built_number[0],"%Lf",&return_value);
#else 
  sscanf((char*)&built_number[0],"%Lf",&return_value);
#endif
  return return_value;

 hex://hex/oct
  if (i>=(s->len-2)) return 0;
  c=s->chr[i+1];
  if ((c==79)||(c==111)){//"O"or"o"
    hex_digits=0;
    hex_value=0;
    for (i=i+2;i<s->len;i++){
      c=s->chr[i];
      if ((c>=48)&&(c<=55)){//0-7
    c-=48;
    hex_value<<=3;
    hex_value|=c;
    if (hex_digits||c) hex_digits++; 
    if (hex_digits>=22){
      if ((hex_digits>22)||(s->chr[i-21]>49)){error(6); return 0;}
    }
      }else break;
    }//i
    return hex_value;
  }
  if ((c==66)||(c==98)){//"B"or"b"
      hex_digits=0;
      hex_value=0;
      for (i=i+2;i<s->len;i++){
      c=s->chr[i];
      if ((c>47)&&(c<50)){//0-1
          c-=48;
          hex_value<<=1;
          hex_value|=c;
          if (hex_digits||c) hex_digits++; 
          if (hex_digits>64){error(6); return 0;}
      }else 
              break;
      }//i
      return hex_value;
  }
  if ((c==72)||(c==104)){//"H"or"h"
    hex_digits=0;
    hex_value=0;
    for (i=i+2;i<s->len;i++){
      c=s->chr[i];
      if ( ((c>=48)&&(c<=57)) || ((c>=65)&&(c<=70)) || ((c>=97)&&(c<=102)) ){//0-9 or A-F or a-f
    if ((c>=48)&&(c<=57)) c-=48;
    if ((c>=65)&&(c<=70)) c-=55;

    if ((c>=97)&&(c<=102)) c-=87;
    hex_value<<=4;
    hex_value|=c;
    if (hex_digits||c) hex_digits++;
    if (hex_digits>16) {error(6); return 0;}
      }else break;
    }//i
    return hex_value;
  }
  return 0;//& followied by unknown
}






int32 unsupported_port_accessed=0;

int32 H3C7_palette_register_read_index=0;
int32 H3C8_palette_register_index=0;
int32 H3C9_next=0;
int32 H3C9_read_next=0;

int32 H3C0_blink_enable = 1;

void sub__blink(int32 onoff){
  if (onoff==1) H3C0_blink_enable=1; else H3C0_blink_enable=0;
}

int32 func__blink(){
  return -H3C0_blink_enable;
}

int32 func__handle(){
    #ifdef QB64_GUI
        #ifdef QB64_WINDOWS
            while (!window_handle){Sleep(100);}
            return (int32)window_handle;
        #endif
    #endif

    return 0;
}

qbs *func__title(){
    if (!window_title){
      return qbs_new_txt("");
    }else{
      return qbs_new_txt((char*)window_title);
    }
}

int32 func__hasfocus() {
    #ifdef QB64_GUI
        #ifdef QB64_WINDOWS
            while (!window_handle){Sleep(100);}
            return -(window_handle==GetForegroundWindow());
        #elif defined(QB64_LINUX)
            return window_focused;
        #endif
    #endif
    return -1;
}

void sub_out(int32 port,int32 data){
  if (new_error) return;
  unsupported_port_accessed=0;
  port=port&65535;
  data=data&255;

  if (port==0x3C0) {
	  H3C0_blink_enable = data&(1<<3);
	  goto done;
  }
  
  if (port==0x3C7){//&H3C7, set palette register read index
    H3C7_palette_register_read_index=data;
    H3C9_read_next=0;
    goto done;
  }
  if (port==968){//&H3C8, set palette register write index
    H3C8_palette_register_index=data;
    H3C9_next=0;
    goto done;
  }
  if (port==969){//&H3C9, set palette color
    data=data&63;
    if (write_page->pal){//avoid NULL pointer
      if (H3C9_next==0){//red
    write_page->pal[H3C8_palette_register_index]&=0xFF00FFFF;
    write_page->pal[H3C8_palette_register_index]+=(qbr((double)data*4.063492f-0.4999999f)<<16);
      }
      if (H3C9_next==1){//green
    write_page->pal[H3C8_palette_register_index]&=0xFFFF00FF;
    write_page->pal[H3C8_palette_register_index]+=(qbr((double)data*4.063492f-0.4999999f)<<8);
      }
      if (H3C9_next==2){//blue
    write_page->pal[H3C8_palette_register_index]&=0xFFFFFF00;
    write_page->pal[H3C8_palette_register_index]+=qbr((double)data*4.063492f-0.4999999f);
      }
    }
    H3C9_next=H3C9_next+1;
    if (H3C9_next==3){
      H3C9_next=0;
      H3C8_palette_register_index=H3C8_palette_register_index+1;
      H3C8_palette_register_index&=0xFF;
    }
    goto done;
  }

  unsupported_port_accessed=1;
 done:
  return;
 error:
  error(5);
}

uint32 rnd_seed=327680;
uint32 rnd_seed_first=327680;//Note: must contain the same value as rnd_seed

void sub_randomize (double seed,int32 passed){
  if (new_error) return;

  if (passed==3){//USING
    //Dim As Uinteger m = cptr(Uinteger Ptr, @n)[1]
    static uint32 m;
    m=((uint32*)&seed)[1];
    //m Xor= (m Shr 16)
    m^=(m>>16);
    //rnd_seed = (m And &hffff) Shl 8 Or (rnd_seed And &hff)
    rnd_seed=((m&0xffff)<<8)|(rnd_seed_first&0xff);//Note: rnd_seed changed to rnd_seed_first
    return;
  }

  if (passed==1){
    //Dim As Uinteger m = cptr(Uinteger Ptr, @n)[1]
    static uint32 m;
    m=((uint32*)&seed)[1];
    //m Xor= (m Shr 16)
    m^=(m>>16);
    //rnd_seed = (m And &hffff) Shl 8 Or (rnd_seed And &hff)
    rnd_seed=((m&0xffff)<<8)|(rnd_seed&0xff);
    return;
  }

  qbs_print(qbs_new_txt("Random-number seed (-32768 to 32767)? "),0);
  static int16 integerseed;
  qbs_input_variabletypes[1]=16;//id.t=16 'a signed 16 bit integer
  qbs_input_variableoffsets[1]=&integerseed;
  qbs_input(1,1);
  //rnd_seed = (m And &hffff) Shl 8 Or (rnd_seed And &hff) 'nb. same as above
  rnd_seed=((integerseed&0xffff)<<8)|(rnd_seed&0xff);
  return;
}

float func_rnd(float n,int32 passed){
  if (new_error) return 0;

  static uint32 m;
  if (!passed) n=1.0f;
  if (n!=0.0){
    if (n<0.0){
      m=*((uint32*)&n);
      rnd_seed=(m&0xFFFFFF)+((m&0xFF000000)>>24);
    }
    rnd_seed=(rnd_seed*16598013+12820163)&0xFFFFFF;
  }     
  return (double)rnd_seed/0x1000000;
}

double func_timer(double accuracy,int32 passed){
  if (new_error) return 0;
  static uint32 x;
  static double d;
  static float f;
  x=GetTicks();
  x-=clock_firsttimervalue;
  x+=qb64_firsttimervalue;
  //make timer value loop after midnight
  //note: there are 86400000 milliseconds in 24hrs(1 day)
  x%=86400000;
  d=x;//convert to double
  d/=1000.0;//convert from ms to sec
  //reduce accuracy
  if (!passed){
    accuracy=18.2;
  }else{
    if (accuracy<=0.0){error(5); return 0;}
    accuracy=1.0/accuracy;
  }
  d*=accuracy;
  d=qbr(d);
  d/=accuracy;
  if (!passed){f=d; d=f;}
  return d;
}

void sub__delay(double seconds){
  double ms,base,elapsed,prev_now,now;//cannot be static
  base=GetTicks();
  if (new_error) return;
  if (seconds<0){error(5); return;}
  if (seconds>2147483.647){error(5); return;}
  ms=seconds*1000.0;
  now=base;//force first prev=... assignment to equal base
 recalculate:
  prev_now=now;
  now=GetTicks();
  elapsed=now-base;
  if (elapsed<0){//GetTicks looped
    base=now-(prev_now-base);//calculate new base
  }
  if (elapsed<ms){
    int64 wait;//cannot be static
    wait=ms-elapsed;
    if (!wait) wait=1;
    if (wait>=10){
      Sleep(9);
      evnt(0);//check for new events
      //recalculate time
      goto recalculate;
    }else{
      Sleep(wait);
    }
  }
}

void sub__fps(double fps, int32 passed){
//passed=1 means _AUTO
//passed=2 means use fps
if (new_error) return;
if (passed!=1 && passed!=2){error(5); return;}
if (passed==1){
  auto_fps=1;//_AUTO
}
if (passed==2){
  if (fps<1){error(5); return;}
  if (fps>200) fps=200;
  max_fps=fps;
  auto_fps=0;
}
}

void sub__limit(double fps){
  if (new_error) return;
  static double prev=0;
  double ms,now,elapsed;//cannot be static
  if (fps<=0.0){error(5); return;}
  ms=1000.0/fps;
  if (ms>60000.0){error(5); return;}//max. 1 min delay between frames allowed to avoid accidental lock-up of program
 recalculate:
  now=GetTicks();
  if (prev==0.0){//first call?
    prev=now;
    return;
  }
  if (now<prev){//value looped?
    prev=now;
    return;
  }
  elapsed=now-prev;//elapsed time since prev

  if (elapsed==ms){
    prev=prev+ms;
    return; 
  }

  if (elapsed<ms){
    int64 wait;//cannot be static
    wait=ms-elapsed;
    if (!wait) wait=1;
    if (wait>=10){
      Sleep(9);
      evnt(0);//check for new events
    }else{
      Sleep(wait);  
    }
    //recalculate time
    goto recalculate;
  }

  //too long since last call, adjust prev to current time
  //minor overshoot up to 32ms is recovered, otherwise time is re-seeded
  if (elapsed<=(ms+32.0)) prev=prev+ms; else prev=now;
}





int32 generic_put(int32 i,int32 offset,uint8 *cp,int32 bytes){
  //note: generic_put & generic_get have been made largely redundant by gfs_read & gfs_write
  //      offset is a byte-offset from base 0 (-1=current pos)
  //      generic_put has been kept 32-bit for compatibility
  //      the return value of generic_put is always 0
  //      though errors are handled, generic_put should only be called in error-less situations
  if (new_error) return 0;
  if (gfs_fileno_valid(i)!=1){error(52); return 0;}//Bad file name or number
  i=gfs_fileno[i];//convert fileno to gfs index
  static gfs_file_struct *gfs;
  gfs=&gfs_file[i];
  if (!gfs->write){error(75); return 0;}//Path/file access error
  static int32 e;
  e=gfs_write(i,offset,(uint8*)cp,bytes);
  if (e){
    if (e==-2){error(258); return 0;}//invalid handle
    if (e==-3){error(54); return 0;}//bad file mode
    if (e==-4){error(5); return 0;}//illegal function call
    if (e==-7){error(70); return 0;}//permission denied
    error(75); return 0;//assume[-9]: path/file access error
  }
  return 0;
}

int32 generic_get_bytes_read;
int32 generic_get(int32 i,int32 offset,uint8 *cp,int32 bytes){
  //note: generic_put & generic_get have been made largely redundant by gfs_read & gfs_write
  //      offset is a byte-offset from base 0 (-1=current pos)
  //      generic_get has been kept 32-bit for compatibility
  //      the return value of generic_get is always 0
  //      though errors are handled, generic_get should only be called in error-less situations
  generic_get_bytes_read=0;
  if (new_error) return 0;
  if (gfs_fileno_valid(i)!=1){error(52); return 0;}//Bad file name or number
  i=gfs_fileno[i];//convert fileno to gfs index
  static gfs_file_struct *gfs;
  gfs=&gfs_file[i];
  if (!gfs->read){error(75); return 0;}//Path/file access error
  static int32 e;
  e=gfs_read(i,offset,(uint8*)cp,bytes);
  generic_get_bytes_read=gfs_read_bytes();
  if (e){
    if (e!=-10){//note: on eof, unread buffer area becomes NULL
      if (e==-2){error(258); return 0;}//invalid handle
      if (e==-3){error(54); return 0;}//bad file mode
      if (e==-4){error(5); return 0;}//illegal function call
      if (e==-7){error(70); return 0;}//permission denied
      error(75); return 0;//assume[-9]: path/file access error
    }
  }
  return 0;
}



void sub_open(qbs *name,int32 type,int32 access,int32 sharing,int32 i,int64 record_length,int32 passed){
  if (new_error) return;
  //?[{FOR RANDOM|FOR BINARY|FOR INPUT|FOR OUTPUT|FOR APPEND}]
  //1 2
  //[{ACCESS READ WRITE|ACCESS READ|ACCESS WRITE}]
  //  3
  //[{SHARED|LOCK READ WRITE|LOCK READ|LOCK WRITE}]{AS}[#]?[{LEN =}?]
  //  4                                                   5        6[1]
  static int32 x;
  static int32 g_access,g_restrictions,g_how;

  if (type==0) type=1;
  if (passed) if ((record_length==0)||(record_length<-1)){error(5); return;}//Illegal function call
  //note: valid record_length values are allowable but ignored by QB for non-RANDOM modes too!

  x=gfs_fileno_valid(i);
  if (x==-2){error(52); return;}//Bad file name or number
  if (x==1){error(55); return;}//File already open

  if (type<=2){g_access=3; g_restrictions=0; g_how=3;}
  if (type==3){g_access=1; g_restrictions=0; g_how=0;}
  if (type==4){g_access=2; g_restrictions=0; g_how=2;}
  if (type==5){g_access=2; g_restrictions=0; g_how=1;}

  if (access==1) g_access=3;
  if (access==2) g_access=1;
  if (access==3) g_access=2;
  if (access&&(g_how==3)) g_how=1;//undefined access not possible when ACCESS is explicitly specified

  if (sharing==1) g_restrictions=0;
  if (sharing==2) g_restrictions=3;
  if (sharing==3) g_restrictions=1;
  if (sharing==4) g_restrictions=2;
  if (cloud_app) g_restrictions=0;//applying restrictions on server not possible
  //note: In QB, opening a file already open for OUTPUT/APPEND created the 'file already open' error.

  //      However, from a new cmd window (or a SHELLed QB program) it can be opened!
  //      So it is not a true OS restriction/lock, just a block applied internally by QB.
  //      This is currently unsupported by QB64.

  x=gfs_open(name,g_access,g_restrictions,g_how);
  if (x<0){
    if (x==-5){error(53); return;}
    if (x==-6){error(76); return;}
    if (x==-7){error(70); return;}
    if (x==-8){error(68); return;}
    if (x==-11){error(64); return;}
    if (x==-12){error(54); return;}
    error(53); return;//default assumption: 'file not found'
  }

  gfs_fileno_use(i,x);

  static gfs_file_struct *f; f=&gfs_file[x];

  f->type=type; if (type==5) f->type=4;

  f->column=1;

  if (type==1){//set record length
    f->record_length=128;
    if (passed) if (record_length!=-1) f->record_length=record_length;
    f->field_buffer=(uint8*)calloc(record_length,1);
  }

  if (type==5){//seek eof
    static int64 x64;
    x64=gfs_lof(x);
    if (x64>0) gfs_setpos(x,x64);//not an error and not null length
  }

  if (type==3){//check if eof character, CHR$(26), is the first byte and set EOF accordingly
    static int64 x64;
    x64=gfs_lof(x);
    if (x64){
      //read first byte 
      static uint8 c;
      static int32 e;
      if (e=gfs_read(x,-1,&c,1)){
    //if (e==-10) return -1;
    //if (e==-2){error(258); return -2;}//invalid handle
    //if (e==-3){error(54); return -2;}//bad file mode
    //if (e==-4){error(5); return -2;}//illegal function call
    if (e==-7){error(70); return;}//permission denied
    error(75); return;//assume[-9]: path/file access error
      }
      if (c==26){
    gfs_file[x].eof_passed=1;//set EOF flag
      }
      gfs_setpos(x,0);
    }
  }//type==3

}

void sub_open_gwbasic(qbs *typestr,int32 i,qbs *name,int64 record_length,int32 passed){
  if (new_error) return;
  static int32 a,type;
  if (!typestr->len){error(54); return;}//bad file mode
  a=typestr->chr[0]&223;
  type=0;
  if (a==82) type=1;//R
  if (a==66) type=2;//B
  if (a==73) type=3;//I
  if (a==79) type=4;//O
  if (a==65) type=5;//A
  if (!type){error(54); return;}//bad file mode
  if (passed){
    sub_open(name,type,NULL,NULL,i,record_length,1);
  }else{
    sub_open(name,type,NULL,NULL,i,NULL,0);
  }
}

void sub_close(int32 i2,int32 passed){
  if (new_error) return;
  int32 i,x;//<--RECURSIVE function - do not make this static

  if (passed){

    if (i2<0){//special handle
      //determine which close procedure to call
      x=-(i2+1);
      static special_handle_struct *sh; sh=(special_handle_struct*)list_get(special_handles,x); if (!sh) return;
      if (sh->type==1){//stream
    static stream_struct *st; st=(stream_struct*)sh->index;
    if (st->type==1){//connection
      connection_close(x);
    }//connection
      }//stream

      if (sh->type==2){//host listener
    connection_close(x);
      }//host listener

      return;
    }//special handle

        
    if (gfs_fileno_valid(i2)==1) gfs_close(gfs_fileno[i2]);
    return;

  }//passed

  //special handles



  for (i=1;i<=special_handles->indexes;i++){
    sub_close(-i-1,1);
  }


  for (i=1;i<=gfs_fileno_n;i++){
    if (gfs_fileno_valid(i)==1) gfs_close(gfs_fileno[i]);
  }

}//close

int32 file_input_chr(int32 i){
  //returns the ASCII value of the character (0-255)
  //returns -1 if eof reached (error to be externally handled)
  //returns -2 for other errors (internally handled), the calling function should abort

  static uint8 c;
  static int32 e;
  if (e=gfs_read(i,-1,&c,1)){
    if (e==-10) return -1;
    if (e==-2){error(258); return -2;}//invalid handle
    if (e==-3){error(54); return -2;}//bad file mode
    if (e==-4){error(5); return -2;}//illegal function call
    if (e==-7){error(70); return -2;}//permission denied
    error(75); return -2;//assume[-9]: path/file access error
  }
  if (c==26){//eof character (go back 1 byte so subsequent reads will re-encounter the eof character)
    gfs_setpos(i,gfs_getpos(i)-1);
    gfs_file[i].eof_passed=1;//also set EOF flag
    return -1;
  }
  return c;

}

void file_input_skip1310(int32 i,int32 c){
  //assumes a character of value 13 or 10 has just been read (passed)
  //peeks next character and skips it too if it is a corresponding 13 or 10 pair
  static int32 nextc;
  nextc=file_input_chr(i);
  if (nextc==-2) return;
  if (nextc==-1) return;
  if (((c==10)&&(nextc!=13))||((c==13)&&(nextc!=10))){
    gfs_setpos(i,gfs_getpos(i)-1);//go back 1 character
  }else{
    //check next character for EOF CHR$(26)
    nextc=file_input_chr(i);
    if (nextc==-2) return;
    if (nextc==-1) return;
    gfs_setpos(i,gfs_getpos(i)-1);//go back 1 character
  }
}

void file_input_nextitem(int32 i,int32 lastc){
  if (i<0) return;
  //this may require reversing a bit too!
  int32 c,nextc;
  c=lastc;
 nextchr:
  if (c==-1) return;
  if (c==32){
    nextc=file_input_chr(i); if (nextc==-2) return;
    if (nextc==-1) return;
    if ( (nextc!=32)&&(nextc!=44)&&(nextc!=10)&&(nextc!=13) ){
      gfs_setpos(i,gfs_getpos(i)-1);
      return;
    }else{
      c=nextc;
      goto nextchr;
    }
  }
  if (c==44) return;//,
  if ((c==10)||(c==13)){//lf cr
    file_input_skip1310(i,c);
    return;
  }
  c=file_input_chr(i); if (c==-2) return;
  goto nextchr;
}

uint8 sub_file_print_spaces[32];
void sub_file_print(int32 i,qbs *str,int32 extraspace,int32 tab,int32 newline){
  if (new_error) return;
  static int32 x,x2,x3,x4;
  static int32 e;

  //tcp/ip?
  //note: spacing considerations such as 'extraspace' & 'tab' are ignored
  if (i<0){

    return;
  }
  if (gfs_fileno_valid(i)!=1){error(52); return;}//Bad file name or number
  i=gfs_fileno[i];//convert fileno to gfs index
  static gfs_file_struct *gfs;
  gfs=&gfs_file[i];

  if (gfs->scrn==1) {
        qbs_print (str, newline);
        return;
  };

  if (gfs->type!=4){error(54); return;}//Bad file mode
  if (!gfs->write){error(75); return;}//Path/file access error

  e=gfs_write(i,-1,str->chr,str->len);
  if (e){
    if (e==-2){error(258); return;}//invalid handle
    if (e==-3){error(54); return;}//bad file mode
    if (e==-4){error(5); return;}//illegal function call
    if (e==-7){error(70); return;}//permission denied
    error(75); return;//assume[-9]: path/file access error
  }

  //move column if carriage return found in content
  static int32 stri,strl;
  static uint8 c;
  strl=str->len;
  for (stri=0;stri<strl;stri++){
    c=str->chr[stri];
    if ((c!=13)&&(c!=10)){
      gfs->column++;
    }else{
      if (c==13) gfs->column=1;
    }
  }

  //add extra spaces as needed
  static int32 nspaces;
  static int16 cr_lf=13+10*256; 
  nspaces=0;
  if (extraspace){
    nspaces++;
    gfs->column++;
  }
  if (tab){
    //a space MUST be added
    nspaces++;
    gfs->column++;
    x=(gfs->column-1)%14;
    if (x!=0){
      x=14-x;
      nspaces+=x;
      gfs->column+=x;
    }
  }
  if (nspaces){

    e=gfs_write(i,-1,sub_file_print_spaces,nspaces);
    if (e){
      if (e==-2){error(258); return;}//invalid handle
      if (e==-3){error(54); return;}//bad file mode
      if (e==-4){error(5); return;}//illegal function call
      if (e==-7){error(70); return;}//permission denied
      error(75); return;//assume[-9]: path/file access error
    }

  }
  if (newline){

    e=gfs_write(i,-1,(uint8*)&cr_lf,2);
    if (e){
      if (e==-2){error(258); return;}//invalid handle
      if (e==-3){error(54); return;}//bad file mode
      if (e==-4){error(5); return;}//illegal function call
      if (e==-7){error(70); return;}//permission denied
      error(75); return;//assume[-9]: path/file access error
    }

    gfs->column=1;
  }
}

//number limits
const char *range_int64_max[]={"9223372036854775807"};//19 digits
const char *range_int64_neg_max[]={"9223372036854775808"};//19 digits
const char *range_uint64_max[]={"18446744073709551615"};//20 digits
const char *range_float_max[]=    {"17976931348623157"};//17 digits
                          
//universal number representation
uint16 n_digits;
uint8 n_digit[256];
int64 n_exp;//if 0, there is one digit in front of the decimal place
uint8 n_neg;//if 1, the number is negative
uint8 n_hex;//if 1, the digits are in hexidecimal and n_exp should be ignored
//if 2, the digits are in octal and n_exp should be ignored
//(consider revising variable name n_hex)

int32 n_roundincrement(){
  static int32 i,i2,i3;
  if (n_digits==0) return 0;
  if (n_digits>(n_exp+1)){//numbers exist after the decimal point
    i=n_digit[n_exp+1]-48;
    if (i>=5) return 1;
  }
  return 0;
}

long double n_float_value;
int32 n_float(){
  //return value: Bit 0=successful
  //data
  static uint8 built[256];
  static int64 value;
  uint64 uvalue;
  static int32 i,i2,i3;
  static uint8 *max;
  max=(uint8*)range_float_max[0];
  n_float_value=0; value=0; uvalue=0;
  if (n_digits==0) return 1;
  //hex?
  if (n_hex==1){
    if (n_digits>16) return 0;
    for (i=0;i<n_digits;i++){
      i2=n_digit[i];
      if ((i2>=48)&&(i2<=57)) i2-=48;
      if ((i2>=65)&&(i2<=70)) i2-=55;
      if ((i2>=97)&&(i2<=102)) i2-=87;
      value<<=4;
      value|=i2;
    }
    n_float_value=value;
    return 1;
  }
  //oct?
  if (n_hex==2){
    if (n_digits>=22){
      if ((n_digits>22)||(n_digit[0]>49)) return 0;
    }
    for (i=0;i<n_digits;i++){
      i2=n_digit[i]-48;
      value<<=3;
      value|=i2;
    }
    n_float_value=value;
    return 1;
  }

  //max range check (+-1.7976931348623157E308)
  if (n_exp>308)return 0;//overflow
  if (n_exp==308){
    i2=n_digits; if (i2>17) i2=17;
    for (i=0;i<i2;i++){
      if (n_digit[i]>max[i]) return 0;//overflow
      if (n_digit[i]<max[i]) break;
    }
  }
  //too close to 0?
  if (n_exp<-324) return 1;
  //read & return value (via C++ function)
  //build number
  i=0;
  if (n_neg){built[i]=45; i++;}//-
  built[i]=n_digit[0]; i++;
  built[i]=46; i++;//.
  if (n_digits==1){
    built[i]=48; i++;//0
  }else{
    i3=n_digits; if (i3>17) i3=17;
    for (i2=1;i2<i3;i2++){
      built[i]=n_digit[i2]; i++;
    }
  }
  built[i]=69; i++;//E
#ifdef QB64_WINDOWS
  i2=sprintf((char*)&built[i],"%I64i",n_exp);
#else
  i2=sprintf((char*)&built[i],"%lli",n_exp);
#endif
  i=i+i2;
  built[i]=0;//NULL terminate for sscanf

  static double sscanf_fix;
  sscanf((char*)&built[0],"%lf",&sscanf_fix);
  n_float_value=sscanf_fix;

  return 1;
}

int64 n_int64_value;
int32 n_int64(){
  //return value: Bit 0=successful
  //data
  static int64 value;
  uint64 uvalue;
  static int32 i,i2;
  static uint8 *max; static uint8 *neg_max;
  static int64 v0=build_int64(0x80000000,0x00000000);
  static int64 v1=build_int64(0x7FFFFFFF,0xFFFFFFFF);
  max=(uint8*)range_int64_max[0]; neg_max=(uint8*)range_int64_neg_max[0];
  n_int64_value=0; value=0; uvalue=0;
  if (n_digits==0) return 1;
  //hex
  if (n_hex==1){
    if (n_digits>16) return 0;
    for (i=0;i<n_digits;i++){
      i2=n_digit[i];
      if ((i2>=48)&&(i2<=57)) i2-=48;
      if ((i2>=65)&&(i2<=70)) i2-=55;
      if ((i2>=97)&&(i2<=102)) i2-=87;
      value<<=4;
      value|=i2;
    }
    n_int64_value=value;
    return 1;
  }
  //oct

  if (n_hex==2){
    if (n_digits>=22){

      if ((n_digits>22)||(n_digit[0]>49)) return 0;
    }
    for (i=0;i<n_digits;i++){
      i2=n_digit[i]-48;
      value<<=3;
      value|=i2;
    }
    n_int64_value=value;
    return 1;
  }

  //range check: int64 (-9,223,372,036,854,775,808 to 9,223,372,036,854,775,807)
  if (n_exp>18)return 0;//overflow
  if (n_exp==18){
    i2=n_digits; if (i2>19) i2=19;//only scan integeral digits
    for (i=0;i<i2;i++){
      if (n_neg){
    if (n_digit[i]>neg_max[i]) return 0;//overflow
    if (n_digit[i]<neg_max[i]) break;
      }else{
    if (n_digit[i]>max[i]) return 0;//overflow
    if (n_digit[i]<max[i]) break;
      }
    }
  }
  //calculate integeral value
  i2=n_digits; if (i2>(n_exp+1)) i2=n_exp+1;
  for (i=0;i<(n_exp+1);i++){
    uvalue*=10;
    if (i<i2) uvalue=uvalue+(n_digit[i]-48);
  }
  if (n_neg){
    value=-uvalue;
  }else{
    value=uvalue;
  }
  //apply rounding
  if (n_roundincrement()){
    if (n_neg){
      if (value==v0) return 0;
      value--;
    }else{
      if (value==v1) return 0;
      value++;
    }
  }
  //return value
  n_int64_value=value;
  return 1;
}

uint64 n_uint64_value;
int32 n_uint64(){
  //return value: Bit 0=successful
  //data
  static int64 value;
  uint64 uvalue;
  static int32 i,i2;
  static uint8 *max;
  static int64 v0=build_uint64(0xFFFFFFFF,0xFFFFFFFF);
  max=(uint8*)range_uint64_max[0];
  n_uint64_value=0; value=0; uvalue=0;
  if (n_digits==0) return 1;
  //hex
  if (n_hex==1){
    if (n_digits>16) return 0;
    for (i=0;i<n_digits;i++){
      i2=n_digit[i];
      if ((i2>=48)&&(i2<=57)) i2-=48;
      if ((i2>=65)&&(i2<=70)) i2-=55;
      if ((i2>=97)&&(i2<=102)) i2-=87;
      uvalue<<=4;
      uvalue|=i2;
    }
    n_uint64_value=uvalue;
    return 1;
  }
  //oct
  if (n_hex==2){
    if (n_digits>=22){
      if ((n_digits>22)||(n_digit[0]>49)) return 0;
    }
    for (i=0;i<n_digits;i++){
      i2=n_digit[i]-48;
      uvalue<<=3;
      uvalue|=i2;
    }
    n_uint64_value=uvalue;
    return 1;
  }

  //negative?
  if (n_neg){
    if (n_exp>=0) return 0;//cannot return a negative number!
  }
  //range check: int64 (0 to 18446744073709551615)
  if (n_exp>19)return 0;//overflow
  if (n_exp==19){
    i2=n_digits; if (i2>20) i2=20;//only scan integeral digits
    for (i=0;i<i2;i++){ 
      if (n_digit[i]>max[i]) return 0;//overflow
      if (n_digit[i]<max[i]) break; 
    }
  }
  //calculate integeral value
  i2=n_digits; if (i2>(n_exp+1)) i2=n_exp+1;
  for (i=0;i<(n_exp+1);i++){
    uvalue*=10;
    if (i<i2) uvalue=uvalue+(n_digit[i]-48);
  }
  //apply rounding
  if (n_roundincrement()){
    if (n_neg){
      return 0;
    }else{
      if (uvalue==v0) return 0;
      uvalue++;
    }
  }
  //return value
  n_uint64_value=uvalue;



  return 1;
}

int32 n_inputnumberfromdata(uint8 *data,ptrszint *data_offset,ptrszint data_size){
  //return values:
  //0=success!
  //1=Overflow
  //2=Out of DATA
  //3=Syntax error
  //note: when read fails the data_offset MUST be restored to its old position

  //data
  static int32 i,i2;
  static int32 step,c;
  static int32 exponent_digits;
  static uint8 negate_exponent;
  static int64 exponent_value;
  static int32 return_value;

  return_value=1;//overflow (default)
  step=0;
  negate_exponent=0;
  exponent_value=0;
  exponent_digits=0;

  //prepare universal number representation
  n_digits=0; n_exp=0; n_neg=0; n_hex=0;

  //Out of DATA?
  if (*data_offset>=data_size) return 2;

  //read character
  c=data[*data_offset];

  //hex/oct
  if (c==38){//&
    (*data_offset)++; if (*data_offset>=data_size) goto gotnumber;
    c=data[*data_offset];
    if (c==44){(*data_offset)++; goto gotnumber;}
    if ((c==72)||(c==104)){//"H"or"h"
    nexthexchr:
      (*data_offset)++; if (*data_offset>=data_size) goto gotnumber;
      c=data[*data_offset];
      if (c==44){(*data_offset)++; goto gotnumber;}
      if ( ((c>=48)&&(c<=57)) || ((c>=65)&&(c<=70)) || ((c>=97)&&(c<=102)) ){//0-9 or A-F or a-f
    if (n_digits==256) return 1;//Overflow
    n_digit[n_digits]=c;
    n_digits++;
    n_hex=1;
    goto nexthexchr;
      }
      return 3;//Syntax error
    }
    if ((c==79)||(c==111)){//"O"or"o"
    nexthexchr2:
      (*data_offset)++; if (*data_offset>=data_size) goto gotnumber;
      c=data[*data_offset];
      if (c==44){(*data_offset)++; goto gotnumber;}
      if ((c>=48)&&(c<=55)){//0-7
    if (n_digits==256) return 1;//Overflow
    n_digit[n_digits]=c;
    n_digits++;
    n_hex=2;
    goto nexthexchr2;
      }
      return 3;//Syntax error
    }
    return 3;//Syntax error
  }//&

 readnextchr:
  if (c==44){(*data_offset)++; goto gotnumber;}

  if (c==45){//-
    if (step==0){n_neg=1; step=1; goto nextchr;}
    if (step==3){negate_exponent=1; step=4; goto nextchr;}
    return 3;//Syntax error
  }

  if (c==43){//+
    if (step==0){step=1; goto nextchr;}
    if (step==3){step=4; goto nextchr;}
    return 3;//Syntax error
  }

  if ((c>=48)&&(c<=57)){//0-9

    if (step<=1){//before decimal point
      step=1;
      if (n_digits||(c>48)){
    if (n_digits) n_exp++;
    if (n_digits==256) return 1;//Overflow
    n_digit[n_digits]=c;
    n_digits++;
      }
    }

    if (step==2){//after decimal point
      if ((n_digits==0)&&(c==48)) n_exp--;
      if ((n_digits)||(c>48)){
    if (n_digits==256) return 1;//Overflow
    n_digit[n_digits]=c;
    n_digits++;
      }
    }

    if (step>=3){//exponent
      step=4;
      if ((exponent_digits)||(c>48)){
    if (exponent_digits==18) return 1;//Overflow
    exponent_value*=10;
    exponent_value=exponent_value+(c-48);
    exponent_digits++;
      }
    }

    goto nextchr;
  }

  if (c==46){//.
    if (step>1) return 3;//Syntax error
    if (n_digits==0) n_exp=-1;
    step=2; goto nextchr;
  }

  if ((c==68)||(c==69)||(c==100)||(c==101)){//D,E,d,e
    if (step>2) return 3;//Syntax error
    step=3; goto nextchr;
  }

  return 3;//Syntax error
 nextchr:
  (*data_offset)++; if (*data_offset>=data_size) goto gotnumber;
  c=data[*data_offset];
  goto readnextchr;

 gotnumber:;
  if (negate_exponent) n_exp-=exponent_value; else n_exp+=exponent_value;//complete exponent
  if (n_digits==0) {n_exp=0; n_neg=0;}//clarify number
  return 0;//success
}





















int32 n_inputnumberfromfile(int32 fileno){
  //return values:
  //0=success
  //1=overflow
  //2=eof
  //3=failed (no further errors)

  //data
  static int32 i,i2;
  static int32 step,c;
  static int32 exponent_digits;
  static uint8 negate_exponent;
  static int64 exponent_value;
  static int32 return_value;

  //tcp/ip specific data
  static qbs *str,*character;
  int32 nextc,x,x2,x3,x4;
  int32 i1;
  int32 inspeechmarks;
  static uint8 *ucbuf;
  static uint32 ucbufsiz;
  static int32 info;

  if (fileno>=0){
    if (gfs_fileno_valid(fileno)!=1){error(52); return 3;}//Bad file name or number
    fileno=gfs_fileno[fileno];//convert fileno to gfs index
    static gfs_file_struct *gfs;
    gfs=&gfs_file[fileno];
    if (gfs->type!=3){error(54); return 3;}//Bad file mode
    if (!gfs->read){error(75); return 3;}//Path/file access error
  }

  return_value=1;//overflow (default)
  step=0;
  negate_exponent=0;
  exponent_value=0;
  exponent_digits=0;

  //prepare universal number representation
  n_digits=0; n_exp=0; n_neg=0; n_hex=0;

  //skip any leading spaces
  do{
    c=file_input_chr(fileno); if (c==-2) return 3;
    if (c==-1){return_value=2; goto error;}//input past end of file
  }while(c==32);

  //hex/oct
  if (c==38){//&
    c=file_input_chr(fileno); if (c==-2) return 3;
    if (c==-1) goto gotnumber;
    if ((c==72)||(c==104)){//"H"or"h"
    nexthexchr:
      c=file_input_chr(fileno); if (c==-2) return 3;
      if ( ((c>=48)&&(c<=57)) || ((c>=65)&&(c<=70)) || ((c>=97)&&(c<=102)) ){//0-9 or A-F or a-f
    if (n_digits==256) goto error;//overflow
    n_digit[n_digits]=c;
    n_digits++;
    n_hex=1;
    goto nexthexchr;
      }
      goto gotnumber;
    }
    if ((c==79)||(c==111)){//"O"or"o"
    nexthexchr2:
      c=file_input_chr(fileno); if (c==-2) return 3;
      if ((c>=48)&&(c<=55)){//0-7
    if (n_digits==256) goto error;//overflow
    n_digit[n_digits]=c;
    n_digits++;
    n_hex=2;
    goto nexthexchr2;
      }
      goto gotnumber;
    }
    goto gotnumber;
  }//&

 readnextchr:
  if (c==-1) goto gotnumber;

  if (c==45){//-
    if (step==0){n_neg=1; step=1; goto nextchr;}
    if (step==3){negate_exponent=1; step=4; goto nextchr;}
    goto gotnumber;
  }

  if (c==43){//+
    if (step==0){step=1; goto nextchr;}
    if (step==3){step=4; goto nextchr;}
    goto gotnumber;
  }

  if ((c>=48)&&(c<=57)){//0-9

    if (step<=1){//before decimal point
      step=1;
      if (n_digits||(c>48)){
    if (n_digits) n_exp++;
    if (n_digits==256) goto error;//overflow
    n_digit[n_digits]=c;
    n_digits++;
      }
    }

    if (step==2){//after decimal point
      if ((n_digits==0)&&(c==48)) n_exp--;
      if ((n_digits)||(c>48)){
    if (n_digits==256) goto error;//overflow
    n_digit[n_digits]=c;
    n_digits++;
      }
    }

    if (step>=3){//exponent
      step=4;
      if ((exponent_digits)||(c>48)){
    if (exponent_digits==18) goto error;//overflow
    exponent_value*=10;
    exponent_value=exponent_value+(c-48);
    exponent_digits++;
      }
    }

    goto nextchr;
  }

  if (c==46){//.
    if (step>1) goto gotnumber;
    if (n_digits==0) n_exp=-1;
    step=2; goto nextchr;
  }

  if ((c==68)||(c==69)||(c==100)||(c==101)){//D,E,d,e
    if (step>2) goto gotnumber;
    step=3; goto nextchr;
  }

  goto gotnumber;//invalid character
 nextchr:
  c=file_input_chr(fileno); if (c==-2) return 3;
  goto readnextchr;

 gotnumber:;
  if (negate_exponent) n_exp-=exponent_value; else n_exp+=exponent_value;//complete exponent
  if (n_digits==0) {n_exp=0; n_neg=0;}//clarify number
  file_input_nextitem(fileno,c);
  return 0;//success

 error:
  file_input_nextitem(fileno,c);
  if (fileno<0){

  }
  return return_value;
}


void revert_input_check(){
}

void sub_file_input_string(int32 fileno,qbs *deststr){
  if (new_error) return;
  static qbs *str,*character;
  int32 c,nextc,x,x2,x3,x4;
  int32 i,i1;
  int32 inspeechmarks;
  static uint8 *ucbuf;
  static uint32 ucbufsiz;
  static int32 info;

  //tcp/ip?
  //note: spacing considerations are ignored
  if (fileno<0){
    return;
  }

  if (gfs_fileno_valid(fileno)!=1){error(52); return;}//Bad file name or number
  fileno=gfs_fileno[fileno];//convert fileno to gfs index

  static gfs_file_struct *gfs;
  gfs=&gfs_file[fileno];
  if (gfs->type!=3){error(54); return;}//Bad file mode
  if (!gfs->read){error(75); return;}//Path/file access error

  str=qbs_new(0,0);
  //skip whitespace (spaces & tabs)
  do{
    c=file_input_chr(fileno); if (c==-2) return;
    if (c==-1){
      qbs_set(deststr,str);
      qbs_free(str);
      error(62);//input past end of file
      return;
    }
  }while((c==32)||(c==9));
  //quoted string?
  inspeechmarks=0;
  if (c==34){//"
    inspeechmarks=1;
    c=file_input_chr(fileno);
  }
  //read string
  character=qbs_new(1,0);
 nextchr:
  if (c==-2) return;
  if (c==-1) goto gotstr;
  if (inspeechmarks){
    if (c==34) goto gotstr;//"
  }else{
    if (c==44) goto gotstr;//,
    if (c==10) goto gotstr;
    if (c==13) goto gotstr;
  }
  character->chr[0]=c; qbs_set(str,qbs_add(str,character));
  c=file_input_chr(fileno);
  goto nextchr;
 gotstr:
  //cull trailing whitespace
  if (!inspeechmarks){
  cullstr:
    if (str->len){
      if ((str->chr[str->len-1]==32)||(str->chr[str->len-1]==9)){str->len--; goto cullstr;}
    }
  }
 nextstr:
  //scan until next item (or eof) reached
  if (c==-2) return;
  if (c==-1) goto returnstr;
  if (c==44) goto returnstr;
  if ((c==10)||(c==13)){//lf cr
    file_input_skip1310(fileno,c);
    goto returnstr;
  }
  c=file_input_chr(fileno);
  goto nextstr;
  //return string
 returnstr:
  qbs_set(deststr,str);
  qbs_free(str);
  qbs_free(character);
  return;
}

int64 func_file_input_int64(int32 fileno){
  if (new_error) return 0;
  static int32 i;
  i=n_inputnumberfromfile(fileno);
  if (i==1){error(6); return 0;}//overflow
  if (i==2){error(62); return 0;}//input past end of file
  if (i==3) return 0;//failed
  if (n_int64()) return n_int64_value;
  error(6);//overflow
  return 0;
}

uint64 func_file_input_uint64(int32 fileno){
  if (new_error) return 0;
  static int32 i;
  i=n_inputnumberfromfile(fileno);
  if (i==1){error(6); return 0;}//overflow
  if (i==2){error(62); return 0;}//input past end of file
  if (i==3) return 0;//failed
  if (n_uint64()) return n_uint64_value;
  error(6);//overflow
  return 0;
}




void sub_read_string(uint8 *data,ptrszint *data_offset,ptrszint data_size,qbs *deststr){
  if (new_error) return;
  static qbs *str,*character;
  static int32 c,inspeechmarks;
  str=qbs_new(0,0);
  character=qbs_new(1,0);
  inspeechmarks=0;

  if (*data_offset>=data_size){error(4); goto gotstr;}//Out of DATA

  c=data[*data_offset];
 nextchr:

  if (c==44){//,
    if (inspeechmarks!=1){
      (*data_offset)++;
      goto gotstr;
    }
  }
  if (inspeechmarks==2){error(4); str->len=0; goto gotstr;}//syntax error (expected , after closing " unless at end of data in which " is assumed by QB)

  if (c==34){//"
    if (inspeechmarks) {inspeechmarks=2; goto skipchr;}
    if (!str->len){inspeechmarks=1; goto skipchr;}
  }

  character->chr[0]=c; qbs_set(str,qbs_add(str,character));
 skipchr:

  (*data_offset)++; if (*data_offset>=data_size) goto gotstr;
  c=data[*data_offset];
  goto nextchr;

 gotstr:
  qbs_set(deststr,str);
  qbs_free(str);
  qbs_free(character);
  return;
}

long double func_read_float(uint8 *data,ptrszint *data_offset,ptrszint data_size,int32 typ){
  if (new_error) return 0;
  static int32 i;
  static int64 maxval,minval;
  static int64 value;
  static ptrszint old_data_offset;
  old_data_offset=*data_offset;
  i=n_inputnumberfromdata(data,data_offset,data_size);


  //return values:
  //0=success!
  //1=Overflow
  //2=Out of DATA
  //3=Syntax error
  //note: when read fails the data_offset MUST be restored to its old position
  if (i==1){//Overflow
    goto overflow;
  }
  if (i==2){//Out of DATA
    error(4);
    return 0;
  }
  if (i==3){//Syntax error
    *data_offset=old_data_offset;
    error(2); 
    return 0;
  }



  if (!n_float()) goto overflow; //returns n_float_value

  //range test & return value
  if (typ&ISFLOAT){
    if ((typ&511)>=64) return n_float_value;
    if (n_float_value>3.402823466E+38) goto overflow;
    return n_float_value;
  }else{
    if (n_float_value<(-(9.2233720368547758E+18)))goto overflow;//too low to store!
    if (n_float_value>   9.2233720368547758E+18)  goto overflow;//too high to store!
    value=qbr(n_float_value);
    if ((typ&ISUNSIGNED)||n_hex){
      maxval=(((int64)1)<<(typ&511))-1;
      minval=0;
    }else{
      maxval=(((int64)1)<<((typ&511)-1))-1;
      minval=-(((int64)1)<<((typ&511)-1));
    }
    if ((value>maxval)||(value<minval)) goto overflow;

    if (((typ&ISUNSIGNED)==0)&&n_hex){//signed hex/oct/...  
      if (  ( ((int64)1) << ((typ&511)-1) )  &value){//if top bit is set, set all bits above it to form a negative value
    value=(maxval^((int64)-1))+value;
      }
    }

    return value;
  }

 overflow:
  *data_offset=old_data_offset;
  error(6); 
  return 0;
}

int64 func_read_int64(uint8 *data,ptrszint *data_offset,ptrszint data_size){
  if (new_error) return 0;
  static int32 i;
  static ptrszint old_data_offset;
  old_data_offset=*data_offset;
  i=n_inputnumberfromdata(data,data_offset,data_size);
  //return values:
  //0=success!
  //1=Overflow
  //2=Out of DATA
  //3=Syntax error
  //note: when read fails the data_offset MUST be restored to its old position
  if (i==1){//Overflow
    goto overflow;
  }
  if (i==2){//Out of DATA
    error(4);
    return 0;
  }
  if (i==3){//Syntax error
    *data_offset=old_data_offset;
    error(2); 
    return 0;
  }
  if (n_int64()) return n_int64_value;
 overflow:
  *data_offset=old_data_offset;
  error(6); 
  return 0;
}

uint64 func_read_uint64(uint8 *data,ptrszint *data_offset,ptrszint data_size){
  if (new_error) return 0;
  static int32 i;
  static ptrszint old_data_offset;
  old_data_offset=*data_offset;
  i=n_inputnumberfromdata(data,data_offset,data_size);
  //return values:
  //0=success!
  //1=Overflow
  //2=Out of DATA
  //3=Syntax error
  //note: when read fails the data_offset MUST be restored to its old position
  if (i==1){//Overflow
    goto overflow;
  }
  if (i==2){//Out of DATA
    error(4);
    return 0;
  }
  if (i==3){//Syntax error
    *data_offset=old_data_offset;
    error(2); 
    return 0;
  }
  if (n_uint64()) return n_uint64_value;
 overflow:
  *data_offset=old_data_offset;
  error(6); 
  return 0;
}

long double func_file_input_float(int32 fileno,int32 typ){
  if (new_error) return 0;
  static int32 i;
  static int64 maxval,minval;
  static int64 value;
  i=n_inputnumberfromfile(fileno);
  if (i==1){error(6); return 0;}//overflow
  if (i==2){error(62); return 0;}//input past end of file
  if (i==3) return 0;//failed
  if (!n_float()){error(6); return 0;} //returns n_float_value
  //range test & return value
  if (typ&ISFLOAT){
    if ((typ&511)>=64) return n_float_value;
    if (n_float_value>3.402823466E+38){error(6); return 0;}
    return n_float_value;
  }else{
    if (n_float_value<(-(9.2233720368547758E+18))){error(6); return 0;}//too low to store!
    if (n_float_value>   9.2233720368547758E+18)  {error(6); return 0;}//too high to store!
    value=qbr(n_float_value);
    if (typ&ISUNSIGNED){
      maxval=(((int64)1)<<(typ&511))-1;
      minval=0;
    }else{
      maxval=(((int64)1)<<((typ&511)-1))-1;
      minval=-(((int64)1)<<((typ&511)-1));
    }
    if ((value>maxval)||(value<minval)){error(6); return 0;}

    if (((typ&ISUNSIGNED)==0)&&n_hex){//signed hex/oct/...  
      if (  ( ((int64)1) << ((typ&511)-1) )  &value){//if top bit is set, set all bits above it to form a negative value
    value=(maxval^((int64)-1))+value;
      }
    }

    return value;
  }
}//func_file_input_float

void *byte_element(uint64 offset,int32 length){
  if (length<0) length=0;//some calculations may result in negative values which mean 0 (no bytes available)
  //add structure to xms stack (byte_element structures are never stored in cmem!)
  void *p;
  if ((mem_static_pointer+=12)<mem_static_limit) p=mem_static_pointer-12; else p=mem_static_malloc(12);
  *((uint64*)p)=offset;
  ((uint32*)p)[2]=length;
  return p;
}

void *byte_element(uint64 offset,int32 length,byte_element_struct *info){
  if (length<0) length=0;//some calculations may result in negative values which mean 0 (no bytes available)
  info->length=length;
  info->offset=offset;
  return info;
}

void call_interrupt(int32 intno, void *inregs,void *outregs){
  if (new_error) return;
  static byte_element_struct *ele;
  static uint16 *sp;
  /* testing only
     static qbs* s=NULL;
     if (s==NULL) s=qbs_new(0,0);
     qbs_set(s,qbs_str(ele->length));
     MessageBox2(NULL,(char*)s->chr,"CALL INTERRUPT: size",MB_OK|MB_SYSTEMMODAL);
     qbs_set(s,qbs_str( ((uint8*)(ele->offset))[0] ));
     MessageBox2(NULL,(char*)s->chr,"CALL INTERRUPT: value",MB_OK|MB_SYSTEMMODAL);
  */
  /* reference
     TYPE RegType
     AX AS INTEGER
     BX AS INTEGER
     CX AS INTEGER
     DX AS INTEGER
     BP AS INTEGER
     SI AS INTEGER
     DI AS INTEGER
     FLAGS AS INTEGER
     END TYPE
  */
  //error checking
  ele=(byte_element_struct*)outregs;
  if (ele->length<16){error(5); return;}//Illegal function call
  ele=(byte_element_struct*)inregs;
  if (ele->length<16){error(5); return;}//Illegal function call
  //load virtual registers
  sp=(uint16*)(ele->offset);
  cpu.ax=sp[0];
  cpu.bx=sp[1];
  cpu.cx=sp[2];
  cpu.dx=sp[3];
  cpu.bp=sp[4];
  cpu.si=sp[5];
  cpu.di=sp[6];
  //note: flags ignored (revise)
  call_int(intno);
  //save virtual registers
  ele=(byte_element_struct*)outregs;
  sp=(uint16*)(ele->offset);
  sp[0]=cpu.ax;
  sp[1]=cpu.bx;
  sp[2]=cpu.cx;
  sp[3]=cpu.dx;
  sp[4]=cpu.bp;
  sp[5]=cpu.si;
  sp[6]=cpu.di;
  //note: flags ignored (revise)
  return;
}

void call_interruptx(int32 intno, void *inregs,void *outregs){
  if (new_error) return;
  static byte_element_struct *ele;
  static uint16 *sp;
  /* reference
     TYPE RegTypeX
     AX AS INTEGER
     BX AS INTEGER
     CX AS INTEGER
     DX AS INTEGER
     BP AS INTEGER
     SI AS INTEGER
     DI AS INTEGER
     FLAGS AS INTEGER
     DS AS INTEGER
     ES AS INTEGER
     END TYPE
  */
  //error checking
  ele=(byte_element_struct*)outregs;
  if (ele->length<20){error(5); return;}//Illegal function call
  ele=(byte_element_struct*)inregs;
  if (ele->length<20){error(5); return;}//Illegal function call
  //load virtual registers
  sp=(uint16*)(ele->offset);
  cpu.ax=sp[0];
  cpu.bx=sp[1];
  cpu.cx=sp[2];
  cpu.dx=sp[3];
  cpu.bp=sp[4];
  cpu.si=sp[5];
  cpu.di=sp[6];
  //note: flags ignored (revise)
  cpu.ds=sp[8];
  cpu.es=sp[9];
  call_int(intno);
  //save virtual registers
  ele=(byte_element_struct*)outregs;
  sp=(uint16*)(ele->offset);
  sp[0]=cpu.ax;
  sp[1]=cpu.bx;
  sp[2]=cpu.cx;
  sp[3]=cpu.dx;
  sp[4]=cpu.bp;
  sp[5]=cpu.si;
  sp[6]=cpu.di;
  //note: flags ignored (revise)
  sp[8]=cpu.ds;
  sp[9]=cpu.es;
  return;
}

void sub_get(int32 i,int64 offset,void *element,int32 passed){
  if (new_error) return;
  static byte_element_struct *ele;
  static int32 x,x2;

  if (i<0){//special handle?
    x=-(i+1);
    static special_handle_struct *sh; sh=(special_handle_struct*)list_get(special_handles,x); if (!sh){error(52); return;}
    if (sh->type==1){//stream
      static stream_struct *st; st=(stream_struct*)sh->index;
      stream_update(st);
      ele=(byte_element_struct*)element;
      if (st->in_size<ele->length){st->eof=1; return;}
      st->eof=0;
      memcpy((void*)(ele->offset),st->in,ele->length);
      x2=st->in_size-ele->length; if (x2) memmove(st->in,st->in+ele->length,x2);
      st->in_size-=ele->length;
      return;
    }//stream
    error(52); return;
  }//special handle

  if (gfs_fileno_valid(i)!=1){error(52); return;}//Bad file name or number
  i=gfs_fileno[i];//convert fileno to gfs index
  static gfs_file_struct *gfs;
  gfs=&gfs_file[i];
  if (gfs->type>2){error(54); return;}//Bad file mode
  if (!gfs->read){error(75); return;}//Path/file access error

  ele=(byte_element_struct*)element;

  if (gfs->type==1){//RANDOM
    if (ele->length>gfs->record_length){error(59); return;}//Bad record length
    if (passed){
      offset--;
      if (offset<0){error(63); return;}//Bad record number
      offset*=gfs->record_length;
    }else{
      offset=-1;
    }
  }else{//BINARY
    if (passed){
      offset--;
      if (offset<0){error(63); return;}//Bad record number
    }else{offset=-1;}
  }

  static int32 e;

  e=gfs_read(i,offset,(uint8*)ele->offset,ele->length);
  if (e){
    if (e!=-10){//note: on eof, unread buffer area becomes NULL
      if (e==-2){error(258); return;}//invalid handle
      if (e==-3){error(54); return;}//bad file mode
      if (e==-4){error(5); return;}//illegal function call
      if (e==-7){error(70); return;}//permission denied
      error(75); return;//assume[-9]: path/file access error
    }
  }

  //seek to beginning of next field
  if (gfs->type==1){
    if (e!=-10){//note: seek index not advanced if record did not exist
      if (ele->length<gfs->record_length){
    if (offset!=-1){
      e=gfs_setpos(i,offset+gfs->record_length);
    }else{
      e=gfs_setpos(i,gfs_getpos(i)-ele->length+gfs->record_length);
    }
    if (e){error(54); return;}//assume[-3]: bad file mode
      }
    }//e!=-10
  }

}//get

void sub_get2(int32 i,int64 offset,qbs *str,int32 passed){
  if (new_error) return;
  static int32 x,x2,x3,x4;


  if (i<0){//special handle?
    if (str->fixed){//following method is only for variable length strings
      static byte_element_struct tbyte_element_struct;
      sub_get(i,offset,byte_element((uint64)str->chr,str->len,&tbyte_element_struct),passed);
      return;
    }
    x=-(i+1);
    static special_handle_struct *sh; sh=(special_handle_struct*)list_get(special_handles,x); if (!sh){error(52); return;}
    if (sh->type==1){//stream
      static stream_struct *st; st=(stream_struct*)sh->index;
      stream_update(st);
      static qbs* tqbs;
      tqbs=qbs_new(st->in_size,1);
      if (st->in_size) memcpy(tqbs->chr,st->in,st->in_size);
      st->in_size=0;
      st->eof=0;
      qbs_set(str,tqbs);
      return;
    }//stream
    error(52); return;
  }//special handle


  if (gfs_fileno_valid(i)!=1){error(52); return;}//Bad file name or number
  i=gfs_fileno[i];//convert fileno to gfs index
  static gfs_file_struct *gfs;
  gfs=&gfs_file[i];
  if (gfs->type>2){error(54); return;}//Bad file mode
  if (!gfs->read){error(75); return;}//Path/file access error

  if (gfs->type==2){//BINARY (use normal sub_get)
    static byte_element_struct tbyte_element_struct;
    sub_get(gfs->fileno,offset,byte_element((uint64)str->chr,str->len,&tbyte_element_struct),passed);
    return;
  }

  if (gfs->record_length<2){error(59); return;}//Bad record length

  if (passed){
    offset--;
    if (offset<0){error(63); return;}//Bad record number
    offset*=gfs->record_length;
  }else{
    offset=-1;
  }

  static int32 e;

  static uint8 *data;
  static uint64 l,bytes;
  data=(uint8*)malloc(gfs->record_length);
  e=gfs_read(i,offset,data,gfs->record_length);//read the whole record (including header & data)
  if (e){
    if (e!=-10){//note: on eof, unread buffer area becomes NULL
      if (e==-2){error(258); return;}//invalid handle
      if (e==-3){error(54); return;}//bad file mode
      if (e==-4){error(5); return;}//illegal function call
      if (e==-7){error(70); return;}//permission denied
      error(75); return;//assume[-9]: path/file access error
    }
  }

  bytes=gfs_read_bytes();//note: any unread part of the buffer is set to NULL (by gfs_read) and is treated as valid record data
  if (!bytes){qbs_set(str,qbs_new(0,1)); free(data); return;}//as in QB when 0 bytes read, NULL string returned and (as no bytes read) no seek advancement

  //seek to beginning of next field
  //note: advancement occurs even if e==-10 (eof reached)
  if (bytes<gfs->record_length){
    if (offset!=-1){
      e=gfs_setpos(i,offset+gfs->record_length);
    }else{
      e=gfs_setpos(i,gfs_getpos(i)-bytes+gfs->record_length);
    }
    if (e){error(54); free(data); return;}//assume[-3]: bad file mode
  }

  x=2;//offset where string data will be read from
  l=((uint16*)data)[0];
  if (l&32768){
    if (gfs->record_length<8){//record length is too small to read the length!
      //restore seek to original location
      if (offset!=-1){
    e=gfs_setpos(i,offset);
      }else{
    e=gfs_setpos(i,gfs_getpos(i)-gfs->record_length);
      }
      error(59); free(data); return;//Bad record length
    }
    x=8;
    l=(l&0x7FFF)+( ( (((uint64*)data)[0]) >> 16) << 15 );
  }

  //can record_length cannot fit the required string data?
  if ((gfs->record_length-x2)<l){
    //restore seek to original location
    if (offset!=-1){
      e=gfs_setpos(i,offset);
    }else{
      e=gfs_setpos(i,gfs_getpos(i)-gfs->record_length);
    }
    error(59); free(data); return;//Bad record length
  }

  qbs_set(str,qbs_new_txt_len((char*)(data+x),l));
  free(data);
}

void sub_put(int32 i,int64 offset,void *element,int32 passed){
  if (new_error) return;
  static byte_element_struct *ele;
  static int32 x,x2;

  if (i<0){//special handle?
    x=-(i+1);
    static special_handle_struct *sh; sh=(special_handle_struct*)list_get(special_handles,x); if (!sh){error(52); return;}
    if (sh->type==1){//stream
      static stream_struct *st; st=(stream_struct*)sh->index;
      ele=(byte_element_struct*)element;
      stream_out(st,(void*)ele->offset,ele->length);
      return;
    }//stream
    error(52); return;
  }//special handle

  if (gfs_fileno_valid(i)!=1){error(52); return;}//Bad file name or number
  i=gfs_fileno[i];//convert fileno to gfs index
  static gfs_file_struct *gfs;
  gfs=&gfs_file[i];
  if (gfs->type>2){error(54); return;}//Bad file mode
  if (!gfs->write){error(75); return;}//Path/file access error

  ele=(byte_element_struct*)element;

  if (gfs->type==1){//RANDOM
    if (ele->length>gfs->record_length){error(59); return;}//Bad record length
    if (passed){
      offset--;
      if (offset<0){error(63); return;}//Bad record number
      offset*=gfs->record_length;
    }else{
      offset=-1;
    }
  }else{//BINARY
    if (passed){
      offset--;
      if (offset<0){error(63); return;}//Bad record number
    }else{offset=-1;}
  }

  static int32 e;

  e=gfs_write(i,offset,(uint8*)ele->offset,ele->length);
  if (e){
    if (e==-2){error(258); return;}//invalid handle
    if (e==-3){error(54); return;}//bad file mode
    if (e==-4){error(5); return;}//illegal function call
    if (e==-7){error(70); return;}//permission denied
    error(75); return;//assume[-9]: path/file access error
  }

  //seek to beginning of next field
  if (gfs->type==1){
    if (ele->length<gfs->record_length){
      if (offset!=-1){
    e=gfs_setpos(i,offset+gfs->record_length);
      }else{
    e=gfs_setpos(i,gfs_getpos(i)-ele->length+gfs->record_length);
      }
      if (e) error(54); return;//assume[-3]: bad file mode
    }
  }

}

//put2 adds a 2-4 byte length descriptor to the data
//(used to PUT variable length strings in RANDOM mode)
void sub_put2(int32 i,int64 offset,void *element,int32 passed){
  if (new_error) return;
  static byte_element_struct *ele;
  static int32 x;
  static uint8 *data;

  if (i<0){//special handle?
    sub_put(i,offset,element,passed);//(use standard put call)
    return;
  }

  if (gfs_fileno_valid(i)!=1){error(52); return;}//Bad file name or number
  i=gfs_fileno[i];//convert fileno to gfs index
  static gfs_file_struct *gfs;
  gfs=&gfs_file[i];
  if (gfs->type>2){error(54); return;}//Bad file mode
  if (!gfs->write){error(75); return;}//Path/file access error

  if (gfs->type==2){//BINARY (use normal sub_put)
    sub_put(gfs->fileno,offset,element,passed);
    return;
  }

  //RANDOM
  static uint64 l;
  static int64 lmask;
  lmask=-1;
  lmask>>=16;
  ele=(byte_element_struct*)element;
  l=ele->length;//note: ele->length is currently 32-bit, but sub_put2 is 64-bit compliant
  //{15}{1}[{48}]
  if (l>32767){
    data=(uint8*)malloc(l+8);
    memcpy(&data[8],(void*)(ele->offset),l);
    ((uint64*)data)[0]=0;
    ((uint16*)data)[0]=(l&32767)+32768;
    l=((l>>15)&lmask);
    ((uint64*)(data+2))[0]|=l;
    ele->length+=8;
  }else{
    data=(uint8*)malloc(l+2);
    memcpy(&data[2],(void*)(ele->offset),l);
    ((uint16*)data)[0]=l;
    ele->length+=2;
  }
  ele->offset=(uint64)&data[0];
  sub_put(gfs->fileno,offset,element,passed);
  free(data);

}//put2






void sub_graphics_get(float x1f,float y1f,float x2f,float y2f,void *element,uint32 mask,int32 passed){
  //"[{STEP}](?,?)-[{STEP}](?,?),?[,?]"
  //   &1            &2            &4
  if (new_error) return;

  static int32 x1,y1,x2,y2,z,w,h,bits,x,y,bytes,sx,sy,x3,y3,z2;
  static uint32 col,off,col1,col2,col3,col4,byte;

  if (read_page->text){error(5); return;}

  //change coordinates according to step
  if (passed&1){x1f=read_page->x+x1f; y1f=read_page->y+y1f;}
  read_page->x=x1f; read_page->y=y1f;
  if (passed&2){x2f=read_page->x+x2f; y2f=read_page->y+y2f;}
  read_page->x=x2f; read_page->y=y2f;

  //resolve coordinates
  if (read_page->clipping_or_scaling){
    if (read_page->clipping_or_scaling==2){
      x1=qbr_float_to_long(x1f*read_page->scaling_x+read_page->scaling_offset_x)+read_page->view_offset_x;
      y1=qbr_float_to_long(y1f*read_page->scaling_y+read_page->scaling_offset_y)+read_page->view_offset_y;
      x2=qbr_float_to_long(x2f*read_page->scaling_x+read_page->scaling_offset_x)+read_page->view_offset_x;
      y2=qbr_float_to_long(y2f*read_page->scaling_y+read_page->scaling_offset_y)+read_page->view_offset_y;
    }else{
      x1=qbr_float_to_long(x1f)+read_page->view_offset_x; y1=qbr_float_to_long(y1f)+read_page->view_offset_y;
      x2=qbr_float_to_long(x2f)+read_page->view_offset_x; y2=qbr_float_to_long(y2f)+read_page->view_offset_y;
    }
  }else{
    x1=qbr_float_to_long(x1f); y1=qbr_float_to_long(y1f);
    x2=qbr_float_to_long(x2f); y2=qbr_float_to_long(y2f);
  }

  //swap coordinates if reversed
  if (x2<x1){z=x1; x1=x2; x2=z;}
  if (y2<y1){z=y1; y1=y2; y2=z;}

  sx=read_page->width; sy=read_page->height;

  //boundry checking (if no mask colour was passed)
  if (!(passed&4)){
    if ((x1<0)||(y1<0)||(x2>=sx)||(y2>=sy)){error(5); return;}
  }

  static byte_element_struct *ele;
  ele=(byte_element_struct*)element;
  static uint16 *dimensions;
  dimensions=(uint16*)(ele->offset);
  static uint8 *cp,*cp1,*cp2,*cp3,*cp4;
  cp=(uint8*)(ele->offset+4);
  static uint32 *lp;
  lp=(uint32*)(ele->offset+4);

  w=x2-x1+1; h=y2-y1+1;
  bits=read_page->bits_per_pixel;

  if (bits==1){
    mask&=1;
    z=(w+7)>>3;
    bytes=z*h+4;
    if (bytes>ele->length){error(5); return;}
    dimensions[0]=w; dimensions[1]=h;
    for (y=y1;y<=y2;y++){
      z2=128;
      col2=0;
      off=y*sx+x1;
      for (x=x1;x<=x2;x++){
    if ((x>=0)&&(y>=0)&&(x<sx)&&(y<sy)) col=read_page->offset[off]; else col=mask;
    if (col) col2|=z2;
    z2>>=1; if (!z2){z2=128; *cp++=col2; col2=0;}
    off++;
      }
      if (z2!=128) *cp++=col2;
    }
    return;
  }//1

  if (bits==2){
    mask&=3;
    z=(w+7)>>3;
    bytes=z*h+4;
    if (bytes>ele->length){error(5); return;}
    dimensions[0]=w*2; dimensions[1]=h;
    for (y=y1;y<=y2;y++){
      byte=0;
      x3=0;
      off=y*sx+x1;
      for (x=x1;x<=x2;x++){
    if ((x>=0)&&(y>=0)&&(x<sx)&&(y<sy)) col=read_page->offset[off]; else col=mask;
    byte<<=2;
    byte|=col;
    if ((x3&3)==3){*cp++=byte; byte=0;}
    x3++;
    off++;
      }
      if (x3&3) *cp++=col2;
    }
    return;
  }//2

  if (bits==4){
    mask&=15;
    z=(w+7)>>3;
    bytes=z*4*h+4;
    if (bytes>ele->length){error(5); return;}
    dimensions[0]=w; dimensions[1]=h;
    y3=0;
    for (y=y1;y<=y2;y++){
      z2=128;
      off=y*sx+x1;
      cp1=cp+y3*z*4;
      cp2=cp+y3*z*4+z;
      cp3=cp+y3*z*4+z*2;
      cp4=cp+y3*z*4+z*3;
      col1=0; col2=0; col3=0; col4=0;
      for (x=x1;x<=x2;x++){
    if ((x>=0)&&(y>=0)&&(x<sx)&&(y<sy)) col=read_page->offset[off]; else col=mask;
    if (col&1) col1|=z2;
    if (col&2) col2|=z2;
    if (col&4) col3|=z2;
    if (col&8) col4|=z2;
    z2>>=1; if (!z2){z2=128; *cp1++=col1; *cp2++=col2; *cp3++=col3; *cp4++=col4; col1=0; col2=0; col3=0; col4=0;}
    off++;
      }
      if (z2!=128){*cp1=col1; *cp2=col2; *cp3=col3; *cp4=col4;}
      y3++;
    }
    return;
  }//4

  if (bits==8){
    mask&=255;
    bytes=w*h+4;
    if (bytes>ele->length){error(5); return;}
    dimensions[0]=w*8; dimensions[1]=h;
    for (y=y1;y<=y2;y++){
      off=y*sx+x1;
      for (x=x1;x<=x2;x++){
    if ((x>=0)&&(y>=0)&&(x<sx)&&(y<sy)) col=read_page->offset[off]; else col=mask;
    *cp++=col;
    off++;
      }}
    return;
  }//8

  if (bits==32){
    bytes=w*h*4+4;
    if (bytes>ele->length){error(5); return;}
    dimensions[0]=w; dimensions[1]=h;//note: width is left unmultiplied
    for (y=y1;y<=y2;y++){
      off=y*sx+x1;
      for (x=x1;x<=x2;x++){
    if ((x>=0)&&(y>=0)&&(x<sx)&&(y<sy)) col=read_page->offset32[off]; else col=mask;
    *lp++=col;
    off++;
      }}
    return;
  }//32

}//sub_graphics_get



void sub_graphics_put(float x1f,float y1f,void *element,int32 option,uint32 mask,int32 passed){
  //"[{STEP}](?,?),?[,[{_CLIP}][{PSET|PRESET|AND|OR|XOR}][,?]]"
  //step->passed&1
  //clip->passed&2
  //mask->passed&4

  if (new_error) return;

  static int32 step,clip;
  step=0;
  clip=0;
  if (passed&1){step=1; passed-=1;}
  if (passed&2){clip=1; passed-=2;}

  static int32 x1,y1,x2,y2,z,w,h,bits,x,y,bytes,sx,sy,x3,y3,z2;
  static uint32 col,off,col1,col2,col3,col4,byte,pixelmask;

  if (write_page->text){error(5); return;}

  //change coordinates according to step
  if (step){
    x1f+=write_page->x; y1f+=write_page->y;
    write_page->x=x1f; write_page->y=y1f;
  }

  //resolve coordinates
  if (write_page->clipping_or_scaling){
    if (write_page->clipping_or_scaling==2){
      x1=qbr_float_to_long(x1f*write_page->scaling_x+write_page->scaling_offset_x)+write_page->view_offset_x;
      y1=qbr_float_to_long(y1f*write_page->scaling_y+write_page->scaling_offset_y)+write_page->view_offset_y;
    }else{
      x1=qbr_float_to_long(x1f)+write_page->view_offset_x; y1=qbr_float_to_long(y1f)+write_page->view_offset_y;
    }
  }else{
    x1=qbr_float_to_long(x1f); y1=qbr_float_to_long(y1f);
  }

  sx=write_page->width; sy=write_page->height;
  bits=write_page->bits_per_pixel;

  static byte_element_struct *ele;
  ele=(byte_element_struct*)element;
  static uint16 *dimensions;
  dimensions=(uint16*)(ele->offset);
  static uint8 *cp,*cp1,*cp2,*cp3,*cp4;
  cp=(uint8*)(ele->offset+4);
  static uint32 *lp;
  lp=(uint32*)(ele->offset+4);

  static uint8 *offp;
  static uint32 *off32p;

  if (4>ele->length){error(5); return;}

  //get dimensions
  w=dimensions[0]; h=dimensions[1];
  z=w;//(used below)
  if (bits==2){if (w&1){error(5); return;} else w>>=1;}
  if (bits==8){if (w&7){error(5); return;} else w>>=3;}
  x2=x1+w-1; y2=y1+h-1;

  //boundry checking (if CLIP option was not used)
  if (!clip){
    if ((x1<0)||(y1<0)||(x2>=sx)||(y2>=sy)){error(5); return;}
  }

  //array size check (avoid reading unacclocated memory)
  if (bits==32) z*=32;
  z=(z+7)>>3;//bits per row->bytes per row
  bytes=h*z;
  if (bits==4) bytes*=4;
  if ((bytes+4)>ele->length){error(5); return;}

  pixelmask=write_page->mask;

  if (bits==1){
    mask&=1;
    y3=0;
    for (y=y1;y<=y2;y++){
      offp=(uint8*)write_page->offset+(y*sx+x1);
      x3=0;
      for (x=x1;x<=x2;x++){
    if (!(x3--)){x3=7; col2=*cp++;}
    if ((x>=0)&&(y>=0)&&(x<sx)&&(y<sy)){
      col=(col2>>x3)&1;
      if ((!passed)||(col!=mask)){
        switch(option){
        case 0: *offp^=col; break;
        case 1: *offp=col; break;
        case 2: *offp=(~col)&pixelmask; break;
        case 3: *offp&=col; break;
        case 4: *offp|=col; break;
        case 5: *offp^=col; break;
        }
      }//mask
    }//bounds
    offp++;
      }
      y3++;
    }
    return;
  }//1

  if (bits==2){
    mask&=3;
    y3=0;
    for (y=y1;y<=y2;y++){
      offp=(uint8*)write_page->offset+(y*sx+x1);
      x3=0;
      for (x=x1;x<=x2;x++){
    if (!(x3--)){x3=3; col2=*cp++;}
    if ((x>=0)&&(y>=0)&&(x<sx)&&(y<sy)){
      col=(col2>>(x3<<1))&3;
      if ((!passed)||(col!=mask)){
        switch(option){
        case 0: *offp^=col; break;
        case 1: *offp=col; break;
        case 2: *offp=(~col)&pixelmask; break;
        case 3: *offp&=col; break;
        case 4: *offp|=col; break;
        case 5: *offp^=col; break;
        }
      }//mask
    }//bounds
    offp++;
      }
      y3++;
    }
    return;
  }//2

  if (bits==4){
    mask&=15;
    y3=0;
    for (y=y1;y<=y2;y++){
      offp=(uint8*)write_page->offset+(y*sx+x1);
      cp1=cp+y3*z*4;
      cp2=cp+y3*z*4+z;
      cp3=cp+y3*z*4+z*2;

      cp4=cp+y3*z*4+z*3;
      x3=0;
      for (x=x1;x<=x2;x++){
    if (!(x3--)){x3=7; col1=*cp1++; col2=(*cp2++)<<1; col3=(*cp3++)<<2; col4=(*cp4++)<<3;}
    if ((x>=0)&&(y>=0)&&(x<sx)&&(y<sy)){
      col=((col1>>x3)&1)|((col2>>x3)&2)|((col3>>x3)&4)|((col4>>x3)&8);
      if ((!passed)||(col!=mask)){
        switch(option){
        case 0: *offp^=col; break;
        case 1: *offp=col; break;
        case 2: *offp=(~col)&pixelmask; break;
        case 3: *offp&=col; break;
        case 4: *offp|=col; break;
        case 5: *offp^=col; break;
        }
      }//mask
    }//bounds
    offp++;
      }
      y3++;
    }
    return;
  }//4

  if (bits==8){
    mask&=255;
    for (y=y1;y<=y2;y++){
      offp=(uint8*)write_page->offset+(y*sx+x1);
      for (x=x1;x<=x2;x++){
    if ((x>=0)&&(y>=0)&&(x<sx)&&(y<sy)){
      col=*cp;
      if ((!passed)||(col!=mask)){
        switch(option){
        case 0: *offp^=col; break;
        case 1: *offp=col; break;
        case 2: *offp=(~col)&pixelmask; break;
        case 3: *offp&=col; break;
        case 4: *offp|=col; break;
        case 5: *offp^=col; break;
        }
      }//mask
    }//bounds
    offp++;
    cp++;
      }}
    return;
  }//8

  if (bits==32){
    for (y=y1;y<=y2;y++){
      off32p=(uint32*)write_page->offset32+(y*sx+x1);
      for (x=x1;x<=x2;x++){
    if ((x>=0)&&(y>=0)&&(x<sx)&&(y<sy)){
      col=*lp;
      if ((!passed)||(col!=mask)){
        switch(option){
        case 0: *off32p^=col; break;
        case 1: *off32p=col; break;
        case 2: *off32p=(~col)&pixelmask; break;
        case 3: *off32p&=col; break;
        case 4: *off32p|=col; break;
        case 5: *off32p^=col; break;
        }
      }//mask
    }//bounds
    off32p++;
    lp++;
      }}
    return;
  }//32


















  /*
    static byte_element_struct *ele;
    ele=(byte_element_struct*)element;
    static int32 x,y;
    static int32 sx,sy,c,px,py;
    static uint8 *cp;
    static int32 *lp;

    sx=((uint16*)ele->offset)[0];
    sy=((uint16*)ele->offset)[1];
    cp=(uint8*)(ele->offset+4);
    lp=(int32*)cp;


    static int32 sizeinbytes;
    static int32 byte;
    static int32 bitvalue;
    static int32 bytesperrow;
    static int32 row2offset;
    static int32 row3offset;
    static int32 row4offset;

    static int32 longval;

    if (write_page->bits_per_pixel==8){
    mask&=255;
    //create error if not divisible by 8!
    sx>>=3;
    }

    if (write_page->bits_per_pixel==1){
    mask&=1;
    }

    if (write_page->bits_per_pixel==2){
    mask&=3;
    sx>>=1;
    }


    if (write_page->bits_per_pixel==4){
    mask&=15;
    bytesperrow=sx>>3; if (sx&7) bytesperrow++;
    row2offset=bytesperrow;
    row3offset=bytesperrow*2;
    row4offset=bytesperrow*3;
    }


    for (y=0;y<sy;y++){
    py=y1+y;

    if (write_page->bits_per_pixel==4){
    bitvalue=128;
    byte=0;
    }

    for (x=0;x<sx;x++){
    px=x1+x;


    //get colour
    if (write_page->bits_per_pixel==8){
    c=*cp;
    cp++;
    }

    if (write_page->bits_per_pixel==4){
    byte=x>>3;
    c=0;
    if (cp[byte]&bitvalue) c|=1;
    if (cp[row2offset+byte]&bitvalue) c|=2;
    if (cp[row3offset+byte]&bitvalue) c|=4;
    if (cp[row4offset+byte]&bitvalue) c|=8;
    bitvalue>>=1; if (bitvalue==0) bitvalue=128;
    }


    if (write_page->bits_per_pixel==1){
    if (!(x&7)){
    byte=*cp;
    cp++;
    }
    c=(byte&128)>>7; byte<<=1;
    }

    if (write_page->bits_per_pixel==2){
    if (!(x&3)){
    byte=*cp;
    cp++;
    }
    c=(byte&192)>>6; byte<<=2;
    }


    if ((px>=0)&&(px<write_page->width)&&(py>=0)&&(py<write_page->height)){

    //check color
    if (passed){
    if (c==mask) goto maskpixel;
    }


    //"pset" color

    //PUT[{STEP}](?,?),?[,[{PSET|PRESET|AND|OR|XOR}][,[?]]]
    //apply option

    if (option==1){
    write_page->offset[py*write_page->width+px]=c;
    }
    if (option==2){
    //PRESET=bitwise NOT
    write_page->offset[py*write_page->width+px]=(~c)&write_page->mask;
    }
    if (option==3){
    write_page->offset[py*write_page->width+px]&=c;
    }
    if (option==4){
    write_page->offset[py*write_page->width+px]|=c;
    }
    if ((option==5)||(option==0)){
    write_page->offset[py*write_page->width+px]^=c;
    }






    }
    maskpixel:;


    }//x


    if (write_page->bits_per_pixel==4) cp+=(bytesperrow*4);

    //if (_bits_per_pixel==1){
    // if (sx&7) cp++;
    //}

    //if (_bits_per_pixel==2){
    // if (sx&3) cp++;
    //}


    }//y
  */

}



void sub_date(qbs* date){
  if (new_error) return;
  return;//stub
}

qbs *func_date(){
  //mm-dd-yyyy
  //0123456789
  static time_t qb64_tm_val;
  static tm *qb64_tm;
  //struct tm {
  //        int tm_sec;     /* seconds after the minute - [0,59] */
  //        int tm_min;     /* minutes after the hour - [0,59] */
  //        int tm_hour;    /* hours since midnight - [0,23] */
  //        int tm_mday;    /* day of the month - [1,31] */
  //        int tm_mon;     /* months since January - [0,11] */
  //        int tm_year;    /* years since 1900 */
  //        int tm_wday;    /* days since Sunday - [0,6] */
  //        int tm_yday;    /* days since January 1 - [0,365] */
  //        int tm_isdst;   /* daylight savings time flag */
  //        };
  static int32 x,x2,i;
  static qbs *str;
  str=qbs_new(10,1);
  str->chr[2]=45; str->chr[5]=45;//-
  time(&qb64_tm_val); if (qb64_tm_val==-1){error(5); str->len=0; return str;}
  qb64_tm=localtime(&qb64_tm_val); if (qb64_tm==NULL){error(5); str->len=0; return str;}
  x=qb64_tm->tm_mon; x++; 
  i=0; str->chr[i]=x/10+48; str->chr[i+1]=x%10+48;
  x=qb64_tm->tm_mday;
  i=3; str->chr[i]=x/10+48; str->chr[i+1]=x%10+48;
  x=qb64_tm->tm_year; x+=1900;
  i=6;
  x2=x/1000; x=x-x2*1000; str->chr[i]=x2+48; i++;
  x2=x/100; x=x-x2*100; str->chr[i]=x2+48; i++;
  x2=x/10; x=x-x2*10; str->chr[i]=x2+48; i++;
  str->chr[i]=x+48;
  return str;
}








void sub_time(qbs* str){
  if (new_error) return;
  return;//stub
}

qbs *func_time(){
  //23:59:59 (hh:mm:ss)
  //01234567
  static time_t qb64_tm_val;
  static tm *qb64_tm;
  //struct tm {
  //        int tm_sec;     /* seconds after the minute - [0,59] */
  //        int tm_min;     /* minutes after the hour - [0,59] */
  //        int tm_hour;    /* hours since midnight - [0,23] */
  //        int tm_mday;    /* day of the month - [1,31] */
  //        int tm_mon;     /* months since January - [0,11] */
  //        int tm_year;    /* years since 1900 */
  //        int tm_wday;    /* days since Sunday - [0,6] */
  //        int tm_yday;    /* days since January 1 - [0,365] */
  //        int tm_isdst;   /* daylight savings time flag */
  //        };
  static int32 x,x2,i;
  static qbs *str;
  str=qbs_new(8,1);
  str->chr[2]=58; str->chr[5]=58;//:
  time(&qb64_tm_val); if (qb64_tm_val==-1){error(5); str->len=0; return str;}
  qb64_tm=localtime(&qb64_tm_val); if (qb64_tm==NULL){error(5); str->len=0; return str;}
  x=qb64_tm->tm_hour;
  i=0; str->chr[i]=x/10+48; str->chr[i+1]=x%10+48;
  x=qb64_tm->tm_min;
  i=3; str->chr[i]=x/10+48; str->chr[i+1]=x%10+48;
  x=qb64_tm->tm_sec;
  i=6; str->chr[i]=x/10+48; str->chr[i+1]=x%10+48;
  return str;
}


int32 func_csrlin(){
  if (write_page->holding_cursor){
    if (write_page->cursor_y>=write_page->bottom_row) return write_page->bottom_row; else return write_page->cursor_y+1;
  }
  return write_page->cursor_y;
}
int32 func_pos(int32 ignore){
  if (write_page->holding_cursor) return 1;
  return write_page->cursor_x;
}



double func_log(double value){
  if (value<=0){error(5);return 0;}
  return log(value);
}

//FIX
double func_fix_double(double value){
  if (value<0) return ceil(value); else return floor(value);
}
long double func_fix_float(long double value){
  if (value<0) return ceil(value); else return floor(value);
}

//EXP
double func_exp_single(double value){
  if (value<=88.02969){
    return exp(value);
  }
  error(6); return 0;
}
long double func_exp_float(long double value){
  if (value<=709.782712893){
    return exp(value);
  }
  error(6); return 0;
}



int32 sleep_break=0;

void sub_sleep(int32 seconds,int32 passed){
  if (new_error) return;
  sleep_break=0;
  double prev,ms,now,elapsed;//cannot be static
  if (passed) prev=GetTicks();
  ms=1000.0*(double)seconds;
 recalculate:
 wait:
  evnt(0);//handle general events
  //exit condition checks/events
  if (sleep_break) return;
  if (stop_program) return;
  if (ms<=0){//untimed SLEEP
    Sleep(9);
    goto wait;
  }
  now=GetTicks();
  if (now<prev){//value looped?
    return;
  }
  elapsed=now-prev;//elapsed time since prev
  if (elapsed<ms){
    int64 wait;//cannot be static
    wait=ms-elapsed;
    if (!wait) wait=1;
    if (wait>=10){
      Sleep(9);  
      //recalculate time
      goto recalculate;
    }else{
      Sleep(wait);
    }
  }
  return;
}


qbs *func_oct(int64 value,int32 neg_bits){

  static int32 i,i2,i3,x,x2,neg;
  static int64 value2;
  static qbs *str;

  str=qbs_new(22,1);

  //negative?
  if ((value>>63)&1) neg=1; else neg=0;

  //calc. most significant bit
  i2=0;
  value2=value;
  if (neg){
    for (i=1;i<=64;i++){
      if (!(value2&1)) i2=i;
      value2>>=1;
    }
    if (i2>=neg_bits){
      //doesn't fit in neg_bits, so expand to next 16/32/64 boundary
      i3=64;
      if (i2<32) i3=32;
      if (i2<16) i3=16;
      i2=i3;
    }else i2=neg_bits;
  }else{
    for (i=1;i<=64;i++){
      if (value2&1) i2=i;
      value2>>=1;
    }
  }

  if (!i2){str->chr[0]=48; str->len=1; return str;}//"0"

  //calc. number of characters required in i3
  i3=i2/3; if ((i3*3)!=i2) i3++;

  //build string
  str->len=i3;
  i3--;
  x=0; x2=0;
  for (i=1;i<=i2;i++){
    if (value&1) x2|=(1<<x);
    value>>=1;
    x++;
    if (x==3){str->chr[i3--]=x2+48; x2=0; x=0;}
  }
  if (x) str->chr[i3]=x2+48;

  return str;

}

//note: QBASIC uses 8 characters for SINGLE/DOUBLE or generates "OVERFLOW" if this range is exceeded
//      QB64   uses 8 characters for SINGLE/DOUBLE/FLOAT but if this range is exceeded
//      it uses up to 16 characters before generating an "OVERFLOW" error
//performs overflow check before calling func_hex
qbs *func_oct_float(long double value){
  static qbs *str;
  static int64 ivalue;
  static int64 uivalue;
  //ref: uint64 0-18446744073709551615
  //      int64 \969223372036854775808 to 9223372036854775807
  if ((value>=9.223372036854776E18)||(value<=-9.223372036854776E18)){
    //note: ideally, the following line would be used, however, qbr_longdouble_to_uint64 just does the same as qbr
    //if ((value>=1.844674407370956E19)||(value<=-9.223372036854776E18)){
    str=qbs_new(0,1); error(6);//Overflow
    return str;
  }
  if (value>=0){
    uivalue=qbr_longdouble_to_uint64(value);
    ivalue=uivalue;
  }else{
    ivalue=qbr(value);
  }
  return func_oct(ivalue,32);
}

qbs *func_hex(int64 value,int32 neg_size){
  //note: negative int64 values can be treated as positive uint64 values (and vise versa)

  static int32 i,i2,i3,x,neg;
  static int64 value2;
  static qbs *str;

  str=qbs_new(16,1);

  value2=value;
  i2=0; i3=0;
  for (i=1;i<=16;i++){
    if (value2&15) i2=i;//most significant digit of positive value
    if ((value2&15)!=15){
      i3=i;//most significant digit of negative value
      if ((((value2&8)==0)&&(i!=16))) i3++;//for a negative number to fit into 4/8 characters, its top bit must be on
    }
    x=value2&15; if (x>9) x+=55; else x+=48; str->chr[16-i]=x;
    value2>>=4;
  }
  if (!i2){str->chr[0]=48; str->len=1; return str;}//"0"

  //negative?
  if ((value>>63)&1) neg=1; else neg=0;

  //change i2 from sig-digits to string-output-digits
  if (neg){
    if (i3<=neg_size){
      i2=neg_size;//extend to minimum character size
    }else{
      //didn't fit in recommended size, expand to either 4, 8 or 16 appropriately
      i2=16;
      if (i3<=8) i2=8;
      if (i3<=4) i2=4;
    }
  }//neg

  //adjust string to the left to remove unnecessary characters
  if (i2!=16){
    memmove(str->chr,str->chr+(16-i2),i2);
    str->len=i2;
  }

  return str;

}

//note: QBASIC uses 8 characters for SINGLE/DOUBLE or generates "OVERFLOW" if this range is exceeded
//      QB64   uses 8 characters for SINGLE/DOUBLE/FLOAT but if this range is exceeded
//      it uses up to 16 characters before generating an "OVERFLOW" error
//performs overflow check before calling func_hex
qbs *func_hex_float(long double value){
  static qbs *str;
  static int64 ivalue;
  static int64 uivalue;
  //ref: uint64 0-18446744073709551615
  //      int64 \969223372036854775808 to 9223372036854775807
  if ((value>=9.223372036854776E18)||(value<=-9.223372036854776E18)){
    //note: ideally, the following line would be used, however, qbr_longdouble_to_uint64 just does the same as qbr
    //if ((value>=1.844674407370956E19)||(value<=-9.223372036854776E18)){
    str=qbs_new(0,1); error(6);//Overflow
    return str;
  }
  if (value>=0){
    uivalue=qbr_longdouble_to_uint64(value);
    ivalue=uivalue;
  }else{
    ivalue=qbr(value);
  }
  return func_hex(ivalue,8);
}

ptrszint func_lbound(ptrszint *array,int32 index,int32 num_indexes){
  if ((index<1)||(index>num_indexes)||((array[2]&1)==0)){
    error(9); return 0;
  }
  index=num_indexes-index+1;
  return array[4*index];
}

ptrszint func_ubound(ptrszint *array,int32 index,int32 num_indexes){
  if ((index<1)||(index>num_indexes)||((array[2]&1)==0)){
    error(9); return 0;
  }
  index=num_indexes-index+1;
  return array[4*index]+array[4*index+1]-1;
}

uint8 port60h_event[256];
int32 port60h_events=0;


int32 func_inp(int32 port){
  static int32 value;
  unsupported_port_accessed=0;
  if ((port>65535)||(port<-65536)){
    error(6); return 0;//Overflow
  }
  port&=0xFFFF;

  if (port==0x3C9){//read palette
    if (write_page->pal){//avoid NULL pointer
      //convert 0-255 value to 0-63 value
      if (H3C9_read_next==0){//red
    value=qbr_double_to_long((((double)((write_page->pal[H3C7_palette_register_read_index]>>16)&255))/3.984376-0.4999999f));
      }
      if (H3C9_read_next==1){//green
    value=qbr_double_to_long((((double)((write_page->pal[H3C7_palette_register_read_index]>>8)&255))/3.984376-0.4999999f));
      }
      if (H3C9_read_next==2){//blue
    value=qbr_double_to_long((((double)(write_page->pal[H3C7_palette_register_read_index]&255))/3.984376-0.4999999f));
      }
      H3C9_read_next=H3C9_read_next+1;
      if (H3C9_read_next==3){
    H3C9_read_next=0;
    H3C7_palette_register_read_index=H3C7_palette_register_read_index+1;
    H3C7_palette_register_read_index&=0xFF;
      }
      return value;
    }//->pal
    return 0;//non-palette modes
  }

  /*
    3dAh (R):  Input Status #1 Register
    bit   0  Either Vertical or Horizontal Retrace active if set
    1  Light Pen has triggered if set
    2  Light Pen switch is open if set
    3  Vertical Retrace in progress if set
    4-5  Shows two of the 6 color outputs, depending on 3C0h index 12h.
    Attr: Bit 4-5:   Out bit 4  Out bit 5
    0          Blue       Red
    1        I Blue       Green
    2        I Red      I Green
  */
  if (port==0x3DA){
    value=0;
    if (vertical_retrace_happened||vertical_retrace_in_progress){
      vertical_retrace_happened=0;
      value|=8;
    }
    return value;
  }

  if (port==0x60){
    //return last scancode event
    if (port60h_events){
      value=port60h_event[0];
      if (port60h_events>1) memmove(port60h_event,port60h_event+1,255);
      port60h_events--;
      return value;
    }else{
      return port60h_event[0];
    }

  }



  unsupported_port_accessed=1;
  return 0;//unknown port!
}

void sub_wait(int32 port,int32 andexpression,int32 xorexpression,int32 passed){
  if (new_error) return;
  //1. read value from port
  //2. value^=xorexpression (if passed!)
  //3. value^=andexpression
  //IMPORTANT: Wait returns immediately if given port is unsupported by QB64 so program
  //           can continue
  static int32 value;

  //error & range checking
  if ((port>65535)||(port<-65536)){
    error(6); return;//Overflow
  }
  port&=0xFFFF;
  if ((andexpression<-32768)||(andexpression>65535)){
    error(6); return;//Overflow
  }
  andexpression&=0xFF;
  if (passed){
    if ((xorexpression<-32768)||(xorexpression>65535)){
      error(6); return;//Overflow
    }
  }
  xorexpression&=0xFF;

 wait:
  value=func_inp(port);
  if (passed) value^=xorexpression;
  value&=andexpression;
  if (value||unsupported_port_accessed||stop_program) return;
  Sleep(1);
  goto wait;
}

extern int32 tab_LPRINT;//1=dest is LPRINT image
extern int32 tab_spc_cr_size; //=1;//default
extern int32 tab_fileno;
qbs *func_tab(int32 pos){
  if (new_error) return qbs_new(0,1);

  static int32 tab_LPRINT_olddest;
  if (tab_LPRINT){
    if (!lprint_image) qbs_lprint(qbs_new(0,1),0);//send dummy data to init the LPRINT image
    tab_LPRINT_olddest=func__dest();
    sub__dest(lprint_image);
  }

  //returns a string to advance to the horizontal position "pos" on either
  //the current line or the next line.
  static int32 w,div,cursor;
  //calculate width in spaces & current position
  if (tab_spc_cr_size==2){
    //print to file
    div=1;
    w=2147483647;
    cursor=1;
    //validate file
    static int32 i;
    i=tab_fileno;
    if (i<0) goto invalid_file;//TCP/IP unsupported
    if (gfs_fileno_valid(i)!=1) goto invalid_file;//Bad file name or number
    i=gfs_fileno[i];//convert fileno to gfs index
    cursor=gfs_file[i].column;
  invalid_file:;
  }else{
    //print to surface
    if (write_page->text){
      w=write_page->width;
      div=1;
    }else{
      if (fontwidth[write_page->font]){
    w=write_page->width/fontwidth[write_page->font];
    div=1;
      }else{
    //w=func__printwidth(singlespace,NULL,0);
    w=write_page->width;
    div=func__printwidth(singlespace,NULL,0);
      }
    }
    cursor=write_page->cursor_x;
  }

  static qbs *tqbs;
  if ((pos<-32768)||(pos>32767)){
    if (tab_LPRINT) sub__dest(tab_LPRINT_olddest);
    tqbs=qbs_new(0,1);
    error(7); return tqbs;//Overflow
  }
  if (pos>w) pos%=w;
  if (pos<1) pos=1;
  static int32 size,spaces,cr;
  size=0; spaces=0; cr=0;
  if (cursor>pos){
    cr=1;
    size=tab_spc_cr_size;
    spaces=pos/div; if (pos%div) spaces++;
    spaces--;//don't put a space on the dest position
    size+=spaces;
  }else{
    spaces=(pos-cursor)/div; if ((pos-cursor)%div) spaces++;
    size=spaces;
  }
  //build custom string
  tqbs=qbs_new(size,1);
  if (cr){
    tqbs->chr[0]=13; if (tab_spc_cr_size==2) tqbs->chr[1]=10;
    memset(&tqbs->chr[tab_spc_cr_size],32,spaces);
  }else{
    memset(tqbs->chr,32,spaces);
  }
  if (tab_LPRINT) sub__dest(tab_LPRINT_olddest);
  return tqbs;
}

qbs *func_spc(int32 spaces){
  if (new_error) return qbs_new(0,1);

  static qbs *tqbs;
  if ((spaces<-32768)||(spaces>32767)){tqbs=qbs_new(0,1); error(7); return tqbs;}//Overflow
  if (spaces<0) spaces=0;

  //for files, spc simply adds that many spaces
  if (tab_spc_cr_size==2){//files
    tqbs=qbs_new(spaces,1);
    memset(tqbs->chr,32,spaces);
    return tqbs;
  }

  static int32 tab_LPRINT_olddest;
  if (tab_LPRINT){
    if (!lprint_image) qbs_lprint(qbs_new(0,1),0);//send dummy data to init the LPRINT image
    tab_LPRINT_olddest=func__dest();
    sub__dest(lprint_image);
  }

  //for screens, spc adds that many spaces MOD screen_width_in_characters
  //if 2 rows are bridged, the top row's characters are not printed to, just the lower
  static int32 x,x2;
  //calc spaces remaining on current screen row & MOD
  static int32 spaces_left_on_line;
  static qbs *onespace=NULL; if (!onespace){onespace=qbs_new(1,0); onespace->chr[0]=32;}
  static int32 onespace_width;//for variable width fonts
  if (write_page->text){
    spaces_left_on_line=write_page->width-write_page->cursor_x+1;
    spaces%=write_page->width;//MOD
  }else{
    x=fontwidth[write_page->font]; 
    if (x){
      x2=write_page->width/x;//characters per row
      spaces_left_on_line=x2-write_page->cursor_x+1;
      spaces%=x2;//MOD
    }else{
      x2=write_page->width-write_page->cursor_x+1;//pixels left on row
      onespace_width=func__printwidth(onespace,NULL,0);
      spaces_left_on_line=x2/onespace_width;
      spaces%=(write_page->width/onespace_width);//MOD
    }
  }

  //build string
  if (spaces_left_on_line>=spaces){
    tqbs=qbs_new(spaces,1);
    memset(tqbs->chr,32,spaces);
  }else{
    spaces-=spaces_left_on_line;
    tqbs=qbs_new(1+spaces,1);
    tqbs->chr[0]=13;
    memset(tqbs->chr+1,32,spaces);
  }

  if (tab_LPRINT) sub__dest(tab_LPRINT_olddest);
  return tqbs;
}

float func_pmap(float val,int32 option){
  static int32 x,y;
  if (new_error) return 0;
  if (!write_page->text){
    //note: for QBASIC/4.5/7.1 compatibility clipping_or_scaling check is skipped
    if (option==0){
      x=qbr_float_to_long(val*write_page->scaling_x+write_page->scaling_offset_x);
      return x;
    }
    if (option==1){
      y=qbr_float_to_long(val*write_page->scaling_y+write_page->scaling_offset_y);
      return y;
    }
    if (option==2){
      return (((double)qbr_float_to_long(val))-write_page->scaling_offset_x)/write_page->scaling_x;
    }
    if (option==3){
      return (((double)qbr_float_to_long(val))-write_page->scaling_offset_y)/write_page->scaling_y;
    }
  }//!write_page->text
  error(5);
  return 0;
}



uint32 func_screen(int32 y,int32 x,int32 returncol,int32 passed){

  static uint8 chr[65536];
  static int32 x2,y2,x3,y3,i,i2,i3;
  static uint32 col,firstcol;
  uint8 *cp;

  if (!passed) returncol=0;

  if (read_page->text){
    //on screen?
    if ((y<1)||(y>read_page->height)){error(5); return 0;}
    if ((x<1)||(x>read_page->width)){error(5); return 0;}
    if (returncol){
      return read_page->offset[((y-1)*read_page->width+x-1)*2+1];
    }
    return read_page->offset[((y-1)*read_page->width+x-1)*2];
  }

  //only 8x8,8x14,8x16 supported
  if ((read_page->font!=8)&&(read_page->font!=14)&&(read_page->font!=16)){error(5); return 0;}

  //on screen?
  x2=read_page->width/fontwidth[read_page->font];
  y2=read_page->height/fontheight[read_page->font];
  if ((y<1)||(y>y2)){error(5); return 0;}
  if ((x<1)||(x>x2)){error(5); return 0;}

  //create "signature" of character on screen
  x--; y--;
  i=0;
  i3=1;
  y3=y*fontheight[read_page->font];
  for (y2=0;y2<fontheight[read_page->font];y2++){
    x3=x*fontwidth[read_page->font];
    for (x2=0;x2<fontwidth[read_page->font];x2++){
      col=point(x3,y3);
      if (col){
    if (i3){i3=0;firstcol=col;}
    col=255;
      }
      chr[i]=col;
      i++;
      x3++;
    }
    y3++;
  }

  if (i3){//assume SPACE, no non-zero pixels were found
    if (returncol) return 1;
    return 32;
  }

  i3=i;//number of bytes per character "signature"

  //compare signature with all ascii characters
  for (i=0;i<=255;i++){
    if (read_page->font==8) cp=&charset8x8[i][0][0];
    if (read_page->font==14) cp=&charset8x16[i][1][0];
    if (read_page->font==16) cp=&charset8x16[i][0][0];
    i2=memcmp(cp,chr,i3);
    if (!i2){//identical!
      if (returncol) return firstcol;
      return i;
    }
  }

  //no match found
  if (returncol) return 0;
  return 32;
}

void sub_bsave(qbs *filename,int32 offset,int32 size){
  if (new_error) return;
  static ofstream fh;

  static qbs *tqbs=NULL;
  if (!tqbs) tqbs=qbs_new(0,0);
  static qbs *nullt=NULL;
  if (!nullt) nullt=qbs_new(1,0);

  static int32 x;
  nullt->chr[0]=0;
  if ((offset<-65536)||(offset>65535)){error(6); return;}//Overflow
  offset&=0xFFFF;
  //note: QB64 allows a maximum of 65536 bytes, QB only allows 65535
  if ((size<-65536)||(size>65536)){error(6); return;}//Overflow
  if (size!=65536) size&=0xFFFF;
  qbs_set(tqbs,qbs_add(filename,nullt));//prepare null-terminated filename
  fh.open(fixdir(tqbs),ios::binary|ios::out);
  if (fh.is_open()==NULL){error(64); return;}//Bad file name
  x=253; fh.write((char*)&x,1);//signature byte
  x=(defseg-&cmem[0])/16; fh.write((char*)&x,2);//segment
  x=offset; fh.write((char*)&x,2);//offset
  x=size; if (x>65535) x=0;//if filesize>65542 then size=filesize-7
  fh.write((char*)&x,2);//size
  fh.write((char*)(defseg+offset),size);//data
  fh.close();
}

void sub_bload(qbs *filename,int32 offset,int32 passed){
  if (new_error) return;
  static uint8 header[7];
  static ifstream fh;
  static qbs *tqbs=NULL;
  if (!tqbs) tqbs=qbs_new(0,0);
  static qbs *nullt=NULL;
  if (!nullt) nullt=qbs_new(1,0);


  static int32 x,file_seg,file_off,file_size;
  nullt->chr[0]=0;
  if (passed){
    if ((offset<-65536)||(offset>65535)){error(6); return;}//Overflow
    offset&=0xFFFF;
  }
  qbs_set(tqbs,qbs_add(filename,nullt));//prepare null-terminated filename
  fh.open(fixdir(tqbs),ios::binary|ios::in);
  if (fh.is_open()==NULL){error(53); return;}//File not found
  fh.read((char*)header,7); if (fh.gcount()!=7) goto error;
  if (header[0]!=253) goto error;
  file_seg=header[1]+header[2]*256;
  file_off=header[3]+header[4]*256;
  file_size=header[5]+header[6]*256;
  if (file_size==0){
    fh.seekg(0,ios::end);
    file_size=fh.tellg();
    fh.seekg(7,ios::beg);
    file_size-=7;
    if (file_size<65536) file_size=0;
  }
  if (passed){
    fh.read((char*)(defseg+offset),file_size);
  }else{
    fh.read((char*)(&cmem[0]+file_seg*16+file_off),file_size);
  }
  if (fh.gcount()!=file_size) goto error;
  fh.close();
  return;
 error:
  fh.close();
  error(54);//Bad file mode
}

int64 func_lof(int32 i){
  static int64 size;

  if (i<0){//special handle?
    static special_handle_struct *sh; sh=(special_handle_struct*)list_get(special_handles,-(i+1)); if (!sh){error(52); return 0;}
    if (sh->type==1){//stream
      static stream_struct *st; st=(stream_struct*)sh->index;
      stream_update(st);
      return st->in_size;
    }//stream
    error(52); return 0;
  }//special handle

  if (gfs_fileno_valid(i)!=1){error(52); return 0;}//Bad file name or number
  i=gfs_fileno[i];//convert fileno to gfs index
  size=gfs_lof(i);
  if (size<0){
    if (size==-2){error(258); return 0;}//invalid handle
    if (size==-3){error(54); return 0;}//bad file mode
    if (size==-4){error(5); return 0;}//illegal function call
    error(75); return 0;//assume[-9]: path/file access error
  }
  return size;
}

int32 func_eof(int32 i){
  static int32 pos,lof,x;

  if (i<0){//special handle?
    x=-(i+1);
    static special_handle_struct *sh; sh=(special_handle_struct*)list_get(special_handles,x); if (!sh){error(52); return 0;}
    if (sh->type==1){//stream
      static stream_struct *st; st=(stream_struct*)sh->index;
      if (st->eof) return -1;
      return 0;
    }//stream
    error(52); return 0;
  }//special handle

  if (gfs_fileno_valid(i)!=1){error(52); return 0;}//Bad file name or number
  i=gfs_fileno[i];//convert fileno to gfs index
  static gfs_file_struct *gfs;
  gfs=&gfs_file[i];
  if (gfs->scrn){error(5); return 0;}
  if (gfs->type!=3){  //uint8 type;//qb access method (1=RANDOM,2=BINARY,3=INPUT,4=OUTPUT)
    if (gfs_eof_passed(i)==1) return -1;
  }else{
    if (gfs_eof_reached(i)==1) return -1;
    if (gfs_eof_passed(i)==1) return -1;
  }
  return 0;

}

void sub_seek(int32 i,int64 pos){
  if (new_error) return;
  if (gfs_fileno_valid(i)!=1){error(52); return;}//Bad file name or number
  i=gfs_fileno[i];//convert fileno to gfs index
  static gfs_file_struct *gfs;
  gfs=&gfs_file[i];
  if (gfs->type==1){//RANDOM
    pos--;
    if (pos<0){error(63); return;}//Bad record number
    pos*=gfs->record_length;
    pos++;
  }
  pos--;
  if (pos<0){error(63); return;}//Bad record number
  int32 e;
  e=gfs_setpos(i,pos);
  if (e<0){
    if (e==-2){error(258); return;}//invalid handle
    if (e==-3){error(54); return;}//bad file mode
    if (e==-4){error(5); return;}//illegal function call
    error(75); return;//assume[-9]: path/file access error
  }
}

int64 func_seek(int32 i){
  if (gfs_fileno_valid(i)!=1){error(52); return 0;}//Bad file name or number
  i=gfs_fileno[i];//convert fileno to gfs index
  static gfs_file_struct *gfs;
  gfs=&gfs_file[i];
  if (gfs->scrn) return 0;
  if (gfs->type==1){//RANDOM
    return gfs_getpos(i)/gfs->record_length+1;
  }
  return gfs_getpos(i)+1;
}

int64 func_loc(int32 i){
  if (gfs_fileno_valid(i)!=1){error(52); return 0;}//Bad file name or number
  i=gfs_fileno[i];//convert fileno to gfs index
  static gfs_file_struct *gfs;
  gfs=&gfs_file[i];
  
  if (gfs->scrn){error(5); return 0;}
  if (gfs->com_port){
#ifdef QB64_WINDOWS
    static gfs_file_win_struct *f_w;
    f_w=&gfs_file_win[i];
    static COMSTAT c;
    ZeroMemory(&c,sizeof(COMSTAT));
    static DWORD ignore;
    if (!ClearCommError(f_w->file_handle,&ignore,&c)) return 0;
    return c.cbInQue;//bytes in COM input buffer
#endif
  }

  if (gfs->type==1){//RANDOM
    return gfs_getpos(i)/gfs->record_length+1;
  }
  if (gfs->type==2){//BINARY
    return gfs_getpos(i);
  }
  //APPEND/OUTPUT/INPUT
  int64 pos;
  pos=gfs_getpos(i);
  if (!pos) return 1;
  pos--;
  pos/=128;
  pos++;
  return pos;
}

qbs *func_input(int32 n,int32 i,int32 passed){
  if (new_error) return qbs_new(0,1);
  static qbs *str,*str2;
  static int32 x,c;
  if (n<0) str=qbs_new(0,1); else str=qbs_new(n,1);
  if (passed){

    if (gfs_fileno_valid(i)!=1){error(52); return str;}//Bad file name or number
    i=gfs_fileno[i];//convert fileno to gfs index
    static gfs_file_struct *gfs;
    gfs=&gfs_file[i];
    if ((gfs->type<2)||(gfs->type>3)){error(62); return str;}//Input past end of file
    //note: RANDOM should be supported
    //note: Unusually, QB returns "Input past end of file" instead of "Bad file mode"
    if (!gfs->read){error(75); return str;}//Path/file access error

    if (n<0){error(52); return str;}//Bad file name or number
    if (n==0) return str;

    //INPUT -> Input past end of file at EOF or CHR$(26)
    //         unlike BINARY, partial strings cannot be returned
    //         (use input_file_chr and modify it to support CHR$(26)
    if (gfs->type==3){
      x=0;
      do{
    c=file_input_chr(i);
    if (c==-1){error(62); return str;}//Input past end of file
    if (c==-2){error(75); return str;}//path/file access error
    str->chr[x]=c;

	if (gfs_file[i].eof_passed!=1) { //If we haven't declared the End of the File, check to see if the next byte is an EOF byte
		c=file_input_chr(i); //read the next byte
		if (gfs_file[i].eof_passed!=1) {gfs_setpos(i,gfs_getpos(i)-1);} //and if it's not EOF, move our position back to where it should be
	}

    x++;
      }while(x<n);
      return str;
    }

    //BINARY -> If past EOF, returns a NULL length string!
    //          or as much of the data that was valid as possible
    //          Seek POS is only advanced as far as was read!
    //          Binary can read past CHR$(26)
    //          (simply call C's read statement and manage via .eof
    if (gfs->type==2){
      static int32 e;
      e=gfs_read(i,-1,str->chr,n);
      if (e){
    if (e!=-10){//note: on eof, unread buffer area becomes NULL
      str->len=0;
      if (e==-2){error(258); return str;}//invalid handle
      if (e==-3){error(54); return str;}//bad file mode
      if (e==-4){error(5); return str;}//illegal function call
      if (e==-7){error(70); return str;}//permission denied
      error(75); return str;//assume[-9]: path/file access error
    }
      }
      str->len=gfs_read_bytes();//adjust if not enough data was available
      return str;
    }

    //RANDOM (todo)

  }else{
    //keyboard/piped
    //      For extended-two-byte character codes, only the first, CHR$(0), is returned and counts a 1 byte
    if (n<0){error(52); return str;}
    if (n==0) return str;
    x=0;
  waitforinput:
    str2=qbs_inkey();
    if (str2->len){
      str->chr[x]=str2->chr[0];
      x++;
    }
    qbs_free(str2);
    if (stop_program) return str;
    if (x<n){
        evnt(0);//check for new events
        Sleep(10);
        goto waitforinput;
    }
    return str;
  }
}

void file_line_input_string_character(int32 filehandle, qbs *deststr) {
  static qbs *str,*character;
  int32 c,nextc;
  int32 inspeechmarks;

  str=qbs_new(0,0);
  c=file_input_chr(filehandle); if (c==-2) return;
  if (c==-1){
    qbs_set(deststr,str);
    qbs_free(str);
    error(62);//input past end of file
    return;
  }
  character=qbs_new(1,0);
 nextchr:
  if (c==-1) goto gotstr;
  if (c==10) goto gotstr;
  if (c==13) goto gotstr;
  character->chr[0]=c; qbs_set(str,qbs_add(str,character));
  c=file_input_chr(filehandle); if (c==-2) return;
  goto nextchr;
 gotstr:
 nextstr:
  //scan until next item (or eof) reached
  if (c==-1) goto returnstr;
  if ((c==10)||(c==13)){//lf cr
    file_input_skip1310(filehandle,c);
    goto returnstr;
  }
  c=file_input_chr(filehandle); if (c==-2) return;
  goto nextstr;
  //return string
 returnstr:
  qbs_set(deststr,str);
  qbs_free(str);
  qbs_free(character);
  return;
}


void file_line_input_string_binary(int32 fileno, qbs *deststr) {
  int32 filebuf_size = 512;
  int32 filehandle;
  qbs *eol;

  filehandle=gfs_fileno[fileno];//convert fileno to gfs index
  eol = qbs_new_txt_len("\n", 1);

  int64 start_byte = func_seek(fileno);
  int64 filelength = func_lof(fileno);
   if (start_byte > filelength) {
    error(62);//input past end of file
        return;
  }
  qbs *buffer = qbs_new(filebuf_size, 0);
  qbs_set(deststr, qbs_new_txt_len("", 0));
    do {
      if (start_byte + filebuf_size > filelength) filebuf_size = filelength - start_byte + 1;
      qbs_set(buffer,func_space(qbr(filebuf_size))); 
          
          sub_get2(fileno, start_byte, buffer, 1);
      int32 eol_pos = func_instr(0, buffer, eol, 0);
      if (eol_pos == 0) {
                if ((start_byte + filebuf_size)>=filelength) {
            qbs_set(deststr, buffer);
                        gfs_setpos(filehandle,filelength); //set the position right before the EOF marker
                gfs_file[filehandle].eof_passed=1;//also set EOF flag;
                        qbs_free(buffer);
                return;
                }
        filebuf_size += 512;
          }
      else {
            qbs_set(deststr, qbs_add(deststr, qbs_left(buffer, eol_pos - 1)));
            break;
      }
    } while (!func_eof(fileno));
  qbs_free(buffer);
  if (start_byte + deststr->len + 2 >= filelength) { //if we've read to the end of the line
          gfs_setpos(filehandle,filelength); //set the position right before the EOF marker
          gfs_file[filehandle].eof_passed=1;//also set EOF flag;
          if (deststr->chr[deststr->len - 1] == '\r') qbs_set(deststr, qbs_left(deststr, deststr->len-1));
          return;
  }
  gfs_setpos(filehandle,start_byte + deststr->len); //set the position at the end of the text
  if (deststr->chr[deststr->len - 1] == '\r') qbs_set(deststr, qbs_left(deststr, deststr->len-1));
}


void sub_file_line_input_string(int32 fileno, qbs *deststr){
  int32 filehandle;
  if (new_error) return;
  if (gfs_fileno_valid(fileno)!=1){error(52); return;}//Bad file name or number
  filehandle=gfs_fileno[fileno];//convert fileno to gfs index
  static gfs_file_struct *gfs;
  gfs=&gfs_file[filehandle];
  if (!gfs->read){error(75); return;}//Path/file access error

  if (gfs->type == 2) {
    file_line_input_string_binary(fileno, deststr);
  }
  else if (gfs->type == 3) {
    file_line_input_string_character(filehandle, deststr);
  } else {
    error(54); //Bad file mode
  }
  return;
}

double func_sqr(double value){
  if (value<0){error(5); return 0;}
  return sqrt(value);
}





#ifdef QB64_BACKSLASH_FILESYSTEM
#include "parts\\audio\\out\\src.c"
#else
#include "parts/audio/out/src.c"
#endif

#ifdef QB64_BACKSLASH_FILESYSTEM
#include "parts\\audio\\conversion\\src.c"
#include "parts\\audio\\decode\\src.c"
#else
#include "parts/audio/conversion/src.c"
#include "parts/audio/decode/src.c"
#endif









qbs *func_command_str=NULL;
char **func_command_array = NULL;
int32 func_command_count = 0;

qbs *func_command(int32 index, int32 passed){
  static qbs *tqbs;
  if (passed) { //Get specific parameter
    //If out of bounds or error getting cmdline args (on Android, perhaps), return empty string.
    if (index >= func_command_count || index < 0 || func_command_array==NULL) {tqbs = qbs_new(0, 1); return tqbs;}
    int len = strlen(func_command_array[index]);
    //Create new temp qbs and copy data into it.
    tqbs=qbs_new(len, 1);
    memcpy(tqbs->chr, func_command_array[index], len);
  }
  else { //Legacy support; return whole commandline
    tqbs=qbs_new(func_command_str->len,1);
    memcpy(tqbs->chr,func_command_str->chr,func_command_str->len);
  }
  return tqbs;
}

int32 func__commandcount(){
  return func_command_count - 1;
}

int32 shell_call_in_progress=0;

#ifdef QB64_WINDOWS
int32 cmd_available=-1;
int32 cmd_ok(){
  if (cmd_available==-1){
    static STARTUPINFO si;
    static PROCESS_INFORMATION pi;
    ZeroMemory( &si, sizeof(si) ); si.cb = sizeof(si); ZeroMemory( &pi, sizeof(pi) );
    if(
       CreateProcess(
             NULL,           // No module name (use command line)
             "cmd.exe /c ver",// Command line
             NULL,           // Process handle not inheritable
             NULL,           // Thread handle not inheritable
             FALSE,          // Set handle inheritance to FALSE
             CREATE_NO_WINDOW, // No creation flags
             NULL,           // Use parent's environment block
             NULL,           // Use parent's starting directory 
             &si,            // Pointer to STARTUPINFO structure
             &pi             // Pointer to PROCESS_INFORMATION structure
             )
       ){
      WaitForSingleObject(pi.hProcess,INFINITE);
      CloseHandle(pi.hProcess);
      CloseHandle(pi.hThread);
      cmd_available=1;
    }else{
      cmd_available=0;
    }
  }//-1
  return cmd_available;
}
#endif






int32 cmd_command(qbs *str2){
  static qbs *str=NULL;
  static int32 s;
  if (!str) str=qbs_new(0,0);
  qbs_set(str,qbs_ucase(str2));
  s=0;
  if (qbs_equal(str,qbs_new_txt("ASSOC")))s=1;
  if (qbs_equal(str,qbs_new_txt("BREAK")))s=1;
  if (qbs_equal(str,qbs_new_txt("BCDBOOT")))s=1;
  if (qbs_equal(str,qbs_new_txt("BCDEDIT")))s=1;
  if (qbs_equal(str,qbs_new_txt("CALL")))s=1;
  if (qbs_equal(str,qbs_new_txt("CD")))s=1;
  if (qbs_equal(str,qbs_new_txt("CHDIR")))s=1;
  if (qbs_equal(str,qbs_new_txt("CLS")))s=1;
  if (qbs_equal(str,qbs_new_txt("COLOR")))s=1;
  if (qbs_equal(str,qbs_new_txt("COPY")))s=1;
  if (qbs_equal(str,qbs_new_txt("DATE")))s=1;
  if (qbs_equal(str,qbs_new_txt("DEFRAG")))s=1;
  if (qbs_equal(str,qbs_new_txt("DEL")))s=1;
  if (qbs_equal(str,qbs_new_txt("DIR")))s=1;
  if (qbs_equal(str,qbs_new_txt("ECHO")))s=1;
  if (qbs_equal(str,qbs_new_txt("ENDLOCAL")))s=1;
  if (qbs_equal(str,qbs_new_txt("ERASE")))s=1;
  if (qbs_equal(str,qbs_new_txt("FOR")))s=1;
  if (qbs_equal(str,qbs_new_txt("FTYPE")))s=1;
  if (qbs_equal(str,qbs_new_txt("GOTO")))s=1;
  if (qbs_equal(str,qbs_new_txt("GRAFTABL")))s=1;
  if (qbs_equal(str,qbs_new_txt("IF")))s=1;
  if (qbs_equal(str,qbs_new_txt("MD")))s=1;
  if (qbs_equal(str,qbs_new_txt("MKDIR")))s=1;
  if (qbs_equal(str,qbs_new_txt("MKLINK")))s=1;
  if (qbs_equal(str,qbs_new_txt("MOVE")))s=1;
  if (qbs_equal(str,qbs_new_txt("PATH")))s=1;
  if (qbs_equal(str,qbs_new_txt("PAUSE")))s=1;
  if (qbs_equal(str,qbs_new_txt("POPD")))s=1;
  if (qbs_equal(str,qbs_new_txt("PROMPT")))s=1;
  if (qbs_equal(str,qbs_new_txt("PUSHD")))s=1;
  if (qbs_equal(str,qbs_new_txt("RD")))s=1;
  if (qbs_equal(str,qbs_new_txt("REM")))s=1;
  if (qbs_equal(str,qbs_new_txt("REN")))s=1;
  if (qbs_equal(str,qbs_new_txt("RENAME")))s=1;
  if (qbs_equal(str,qbs_new_txt("RMDIR")))s=1;
  if (qbs_equal(str,qbs_new_txt("SET")))s=1;
  if (qbs_equal(str,qbs_new_txt("SETLOCAL")))s=1;
  if (qbs_equal(str,qbs_new_txt("SHIFT")))s=1;
  if (qbs_equal(str,qbs_new_txt("START")))s=1;
  if (qbs_equal(str,qbs_new_txt("TIME")))s=1;
  if (qbs_equal(str,qbs_new_txt("TITLE")))s=1;
  if (qbs_equal(str,qbs_new_txt("TYPE")))s=1;
  if (qbs_equal(str,qbs_new_txt("VER")))s=1;
  if (qbs_equal(str,qbs_new_txt("VERIFY")))s=1;
  if (qbs_equal(str,qbs_new_txt("VOL")))s=1;
  return s;
}

int64 func_shell(qbs *str){
  if (new_error) return 1;
  if (cloud_app){error(262); return 1;}

  int64 return_code;

  //exit full screen mode if necessary
  static int32 full_screen_mode;
  full_screen_mode=full_screen;
  if (full_screen_mode){
    full_screen_set=0;
    do{
      Sleep(0);
    }while(full_screen);
  }//full_screen_mode
  static qbs *strz=NULL;
  static qbs *str1=NULL;
  static qbs *str1z=NULL;
  static qbs *str2=NULL;
  static qbs *str2z=NULL;
  static int32 i;

  static int32 use_console;
  use_console=0;
  if (console){
    if (console_active){
      use_console=1;
    }
  }

  if (!strz) strz=qbs_new(0,0);
  if (!str1) str1=qbs_new(0,0);
  if (!str1z) str1z=qbs_new(0,0);
  if (!str2) str2=qbs_new(0,0);
  if (!str2z) str2z=qbs_new(0,0);

  if (str->len){

#ifdef QB64_WINDOWS


    if (use_console){
      qbs_set(strz,qbs_add(str,qbs_new_txt_len("\0",1)));
      shell_call_in_progress=1;
      /*
    freopen("stdout.buf", "w", stdout);
    freopen("stderr.buf", "w", stderr);
      */
      return_code=system((char*)strz->chr);
      /*
    freopen("CON", "w", stdout);
    freopen("CON", "w", stderr);
    static char buf[1024];
    static int buflen;
    static int fd;
    fd = open("stdout.buf", O_RDONLY);
    while((buflen = read(fd, buf, 1024)) > 0)
    {
    write(1, buf, buflen);
    }
    close(fd);
    fd = open("stderr.buf", O_RDONLY);
    while((buflen = read(fd, buf, 1024)) > 0)
    {
    write(1, buf, buflen);
    }
    close(fd);
    remove("stdout.buf");
    remove("stderr.buf");
      */
      shell_call_in_progress=0;
      goto shell_complete;
    }

    static STARTUPINFO si;
    static PROCESS_INFORMATION pi;

    if (cmd_ok()){

      static SHELLEXECUTEINFO shi;
      static char cmd[10]="cmd\0";

      //attempt to separate file name (if any) from parameters
      static int32 x,quotes;

      qbs_set(str1,str);
      qbs_set(str2,qbs_new_txt(""));
      if (!str1->len) goto shell_complete;//failed!

      if (!cmd_command(str1)){
    //try directly, as is
    qbs_set(str1z,qbs_add(str1,qbs_new_txt_len("\0",1)));
    ZeroMemory( &shi, sizeof(shi) );
    shi.cbSize = sizeof(shi);
    shi.lpFile=(char*)&str1z->chr[0];
    shi.lpParameters=NULL;
    shi.fMask=SEE_MASK_NOCLOSEPROCESS|SEE_MASK_FLAG_NO_UI;
    shi.nShow=SW_SHOW;
    if(ShellExecuteEx(&shi)){
      shell_call_in_progress=1;
      // Wait until child process exits.
      WaitForSingleObject( shi.hProcess, INFINITE );
      GetExitCodeProcess(shi.hProcess,(DWORD*)&return_code);
      CloseHandle(shi.hProcess);
      shell_call_in_progress=0;
      goto shell_complete;
    }
      }

      x=0;
      quotes=0;
      while (x<str1->len){
    if (str1->chr[x]==34){
      if (!quotes) quotes=1; else quotes=0;
    }
    if (str1->chr[x]==32){
      if (quotes==0){
        qbs_set(str2,qbs_right(str1,str1->len-x-1));
        qbs_set(str1,qbs_left(str1,x));
        goto split;
      }
    }
    x++;
      }
    split:
      if (!str1->len) goto shell_complete;//failed!

      if (str2->len){ if (!cmd_command(str1)){
      qbs_set(str1z,qbs_add(str1,qbs_new_txt_len("\0",1)));
      qbs_set(str2z,qbs_add(str2,qbs_new_txt_len("\0",1)));
      ZeroMemory( &shi, sizeof(shi) );
      shi.cbSize = sizeof(shi);
      shi.lpFile=(char*)&str1z->chr[0];
      shi.lpParameters=(char*)&str2z->chr[0];
      //if (str2->len<=1) shi.lpParameters=NULL;
      shi.fMask=SEE_MASK_NOCLOSEPROCESS|SEE_MASK_FLAG_NO_UI;
      shi.nShow=SW_SHOW;
      if(ShellExecuteEx(&shi)){
        shell_call_in_progress=1;
        // Wait until child process exits.
        WaitForSingleObject( shi.hProcess, INFINITE );
        GetExitCodeProcess(shi.hProcess,(DWORD*)&return_code);
        CloseHandle(shi.hProcess);
        shell_call_in_progress=0;
        goto shell_complete;
      }
    }}

      //failed, try cmd /c method...
      if (str2->len) qbs_set(str2,qbs_add(qbs_new_txt(" "),str2));
      qbs_set(strz,qbs_add(str1,str2));
      qbs_set(strz,qbs_add(qbs_new_txt(" /c "),strz));
      qbs_set(strz,qbs_add(strz,qbs_new_txt_len("\0",1)));
      ZeroMemory( &shi, sizeof(shi) );
      shi.cbSize = sizeof(shi);
      shi.lpFile=&cmd[0];
      shi.lpParameters=(char*)&strz->chr[0];
      shi.fMask=SEE_MASK_NOCLOSEPROCESS|SEE_MASK_FLAG_NO_UI;
      shi.nShow=SW_SHOW;
      if(ShellExecuteEx(&shi)){
    shell_call_in_progress=1;
    // Wait until child process exits.
    WaitForSingleObject( shi.hProcess, INFINITE );
    GetExitCodeProcess(shi.hProcess,(DWORD*)&return_code);
    CloseHandle(shi.hProcess);
    shell_call_in_progress=0;
    goto shell_complete;
      }

      /*
    qbs_set(strz,qbs_add(qbs_new_txt("cmd.exe /c "),str));
    qbs_set(strz,qbs_add(strz,qbs_new_txt_len("\0",1)));
    ZeroMemory( &si, sizeof(si) ); si.cb = sizeof(si); ZeroMemory( &pi, sizeof(pi) );
    if(CreateProcess(
        NULL,           // No module name (use command line)
        (char*)&strz->chr[0], // Command line
        NULL,           // Process handle not inheritable
        NULL,           // Thread handle not inheritable
        FALSE,          // Set handle inheritance to FALSE
        DETACHED_PROCESS, // No creation flags
        NULL,           // Use parent's environment block
        NULL,           // Use parent's starting directory 
        &si,            // Pointer to STARTUPINFO structure
        &pi )           // Pointer to PROCESS_INFORMATION structure
    ){
    shell_call_in_progress=1;
    // Wait until child process exits.
    WaitForSingleObject( pi.hProcess, INFINITE );
    // Close process and thread handles. 
    CloseHandle( pi.hProcess );
    CloseHandle( pi.hThread );
    shell_call_in_progress=0;
    goto shell_complete;
    }
      */

      return_code=1;
      goto shell_complete;//failed

    }else{

      qbs_set(strz,qbs_add(qbs_new_txt("command.com /c "),str));
      qbs_set(strz,qbs_add(strz,qbs_new_txt_len("\0",1)));
      ZeroMemory( &si, sizeof(si) ); si.cb = sizeof(si); ZeroMemory( &pi, sizeof(pi) );
      if(CreateProcess(
               NULL,           // No module name (use command line)
               (char*)&strz->chr[0], // Command line
               NULL,           // Process handle not inheritable
               NULL,           // Thread handle not inheritable
               FALSE,          // Set handle inheritance to FALSE
               CREATE_NEW_CONSOLE, // No creation flags
               NULL,           // Use parent's environment block
               NULL,           // Use parent's starting directory 
               &si,            // Pointer to STARTUPINFO structure
               &pi )           // Pointer to PROCESS_INFORMATION structure
     ){
    shell_call_in_progress=1;
    // Wait until child process exits.
    WaitForSingleObject( pi.hProcess, INFINITE );
    // Close process and thread handles. 
    CloseHandle( pi.hProcess );
    CloseHandle( pi.hThread );
    shell_call_in_progress=0;
    goto shell_complete;
      }
      goto shell_complete;//failed

    }//cmd_ok()

#else

    qbs_set(strz,qbs_add(str,qbs_new_txt_len("\0",1)));
    shell_call_in_progress=1;
    return_code = system((char*)strz->chr);
    shell_call_in_progress=0;
    if (return_code == -1) {/* do nothing */}
    else {
      return_code = WEXITSTATUS(return_code);
    }

#endif

  }else{

    //SHELL (with no parameters)
    //note: opening a new shell is only available in windows atm via cmd
    //note: assumes cmd available
#ifdef QB64_WINDOWS
    if (!use_console) AllocConsole();
    qbs_set(strz,qbs_new_txt_len("cmd\0",4));
    shell_call_in_progress=1;
    return_code = system((char*)strz->chr);
    shell_call_in_progress=0;
    if (!use_console) FreeConsole();
    goto shell_complete;
#endif

  }

 shell_complete:
  //reenter full screen mode if necessary
  if (full_screen_mode){
    full_screen_set=full_screen_mode;
    do{
      Sleep(0);
    }while(!full_screen);
  }//full_screen_mode

  return return_code;
}//func SHELL(...





int64 func__shellhide(qbs *str){ //func _SHELLHIDE(...
  if (new_error) return 1;
  if (cloud_app){error(262); return 1;}

  static int64 return_code;
  return_code=0;

  static qbs *strz=NULL;
  static int32 i;
  if (!strz) strz=qbs_new(0,0);
  if (!str->len){error(5); return 1;}//cannot launch a hidden console

  static qbs *str1=NULL;
  static qbs *str2=NULL;
  static qbs *str1z=NULL;
  static qbs *str2z=NULL;
  if (!str1) str1=qbs_new(0,0);
  if (!str2) str2=qbs_new(0,0);
  if (!str1z) str1z=qbs_new(0,0);
  if (!str2z) str2z=qbs_new(0,0);

#ifdef QB64_WINDOWS

  static STARTUPINFO si;
  static PROCESS_INFORMATION pi;

  if (cmd_ok()){

    static SHELLEXECUTEINFO shi;
    static char cmd[10]="cmd\0";

    //attempt to separate file name (if any) from parameters
    static int32 x,quotes;

    qbs_set(str1,str);
    qbs_set(str2,qbs_new_txt(""));

    if (!cmd_command(str1)){
      //try directly, as is
      qbs_set(str1z,qbs_add(str1,qbs_new_txt_len("\0",1)));
      ZeroMemory( &shi, sizeof(shi) );
      shi.cbSize = sizeof(shi);
      shi.lpFile=(char*)&str1z->chr[0];
      shi.lpParameters=NULL;
      shi.fMask=SEE_MASK_NOCLOSEPROCESS|SEE_MASK_FLAG_NO_UI;
      shi.nShow=SW_HIDE;
      if(ShellExecuteEx(&shi)){
    shell_call_in_progress=1;
    // Wait until child process exits.
    WaitForSingleObject( shi.hProcess, INFINITE );
    GetExitCodeProcess(shi.hProcess,(DWORD*)&return_code);
    CloseHandle(shi.hProcess);
    shell_call_in_progress=0;
    goto shell_complete;
      }
    }

    x=0;
    quotes=0;
    while (x<str1->len){
      if (str1->chr[x]==34){
    if (!quotes) quotes=1; else quotes=0;
      }
      if (str1->chr[x]==32){
    if (quotes==0){
      qbs_set(str2,qbs_right(str1,str1->len-x-1));
      qbs_set(str1,qbs_left(str1,x));
      goto split;
    }
      }
      x++;
    }
  split:
    if (!str1->len) {return_code=1; goto shell_complete;}//failed!

    if (str2->len){ if (!cmd_command(str1)){
    qbs_set(str1z,qbs_add(str1,qbs_new_txt_len("\0",1)));
    qbs_set(str2z,qbs_add(str2,qbs_new_txt_len("\0",1)));
    ZeroMemory( &shi, sizeof(shi) );
    shi.cbSize = sizeof(shi);
    shi.lpFile=(char*)&str1z->chr[0];
    shi.lpParameters=(char*)&str2z->chr[0];
    //if (str2->len<=1) shi.lpParameters=NULL;
    shi.fMask=SEE_MASK_NOCLOSEPROCESS|SEE_MASK_FLAG_NO_UI;
    shi.nShow=SW_HIDE;
    if(ShellExecuteEx(&shi)){
      shell_call_in_progress=1;
      // Wait until child process exits.
      WaitForSingleObject( shi.hProcess, INFINITE );
      GetExitCodeProcess(shi.hProcess,(DWORD*)&return_code);
      CloseHandle(shi.hProcess);
      shell_call_in_progress=0;
      goto shell_complete;
    }
      }}

    //failed, try cmd /c method...
    if (str2->len) qbs_set(str2,qbs_add(qbs_new_txt(" "),str2));
    qbs_set(strz,qbs_add(str1,str2));
    qbs_set(strz,qbs_add(qbs_new_txt(" /c "),strz));
    qbs_set(strz,qbs_add(strz,qbs_new_txt_len("\0",1)));
    ZeroMemory( &shi, sizeof(shi) );
    shi.cbSize = sizeof(shi);
    shi.lpFile=&cmd[0];
    shi.lpParameters=(char*)&strz->chr[0];
    shi.fMask=SEE_MASK_NOCLOSEPROCESS|SEE_MASK_FLAG_NO_UI;
    shi.nShow=SW_HIDE;
    if(ShellExecuteEx(&shi)){
      shell_call_in_progress=1;
      // Wait until child process exits.
      WaitForSingleObject( shi.hProcess, INFINITE );
      GetExitCodeProcess(shi.hProcess,(DWORD*)&return_code);
      CloseHandle(shi.hProcess);
      shell_call_in_progress=0;
      goto shell_complete;
    }

    /*
      qbs_set(strz,qbs_add(qbs_new_txt("cmd.exe /c "),str));
      qbs_set(strz,qbs_add(strz,qbs_new_txt_len("\0",1)));
      ZeroMemory( &si, sizeof(si) ); si.cb = sizeof(si); ZeroMemory( &pi, sizeof(pi) );
      if(CreateProcess(
      NULL,           // No module name (use command line)
      (char*)&strz->chr[0], // Command line
      NULL,           // Process handle not inheritable
      NULL,           // Thread handle not inheritable
      FALSE,          // Set handle inheritance to FALSE
      CREATE_NO_WINDOW, // No creation flags
      NULL,           // Use parent's environment block
      NULL,           // Use parent's starting directory 
      &si,            // Pointer to STARTUPINFO structure
      &pi )           // Pointer to PROCESS_INFORMATION structure
      ){
      shell_call_in_progress=1;
      // Wait until child process exits.
      WaitForSingleObject( pi.hProcess, INFINITE );
      // Close process and thread handles. 
      CloseHandle( pi.hProcess );
      CloseHandle( pi.hThread );
      shell_call_in_progress=0;
      goto shell_complete;
      }
    */

    return_code=1;
    goto shell_complete;//failed

  }else{

    qbs_set(strz,qbs_add(qbs_new_txt("command.com /c "),str));
    qbs_set(strz,qbs_add(strz,qbs_new_txt_len("\0",1)));
    ZeroMemory( &si, sizeof(si) ); si.cb = sizeof(si); ZeroMemory( &pi, sizeof(pi) );
    if(CreateProcess(

             NULL,           // No module name (use command line)
             (char*)&strz->chr[0], // Command line
             NULL,           // Process handle not inheritable
             NULL,           // Thread handle not inheritable
             FALSE,          // Set handle inheritance to FALSE
             CREATE_NEW_CONSOLE, //note: cannot hide new console, but can preserve existing one
             NULL,           // Use parent's environment block
             NULL,           // Use parent's starting directory 
             &si,            // Pointer to STARTUPINFO structure
             &pi )           // Pointer to PROCESS_INFORMATION structure
       ){
      shell_call_in_progress=1;
      // Wait until child process exits.
      WaitForSingleObject( pi.hProcess, INFINITE );
      // Close process and thread handles. 
      CloseHandle( pi.hProcess );
      CloseHandle( pi.hThread );
      shell_call_in_progress=0;
      goto shell_complete;
    }
    goto shell_complete;//failed

  }//cmd_ok()

#else

  qbs_set(strz,qbs_add(str,qbs_new_txt_len("\0",1)));
  shell_call_in_progress=1;
  return_code=system((char*)strz->chr);
  shell_call_in_progress=0;
  return return_code;

#endif

 shell_complete:;

  return return_code;
}//func _SHELLHIDE(...



































void sub_shell(qbs *str,int32 passed){
  if (new_error) return;
  if (cloud_app){error(262); return;}

  //exit full screen mode if necessary
  static int32 full_screen_mode;
  full_screen_mode=full_screen;
  if (full_screen_mode){
    full_screen_set=0;
    do{
      Sleep(0);
    }while(full_screen);
  }//full_screen_mode
  static qbs *strz=NULL;
  static qbs *str1=NULL;
  static qbs *str1z=NULL;
  static qbs *str2=NULL;
  static qbs *str2z=NULL;
  static int32 i;

  static int32 use_console;
  use_console=0;
  if (console){
    if (console_active){
      use_console=1;
    }
  }

  if (!strz) strz=qbs_new(0,0);
  if (!str1) str1=qbs_new(0,0);
  if (!str1z) str1z=qbs_new(0,0);
  if (!str2) str2=qbs_new(0,0);
  if (!str2z) str2z=qbs_new(0,0);

  if (passed) if (str->len==0) passed=0;//"" means launch a CONSOLE
  if (passed){



#ifdef QB64_WINDOWS


    if (use_console){
      qbs_set(strz,qbs_add(str,qbs_new_txt_len("\0",1)));
      shell_call_in_progress=1;
      /*
    freopen("stdout.buf", "w", stdout);
    freopen("stderr.buf", "w", stderr);
      */
      system((char*)strz->chr);
      /*
    freopen("CON", "w", stdout);
    freopen("CON", "w", stderr);
    static char buf[1024];
    static int buflen;
    static int fd;
    fd = open("stdout.buf", O_RDONLY);
    while((buflen = read(fd, buf, 1024)) > 0)
    {
    write(1, buf, buflen);
    }
    close(fd);
    fd = open("stderr.buf", O_RDONLY);
    while((buflen = read(fd, buf, 1024)) > 0)
    {
    write(1, buf, buflen);
    }
    close(fd);
    remove("stdout.buf");
    remove("stderr.buf");
      */
      shell_call_in_progress=0;
      goto shell_complete;
    }



    static STARTUPINFO si;
    static PROCESS_INFORMATION pi;

    if (cmd_ok()){

      static SHELLEXECUTEINFO shi;
      static char cmd[10]="cmd\0";

      //attempt to separate file name (if any) from parameters
      static int32 x,quotes;

      qbs_set(str1,str);
      qbs_set(str2,qbs_new_txt(""));
      if (!str1->len) goto shell_complete;//failed!

      if (!cmd_command(str1)){
    //try directly, as is
    qbs_set(str1z,qbs_add(str1,qbs_new_txt_len("\0",1)));
    ZeroMemory( &shi, sizeof(shi) );
    shi.cbSize = sizeof(shi);
    shi.lpFile=(char*)&str1z->chr[0];
    shi.lpParameters=NULL;
    shi.fMask=SEE_MASK_NOCLOSEPROCESS|SEE_MASK_FLAG_NO_UI;
    shi.nShow=SW_SHOW;
    if(ShellExecuteEx(&shi)){
      shell_call_in_progress=1;
      // Wait until child process exits.
      WaitForSingleObject( shi.hProcess, INFINITE );
      CloseHandle(shi.hProcess);
      shell_call_in_progress=0;
      goto shell_complete;
    }
      }

      x=0;
      quotes=0;
      while (x<str1->len){
    if (str1->chr[x]==34){
      if (!quotes) quotes=1; else quotes=0;
    }
    if (str1->chr[x]==32){
      if (quotes==0){
        qbs_set(str2,qbs_right(str1,str1->len-x-1));
        qbs_set(str1,qbs_left(str1,x));
        goto split;
      }
    }
    x++;
      }
    split:
      if (!str1->len) goto shell_complete;//failed!

      if (str2->len){ if (!cmd_command(str1)){
      qbs_set(str1z,qbs_add(str1,qbs_new_txt_len("\0",1)));
      qbs_set(str2z,qbs_add(str2,qbs_new_txt_len("\0",1)));
      ZeroMemory( &shi, sizeof(shi) );
      shi.cbSize = sizeof(shi);
      shi.lpFile=(char*)&str1z->chr[0];
      shi.lpParameters=(char*)&str2z->chr[0];
      //if (str2->len<=1) shi.lpParameters=NULL;
      shi.fMask=SEE_MASK_NOCLOSEPROCESS|SEE_MASK_FLAG_NO_UI;
      shi.nShow=SW_SHOW;
      if(ShellExecuteEx(&shi)){
        shell_call_in_progress=1;
        // Wait until child process exits.
        WaitForSingleObject( shi.hProcess, INFINITE );
        CloseHandle(shi.hProcess);
        shell_call_in_progress=0;
        goto shell_complete;
      }
    }}

      //failed, try cmd /c method...
      if (str2->len) qbs_set(str2,qbs_add(qbs_new_txt(" "),str2));
      qbs_set(strz,qbs_add(str1,str2));
      qbs_set(strz,qbs_add(qbs_new_txt(" /c "),strz));
      qbs_set(strz,qbs_add(strz,qbs_new_txt_len("\0",1)));
      ZeroMemory( &shi, sizeof(shi) );
      shi.cbSize = sizeof(shi);
      shi.lpFile=&cmd[0];
      shi.lpParameters=(char*)&strz->chr[0];
      shi.fMask=SEE_MASK_NOCLOSEPROCESS|SEE_MASK_FLAG_NO_UI;
      shi.nShow=SW_SHOW;
      if(ShellExecuteEx(&shi)){
    shell_call_in_progress=1;
    // Wait until child process exits.
    WaitForSingleObject( shi.hProcess, INFINITE );
    CloseHandle(shi.hProcess);
    shell_call_in_progress=0;
    goto shell_complete;
      }

      /*
    qbs_set(strz,qbs_add(qbs_new_txt("cmd.exe /c "),str));
    qbs_set(strz,qbs_add(strz,qbs_new_txt_len("\0",1)));
    ZeroMemory( &si, sizeof(si) ); si.cb = sizeof(si); ZeroMemory( &pi, sizeof(pi) );
    if(CreateProcess(
        NULL,           // No module name (use command line)
        (char*)&strz->chr[0], // Command line
        NULL,           // Process handle not inheritable
        NULL,           // Thread handle not inheritable
        FALSE,          // Set handle inheritance to FALSE
        DETACHED_PROCESS, // No creation flags
        NULL,           // Use parent's environment block
        NULL,           // Use parent's starting directory 
        &si,            // Pointer to STARTUPINFO structure
        &pi )           // Pointer to PROCESS_INFORMATION structure
    ){
    shell_call_in_progress=1;
    // Wait until child process exits.
    WaitForSingleObject( pi.hProcess, INFINITE );
    // Close process and thread handles. 
    CloseHandle( pi.hProcess );
    CloseHandle( pi.hThread );
    shell_call_in_progress=0;
    goto shell_complete;
    }
      */

      goto shell_complete;//failed

    }else{

      qbs_set(strz,qbs_add(qbs_new_txt("command.com /c "),str));
      qbs_set(strz,qbs_add(strz,qbs_new_txt_len("\0",1)));
      ZeroMemory( &si, sizeof(si) ); si.cb = sizeof(si); ZeroMemory( &pi, sizeof(pi) );
      if(CreateProcess(
               NULL,           // No module name (use command line)
               (char*)&strz->chr[0], // Command line
               NULL,           // Process handle not inheritable
               NULL,           // Thread handle not inheritable
               FALSE,          // Set handle inheritance to FALSE
               CREATE_NEW_CONSOLE, // No creation flags
               NULL,           // Use parent's environment block
               NULL,           // Use parent's starting directory 
               &si,            // Pointer to STARTUPINFO structure
               &pi )           // Pointer to PROCESS_INFORMATION structure
     ){
    shell_call_in_progress=1;
    // Wait until child process exits.
    WaitForSingleObject( pi.hProcess, INFINITE );
    // Close process and thread handles. 
    CloseHandle( pi.hProcess );
    CloseHandle( pi.hThread );
    shell_call_in_progress=0;
    goto shell_complete;
      }
      goto shell_complete;//failed

    }//cmd_ok()

#else

    qbs_set(strz,qbs_add(str,qbs_new_txt_len("\0",1)));
    shell_call_in_progress=1;
    system((char*)strz->chr);
    shell_call_in_progress=0;

#endif

  }else{

    //SHELL (with no parameters)
    //note: opening a new shell is only available in windows atm via cmd
    //note: assumes cmd available
#ifdef QB64_WINDOWS
    if (!use_console) AllocConsole();
    qbs_set(strz,qbs_new_txt_len("cmd\0",4));
    shell_call_in_progress=1;
    system((char*)strz->chr);
    shell_call_in_progress=0;
    if (!use_console) FreeConsole();
    goto shell_complete;
#endif

  }

 shell_complete:
  //reenter full screen mode if necessary
  if (full_screen_mode){
    full_screen_set=full_screen_mode;
    do{
      Sleep(0);
    }while(!full_screen);
  }//full_screen_mode
}















void sub_shell2(qbs *str,int32 passed){ //HIDE
  if (new_error) return;
  if (cloud_app){error(262); return;}

  if (passed&1){sub_shell4(str,passed&2); return;}
  if (!(passed&2)){error(5); return;}//should not hide a shell waiting for input

  static qbs *strz=NULL;
  static int32 i;
  if (!strz) strz=qbs_new(0,0);
  if (!str->len){error(5); return;}//cannot launch a hidden console

  static qbs *str1=NULL;
  static qbs *str2=NULL;
  static qbs *str1z=NULL;
  static qbs *str2z=NULL;
  if (!str1) str1=qbs_new(0,0);
  if (!str2) str2=qbs_new(0,0);
  if (!str1z) str1z=qbs_new(0,0);
  if (!str2z) str2z=qbs_new(0,0);

#ifdef QB64_WINDOWS

  static STARTUPINFO si;
  static PROCESS_INFORMATION pi;

  if (cmd_ok()){

    static SHELLEXECUTEINFO shi;
    static char cmd[10]="cmd\0";

    //attempt to separate file name (if any) from parameters
    static int32 x,quotes;

    qbs_set(str1,str);
    qbs_set(str2,qbs_new_txt(""));

    if (!cmd_command(str1)){
      //try directly, as is
      qbs_set(str1z,qbs_add(str1,qbs_new_txt_len("\0",1)));
      ZeroMemory( &shi, sizeof(shi) );
      shi.cbSize = sizeof(shi);
      shi.lpFile=(char*)&str1z->chr[0];
      shi.lpParameters=NULL;
      shi.fMask=SEE_MASK_NOCLOSEPROCESS|SEE_MASK_FLAG_NO_UI;
      shi.nShow=SW_HIDE;
      if(ShellExecuteEx(&shi)){
    shell_call_in_progress=1;
    // Wait until child process exits.
    WaitForSingleObject( shi.hProcess, INFINITE );
    CloseHandle(shi.hProcess);
    shell_call_in_progress=0;
    goto shell_complete;
      }
    }

    x=0;
    quotes=0;
    while (x<str1->len){
      if (str1->chr[x]==34){
    if (!quotes) quotes=1; else quotes=0;
      }
      if (str1->chr[x]==32){
    if (quotes==0){
      qbs_set(str2,qbs_right(str1,str1->len-x-1));
      qbs_set(str1,qbs_left(str1,x));
      goto split;
    }
      }
      x++;
    }
  split:
    if (!str1->len) goto shell_complete;//failed!

    if (str2->len){ if (!cmd_command(str1)){
    qbs_set(str1z,qbs_add(str1,qbs_new_txt_len("\0",1)));
    qbs_set(str2z,qbs_add(str2,qbs_new_txt_len("\0",1)));
    ZeroMemory( &shi, sizeof(shi) );
    shi.cbSize = sizeof(shi);
    shi.lpFile=(char*)&str1z->chr[0];
    shi.lpParameters=(char*)&str2z->chr[0];
    //if (str2->len<=1) shi.lpParameters=NULL;
    shi.fMask=SEE_MASK_NOCLOSEPROCESS|SEE_MASK_FLAG_NO_UI;
    shi.nShow=SW_HIDE;
    if(ShellExecuteEx(&shi)){
      shell_call_in_progress=1;
      // Wait until child process exits.
      WaitForSingleObject( shi.hProcess, INFINITE );
      CloseHandle(shi.hProcess);
      shell_call_in_progress=0;
      goto shell_complete;
    }
      }}

    //failed, try cmd /c method...
    if (str2->len) qbs_set(str2,qbs_add(qbs_new_txt(" "),str2));
    qbs_set(strz,qbs_add(str1,str2));
    qbs_set(strz,qbs_add(qbs_new_txt(" /c "),strz));
    qbs_set(strz,qbs_add(strz,qbs_new_txt_len("\0",1)));
    ZeroMemory( &shi, sizeof(shi) );
    shi.cbSize = sizeof(shi);
    shi.lpFile=&cmd[0];
    shi.lpParameters=(char*)&strz->chr[0];
    shi.fMask=SEE_MASK_NOCLOSEPROCESS|SEE_MASK_FLAG_NO_UI;
    shi.nShow=SW_HIDE;
    if(ShellExecuteEx(&shi)){
      shell_call_in_progress=1;
      // Wait until child process exits.
      WaitForSingleObject( shi.hProcess, INFINITE );
      CloseHandle(shi.hProcess);
      shell_call_in_progress=0;
      goto shell_complete;
    }

    /*
      qbs_set(strz,qbs_add(qbs_new_txt("cmd.exe /c "),str));
      qbs_set(strz,qbs_add(strz,qbs_new_txt_len("\0",1)));
      ZeroMemory( &si, sizeof(si) ); si.cb = sizeof(si); ZeroMemory( &pi, sizeof(pi) );
      if(CreateProcess(
      NULL,           // No module name (use command line)
      (char*)&strz->chr[0], // Command line
      NULL,           // Process handle not inheritable
      NULL,           // Thread handle not inheritable
      FALSE,          // Set handle inheritance to FALSE
      CREATE_NO_WINDOW, // No creation flags
      NULL,           // Use parent's environment block
      NULL,           // Use parent's starting directory 
      &si,            // Pointer to STARTUPINFO structure
      &pi )           // Pointer to PROCESS_INFORMATION structure
      ){
      shell_call_in_progress=1;
      // Wait until child process exits.
      WaitForSingleObject( pi.hProcess, INFINITE );
      // Close process and thread handles. 
      CloseHandle( pi.hProcess );
      CloseHandle( pi.hThread );
      shell_call_in_progress=0;
      goto shell_complete;
      }
    */

    goto shell_complete;//failed

  }else{

    qbs_set(strz,qbs_add(qbs_new_txt("command.com /c "),str));
    qbs_set(strz,qbs_add(strz,qbs_new_txt_len("\0",1)));
    ZeroMemory( &si, sizeof(si) ); si.cb = sizeof(si); ZeroMemory( &pi, sizeof(pi) );
    if(CreateProcess(

             NULL,           // No module name (use command line)
             (char*)&strz->chr[0], // Command line
             NULL,           // Process handle not inheritable
             NULL,           // Thread handle not inheritable
             FALSE,          // Set handle inheritance to FALSE
             CREATE_NEW_CONSOLE, //note: cannot hide new console, but can preserve existing one
             NULL,           // Use parent's environment block
             NULL,           // Use parent's starting directory 
             &si,            // Pointer to STARTUPINFO structure
             &pi )           // Pointer to PROCESS_INFORMATION structure
       ){
      shell_call_in_progress=1;
      // Wait until child process exits.
      WaitForSingleObject( pi.hProcess, INFINITE );
      // Close process and thread handles. 
      CloseHandle( pi.hProcess );
      CloseHandle( pi.hThread );
      shell_call_in_progress=0;
      goto shell_complete;
    }
    goto shell_complete;//failed

  }//cmd_ok()

#else

  qbs_set(strz,qbs_add(str,qbs_new_txt_len("\0",1)));
  shell_call_in_progress=1;
  system((char*)strz->chr);
  shell_call_in_progress=0;
  return;

#endif

 shell_complete:;
}















void sub_shell3(qbs *str,int32 passed){//_DONTWAIT
  //shell3 launches 'str' but does not wait for it to complete
  if (new_error) return;
  if (cloud_app){error(262); return;}

  if (passed&1){sub_shell4(str,passed&2); return;}

  static qbs *strz=NULL;
  static int32 i;

  static qbs *str1=NULL;
  static qbs *str2=NULL;
  static qbs *str1z=NULL;
  static qbs *str2z=NULL;
  if (!str1) str1=qbs_new(0,0);
  if (!str2) str2=qbs_new(0,0);
  if (!str1z) str1z=qbs_new(0,0);
  if (!str2z) str2z=qbs_new(0,0);

  if (!strz) strz=qbs_new(0,0);

#ifdef QB64_WINDOWS

  static STARTUPINFO si;
  static PROCESS_INFORMATION pi;

  if (cmd_ok()){

    static SHELLEXECUTEINFO shi;
    static char cmd[10]="cmd\0";

    //attempt to separate file name (if any) from parameters
    static int32 x,quotes;

    //allow for launching of a console
    if (!(passed&2)){
      qbs_set(str1,qbs_new_txt("cmd"));
    }else{
      qbs_set(str1,str);
      if (!str1->len) qbs_set(str1,qbs_new_txt("cmd"));
    }
    qbs_set(str2,qbs_new_txt(""));

    if (!cmd_command(str1)){
      //try directly, as is
      qbs_set(str1z,qbs_add(str1,qbs_new_txt_len("\0",1)));
      ZeroMemory( &shi, sizeof(shi) );
      shi.cbSize = sizeof(shi);
      shi.lpFile=(char*)&str1z->chr[0];
      shi.lpParameters=NULL;
      shi.fMask=SEE_MASK_FLAG_NO_UI;
      shi.nShow=SW_SHOW;
      if(ShellExecuteEx(&shi)){
    goto shell_complete;
      }
    }

    x=0;
    quotes=0;
    while (x<str1->len){
      if (str1->chr[x]==34){
    if (!quotes) quotes=1; else quotes=0;
      }
      if (str1->chr[x]==32){
    if (quotes==0){
      qbs_set(str2,qbs_right(str1,str1->len-x-1));
      qbs_set(str1,qbs_left(str1,x));
      goto split;
    }
      }
      x++;
    }
  split:
    if (!str1->len) goto shell_complete;//failed!

    if (str2->len){ if (!cmd_command(str1)){
    qbs_set(str1z,qbs_add(str1,qbs_new_txt_len("\0",1)));
    qbs_set(str2z,qbs_add(str2,qbs_new_txt_len("\0",1)));
    ZeroMemory( &shi, sizeof(shi) );
    shi.cbSize = sizeof(shi);
    shi.lpFile=(char*)&str1z->chr[0];
    shi.lpParameters=(char*)&str2z->chr[0];
    //if (str2->len<=1) shi.lpParameters=NULL;
    shi.fMask=SEE_MASK_FLAG_NO_UI;
    shi.nShow=SW_SHOW;
    if(ShellExecuteEx(&shi)){
      goto shell_complete;
    }
      }}

    //failed, try cmd /c method...
    if (str2->len) qbs_set(str2,qbs_add(qbs_new_txt(" "),str2));
    qbs_set(strz,qbs_add(str1,str2));
    qbs_set(strz,qbs_add(qbs_new_txt(" /c "),strz));
    qbs_set(strz,qbs_add(strz,qbs_new_txt_len("\0",1)));
    ZeroMemory( &shi, sizeof(shi) );
    shi.cbSize = sizeof(shi);
    shi.lpFile=&cmd[0];
    shi.lpParameters=(char*)&strz->chr[0];
    shi.fMask=SEE_MASK_FLAG_NO_UI;
    shi.nShow=SW_SHOW;
    if(ShellExecuteEx(&shi)){
      goto shell_complete;
    }

    /*
      qbs_set(strz,qbs_add(qbs_new_txt("cmd.exe /c "),str));
      qbs_set(strz,qbs_add(strz,qbs_new_txt_len("\0",1)));
      ZeroMemory( &si, sizeof(si) ); si.cb = sizeof(si); ZeroMemory( &pi, sizeof(pi) );
      if(CreateProcess(
      NULL,           // No module name (use command line)
      (char*)&strz->chr[0], // Command line
      NULL,           // Process handle not inheritable
      NULL,           // Thread handle not inheritable
      FALSE,          // Set handle inheritance to FALSE
      DETACHED_PROCESS, // No creation flags
      NULL,           // Use parent's environment block
      NULL,           // Use parent's starting directory 
      &si,            // Pointer to STARTUPINFO structure
      &pi )           // Pointer to PROCESS_INFORMATION structure
      ){
      //ref: The created process remains in the system until all threads within the process have terminated and all handles to the process and any of its threads have been closed through calls to CloseHandle. The handles for both the process and the main thread must be closed through calls to CloseHandle. If these handles are not needed, it is best to close them immediately after the process is created. 
      CloseHandle( pi.hProcess );
      CloseHandle( pi.hThread );
      goto shell_complete;
      }
    */

    goto shell_complete;//failed

  }else{

    qbs_set(strz,qbs_add(qbs_new_txt("command.com /c "),str));
    qbs_set(strz,qbs_add(strz,qbs_new_txt_len("\0",1)));
    ZeroMemory( &si, sizeof(si) ); si.cb = sizeof(si); ZeroMemory( &pi, sizeof(pi) );
    if(CreateProcess(
             NULL,           // No module name (use command line)
             (char*)&strz->chr[0], // Command line
             NULL,           // Process handle not inheritable
             NULL,           // Thread handle not inheritable
             FALSE,          // Set handle inheritance to FALSE
             CREATE_NEW_CONSOLE, //note: cannot hide new console, but can preserve existing one
             NULL,           // Use parent's environment block
             NULL,           // Use parent's starting directory 
             &si,            // Pointer to STARTUPINFO structure
             &pi )           // Pointer to PROCESS_INFORMATION structure
       ){
      //ref: The created process remains in the system until all threads within the process have terminated and all handles to the process and any of its threads have been closed through calls to CloseHandle. The handles for both the process and the main thread must be closed through calls to CloseHandle. If these handles are not needed, it is best to close them immediately after the process is created. 
      CloseHandle( pi.hProcess );
      CloseHandle( pi.hThread );
      goto shell_complete;
    }
    goto shell_complete;//failed

  }//cmd_ok()

#else

  qbs_set(strz,qbs_add(str,qbs_new_txt_len("\0",1)));
  pid_t pid = fork ();
  if (pid == 0)
    _exit (system((char*)strz->chr));
  return;

#endif

 shell_complete:;
}//SHELL _DONTWAIT

























void sub_shell4(qbs *str,int32 passed){//_DONTWAIT & _HIDE
  //if passed&2 set a string was given
  if (!(passed&2)){error(5); return;}//should not hide a shell waiting for input
  if (cloud_app){error(262); return;}

  static qbs *strz=NULL;
  static int32 i;

  static qbs *str1=NULL;
  static qbs *str2=NULL;
  static qbs *str1z=NULL;
  static qbs *str2z=NULL;
  if (!str1) str1=qbs_new(0,0);
  if (!str2) str2=qbs_new(0,0);
  if (!str1z) str1z=qbs_new(0,0);
  if (!str2z) str2z=qbs_new(0,0);

  if (!strz) strz=qbs_new(0,0);

  if (!str->len){error(5); return;}//console unsupported

#ifdef QB64_WINDOWS

  static STARTUPINFO si;
  static PROCESS_INFORMATION pi;

  if (cmd_ok()){

    static SHELLEXECUTEINFO shi;
    static char cmd[10]="cmd\0";

    //attempt to separate file name (if any) from parameters
    static int32 x,quotes;

    qbs_set(str1,str);
    qbs_set(str2,qbs_new_txt(""));

    if (!cmd_command(str1)){
      //try directly, as is
      qbs_set(str1z,qbs_add(str1,qbs_new_txt_len("\0",1)));
      ZeroMemory( &shi, sizeof(shi) );
      shi.cbSize = sizeof(shi);
      shi.lpFile=(char*)&str1z->chr[0];
      shi.lpParameters=NULL;
      shi.fMask=SEE_MASK_FLAG_NO_UI;
      shi.nShow=SW_HIDE;
      if(ShellExecuteEx(&shi)){
    goto shell_complete;
      }
    }

    x=0;
    quotes=0;
    while (x<str1->len){
      if (str1->chr[x]==34){
    if (!quotes) quotes=1; else quotes=0;
      }
      if (str1->chr[x]==32){
    if (quotes==0){
      qbs_set(str2,qbs_right(str1,str1->len-x-1));
      qbs_set(str1,qbs_left(str1,x));
      goto split;
    }
      }
      x++;
    }
  split:
    if (!str1->len) goto shell_complete;//failed!

    if (str2->len){ if (!cmd_command(str1)){
    qbs_set(str1z,qbs_add(str1,qbs_new_txt_len("\0",1)));
    qbs_set(str2z,qbs_add(str2,qbs_new_txt_len("\0",1)));
    ZeroMemory( &shi, sizeof(shi) );
    shi.cbSize = sizeof(shi);
    shi.lpFile=(char*)&str1z->chr[0];
    shi.lpParameters=(char*)&str2z->chr[0];
    //if (str2->len<=1) shi.lpParameters=NULL;
    shi.fMask=SEE_MASK_FLAG_NO_UI;
    shi.nShow=SW_HIDE;
    if(ShellExecuteEx(&shi)){
      goto shell_complete;
    }
      }}

    //failed, try cmd /c method...
    if (str2->len) qbs_set(str2,qbs_add(qbs_new_txt(" "),str2));
    qbs_set(strz,qbs_add(str1,str2));
    qbs_set(strz,qbs_add(qbs_new_txt(" /c "),strz));
    qbs_set(strz,qbs_add(strz,qbs_new_txt_len("\0",1)));
    ZeroMemory( &shi, sizeof(shi) );
    shi.cbSize = sizeof(shi);
    shi.lpFile=&cmd[0];
    shi.lpParameters=(char*)&strz->chr[0];
    shi.fMask=SEE_MASK_FLAG_NO_UI;
    shi.nShow=SW_HIDE;
    if(ShellExecuteEx(&shi)){
      goto shell_complete;
    }

    /*
      qbs_set(strz,qbs_add(qbs_new_txt("cmd.exe /c "),str));
      qbs_set(strz,qbs_add(strz,qbs_new_txt_len("\0",1)));
      ZeroMemory( &si, sizeof(si) ); si.cb = sizeof(si); ZeroMemory( &pi, sizeof(pi) );
      if(CreateProcess(
      NULL,           // No module name (use command line)
      (char*)&strz->chr[0], // Command line
      NULL,           // Process handle not inheritable
      NULL,           // Thread handle not inheritable
      FALSE,          // Set handle inheritance to FALSE
      DETACHED_PROCESS, // No creation flags
      NULL,           // Use parent's environment block
      NULL,           // Use parent's starting directory 
      &si,            // Pointer to STARTUPINFO structure
      &pi )           // Pointer to PROCESS_INFORMATION structure
      ){
      //ref: The created process remains in the system until all threads within the process have terminated and all handles to the process and any of its threads have been closed through calls to CloseHandle. The handles for both the process and the main thread must be closed through calls to CloseHandle. If these handles are not needed, it is best to close them immediately after the process is created. 
      CloseHandle( pi.hProcess );
      CloseHandle( pi.hThread );
      goto shell_complete;
      }
    */

    goto shell_complete;//failed

  }else{

    qbs_set(strz,qbs_add(qbs_new_txt("command.com /c "),str));
    qbs_set(strz,qbs_add(strz,qbs_new_txt_len("\0",1)));
    ZeroMemory( &si, sizeof(si) ); si.cb = sizeof(si); ZeroMemory( &pi, sizeof(pi) );
    if(CreateProcess(
             NULL,           // No module name (use command line)
             (char*)&strz->chr[0], // Command line
             NULL,           // Process handle not inheritable
             NULL,           // Thread handle not inheritable
             FALSE,          // Set handle inheritance to FALSE
             CREATE_NEW_CONSOLE, //note: cannot hide new console, but can preserve existing one
             NULL,           // Use parent's environment block
             NULL,           // Use parent's starting directory 
             &si,            // Pointer to STARTUPINFO structure
             &pi )           // Pointer to PROCESS_INFORMATION structure
       ){
      //ref: The created process remains in the system until all threads within the process have terminated and all handles to the process and any of its threads have been closed through calls to CloseHandle. The handles for both the process and the main thread must be closed through calls to CloseHandle. If these handles are not needed, it is best to close them immediately after the process is created. 
      CloseHandle( pi.hProcess );
      CloseHandle( pi.hThread );
      goto shell_complete;
    }
    goto shell_complete;//failed

  }//cmd_ok()

#else

  qbs_set(strz,qbs_add(str,qbs_new_txt_len("\0",1)));
  pid_t pid = fork ();
  if (pid == 0)
    _exit (system((char*)strz->chr));
  return;

#endif

 shell_complete:;

}//_DONTWAIT & _HIDE






















void sub_kill(qbs *str){
  //note: file not found returned for non-existant paths too
  //      file already open returned if access unavailable
  if (new_error) return;
  static int32 i;
  static qbs *strz=NULL;
  if (!strz) strz=qbs_new(0,0);
  qbs_set(strz,qbs_add(str,qbs_new_txt_len("\0",1)));
#ifdef QB64_WINDOWS
  static WIN32_FIND_DATA fd;
  static HANDLE hFind;
  static qbs *strpath=NULL; if (!strpath) strpath=qbs_new(0,0);
  static qbs *strfullz=NULL; if (!strfullz) strfullz=qbs_new(0,0);
  //find path
  qbs_set(strpath,strz);
  for(i=strpath->len;i>0;i--){
    if ((strpath->chr[i-1]==47)||(strpath->chr[i-1]==92)){strpath->len=i; break;}
  }//i
  if (i==0) strpath->len=0;//no path specified
  static int32 count;
  count=0;
  hFind = FindFirstFile(fixdir(strz), &fd);
  if(hFind==INVALID_HANDLE_VALUE){error(53); return;}//file not found
  do{
    if ((fd.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)==0){
      qbs_set(strfullz,qbs_add(strpath,qbs_new_txt_len(fd.cFileName,strlen(fd.cFileName)+1)));
      if (!DeleteFile((char*)strfullz->chr)){
    i=GetLastError();
    if ((i==5)||(i==19)||(i==33)||(i==32)){FindClose(hFind); error(55); return;}//file already open
    FindClose(hFind); error(53); return;//file not found
      }
      count++;
    }//not a directory
  }while(FindNextFile(hFind,&fd));
  FindClose(hFind);
  if (!count){error(53); return;}//file not found 
  return;
#else
  if (remove(fixdir(strz))){
    i=errno;
    if (i==ENOENT){error(53); return;}//file not found
    if (i==EACCES){error(75); return;}//path/file access error
    error(64);//bad file name (assumed)
  }
#endif
}

void sub_name(qbs *oldname,qbs *newname){
  if (new_error) return;
  static qbs *strz=NULL;
  if (!strz) strz=qbs_new(0,0);
  static qbs *strz2=NULL;
  if (!strz2) strz2=qbs_new(0,0);
  static int32 i;
  qbs_set(strz,qbs_add(oldname,qbs_new_txt_len("\0",1)));
  qbs_set(strz2,qbs_add(newname,qbs_new_txt_len("\0",1)));
  if (rename(fixdir(strz),fixdir(strz2))){
    i=errno;
    if (i==ENOENT){error(53); return;}//file not found
    if (i==EINVAL){error(64); return;}//bad file name
    if (i==EACCES){error(75); return;}//path/file access error
    error(5);//Illegal function call (assumed)
  }
}

void sub_chdir(qbs *str){



  if (new_error) return;
  static qbs *strz=NULL;
  if (!strz) strz=qbs_new(0,0);
  qbs_set(strz,qbs_add(str,qbs_new_txt_len("\0",1)));
  if (chdir(fixdir(strz))==-1){
    //assume errno==ENOENT
    error(76);//path not found
  }

  static int32 tmp_long;
  static int32 got_ports=0;
  if (cloud_app){
    cloud_chdir_complete=1;

    if (!got_ports){
      got_ports=1;
      static FILE* file = fopen ("..\\ports.txt\0", "r");
      fscanf (file, "%d", &tmp_long);
      cloud_port[1]=tmp_long;
      fscanf (file, "%d", &tmp_long);
      cloud_port[2]=tmp_long;
      fclose (file);
    }
  }

}

void sub_mkdir(qbs *str){
  if (new_error) return;
  if (cloud_app){error(262); return;}
  static qbs *strz=NULL;
  if (!strz) strz=qbs_new(0,0);
  qbs_set(strz,qbs_add(str,qbs_new_txt_len("\0",1)));
#ifdef QB64_UNIX
  if (mkdir(fixdir(strz),0770)==-1){
#else
    if (mkdir(fixdir(strz))==-1){
#endif
      if (errno==EEXIST){error(75); return;}//path/file access error
      //assume errno==ENOENT
      error(76);//path not found
    }

  }

  void sub_rmdir(qbs *str){
    if (new_error) return;
    static qbs *strz=NULL;
    if (!strz) strz=qbs_new(0,0);
    qbs_set(strz,qbs_add(str,qbs_new_txt_len("\0",1)));
    if (rmdir(fixdir(strz))==-1){
      if (errno==ENOTEMPTY){error(75); return;}//path/file access error
      //assume errno==ENOENT
      error(76);//path not found
    }
  }

  long double pow2(long double x,long double y){
    if (x<0){
      if (y!=floor(y)){error(5); return 0;}
    }
    return pow(x,y);
  }

  int32 func_freefile(){
    return gfs_fileno_freefile();
  }

  void sub__mousehide(){
    #ifdef QB64_GUI    
    if (!screen_hide){
      while (!window_exists){Sleep(100);}      
      #ifdef QB64_GLUT
      #ifndef QB64_ANDROID
      glutSetCursor(GLUT_CURSOR_NONE);
      #endif
      #endif
    }
    #endif
  }

#ifdef QB64_GLUT
  int mouse_cursor_style=GLUT_CURSOR_LEFT_ARROW;
#else
  int mouse_cursor_style=1;
#endif

  void sub__mouseshow(qbs *style, int32 passed){
    if (new_error) return;

#ifdef QB64_GLUT

    static qbs *str=NULL; if (str==NULL) str=qbs_new(0,0);
    if (passed){
      qbs_set(str,qbs_ucase(style));
      if (qbs_equal(str,qbs_new_txt("DEFAULT"))) {mouse_cursor_style=GLUT_CURSOR_LEFT_ARROW; goto cursor_valid;}
      if (qbs_equal(str,qbs_new_txt("LINK"))) {mouse_cursor_style=GLUT_CURSOR_INFO; goto cursor_valid;}
      if (qbs_equal(str,qbs_new_txt("TEXT"))) {mouse_cursor_style=GLUT_CURSOR_TEXT; goto cursor_valid;}
      if (qbs_equal(str,qbs_new_txt("CROSSHAIR"))) {mouse_cursor_style=GLUT_CURSOR_CROSSHAIR; goto cursor_valid;}
      if (qbs_equal(str,qbs_new_txt("VERTICAL"))) {mouse_cursor_style=GLUT_CURSOR_UP_DOWN; goto cursor_valid;}
      if (qbs_equal(str,qbs_new_txt("HORIZONTAL"))) {mouse_cursor_style=GLUT_CURSOR_LEFT_RIGHT; goto cursor_valid;}
      if (qbs_equal(str,qbs_new_txt("TOPLEFT_BOTTOMRIGHT"))) {mouse_cursor_style=GLUT_CURSOR_TOP_LEFT_CORNER; goto cursor_valid;}
      if (qbs_equal(str,qbs_new_txt ("TOPRIGHT_BOTTOMLEFT"))) {mouse_cursor_style=GLUT_CURSOR_TOP_RIGHT_CORNER; goto cursor_valid;}
      error(5); return;
    }
  cursor_valid:

    if (!screen_hide){
      while (!window_exists){Sleep(100);}
      glutSetCursor(mouse_cursor_style);
    }

#endif

  }

  float func__mousemovementx(int32 context, int32 passed){
    int32 handle;
    handle=mouse_message_queue_default;
    if (passed) handle=context;
    mouse_message_queue_struct *queue=(mouse_message_queue_struct*)list_get(mouse_message_queue_handles,handle);
    if (queue==NULL){error(258); return 0;}
    return queue->queue[queue->current].movementx;
  }
  float func__mousemovementy(int32 context, int32 passed){
    int32 handle;
    handle=mouse_message_queue_default;
    if (passed) handle=context;    
    mouse_message_queue_struct *queue=(mouse_message_queue_struct*)list_get(mouse_message_queue_handles,handle);
    if (queue==NULL){error(258); return 0;}
    return queue->queue[queue->current].movementy;
  }

  void sub__mousemove(float x,float y){
#ifdef QB64_GLUT
    int32 x2,y2,sx,sy;
    if (display_page->text){
      sx=fontwidth[display_page->font]*display_page->width; sy=fontheight[display_page->font]*display_page->height;
      if (x<0.5) goto error;
      if (y<0.5) goto error;
      if (x>((float)display_page->width)+0.5) goto error;
      if (y>((float)display_page->height)+0.5) goto error;
      x-=0.5; y-=0.5;
      x=x*(float)fontwidth[display_page->font]; y=y*(float)fontheight[display_page->font];
      x2=qbr_float_to_long(x); y2=qbr_float_to_long(y);
      if (x2<0) x2=0;
      if (y2<0) y2=0;
      if (x2>sx-1) x2=sx-1;
      if (y2>sy-1) y2=sy-1;
    }else{
      sx=display_page->width; sy=display_page->height;
      x2=qbr_float_to_long(x); y2=qbr_float_to_long(y);
      if (x2<0) goto error;
      if (y2<0) goto error;
      if (x2>sx-1) goto error;
      if (y2>sy-1) goto error;
    }
    //x2,y2 are pixel co-ordinates
    //adjust for fullscreen position as necessary:
    x2*=x_scale; y2*=y_scale;
    x2+=x_offset; y2+=y_offset;
    while (!window_exists){Sleep(100);} 
    glutWarpPointer(x2, y2);
    return;
  error:
    error(5);
#endif
  }

  float func__mousex(int32 context, int32 passed){

    static int32 x,x2;
    static float f;

    int32 handle;
    handle=mouse_message_queue_default;
    if (passed) handle=context;    
    mouse_message_queue_struct *queue=(mouse_message_queue_struct*)list_get(mouse_message_queue_handles,handle);
    if (queue==NULL){error(258); return 0;}
    x=queue->queue[queue->current].x;

    /*
      if (cloud_app){
      x2=display_page->width; if (display_page->text) x2*=fontwidth[display_page->font];
      x_limit=x2-1;
      x_scale=1;
      x_offset=0;
      }
    */

    //calculate pixel offset of mouse within SCREEN using environment variables
    x-=environment_2d__screen_x1;
    x=qbr_float_to_long((((float)x+0.5f)/environment_2d__screen_x_scale)-0.5f);
    if (x<0) x=0;
    if (x>=environment_2d__screen_width) x=environment_2d__screen_width-1;

    //restrict range to the current display page's range to avoid causing errors
    x2=display_page->width; if (display_page->text) x2*=fontwidth[display_page->font];
    if (x>=x2) x=x2-1;

    if (display_page->text){
      f=x;
      x2=fontwidth[display_page->font];
      f=f/(float)x2+0.5f;
      x2=qbr_float_to_long(f);
      if (x2>x) f-=0.001f;
      if (x2<x) f+=0.001f;
      return floor(f + 0.5);
    }

    return x;
  }

  float func__mousey(int32 context, int32 passed){

    static int32 y,y2;
    static float f;

    int32 handle;
    handle=mouse_message_queue_default;
    if (passed) handle=context;    
    mouse_message_queue_struct *queue=(mouse_message_queue_struct*)list_get(mouse_message_queue_handles,handle);
    if (queue==NULL){error(258); return 0;}
    y=queue->queue[queue->current].y;

    /*
      if (cloud_app){
      y2=display_page->height; if (display_page->text) y2*=fontheight[display_page->font];
      y_limit=y2-1;
      y_scale=1;
      y_offset=0;
      }
    */

    //calculate pixel offset of mouse within SCREEN using environment variables
    y-=environment_2d__screen_y1;
    y=qbr_float_to_long((((float)y+0.5f)/environment_2d__screen_y_scale)-0.5f);
    if (y<0) y=0;
    if (y>=environment_2d__screen_height) y=environment_2d__screen_height-1;

    //restrict range to the current display page's range to avoid causing errors
    y2=display_page->height; if (display_page->text) y2*=fontheight[display_page->font];
    if (y>=y2) y=y2-1;

    if (display_page->text){
      f=y;
      y2=fontheight[display_page->font];
      f=f/(float)y2+0.5f;
      y2=qbr_float_to_long(f);
      if (y2>y) f-=0.001f;
      if (y2<y) f+=0.001f;
      return floor(f + 0.5);
    }

    return y;
  }


int32 func__mousepipeopen(){
    //creates a new mouse pipe, routing all mouse input into it before any preceeding pipes receive access to the data

    //create new queue
    int32 context=list_add(mouse_message_queue_handles);
    mouse_message_queue_struct *queue=(mouse_message_queue_struct*)list_get(mouse_message_queue_handles,context);
    queue->lastIndex=65535;
    queue->queue=(mouse_message*)calloc(1,sizeof(mouse_message)*(queue->lastIndex+1));

    //link new queue to child queue
    int32 child_context=mouse_message_queue_first;
    mouse_message_queue_struct *child_queue=(mouse_message_queue_struct*)list_get(mouse_message_queue_handles,child_context);
    queue->child=child_context;
    child_queue->parent=context;

    //set new queue and primary queue
    mouse_message_queue_first=context;

    return context;
}

void sub__mouseinputpipe(int32 context){
    //pushes the current _MOUSEINPUT event to the lower pipe, effectively sharing the input with the lower pipe
    
    mouse_message_queue_struct *queue=(mouse_message_queue_struct*)list_get(mouse_message_queue_handles,context);
    if (queue==NULL){error(258); return;}

    if (context==mouse_message_queue_default){error(5); return;} //cannot pipe input from the default queue

    int32 child_context=queue->child;
    mouse_message_queue_struct *child_queue=(mouse_message_queue_struct*)list_get(mouse_message_queue_handles,child_context);

    //create new event in child queue
    int32 i=child_queue->last+1; if (i>child_queue->lastIndex) i=0;
    if (i==child_queue->current){
      int32 nextIndex=child_queue->last+1; if (nextIndex>child_queue->lastIndex) nextIndex=0;
      child_queue->current=nextIndex;
    }

    int32 i2=queue->current;

    //copy event to child queue
    child_queue->queue[i].x=queue->queue[i2].x;
    child_queue->queue[i].y=queue->queue[i2].y;
    child_queue->queue[i].movementx=queue->queue[i2].movementx;
    child_queue->queue[i].movementy=queue->queue[i2].movementy;
    child_queue->queue[i].buttons=queue->queue[i2].buttons;
    child_queue->last=i;

}

void sub__mousepipeclose(int32 context){
    //closes an existing pipe and reverts the new route the pipe created

    mouse_message_queue_struct *queue=(mouse_message_queue_struct*)list_get(mouse_message_queue_handles,context);
    if (queue==NULL){error(258); return;}
    if (context==mouse_message_queue_default){error(5); return;} //cannot delete default queue

    //todo!

}

 
  int32 func__mouseinput(int32 context, int32 passed){
    int32 handle;
    handle=mouse_message_queue_default;
    if (passed) handle=context;    
    mouse_message_queue_struct *queue=(mouse_message_queue_struct*)list_get(mouse_message_queue_handles,handle);
    if (queue==NULL){error(258); return 0;}
    if (queue->current==queue->last) return 0;
    int32 newIndex=queue->current+1;
    if (newIndex>queue->lastIndex) newIndex=0;
    queue->current=newIndex;
    return -1;
  }

  int32 func__mousebutton(int32 i, int32 context, int32 passed){
    if (i<1){error(5); return 0;}
    if (i>3) return 0;//current SDL only supports 3 mouse buttons!
    //swap indexes 2&3
    if (i==2){
      i=3;
    }else{
      if (i==3) i=2;
    }
    int32 handle;
    handle=mouse_message_queue_default;
    if (passed) handle=context;    
    mouse_message_queue_struct *queue=(mouse_message_queue_struct*)list_get(mouse_message_queue_handles,handle);
    if (queue==NULL){error(258); return 0;}
    if (queue->queue[queue->current].buttons&(1<<(i-1))) return -1;
    return 0;
  }

  int32 func__mousewheel(int32 context, int32 passed){
    static uint32 x;
    int32 handle;
    handle=mouse_message_queue_default;
    if (passed) handle=context;    
    mouse_message_queue_struct *queue=(mouse_message_queue_struct*)list_get(mouse_message_queue_handles,handle);
    if (queue==NULL){error(258); return 0;}
    x=queue->queue[queue->current].buttons;
    if ((x&(8+16))==(8+16)) return 0;//cancelled out change
    if (x&8) return -1;//up
    if (x&16) return 1;//down
    return 0;//no change
  }

  extern uint16 call_absolute_offsets[256];
  void call_absolute(int32 args,uint16 offset){
    memset(&cpu,0,sizeof(cpu_struct));//flush cpu
    cpu.cs=((defseg-cmem)>>4); cpu.ip=offset;
    cpu.ss=0xFFFF; cpu.sp=0;//sp "loops" to <65536 after first push
    cpu.ds=80;
    //push (near) arg offsets
    static int32 i;
    for (i=0;i<args;i++){
      cpu.sp-=2; *(uint16*)(cmem+cpu.ss*16+cpu.sp)=call_absolute_offsets[i];
    }
    //push ret segment, then push ret offset (both 0xFFFF to return control to QB64)
    cpu.sp-=4; *(uint32*)(cmem+cpu.ss*16+cpu.sp)=0xFFFFFFFF;
    cpu_call();
  }

  void call_int(int32 i){

    if (i==0x33){

      if (cpu.ax==0){
    cpu.ax=0xFFFF;//mouse installed
    cpu.bx=2;
    return;
      }

      if (cpu.ax==1){sub__mouseshow(NULL,0); return;}
      if (cpu.ax==2){sub__mousehide(); return;}
      if (cpu.ax==3){
    //return the current mouse status
    //buttons
    
    int32 handle;
    handle=mouse_message_queue_default;    
    mouse_message_queue_struct *queue=(mouse_message_queue_struct*)list_get(mouse_message_queue_handles,handle);

    //buttons
    cpu.bx=queue->queue[queue->last].buttons&1;
    if (queue->queue[queue->last].buttons&4) cpu.bx+=2;

    //x,y offsets    
    static float mx,my;

    //temp override current message index to the most recent event
    static int32 current_mouse_message_backup;
    current_mouse_message_backup=queue->current;
    queue->current=queue->last;

    mx=func__mousex(0,0); my=func__mousey(0,0);

    //restore "current" message index
    queue->current=current_mouse_message_backup;

    cpu.cx=mx; cpu.dx=my;
    //double x-axis value for modes 1,7,13
    if ((display_page->compatible_mode==1)||(display_page->compatible_mode==7)||(display_page->compatible_mode==13)) cpu.cx*=2;
    if (display_page->text){
      //note: a range from 0 to columns*8-1 is returned regardless of the number of actual pixels
      cpu.cx=(mx-0.5)*8.0;
      if (cpu.cx>=(display_page->width*8)) cpu.cx=(display_page->width*8)-1;
      //note: a range from 0 to rows*8-1 is returned regardless of the number of actual pixels
      //obselete line of code: cpu.dx=(((float)cpu.dx)/((float)(display_page->height*fontheight[display_page->font])))*((float)(display_page->height*8));//(mouse_y/height_in_pixels)*(rows*8)
      cpu.dx=(my-0.5)*8.0;
      if (cpu.dx>=(display_page->height*8)) cpu.dx=(display_page->height*8)-1;
    }
    return;
      }

      if (cpu.ax==7){//horizontal min/max
    return;
      }
      if (cpu.ax==8){//vertical min/max
    return;
      }

      //MessageBox2(NULL,"Unknown MOUSE Sub-function","Call Interrupt Error",MB_OK|MB_SYSTEMMODAL);
      //exit(cpu.ax);

      return;
    }

  }





  //2D PROTOTYPE QB64<->C CALLS

  //Creating/destroying an image surface:

  int32 func__newimage(int32 x,int32 y,int32 bpp,int32 passed){
    static int32 i;
    if (new_error) return 0;
    if (x<=0||y<=0){error(5); return 0;}
    if (!passed){
      bpp=write_page->compatible_mode;
    }else{
      i=0;
      if (bpp>=0&&bpp<=2) i=1;
      if (bpp>=7&&bpp<=13) i=1;
      if (bpp==256) i=1;
      if (bpp==32) i=1;
      if (!i){error(5); return 0;}
    }
    i=imgnew(x,y,bpp);
    if (!i) return -1;
    if (!passed){
      //adopt palette
      if (write_page->pal){
    memcpy(img[i].pal,write_page->pal,1024);
      }
      //adopt font
      sub__font(write_page->font,-i,1);
      //adopt colors
      img[i].color=write_page->color;
      img[i].background_color=write_page->background_color;
      //adopt transparent color
      img[i].transparent_color=write_page->transparent_color;
      //adopt blend state
      img[i].alpha_disabled=write_page->alpha_disabled;
      //adopt print mode
      img[i].print_mode=write_page->print_mode;
    }
    return -i;
  }

#ifdef QB64_BACKSLASH_FILESYSTEM
#include "parts\\video\\image\\src.c"
#else
#include "parts/video/image/src.c"
#endif

  int32 func__copyimage(int32 i,int32 mode,int32 passed){
    static int32 i2,bytes;
    static img_struct *s,*d;
    if (new_error) return 0;
    //if (passed){
    if (i>=0){//validate i
      validatepage(i); i=page[i];
    }else{
      i=-i; if (i>=nextimg){error(258); return 0;} if (!img[i].valid){error(258); return 0;}
    }
    // }else{
    // i=write_page_index;
    // }

    s=&img[i];

    if (passed&1){
      if (mode!=s->compatible_mode){
    if (mode!=33||s->compatible_mode!=32){error(5); return -1;}
    //create new buffered hardware image
    i2=new_hardware_img(s->width, s->height, (uint32*)s->offset32,
                NEW_HARDWARE_IMG__BUFFER_CONTENT | NEW_HARDWARE_IMG__DUPLICATE_PROVIDED_BUFFER);
    return i2+HARDWARE_IMG_HANDLE_OFFSET;
      }
    }

    //duplicate structure
    i2=newimg();
    d=&img[i2];
    memcpy(d,s,sizeof(img_struct));
    //duplicate pixel data
    bytes=d->width*d->height*d->bytes_per_pixel;
    d->offset=(uint8*)malloc(bytes);
    if (!d->offset){freeimg(i2); return -1;}
    memcpy(d->offset,s->offset,bytes);
    d->flags|=IMG_FREEMEM;
    //duplicate palette
    if (d->pal){
      d->pal=(uint32*)malloc(1024);
      if (!d->pal){free(d->offset); freeimg(i2); return -1;}
      memcpy(d->pal,s->pal,1024);
      d->flags|=IMG_FREEPAL;
    }
    //adjust flags
    if (d->flags&IMG_SCREEN)d->flags^=IMG_SCREEN;
    //return new handle
    return -i2;
  }

  void sub__freeimage(int32 i,int32 passed){
    if (new_error) return;
    if (passed){
      if (i>=0){//validate i
    error(5); return;//The SCREEN's pages cannot be freed!
      }else{

    static hardware_img_struct *himg;  
    if (himg=get_hardware_img(i)){
      flush_old_hardware_commands();
      //add command to free image
      //create new command handle & structure
      int32 hgch=list_add(hardware_graphics_command_handles);
      hardware_graphics_command_struct* hgc=(hardware_graphics_command_struct*)list_get(hardware_graphics_command_handles,hgch);
      hgc->remove=0;
      //set command values
      hgc->command=HARDWARE_GRAPHICS_COMMAND__FREEIMAGE_REQUEST;
      hgc->src_img=get_hardware_img_index(i);
      himg->valid=0;

      //queue the command
      hgc->next_command=0;
      hgc->order=display_frame_order_next;
      if (last_hardware_command_added){
        hardware_graphics_command_struct* hgc2=(hardware_graphics_command_struct*)list_get(hardware_graphics_command_handles,last_hardware_command_added);
        hgc2->next_command=hgch;
      }
      last_hardware_command_added=hgch;
      if (first_hardware_command==0) first_hardware_command=hgch;
    
      return;
    }

    i=-i; if (i>=nextimg){error(258); return;} if (!img[i].valid){error(258); return;}
      }
    }else{
      i=write_page_index;
    }
    if (img[i].flags&IMG_SCREEN){error(5); return;}//The SCREEN's pages cannot be freed!
    if (write_page_index==i) sub__dest(-display_page_index);
    if (read_page_index==i) sub__source(-display_page_index);
    if (img[i].flags&IMG_FREEMEM) free(img[i].offset);//free pixel data (potential crash here)
    if (img[i].flags&IMG_FREEPAL) free(img[i].pal);//free palette
    freeimg(i);
  }

  void freeallimages(){
    static int32 i;
    //note: handles 0 & -1(1) are reserved
    for (i=2;i<nextimg;i++){
      if (img[i].valid){
    if ((img[i].flags&IMG_SCREEN)==0){//The SCREEN's pages cannot be freed!
      sub__freeimage(-i,1);
    }
      }//valid
    }//i
  }

  //Selecting images:

  void sub__source(int32 i){ 
    if (new_error) return;
    if (i>=0){//validate i
      validatepage(i); i=page[i];
    }else{
      i=-i; if (i>=nextimg){error(258); return;} if (!img[i].valid){error(258); return;} 
    }
    read_page_index=i; read_page=&img[i];
  }

  void sub__dest(int32 i){ 
    if (new_error) return;
    if (i>=0){//validate i
      validatepage(i); i=page[i];
    }else{
      i=-i; if (i>=nextimg){error(258); return;} if (!img[i].valid){error(258); return;} 
    }
    write_page_index=i; write_page=&img[i];
  }

  int32 func__source(){
    return -read_page_index;
  }

  int32 func__dest(){
    return -write_page_index;
  }

  int32 func__display(){
    return -display_page_index;
  }

  //Changing the settings of an image surface:

  void sub__blend(int32 i,int32 passed){
    if (new_error) return;
    if (passed){
      if (i>=0){//validate i
    validatepage(i); i=page[i];
      }else{
    static hardware_img_struct *himg;  
    if (himg=get_hardware_img(i)){
      himg->alpha_disabled=0;
      return;
    }
    i=-i; if (i>=nextimg){error(258); return;} if (!img[i].valid){error(258); return;} 
      }
    }else{
      i=write_page_index;
    }
    if (img[i].bytes_per_pixel!=4){error(5); return;}
    img[i].alpha_disabled=0;
  }

  void sub__dontblend(int32 i,int32 passed){
    if (new_error) return;
    if (passed){
      if (i>=0){//validate i
    validatepage(i); i=page[i];
      }else{
    static hardware_img_struct *himg;  
    if (himg=get_hardware_img(i)){
      himg->alpha_disabled=1;
      return;
    }
    i=-i;
    if (i>=nextimg){error(258); return;}
    if (!img[i].valid){
      error(258); return;
    } 
      }
    }else{
      i=write_page_index;
    }
    if (img[i].bytes_per_pixel!=4) return;
    img[i].alpha_disabled=1;
  }


  void sub__clearcolor(uint32 c,int32 i,int32 passed){
    //--         _NONE->1       2       4
    //id.specialformat = "[{_NONE}][?][,?]"
    if (new_error) return;
    static img_struct *im;
    static int32 z;
    static uint32 *lp,*last;
    static uint8 b_max,b_min,g_max,g_min,r_max,r_min;
    static uint8 *cp,*clast,v;
    if (passed&4){
      if (i>=0){//validate i
    validatepage(i); i=page[i];
      }else{
    i=-i; if (i>=nextimg){error(258); return;} if (!img[i].valid){error(258); return;} 
      }
    }else{
      i=write_page_index;
    }
    im=&img[i];
    //text?
    if (im->text){
      if ((passed&1)&&(!(passed&2))) return; //you can disable clearcolor using _CLEARCOLOR _NONE in text modes
      error(5); return;
    }
    //palette?
    if (im->pal){
      if (passed&1){
    if (passed&2){error(5); return;}//invalid options
    im->transparent_color=-1;
    return;
      }
      if (!(passed&2)){error(5); return;}//invalid options
      if (c>255){error(5); return;}//invalid color
      im->transparent_color=c;
      return;
    }
    //32-bit? (alpha is ignored in this case)
    if (passed&1){
      if (passed&2){error(5); return;}//invalid options
      return;//no action
    }
    if (!(passed&2)){error(5); return;}//invalid options
    c&=0xFFFFFF;
    last=im->offset32+im->width*im->height;
    for (lp=im->offset32;lp<last;lp++){
      if ((*lp&0xFFFFFF)==c) *lp=c;
    }
    return;
  }

  //Changing/Using an image surface:

  //_PUT "[(?,?)[-(?,?)]][,[?][,[?][,[(?,?)[-(?,?)]]]]]"
  //(defined elsewhere)

  //_IMGALPHA "?[,[?[{TO}?]][,?]]"
  void sub__setalpha(int32 a,uint32 c,uint32 c2,int32 i,int32 passed){
    //-->                             1        4        2
    static img_struct *im;
    static int32 z;
    static uint32 *lp,*last;
    static uint8 b_max,b_min,g_max,g_min,r_max,r_min,a_max,a_min;
    static uint8 *cp,*clast,v;
    if (new_error) return;
    if (passed&2){
      if (i>=0){//validate i
    validatepage(i); i=page[i];
      }else{
    i=-i; if (i>=nextimg){error(258); return;} if (!img[i].valid){error(258); return;} 
      }
    }else{
      i=write_page_index;
    }
    im=&img[i];
    if (im->pal){error(5); return;}//does not work on paletted images!
    if (a<0||a>255){error(5); return;}//invalid range
    if (passed&4){
      //ranged
      if (c==c2) goto uniquerange;
      b_min=c&0xFF;  g_min=c>>8&0xFF;  r_min=c>>16&0xFF; a_min=c>>24&0xFF;
      b_max=c2&0xFF; g_max=c2>>8&0xFF; r_max=c2>>16&0xFF; a_max=c2>>24&0xFF;
      if (b_min>b_max) swap(b_min,b_max);

      if (g_min>g_max) swap(g_min,g_max);
      if (r_min>r_max) swap(r_min,r_max);
      if (a_min>a_max) swap(a_min,a_max);
      cp=im->offset;
      z=im->width*im->height;
    setalpha:
      if (z--){
    v=*cp; if (v<=b_max&&v>=b_min){
      v=*(cp+1); if (v<=g_max&&v>=g_min){
        v=*(cp+2); if (v<=r_max&&v>=r_min){
          v=*(cp+3); if (v<=a_max&&v>=a_min){
        *(cp+3)=a;
          }}}}
    cp+=4;
    goto setalpha;
      }
      return;
    }
    if (passed&1){
    uniquerange:
      //alpha of c=a
      c2=a<<24;
      lp=im->offset32-1;
      last=im->offset32+im->width*im->height-1;
      while (lp<last){
    if (*++lp==c){
      *lp=(*lp&0xFFFFFF)|c2;
    }
      }
      return;
    }
    //all alpha=a
    cp=im->offset-1;
    clast=im->offset+im->width*im->height*4-4;
    while (cp<clast){*(cp+=4)=a;}
    return;
  }

  //Finding infomation about an image surface:

  int32 func__width(int32 i,int32 passed){
    if (new_error) return 0;
    if (passed){
      if (i>=0){//validate i
    validatepage(i); i=page[i];
      }else{
    static hardware_img_struct *himg;  
    if (himg=get_hardware_img(i)){
      return himg->w;
    }
    i=-i; if (i>=nextimg){error(258); return 0;} if (!img[i].valid){error(258); return 0;}
      }
    }else{
      i=write_page_index;
    }
    return img[i].width;
  }

  int32 func__height(int32 i,int32 passed){
    if (new_error) return 0;
    if (passed){
      if (i>=0){//validate i
    validatepage(i); i=page[i];
      }else{
    static hardware_img_struct *himg;  
    if (himg=get_hardware_img(i)){
      return himg->h;
    }
    i=-i; if (i>=nextimg){error(258); return 0;} if (!img[i].valid){error(258); return 0;}
      }
    }else{
      i=write_page_index;
    }
    return img[i].height;
  }

  int32 func__pixelsize(int32 i,int32 passed){
    if (new_error) return 0;
    if (passed){
      if (i>=0){//validate i
    validatepage(i); i=page[i];
      }else{
    i=-i; if (i>=nextimg){error(258); return 0;} if (!img[i].valid){error(258); return 0;}
      }
    }else{
      i=write_page_index;
    }
    i=img[i].compatible_mode;
    if (i==32) return 4;
    if (!i) return 0;
    return 1;
  }

  int32 func__clearcolor(int32 i,int32 passed){
    if (new_error) return 0;
    if (passed){
      if (i>=0){//validate i
    validatepage(i); i=page[i];
      }else{
    i=-i; if (i>=nextimg){error(258); return 0;} if (!img[i].valid){error(258); return 0;}
      }
    }else{
      i=write_page_index;
    }
    if (img[i].text) return -1;
    if (img[i].compatible_mode==32) return 0;
    return img[i].transparent_color;
  }

  int32 func__blend(int32 i,int32 passed){
    if (new_error) return 0;
    if (passed){
      if (i>=0){//validate i
    validatepage(i); i=page[i];
      }else{
    i=-i; if (i>=nextimg){error(258); return 0;} if (!img[i].valid){error(258); return 0;}
      }
    }else{
      i=write_page_index;
    }
    if (img[i].compatible_mode==32){
      if (!img[i].alpha_disabled) return -1;
    }
    return 0;
  }

  uint32 func__defaultcolor(int32 i,int32 passed){
    if (new_error) return 0;
    if (passed){
      if (i>=0){//validate i
    validatepage(i); i=page[i];
      }else{
    i=-i; if (i>=nextimg){error(258); return 0;} if (!img[i].valid){error(258); return 0;}
      }
    }else{
      i=write_page_index;
    }
    return img[i].color;
  }

  uint32 func__backgroundcolor(int32 i,int32 passed){
    if (new_error) return 0;
    if (passed){
      if (i>=0){//validate i
    validatepage(i); i=page[i];
      }else{
    i=-i; if (i>=nextimg){error(258); return 0;} if (!img[i].valid){error(258); return 0;}
      }
    }else{
      i=write_page_index;
    }
    return img[i].background_color;
  }

  //Working with 256 color palettes:

  uint32 func__palettecolor(int32 n,int32 i,int32 passed){
    if (new_error) return 0;
    if (passed){
      if (i>=0){//validate i
    validatepage(i); i=page[i];
      }else{
    i=-i; if (i>=nextimg){error(258); return 0;} if (!img[i].valid){error(258); return 0;}
      }
    }else{
      i=write_page_index;
    }
    if (!img[i].pal){error(5); return 0;}
    if (n<0||n>255){error(5); return 0;}//out of range
    return img[i].pal[n]|0xFF000000;
  }

  void sub__palettecolor(int32 n,uint32 c,int32 i,int32 passed){
    if (new_error) return;
    if (passed){
      if (i>=0){//validate i
    validatepage(i); i=page[i];
      }else{
    i=-i; if (i>=nextimg){error(258); return;} if (!img[i].valid){error(258); return;}
      }
    }else{
      i=write_page_index;
    }
    if (!img[i].pal){error(5); return;}
    if (n<0||n>255){error(5); return;}//out of range
    img[i].pal[n]=c;
  }

  void sub__copypalette(int32 i,int32 i2,int32 passed){
    if (new_error) return;
    if (passed&1){
      if (i>=0){//validate i
    validatepage(i); i=page[i];
      }else{
    i=-i; if (i>=nextimg){error(258); return;} if (!img[i].valid){error(258); return;}
      }
    }else{
      i=read_page_index;
    }
    if (!img[i].pal){error(5); return;}
    swap(i,i2);
    if (passed&2){
      if (i>=0){//validate i
    validatepage(i); i=page[i];
      }else{
    i=-i; if (i>=nextimg){error(258); return;} if (!img[i].valid){error(258); return;}
      }
    }else{
      i=write_page_index;
    }
    if (!img[i].pal){error(5); return;}
    swap(i,i2);
    memcpy(img[i2].pal,img[i].pal,1024);
  }



  void sub__printstring(float x,float y,qbs* text,int32 i,int32 passed){
    if (new_error) return;
    if (passed&2){
      if (i>=0){//validate i
    validatepage(i); i=page[i];
      }else{
    i=-i; if (i>=nextimg){error(258); return;} if (!img[i].valid){error(258); return;}
      }
    }else{
      i=write_page_index;
    }
    static img_struct *im;
    im=&img[i];
    if (!text->len) return;
    if (im->text){
      int oldx = func_pos(0), oldy = func_csrlin();
      qbg_sub_locate(y, x, 0, 0, 0, 3);
      qbs_print(text, 0);
      qbg_sub_locate(oldy, oldx, 0, 0, 0, 3);
      return;
    }
    //graphics modes only
    if (!text->len) return;
    //Step?
    if (passed&1){im->x+=x; im->y+=y;}else{im->x=x; im->y=y;}
    //Adjust co-ordinates for viewport?
    static int32 x2,y2;
    if (im->clipping_or_scaling){
      if (im->clipping_or_scaling==2){
    x2=qbr_float_to_long(im->x*im->scaling_x+im->scaling_offset_x)+im->view_offset_x;
    y2=qbr_float_to_long(im->y*im->scaling_y+im->scaling_offset_y)+im->view_offset_y;
      }else{
    x2=qbr_float_to_long(im->x)+im->view_offset_x; y2=qbr_float_to_long(im->y)+im->view_offset_y;
      }
    }else{
      x2=qbr_float_to_long(im->x); y2=qbr_float_to_long(im->y); 
    }

    if (!text->len) return;


    static uint32 w,h,z,z2,z3,a,a2,a3,color,background_color,f;
    static uint32 *lp;
    static uint8 *cp;

    color=im->color;
    background_color=im->background_color;

    f=im->font;
    h=fontheight[f];

    if (f>=32){//custom font

      //8-bit / alpha-disabled 32-bit / dont-blend(alpha may still be applied)
      if ((im->bytes_per_pixel==1)||((im->bytes_per_pixel==4)&&(im->alpha_disabled))||(fontflags[f]&8)){

    //render character
    static int32 ok;
    static uint8 *rt_data;
    static int32 rt_w,rt_h,rt_pre_x,rt_post_x;
    //int32 FontRenderTextASCII(int32 i,uint8*codepoint,int32 codepoints,int32 options,
    //                          uint8**out_data,int32*out_x,int32 *out_y,int32*out_x_pre_increment,int32*out_x_post_increment){
    ok=FontRenderTextASCII(font[f],(uint8*)text->chr,text->len,1,
                   &rt_data,&rt_w,&rt_h,&rt_pre_x,&rt_post_x);
    if (!ok) return;

    w=rt_w;

    switch(im->print_mode){
    case 3:
      for (y2=0;y2<h;y2++){
        cp=rt_data+y2*w;
        for (x2=0;x2<w;x2++){
          if (*cp++) pset_and_clip(x+x2,y+y2,color); else pset_and_clip(x+x2,y+y2,background_color);
        }}
      break;
    case 1:
      for (y2=0;y2<h;y2++){
        cp=rt_data+y2*w;
        for (x2=0;x2<w;x2++){
          if (*cp++) pset_and_clip(x+x2,y+y2,color);
        }}
      break;
    case 2:
      for (y2=0;y2<h;y2++){
        cp=rt_data+y2*w;
        for (x2=0;x2<w;x2++){
          if (!(*cp++)) pset_and_clip(x+x2,y+y2,background_color);
        }}
      break;
    default:
      break;
    }

    free(rt_data);
    return;
      }//1-8 bit
      //assume 32-bit blended

      a=(color>>24)+1; a2=(background_color>>24)+1;
      z=color&0xFFFFFF; z2=background_color&0xFFFFFF;

      //render character
      static int32 ok;
      static uint8 *rt_data;
      static int32 rt_w,rt_h,rt_pre_x,rt_post_x;
      //int32 FontRenderTextASCII(int32 i,uint8*codepoint,int32 codepoints,int32 options,
      //                          uint8**out_data,int32*out_x,int32 *out_y,int32*out_x_pre_increment,int32*out_x_post_increment){
      ok=FontRenderTextASCII(font[f],(uint8*)text->chr,text->len,0,
                 &rt_data,&rt_w,&rt_h,&rt_pre_x,&rt_post_x);

      if (!ok) return;

      w=rt_w;

      switch(im->print_mode){
      case 3:

    static float r1,g1,b1,alpha1,r2,g2,b2,alpha2;
    alpha1=(color>>24)&255; r1=(color>>16)&255; g1=(color>>8)&255; b1=color&255;
    alpha2=(background_color>>24)&255; r2=(background_color>>16)&255; g2=(background_color>>8)&255; b2=background_color&255;
    static float dr,dg,db,da;

    dr=r2-r1;
    dg=g2-g1;
    db=b2-b1;
    da=alpha2-alpha1;
    static float cw;//color weight multiplier, avoids seeing black when transitioning from RGBA(?,?,?,255) to RGBA(0,0,0,0)
    if (alpha1) cw=alpha2/alpha1; else cw=100000;
    static float d;
 
    for (y2=0;y2<h;y2++){
      cp=rt_data+y2*w;
      for (x2=0;x2<w;x2++){

        d=*cp++;
        d=255-d;
        d/=255.0;
        static float r3,g3,b3,alpha3;
        alpha3=alpha1+da*d;
        d*=cw; if (d>1.0) d=1.0;
        r3=r1+dr*d;
        g3=g1+dg*d;
        b3=b1+db*d;
        static int32 r4,g4,b4,alpha4;
        r4=qbr_float_to_long(r3);
        g4=qbr_float_to_long(g3);
        b4=qbr_float_to_long(b3);
        alpha4=qbr_float_to_long(alpha3);
        pset_and_clip(x+x2,y+y2,b4+(g4<<8)+(r4<<16)+(alpha4<<24));

      }}
    break;
      case 1:
    for (y2=0;y2<h;y2++){
      cp=rt_data+y2*w;
      for (x2=0;x2<w;x2++){
        z3=*cp++;
        if (z3) pset_and_clip(x+x2,y+y2,((z3*a)>>8<<24)+z);
      }}
    break;
      case 2:
    for (y2=0;y2<h;y2++){
      cp=rt_data+y2*w;
      for (x2=0;x2<w;x2++){
        z3=*cp++;
        if (z3!=255) pset_and_clip(x+x2,y+y2,(((255-z3)*a2)>>8<<24)+z2);
      }}
    break;
      default:
    break;
      }
      free(rt_data);
      return;
    }//custom font

    //default fonts
    static int32 character,character_c;
    for (character_c=0;character_c<text->len;character_c++){
      character=text->chr[character_c];
      if (im->font==8) cp=&charset8x8[character][0][0];
      if (im->font==14) cp=&charset8x16[character][1][0];
      if (im->font==16) cp=&charset8x16[character][0][0];
      switch(im->print_mode){
      case 3:
    for (y2=0;y2<h;y2++){ for (x2=0;x2<8;x2++){
        if (*cp++) pset_and_clip(x+x2,y+y2,color); else pset_and_clip(x+x2,y+y2,background_color);
      }}
    break;
      case 1:
    for (y2=0;y2<h;y2++){ for (x2=0;x2<8;x2++){
        if (*cp++) pset_and_clip(x+x2,y+y2,color);
      }}
    break;
      case 2:
    for (y2=0;y2<h;y2++){ for (x2=0;x2<8;x2++){
        if (!(*cp++)) pset_and_clip(x+x2,y+y2,background_color);
      }}
    break;
      default:
    break;
      }
      x+=8;
    }//z
    return;
  }

int32 func__fontwidth(int32 f,int32 passed);
int32 func__fontheight(int32 f,int32 passed);
int32 func__font(int32 f,int32 passed);

int32 func__printwidth(qbs* text, int32 screenhandle, int32 passed){
  /* Luke Ceddia <flukiluke@gmail.com>
   * This routine should be rewritten properly by calling True Type.
   * This is a temporary implementation
   */
  //Validate screenhandle (taken from func__font)
  if (passed) {
    if (screenhandle >= 0) {
      validatepage(screenhandle);
      screenhandle = page[screenhandle];
    }
    else {
      screenhandle = -screenhandle; 
      if (screenhandle >= nextimg) {
    error(258); 
    return 0;
      } 
      if (!img[screenhandle].valid) {
    error(258); 
    return 0;
      }
    }
  }
  else {
    screenhandle = write_page_index;
  }
  if (img[screenhandle].text) { //Return LEN(text) in non-graphic modes
    //error(5);
    return text->len;
  }
  if (text->len == 0) return 0; //No point calculating an empty string
  int32 fonthandle = img[screenhandle].font; //Get the font used in screenhandle
  int32 fwidth = func__fontwidth(fonthandle, 1); //Try and get the font width
  if (fwidth != 0) return fwidth*(text->len); //if it's not a variable width font, return the width * the letter count
  int32 fheight = func__fontheight(fonthandle, 1); //Height of the font used in screenhandle
  int32 tempscreen = func__newimage(65535, fheight, 32, 1); //Just like calling _NEWIMAGE
  int32 oldwritepage = func__dest();
  sub__dest(tempscreen);
  int32 oldreadpage = func__source();
  sub__source(tempscreen);
  sub__font(fonthandle, NULL, 0); //Set the font on our new screen
  qbg_sub_color(0xffffffff, 0xffffffff, 0, NULL);
  qbs_print(text, 0);
  int xpos = 0;
  for (int i = 65534; i >= 0; i--) if (point(i, 0) != 0) {xpos = i; break;}
  sub__freeimage(tempscreen, 1);
  sub__dest(oldwritepage);
  sub__source(oldreadpage);
  if (xpos == 0) return 0;
  return xpos + 1;
}

  /*int32 func__printwidth(qbs* text,int32 i,int32 passed){
    if (new_error) return 0;

    static int32 f;
    static img_struct *im;

    if (passed){
      if (i>=0){//validate i
    validatepage(i); i=page[i];
      }else{
    i=-i; if (i>=nextimg){error(258); return 0;} if (!img[i].valid){error(258); return 0;}
      }
    }else{
      i=write_page_index;
    }
    im=&img[i];
    if (im->text){error(5); return 0;}//graphics modes only
    if (!text->len) return 0;

    if (f>=32){//custom font
      //8-bit / alpha-disabled 32-bit / dont-blend(alpha may still be applied)
      if ((im->bytes_per_pixel==1)||((im->bytes_per_pixel==4)&&(im->alpha_disabled))||(fontflags[f]&8)){
    //render
    static int32 ok;
    static uint8 *rt_data;
    static int32 rt_w,rt_h,rt_pre_x,rt_post_x;
    //int32 FontRenderTextASCII(int32 i,uint8*codepoint,int32 codepoints,int32 options,
    //                          uint8**out_data,int32*out_x,int32 *out_y,int32*out_x_pre_increment,int32*out_x_post_increment){
    ok=FontRenderTextASCII(font[f],(uint8*)text->chr,text->len,1,
                   &rt_data,&rt_w,&rt_h,&rt_pre_x,&rt_post_x);
    if (!ok) return 0;
    free(rt_data);
    return rt_w;
      }//1-8 bit
      //assume 32-bit blended
      //render
      static int32 ok;
      static uint8 *rt_data;
      static int32 rt_w,rt_h,rt_pre_x,rt_post_x;
      //int32 FontRenderTextASCII(int32 i,uint8*codepoint,int32 codepoints,int32 options,
      //                          uint8**out_data,int32*out_x,int32 *out_y,int32*out_x_pre_increment,int32*out_x_post_increment){
      ok=FontRenderTextASCII(font[f],(uint8*)text->chr,text->len,0,
                 &rt_data,&rt_w,&rt_h,&rt_pre_x,&rt_post_x);
      if (!ok) return 0;
      free(rt_data);
      return rt_w;
    }//custom font

    //default fonts
    return text->len*8;

    }//printwidth*/


int32 func__loadfont(qbs *f,int32 size,qbs *requirements,int32 passed){
    //f=_LOADFONT(ttf_filename$,height[,"bold,italic,underline,monospace,dontblend,unicode"])

    if (new_error) return NULL;

    qbs *s1=NULL; s1=qbs_new(0,0);
    qbs *req=NULL; req=qbs_new(0,0);
    qbs *s3=NULL; s3=qbs_new(0,0);
    uint8 r[32];
    int32 i,i2,i3;
	static int32 recall;

    //validate size
    if (size<1){error(5); return NULL;}
    if (size>2048) return -1;

    //check requirements
    memset(r,0,32);
    if (passed){
      if (requirements->len){
    i=1;
    qbs_set(req,qbs_ucase(requirements));//convert tmp str to perm str
      nextrequirement:
    i2=func_instr(i,req,qbs_new_txt(","),1);
    if (i2){
      qbs_set(s1,func_mid(req,i,i2-i,1));
    }else{
      qbs_set(s1,func_mid(req,i,req->len-i+1,1));
    }
    qbs_set(s1,qbs_rtrim(qbs_ltrim(s1)));
    if (qbs_equal(s1,qbs_new_txt("BOLD"))){r[0]++; goto valid;}
    if (qbs_equal(s1,qbs_new_txt("ITALIC"))){r[1]++; goto valid;}
    if (qbs_equal(s1,qbs_new_txt("UNDERLINE"))){r[2]++; goto valid;}
    if (qbs_equal(s1,qbs_new_txt("DONTBLEND"))){r[3]++; goto valid;}
    if (qbs_equal(s1,qbs_new_txt("MONOSPACE"))){r[4]++; goto valid;}
    if (qbs_equal(s1,qbs_new_txt("UNICODE"))){r[5]++; goto valid;}
    error(5); return NULL;//invalid requirements
      valid:
    if (i2){i=i2+1; goto nextrequirement;}
    for (i=0;i<32;i++) if (r[i]>1){error(5); return NULL;}//cannot define requirements twice
      }//->len
    }//passed
    int32 options;
    options=r[0]+(r[1]<<1)+(r[2]<<2)+(r[3]<<3)+(r[4]<<4)+(r[5]<<5);
    //1 bold TTF_STYLE_BOLD
    //2 italic TTF_STYLE_ITALIC
    //4 underline TTF_STYLE_UNDERLINE
    //8 dontblend (blending is the default in 32-bit alpha-enabled modes)
    //16 monospace
    //32 unicode

    //load the file
    if (!f->len) return -1;//return invalid handle if null length string
    int32 fh,result;
    int64 bytes;
    fh=gfs_open(f,1,0,0);

        #ifdef QB64_WINDOWS //rather than just immediately tossing an error, let's try looking in the default OS folder for the font first in case the user left off the filepath.
            if (fh<0&&recall==0) {
			    recall=-1; //to set a flag so we don't get trapped endlessly recalling the routine when the font actually doesn't exist
			    i=func__loadfont(qbs_add(qbs_new_txt("C:/Windows/Fonts/"),f), size, requirements,passed); //Look in the default windows font location
				return i;
			}
		#endif
	recall=0;

    if (fh<0) return -1;
    bytes=gfs_lof(fh);
    static uint8* content;
    content=(uint8*)malloc(bytes); if (!content){gfs_close(fh); return -1;}
    result=gfs_read(fh,-1,content,bytes);
    gfs_close(fh);
    if (result<0){free(content); return -1;}

    //open the font
    //get free font handle
    for (i=32;i<=lastfont;i++){
      if (!font[i]) goto got_font_index;
    }
    //increase handle range
    lastfont++;
    font=(int32*)realloc(font,4*(lastfont+1)); font[lastfont]=NULL;
    fontheight=(int32*)realloc(fontheight,4*(lastfont+1));
    fontwidth=(int32*)realloc(fontwidth,4*(lastfont+1));
    fontflags=(int32*)realloc(fontflags,4*(lastfont+1));
    i=lastfont;
  got_font_index:
    static int32 h;
    h=FontLoad(content,bytes,size,-1,options);
    free(content);
    if (!h) return -1;

    font[i]=h;
    fontheight[i]=size;
    fontwidth[i]=FontWidth(h);
    fontflags[i]=options;
    return i;
    }

  int32 fontopen(qbs *f,int32 size,int32 options){//Note: Just a stripped down verions of FUNC__LOADFONT

    static int32 i;

    //load the file
    if (!f->len) return -1;//return invalid handle if null length string
    static int32 fh,result;
    static int64 bytes;
    fh=gfs_open(f,1,0,0);
    if (fh<0) return -1;
    bytes=gfs_lof(fh);
    static uint8* content;
    content=(uint8*)malloc(bytes); if (!content){gfs_close(fh); return -1;}
    result=gfs_read(fh,-1,content,bytes);
    gfs_close(fh);
    if (result<0){free(content); return -1;}

    //open the font
    //get free font handle
    for (i=32;i<=lastfont;i++){
      if (!font[i]) goto got_font_index;
    }
    //increase handle range
    lastfont++;
    font=(int32*)realloc(font,4*(lastfont+1)); font[lastfont]=NULL;
    fontheight=(int32*)realloc(fontheight,4*(lastfont+1));
    fontwidth=(int32*)realloc(fontwidth,4*(lastfont+1));
    fontflags=(int32*)realloc(fontflags,4*(lastfont+1));
    i=lastfont;
  got_font_index:
    static int32 h;
    h=FontLoad(content,bytes,size,-1,options);
    free(content);
    if (!h) return -1;

    font[i]=h;
    fontheight[i]=size;
    fontwidth[i]=FontWidth(h);
    fontflags[i]=options;
    return i;
  }




  void sub__font(int32 f,int32 i,int32 passed){
    //_FONT "?[,?]"
    static int32 i2;
    static img_struct *im;
    if (new_error) return;
    if (passed&1){
      if (i>=0){//validate i
    validatepage(i); i=page[i];
      }else{
    i=-i; if (i>=nextimg){error(258); return;} if (!img[i].valid){error(258); return;}
      }
    }else{
      i=write_page_index;
    }
    im=&img[i];
    //validate f
    i2=0;
    if (f==8) i2=1;
    if (f==9) i2=1;
    if (f==14) i2=1;
    if (f==15) i2=1;
    if (f==16) i2=1;
    if (f==17) i2=1;
    if (f>=32&&f<=lastfont){
      if (font[f]) i2=1;
    }
    if (!i2){error(258); return;}

    if (im->text&&((fontflags[f]&16)==0)){error(5); return;}//only monospace fonts can be used on text surfaces
    //note: font changes to text screen mode images requires:
    //      i) font change across all screen pages
    //      ii) locking of the display
    //      iii) update of the data being displayed
    if (im->text){
      if (im->flags&IMG_SCREEN){
    //lock display
    if (autodisplay){
      if (lock_display==0) lock_display=1;//request lock
      while (lock_display!=2) Sleep(0);
    }
    //force update of data
    screen_last_valid=0;//ignore cache used to update the screen on next update
    //apply change across all video pages
    for(i=0;i<pages;i++){
      if(page[i]){
        im=&img[page[i]];
        im->font=f;
        //note: moving the cursor is unnecessary
      }
    }
    //unlock
    if (autodisplay){
      if (lock_display_required) lock_display=0;//release lock
    }
    return;
      }
    }//text

    im->font=f;
    im->cursor_x=1; im->cursor_y=1;
    im->top_row=1;
    if (im->compatible_mode) im->bottom_row=im->height/fontheight[f]; else im->bottom_row=im->height;
    im->bottom_row--; if (im->bottom_row<=0) im->bottom_row=1;
    return;
  }

  int32 func__fontwidth(int32 f,int32 passed){
    static int32 i2;
    if (new_error) return 0;
    if (passed){
      //validate f
      i2=0;
      if (f==8) i2=1;
      if (f==14) i2=1;
      if (f==16) i2=1;
      if (f>=32&&f<=lastfont){
    if (font[f]) i2=1;
      }
      if (!i2){error(258); return 0;}
    }else{
      f=write_page->font;
    }
    return fontwidth[f];
  }

  int32 func__fontheight(int32 f,int32 passed){
    static int32 i2;
    if (new_error) return 0;
    if (passed){
      //validate f
      i2=0;
      if (f==8) i2=1;
      if (f==14) i2=1;
      if (f==16) i2=1;
      if (f>=32&&f<=lastfont){
    if (font[f]) i2=1;
      }
      if (!i2){error(258); return 0;}
    }else{
      f=write_page->font;
    }
    return fontheight[f];
  }

  int32 func__font(int32 i,int32 passed){
    if (new_error) return 0;
    if (passed){
      if (i>=0){//validate i
    validatepage(i); i=page[i];
      }else{
    i=-i; if (i>=nextimg){error(258); return 0;} if (!img[i].valid){error(258); return 0;}
      }
    }else{
      i=write_page_index;
    }
    return img[i].font;
  }

  void sub__freefont(int32 f){
    if (new_error) return;
    static int32 i,i2;
    //validate f (cannot remove QBASIC built in fonts!)
    i2=0;
    if (f>=32&&f<=lastfont){
      if (font[f]) i2=1;
    }
    if (!i2){error(258); return;}
    //check all surfaces, no surface can be using the font
    for (i=1;i<nextimg;i++){
      if (img[i].valid){
    if (img[i].font==f){error(5); return;}
      }
    }
    //remove font
    FontFree(font[f]);
    font[f]=NULL;
  }

  void sub__printmode(int32 mode,int32 i,int32 passed){
    if (new_error) return;
    if (passed){
      if (i>=0){//validate i
    validatepage(i); i=page[i];
      }else{
    i=-i; if (i>=nextimg){error(258); return;} if (!img[i].valid){error(258); return;}
      }
    }else{
      i=write_page_index;
    }
    if (img[i].text){
      if (mode!=1){error(5); return;}
    }
    if (mode==1) img[i].print_mode=3;//fill
    if (mode==2) img[i].print_mode=1;//keep
    if (mode==3) img[i].print_mode=2;//only
  }

  int32 func__printmode(int32 i,int32 passed){
    if (new_error) return 0;
    if (passed){
      if (i>=0){//validate i
    validatepage(i); i=page[i];
      }else{
    i=-i; if (i>=nextimg){error(258); return 0;} if (!img[i].valid){error(258); return 0;}
      }
    }else{
      i=write_page_index;
    }
    return img[i].print_mode;
  }


  uint32 matchcol(int32 r,int32 g,int32 b){
    static int32 v,v2,n,n2,best,c;
    static int32 *p;
    p=(int32*)write_page->pal;
    if (write_page->text) n2=16; else n2=write_page->mask+1;
    v=1000;
    best=0;
    for (n=0;n<n2;n++){
      c=*p++;
      v2=abs(b-(c&0xFF))+abs(g-(c>>8&0xFF))+abs(r-(c>>16&0xFF));
      if (v2<v){
    if (!v2) return n;//perfect match
    v=v2;
    best=n;
      }
    }//n
    return best;
  }

  uint32 matchcol(int32 r,int32 g,int32 b,int32 i){
    static int32 v,v2,n,n2,best,c;
    static int32 *p;
    p=(int32*)img[i].pal;
    if (img[i].text) n2=16; else n2=img[i].mask+1;
    v=1000;
    best=0;
    for (n=0;n<n2;n++){
      c=*p++;
      v2=abs(b-(c&0xFF))+abs(g-(c>>8&0xFF))+abs(r-(c>>16&0xFF));
      if (v2<v){
    if (!v2) return n;//perfect match
    v=v2;
    best=n;
      }
    }//n
    return best;
  }

  uint32 func__rgb(int32 r,int32 g,int32 b,int32 i,int32 passed){
    if (new_error) return 0;
    if (r<0) r=0;
    if (r>255) r=255;
    if (g<0) g=0;
    if (g>255) g=255;
    if (b<0) b=0;
    if (b>255) b=255;
    if (passed){
      if (i>=0){//validate i
    validatepage(i); i=page[i];
      }else{
    i=-i; if (i>=nextimg){error(258); return 0;} if (!img[i].valid){error(258); return 0;}
      }
      if (img[i].bytes_per_pixel==4){
    return (r<<16)+(g<<8)+b|0xFF000000;
      }else{//==4
    return matchcol(r,g,b,i);
      }//==4
    }else{
      if (write_page->bytes_per_pixel==4){
    return (r<<16)+(g<<8)+b|0xFF000000;
      }else{//==4
    return matchcol(r,g,b);
      }//==4
    }//passed
  }//rgb

  uint32 func__rgba(int32 r,int32 g,int32 b,int32 a,int32 i,int32 passed){
    if (new_error) return 0;
    if (r<0) r=0;
    if (r>255) r=255;
    if (g<0) g=0;
    if (g>255) g=255;
    if (b<0) b=0;
    if (b>255) b=255;
    if (a<0) a=0;
    if (a>255) a=255;
    if (passed){
      if (i>=0){//validate i
    validatepage(i); i=page[i];
      }else{
    i=-i; if (i>=nextimg){error(258); return 0;} if (!img[i].valid){error(258); return 0;}
      }
      if (img[i].bytes_per_pixel==4){
    return (a<<24)+(r<<16)+(g<<8)+b;
      }else{//==4
    //error(5); return 0;
    if ((!a)&&(img[i].transparent_color!=-1)) return img[i].transparent_color;
    return matchcol(r,g,b,i);
      }//==4
    }else{
      if (write_page->bytes_per_pixel==4){
    return (a<<24)+(r<<16)+(g<<8)+b;
      }else{//==4
    //error(5); return 0;
    if ((!a)&&(write_page->transparent_color!=-1)) return write_page->transparent_color;
    return matchcol(r,g,b);
      }//==4
    }//passed
  }//rgba

  int32 func__alpha(uint32 col,int32 i,int32 passed){
    if (new_error) return 0;
    if (passed){
      if (i>=0){//validate i
    validatepage(i); i=page[i];
      }else{
    i=-i; if (i>=nextimg){error(258); return 0;} if (!img[i].valid){error(258); return 0;}
      }
      if (img[i].bytes_per_pixel==4){
    return col>>24;
      }else{//==4
    //error(5); return 0; 
    if ((col<0)||(col>(img[i].mask))){error(5); return 0;} 
    if (img[i].transparent_color==col) return 0;
    return 255;
      }//==4
    }else{
      if (write_page->bytes_per_pixel==4){
    return col>>24;
      }else{//==4
    //error(5); return 0; 
    if ((col<0)||(col>(write_page->mask))){error(5); return 0;} 
    if (write_page->transparent_color==col) return 0;
    return 255;
      }//==4
    }//passed
  }

  int32 func__red(uint32 col,int32 i,int32 passed){
    if (new_error) return 0;
    if (passed){
      if (i>=0){//validate i
    validatepage(i); i=page[i];
      }else{
    i=-i; if (i>=nextimg){error(258); return 0;} if (!img[i].valid){error(258); return 0;}
      }
      if (img[i].bytes_per_pixel==4){
    return col>>16&0xFF;
      }else{//==4
    if ((col<0)||(col>(img[i].mask))){error(5); return 0;}
    return img[i].pal[col]>>16&0xFF;
      }//==4
    }else{
      if (write_page->bytes_per_pixel==4){
    return col>>16&0xFF;
      }else{//==4
    if ((col<0)||(col>(write_page->mask))){error(5); return 0;}
    return write_page->pal[col]>>16&0xFF;
      }//==4
    }//passed
  }

  int32 func__green(uint32 col,int32 i,int32 passed){
    if (new_error) return 0;
    if (passed){
      if (i>=0){//validate i
    validatepage(i); i=page[i];
      }else{
    i=-i; if (i>=nextimg){error(258); return 0;} if (!img[i].valid){error(258); return 0;}
      }
      if (img[i].bytes_per_pixel==4){
    return col>>8&0xFF;
      }else{//==4
    if ((col<0)||(col>(img[i].mask))){error(5); return 0;}
    return img[i].pal[col]>>8&0xFF;
      }//==4
    }else{
      if (write_page->bytes_per_pixel==4){
    return col>>8&0xFF;
      }else{//==4
    if ((col<0)||(col>(write_page->mask))){error(5); return 0;}
    return write_page->pal[col]>>8&0xFF;
      }//==4
    }//passed
  }

  int32 func__blue(uint32 col,int32 i,int32 passed){
    if (new_error) return 0;
    if (passed){
      if (i>=0){//validate i
    validatepage(i); i=page[i];
      }else{
    i=-i; if (i>=nextimg){error(258); return 0;} if (!img[i].valid){error(258); return 0;}

      }
      if (img[i].bytes_per_pixel==4){
    return col&0xFF;
      }else{//==4
    if ((col<0)||(col>(img[i].mask))){error(5); return 0;}
    return img[i].pal[col]&0xFF;
      }//==4
    }else{
      if (write_page->bytes_per_pixel==4){
    return col&0xFF;
      }else{//==4
    if ((col<0)||(col>(write_page->mask))){error(5); return 0;}
    return write_page->pal[col]&0xFF;
      }//==4
    }//passed
  }

  void sub_end(){

    if (sub_gl_called) error(271);

    dont_call_sub_gl=1;

    sub_close(NULL,0);
    exit_blocked=0;//allow exit via X-box or CTRL+BREAK

    if (cloud_app){
      //1. set the display page as the destination page
      sub__dest(func__display());
      //2. VIEW PRINT bottomline,bottomline
      static int32 y;
      if (write_page->text){
    y=write_page->height;
      }else{
    y=write_page->height/fontheight[write_page->font];
      }
      qbg_sub_view_print(y,y,1|2);
      //3. PRINT 'clears the line without having to worry about its contents/size
      qbs_print(nothingstring,1);
      //4. PRINT "Press any key to continue"
      qbs_print(qbs_new_txt("Program ended. Closing (10 seconds)..."),0);
      //6. Enable autodisplay
      autodisplay=1;
      int sec=7;
      while(sec--){
    evnt(1);
    Sleep(1000);
    qbs_print(qbs_new_txt("."),0);
      }
      sec=3;
      while(sec--){
    Sleep(1000);
    evnt(1);
      }

      close_program=1;
      end();
      exit(0);//<-- should never happen
    }

#ifdef DEPENDENCY_CONSOLE_ONLY
        screen_hide=1;
#endif

    if (!screen_hide){
      //1. set the display page as the destination page
      sub__dest(func__display());
      //2. VIEW PRINT bottomline,bottomline
      static int32 y;
      if (write_page->text){
    y=write_page->height;
      }else{
    y=write_page->height/fontheight[write_page->font];
      }
      qbg_sub_view_print(y,y,1|2);
      //3. PRINT 'clears the line without having to worry about its contents/size
      qbs_print(nothingstring,1);
      //4. PRINT "Press any key to continue"
      qbs_print(qbs_new_txt("Press any key to continue"),0);
      //5. Clear any buffered keypresses
      static uint32 qbs_tmp_base;
      qbs_tmp_base=qbs_tmp_list_nexti;
      while(qbs_cleanup(qbs_tmp_base,qbs_notequal(qbs_inkey(),qbs_new_txt("")))){
          Sleep(0);
      }
      //6. Enable autodisplay
      autodisplay=1;
      //7. Wait for a new keypress
      do{
        Sleep(100);
    if (stop_program) end();
    }while(qbs_cleanup(qbs_tmp_base,qbs_equal(qbs_inkey(),qbs_new_txt(""))));


    }else{
      if (console){
    //screen is hidden, console is visible
    cout<<"\nPress enter to continue";
    static int32 ignore;
    ignore=fgetc(stdin);
      }
    }
    close_program=1;
    end();
    exit(0);//<-- should never happen
  }

  uint8 pu_dig[1024];//digits (left justified)
  int32 pu_ndig;//number of digits
  int32 pu_dp;//decimal place modifier
  //note: if dp=0, the number is an integer and can be read as is
  //      if dp=1 the number is itself*10
  //      if dp=-1 the number is itself/10
  int32 pu_neg;
  uint8 pu_buf[1024];//a buffer for preprocessing
  uint8 pu_exp_char=69; //"E"

  int32 print_using(qbs *f, int32 s2, qbs *dest, qbs* pu_str){
    //type: 1=numeric, 2=string
    if (new_error) return 0;

    static int32 x,z,z2,z3,z4,ii;
    //x  - current format string read position
    //z - used as a temp variable for various calculations and loops
    //z2  - used for various calculations involving exponent digits
    //z3 - used as a temp variable for various calculations and loops
    //z4 - number of 0s between . and digits after point
    //ii  - used as a counter for writing the output
    static uint8 c;
    static int32 stage,len,chrsleft,type,s;
    static int32 leading_plus,dollar_sign,asterisk_spaces,digits_before_point,commas;
    static int32 decimal_point,digits_after_point,trailing_plus,exponent_digits, trailing_minus;
    static int32 cant_fit,extra_sign_space,rounded,digits_and_commas_before_point,leading_zero;
    static qbs *qbs1=NULL;

    if (qbs1==NULL) qbs1=qbs_new(1,0);

    if (pu_str) type=2; else type=1;

    s=s2;
    len=f->len;

  scan:
    rounded=0;
  rounded_repass:

    x=s-1; //subtract one to counter pre-increment later

    leading_plus=0; dollar_sign=0; asterisk_spaces=0; digits_before_point=0; commas=0;
    decimal_point=0; digits_after_point=0; trailing_plus=0; exponent_digits=0; trailing_minus=0;
    digits_and_commas_before_point=0; leading_zero=0;
    stage=0;

  nextchar:
    x++;
    if (x<len){
      c=f->chr[x];
      chrsleft=len-x;

      if ((stage>=2)&&(stage<=4)){

    if (c==43){//+
      trailing_plus=1; x++; goto numeric_spacer;
    }

    if (c==45){//-
      trailing_minus=1; x++; goto numeric_spacer;
    }

      }//stage>=2 & stage<=4

      if ((stage>=2)&&(stage<=3)){

    if (chrsleft>=5){
      if ((c==94)&&(f->chr[x+1]==94)&&(f->chr[x+2]==94)&&(f->chr[x+3]==94)&&(f->chr[x+4]==94)){//^^^^^
        exponent_digits=3; stage=4; x+=4; goto nextchar;
      }
    }//5

    if (chrsleft>=4){
      if ((c==94)&&(f->chr[x+1]==94)&&(f->chr[x+2]==94)&&(f->chr[x+3]==94)){//^^^^
        exponent_digits=2; stage=4; x+=3; goto nextchar;
      }
    }//4

      }//stage>=2 & stage<=3

      if (stage==3){

    if (c==35){//#
      digits_after_point++; goto nextchar;
    }

      }//stage==3

      if (stage==2){

    if (c==44){//,
      commas=1; digits_before_point++; goto nextchar;
    }

      }//stage==2

      if (stage<=2){

    if (c==35){//#
      digits_before_point++; stage=2; goto nextchar;
    }

    if (c==46){//.
      decimal_point=1; stage=3; goto nextchar;
    }

      }//stage<=2

      if (stage<=1){

    if (chrsleft>=3){
      if ((c==42)&&(f->chr[x+1]==42)&&(f->chr[x+2]==36)){//**$
        asterisk_spaces=1; digits_before_point=2; dollar_sign=1; stage=2; x+=2; goto nextchar;
      }
    }//3

    if (chrsleft>=2){
      if ((c==42)&&(f->chr[x+1]==42)){//**
        asterisk_spaces=1; digits_before_point=2; stage=2; x++; goto nextchar;
      }
      if ((c==36)&&(f->chr[x+1]==36)){//$$
        dollar_sign=1; digits_before_point=1; stage=2; x++; goto nextchar;
      }
    }//2

      }//stage 1

      if (stage==0){

    if (c==43){//+
      leading_plus=1; stage=1; goto nextchar;
    }

      }//stage 0

      //spacer/end encountered
    }//x<len
  numeric_spacer:

    //valid numeric format?
    if (stage<=1) goto invalid_numeric_format;
    if ((digits_before_point==0)&&(digits_after_point==0)) goto invalid_numeric_format;

    if (type==0) return s; //s is the beginning of a new format but item has already been added to dest
    if (type==2){//expected string format, not numeric format
      error(13);//type mismatch
      return 0;
    }

    //reduce digits before point appropriatly
    extra_sign_space=0;
    if (exponent_digits){
      if ((leading_plus==0)&&(trailing_plus==0)&&(trailing_minus==0)){
    digits_before_point--;
    if (digits_before_point==-1){
      digits_after_point--; digits_before_point=0;
      if (digits_after_point==0){decimal_point=0; digits_before_point++;}
    }
    extra_sign_space=1;
      }
    }else{
      //the following doesn't occur if using an exponent
      if (pu_neg){
    if ((leading_plus==0)&&(trailing_plus==0)&&(trailing_minus==0)){digits_before_point--; extra_sign_space=1;}
      }
      if (commas){
    digits_and_commas_before_point=digits_before_point;
    ii=digits_before_point/4;//for every 4 digits, one digit will be used up by a comma
    digits_before_point-=ii;
      }
    }

    //'0'->'.0' exception (for when format doesn't allow for any digits_before_point)
    if (digits_before_point==0){//no digits allowed before decimal point
      //note: pu_ndig=256, pu_dp=-255
      if ((pu_ndig+pu_dp)==1){//1 digit exists in front of the decimal point
    if (pu_dig[0]==48){//is it 0?
      pu_dp--;//moves decimal point left one position
    }//0
      }
    }

    //will number fit? if it can't then adjustments will be made
    cant_fit=0;
    if (exponent_digits){
      //give back extra_sign_space?
      if (extra_sign_space){
    if (!pu_neg){
      if (digits_before_point<=0){
        extra_sign_space=0;
        digits_before_point++;//will become 0 or 1
        //force 0 in recovered digit?
        if ((digits_before_point==1)&&(digits_after_point>0)){
          digits_before_point--;
          extra_sign_space=2;//2=put 0 instead of blank space
        }
      }
    }
      }
      if ((digits_before_point==0)&&(digits_after_point==0)){
    cant_fit=1;
    digits_before_point=1;//give back removed (for extra sign space) digit
      }
      //but does the exponent fit?
      z2=pu_ndig+pu_dp-1;//calc exponent of most significant digit
      //1.0  = 0
      //10.0 = 1
      //0.1  = -1
      //calc exponent of format's most significant position
      if (digits_before_point) z3=digits_before_point-1; else z3=-1;
      z=z2-z3;//combine to calculate actual exponent which will be "printed" 
      z3=abs(z);
      z2=sprintf((char*)pu_buf,"%u",z3);//use pu_buf to convert exponent to a string
      if (z2>exponent_digits){cant_fit=1; exponent_digits=z2;}
    }else{
      z2=0;
      z=pu_ndig+pu_dp;//calc number of digits required before decimal places
      if (digits_before_point<z){
    digits_before_point=z; cant_fit=1;
    if (commas) digits_and_commas_before_point=digits_before_point+(digits_before_point-1)/3;
      }
    }



    static int32 buf_size;//buf_size is an estimation of size required
    static uint8 *cp,*buf=NULL;
    static int32 count;
    if (buf) free(buf);
    buf_size=256;//256 bytes to account for calc overflow (such as exponent digits)
    buf_size+=9;//%(1)+-(1)$(1)???.(1)???exponent(5)
    buf_size+=digits_before_point;
    if (commas) buf_size+=((digits_before_point/3)+2);
    buf_size+=digits_after_point;
    buf=(uint8*)malloc(buf_size);
    cp=buf;
    count=0;//char count
    ii=0;

    if (asterisk_spaces) asterisk_spaces=42; else asterisk_spaces=32;//chraracter to fill blanks with

    if (cant_fit) {*cp++=37; count++;}//%

    //leading +/-
    if (leading_plus){
      if (pu_neg) *cp++=45; else *cp++=43;
      count++;
    }

    if (exponent_digits){
      z4=0;
      //add $?
      if (dollar_sign) {*cp++=36; count++;}//$
      //add - sign? (as sign space was not specified)
      if (extra_sign_space){
    if (pu_neg){
      *cp++=45;
    }else{
      if (extra_sign_space==2) *cp++=48; else *cp++=32;
    }
    count++;
      }
      //add digits left of decimal point
      for (z3=0;z3<digits_before_point;z3++){
    if (ii<pu_ndig) *cp++=pu_dig[ii++]; else *cp++=48;
    count++;
      }
      //add decimal point
      if (decimal_point){*cp++=46; count++;}
      //add digits right of decimal point
      for (z3=0;z3<digits_after_point;z3++){
    if (ii<pu_ndig) *cp++=pu_dig[ii++]; else *cp++=48;
    count++;

      }
      //round last digit (requires a repass)
      if (!rounded){
    if (ii<pu_ndig){
      if (pu_dig[ii]>=53){//>="5" (round 5 up)
        z=ii-1;
        //round up pu (by adding 1 from digit at character position z)
        //note: pu_dig is rebuilt one character to the right so highest digit can flow over into next character
        rounded=1;
        memmove(&pu_dig[1],&pu_dig[0],pu_ndig); pu_dig[0]=48; z++;
      puround2:
        pu_dig[z]++;
        if (pu_dig[z]>57) {pu_dig[z]=48; z--; goto puround2;}
        if (pu_dig[0]!=48){//was extra character position necessary?
          pu_ndig++; //note: pu_dp does not require any changes  
        }else{
          memmove(&pu_dig[0],&pu_dig[1],pu_ndig);
        }
        goto rounded_repass;
      }
    }
      }
      //add exponent...
      *cp++=pu_exp_char; count++; //add exponent D/E/F (set and restored by calling function as necessary)
      if (z>=0) {*cp++=43; count++;} else {*cp++=45; count++;} //+/- exponent's sign
      //add exponent's leading 0s (if any)
      for (z3=0;z3<(exponent_digits-z2);z3++){
    *cp++=48; count++;
      }
      //add exponent's value
      for (z3=0;z3<z2;z3++){
    *cp++=pu_buf[z3]; count++;
      }
    }else{
      //"print" everything before the point
      //calculate digit spaces before the point in number
      if (!commas) digits_and_commas_before_point=digits_before_point;
      z=pu_ndig+pu_dp;//num of character whole portion of number requires
      z4=0; if (z<0) z4=-z;//number of 0s between . and digits after point
      if (commas) z=z+(z-1)/3;//including appropriate amount of commas
      if (z<0) z=0;
      z2=digits_and_commas_before_point-z;
      if ((z==0)&&(z2>0)){leading_zero=1; z2--;}//change .1 to 0.1 if possible
      for (z3=0;z3<z2;z3++){*cp++=asterisk_spaces; count++;}
      //add - sign? (as sign space was not specified)
      if (extra_sign_space){*cp++=45; count++;}
      //add $?
      if (dollar_sign){*cp++=36; count++;}//$ 
      //leading 0?
      if (leading_zero){*cp++=48; count++;}//0
      //add digits left of decimal point
      for (z3=0;z3<z;z3++){
    if ((commas!=0)&&(((z-z3)&3)==0)){
      *cp++=44;
    }else{
      if (ii<pu_ndig) *cp++=pu_dig[ii++]; else *cp++=48;
    }
    count++;
      }
      //add decimal point
      if (decimal_point){*cp++=46; count++;}
      //add digits right of decimal point
      for (z3=0;z3<digits_after_point;z3++){
    if (z4){
      z4--;
      *cp++=48;
    }else{
      if (ii<pu_ndig) *cp++=pu_dig[ii++]; else *cp++=48;
    }
    count++;
      }
      //round last digit (requires a repass)
      if (!rounded){
    if (ii<pu_ndig){
      if (pu_dig[ii]>=53){//>="5" (round 5 up)
        z=ii-1;
        //round up pu (by adding 1 from digit at character position z)
        //note: pu_dig is rebuilt one character to the right so highest digit can flow over into next character
        rounded=1;  
        memmove(&pu_dig[1],&pu_dig[0],pu_ndig); pu_dig[0]=48; z++;
      puround1: 
        pu_dig[z]++;
        if (pu_dig[z]>57) {pu_dig[z]=48; z--; goto puround1;}
        if (pu_dig[0]!=48){//was extra character position necessary?
          pu_ndig++; //note: pu_dp does not require any changes  
        }else{
          memmove(&pu_dig[0],&pu_dig[1],pu_ndig);  
        }
        goto rounded_repass;
      } 
    }
      }
    }//exponent_digits

    //add trailing sign?
    //trailing +/-
    if (trailing_plus){
      if (pu_neg) *cp++=45; else *cp++=43;
      count++;
    }
    //trailing -
    if (trailing_minus){
      if (pu_neg) *cp++=45; else *cp++=32;
      count++;
    }

    qbs_set(dest,qbs_add(dest,qbs_new_txt_len((char*)buf,count)));

    s=x;
    type=0;//passed type added
    if (s>=len) return 0;//end of format line encountered and passed item added
    goto scan;

  invalid_numeric_format:
    //string format
    static int32 string_size;

    x=s;
    if (x<len){
      c=f->chr[x];
      string_size=0;//invalid
      if (c==38) string_size=-1; //"&" (all of string)
      if (c==33) string_size=1; //"!" (first character only)
      if (c==92){ //"\" first n characters
    z=1;
    x++;
      get_str_fmt:
    if (x>=len) goto invalid_string_format;
    c=f->chr[x];
    z++;
    if (c==32){x++; goto get_str_fmt;}
    if (c!=92) goto invalid_string_format;
    string_size=z;
      }//c==47
      if (string_size){
    if (type==0) return s; //s is the beginning of a new format but item has already been added to dest
    if (type==1){//expected numeric format, not string format
      error(13);//type mismatch
      return 0;
    }
    if (string_size!=-1){
      s+=string_size;
      for (z=0;z<string_size;z++){
        if (z<pu_str->len) qbs1->chr[0]=pu_str->chr[z]; else qbs1->chr[0]=32;
        qbs_set(dest,qbs_add(dest,qbs1));
      }//z 
    }else{
      qbs_set(dest,qbs_add(dest,pu_str));
      s++;
    }
    type=0;//passed type added
    if (s>=len) return 0;//end of format line encountered and passed item added
    goto scan;
      }//string_size
    }//x<len
  invalid_string_format:

    //add literal?
    if ((f->chr[s]==95)&&(s<(len-1))){//trailing single _ in format is treated as a literal _
      s++;
    }
    //add non-format character
    qbs1->chr[0]=f->chr[s]; qbs_set(dest,qbs_add(dest,qbs1));

    s++;
    if (s>=len){
      s=0;
      if (type==0) return s;//end of format line encountered and passed item added
      //illegal format? (format has been passed from start (s2=0) to end and has no numeric/string identifiers
      if (s2==0){
    error(5);//illegal function call
    return 0;
      }
    }
    goto scan;

    return 0;
  }

  int32 print_using_integer64(qbs* format, int64 value, int32 start, qbs *output){
    if (new_error) return 0;
#ifdef QB64_WINDOWS
    pu_ndig=sprintf((char*)pu_buf,"% I64i",value);
#else
    pu_ndig=sprintf((char*)pu_buf,"% lli",value);
#endif
    if (pu_buf[0]==45) pu_neg=1; else pu_neg=0;
    pu_ndig--;//remove sign
    memcpy(pu_dig,&pu_buf[1],pu_ndig);
    pu_dp=0;
    start=print_using(format,start,output,NULL);
    return start;
  }

  int32 print_using_uinteger64(qbs* format, uint64 value, int32 start, qbs *output){
    if (new_error) return 0;
#ifdef QB64_WINDOWS
    pu_ndig=sprintf((char*)pu_dig,"%I64u",value);
#else
    pu_ndig=sprintf((char*)pu_dig,"%llu",value);
#endif
    pu_neg=0;
    pu_dp=0;
    start=print_using(format,start,output,NULL);
    return start;
  }

  int32 print_using_single(qbs* format, float value, int32 start, qbs *output){
    if (new_error) return 0;
    static int32 i,len,neg_exp;
    static uint8 c;
    static int64 exp;
    len=sprintf((char*)&pu_buf,"% .255E",value);//256 character limit ([1].[255])
    pu_dp=0;
    pu_ndig=0;
    //1. sign
    if (pu_buf[0]==45) pu_neg=1; else pu_neg=0;
    i=1;
    //2. digits before decimal place
  getdigits:
    if (i>=len){error(5); return 0;}
    c=pu_buf[i];
    if ((c>=48)&&(c<=57)){
      pu_dig[pu_ndig++]=c;
      i++;
      goto getdigits;
    }
    //3. decimal place
    if (c!=46){error(5); return 0;}//expected decimal point
    i++;
    //4. digits after decimal place
  getdigits2:
    if (i>=len){error(5); return 0;}
    c=pu_buf[i];
    if ((c>=48)&&(c<=57)){
      pu_dig[pu_ndig++]=c;
      pu_dp--;
      i++;
      goto getdigits2;
    }
    //assume random character signifying an exponent
    i++;
    //optional exponent sign
    neg_exp=0;
    if (i>=len){error(5); return 0;}
    c=pu_buf[i];
    if (c==45){neg_exp=1; i++;}//-
    if (c==43) i++;//+
    //assume remaining characters are an exponent
    exp=0;
  getdigits3:
    if (i<len){
      c=pu_buf[i];
      if ((c<48)||(c>57)){error(5); return 0;}
      exp=exp*10;
      exp=exp+c-48;
      i++;
      goto getdigits3;
    }
    if (neg_exp) exp=-exp;
    pu_dp+=exp;
    start=print_using(format,start,output,NULL);
    return start;
  }

  int32 print_using_double(qbs* format, double value, int32 start, qbs *output){
    if (new_error) return 0;
    static int32 i,len,neg_exp;
    static uint8 c;
    static int64 exp;
    len=sprintf((char*)&pu_buf,"% .255E",value);//256 character limit ([1].[255])
    pu_dp=0;
    pu_ndig=0;
    //1. sign
    if (pu_buf[0]==45) pu_neg=1; else pu_neg=0;
    i=1;
    //2. digits before decimal place
  getdigits:
    if (i>=len){error(5); return 0;}
    c=pu_buf[i];
    if ((c>=48)&&(c<=57)){
      pu_dig[pu_ndig++]=c;
      i++;
      goto getdigits;
    }
    //3. decimal place
    if (c!=46){error(5); return 0;}//expected decimal point
    i++;
    //4. digits after decimal place
  getdigits2:
    if (i>=len){error(5); return 0;}
    c=pu_buf[i];
    if ((c>=48)&&(c<=57)){
      pu_dig[pu_ndig++]=c;
      pu_dp--;
      i++;
      goto getdigits2;
    }
    //assume random character signifying an exponent
    i++;
    //optional exponent sign
    neg_exp=0;
    if (i>=len){error(5); return 0;}
    c=pu_buf[i];
    if (c==45){neg_exp=1; i++;}//-
    if (c==43) i++;//+
    //assume remaining characters are an exponent
    exp=0;
  getdigits3:
    if (i<len){
      c=pu_buf[i];
      if ((c<48)||(c>57)){error(5); return 0;}
      exp=exp*10;
      exp=exp+c-48;
      i++;
      goto getdigits3;
    }
    if (neg_exp) exp=-exp;
    pu_dp+=exp;
    pu_exp_char=68; //"D"
    start=print_using(format,start,output,NULL);
    pu_exp_char=69; //"E"
    return start;
  }

  //WARNING: DUE TO MINGW NOT SUPPORTING PRINTF long double, VALUES ARE CONVERTED TO A DOUBLE
  //         BUT PRINTED AS IF THEY WERE A long double
  int32 print_using_float(qbs* format, long double value, int32 start, qbs *output){
    if (new_error) return 0;
    static int32 i,len,neg_exp;
    static uint8 c;
    static int64 exp;
    //len=sprintf((char*)&pu_buf,"% .255E",value);//256 character limit ([1].[255])
#ifdef QB64_MINGW
    len=__mingw_sprintf((char*)&pu_buf,"% .255Lf",value);//256 character limit ([1].[255])
#else 
    len=sprintf((char*)&pu_buf,"% .255Lf",value);//256 character limit ([1].[255])
#endif

    //qbs_print(qbs_new_txt((char*)&pu_buf),1);


    pu_dp=0;
    pu_ndig=0;
    //1. sign
    if (pu_buf[0]==45) pu_neg=1; else pu_neg=0;
    i=1;
    //2. digits before decimal place
  getdigits:
    if (i>=len){error(5); return 0;}
    c=pu_buf[i];
    if ((c>=48)&&(c<=57)){
      pu_dig[pu_ndig++]=c;
      i++;
      goto getdigits;
    }
    //3. decimal place
    if (c!=46){error(5); return 0;}//expected decimal point
    i++;
    //4. digits after decimal place
  getdigits2:
    if (i>=len){
      //no exponent information has been provided
      neg_exp=0;
      exp=0;
      goto no_exponent_provided;
      //error(5); return 0;
    }
    c=pu_buf[i];
    if ((c>=48)&&(c<=57)){
      pu_dig[pu_ndig++]=c;
      pu_dp--;
      i++;
      goto getdigits2;
    }
    //assume random character signifying an exponent
    i++;
    //optional exponent sign
    neg_exp=0;
    if (i>=len){error(5); return 0;}
    c=pu_buf[i];
    if (c==45){neg_exp=1; i++;}//-
    if (c==43) i++;//+
    //assume remaining characters are an exponent
    exp=0;
  getdigits3:
    if (i<len){
      c=pu_buf[i];
      if ((c<48)||(c>57)){error(5); return 0;}
      exp=exp*10;
      exp=exp+c-48;
      i++;
      goto getdigits3;
    }
    if (neg_exp) exp=-exp;
    pu_dp+=exp;
  no_exponent_provided:
    pu_exp_char=70; //"F"
    start=print_using(format,start,output,NULL);
    pu_exp_char=69; //"E"
    return start;
  }

  void sub_run_init(){
    //Reset ON KEY trapping
    //note: KEY bar F-key bindings are not affected
    static int32 i;
    for (i=1;i<=31;i++){
      onkey[i].id=0;
      onkey[i].active=0;
      onkey[i].state=0;
    }
    onkey_inprogress=0;
    //note: if already in screen 0:80x25, screen pages are left intact
    //set screen mode to 0 (80x25)
    qbg_screen(0,NULL,0,0,NULL,1|4|8);
    //make sure WIDTH is 80x25
    qbsub_width(NULL,80,25,1|2);
    //restore palette
    restorepalette(write_page);
    //restore default colors
    write_page->background_color=0;
    write_page->color=7;
    //note: cursor state does not appear to be reset by the RUN command
    //im->cursor_show=0; im->cursor_firstvalue=4; im->cursor_lastvalue=4;
    //Reset RND & RANDOMIZE state
    rnd_seed=327680;
    rnd_seed_first=327680;//Note: must contain the same value as rnd_seed
  }



  void sub_run(qbs* f){
    if (new_error) return;
    if (cloud_app){error(262); return;}

    //run program
    static qbs *str=NULL;
    if (str==NULL) str=qbs_new(0,0);
    static qbs *strz=NULL;
    if (!strz) strz=qbs_new(0,0);

    qbs_set(str,f);
    fixdir(str);

#ifdef QB64_WINDOWS

    qbs_set(strz,qbs_add(str,qbs_new_txt_len("\0",1)));
    if (WinExec((char *)strz->chr,SW_SHOWDEFAULT)>31){
      goto run_exit;
    }else{
      //0-out of resources/memory
      //ERROR_BAD_FORMAT
      //ERROR_FILE_NOT_FOUND
      //ERROR_PATH_NOT_FOUND
      error(53); return;//file not found
    }

#else
    qbs_set(strz,qbs_add(str,qbs_new_txt_len("\0",1)));
    system((char*)strz->chr);
    //success?
    goto run_exit;

#endif

    //exit this program
  run_exit:
    close_program=1;
    end();
    exit(99);//<--this line should never actually be executed

  }


  int32 func_screenwidth () {
  #ifdef QB64_GLUT
    while (!window_exists){Sleep(100);}
      #ifdef QB64_WINDOWS
          while (!window_handle){Sleep(100);}
      #endif
      return glutGet(GLUT_SCREEN_WIDTH);
  #else
    return 0;
  #endif
  }

  int32 func_screenheight () {
  #ifdef QB64_GLUT
    while (!window_exists){Sleep(100);}
      #ifdef QB64_WINDOWS
          while (!window_handle){Sleep(100);}
      #endif
      return glutGet(GLUT_SCREEN_HEIGHT);
  #else
      return 0;
  #endif
  }

  void sub_screenicon () {
  #ifdef QB64_GLUT
    while (!window_exists){Sleep(100);}
      #ifdef QB64_WINDOWS
          while (!window_handle){Sleep(100);}
      #endif  
      glutIconifyWindow();
    return;
  #endif
  }

  int32 func_windowexists () {
  #ifdef QB64_GLUT
      #ifdef QB64_WINDOWS
          if (!window_handle){return 0;}
      #endif
    return -window_exists;
  #else
    return -1;
  #endif
  }

  int32 func_screenicon () {
  #ifdef QB64_GLUT
      while (!window_exists){Sleep(100);}
      #ifdef QB64_WINDOWS
          while (!window_handle){Sleep(100);}
      #endif
      extern int32 screen_hide;
      if (screen_hide) {error(5); return 0;}
          #ifdef QB64_WINDOWS
                return -IsIconic(window_handle);
          #else
            /*     
             Linux code not compiling for now
             #include <X11/X.h>
             #include <X11/Xlib.h>
             extern Display *X11_display;
             extern Window X11_window;
             extern int32 screen_hide;
             XWindowAttributes attribs;
             while (!(X11_display && X11_window));
             XGetWindowAttributes(X11_display, X11_window, &attribs);
             if (attribs.map_state == IsUnmapped) return -1;
             return 0;
             #endif */
           return 0; //if we get here and haven't exited already, we failed somewhere along the way.
        #endif
  #endif
  }

  int32 func__autodisplay () {
      if (autodisplay) {return -1;}
      return 0;
  }
  void sub__autodisplay(){
    autodisplay=1;
  }

  void sub__display(){

    if (cloud_app==0){
      if (screen_hide) return;
    }

    //disable autodisplay (if enabled)
    if (autodisplay){
      autodisplay=-1;//toggle request
      while(autodisplay) Sleep(1);
      return;//note: autodisplay is set to 0 after display() has been called so a second call to display() is unnecessary
    }
    display();
  }

  int32 sub_draw_i;
  uint8 *sub_draw_cp;
  int32 sub_draw_len;

  int32 draw_num_invalid;
  int32 draw_num_undefined;
  double draw_num(){
    static int32 c,dp,vptr,x,offset;
    static double d,dp_mult,sgn;

    draw_num_invalid=0;
    draw_num_undefined=1;
    d=0;
    dp=0;
    sgn=1;
    vptr=0;

  nextchar:
    if (sub_draw_i>=sub_draw_len) return d*sgn;
    c=sub_draw_cp[sub_draw_i];

    if (vptr){
      if ((sub_draw_i+2)>=sub_draw_len) {draw_num_invalid=1; return 0;}//not enough data!
      offset=sub_draw_cp[sub_draw_i+2]*256+sub_draw_cp[sub_draw_i+1];
      sub_draw_i+=3;
      vptr=0;
      /*
    'BYTE=1
    'INTEGER=2
    'STRING=3 (unsupported)
    'SINGLE=4
    'INT64=5
    'FLOAT=6
    'DOUBLE=8
    'LONG=20
    'BIT=64+n (unsupported)
      */
      if (c==1){d=*((int8*)(&cmem[1280+offset])); goto nextcharv;}
      if (c==(1+128)){d=*((uint8*)(&cmem[1280+offset])); goto nextcharv;}
      if (c==2){d=*((int16*)(&cmem[1280+offset])); goto nextcharv;}
      if (c==(2+128)){d=*((uint16*)(&cmem[1280+offset])); goto nextcharv;}
      if (c==4){d=*((float*)(&cmem[1280+offset])); goto nextcharv;}
      if (c==5){d=*((int64*)(&cmem[1280+offset])); goto nextcharv;}
      if (c==(5+128)){d=*((uint64*)(&cmem[1280+offset])); goto nextcharv;}
      if (c==6){d=*((long double*)(&cmem[1280+offset])); goto nextcharv;}
      if (c==8){d=*((double*)(&cmem[1280+offset])); goto nextcharv;}
      if (c==20){d=*((int32*)(&cmem[1280+offset])); goto nextcharv;}
      if (c==(20+128)){d=*((uint32*)(&cmem[1280+offset])); goto nextcharv;}
      //unknown/unsupported types(bit/string) return an error
      draw_num_invalid=1; return 0;
    nextcharv:
      draw_num_invalid=0;
      draw_num_undefined=0;
      return d;
    }

    if ((c==32)||(c==9)){sub_draw_i++; goto nextchar;}//skip whitespace

    if ((c>=48)&&(c<=57)){
      c-=48;
      if (dp){
    d+=(((double)c)*dp_mult);
    dp_mult/=10.0;
      }else{
    d=(d*10)+c;
      }
      draw_num_undefined=0;
      draw_num_invalid=0;
      sub_draw_i++; goto nextchar;
    }

    if (c==45){//-
      if (dp||(!draw_num_undefined)) return d*sgn;
      sgn=-sgn;
      draw_num_invalid=1;
      sub_draw_i++; goto nextchar;
    }

    if (c==43){//+
      if (dp||(!draw_num_undefined)) return d*sgn;
      draw_num_invalid=1;
      sub_draw_i++; goto nextchar;
    }

    if (c==46){//.
      if (dp) return d*sgn;
      dp=1; dp_mult=0.1;
      if (!draw_num_undefined) draw_num_invalid=1;
      sub_draw_i++; goto nextchar;
    }

    if (c==61){//=
      if (draw_num_invalid||dp||(!draw_num_undefined)){draw_num_invalid=1; return 0;}//leading data invalid!
      vptr=1;
      sub_draw_i++; goto nextchar;
    }

    return d*sgn;
  }

  void sub_draw(qbs* s){
    if (new_error) return;

    /*

      Aspect ratio determination:
      32/256 modes always assume 1:1 ratio
      All other modes (1-13) determine their aspect ratio from the destination surface's dimensions (presuming it is stretched onto a 4:3 ratio monitor)

      Reference:
      Line-drawing and cursor-movement commands:
      D[n%]            Moves cursor down n% units.
      E[n%]            Moves cursor up and right n% units.
      F[n%]            Moves cursor down and right n% units.
      G[n%]            Moves cursor down and left n% units.
      H[n%]            Moves cursor up and left n% units.
      L[n%]            Moves cursor left n% units.
      M[{+|-}]x%,y%    Moves cursor to point x%,y%. If x% is preceded
      by + or -, moves relative to the current point.
      -+/- relative ONLY if after the M, after comma doesn't affect method
      -nothing to do with VIEW/WINDOW coordinates (but still clipped)
      R[n%]            Moves cursor right n% units.
      U[n%]            Moves cursor up n% units.
      [B]              Optional prefix that moves cursor without drawing.
      [N]              Optional prefix that draws and returns cursor to
      its original position.
      *Prefixes B&N can be used anywhere. They set (not toggle) their respective states. They are only cleared if they are used in a statement. They are forgotten when a new DRAW statement is called.
      Color, rotation, and scale commands:
      An%              Rotates an object n% * 90 degrees (n% can be 0, 1,
      2, or 3).
      Cn%              Sets the drawing color (n% is a color attribute).

      Pn1%,n2%         Sets the paint fill and border colors of an object
      (n1% is the fill-color attribute, n2% is the
      border-color attribute).
      Sn%              Determines the drawing scale by setting the length
      of a unit of cursor movement. The default n% is 4,
      which is equivalent to 1 pixel.
      TAn%             Turns an angle n% degrees (-360 through 360).

      -If you omit n% from line-drawing and cursor-movement commands, the
      cursor moves 1 unit.
      -To execute a DRAW command substring from a DRAW command string, use
      the "X" command:
      DRAW "X"+ VARPTR$(commandstring$)
    */

    static double r,ir,vx,vy,hx,hy,ex,ey,fx,fy,xx,yy,px,py,px2,py2,d,d2,sin_ta,cos_ta;
    static int64 c64,c64b,c64c;
    static uint32 col;
    static int32 x,c,prefix_b,prefix_n,offset;
    static uint8 *stack_s[8192];
    static uint16 stack_len[8192];
    static uint16 stack_i[8192];
    static int32 stacksize;
    static double draw_ta;
    static double draw_scale;

    if (write_page->text){error(5); return;}

    draw_ta=write_page->draw_ta; draw_scale=write_page->draw_scale;

    if (write_page->compatible_mode<=13){
      if (write_page->compatible_mode==1) r=4.0/((3.0/200.0)*320.0);
      if (write_page->compatible_mode==2) r=4.0/((3.0/200.0)*640.0);
      if (write_page->compatible_mode==7) r=4.0/((3.0/200.0)*320.0);
      if (write_page->compatible_mode==8) r=4.0/((3.0/200.0)*640.0);
      if (write_page->compatible_mode==9) r=4.0/((3.0/350.0)*640.0);
      if (write_page->compatible_mode==10) r=4.0/((3.0/350.0)*640.0);
      if (write_page->compatible_mode==11) r=4.0/((3.0/480.0)*640.0); 
      if (write_page->compatible_mode==12) r=4.0/((3.0/480.0)*640.0);
      if (write_page->compatible_mode==13) r=4.0/((3.0/200.0)*320.0);
      //Old method: r=4.0 /( (3.0/((double)write_page->height)) * ((double)write_page->width) ); //calculate aspect ratio of image
      ir=1/r; //note: all drawing must multiply the x offset by ir (inverse ratio)
    }else{
      r=1;
      ir=1;
    }



    vx=0; vy=-1; ex=r; ey=-1; hx=r; hy=0; fx=r; fy=1;//reset vectors
    //rotate vectors by ta?
    if (draw_ta){
      d=draw_ta*0.0174532925199433; sin_ta=sin(d); cos_ta=cos(d);
      px2=vx;
      py2=vy;
      vx=px2*cos_ta+py2*sin_ta;
      vy=py2*cos_ta-px2*sin_ta;
      px2=hx;
      py2=hy;
      hx=px2*cos_ta+py2*sin_ta;
      hy=py2*cos_ta-px2*sin_ta;
      px2=ex;
      py2=ey;
      ex=px2*cos_ta+py2*sin_ta;
      ey=py2*cos_ta-px2*sin_ta;
      px2=fx;
      py2=fy;
      fx=px2*cos_ta+py2*sin_ta;
      fy=py2*cos_ta-px2*sin_ta;
    }

    //convert x,y image position into a pixel coordinate
    if (write_page->clipping_or_scaling){
      if (write_page->clipping_or_scaling==2){
    px=write_page->x*write_page->scaling_x+write_page->scaling_offset_x+write_page->view_offset_x;
    py=write_page->y*write_page->scaling_y+write_page->scaling_offset_y+write_page->view_offset_y;
      }else{
    px=write_page->x+write_page->view_offset_x;
    py=write_page->y+write_page->view_offset_y;
      }
    }else{
      px=write_page->x;
      py=write_page->y;
    }

    col=write_page->draw_color;
    prefix_b=0; prefix_n=0;

    stacksize=0;

    sub_draw_cp=s->chr;
    sub_draw_len=s->len;
    sub_draw_i=0;

  nextchar:
    if (sub_draw_i>=sub_draw_len){

      //revert from X-stack
      if (stacksize){
    stacksize--; sub_draw_cp=stack_s[stacksize]; sub_draw_len=stack_len[stacksize]; sub_draw_i=stack_i[stacksize];//restore state
    //continue
    goto nextchar;
      }

      //revert px,py to image->x,y offsets
      if (write_page->clipping_or_scaling){
    if (write_page->clipping_or_scaling==2){
      px=(px-write_page->view_offset_x-write_page->scaling_offset_x)/write_page->scaling_x;
      py=(py-write_page->view_offset_y-write_page->scaling_offset_y)/write_page->scaling_y;
    }else{
      px=px-write_page->view_offset_x;
      py=py-write_page->view_offset_y;
    }
      }
      write_page->x=px; write_page->y=py;
      return;
    }
    c=sub_draw_cp[sub_draw_i];

    if ((c>=97)&&(c<=122)) c-=32;//ucase c

    if (c==77){//M
    m_nextchar:
      sub_draw_i++; if (sub_draw_i>=sub_draw_len){error(5); return;}
      c=sub_draw_cp[sub_draw_i];
      if ((c==32)||(c==9)) goto m_nextchar;//skip whitespace
      //check for absolute/relative positioning
      if ((c==43)||(c==45)) x=1; else x=0;
      px2=draw_num();
      if (draw_num_invalid||draw_num_undefined){error(5); return;}
      c=sub_draw_cp[sub_draw_i];
      if (c!=44){error(5); return;}//expected ,
      sub_draw_i++;
      py2=draw_num();
      if (draw_num_invalid||draw_num_undefined){error(5); return;}
      if (x){//relative positioning
    xx=(px2*ir)*hx-(py2*ir)*vx; yy=px2*hy-py2*vy; px2=px+xx*draw_scale; py2=py+yy*draw_scale;
      }
      if (!prefix_b) fast_line(qbr(px),qbr(py),qbr(px2),qbr(py2),col);
      if (!prefix_n){px=px2; py=py2;}//update position
      prefix_b=0; prefix_n=0;
      goto nextchar;
    }

    if (c==84){//T(A)
    ta_nextchar:
      sub_draw_i++; if (sub_draw_i>=sub_draw_len){error(5); return;}
      c=sub_draw_cp[sub_draw_i];
      if ((c==32)||(c==9)) goto ta_nextchar;//skip whitespace
      if ((c!=65)&&(c!=97)){error(5); return;}//not TA
      sub_draw_i++;
      d=draw_num();
      if (draw_num_invalid||draw_num_undefined){error(5); return;}
      draw_ta=d;
      write_page->draw_ta=draw_ta;
    ta_entry:
      //note: ta rotation is not relative to previous angle
      vx=0; vy=-1; ex=r; ey=-1; hx=r; hy=0; fx=r; fy=1;//reset vectors
      //rotate vectors by ta
      d=draw_ta*0.0174532925199433; sin_ta=sin(d); cos_ta=cos(d);
      px2=vx;
      py2=vy;
      vx=px2*cos_ta+py2*sin_ta;
      vy=py2*cos_ta-px2*sin_ta;
      px2=hx;
      py2=hy;
      hx=px2*cos_ta+py2*sin_ta;
      hy=py2*cos_ta-px2*sin_ta;
      px2=ex;
      py2=ey;
      ex=px2*cos_ta+py2*sin_ta;
      ey=py2*cos_ta-px2*sin_ta;
      px2=fx;
      py2=fy;
      fx=px2*cos_ta+py2*sin_ta;
      fy=py2*cos_ta-px2*sin_ta;
      goto nextchar;
    }

    if (c==85){xx=vx; yy=vy; goto udlr;}//U
    if (c==68){xx=-vx; yy=-vy; goto udlr;}//D
    if (c==76){xx=-hx; yy=-hy; goto udlr;}//L
    if (c==82){xx=hx; yy=hy; goto udlr;}//R

    if (c==69){xx=ex; yy=ey; goto udlr;}//E
    if (c==70){xx=fx; yy=fy; goto udlr;}//F
    if (c==71){xx=-ex; yy=-ey; goto udlr;}//G
    if (c==72){xx=-fx; yy=-fy; goto udlr;}//H

    if (c==67){//C
      sub_draw_i++;
      d=draw_num();
      if (draw_num_invalid||draw_num_undefined){error(5); return;}
      c64=d; xx=c64; if (xx!=d){error(5); return;}//non-integer
      //if (c64<0){error(5); return;}
      //c64b=1; c64b<<=write_page->bits_per_pixel; c64b--;
      //if (c64>c64b){error(5); return;}
      col=c64;
      write_page->draw_color=col;
      goto nextchar;
    }

    if (c==66){//B (move without drawing prefix)
      prefix_b=1;
      sub_draw_i++;
      goto nextchar;
    }

    if (c==78){//N (draw without moving)
      prefix_n=1;
      sub_draw_i++;
      goto nextchar;
    }

    if (c==83){//S
      sub_draw_i++;
      d=draw_num();
      if (draw_num_invalid||draw_num_undefined){error(5); return;}
      if (d<0){error(5); return;}
      draw_scale=d/4.0;
      write_page->draw_scale=draw_scale;
      goto nextchar;
    }

    if (c==80){//P
      sub_draw_i++;
      d=draw_num();
      if (draw_num_invalid||draw_num_undefined){error(5); return;}
      c64=d; xx=c64; if (xx!=d){error(5); return;}//non-integer
      //if (c64<0){error(5); return;}
      //c64b=1; c64b<<=write_page->bits_per_pixel; c64b--;
      //if (c64>c64b){error(5); return;}
      c64c=c64;
      c=sub_draw_cp[sub_draw_i];
      if (c!=44){error(5); return;}//expected ,
      sub_draw_i++;
      d=draw_num();
      if (draw_num_invalid||draw_num_undefined){error(5); return;}
      c64=d; xx=c64; if (xx!=d){error(5); return;}//non-integer
      //if (c64<0){error(5); return;}
      //c64b=1; c64b<<=write_page->bits_per_pixel; c64b--;
      //if (c64>c64b){error(5); return;}
      //revert px,py to x,y offsets
      if (write_page->clipping_or_scaling){
    if (write_page->clipping_or_scaling==2){
      xx=(px-write_page->view_offset_x-write_page->scaling_offset_x)/write_page->scaling_x;
      yy=(py-write_page->view_offset_y-write_page->scaling_offset_y)/write_page->scaling_y;
    }else{
      xx=px-write_page->view_offset_x;
      yy=py-write_page->view_offset_y;
    }
      }else{
    xx=px;
    yy=py;
      }
      sub_paint(xx,yy,c64c,c64,NULL,2+4);
      col=c64c;
      goto nextchar;
    }

    if (c==65){//A
      sub_draw_i++;
      d=draw_num();
      if (draw_num_invalid||draw_num_undefined){error(5); return;}
      if (d==0){draw_ta=0; write_page->draw_ta=draw_ta; goto ta_entry;}
      if (d==1){draw_ta=90; write_page->draw_ta=draw_ta; goto ta_entry;}
      if (d==2){draw_ta=180; write_page->draw_ta=draw_ta; goto ta_entry;}
      if (d==3){draw_ta=270; write_page->draw_ta=draw_ta; goto ta_entry;}
      error(5); return;//invalid value
    }

    if (c==88){//X
      sub_draw_i++;
      if ((sub_draw_i+2)>=sub_draw_len){error(5); return;}
      if (sub_draw_cp[sub_draw_i]!=3){error(5); return;}
      offset=sub_draw_cp[sub_draw_i+2]*256+sub_draw_cp[sub_draw_i+1];//offset of string descriptor in DBLOCK
      sub_draw_i+=3;
      if (stacksize==8192){error(6); return;}//X-stack "OVERFLOW" (should never occur because DBLOCK will overflow first)
      stack_s[stacksize]=sub_draw_cp; stack_len[stacksize]=sub_draw_len; stack_i[stacksize]=sub_draw_i; stacksize++;//backup state
      //set new state
      sub_draw_i=0;
      x=cmem[1280+offset+3]*256+cmem[1280+offset+2];
      sub_draw_cp=&cmem[1280]+x;
      sub_draw_len=cmem[1280+offset+1]*256+cmem[1280+offset+0];
      //continue processing
      goto nextchar;
    }

    if ((c==32)||(c==9)||(c==59)){sub_draw_i++; goto nextchar;}//skip whitespace/semicolons

    error(5); return;//unknown command encountered!




  udlr:
    sub_draw_i++;
    d=draw_num();
    if (draw_num_invalid){error(5); return;}
    if (draw_num_undefined) d=1;
    xx*=d; yy*=d;
    //***apply scaling here***
    xx=xx*ir;
    px2=px+xx*draw_scale; py2=py+yy*draw_scale;
    if (!prefix_b) fast_line(qbr(px),qbr(py),qbr(px2),qbr(py2),col);
    if (!prefix_n){px=px2; py=py2;}//update position
    prefix_b=0; prefix_n=0;
    goto nextchar;
  }



#ifdef QB64_UNIX
  extern char** environ;
#define envp environ
#else /* WINDOWS */
 #define envp _environ
#endif
  size_t environ_count;

  qbs *func_environ(qbs *name)
  {
    static char *cp;
    static qbs *tqbs;
    static int32 bytes;
    cp=getenv((char*)name->chr);
    if (cp&&(cloud_app==0)){
      bytes=strlen(cp);
      tqbs=qbs_new(bytes,1);
      memcpy(tqbs->chr,cp,bytes); 
    }else{
      tqbs=qbs_new(0,1);
    }
    return tqbs;
  }

  qbs *func_environ(int32 number)
    {
      static qbs *tqbs;
      static char *cp;
      static int32 bytes;
      if (cloud_app){tqbs=qbs_new(0,1); error(5); return tqbs;}
      if (number<=0){tqbs=qbs_new(0,1); error(5); return tqbs;}
      if (number>=environ_count){tqbs=qbs_new(0,1); return tqbs;}
      cp=*(envp+number-1);
      bytes=strlen(cp);
      tqbs=qbs_new(bytes,1);
      memcpy(tqbs->chr,cp,bytes);
      return tqbs;
    }

  void sub_environ(qbs *str)
  {
    if (cloud_app){error(262); return;}
    static char *cp;
    cp=(char*)malloc(str->len+1);
    cp[str->len]=0;//add NULL terminator
    memcpy(cp,str->chr,str->len);
    putenv(cp);
    free(cp);
    environ_count++;
  }

#ifdef QB64_WINDOWS
  void showvalue(__int64 v){
    static qbs* s=NULL;
    if (s==NULL) s=qbs_new(0,0);
    qbs_set(s,qbs_str(v));
    MessageBox2(NULL,(char*)s->chr,"showvalue",MB_OK|MB_SYSTEMMODAL);
  }
#endif











  //Referenced: http://johnnie.jerrata.com/winsocktutorial/
  //Much of the unix sockets code based on http://beej.us/guide/bgnet/
#ifdef QB64_WINDOWS
#include <winsock2.h>
  WSADATA wsaData;
  WORD sockVersion;
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#endif



#define NETWORK_ERROR -1
#define NETWORK_OK     0

  void tcp_init(){
    static int32 init=0;
    if (!init){
      init=1;
#if !defined(DEPENDENCY_SOCKETS)
#elif defined(QB64_WINDOWS)
      sockVersion = MAKEWORD(1, 1);
      WSAStartup(sockVersion, &wsaData);
#endif
    }
  }

  void tcp_done(){
#if !defined(DEPENDENCY_SOCKETS)
#elif defined(QB64_WINDOWS)
    WSACleanup();
#endif
  }

  struct tcp_connection{
#if !defined(DEPENDENCY_SOCKETS)
#elif defined(QB64_WINDOWS)
    SOCKET socket;
#elif defined(QB64_UNIX)
    int socket;
#else
#endif
    int32 port;//connection to host & clients only
    uint8 ip4[4];//connection to host only
    uint8* hostname;//clients only
    int connected;
  };

  void *tcp_host_open(int64 port){
    tcp_init();
    if ((port<0)||(port>65535)) return NULL;
#if !defined(DEPENDENCY_SOCKETS)
    return NULL;
#elif defined(QB64_WINDOWS)
    //Ref. from 'winsock.h': typedef u_int SOCKET;
    static SOCKET listeningSocket;
    listeningSocket = socket(AF_INET,       // Go over TCP/IP
                 SOCK_STREAM,       // This is a stream-oriented socket
                 IPPROTO_TCP);      // Use TCP rather than UDP
    if (listeningSocket==INVALID_SOCKET) return NULL;
    static SOCKADDR_IN serverInfo;
    serverInfo.sin_family=AF_INET;
    serverInfo.sin_addr.s_addr=INADDR_ANY;  // Since this socket is listening for connections,
    // any local address will do
    serverInfo.sin_port=htons(port);    // Convert integer port to network-byte order
    // and insert into the port field
    // Bind the socket to our local server address
    static int nret;
    nret=bind(listeningSocket,(LPSOCKADDR)&serverInfo,sizeof(struct sockaddr));
    if (nret==SOCKET_ERROR){closesocket(listeningSocket); return NULL;}
    nret=listen(listeningSocket,SOMAXCONN);     // Up to x connections may wait at any
    // one time to be accept()'ed
    if (nret==SOCKET_ERROR){closesocket(listeningSocket); return NULL;}
    static u_long iMode;
    iMode=1;
    ioctlsocket(listeningSocket, FIONBIO,&iMode);

    static tcp_connection *connection;
    connection=(tcp_connection*)calloc(sizeof(tcp_connection),1);
    connection->socket=listeningSocket;
    connection->connected = -1;
    return (void*)connection;
#elif defined(QB64_UNIX)
    struct addrinfo hints, *servinfo, *p;
    int sockfd;
    char str_port[6];
    int yes = 1;
    snprintf(str_port, 6, "%d", port);
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    if (getaddrinfo(NULL, str_port, &hints, &servinfo) != 0) return NULL;
    for(p = servinfo; p != NULL; p = p->ai_next) {
      sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
      if (sockfd == -1) continue;
      setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
      if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            continue;
      }
      break; //if we get here, all is good
    }
    freeaddrinfo(servinfo);
    if (p == NULL) return NULL; //indicates none of the entries succeeded
    fcntl(sockfd, F_SETFL, O_NONBLOCK); //make socket non-blocking
    
    if (listen(sockfd, SOMAXCONN) == -1) {
      close(sockfd);
      return NULL;
    }    

    tcp_connection *connection;
    connection=(tcp_connection*)calloc(sizeof(tcp_connection),1);
    connection->socket=sockfd;
    connection->connected = -1;
    return (void*)connection;
#else
    return NULL;
#endif
  }









  void *tcp_client_open(uint8 *host,int64 port){

    tcp_init();

    if ((port<0)||(port>65535)) return NULL;
#if !defined(DEPENDENCY_SOCKETS)
    return NULL;
#elif defined(QB64_WINDOWS)
    static LPHOSTENT hostEntry;
    hostEntry=gethostbyname((char*)host);
    if (!hostEntry) return NULL;
    //Ref. from 'winsock.h': typedef u_int SOCKET;
    static SOCKET theSocket;
    theSocket = socket(AF_INET,     // Go over TCP/IP
               SOCK_STREAM,     // This is a stream-oriented socket
               IPPROTO_TCP);    // Use TCP rather than UDP
    if (theSocket==INVALID_SOCKET) return NULL;
    static SOCKADDR_IN serverInfo;
    serverInfo.sin_family=AF_INET;
    serverInfo.sin_addr=*((LPIN_ADDR)*hostEntry->h_addr_list);
    serverInfo.sin_port=htons(port);
    static int nret;
    nret = connect(theSocket,
           (LPSOCKADDR)&serverInfo,
           sizeof(struct sockaddr));
    if (nret==SOCKET_ERROR){closesocket(theSocket); return NULL;}
    //Reference: http://msdn.microsoft.com/en-us/library/windows/desktop/ms738573%28v=vs.85%29.aspx
    // Set the socket I/O mode: In this case FIONBIO
    // enables or disables the blocking mode for the 
    // socket based on the numerical value of iMode.
    // If iMode = 0, blocking is enabled; 
    // If iMode != 0, non-blocking mode is enabled.
    static u_long iMode;
    iMode=1;
    ioctlsocket(theSocket, FIONBIO,&iMode);

    static tcp_connection *connection;
    connection=(tcp_connection*)calloc(sizeof(tcp_connection),1);
    connection->socket=theSocket;
    connection->port=port;
    connection->connected = -1;
    connection->hostname=(uint8*)malloc(strlen((char*)host)+1);
    memcpy(connection->hostname,host,strlen((char*)host)+1);
    return (void*)connection;
#elif defined(QB64_UNIX)
    struct addrinfo hints, *servinfo, *p;
    int sockfd;
    char str_port[6];
    snprintf(str_port, 6, "%d", port);
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo((char*)host, str_port, &hints, &servinfo) != 0) return NULL;
    for(p = servinfo; p != NULL; p = p->ai_next) {
      sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
      if (sockfd == -1) continue;
      if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            continue;
      }
      break; //if we get here, all is good
    }
    freeaddrinfo(servinfo);
    if (p == NULL) return NULL; //indicates none of the entries succeeded
    fcntl(sockfd, F_SETFL, O_NONBLOCK); //make socket non-blocking

    //now we just need to create a struct tcp_connection to return
    tcp_connection *connection;
    connection=(tcp_connection*)calloc(sizeof(tcp_connection),1);
    connection->socket=sockfd;
    connection->port=port;
    connection->connected = -1;
    connection->hostname=(uint8*)malloc(strlen((char*)host)+1);
    memcpy(connection->hostname,host,strlen((char*)host)+1);
    return (void*)connection;
#else
    return NULL;
#endif
  }


  void *tcp_connection_open(void *host_tcp){
#if !defined(DEPENDENCY_SOCKETS)
    return NULL;
#elif defined(QB64_WINDOWS)
    static tcp_connection *host; host=(tcp_connection*)host_tcp;
    static sockaddr sa;
    static int sa_size;
    sa_size=sizeof(sa);
    static SOCKET new_socket;
    new_socket = accept(host->socket,
            &sa,             // Optionally, address of a SOCKADDR_IN struct
            &sa_size);       //             sizeof ( struct SOCKADDR_IN )                        
    if (new_socket==INVALID_SOCKET) return NULL;
    static u_long iMode;
    iMode=1;
    ioctlsocket(new_socket, FIONBIO,&iMode);
    static tcp_connection *connection;
    connection=(tcp_connection*)calloc(sizeof(tcp_connection),1);
    connection->socket=new_socket;
    //IPv4: port,port,ip,ip,ip,ip
    connection->port=*((uint16*)sa.sa_data);
    connection->connected = -1;
    *((uint32*)(connection->ip4))=*((uint32*)(sa.sa_data+2));
    return (void*)connection;
#elif defined(QB64_UNIX)
    tcp_connection *host; host=(tcp_connection*)host_tcp;
    struct sockaddr remote_addr;
    socklen_t addr_size;
    int fd;
    
    addr_size = sizeof(remote_addr);
    fd = accept(host->socket, &remote_addr, &addr_size);
    if (fd == -1) return NULL;
    fcntl(fd, F_SETFL, O_NONBLOCK); //make socket non-blocking

    tcp_connection *connection;
    connection=(tcp_connection*)calloc(sizeof(tcp_connection),1);
    connection->socket=fd;
    connection->connected = -1;
    //IPv4: port,port,ip,ip,ip,ip
    connection->port=*((uint16*)remote_addr.sa_data);
    *((uint32*)(connection->ip4))=*((uint32*)(remote_addr.sa_data+2));
    return (void*)connection;
#else
    return NULL:
#endif
  }

  void tcp_close(void* connection){
    tcp_connection *tcp=(tcp_connection*)connection;
#if !defined(DEPENDENCY_SOCKETS)
#elif defined(QB64_WINDOWS)
    if (tcp->socket) {
      shutdown(tcp->socket,SD_BOTH);
      closesocket(tcp->socket);
    }
#elif defined(QB64_UNIX)
    if (tcp->socket) {
      shutdown(tcp->socket, SHUT_RDWR);
      close(tcp->socket);
    }
#endif
    if (tcp->hostname) free(tcp->hostname);
    free(tcp);
  }

  void tcp_out(void *connection,void *offset,ptrszint bytes){
#if !defined(DEPENDENCY_SOCKETS)
#elif defined(QB64_WINDOWS) || defined(QB64_UNIX)
    tcp_connection *tcp; tcp=(tcp_connection*)connection;
    int total = 0;        // how many bytes we've sent
    int bytesleft = bytes; // how many we have left to send
    int n;

    while(total < bytes) {
      n = send(tcp->socket, (char*)((char *)offset + total), bytesleft, 0);
      if (n < 0) {
	tcp->connected = 0;
	return;
      }
      total += n;
      bytesleft -= n;
    }
#else
#endif
  }

  int32 cloud_port_redirect=-1;
  struct connection_struct{
    int8 in_use;//0=not being used, 1=in use
    int8 protocol;//1=TCP/IP
    int8 type;//1=client, 2=host(listening), 3=host's connection from a client
    ptrszint stream;
    ptrszint handle;
    void *connection;
    //---------------------------------
    int32 port;
  };
  list *connection_handles=NULL;

  void stream_out(stream_struct *st,void *offset,ptrszint bytes){
    if (st->type==1){//Network
      static connection_struct *co; co=(connection_struct*)st->index;
      if ((co->type==1)||(co->type==3)){//client or host's connection from a client

    if (co->protocol==1){//TCP/IP
      tcp_out((void*)co->connection,offset,bytes);
    }
      }//client or host's connection from a client
    }//Network
  }//stream_out


  void stream_update(stream_struct *stream){
#ifdef DEPENDENCY_SOCKETS
    //assume tcp

    static connection_struct *connection;
    connection=(connection_struct*)(stream->index);
    static tcp_connection *tcp;
    tcp=(tcp_connection*)(connection->connection);
    static ptrszint bytes;

    if (!stream->in_limit){
      stream->in=(uint8*)malloc(1024);
      stream->in_size=0;
      stream->in_limit=1024;
    }

  expand_and_retry:

    //expand buffer if 'in' stream is full
    //also guarantees that bytes requested from recv() is not 0
    if (stream->in_size==stream->in_limit){
      stream->in_limit*=2; stream->in=(uint8*)realloc(stream->in,stream->in_limit);
    }


    bytes = recv(tcp->socket,(char*)(stream->in+stream->in_size),
		 stream->in_limit-stream->in_size,
		 0);
    if (bytes < 0) { //some kind of error
#ifdef QB64_WINDOWS
      if (WSAGetLastError() != WSAEWOULDBLOCK) tcp->connected = 0; //fatal error
#else
      if (errno != EAGAIN && errno != EWOULDBLOCK) tcp->connected = 0;
#endif
    }
    else if (bytes == 0) { //graceful shutdown occured
      tcp->connected = 0;
    }
    else {
      stream->in_size+=bytes;
      if (stream->in_size==stream->in_limit) goto expand_and_retry;
    }
#endif    
  }




  void connection_close(ptrszint i){
    //Note: 'i' is a positive integer 1 or greater
    //      'i' must be a valid handle
    static special_handle_struct *sh; sh=(special_handle_struct*)list_get(special_handles,i);

    if (sh->type==2){//host listener
      static connection_struct *cs; cs=(connection_struct*)sh->index;
      if (cs->protocol==1) tcp_close(cs->connection);
      list_remove(connection_handles,list_get_index(connection_handles,cs));
      list_remove(special_handles,list_get_index(special_handles,sh));
      return;
    }//host listener

    //client or connection to host
    if (sh->type==1){//stream
      static stream_struct *ss; ss=(stream_struct*)sh->index;
      if (ss->type==1){//network
    static connection_struct *cs; cs=(connection_struct*)ss->index;
    if (cs->protocol==1) tcp_close(cs->connection);
    list_remove(connection_handles,list_get_index(connection_handles,cs));
    stream_free(ss);
    list_remove(special_handles,list_get_index(special_handles,sh));
    return;
      }//network
    }//stream

  }

  int32 connection_new(int32 method, qbs *info_in, int32 value){
    //method: 0=_OPENCLIENT [info=~"TCP/IP:12345:23.96.32.123], value=NULL"
    //        1=_OPENHOST [info=~"TCP/IP:12345", value=NULL]
    //        2=_OPENCONNECTION [info=NULL, value=host's handle]
    //returns: -1=invalid arguments passed
    //          0=failed to open
    //         >0=handle of successfully opened connection

    static int32 i,x;

    //generic division of parts
    static qbs *info_part[10+1];
    static qbs *str;
    static qbs *strz;
    static qbs *info;

    static int32 first_call=1;
    if (first_call){
      first_call=0;
      for(i=1;i<=10;i++){info_part[i]=qbs_new(0,0);}
      str=qbs_new(0,0);
      strz=qbs_new(1,0); strz->chr[0]=0;
      info=qbs_new(0,0);
    }//first call

    //split info string
    static int32 parts;
    parts=0;
    if ((method==0)||(method==1)){
      qbs_set(info,info_in);
      qbs_set(str,qbs_new_txt(":"));
      i=1;
    next_part:
      x=func_instr(i,info,str,1);
      if (x){
    parts++; qbs_set(info_part[parts],func_mid(info,i,x-i,1));
    i=x+1;
    goto next_part;
      }
      parts++; qbs_set(info_part[parts],func_mid(info,i,NULL,NULL));
    }//split info string

    static double d;
    static int32 port;

    if ((method==0)||(method==1)){

      if (parts<2) return -1;
      if (qbs_equal(qbs_ucase(info_part[1]),qbs_new_txt("TCP/IP"))==0) return -1;

      d=func_val(info_part[2]);
      port=qbr_double_to_long(d);//***assume*** port number is within valid range

      if (method==0){//_OPENCLIENT
    if (parts!=3) return -1;

    if (cloud_app){
      if (port==cloud_port_redirect) port=cloud_port[1];
    }

    static void *connection;
    qbs_set(str,qbs_add(info_part[3],strz));
    connection=tcp_client_open(str->chr,port);
    if (!connection) return 0;

    static int32 my_handle; my_handle=list_add(special_handles);
    static special_handle_struct *my_handle_struct; my_handle_struct=(special_handle_struct*)list_get(special_handles,my_handle);
    static int32 my_stream; my_stream=list_add(stream_handles);
    static stream_struct *my_stream_struct; my_stream_struct=(stream_struct*)list_get(stream_handles,my_stream);
    static int32 my_connection; my_connection=list_add(connection_handles);
    static connection_struct *my_connection_struct; my_connection_struct=(connection_struct*)list_get(connection_handles,my_connection);
    my_handle_struct->type=1;//stream
    my_handle_struct->index=(ptrszint)my_stream_struct;
    my_stream_struct->type=1;//network
    my_stream_struct->index=(ptrszint)my_connection_struct;
    my_connection_struct->protocol=1;//tcp/ip
    my_connection_struct->type=1;//client
    my_connection_struct->connection=connection;
    my_connection_struct->port=port;

    //init stream
    my_stream_struct->in=NULL; my_stream_struct->in_size=0; my_stream_struct->in_limit=0;

    return my_handle;
      }//client


      if (method==1){//_OPENHOST
    if (parts!=2) return -1;

    if (cloud_app){
      if (port==cloud_port[1]) goto gotcloudport;
      if (port==cloud_port[2]) goto gotcloudport;
      if ((port>=1)&&(port<=2)){port=cloud_port[port]; goto gotcloudport;}
      cloud_port_redirect=port;
      port=cloud_port[1];//unknown values default to primary hosting port
    }
      gotcloudport:

    static void *connection;
    connection=tcp_host_open(port);
    if (!connection) return 0;

    static int32 my_handle; my_handle=list_add(special_handles);
    static special_handle_struct *my_handle_struct; my_handle_struct=(special_handle_struct*)list_get(special_handles,my_handle);
    static int32 my_connection; my_connection=list_add(connection_handles);
    static connection_struct *my_connection_struct; my_connection_struct=(connection_struct*)list_get(connection_handles,my_connection);
    my_handle_struct->type=2;//host listener
    my_handle_struct->index=(ptrszint)my_connection_struct;
    my_connection_struct->protocol=1;//tcp/ip
    my_connection_struct->type=2;//host(listening)
    my_connection_struct->connection=connection;
    my_connection_struct->port=port;
    return my_handle;

      }
    }//0 or 1

    if (method==2){//_OPENCONNECTION
      static special_handle_struct *sh; sh=(special_handle_struct*)list_get(special_handles,value); if (!sh) return -1;
      if (sh->type!=2) return -1;//listening host?
      static connection_struct *co; co=(connection_struct*)sh->index;
      static void *connection;
      connection=tcp_connection_open(co->connection);
      if (!connection) return 0;

      static int32 my_handle; my_handle=list_add(special_handles);
      static special_handle_struct *my_handle_struct; my_handle_struct=(special_handle_struct*)list_get(special_handles,my_handle);
      static int32 my_stream; my_stream=list_add(stream_handles);
      static stream_struct *my_stream_struct; my_stream_struct=(stream_struct*)list_get(stream_handles,my_stream);
      static int32 my_connection; my_connection=list_add(connection_handles);
      static connection_struct *my_connection_struct; my_connection_struct=(connection_struct*)list_get(connection_handles,my_connection);
      my_handle_struct->type=1;//stream
      my_handle_struct->index=(ptrszint)my_stream_struct;
      my_stream_struct->type=1;//network
      my_stream_struct->index=(ptrszint)my_connection_struct;
      my_connection_struct->protocol=1;//tcp/ip
      my_connection_struct->type=3;//host's client connection
      my_connection_struct->connection=connection;
      my_connection_struct->port=port;

      //init stream
      my_stream_struct->in=NULL; my_stream_struct->in_size=0; my_stream_struct->in_limit=0;

      return my_handle;

    }//_OPENCONNECTION

  }//connection_new


  //network prototype:



  int32 func__openclient(qbs* info){
    if (new_error) return 0;
    static int32 i;
    i=connection_new(0,info,NULL);
    if (i==-1){error(5); return 0;}
    if (i==0) return 0;
    return -1-i;
  }

  int32 func__openhost(qbs* info){
    if (new_error) return 0;
    static int32 i;
    i=connection_new(1,info,NULL);
    if (i==-1){error(5); return 0;}
    if (i==0) return 0;
    return -1-i;
  }

  int32 func__openconnection(int32 i){

    if (new_error) return 0;
    i=-(i+1);
    i=connection_new(2,NULL,i);
    if (i==-1){error(258); return 0;}//invalid handle
    if (i==0) return 0;//no new connections
    return -1-i;
  }



  qbs *func__connectionaddress(int32 i){
    static qbs *tqbs,*tqbs2,*str=NULL,*str2=NULL;
    static int32 x;
    if (new_error) goto error;
    if (!str) str=qbs_new(0,0);
    if (!str2) str2=qbs_new(0,0);

    if (i<0){
      x=-(i+1);
      static special_handle_struct *sh; sh=(special_handle_struct*)list_get(special_handles,x); if (!sh){error(52); goto error;}

      if (sh->type==2){//host listener
    static connection_struct *cs; cs=(connection_struct*)sh->index;
    if (cs->protocol==1){//TCP/IP
      qbs_set(str,qbs_new_txt("TCP/IP:"));//network type
      qbs_set(str,qbs_add(str,qbs_ltrim(qbs_str(cs->port))));//port
      qbs_set(str,qbs_add(str,qbs_new_txt(":")));
      tqbs2=WHATISMYIP();
      if (tqbs2->len){qbs_set(str,qbs_add(str,tqbs2));
      }else{
        qbs_set(str,qbs_add(str,qbs_new_txt("127.0.0.1")));//localhost
      }
      return str;
    }//TCP/IP
      }//host listener

      //client or connection to host
      if (sh->type==1){//stream
    static stream_struct *ss; ss=(stream_struct*)sh->index;
    if (ss->type==1){//network
      static connection_struct *cs; cs=(connection_struct*)ss->index;
      if (cs->protocol==1){//TCP/IP
        if (cs->type==1||cs->type==3){//1=client, 2=host(listening), 3=host's connection from a client

          static tcp_connection *tcp; tcp=(tcp_connection*)cs->connection;
          qbs_set(str,qbs_new_txt("TCP/IP:"));//network type
          qbs_set(str,qbs_add(str,qbs_ltrim(qbs_str(tcp->port))));//port
          qbs_set(str,qbs_add(str,qbs_new_txt(":")));
          if (cs->type==3){
        qbs_set(str,qbs_add(str,qbs_ltrim(qbs_str(tcp->ip4[0]))));//ip
        qbs_set(str,qbs_add(str,qbs_new_txt(".")));
        qbs_set(str,qbs_add(str,qbs_ltrim(qbs_str(tcp->ip4[1]))));//ip
        qbs_set(str,qbs_add(str,qbs_new_txt(".")));
        qbs_set(str,qbs_add(str,qbs_ltrim(qbs_str(tcp->ip4[2]))));//ip
        qbs_set(str,qbs_add(str,qbs_new_txt(".")));
        qbs_set(str,qbs_add(str,qbs_ltrim(qbs_str(tcp->ip4[3]))));//ip
          }else{
        qbs_set(str,qbs_add(str,qbs_new_txt((char*)tcp->hostname)));
          }
          return str;
        }
      }//TCP/IP
    }//network
      }//stream

    }//i<0
    error(52); goto error;

  error:
    tqbs=qbs_new(0,1);
    return tqbs;

  }

  int32 tcp_connected (void *connection){
    tcp_connection *tcp=(tcp_connection*)connection;
#ifndef DEPENDENCY_SOCKETS
    return 0;
#else
    return tcp->connected;
#endif
  }

  int32 func__connected(int32 i){
    if (new_error) return 0;
    if (i<0){
      static int32 x;
      x=-(i+1);
      static special_handle_struct *sh; sh=(special_handle_struct*)list_get(special_handles,x); if (!sh) goto error;

      if (sh->type==2){//host listener
    static connection_struct *cs; cs=(connection_struct*)sh->index;
    if (cs->protocol==1){//TCP/IP
      return -1;
    }//TCP/IP
      }//host listener

      //client or connection to host
      if (sh->type==1){//stream
    static stream_struct *ss; ss=(stream_struct*)sh->index;
    if (ss->type==1){//network
      static connection_struct *cs; cs=(connection_struct*)ss->index;
      if (cs->protocol==1){//TCP/IP
        return tcp_connected(cs->connection);
      }//TCP/IP
    }//network
      }//stream

    }//i<0
  error: error(52); return 0;
  }


int32 func__exit(){
  exit_blocked=1;
  static int32 x;
  x=exit_value;
  if (x) exit_value = 0;
  return x;
}








#if defined(QB64_LINUX)

//X11 clipboard interface for Linux
//SDL_SysWMinfo syswminfo;
Atom targets,utf8string,compoundtext,clipboard;

int x11filter(XEvent *x11event){
static int i;
static char *cp;
static XSelectionRequestEvent *x11request;
static XSelectionEvent x11selectionevent;
static Atom mytargets[]={XA_STRING,utf8string,compoundtext};
 if (x11event->type==SelectionRequest){
  x11request=&x11event->xselectionrequest;
  x11selectionevent.type=SelectionNotify;
  x11selectionevent.serial=x11event->xany.send_event;
  x11selectionevent.send_event=True;
  x11selectionevent.display=X11_display;
  x11selectionevent.requestor=x11request->requestor;
  x11selectionevent.selection=x11request->selection;
  x11selectionevent.target=None;
  x11selectionevent.property=x11request->property;
  x11selectionevent.time=x11request->time;
  if (x11request->target==targets){
   XChangeProperty(X11_display,x11request->requestor,x11request->property,XA_ATOM,32,PropModeReplace,(unsigned char*)mytargets,3);
  }else{
   if (x11request->target==compoundtext||x11request->target==utf8string||x11request->target==XA_STRING){
    cp=XFetchBytes(X11_display,&i);
    XChangeProperty(X11_display,x11request->requestor,x11request->property,x11request->target,8,PropModeReplace,(unsigned char*)cp,i);
    XFree(cp);
   }else{
    x11selectionevent.property=None;
   }
  }
  XSendEvent(x11request->display,x11request->requestor,0,NoEventMask,(XEvent*)&x11selectionevent);
  XSync(X11_display,False);
 }
return 1;
}

void setupx11clipboard(){
static int32 setup=0;
if (!setup){
 setup=1;
 //SDL_GetWMInfo(&syswminfo);
 //SDL_EventState(SDL_SYSWMEVENT,SDL_ENABLE);
 //SDL_SetEventFilter(x11filter);
 x11_lock();
 targets=XInternAtom(X11_display,"TARGETS",True);
 utf8string=XInternAtom(X11_display,"UTF8_STRING",True);
 compoundtext=XInternAtom(X11_display,"COMPOUND_TEXT",True);
 clipboard=XInternAtom(X11_display,"CLIPBOARD",True);
 x11_unlock();
}
}

void x11clipboardcopy(const char *text){
setupx11clipboard();
x11_lock();
XStoreBytes(X11_display,text,strlen(text)+1);
XSetSelectionOwner(X11_display,clipboard,X11_window,CurrentTime);
x11_unlock();
return; 
}

char *x11clipboardpaste(){
static int32 i;
static char *cp;
static unsigned char *cp2;
static Window x11selectionowner;
static XEvent x11event;
static unsigned long data_items,bytes_remaining,ignore;
static int format;
static Atom type;
cp=NULL; cp2=NULL;
setupx11clipboard();
//syswminfo.info.x11.lock_func();
x11_lock();
x11selectionowner=XGetSelectionOwner(X11_display,clipboard);
if (x11selectionowner!=None){
 //The XGetSelectionOwner() function returns the window ID associated with the window
 if (x11selectionowner==X11_window){//we are the provider, so just return buffered content
  x11_unlock();
  int bytes;
  cp=XFetchBytes(X11_display,&bytes);
  return cp;
 }
 XConvertSelection(X11_display,clipboard,utf8string,clipboard,X11_window,CurrentTime);
 XFlush(X11_display);
    bool gotReply = false;
    int timeoutMs = 10000;//10sec
    do {
      XEvent event;
      gotReply = XCheckTypedWindowEvent(X11_display, X11_window, SelectionNotify, &event);
      if (gotReply) {
        if (event.xselection.property == clipboard) {
           XGetWindowProperty(X11_display,X11_window,clipboard,0,0,False,AnyPropertyType,&type,&format,&data_items,&bytes_remaining,&cp2);
 if (cp2){XFree(cp2); cp2=NULL;}
 if (bytes_remaining){
  if (XGetWindowProperty(X11_display,X11_window,clipboard,0,bytes_remaining,False,AnyPropertyType,&type,&format,&data_items, &ignore,&cp2)==Success){
   cp=strdup((char*)cp2);
   XFree(cp2);
   XDeleteProperty(X11_display,X11_window,clipboard);
   x11_unlock();
   return cp;  
  }
 }  
          x11_unlock();
          return NULL;

        } else {
          x11_unlock();
          return NULL;
        }
      }      
      Sleep(1);
      timeoutMs -= 1;
    } while (timeoutMs > 0);    
}//x11selectionowner!=None
    x11_unlock();
    return NULL;
}

#endif


















 qbs *internal_clipboard=NULL;//used only if clipboard services unavailable
 int32 linux_clipboard_init=0;

 void sub__clipboard(qbs *text){

#ifdef QB64_WINDOWS
   static uint8 *textz;
   static HGLOBAL h;
   if (OpenClipboard(NULL)){
     EmptyClipboard();
     h=GlobalAlloc(GMEM_MOVEABLE,text->len+1); if (h){
       textz=(uint8*)GlobalLock(h); if (textz){
         memcpy(textz,text->chr,text->len);
         textz[text->len]=0;
         GlobalUnlock(h);
         SetClipboardData(CF_TEXT,h);
       }
     }
     CloseClipboard();
   }
   return;
#endif

#ifdef QB64_MACOSX
   PasteboardRef clipboard;
   if (PasteboardCreate(kPasteboardClipboard, &clipboard) != noErr) {
     return;
   }
   if (PasteboardClear(clipboard) != noErr) {
     CFRelease(clipboard);
     return;
   }
   CFDataRef data = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, text->chr, 
                                                text->len, kCFAllocatorNull);
   if (data == NULL) {
     CFRelease(clipboard);
     return;
   }
   OSStatus err;
   err = PasteboardPutItemFlavor(clipboard, NULL, kUTTypeUTF8PlainText, data, 0);
   CFRelease(clipboard);
   CFRelease(data);
   return;
#endif

#if defined(QB64_LINUX)
   static qbs *textz=NULL; if (!textz) textz=qbs_new(0,0);
   qbs_set(textz,qbs_add(text,qbs_new_txt_len("\0",1)));
   x11clipboardcopy((char*)textz->chr);
   return;
#endif

   if (internal_clipboard==NULL) internal_clipboard=qbs_new(0,0);
   qbs_set(internal_clipboard,text);
 }

#ifdef DEPENDENCY_SCREENIMAGE
void sub__clipboardimage(int32 src) {
#ifdef QB64_WINDOWS

    if (new_error) return;

    static int32 i,i2,ii,w,h;
    static uint32 *o,*o2;
    static int32 x,y,n,c,i3,c2;

    //validation
    i=src;
    if (i>=0){//validate i
        validatepage(i); i=page[i];
    }else{
        i=-i; if (i>=nextimg){error(258); return;} if (!img[i].valid){error(258); return;}
    }
    
    if (img[i].text){error(5); return;}
    //end of validation

    w=img[i].width;
    h=img[i].height;

    //source[http://support.microsoft.com/kb/318876]
    HDC hdc;
    BITMAPV5HEADER bi;
    HBITMAP hBitmap;
    void *lpBits;
    ZeroMemory(&bi,sizeof(BITMAPV5HEADER));
    bi.bV5Size           = sizeof(BITMAPV5HEADER);
    bi.bV5Width           = w;
    bi.bV5Height          = h;
    bi.bV5Planes = 1;
    bi.bV5BitCount = 32;
    bi.bV5Compression = BI_RGB;

    hdc = GetDC(NULL);
    // Create the DIB section with an alpha channel.
    hBitmap = CreateDIBSection(hdc, (BITMAPINFO *)&bi, DIB_RGB_COLORS, 
                   (void **)&lpBits, NULL, (DWORD)0);
                   
    //Transfer the source image to a new 32-bit image to avoid incompatible formats)
    i2=func__newimage(w,h,32,1);
    sub__putimage(NULL,NULL,NULL,NULL,-i,i2,NULL,NULL,NULL,NULL,8+32);

    o=img[-i2].offset32;
    o2=(uint32*)lpBits;
    for (y=0;y<h;y++){
      for (x=0;x<w;x++){
        c=o[(h-1-y)*w+x];
        o2[y*w+x]=c;
      }}

    sub__freeimage(i2,1);
 
    //Create copy of hBitmap to send to the clipboard
    HBITMAP bitmapCopy;
    HDC hdc2, hdc3;
    bitmapCopy = CreateCompatibleBitmap(hdc, w, h);
    hdc2=CreateCompatibleDC(hdc);
    hdc3=CreateCompatibleDC(hdc);
    SelectObject(hdc2,bitmapCopy);
    SelectObject(hdc3,hBitmap);
    BitBlt(hdc2,0,0,w,h,hdc3,0,0,SRCCOPY);
    
    ReleaseDC(NULL,hdc);
    ReleaseDC(NULL,hdc2);
    ReleaseDC(NULL,hdc3);

    //Send bitmapCopy to the clipboard
    if (OpenClipboard(NULL)) {
        EmptyClipboard();
        SetClipboardData(CF_BITMAP, bitmapCopy);
        CloseClipboard();
    }

    DeleteObject(hBitmap);
    DeleteObject(bitmapCopy);
#endif
}
#endif

#ifdef DEPENDENCY_SCREENIMAGE
int32 func__clipboardimage(){
#ifdef QB64_WINDOWS
    
    if (new_error) return -1;
    
    static HBITMAP bitmap;
    static BITMAP bitmapInfo;
    static HDC hdc;
    static int32 w, h;

    if ( OpenClipboard(NULL) )
    {        
        if (IsClipboardFormatAvailable(CF_BITMAP) == 0) {
            CloseClipboard();
            return -1;
        }
        
        bitmap = (HBITMAP)GetClipboardData(CF_BITMAP);
        CloseClipboard();
        GetObject(bitmap,sizeof( BITMAP ), &bitmapInfo);
        h = bitmapInfo.bmHeight;
        w = bitmapInfo.bmWidth;         

        static BITMAPFILEHEADER   bmfHeader;  
        static BITMAPINFOHEADER   bi;
        bi.biSize = sizeof(BITMAPINFOHEADER);    
        bi.biWidth = w;    
        bi.biHeight = -h;
        bi.biPlanes = 1;    
        bi.biBitCount = 32;    
        bi.biCompression = BI_RGB;    
        bi.biSizeImage = 0;  
        bi.biXPelsPerMeter = 0;    
        bi.biYPelsPerMeter = 0;    
        bi.biClrUsed = 0;    
        bi.biClrImportant = 0;

        static int32 i,i2;
        i2=func__dest();
        i=func__newimage(w,h,32,1);
        sub__dest(i);

        hdc=GetDC(NULL);
        
        GetDIBits(hdc,bitmap,0,h,write_page->offset,(BITMAPINFO*)&bi, DIB_RGB_COLORS);
        sub__setalpha(255,NULL,NULL,NULL,0); // required as some images come
                                             // with alpha 0 from the clipboard
        sub__dest(i2);

        ReleaseDC(NULL,hdc);
        DeleteObject(bitmap);
        return i;

} else return -1;
#endif
return -1;
}
#endif

  qbs *func__clipboard(){
#ifdef QB64_WINDOWS
    static qbs *text;
    static uint8 *textz;
    static HGLOBAL h;
    if (OpenClipboard(NULL)){
      if (IsClipboardFormatAvailable(CF_TEXT)){
    h=GetClipboardData(CF_TEXT); if (h){
      textz=(uint8*)GlobalLock(h); if (textz){
        text=qbs_new(strlen((char*)textz),1);
        memcpy(text->chr,textz,text->len);
        GlobalUnlock(h);
        CloseClipboard();
        return text;
      }
    }
      }
      CloseClipboard();
    }
    text=qbs_new(0,1);
    return text;
#endif

#ifdef QB64_MACOSX
    static qbs *text;
    OSStatus             err = noErr;
    ItemCount            itemCount;
    PasteboardSyncFlags  syncFlags;
    static PasteboardRef inPasteboard = NULL;
    PasteboardCreate( kPasteboardClipboard, &inPasteboard );
    char* data;
    data = "";   
    syncFlags = PasteboardSynchronize( inPasteboard );
    err = badPasteboardSyncErr;
   
    err = PasteboardGetItemCount( inPasteboard, &itemCount );
    if ( (err) != noErr ) goto CantGetPasteboardItemCount;
   
    for( int itemIndex = 1; itemIndex <= itemCount; itemIndex++ ) {
      PasteboardItemID  itemID;
      CFDataRef  flavorData;

      err = PasteboardGetItemIdentifier( inPasteboard, itemIndex, &itemID );
      if ( (err) != noErr ) goto CantGetPasteboardItemIdentifier;

      err = PasteboardCopyItemFlavorData( inPasteboard, itemID, CFSTR("public.utf8-plain-text"), &flavorData );      
      data = (char*)CFDataGetBytePtr(flavorData);

      uint32 size;
      size=CFDataGetLength( flavorData );



      text=qbs_new(size,1);
      memcpy(text->chr,data,text->len);
      //CFRelease (flavorData);
      //CFRelease (flavorTypeArray);
      //CFRelease(inPasteboard);
      return text;



      
    CantGetPasteboardItemIdentifier:
      ;
    }
   
  CantGetPasteboardItemCount:
    text=qbs_new(0,1);
    return text;
    return NULL;
#endif

#if defined(QB64_LINUX)
    qbs *text;
    char *cp=x11clipboardpaste();
    cp=x11clipboardpaste();
    if (!cp){
      text=qbs_new(0,1);
    }else{
      text=qbs_new(strlen(cp),1);
      memcpy(text->chr,cp,text->len);
      free(cp);
    }
    return text;
#endif

    if (internal_clipboard==NULL) internal_clipboard=qbs_new(0,0);
    return internal_clipboard;
  }


  int32 display_called=0;
  void display_now(){
    if (autodisplay){
      display_called=0;
      while(!display_called) Sleep(1);
    }else{
      display();
    }
  }

  void sub__fullscreen(int32 method,int32 passed){
    //ref: "[{_OFF|_STRETCH|_SQUAREPIXELS}]"
    //          1      2           3
    int32 x;
    if (method==0) x=1;
    if (method==1) x=0;
    if (method==2) x=1;
    if (method==3) x=2;
    if (passed&1) fullscreen_smooth=1; else fullscreen_smooth=0;
    if (full_screen!=x) full_screen_set=x;
    force_display_update=1;
  }

  int32 func__fullscreen(){
    static int32 x;
    x=full_screen_set;
    if (x!=-1) return x;
    return full_screen;
  }


  void chain_restorescreenstate(int32 i){
    static int32 i32,i32b,i32c,x,x2;
    generic_get(i,-1,(uint8*)&i32,4);

    if (i32==256){
      generic_get(i,-1,(uint8*)&i32,4);
      if (i32!=0) qbg_screen(i32,0,0,0,0,1);
      generic_get(i,-1,(uint8*)&i32,4);
      if (i32==258){
    generic_get(i,-1,(uint8*)&i32,4); i32b=i32;
    generic_get(i,-1,(uint8*)&i32,4);
    qbsub_width(0,i32b,i32,1+2);
    generic_get(i,-1,(uint8*)&i32,4);
      }
    }

    if (i32==257){
      generic_get(i,-1,(uint8*)&i32,4); i32c=i32;
      generic_get(i,-1,(uint8*)&i32,4); i32b=i32;
      generic_get(i,-1,(uint8*)&i32,4);
      qbg_screen(func__newimage(i32b,i32,i32c,1),0,0,0,0,1);
      generic_get(i,-1,(uint8*)&i32,4);
    }

    if (i32==259){
      generic_get(i,-1,(uint8*)&i32,4);
      sub__font(i32,0,0);
      generic_get(i,-1,(uint8*)&i32,4);
    }

    static img_struct *ix;
    static img_struct imgs;

    while(i32==260){
      generic_get(i,-1,(uint8*)&i32,4); x=i32;
      qbg_screen(0,0,x,0,0,4+8);//switch to page (allocates the page)
      ix=&img[page[x]];
      generic_get(i,-1,ix->offset,ix->width*ix->height*ix->bytes_per_pixel);
      imgs=*ix;
      generic_get(i,-1,(uint8*)ix,sizeof(img_struct));
      //revert specific data
      if (ix->font>=32) ix->font=imgs.font;
      ix->offset=imgs.offset;
      ix->pal=imgs.pal;
      generic_get(i,-1,(uint8*)&i32,4);
    }

    if (i32==261){
      generic_get(i,-1,(uint8*)&i32,4); i32b=i32;
      generic_get(i,-1,(uint8*)&i32,4);
      qbg_screen(0,0,i32b,i32,0,4+8);//switch to correct active & visual pages
      generic_get(i,-1,(uint8*)&i32,4);
    }

    if (i32==262){
      for (x=0;x<=255;x++){
    generic_get(i,-1,(uint8*)&i32,4);
    sub__palettecolor(x,i32,0,1);
      }
      generic_get(i,-1,(uint8*)&i32,4);
    }


    //assume command #511("finished screen state") in i32
  }

  void chain_savescreenstate(int32 i){//adds the screen state to file #i
    static int32 i32,x,x2;
    static img_struct *i0,*ix;
    i0=&img[page[0]];

    if ( (i0->offset>cmem) && (i0->offset<(cmem+1114099)) ){//cmem?[need to maintain cmem state]
      //[256][mode]
      i32=256; generic_put(i,-1,(uint8*)&i32,4);
      i32=i0->compatible_mode; generic_put(i,-1,(uint8*)&i32,4);
      if (i0->text){
    //[258][WIDTH:X][Y]
    i32=258; generic_put(i,-1,(uint8*)&i32,4);
    i32=i0->width; generic_put(i,-1,(uint8*)&i32,4);
    i32=i0->height; generic_put(i,-1,(uint8*)&i32,4);
      }
    }else{
      //[257][mode][X][Y]
      i32=257; generic_put(i,-1,(uint8*)&i32,4);
      i32=i0->compatible_mode; generic_put(i,-1,(uint8*)&i32,4);
      i32=i0->width; generic_put(i,-1,(uint8*)&i32,4);
      i32=i0->height; generic_put(i,-1,(uint8*)&i32,4);
    }
    //[259][font] (standard fonts only)
    if (i0->font<32){
      i32=259; generic_put(i,-1,(uint8*)&i32,4);
      i32=i0->font; generic_put(i,-1,(uint8*)&i32,4);
    }

    //[260][page][rawdata]
    //note: write page is done last to avoid having its values undone by the later page switch
    x2=-1;
    for (x=0;x<pages;x++){
      if (page[x]){
    if (page[x]!=write_page_index){
    save_write_page:
      ix=&img[page[x]];
      i32=260; generic_put(i,-1,(uint8*)&i32,4);
      i32=x; generic_put(i,-1,(uint8*)&i32,4);
      generic_put(i,-1,ix->offset,ix->width*ix->height*ix->bytes_per_pixel);
      //save structure (specific parts will be reincorporated)
      generic_put(i,-1,(uint8*)ix,sizeof(img_struct));
      if (x==x2) break;
    }else x2=x;
      }
    }
    if ((x2!=-1)&&(x!=x2)){x=x2; goto save_write_page;}

    //[261][activepage][visualpage]
    i32=261; generic_put(i,-1,(uint8*)&i32,4);
    i32=0;//note: activepage could be a non-screenpage
    for (x=0;x<pages;x++){
      if (page[x]==write_page_index){i32=x;break;}
    }
    generic_put(i,-1,(uint8*)&i32,4);
    i32=0;
    for (x=0;x<pages;x++){
      if (page[x]==display_page_index){i32=x;break;}
    }
    generic_put(i,-1,(uint8*)&i32,4);

    //[262][256x32-bit color palette]
    if (i0->bytes_per_pixel!=4){
      i32=262; generic_put(i,-1,(uint8*)&i32,4);
      for (x=0;x<=255;x++){
    i32=func__palettecolor(x,0,1); generic_put(i,-1,(uint8*)&i32,4);
      }
    }

    //[511](screen state finished)
    i32=511; generic_put(i,-1,(uint8*)&i32,4);

  }//chain_savescreenstate







  void sub_lock(int32 i,int64 start,int64 end,int32 passed){
    if (new_error) return;
    if (gfs_fileno_valid(i)!=1){error(52); return;}//Bad file name or number
    i=gfs_fileno[i];//convert fileno to gfs index
    static gfs_file_struct *gfs;
    gfs=&gfs_file[i];

    //If the file has been opened for sequential input or output, LOCK and UNLOCK affect the entire file, regardless of the range specified by start& and end&.
    if (gfs->type>2) passed=0;

    if (passed&1){
      start--;
      if (start<0){error(5); return;}
      if (gfs->type==1) start*=gfs->record_length;
    }else{
      start=-1;
    }

    if (passed&2){
      end--;
      if (end<0){error(5); return;}
      if (gfs->type==1) end=end*gfs->record_length+gfs->record_length-1;
    }else{
      end=start;
      if (gfs->type==1) end=start+gfs->record_length-1;
      if (!(passed&1)) end=-1;
    }


    int32 e;
    e=gfs_lock(i,start,end);
    if (e){
      if (e==-2){error(258); return;}//invalid handle
      if (e==-4){error(5); return;}//illegal function call
      if (e==-7){error(70); return;}//permission denied
      error(75); return;//assume[-9]: path/file access error
    }

  }

  void sub_unlock(int32 i,int64 start,int64 end,int32 passed){
    if (new_error) return;
    if (gfs_fileno_valid(i)!=1){error(52); return;}//Bad file name or number
    i=gfs_fileno[i];//convert fileno to gfs index
    static gfs_file_struct *gfs;
    gfs=&gfs_file[i];

    //If the file has been opened for sequential input or output, LOCK and UNLOCK affect the entire file, regardless of the range specified by start& and end&.
    if (gfs->type>2) passed=0;

    if (passed&1){
      start--;
      if (start<0){error(5); return;}
      if (gfs->type==1) start*=gfs->record_length;
    }else{
      start=-1;
    }

    if (passed&2){
      end--;
      if (end<0){error(5); return;}
      if (gfs->type==1) end=end*gfs->record_length+gfs->record_length-1;
    }else{
      end=start;
      if (gfs->type==1) end=start+gfs->record_length-1;
      if (!(passed&1)) end=-1;
    }

    int32 e;
    e=gfs_unlock(i,start,end);
    if (e){
      if (e==-2){error(258); return;}//invalid handle
      if (e==-4){error(5); return;}//illegal function call
      if (e==-7){error(70); return;}//permission denied
      error(75); return;//assume[-9]: path/file access error
    }

  }

#ifdef DEPENDENCY_SCREENIMAGE
  int32 func__screenimage(int32 x1,int32 y1,int32 x2,int32 y2,int32 passed){

#ifdef QB64_WINDOWS

    static int32 init=0;
    static int32 x,y,w,h;

    static HWND hwnd;
    static RECT rect;
    static HDC hdc;

    if (!init){
      hwnd=GetDesktopWindow();
      GetWindowRect(hwnd,&rect);
      x=rect.right-rect.left; y=rect.bottom-rect.top;
    }

    hdc=GetDC(NULL);

    static HDC hdc2=NULL;
    static HBITMAP bitmap;
    static int32 bx,by;

    if (passed){
      if (x1<0) x1=0;
      if (y1<0) y1=0;
      if (x2>x-1) x2=x-1;
      if (y2>y-1) y2=y-1;
      w=x2-x1+1; h=y2-y1+1;
    }else{
      x1=0; x2=x-1; y1=0; y2=y-1; w=x; h=y;
    }



    if (hdc2){
      if ((w!=bx)||(h!=by)){
    DeleteObject(bitmap);
    ReleaseDC(NULL,hdc2);
    hdc2=CreateCompatibleDC(hdc);
    bitmap=CreateCompatibleBitmap(hdc,w,h); bx=w; by=h;
    SelectObject(hdc2,bitmap);
      }
    }else{
      hdc2=CreateCompatibleDC(hdc);
      bitmap=CreateCompatibleBitmap(hdc,w,h); bx=w; by=h;
      SelectObject(hdc2,bitmap);
    }

    init=1;

    BitBlt(        hdc2, 
           0,0, 
           w,h,
           hdc, 
           x1,y1,
           SRCCOPY);

    static BITMAPFILEHEADER   bmfHeader;  
    static BITMAPINFOHEADER   bi;
    bi.biSize = sizeof(BITMAPINFOHEADER);    
    bi.biWidth = w;    
    bi.biHeight = -h; //A bottom-up DIB is specified by setting the height to a positive number, while a top-down DIB is specified by setting the height to a negative number. The bitmap color table will be appended to the BITMAPINFO structure.
    bi.biPlanes = 1;    
    bi.biBitCount = 32;    
    bi.biCompression = BI_RGB;    
    bi.biSizeImage = 0;  
    bi.biXPelsPerMeter = 0;    
    bi.biYPelsPerMeter = 0;    
    bi.biClrUsed = 0;    
    bi.biClrImportant = 0;

    static int32 i,i2;
    i2=func__dest();
    i=func__newimage(w,h,32,1);
    sub__dest(i);

    GetDIBits(hdc2,bitmap,0,h,write_page->offset,(BITMAPINFO*)&bi, DIB_RGB_COLORS);
    sub__setalpha(255,NULL,NULL,NULL,0);
    sub__dest(i2);

    ReleaseDC(NULL,hdc);
    return i;
#else
    return func__newimage(func_screenwidth(), func_screenheight(), 32, 1);
#endif
  }
#endif //DEPENDENCY_SCREENIMAGE

  void sub__screenclick(int32 x,int32 y, int32 button, int32 passed){

    if (cloud_app){error(262); return;}

#ifdef QB64_WINDOWS

    static INPUT input;

    ZeroMemory(&input,sizeof(INPUT));
    input.type=INPUT_MOUSE;
    input.mi.dwFlags=MOUSEEVENTF_ABSOLUTE|MOUSEEVENTF_MOVE;
    static HWND hwnd;
    hwnd=GetDesktopWindow();
    static RECT rect;
    GetWindowRect(hwnd,&rect);
    static double x2,y2,fx,fy;
    x2=rect.right-rect.left;
    y2=rect.bottom-rect.top;
    fx=x*(65535.0f/x2);
    fy=y*(65535.0f/y2);
    input.mi.dx=fx;
    input.mi.dy=fy;
    SendInput(1,&input,sizeof(INPUT));

    ZeroMemory(&input,sizeof(INPUT));
    input.type=INPUT_MOUSE;
    input.mi.dwFlags=MOUSEEVENTF_LEFTDOWN;
    SendInput(1,&input,sizeof(INPUT));

    ZeroMemory(&input,sizeof(INPUT));
    input.type=INPUT_MOUSE;
    
    if (passed){
        if (button==1) {input.mi.dwFlags=MOUSEEVENTF_LEFTDOWN;}
        if (button==2) {input.mi.dwFlags=MOUSEEVENTF_RIGHTDOWN;}
        if (button==3) {input.mi.dwFlags=MOUSEEVENTF_MIDDLEDOWN;}
        SendInput(1,&input,sizeof(INPUT));

        ZeroMemory(&input,sizeof(INPUT));
        input.type=INPUT_MOUSE;

        if (button==1) {input.mi.dwFlags=MOUSEEVENTF_LEFTUP;}
        if (button==2) {input.mi.dwFlags=MOUSEEVENTF_RIGHTUP;}
        if (button==3) {input.mi.dwFlags=MOUSEEVENTF_MIDDLEUP;}
    }else {
        input.mi.dwFlags=MOUSEEVENTF_LEFTDOWN;

        SendInput(1,&input,sizeof(INPUT));

        ZeroMemory(&input,sizeof(INPUT));
        input.type=INPUT_MOUSE;

        input.mi.dwFlags=MOUSEEVENTF_LEFTUP;
    }
    SendInput(1,&input,sizeof(INPUT));

#endif

#ifdef QB64_MACOSX
    CGEventRef click1_down = CGEventCreateMouseEvent(
                             NULL, kCGEventLeftMouseDown,
                             CGPointMake(x, y),
                             kCGMouseButtonLeft
                             );
    CGEventRef click1_up = CGEventCreateMouseEvent(
                           NULL, kCGEventLeftMouseUp,
                           CGPointMake(x, y),
                           kCGMouseButtonLeft
                           );
    CGEventPost(kCGHIDEventTap, click1_down);
    CGEventPost(kCGHIDEventTap, click1_up);
    CFRelease(click1_up);
    CFRelease(click1_down);
#endif

  }

#ifdef QB64_MACOSX
  //128 indicates SHIFT must be held to achieve the indexed ASCII character
  static uint16 ASCII_TO_MACVK[]={
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0x3300+8,
    0x3000+9,
    0,
    0,
    0,
    0x2400+13,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0x3500+27,
    0,
    0,
    0,
    0,
    0x3100+32,
    0x1200+33+128,
    0x2700+34+128,
    0x1400+35+128,
    0x1500+36+128,
    0x1700+37+128,
    0x1A00+38+128,
    0x2700+39,
    0x1900+40+128,
    0x1D00+41+128,
    0x1C00+42+128,
    0x1800+43+128,
    0x2B00+44,
    0x1B00+45,
    0x2F00+46,
    0x2C00+47,
    0x1D00+48,
    0x1200+49,
    0x1300+50,
    0x1400+51,
    0x1500+52,
    0x1700+53,
    0x1600+54,
    0x1A00+55,
    0x1C00+56,
    0x1900+57,
    0x2900+58+128,
    0x2900+59,
    0x2B00+60+128,
    0x1800+61,
    0x2F00+62+128,
    0x2C00+63+128,
    0x1300+64+128,
    0x0000+65+128,
    0x0B00+66+128,
    0x0800+67+128,
    0x0200+68+128,
    0x0E00+69+128,
    0x0300+70+128,
    0x0500+71+128,
    0x0400+72+128,
    0x2200+73+128,
    0x2600+74+128,
    0x2800+75+128,
    0x2500+76+128,
    0x2E00+77+128,
    0x2D00+78+128,
    0x1F00+79+128,
    0x2300+80+128,
    0x0C00+81+128,
    0x0F00+82+128,
    0x0100+83+128,
    0x1100+84+128,
    0x2000+85+128,
    0x0900+86+128,
    0x0D00+87+128,
    0x0700+88+128,
    0x1000+89+128,
    0x0600+90+128,
    0x2100+91,
    0x2A00+92,
    0x1E00+93,
    0x1600+94+128,
    0x1B00+95+128,
    0x3200+96,
    0x0000+65+32,
    0x0B00+66+32,
    0x0800+67+32,
    0x0200+68+32,
    0x0E00+69+32,
    0x0300+70+32,
    0x0500+71+32,
    0x0400+72+32,
    0x2200+73+32,
    0x2600+74+32,
    0x2800+75+32,
    0x2500+76+32,
    0x2E00+77+32,
    0x2D00+78+32,
    0x1F00+79+32,
    0x2300+80+32,
    0x0C00+81+32,
    0x0F00+82+32,
    0x0100+83+32,
    0x1100+84+32,
    0x2000+85+32,
    0x0900+86+32,
    0x0D00+87+32,
    0x0700+88+32,
    0x1000+89+32,
    0x0600+90+32,
    0x2100+123+128,
    0x2A00+124+128,
    0x1E00+125+128,
    0x3200+126+128,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0
  };
#endif

  int32 MACVK_ANSI_A                    = 0x00;
  int32 MACVK_ANSI_S                    = 0x01;
  int32 MACVK_ANSI_D                    = 0x02;
  int32 MACVK_ANSI_F                    = 0x03;
  int32 MACVK_ANSI_H                    = 0x04;
  int32 MACVK_ANSI_G                    = 0x05;
  int32 MACVK_ANSI_Z                    = 0x06;
  int32 MACVK_ANSI_X                    = 0x07;
  int32 MACVK_ANSI_C                    = 0x08;
  int32 MACVK_ANSI_V                    = 0x09;
  int32 MACVK_ANSI_B                    = 0x0B;
  int32 MACVK_ANSI_Q                    = 0x0C;
  int32 MACVK_ANSI_W                    = 0x0D;
  int32 MACVK_ANSI_E                    = 0x0E;
  int32 MACVK_ANSI_R                    = 0x0F;
  int32 MACVK_ANSI_Y                    = 0x10;
  int32 MACVK_ANSI_T                    = 0x11;
  int32 MACVK_ANSI_1                    = 0x12;
  int32 MACVK_ANSI_2                    = 0x13;
  int32 MACVK_ANSI_3                    = 0x14;
  int32 MACVK_ANSI_4                    = 0x15;
  int32 MACVK_ANSI_6                    = 0x16;
  int32 MACVK_ANSI_5                    = 0x17;
  int32 MACVK_ANSI_Equal                = 0x18;
  int32 MACVK_ANSI_9                    = 0x19;
  int32 MACVK_ANSI_7                    = 0x1A;
  int32 MACVK_ANSI_Minus                = 0x1B;
  int32 MACVK_ANSI_8                    = 0x1C;
  int32 MACVK_ANSI_0                    = 0x1D;
  int32 MACVK_ANSI_RightBracket         = 0x1E;
  int32 MACVK_ANSI_O                    = 0x1F;
  int32 MACVK_ANSI_U                    = 0x20;
  int32 MACVK_ANSI_LeftBracket          = 0x21;
  int32 MACVK_ANSI_I                    = 0x22;
  int32 MACVK_ANSI_P                    = 0x23;
  int32 MACVK_ANSI_L                    = 0x25;
  int32 MACVK_ANSI_J                    = 0x26;
  int32 MACVK_ANSI_Quote                = 0x27;
  int32 MACVK_ANSI_K                    = 0x28;
  int32 MACVK_ANSI_Semicolon            = 0x29;
  int32 MACVK_ANSI_Backslash            = 0x2A;
  int32 MACVK_ANSI_Comma                = 0x2B;
  int32 MACVK_ANSI_Slash                = 0x2C;
  int32 MACVK_ANSI_N                    = 0x2D;
  int32 MACVK_ANSI_M                    = 0x2E;
  int32 MACVK_ANSI_Period               = 0x2F;
  int32 MACVK_ANSI_Grave                = 0x32;
  int32 MACVK_ANSI_KeypadDecimal        = 0x41;
  int32 MACVK_ANSI_KeypadMultiply       = 0x43;
  int32 MACVK_ANSI_KeypadPlus           = 0x45;
  int32 MACVK_ANSI_KeypadClear          = 0x47;
  int32 MACVK_ANSI_KeypadDivide         = 0x4B;
  int32 MACVK_ANSI_KeypadEnter          = 0x4C;
  int32 MACVK_ANSI_KeypadMinus          = 0x4E;
  int32 MACVK_ANSI_KeypadEquals         = 0x51;
  int32 MACVK_ANSI_Keypad0              = 0x52;
  int32 MACVK_ANSI_Keypad1              = 0x53;
  int32 MACVK_ANSI_Keypad2              = 0x54;
  int32 MACVK_ANSI_Keypad3              = 0x55;
  int32 MACVK_ANSI_Keypad4              = 0x56;
  int32 MACVK_ANSI_Keypad5              = 0x57;
  int32 MACVK_ANSI_Keypad6              = 0x58;
  int32 MACVK_ANSI_Keypad7              = 0x59;
  int32 MACVK_ANSI_Keypad8              = 0x5B;
  int32 MACVK_ANSI_Keypad9              = 0x5C;
  int32 MACVK_Return                    = 0x24;
  int32 MACVK_Tab                       = 0x30;
  int32 MACVK_Space                     = 0x31;
  int32 MACVK_Delete                    = 0x33;
  int32 MACVK_Escape                    = 0x35;
  int32 MACVK_Command                   = 0x37;
  int32 MACVK_Shift                     = 0x38;
  int32 MACVK_CapsLock                  = 0x39;
  int32 MACVK_Option                    = 0x3A;
  int32 MACVK_Control                   = 0x3B;
  int32 MACVK_RightShift                = 0x3C;
  int32 MACVK_RightOption               = 0x3D;
  int32 MACVK_RightControl              = 0x3E;
  int32 MACVK_Function                  = 0x3F;
  int32 MACVK_F17                       = 0x40;
  int32 MACVK_VolumeUp                  = 0x48;
  int32 MACVK_VolumeDown                = 0x49;
  int32 MACVK_Mute                      = 0x4A;
  int32 MACVK_F18                       = 0x4F;
  int32 MACVK_F19                       = 0x50;
  int32 MACVK_F20                       = 0x5A;
  int32 MACVK_F5                        = 0x60;
  int32 MACVK_F6                        = 0x61;
  int32 MACVK_F7                        = 0x62;
  int32 MACVK_F3                        = 0x63;
  int32 MACVK_F8                        = 0x64;
  int32 MACVK_F9                        = 0x65;
  int32 MACVK_F11                       = 0x67;
  int32 MACVK_F13                       = 0x69;
  int32 MACVK_F16                       = 0x6A;
  int32 MACVK_F14                       = 0x6B;
  int32 MACVK_F10                       = 0x6D;
  int32 MACVK_F12                       = 0x6F;
  int32 MACVK_F15                       = 0x71;
  int32 MACVK_Help                      = 0x72;
  int32 MACVK_Home                      = 0x73;
  int32 MACVK_PageUp                    = 0x74;
  int32 MACVK_ForwardDelete             = 0x75;
  int32 MACVK_F4                        = 0x76;
  int32 MACVK_End                       = 0x77;
  int32 MACVK_F2                        = 0x78;
  int32 MACVK_PageDown                  = 0x79;
  int32 MACVK_F1                        = 0x7A;
  int32 MACVK_LeftArrow                 = 0x7B;
  int32 MACVK_RightArrow                = 0x7C;
  int32 MACVK_DownArrow                 = 0x7D;
  int32 MACVK_UpArrow                   = 0x7E;
  int32 MACVK_ISO_Section               = 0x0A;
  int32 MACVK_JIS_Yen                   = 0x5D;
  int32 MACVK_JIS_Underscore            = 0x5E;
  int32 MACVK_JIS_KeypadComma           = 0x5F;
  int32 MACVK_JIS_Eisu                  = 0x66;
  int32 MACVK_JIS_Kana                  = 0x68;



  void sub__screenprint(qbs *txt){

    if (cloud_app){error(262); return;}

    static int32 i,s,x,vk,c;

#ifdef QB64_MACOSX
    /* MACOSX virtual key code reference (with ASCII value & shift state):
       static int32 MACVK_ANSI_A                    = 0x0000+65+32;
       static int32 MACVK_ANSI_S                    = 0x0100+83+32;
       static int32 MACVK_ANSI_D                    = 0x0200+68+32;
       static int32 MACVK_ANSI_F                    = 0x0300+70+32;
       static int32 MACVK_ANSI_H                    = 0x0400+72+32;
       static int32 MACVK_ANSI_G                    = 0x0500+71+32;
       static int32 MACVK_ANSI_Z                    = 0x0600+90+32;
       static int32 MACVK_ANSI_X                    = 0x0700+88+32;
       static int32 MACVK_ANSI_C                    = 0x0800+67+32;
       static int32 MACVK_ANSI_V                    = 0x0900+86+32;
       static int32 MACVK_ANSI_B                    = 0x0B00+66+32;
       static int32 MACVK_ANSI_Q                    = 0x0C00+81+32;
       static int32 MACVK_ANSI_W                    = 0x0D00+87+32;
       static int32 MACVK_ANSI_E                    = 0x0E00+69+32;
       static int32 MACVK_ANSI_R                    = 0x0F00+82+32;
       static int32 MACVK_ANSI_Y                    = 0x1000+89+32;
       static int32 MACVK_ANSI_T                    = 0x1100+84+32;
       static int32 MACVK_ANSI_1                    = 0x1200+49;
       static int32 MACVK_ANSI_2                    = 0x1300+50;
       static int32 MACVK_ANSI_3                    = 0x1400+51;
       static int32 MACVK_ANSI_4                    = 0x1500+52;
       static int32 MACVK_ANSI_6                    = 0x1600+54;
       static int32 MACVK_ANSI_5                    = 0x1700+53;
       static int32 MACVK_ANSI_Equal                = 0x1800+61;
       static int32 MACVK_ANSI_9                    = 0x1900+57;
       static int32 MACVK_ANSI_7                    = 0x1A00+55;
       static int32 MACVK_ANSI_Minus                = 0x1B00+45;
       static int32 MACVK_ANSI_8                    = 0x1C00+56;
       static int32 MACVK_ANSI_0                    = 0x1D00+48;
       static int32 MACVK_ANSI_RightBracket         = 0x1E00+93;
       static int32 MACVK_ANSI_O                    = 0x1F00+79+32;
       static int32 MACVK_ANSI_U                    = 0x2000+85+32;
       static int32 MACVK_ANSI_LeftBracket          = 0x2100+91;
       static int32 MACVK_ANSI_I                    = 0x2200+73+32;
       static int32 MACVK_ANSI_P                    = 0x2300+80+32;
       static int32 MACVK_ANSI_L                    = 0x2500+76+32;
       static int32 MACVK_ANSI_J                    = 0x2600+74+32;
       static int32 MACVK_ANSI_Quote                = 0x2700+39;
       static int32 MACVK_ANSI_K                    = 0x2800+75+32;
       static int32 MACVK_ANSI_Semicolon            = 0x2900+59;
       static int32 MACVK_ANSI_Backslash            = 0x2A00+92;
       static int32 MACVK_ANSI_Comma                = 0x2B00+44;
       static int32 MACVK_ANSI_Slash                = 0x2C00+47;
       static int32 MACVK_ANSI_N                    = 0x2D00+78+32;
       static int32 MACVK_ANSI_M                    = 0x2E00+77+32;
       static int32 MACVK_ANSI_Period               = 0x2F00+46;
       static int32 MACVK_ANSI_Grave                = 0x3200+96;
       static int32 MACVK_ANSI_KeypadDecimal        = 0x4100;
       static int32 MACVK_ANSI_KeypadMultiply       = 0x4300;
       static int32 MACVK_ANSI_KeypadPlus           = 0x4500;
       static int32 MACVK_ANSI_KeypadClear          = 0x4700;
       static int32 MACVK_ANSI_KeypadDivide         = 0x4B00;
       static int32 MACVK_ANSI_KeypadEnter          = 0x4C00;
       static int32 MACVK_ANSI_KeypadMinus          = 0x4E00;
       static int32 MACVK_ANSI_KeypadEquals         = 0x5100;
       static int32 MACVK_ANSI_Keypad0              = 0x5200;
       static int32 MACVK_ANSI_Keypad1              = 0x5300;
       static int32 MACVK_ANSI_Keypad2              = 0x5400;
       static int32 MACVK_ANSI_Keypad3              = 0x5500;
       static int32 MACVK_ANSI_Keypad4              = 0x5600;
       static int32 MACVK_ANSI_Keypad5              = 0x5700;
       static int32 MACVK_ANSI_Keypad6              = 0x5800;
       static int32 MACVK_ANSI_Keypad7              = 0x5900;
       static int32 MACVK_ANSI_Keypad8              = 0x5B00;
       static int32 MACVK_ANSI_Keypad9              = 0x5C00;
       static int32 MACVK_Return                    = 0x2400+13;
       static int32 MACVK_Tab                       = 0x3000+9;
       static int32 MACVK_Space                     = 0x3100+32;
       static int32 MACVK_Delete                    = 0x3300+8;
       static int32 MACVK_Escape                    = 0x3500+27;
       static int32 MACVK_Command                   = 0x3700;
       static int32 MACVK_Shift                     = 0x3800;
       static int32 MACVK_CapsLock                  = 0x3900;
       static int32 MACVK_Option                    = 0x3A00;
       static int32 MACVK_Control                   = 0x3B00;
       static int32 MACVK_RightShift                = 0x3C00;
       static int32 MACVK_RightOption               = 0x3D00;
       static int32 MACVK_RightControl              = 0x3E00;
       static int32 MACVK_Function                  = 0x3F00;
       static int32 MACVK_F17                       = 0x4000;
       static int32 MACVK_VolumeUp                  = 0x4800;
       static int32 MACVK_VolumeDown                = 0x4900;
       static int32 MACVK_Mute                      = 0x4A00;
       static int32 MACVK_F18                       = 0x4F00;
       static int32 MACVK_F19                       = 0x5000;
       static int32 MACVK_F20                       = 0x5A00;
       static int32 MACVK_F5                        = 0x6000;
       static int32 MACVK_F6                        = 0x6100;
       static int32 MACVK_F7                        = 0x6200;
       static int32 MACVK_F3                        = 0x6300;
       static int32 MACVK_F8                        = 0x6400;
       static int32 MACVK_F9                        = 0x6500;
       static int32 MACVK_F11                       = 0x6700;
       static int32 MACVK_F13                       = 0x6900;
       static int32 MACVK_F16                       = 0x6A00;
       static int32 MACVK_F14                       = 0x6B00;
       static int32 MACVK_F10                       = 0x6D00;
       static int32 MACVK_F12                       = 0x6F00;
       static int32 MACVK_F15                       = 0x7100;
       static int32 MACVK_Help                      = 0x7200;
       static int32 MACVK_Home                      = 0x7300;
       static int32 MACVK_PageUp                    = 0x7400;
       static int32 MACVK_ForwardDelete             = 0x7500;
       static int32 MACVK_F4                        = 0x7600;
       static int32 MACVK_End                       = 0x7700;
       static int32 MACVK_F2                        = 0x7800;
       static int32 MACVK_PageDown                  = 0x7900;
       static int32 MACVK_F1                        = 0x7A00;
       static int32 MACVK_LeftArrow                 = 0x7B00;
       static int32 MACVK_RightArrow                = 0x7C00;
       static int32 MACVK_DownArrow                 = 0x7D00;
       static int32 MACVK_UpArrow                   = 0x7E00;
       static int32 MACVK_ISO_Section               = 0x0A00;
       static int32 MACVK_JIS_Yen                   = 0x5D00;
       static int32 MACVK_JIS_Underscore            = 0x5E00;
       static int32 MACVK_JIS_KeypadComma           = 0x5F00;
       static int32 MACVK_JIS_Eisu                  = 0x6600;
       static int32 MACVK_JIS_Kana                  = 0x6800;
       static int32 MACVKS_ANSI_A                    = 0x0000+65+128;
       static int32 MACVKS_ANSI_S                    = 0x0100+83+128;
       static int32 MACVKS_ANSI_D                    = 0x0200+68+128;
       static int32 MACVKS_ANSI_F                    = 0x0300+70+128;
       static int32 MACVKS_ANSI_H                    = 0x0400+72+128;
       static int32 MACVKS_ANSI_G                    = 0x0500+71+128;
       static int32 MACVKS_ANSI_Z                    = 0x0600+90+128;
       static int32 MACVKS_ANSI_X                    = 0x0700+88+128;
       static int32 MACVKS_ANSI_C                    = 0x0800+67+128;
       static int32 MACVKS_ANSI_V                    = 0x0900+86+128;
       static int32 MACVKS_ANSI_B                    = 0x0B00+66+128;
       static int32 MACVKS_ANSI_Q                    = 0x0C00+81+128;

       static int32 MACVKS_ANSI_W                    = 0x0D00+87+128;
       static int32 MACVKS_ANSI_E                    = 0x0E00+69+128;
       static int32 MACVKS_ANSI_R                    = 0x0F00+82+128;
       static int32 MACVKS_ANSI_Y                    = 0x1000+89+128;
       static int32 MACVKS_ANSI_T                    = 0x1100+84+128;
       static int32 MACVKS_ANSI_1                    = 0x1200+33+128;
       static int32 MACVKS_ANSI_2                    = 0x1300+64+128;
       static int32 MACVKS_ANSI_3                    = 0x1400+35+128;
       static int32 MACVKS_ANSI_4                    = 0x1500+36+128;
       static int32 MACVKS_ANSI_6                    = 0x1600+94+128;
       static int32 MACVKS_ANSI_5                    = 0x1700+37+128;
       static int32 MACVKS_ANSI_Equal                = 0x1800+43+128;
       static int32 MACVKS_ANSI_9                    = 0x1900+40+128;
       static int32 MACVKS_ANSI_7                    = 0x1A00+38+128;
       static int32 MACVKS_ANSI_Minus                = 0x1B00+95+128;
       static int32 MACVKS_ANSI_8                    = 0x1C00+42+128;
       static int32 MACVKS_ANSI_0                    = 0x1D00+41+128;
       static int32 MACVKS_ANSI_RightBracket         = 0x1E00+125+128;
       static int32 MACVKS_ANSI_O                    = 0x1F00+79+128;
       static int32 MACVKS_ANSI_U                    = 0x2000+85+128;
       static int32 MACVKS_ANSI_LeftBracket          = 0x2100+123+128;
       static int32 MACVKS_ANSI_I                    = 0x2200+73+128;
       static int32 MACVKS_ANSI_P                    = 0x2300+80+128;
       static int32 MACVKS_ANSI_L                    = 0x2500+76+128;
       static int32 MACVKS_ANSI_J                    = 0x2600+74+128;
       static int32 MACVKS_ANSI_Quote                = 0x2700+34+128;
       static int32 MACVKS_ANSI_K                    = 0x2800+75+128;
       static int32 MACVKS_ANSI_Semicolon            = 0x2900+58+128;
       static int32 MACVKS_ANSI_Backslash            = 0x2A00+124+128;
       static int32 MACVKS_ANSI_Comma                = 0x2B00+60+128;
       static int32 MACVKS_ANSI_Slash                = 0x2C00+63+128;
       static int32 MACVKS_ANSI_N                    = 0x2D00+78+128;
       static int32 MACVKS_ANSI_M                    = 0x2E00+77+128;
       static int32 MACVKS_ANSI_Period               = 0x2F00+62+128;
       static int32 MACVKS_ANSI_Grave                = 0x3200+126+128;
    */


    static CGEventSourceRef es;
    static CGEventRef e;
 
    for (i=0;i<txt->len;i++){
      c=txt->chr[i];

      //static int32 i,s,x,vk,c;

      /*
    CONTROL+{A-Z} 
    The following 'x' letters cannot be simulated this way because they map to implemented control code (8,9,13) functionality:
    ABCDEFGHIJKLMNOPQRSTUVWXYZ
    .......xx...x.............
    Common/standard CTRL+? combinations for copying, pasting, undoing, cutting, etc. are available
      */

      if ((c>=1)&&(c<=26)){ if ((c!=8)&&(c!=9)&&(c!=13)){
      //Note: Under MacOSX, COMMAND is used instead of control for general tasks
      vk=ASCII_TO_MACVK[c+96]>>8;
      es=CGEventSourceCreate(kCGEventSourceStateCombinedSessionState);
      e=CGEventCreateKeyboardEvent(es,(CGKeyCode)MACVK_Command,1); CGEventPost(kCGAnnotatedSessionEventTap,e); CFRelease(e);
      e=CGEventCreateKeyboardEvent(es,(CGKeyCode)vk,1); CGEventSetFlags(e, kCGEventFlagMaskCommand);
      CGEventPost(kCGAnnotatedSessionEventTap,e); CFRelease(e);
      e=CGEventCreateKeyboardEvent(es,(CGKeyCode)vk,0); CGEventPost(kCGAnnotatedSessionEventTap,e); CFRelease(e);
      e=CGEventCreateKeyboardEvent(es,(CGKeyCode)MACVK_Command,0); CGEventPost(kCGAnnotatedSessionEventTap,e); CFRelease(e);
      goto special_character;
    }}

      //custom extended characters
      if (c==0){
    if (i==(txt->len-1)) goto special_character;
    i++;
    c=txt->chr[i];
    if (c==15){//SHIFT+TAB
      vk=MACVK_Tab;
      es=CGEventSourceCreate(kCGEventSourceStateCombinedSessionState);
      e=CGEventCreateKeyboardEvent(es,(CGKeyCode)MACVK_Shift,1); CGEventPost(kCGAnnotatedSessionEventTap,e); CFRelease(e);
      e=CGEventCreateKeyboardEvent(es,(CGKeyCode)vk,1); CGEventSetFlags(e, kCGEventFlagMaskShift);
      CGEventPost(kCGAnnotatedSessionEventTap,e); CFRelease(e);
      e=CGEventCreateKeyboardEvent(es,(CGKeyCode)vk,0); CGEventPost(kCGAnnotatedSessionEventTap,e); CFRelease(e);
      e=CGEventCreateKeyboardEvent(es,(CGKeyCode)MACVK_Shift,0); CGEventPost(kCGAnnotatedSessionEventTap,e); CFRelease(e);
      CFRelease(es);
    }
    //4 arrows
    if (c==75){
      vk=MACVK_LeftArrow;
      es=CGEventSourceCreate(kCGEventSourceStateCombinedSessionState);
      e=CGEventCreateKeyboardEvent(es,(CGKeyCode)vk,1); CGEventPost(kCGAnnotatedSessionEventTap,e); CFRelease(e);
      e=CGEventCreateKeyboardEvent(es,(CGKeyCode)vk,0); CGEventPost(kCGAnnotatedSessionEventTap,e); CFRelease(e);
      CFRelease(es);
    }
    if (c==77){
      vk=MACVK_RightArrow;
      es=CGEventSourceCreate(kCGEventSourceStateCombinedSessionState);
      e=CGEventCreateKeyboardEvent(es,(CGKeyCode)vk,1); CGEventPost(kCGAnnotatedSessionEventTap,e); CFRelease(e);
      e=CGEventCreateKeyboardEvent(es,(CGKeyCode)vk,0); CGEventPost(kCGAnnotatedSessionEventTap,e); CFRelease(e);
      CFRelease(es);
    }
    if (c==72){
      vk=MACVK_UpArrow;
      es=CGEventSourceCreate(kCGEventSourceStateCombinedSessionState);
      e=CGEventCreateKeyboardEvent(es,(CGKeyCode)vk,1); CGEventPost(kCGAnnotatedSessionEventTap,e); CFRelease(e);
      e=CGEventCreateKeyboardEvent(es,(CGKeyCode)vk,0); CGEventPost(kCGAnnotatedSessionEventTap,e); CFRelease(e);
      CFRelease(es);
    }
    if (c==80){
      vk=MACVK_DownArrow;
      es=CGEventSourceCreate(kCGEventSourceStateCombinedSessionState);
      e=CGEventCreateKeyboardEvent(es,(CGKeyCode)vk,1); CGEventPost(kCGAnnotatedSessionEventTap,e); CFRelease(e);
      e=CGEventCreateKeyboardEvent(es,(CGKeyCode)vk,0); CGEventPost(kCGAnnotatedSessionEventTap,e); CFRelease(e);
      CFRelease(es);
    }
    //6 control keys
    if (c==82){
      vk=MACVK_Help;
      es=CGEventSourceCreate(kCGEventSourceStateCombinedSessionState);
      e=CGEventCreateKeyboardEvent(es,(CGKeyCode)vk,1); CGEventPost(kCGAnnotatedSessionEventTap,e); CFRelease(e);
      e=CGEventCreateKeyboardEvent(es,(CGKeyCode)vk,0); CGEventPost(kCGAnnotatedSessionEventTap,e); CFRelease(e);
      CFRelease(es);
    }
    if (c==71){
      vk=MACVK_Home;
      es=CGEventSourceCreate(kCGEventSourceStateCombinedSessionState);
      e=CGEventCreateKeyboardEvent(es,(CGKeyCode)vk,1); CGEventPost(kCGAnnotatedSessionEventTap,e); CFRelease(e);
      e=CGEventCreateKeyboardEvent(es,(CGKeyCode)vk,0); CGEventPost(kCGAnnotatedSessionEventTap,e); CFRelease(e);
      CFRelease(es);
    }
    if (c==83){
      vk=MACVK_ForwardDelete;
      es=CGEventSourceCreate(kCGEventSourceStateCombinedSessionState);
      e=CGEventCreateKeyboardEvent(es,(CGKeyCode)vk,1); CGEventPost(kCGAnnotatedSessionEventTap,e); CFRelease(e);
      e=CGEventCreateKeyboardEvent(es,(CGKeyCode)vk,0); CGEventPost(kCGAnnotatedSessionEventTap,e); CFRelease(e);
      CFRelease(es);
    }
    if (c==79){
      vk=MACVK_End;
      es=CGEventSourceCreate(kCGEventSourceStateCombinedSessionState);
      e=CGEventCreateKeyboardEvent(es,(CGKeyCode)vk,1); CGEventPost(kCGAnnotatedSessionEventTap,e); CFRelease(e);
      e=CGEventCreateKeyboardEvent(es,(CGKeyCode)vk,0); CGEventPost(kCGAnnotatedSessionEventTap,e); CFRelease(e);
      CFRelease(es);
    }
    if (c==81){
      vk=MACVK_PageDown;
      es=CGEventSourceCreate(kCGEventSourceStateCombinedSessionState);
      e=CGEventCreateKeyboardEvent(es,(CGKeyCode)vk,1); CGEventPost(kCGAnnotatedSessionEventTap,e); CFRelease(e);
      e=CGEventCreateKeyboardEvent(es,(CGKeyCode)vk,0); CGEventPost(kCGAnnotatedSessionEventTap,e); CFRelease(e);
      CFRelease(es);
    }
    if (c==73){
      vk=MACVK_PageUp;
      es=CGEventSourceCreate(kCGEventSourceStateCombinedSessionState);
      e=CGEventCreateKeyboardEvent(es,(CGKeyCode)vk,1); CGEventPost(kCGAnnotatedSessionEventTap,e); CFRelease(e);
      e=CGEventCreateKeyboardEvent(es,(CGKeyCode)vk,0); CGEventPost(kCGAnnotatedSessionEventTap,e); CFRelease(e);
      CFRelease(es);
    }
    //...
    //todo: F1-F12, shift/control/alt+above
    goto special_character;
      }

      //standard ASCII character output
      x=ASCII_TO_MACVK[c];
      if (x&127){//available character
    vk=x>>8;
    if (x&128){
      es=CGEventSourceCreate(kCGEventSourceStateCombinedSessionState);
      e=CGEventCreateKeyboardEvent(es,(CGKeyCode)MACVK_Shift,1); CGEventPost(kCGAnnotatedSessionEventTap,e); CFRelease(e);
      e=CGEventCreateKeyboardEvent(es,(CGKeyCode)vk,1); CGEventSetFlags(e, kCGEventFlagMaskShift);
      CGEventPost(kCGAnnotatedSessionEventTap,e); CFRelease(e);
      e=CGEventCreateKeyboardEvent(es,(CGKeyCode)vk,0); CGEventPost(kCGAnnotatedSessionEventTap,e); CFRelease(e);
      e=CGEventCreateKeyboardEvent(es,(CGKeyCode)MACVK_Shift,0); CGEventPost(kCGAnnotatedSessionEventTap,e); CFRelease(e);
      CFRelease(es);
    }else{
      es=CGEventSourceCreate(kCGEventSourceStateCombinedSessionState);
      e=CGEventCreateKeyboardEvent(es,(CGKeyCode)vk,1); CGEventPost(kCGAnnotatedSessionEventTap,e); CFRelease(e);
      e=CGEventCreateKeyboardEvent(es,(CGKeyCode)vk,0); CGEventPost(kCGAnnotatedSessionEventTap,e); CFRelease(e);
      CFRelease(es);
    }
      }//available character

    special_character:;

    }//i

#endif //QB64_MACOSX



#ifdef QB64_WINDOWS

    static INPUT input;

    /*VK reference:
      http://msdn.microsoft.com/en-us/library/ms927178.aspx
    */

    for (i=0;i<txt->len;i++){
      c=txt->chr[i];
      //custom characters
      if (c==9){ZeroMemory(&input,sizeof(INPUT)); input.ki.wVk=VK_TAB; input.type=INPUT_KEYBOARD; SendInput(1,&input,sizeof(INPUT)); input.ki.dwFlags = KEYEVENTF_KEYUP; SendInput(1,&input,sizeof(INPUT)); goto special_character;}
      if (c==8){ZeroMemory(&input,sizeof(INPUT)); input.ki.wVk=VK_BACK; input.type=INPUT_KEYBOARD; SendInput(1,&input,sizeof(INPUT)); input.ki.dwFlags = KEYEVENTF_KEYUP; SendInput(1,&input,sizeof(INPUT)); goto special_character;}
      if (c==13){ZeroMemory(&input,sizeof(INPUT)); input.ki.wVk=VK_RETURN; input.type=INPUT_KEYBOARD; SendInput(1,&input,sizeof(INPUT)); input.ki.dwFlags = KEYEVENTF_KEYUP; SendInput(1,&input,sizeof(INPUT)); goto special_character;}
      //...

      /*
    CONTROL+{A-Z} 
    The following 'x' letters cannot be simulated this way because they map to above functionality:
    ABCDEFGHIJKLMNOPQRSTUVWXYZ
    .......xx...x.............
    Common/standard CTRL+? combinations for copying, pasting, undoing, cutting, etc. are available
      */
      if ((c>=1)&&(c<=26)){
    ZeroMemory(&input,sizeof(INPUT)); input.type=INPUT_KEYBOARD; input.ki.wVk=VK_CONTROL; SendInput(1,&input,sizeof(INPUT));
    ZeroMemory(&input,sizeof(INPUT)); input.ki.wVk=VkKeyScan(64+c)&255; input.type=INPUT_KEYBOARD; SendInput(1,&input,sizeof(INPUT)); input.ki.dwFlags = KEYEVENTF_KEYUP; SendInput(1,&input,sizeof(INPUT));
    ZeroMemory(&input,sizeof(INPUT)); input.type=INPUT_KEYBOARD; input.ki.wVk=VK_CONTROL; input.ki.dwFlags = KEYEVENTF_KEYUP; SendInput(1,&input,sizeof(INPUT));
    goto special_character;
      }

      //custom extended characters
      if (c==0){
    if (i==(txt->len-1)) goto special_character;
    i++;
    c=txt->chr[i];
    if (c==15){//SHIFT+TAB
      ZeroMemory(&input,sizeof(INPUT)); input.type=INPUT_KEYBOARD; input.ki.wVk=VK_SHIFT; SendInput(1,&input,sizeof(INPUT));
      ZeroMemory(&input,sizeof(INPUT)); input.ki.wVk=VK_TAB; input.type=INPUT_KEYBOARD; SendInput(1,&input,sizeof(INPUT)); input.ki.dwFlags = KEYEVENTF_KEYUP; SendInput(1,&input,sizeof(INPUT));
      ZeroMemory(&input,sizeof(INPUT)); input.type=INPUT_KEYBOARD; input.ki.wVk=VK_SHIFT; input.ki.dwFlags = KEYEVENTF_KEYUP; SendInput(1,&input,sizeof(INPUT));
    }
    if (c==75){ZeroMemory(&input,sizeof(INPUT)); input.ki.wVk=VK_LEFT; input.type=INPUT_KEYBOARD; SendInput(1,&input,sizeof(INPUT)); input.ki.dwFlags = KEYEVENTF_KEYUP; SendInput(1,&input,sizeof(INPUT));}
    if (c==77){ZeroMemory(&input,sizeof(INPUT)); input.ki.wVk=VK_RIGHT; input.type=INPUT_KEYBOARD; SendInput(1,&input,sizeof(INPUT)); input.ki.dwFlags = KEYEVENTF_KEYUP; SendInput(1,&input,sizeof(INPUT));}
    if (c==72){ZeroMemory(&input,sizeof(INPUT)); input.ki.wVk=VK_UP; input.type=INPUT_KEYBOARD; SendInput(1,&input,sizeof(INPUT)); input.ki.dwFlags = KEYEVENTF_KEYUP; SendInput(1,&input,sizeof(INPUT));}
    if (c==80){ZeroMemory(&input,sizeof(INPUT)); input.ki.wVk=VK_DOWN; input.type=INPUT_KEYBOARD; SendInput(1,&input,sizeof(INPUT)); input.ki.dwFlags = KEYEVENTF_KEYUP; SendInput(1,&input,sizeof(INPUT));}
    if (c==82){ZeroMemory(&input,sizeof(INPUT)); input.ki.wVk=VK_INSERT; input.type=INPUT_KEYBOARD; SendInput(1,&input,sizeof(INPUT)); input.ki.dwFlags = KEYEVENTF_KEYUP; SendInput(1,&input,sizeof(INPUT));}
    if (c==71){ZeroMemory(&input,sizeof(INPUT)); input.ki.wVk=VK_HOME; input.type=INPUT_KEYBOARD; SendInput(1,&input,sizeof(INPUT)); input.ki.dwFlags = KEYEVENTF_KEYUP; SendInput(1,&input,sizeof(INPUT));}
    if (c==83){ZeroMemory(&input,sizeof(INPUT)); input.ki.wVk=VK_DELETE; input.type=INPUT_KEYBOARD; SendInput(1,&input,sizeof(INPUT)); input.ki.dwFlags = KEYEVENTF_KEYUP; SendInput(1,&input,sizeof(INPUT));}
    if (c==79){ZeroMemory(&input,sizeof(INPUT)); input.ki.wVk=VK_END; input.type=INPUT_KEYBOARD; SendInput(1,&input,sizeof(INPUT)); input.ki.dwFlags = KEYEVENTF_KEYUP; SendInput(1,&input,sizeof(INPUT));}
    if (c==81){ZeroMemory(&input,sizeof(INPUT)); input.ki.wVk=VK_NEXT; input.type=INPUT_KEYBOARD; SendInput(1,&input,sizeof(INPUT)); input.ki.dwFlags = KEYEVENTF_KEYUP; SendInput(1,&input,sizeof(INPUT));}
    if (c==73){ZeroMemory(&input,sizeof(INPUT)); input.ki.wVk=VK_PRIOR; input.type=INPUT_KEYBOARD; SendInput(1,&input,sizeof(INPUT)); input.ki.dwFlags = KEYEVENTF_KEYUP; SendInput(1,&input,sizeof(INPUT));}
    //...
    //todo: F1-F12, shift/control/alt+above

    goto special_character;
      }

      if ((c>126)||(c<32)) goto special_character;

      x=VkKeyScan(txt->chr[i]);
      vk=x&255;

      s=(x>>8)&255;
      //1 Either shift key is pressed. 
      //2 Either CTRL key is pressed. 
      //4 Either ALT key is pressed. 
      if (s&1){
    ZeroMemory(&input,sizeof(INPUT));
    input.type=INPUT_KEYBOARD;
    input.ki.wVk=VK_SHIFT;
    SendInput(1,&input,sizeof(INPUT));
      }

      ZeroMemory(&input,sizeof(INPUT));
      input.type=INPUT_KEYBOARD;
      input.ki.wVk=vk;
      SendInput(1,&input,sizeof(INPUT));

      ZeroMemory(&input,sizeof(INPUT));
      input.type=INPUT_KEYBOARD;
      input.ki.wVk=vk;
      input.ki.dwFlags = KEYEVENTF_KEYUP;
      SendInput(1,&input,sizeof(INPUT));

      if (s&1){
    ZeroMemory(&input,sizeof(INPUT));
    input.type=INPUT_KEYBOARD;
    input.ki.wVk=VK_SHIFT;
    input.ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(1,&input,sizeof(INPUT));
      }

    special_character:;

    }//i

#endif

  }


  void sub_files(qbs *str,int32 passed){
    if (new_error) return;
    if (cloud_app){error(262); return;}

    static int32 i,i2,i3;
    static qbs *strz=NULL; if (!strz) strz=qbs_new(0,0);

    if (passed){
      qbs_set(strz,qbs_add(str,qbs_new_txt_len("\0",1)));
    }else{
      qbs_set(strz,qbs_new_txt_len("\0",1));
    }



#ifdef QB64_WINDOWS
    static WIN32_FIND_DATA fd;
    static HANDLE hFind;
    static qbs *strpath=NULL; if (!strpath) strpath=qbs_new(0,0);
    static qbs *strz2=NULL; if (!strz2) strz2=qbs_new(0,0);

    i=0;
    if (strz->len>=2){
      if (strz->chr[strz->len-2]==92) i=1;
    }else i=1;
    if (i){//add * (and new NULL term.)
      strz->chr[strz->len-1]=42;//"*"
      qbs_set(strz,qbs_add(strz,qbs_new_txt_len("\0",1))); 
    }

    qbs_set(strpath,strz);

    for(i=strpath->len;i>0;i--){
      if ((strpath->chr[i-1]==47)||(strpath->chr[i-1]==92)){strpath->len=i; break;}
    }//i
    if (i==0) strpath->len=0;//no path specified
 
    //print the current path
    //note: for QBASIC compatibility reasons it does not print the directory name of the files being displayed
    static uint8 curdir[4096];
    static uint8 curdir2[4096];
    i2=GetCurrentDirectory(4096,(char*)curdir);
    if (i2){
      i2=GetShortPathName((char*)curdir,(char*)curdir2,4096);
      if (i2){
    qbs_set(strz2,qbs_ucase(qbs_new_txt_len((char*)curdir2,i2)));
    qbs_print(strz2,1);
      }else{
    error(5); return;
      }
    }else{
      error(5); return;
    }

    hFind = FindFirstFile(fixdir(strz), &fd);
    if(hFind==INVALID_HANDLE_VALUE){error(53); return;}//file not found
    do{

      if (!fd.cAlternateFileName[0]){//no alternate filename exists
    qbs_set(strz2,qbs_ucase(qbs_new_txt_len(fd.cFileName,strlen(fd.cFileName))));
      }else{
    qbs_set(strz2,qbs_ucase(qbs_new_txt_len(fd.cAlternateFileName,strlen(fd.cAlternateFileName))));
      }

      if (strz2->len<12){//padding required
    qbs_set(strz2,qbs_add(strz2,func_space(12-strz2->len)));
    i2=0;
    for (i=0;i<12;i++){
      if (strz2->chr[i]==46){
        memmove(&strz2->chr[8],&strz2->chr[i],4);
        memset(&strz2->chr[i],32,8-i);
        break;
      }
    }//i
      }//padding

      //add "      " or "<DIR> "
      if (fd.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY){
    qbs_set(strz2,qbs_add(strz2,qbs_new_txt_len("<DIR> ",6)));
      }else{
    qbs_set(strz2,qbs_add(strz2,func_space(6)));
      }

      makefit(strz2);
      qbs_print(strz2,0);
 
    }while(FindNextFile(hFind,&fd));
    FindClose(hFind);

    static ULARGE_INTEGER FreeBytesAvailableToCaller;
    static ULARGE_INTEGER TotalNumberOfBytes;
    static ULARGE_INTEGER TotalNumberOfFreeBytes;
    static int64 bytes;
    static char *cp;
    qbs_set(strpath,qbs_add(strpath,qbs_new_txt_len("\0",1)));
    cp=(char*)strpath->chr;
    if (strpath->len==1) cp=NULL;
    if (GetDiskFreeSpaceEx(cp,&FreeBytesAvailableToCaller,&TotalNumberOfBytes,&TotalNumberOfFreeBytes)){
      bytes=*(int64*)(void*)&FreeBytesAvailableToCaller;
    }else{
      bytes=0;
    }
    if (func_pos(NULL)>1){strz2->len=0; qbs_print(strz2,1);}//new line if necessary
    qbs_set(strz2,qbs_add(qbs_str(bytes),qbs_new_txt_len(" Bytes free",11)));
    qbs_print(strz2,1);

#endif
  }


  int32 func__keyhit(){
    /*
    //keyhit cyclic buffer
    int64 keyhit[8192];
    //    keyhit specific internal flags: (stored in high 32-bits)
    //    &4294967296->numpad was used
    int32 keyhit_nextfree=0;
    int32 keyhit_next=0;
    //note: if full, the oldest message is discarded to make way for the new message
    */
    if (keyhit_next!=keyhit_nextfree){
      static int32 x;
      x=*(int32*)&keyhit[keyhit_next];
      keyhit_next=(keyhit_next+1)&0x1FFF;
      return x;
    }
    return 0;
  }

  int32 func__keydown(int32 x){
    if (x<=0){error(5); return 0;}
    if (keyheld(x)) return -1;
    return 0;
  }







  static int32 field_failed=1;
  static int32 field_fileno;
  static int32 field_totalsize;
  static int32 field_maxsize;

  void field_new(int32 fileno){
    field_failed=1;
    if (new_error) return;
    //validate file
    static int32 i;
    static gfs_file_struct *gfs;
    i=fileno;
    if (i<0){error(54); return;}//bad file mode (TCP/IP exclusion)
    if (gfs_fileno_valid(i)!=1){error(52); return;}//Bad file name or number
    i=gfs_fileno[i];//convert fileno to gfs index
    gfs=&gfs_file[i];
    if (gfs->type!=1){error(54); return;}//Bad file mode (note: must have RANDOM access)
    //set global variables for field_add
    field_fileno=fileno;
    field_totalsize=0;
    field_maxsize=gfs->record_length;
    field_failed=0;
    return;
  }


  void field_update(int32 fileno){

    //validate file
    static int32 i;
    static gfs_file_struct *gfs;
    i=fileno;
    if (i<0){exit(7701);}//bad file mode (TCP/IP exclusion)
    if (gfs_fileno_valid(i)!=1){exit(7702);}//Bad file name or number
    i=gfs_fileno[i];//convert fileno to gfs index
    gfs=&gfs_file[i];
    if (gfs->type!=1){exit(7703);}//Bad file mode (note: must have RANDOM access)

    static qbs *str;
    for (i=0;i<gfs->field_strings_n;i++){
      str=gfs->field_strings[i];
      if (!str) exit(7704);

      //fix length if necessary
      if (str->len!=str->field->size){
    if (str->len>str->field->size) str->len=str->field->size; else qbs_set(str,qbs_new(str->field->size,1));
      }

      //copy data from field into string
      memmove(str->chr,gfs->field_buffer+str->field->offset,str->field->size);

    }//i

  }

  void lrset_field(qbs *str){
    //validate file
    static int32 i;
    static gfs_file_struct *gfs;
    i=str->field->fileno;
    if (gfs_fileno_valid(i)!=1) goto remove;
    i=gfs_fileno[i];//convert fileno to gfs index

    gfs=&gfs_file[i];
    if (gfs->type!=1) goto remove;
    //check file ID
    if (gfs->id!=str->field->fileid) goto remove;

    //store in field buffer, padding with spaces or truncating data if necessary
    if (str->field->size<=str->len){

      memmove(gfs->field_buffer+str->field->offset,str->chr,str->field->size);
    }else{
      memmove(gfs->field_buffer+str->field->offset,str->chr,str->len);
      memset(gfs->field_buffer+str->field->offset+str->len,32,str->field->size-str->len);
    }

    //update field strings for this file
    field_update(str->field->fileno);

    return;
  remove:;
    free(str->field);
    str->field=NULL;
  }

  void field_free(qbs* str){

    //validate file
    static int32 i;
    static gfs_file_struct *gfs;
    i=str->field->fileno;
    if (gfs_fileno_valid(i)!=1) goto remove;
    i=gfs_fileno[i];//convert fileno to gfs index
    gfs=&gfs_file[i];
    if (gfs->type!=1) goto remove;
    //check file ID
    if (gfs->id!=str->field->fileid) goto remove;

    //remove from string list
    static qbs *str2;
    for (i=0;i<gfs->field_strings_n;i++){
      str2=gfs->field_strings[i];
      if (str==str2){//match found
    //truncate list
    memmove(&(gfs->field_strings[i]),&(gfs->field_strings[i+1]),(gfs->field_strings_n-i-1)*ptrsz);
    goto remove;
      }
    }//i

  remove:
    free(str->field);
    str->field=NULL;
  }

  void field_add(qbs *str,int64 size){
    if (field_failed) return;
    if (new_error) goto fail;
    if (size<0){error(5); goto fail;}
    if ((field_totalsize+size)>field_maxsize){error(50); goto fail;}

    //revalidate file
    static int32 i;
    static gfs_file_struct *gfs;
    i=field_fileno;
    //TCP/IP exclusion (reason: multi-reading from same TCP/IP position would require a more complex implementation)
    if (i<0){error(54); goto fail;}//bad file mode
    if (gfs_fileno_valid(i)!=1){error(52); goto fail;}//Bad file name or number
    i=gfs_fileno[i];//convert fileno to gfs index
    gfs=&gfs_file[i];
    if (gfs->type!=1){error(54); goto fail;}//Bad file mode (note: must have RANDOM access)

    //1) Remove str from any previous FIELD allocations
    if (str->field) field_free(str);

    //2) Setup qbs field info
    str->field=(qbs_field*)malloc(sizeof(qbs_field));
    str->field->fileno=field_fileno;
    str->field->fileid=gfs->id;
    str->field->size=size;
    str->field->offset=field_totalsize;

    //3) Add str to qbs list of gfs
    if (!gfs->field_strings){
      gfs->field_strings_n=1;
      gfs->field_strings=(qbs**)malloc(ptrsz);
      gfs->field_strings[0]=str;
    }else{
      gfs->field_strings_n++;
      gfs->field_strings=(qbs**)realloc(gfs->field_strings,ptrsz*gfs->field_strings_n);
      gfs->field_strings[gfs->field_strings_n-1]=str;
    }

    //4) Update field strings for this file
    field_update(field_fileno);

    field_totalsize+=size;
    return;
  fail:
    field_failed=1;
    return;
  }

  void field_get(int32 fileno,int64 offset,int32 passed){
    if (new_error) return;

    //validate file
    static int32 i;
    static gfs_file_struct *gfs;
    i=fileno;
    if (i<0){error(54); return;}//bad file mode (TCP/IP exclusion)
    if (gfs_fileno_valid(i)!=1){error(52); return;}//Bad file name or number
    i=gfs_fileno[i];//convert fileno to gfs index
    gfs=&gfs_file[i];
    if (gfs->type!=1){error(54); return;}//Bad file mode (note: must have RANDOM access)

    if (!gfs->read){error(75); return;}//Path/file access error

    if (passed){
      offset--;
      if (offset<0){error(63); return;}//Bad record number
      offset*=gfs->record_length;
    }else{
      offset=-1;
    }

    static int32 e;
    e=gfs_read(i,offset,gfs->field_buffer,gfs->record_length);
    if (e){
      if (e!=-10){//note: on eof, unread buffer area becomes NULL
    if (e==-2){error(258); return;}//invalid handle
    if (e==-3){error(54); return;}//bad file mode
    if (e==-4){error(5); return;}//illegal function call
    if (e==-7){error(70); return;}//permission denied
    error(75); return;//assume[-9]: path/file access error
      }
    }

    field_update(fileno);

  }

  void field_put(int32 fileno,int64 offset,int32 passed){
    if (new_error) return;

    //validate file
    static int32 i;
    static gfs_file_struct *gfs;
    i=fileno;
    if (i<0){error(54); return;}//bad file mode (TCP/IP exclusion)
    if (gfs_fileno_valid(i)!=1){error(52); return;}//Bad file name or number
    i=gfs_fileno[i];//convert fileno to gfs index
    gfs=&gfs_file[i];
    if (gfs->type!=1){error(54); return;}//Bad file mode (note: must have RANDOM access)

    if (!gfs->write){error(75); return;}//Path/file access error

    if (passed){
      offset--;
      if (offset<0){error(63); return;}//Bad record number
      offset*=gfs->record_length;
    }else{
      offset=-1;
    }

    static int32 e;
    e=gfs_write(i,offset,gfs->field_buffer,gfs->record_length);
    if (e){
      if (e==-2){error(258); return;}//invalid handle
      if (e==-3){error(54); return;}//bad file mode
      if (e==-4){error(5); return;}//illegal function call
      if (e==-7){error(70); return;}//permission denied
      error(75); return;//assume[-9]: path/file access error
    }

  }


  void sub__mapunicode(int32 unicode_code, int32 ascii_code){
    if (new_error) return;
    if ((unicode_code<0)||(unicode_code>65535)){error(5); return;}
    if ((ascii_code<0)||(ascii_code>255)){error(5); return;}
    codepage437_to_unicode16[ascii_code]=unicode_code;
  }

  int32 func__mapunicode(int32 ascii_code){
    if (new_error) return NULL;
    if ((ascii_code<0)||(ascii_code>255)){error(5); return NULL;}
    return (codepage437_to_unicode16[ascii_code]);
  }


  int32 addone(int32 x){return x+1;}//for testing purposes only


  qbs *func__os(){
    qbs *tqbs;
#ifdef QB64_WINDOWS
  #ifdef QB64_32
    tqbs=qbs_new_txt("[WINDOWS][32BIT]");
  #else
    tqbs=qbs_new_txt("[WINDOWS][64BIT]");
  #endif
#elif defined(QB64_LINUX)
  #ifdef QB64_32
    tqbs=qbs_new_txt("[LINUX][32BIT]");
  #else
    tqbs=qbs_new_txt("[LINUX][64BIT]");
  #endif
#elif defined(QB64_MACOSX)
  #ifdef QB64_32
    tqbs=qbs_new_txt("[MACOSX][32BIT][LINUX]");
  #else
    tqbs=qbs_new_txt("[MACOSX][64BIT][LINUX]");
  #endif
#else
  #ifdef QB64_32
    tqbs=qbs_new_txt("[32BIT]");
  #else
    tqbs=qbs_new_txt("[64BIT]");
  #endif
#endif
    return tqbs;
  }

  int32 func__screenx(){
          #if defined(QB64_GUI) && defined(QB64_WINDOWS) && defined(QB64_GLUT)
              while (!window_exists){Sleep(100);} //Wait for window to be created before checking position
              return glutGet(GLUT_WINDOW_X) - glutGet(GLUT_WINDOW_BORDER_WIDTH);
          #endif
          return 0; //if not windows then return 0
  }

  int32 func__screeny(){
          #if defined(QB64_GUI) && defined(QB64_WINDOWS) && defined(QB64_GLUT)
                while (!window_exists){Sleep(100);} //Wait for window to be created before checking position
                return glutGet(GLUT_WINDOW_Y) - glutGet(GLUT_WINDOW_BORDER_WIDTH) - glutGet(GLUT_WINDOW_HEADER_HEIGHT);
          #endif
          return 0; //if not windows then return 0
  }

  void sub__screenmove(int32 x,int32 y,int32 passed){
    if (new_error) return;
    if (!passed) goto error;
    if (passed==3) goto error;
    if (full_screen) return;

        #if defined(QB64_GUI) && defined(QB64_GLUT)
        while (!window_exists){Sleep(100);} //wait for window to be created before moving it.
        if (passed==2){
                glutPositionWindow (x,y);}
        else{
                int32 SW, SH, WW, WH;
                        SW = glutGet(GLUT_SCREEN_WIDTH);
                        SH = glutGet(GLUT_SCREEN_HEIGHT);
                        WW = glutGet(GLUT_WINDOW_WIDTH);
                        WH = glutGet(GLUT_WINDOW_HEIGHT);
                        x = (SW - WW)/2;
                        y = (SH - WH)/2;
                        glutPositionWindow (x,y);
        }
        #endif

        return;

  error:
    error(5);
  }

  void key_update(){

    if (key_display_redraw){
      key_display_redraw=0;
      if (!key_display) return;
    }else{
      if (key_display==key_display_state) return;
      key_display_state=key_display;
    }

    //use display page 0
    static int32 olddest;
    olddest=func__dest();
    sub__dest(0);
    static img_struct *i;
    i=write_page;

    static int32 f,z,c,x2;

    //locate bottom-left
    //get current status
    static int32 cx,cy,holding,col1,col2;
    cx=i->cursor_x; cy=i->cursor_y; holding=i->holding_cursor;
    col1=i->color; col2=i->background_color;
    static int32 h,w;
    //calculate height & width in characters
    if (i->compatible_mode){
      h=i->height/fontheight[i->font];
      if (fontwidth[i->font]){
    w=i->width/fontwidth[i->font];
      }else{
    w=write_page->width;
      }
    }else{
      h=i->height;
      w=i->width;
    }
    i->cursor_x=1; i->cursor_y=h; i->holding_cursor=0;

    static qbs *str=NULL; if (!str) str=qbs_new(0,0);
    static qbs *str2=NULL; if (!str2) str2=qbs_new(1,0);

    //clear bottom row using background color
    if (i->text){
      for (x2=1;x2<=i->width;x2++){
    str2->chr[0]=32; qbs_print(str2,0);
      }
      i->cursor_x=1; i->cursor_y=h; i->holding_cursor=0;
    }else{
      fast_boxfill(0,(i->cursor_y-1)*fontheight[i->font],i->width-1,i->cursor_y*fontheight[i->font]-1,col2|0xFF000000);
    }

    if (!key_display) goto no_key;

    static int32 item_x,limit_x,row_limit,leeway_x;
    leeway_x=0;
    if (i->compatible_mode){
      if (fontwidth[i->font]){
    item_x=w/12; row_limit=item_x*12;
    if (item_x<8){//cannot fit min. width
      item_x=8; row_limit=(w/8)*8;
      if (item_x>w){item_x=w; row_limit=w;}//can't even fit 1!
    }
      }else{
    leeway_x=fontheight[i->font];
    item_x=w/12; row_limit=item_x*12-leeway_x;
    x2=((float)fontheight[i->font])*0.5;//estimate the average character width (it's OK for this to be wrong)
    if (item_x<(x2*8+leeway_x)){//cannot fit min. width
      item_x=(x2*8+leeway_x); row_limit=(w/(x2*8+leeway_x))*(x2*8+leeway_x)-leeway_x;
      if (item_x>w){item_x=w; row_limit=w-leeway_x;}//can't even fit 1!
    }  
      }
    }else{
      item_x=w/12; row_limit=item_x*12;
      if (item_x<8){//cannot fit min. width
    item_x=8; row_limit=(w/8)*8;
    if (item_x>w){item_x=w; row_limit=w;}//can't even fit 1!
      }
    }

    static int32 final_chr,row_final_chr;

    row_final_chr=0;
    for (f=1;f<=12;f++){
      final_chr=0;
      limit_x=f*item_x-leeway_x;//set new limit

      //relocate
      x2=((f-1)*item_x)+1;
      if (x2>=row_limit){row_final_chr=1; goto done_f;}
      i->cursor_x=x2;

      //number string
      if (fontwidth[i->font]){
    qbs_set(str,qbs_ltrim(qbs_str(f)));
      }else{
    qbs_set(str,qbs_add(qbs_ltrim(qbs_str(f)),qbs_new_txt(")")));
      }
      for (z=0;z<str->len;z++){
    if (i->cursor_x>=row_limit) row_final_chr=1;
    if (i->cursor_x>limit_x) goto done_f;
    if (i->cursor_x>=limit_x) final_chr=1;
    str2->chr[0]=str->chr[z]; qbs_print(str2,0);
    if (final_chr) goto done_f;
      }

      //text
      static int32 fi;
      fi=f; if (f>10) fi=f-11+30;
      if (onkey[fi].text){
    qbs_set(str,onkey[fi].text);
    if (i->text){
      if (i->background_color){
        i->color=7; i->background_color=0;
      }else{
        i->color=0; i->background_color=7;
      }
    }
      }else{
    str->len=0;
      }
      z=0;
      while(i->cursor_x<limit_x){
    static int32 c;

    if (z>=str->len){
      if (!onkey[fi].text) goto done_f;
      c=32;
    }else{
      c=str->chr[z++];
    }

    if (i->cursor_x>=row_limit) row_final_chr=1;
    if (i->cursor_x>limit_x) goto done_f;
    if (i->cursor_x>=limit_x) final_chr=1;
    /*
      7->14
      8->254
      9->26
      10->60
      11->127
      12->22
      13->27
      28->16
      29->17
      30->24
      31->25
      KEY LIST puts spaces instead of non-printables
      QBASIC's KEY LIST differs from QBX in this regard
      CHR$(13) is also turned into a space in KEY LIST, even if it is at the end
    */
    if (c==7) c=14;
    if (c==8) c=254;
    if (c==9) c=26;
    if (c==10) c=60;
    if (c==11) c=127;
    if (c==12) c=22;
    if (c==13) c=27;
    if (c==28) c=16;
    if (c==29) c=17;
    if (c==30) c=24;
    if (c==31) c=25;
    str2->chr[0]=c;
    no_control_characters=1; qbs_print(str2,0); no_control_characters=0;
    if (final_chr) goto done_f;
      }

    done_f:;
      i->color=col1; i->background_color=col2;
      if (row_final_chr) goto done_row;
    }
  done_row:;

    //revert status
  no_key:
    i->cursor_x=cx; i->cursor_y=cy; i->holding_cursor=holding; i->color=col1; i->background_color=col2;
  sub__dest(olddest);
  }

  void key_on(){
    key_display=1; key_update();
  }

  void key_off(){
    key_display=0; key_update();
  }

  void key_list(){
    static img_struct *i;
    i=write_page;
    static int32 mono;
    mono=1; if (!fontwidth[i->font]) if (func__printwidth(qbs_new_txt(" "),NULL,NULL)!=func__printwidth(qbs_new_txt(")"),NULL,NULL)) mono=0;
    static int32 f,fi;
    static qbs *str=NULL; if (!str) str=qbs_new(0,0);
    for (f=1;f<=12;f++){

      //F-number & spacer
      if (fontwidth[i->font]){
    if (f<10){
      qbs_set(str,qbs_add(qbs_ltrim(qbs_str(f)),qbs_new_txt("  ")));
    }else{
      qbs_set(str,qbs_add(qbs_ltrim(qbs_str(f)),qbs_new_txt(" ")));
    }
      }else{
    if ((f<10)&&(mono==1)){
      qbs_set(str,qbs_add(qbs_ltrim(qbs_str(f)),qbs_new_txt(")  ")));
    }else{
      qbs_set(str,qbs_add(qbs_ltrim(qbs_str(f)),qbs_new_txt(") ")));
    }
      }
      qbs_set(str,qbs_add(qbs_new_txt("F"),str));


      //text
      fi=f; if (f>10) fi=f-11+30;
      if (onkey[fi].text){
    qbs_print(str,0);
    /*
      7->14
      8->254
      9->26
      10->60
      11->127
      12->22
      13->27
      28->16
      29->17
      30->24
      31->25
      KEY LIST puts spaces instead of non-printables
      QBASIC's KEY LIST differs from QBX in this regard
      CHR$(13) is also turned into a space in KEY LIST, even if it is at the end
    */
    str->len=1;
    static int32 x,c;
    for (x=0;x<onkey[fi].text->len;x++){
      c=onkey[fi].text->chr[x];
      if ((c>=7)&&(c<=13)) c=32;
      if ((c>=28)&&(c<=31)) c=32;
      str->chr[0]=c;
      qbs_print(str,0);
    }
    str->len=0; qbs_print(str,1);
      }else{
    qbs_print(str,1);
      }

    }//f
  }

  void key_assign(int32 i,qbs *str){
    if (new_error) return;
    static int32 x,x2,i2;

    if ( ((i>=1)&&(i<=10)) || (i==30) || (i==31) ){//F1-F10,F11,F12
      if (str->len>15){error(5); return;}
      if (!onkey[i].text) onkey[i].text=qbs_new(0,0);
      qbs_set(onkey[i].text,str);
      key_display_redraw=1; key_update();
      return;
    }//F1-F10,F11,F12

    if ((i>=15)&&(i<=29)){//user defined key
      if (str->len==0){
    onkey[i].key_scancode=0;
      }else{
    x=str->chr[str->len-1];
    x2=0; for (i2=0;i2<str->len-1;i2++) x2|=str->chr[i2];
    onkey[i].key_scancode=x;
    onkey[i].key_flags=x2;
      }
      return;
    }//user defined key

    error(5);
    return;
  }

  void sub_paletteusing(void *element,int32 bits){
    //note: bits is either 16(INTEGER) or 32(LONG)
    if (new_error) return;
    static byte_element_struct *ele; ele=(byte_element_struct*)element;
    static int16 *i16; i16=(int16*)ele->offset;
    static int32 *i32; i32=(int32*)ele->offset;
    if (write_page->bits_per_pixel==32) goto error;
    static int32 last_color,i,c;
    last_color=write_page->mask;
    if (ele->length<((bits/8)*(last_color+1))) goto error;
    if ((write_page->compatible_mode==11)||(write_page->compatible_mode==12)||(write_page->compatible_mode==13)||(write_page->compatible_mode==256)){
      if (bits==16) goto error;//must be an array of type LONG in these modes
    }
    for (i=0;i<=last_color;i++){
      if (bits==16){c=*i16; i16++;}else{c=*i32; i32++;}
      if (c<-1) goto error;
      if (c!=-1){
    qbg_palette(i,c,1);
    if (new_error) return;
      }
    }
    return;
  error:

    error(5);
  }


void sub__depthbuffer(int32 options,int32 dst,int32 passed){
//                    {ON|OFF|LOCK|_CLEAR}

if (new_error) return;

if ((passed&1)==0) dst=0;//the primary hardware surface is implied
hardware_img_struct* dst_himg=NULL;
if (dst<0){
  dst_himg=(hardware_img_struct*)list_get(hardware_img_handles,dst-HARDWARE_IMG_HANDLE_OFFSET);
  if (dst_himg==NULL){error(258); return;}
  dst-=HARDWARE_IMG_HANDLE_OFFSET;
}else{
  if (dst>1) {error(5); return;}
  dst=-dst;
}

if (options==4){
    flush_old_hardware_commands();
    int32 hgch=list_add(hardware_graphics_command_handles);
    hardware_graphics_command_struct* hgc=(hardware_graphics_command_struct*)list_get(hardware_graphics_command_handles,hgch);
    hgc->remove=0;
    //set command values
    hgc->command=HARDWARE_GRAPHICS_COMMAND__CLEAR_DEPTHBUFFER;
    hgc->dst_img=dst;
    //queue the command
    hgc->next_command=0;
    hgc->order=display_frame_order_next;
    if (last_hardware_command_added){
      hardware_graphics_command_struct* hgc2=(hardware_graphics_command_struct*)list_get(hardware_graphics_command_handles,last_hardware_command_added);
      hgc2->next_command=hgch;
    }
    last_hardware_command_added=hgch;
    if (first_hardware_command==0) first_hardware_command=hgch;
    return;
}

int32 new_mode;
if (options==1){
        new_mode=DEPTHBUFFER_MODE__ON;
}
if (options==2){
        new_mode=DEPTHBUFFER_MODE__OFF;
}
if (options==3){
        new_mode=DEPTHBUFFER_MODE__LOCKED;
}

if (dst==0){
depthbuffer_mode0=new_mode;
return;
}
if (dst==-1){
depthbuffer_mode1=new_mode;
return;
}
dst_himg->depthbuffer_mode=new_mode;
}

void sub__maptriangle(int32 cull_options,float sx1,float sy1,float sx2,float sy2,float sx3,float sy3,int32 si,float fdx1,float fdy1,float fdz1,float fdx2,float fdy2,float fdz2,float fdx3,float fdy3,float fdz3,int32 di,int32 smooth_options,int32 passed){
    //[{_CLOCKWISE|_ANTICLOCKWISE}][{_SEAMLESS}](?,?)-(?,?)-(?,?)[,?]{TO}(?,?[,?])-(?,?[,?])-(?,?[,?])[,[?][,{_SMOOTH|_SMOOTHSHRUNK|_SMOOTHSTRETCHED}]]"
    //  (1)       (2)              1                             2           4         8         16    32   (1)     (2)           (3)
    
    if (new_error) return;
    
    static int32 dwidth,dheight,swidth,sheight,swidth2,sheight2;
    static int32 lhs,rhs,lhs1,lhs2,top,bottom,temp,flats,flatg,final,tile,no_edge_overlap;
    flats=0; final=0; tile=0; no_edge_overlap=0;
    static int32 v,i,x,x1,x2,y,y1,y2,z,h,ti,lhsi,rhsi,d; 
    static int32 g1x,g2x,g1tx,g2tx,g1ty,g2ty,g1xi,g2xi,g1txi,g2txi,g1tyi,g2tyi,tx,ty,txi,tyi,roff,loff;
    static int64 i64;
    static img_struct *src,*dst;
    static uint8* pixel_offset;
    static uint32* pixel_offset32;
    static uint8* dst_offset;
    static uint32* dst_offset32;
    static uint8* src_offset;
    static uint32* src_offset32;
    static uint32 col,destcol,transparent_color;
    static uint8* cp;

 //hardware support
  //is source a hardware handle?
  if (si){

    static int32 src,dst;//scope is a wonderful thing
    src=si;
    dst=di;
    hardware_img_struct* src_himg=(hardware_img_struct*)list_get(hardware_img_handles,src-HARDWARE_IMG_HANDLE_OFFSET);
    if (src_himg!=NULL){//source is hardware image
      src-=HARDWARE_IMG_HANDLE_OFFSET;

      flush_old_hardware_commands();

      //check dst
      hardware_img_struct* dst_himg=NULL;
      if (dst<0){
        dst_himg=(hardware_img_struct*)list_get(hardware_img_handles,dst-HARDWARE_IMG_HANDLE_OFFSET);
        if (dst_himg==NULL){error(258); return;}
        dst-=HARDWARE_IMG_HANDLE_OFFSET;
      }else{
        if (dst>1) {error(5); return;}
        dst=-dst;
      }

    static int32 use3d;
    use3d=0;
    if (passed&(4+8+16)) use3d=1;

    if ((passed&1)==1&&use3d==0){error(5);return;}//seamless not supported for 2D hardware version yet

    //create new command handle & structure
    int32 hgch=list_add(hardware_graphics_command_handles);
    hardware_graphics_command_struct* hgc=(hardware_graphics_command_struct*)list_get(hardware_graphics_command_handles,hgch);

    hgc->remove=0;
 
    //set command values
    if (use3d){
        hgc->command=HARDWARE_GRAPHICS_COMMAND__MAPTRIANGLE3D;
        hgc->cull_mode=CULL_MODE__NONE;
        if (cull_options==1) hgc->cull_mode=CULL_MODE__CLOCKWISE_ONLY;
        if (cull_options==2) hgc->cull_mode=CULL_MODE__ANTICLOCKWISE_ONLY;
    }else{
        hgc->command=HARDWARE_GRAPHICS_COMMAND__MAPTRIANGLE;
    }

    hgc->src_img=src;
    hgc->src_x1=sx1;
    hgc->src_y1=sy1;
    hgc->src_x2=sx2;
    hgc->src_y2=sy2;
    hgc->src_x3=sx3;
    hgc->src_y3=sy3;

    hgc->dst_img=dst;
    hgc->dst_x1=fdx1;
    hgc->dst_y1=fdy1;
    hgc->dst_x2=fdx2;
    hgc->dst_y2=fdy2;
    hgc->dst_x3=fdx3;
    hgc->dst_y3=fdy3;
    if (use3d){
        hgc->dst_z1=fdz1;
        hgc->dst_z2=fdz2;
        hgc->dst_z3=fdz3;
            if (dst==0) hgc->depthbuffer_mode=depthbuffer_mode0;
        if (dst==-1) hgc->depthbuffer_mode=depthbuffer_mode1;
        if (dst_himg!=NULL){
             hgc->depthbuffer_mode=dst_himg->depthbuffer_mode;
        }        
    }

    hgc->smooth=smooth_options;

    hgc->use_alpha=1;
    if (src_himg->alpha_disabled) hgc->use_alpha=0;
    //only consider dest alpha setting if it is a hardware image
    if (dst_himg!=NULL){
     if (dst_himg->alpha_disabled) hgc->use_alpha=0;
    }

    //queue the command
    hgc->next_command=0;
    hgc->order=display_frame_order_next;

    if (last_hardware_command_added){
      hardware_graphics_command_struct* hgc2=(hardware_graphics_command_struct*)list_get(hardware_graphics_command_handles,last_hardware_command_added);
      hgc2->next_command=hgch;
    }
    last_hardware_command_added=hgch;
    if (first_hardware_command==0) first_hardware_command=hgch;

    return;

    }
  }

    if (passed&(4+8+16)){error(5);return;}//3D not supported using software surfaces

    //recreate old calling convention
    static int32 passed_original;
    passed_original=passed;
    passed=0;
    if (passed_original&1) passed+=1;
    if (passed_original&2) passed+=2;
    if (passed_original&32) passed+=4;
    if (passed_original&64) passed+=8;

    static int32 dx1,dy1,dx2,dy2,dx3,dy3;
    dx1=qbr_float_to_long(fdx1);
    dy1=qbr_float_to_long(fdy1);
    dx2=qbr_float_to_long(fdx2);
    dy2=qbr_float_to_long(fdy2);
    dx3=qbr_float_to_long(fdx3);
    dy3=qbr_float_to_long(fdy3);

    //get/validate src/dst images
    if (passed&2){
      if (si>=0){//validate si
    validatepage(si); si=page[si];
      }else{
    si=-si; if (si>=nextimg){error(258); return;} if (!img[si].valid){error(258); return;}
      }
      src=&img[si];
    }else{
      src=read_page;
    }
    if (passed&4){
      if (di>=0){//validate di
    validatepage(di); di=page[di];
      }else{
    di=-di; if (di>=nextimg){error(258); return;} if (!img[di].valid){error(258); return;}
      }
      dst=&img[di];
    }else{
      dst=write_page;
    }
    if (src->text||dst->text){error(5);return;}
    if (src->bytes_per_pixel!=dst->bytes_per_pixel){error(5);return;}

    if (passed&1) no_edge_overlap=1;

    dwidth=dst->width; dheight=dst->height;
    swidth=src->width; sheight=src->height;
    swidth2 = swidth<<16; sheight2 = sheight<<16;

    struct PointType{
      int32 x;
      int32 y;
      int32 tx;
      int32 ty;
    };
    static PointType p[4],*p1,*p2,*tp,*tempp;
    struct GradientType{
      int32 x;
      int32 xi;
      int32 tx;
      int32 ty;
      int32 txi;
      int32 tyi;
      int32 y1;
      int32 y2;
      //----
      PointType *p1;
      PointType *p2; //needed for clipping above screen
    };
    static GradientType g[4],*tg,*g1,*g2,*g3,*tempg;
    memset(&g,0,sizeof(GradientType)*4);

    /*
      'Reference:
      'Fixed point division: a/b -> a*65536/b (using intermediate _INTEGER64)
    */

    /* debugging method
       ofstream f;
       char fn[] = "c:\\qb64\\20c.txt";
       f.open(fn, ios::app);
       f<<"\n";
       f<<variablename;
       f<<"\n";
       f.close();
    */

    static int32 limit,limit2,nlimit,nlimit2;

    //----------------------------------------------------------------------------------------------------------------------------------------------------


    limit = 16383;
    limit2 = (limit << 16) + 32678;
    nlimit = -limit;
    nlimit2 = -limit2;

    //convert texture coords to fixed-point & adjust so 0,0 effectively becomes 0.5,0.5 (ie. 32768,32768)
    v = ((int32)(sx1 * 65536.0)) + 32768;
    if(v < 16 | v >= swidth2 - 16) tile = 1;
    if(v < nlimit2 | v > limit2) {error(5); return;}
    p[1].tx = v;
    v = ((int32)(sx2 * 65536.0)) + 32768;
    if(v < 16 | v >= swidth2 - 16) tile = 1;
    if(v < nlimit2 | v > limit2) {error(5); return;}
    p[2].tx = v;
    v = ((int32)(sx3 * 65536.0)) + 32768;
    if(v < 16 | v >= swidth2 - 16) tile = 1;
    if(v < nlimit2 | v > limit2) {error(5); return;}
    p[3].tx = v;
    v = ((int32)(sy1 * 65536.0)) + 32768;
    if(v < 16 | v >= sheight2 - 16) tile = 1;
    if(v < nlimit2 | v > limit2) {error(5); return;}
    p[1].ty = v;
    v = ((int32)(sy2 * 65536.0)) + 32768;
    if(v < 16 | v >= sheight2 - 16) tile = 1;
    if(v < nlimit2 | v > limit2) {error(5); return;}
    p[2].ty = v;
    v = ((int32)(sy3 * 65536.0)) + 32768;
    if(v < 0 | v >= sheight2 - 16) tile = 1;
    if(v < nlimit2 | v > limit2) {error(5); return;}
    p[3].ty = v;

    if(tile){
      //shifting to positive range is required for tiling | mod on negative coords will fail
      //shifting may also alleviate the need for tiling if(shifted coords fall within textures normal range
      //does texture extend beyond surface dimensions?
      lhs = 2147483647;
      rhs = -2147483648;
      top = 2147483647;
      bottom = -2147483648;
      for(i=1;i<=3;i++){
        tp=&p[i];
        y = tp->ty;
        if(y > bottom) bottom = y;
        if(y < top) top = y;
        x = tp->tx;
        if(x > rhs) rhs = x;
        if(x < lhs) lhs = x;
      }
      z = 0;
      if(lhs < 0){
        //shift texture coords right
        v = ((lhs + 1) / -swidth2 + 1) * swidth2; //offset to move by
        for(i=1;i<=3;i++){
      tp=&p[i];
      tp->tx = tp->tx + v;
      z = 1;
        }
      }else{
        if(lhs >= swidth2){
      //shift texture coords left
      z = 1;
      v = (lhs / swidth2) * swidth2; //offset to move by
      for(i=1;i<=3;i++){
        tp=&p[i];
        tp->tx = tp->tx - v;
        z = 1;
      }
        }
      }
      if(top < 0){
        //shift texture coords down
        v = ((top + 1) / -sheight2 + 1) * sheight2; //offset to move by
        for(i=1;i<=3;i++){
      tp=&p[i];
      tp->ty = tp->ty + v;
      z = 1;
        }
      }else{
        if(top >= swidth2){
      //shift texture coords up
      v = (top / sheight2) * sheight2; //offset to move by
      for(i=1;i<=3;i++){
        tp=&p[i];
        tp->ty = tp->ty - v;
        z = 1;

      }
      z = 1;
        }
      }
      if(z){
        //reassess need for tiling
        z = 0;
        for(i=1;i<=3;i++){
      tp=&p[i];
      v = tp->tx; if(v < 16 | v >= swidth2 - 16){
        z = 1; break;
      }
      v = tp->ty; if(v < 16 | v >= sheight2 - 16){
        z = 1; break;
      }
        }
        if(z == 0) tile = 0; //remove tiling flag, this will greatly improve blit speed
      }
    }

    //validate dest points
    if(dx1 < nlimit | dx1 > limit) {error(5); return;}
    if(dx2 < nlimit | dx2 > limit) {error(5); return;}
    if(dx3 < nlimit | dx3 > limit) {error(5); return;}
    if(dy1 < nlimit | dy1 > limit) {error(5); return;}
    if(dy2 < nlimit | dy2 > limit) {error(5); return;}
    if(dy3 < nlimit | dy3 > limit) {error(5); return;}

    //setup dest points
    p[1].x = (dx1 << 16) + 32768;
    p[2].x = (dx2 << 16) + 32768;
    p[3].x = (dx3 << 16) + 32768;
    p[1].y = dy1;
    p[2].y = dy2;
    p[3].y = dy3;

    //get dest extents
    lhs = 2147483647;
    rhs = -2147483648;
    top = 2147483647;
    bottom = -2147483648;
    for(i=1;i<=3;i++){
      tp=&p[i];
      y = tp->y;
      if(y > bottom) bottom = y;
      if(y < top) top = y;
      if(tp->x < 0) x = (tp->x - 65535) / 65536;else x = tp->x / 65536;
      if(x > rhs) rhs = x;
      if(x < lhs) lhs = x;
    }

    if(bottom < 0 | top >= dheight | rhs < 0 | lhs >= dwidth) return; //clip entire triangle

    for(i=1;i<=3;i++){
      tg=&g[i]; p1=&p[i]; if (i==3) p2=&p[1]; else p2=&p[i+1];

      if(p1->y > p2->y){
        tempp = p1; p1 = p2; p2 = tempp;
      }
      tg->tx = p1->tx; tg->ty = p1->ty; //starting co-ordinates
      tg->x = p1->x;
      tg->y1 = p1->y; tg->y2 = p2->y; //top & bottom
      h = tg->y2 - tg->y1;
      if(h == 0){
        flats = flats + 1; //number of flat(horizontal) gradients
        flatg = i; //last detected flat gradient
      }else{
        tg->xi = (p2->x - p1->x) / h;
        tg->txi = (p2->tx - p1->tx) / h;
        tg->tyi = (p2->ty - p1->ty) / h;
      }
      tg->p2 = p2;
      tg->p1 = p1;
    }

    g1=&g[1]; g2=&g[2]; g3=&g[3];
    if(flats == 0){
      //select 2 top-most gradients
      if(g3->y1 < g1->y1){
        tempg = g1; g1 = g3; g3 = tempg;
      }
      if(g3->y1 < g2->y1){
        tempg = g2; g2 = g3; g3 = tempg;
      }
    }else{
      if(flats == 1){
        //select the only 2 non-flat gradients available
        if(flatg == 1){
      tempg = g1; g1 = g3; g3 = tempg;
        }
        if(flatg == 2){
      tempg = g2; g2 = g3; g3 = tempg;
        }
        final = 1; //don't check for continuation
      }else{
        //3 flats
        //create a row from the leftmost to rightmost point
        //find leftmost-rightmost points
        lhs = 2147483647;
        rhs = -2147483648;
        for(ti=1;ti<=3;ti++){
      x = p[ti].x;
      if(x <= lhs){
        lhs = x; lhsi = ti;
      }
      if(x >= rhs){
        rhs = x; rhsi = ti;
      }
        }
        //build (dummy) gradients
        g[1].x = lhs;
        g[2].x = rhs;
        g[1].tx = p[lhsi].tx; g[1].ty = p[lhsi].ty;
        g[2].tx = p[rhsi].tx; g[2].ty = p[rhsi].ty;
        final = 1; //don't check for continuation
      }
    }

    y1 = g1->y1; if(g1->y2 > g2->y2) y2 = g2->y2;else y2 = g1->y2;

    //compare the mid-point x-coords of both runs to determine which is on the left & right
    y = y2 - y1;
    lhs1 = g1->x + (g1->xi * y) / 2;
    lhs2 = g2->x + (g2->xi * y) / 2;
    if(lhs1 > lhs2){
      tempg = g1; g1 = g2; g2 = tempg;
    }

    //----------------------------------------------------------------------------------------------------------------------------------------------------

    if (src->bytes_per_pixel==4){
      dst_offset32=dst->offset32;
      src_offset32=src->offset32;
      if (src->alpha_disabled||dst->alpha_disabled){
    if (tile){
#include "mtri1t.cpp"
    }
#include "mtri1.cpp"
      }else{
    if (tile){
#include "mtri2t.cpp"
    }
#include "mtri2.cpp"
      }
    }//4

    //assume 1 byte per pixel
    dst_offset=dst->offset;
    src_offset=src->offset;
    transparent_color=src->transparent_color;
    if (transparent_color==-1){
      if (tile){
#include "mtri3t.cpp"
      }
#include "mtri3.cpp"
    }else{
      if (tile){
#include "mtri4t.cpp"
      }
#include "mtri4.cpp"
    }//1

    error(5); return;
  }//sub__maptriangle

  extern int32 func__devices();

  int32 func_stick(int32 i,int32 axis_group,int32 passed){
    //note: range: 1-254 (127=neutral), top-left to bottom-right positive
    //             128 returned for unattached devices
    //QB64 extension: 'i' allows for joystick selection 0,1->JoyA, 2,3->JoyB, 4,5->JoyC, etc
    //                'axis_group' selects the pair of axes to read from, 1 is the default
    if (device_last==0) func__devices();//init device interface (if not already setup)
    if (passed){
      if (axis_group<1||axis_group>65535){error(5); return 0;}
    }else{
      axis_group=1;
    }
    if (i<0||i>65535){error(5); return 0;}
    static int32 di,axis,i2,v;
    static device_struct *d;
    static float f;
    axis=(i&1)+(axis_group-1)*2;
    i=i>>1;
    i2=0;
    for(di=1;di<=device_last;di++){
      d=&devices[di];
      if (d->type==1){
    if (i==i2){
      if (axis<d->lastaxis){
	f=getDeviceEventAxisValue(d,d->queued_events-1,axis);
        if (f>-0.01&&f<=0.01) f=0;
        v=qbr_float_to_long(f*127.0)+127;
        if (v>254) v=254;
        if (v<1) v=1;
        return v;
      }//axis valid
    }
    i2++;
      }//type==1
    }//di
    return 128;
  }

  int32 func_strig(int32 i,int32 controller,int32 passed){
    //note: returns 0 or -1(true)
    //QB64 extension: 'i' refers to a button (b1,b1,b1,b1,b2,b2,b2,b2,b3,b3,b3,b3,b4,...)
    //                'controller' overrides the controller implied by 'i', 1 is the default
    if (device_last==0) func__devices();//init device interface (if not already setup)
    if (i<0||i>65535){error(5); return 0;}
    if (passed){
      if (controller<1||controller>65535){error(5); return 0;}
    }else{
      controller=1; if (i&2){controller=2; i-=2;}
    }
    static int32 di,button,method,c,v;
    static device_struct *d;
    button=(i>>2)+1;
    method=(i&1)+1;//1=if pressed since last call, 2=currently down
    c=1;
    for(di=1;di<=device_last;di++){
      d=&devices[di];
      if (d->type==1){
    if (c==controller){
      if (button<=d->lastbutton){//button exists
        if (method==1){
          //method 1: pressed since last call
          if (button>0&&button<=256){
        if (d->STRIG_button_pressed[button-1]){
          d->STRIG_button_pressed[button-1]=0;
          return -1;
        }
          }
          return 0;
        }else{
          //method 2: currently down
	  v=getDeviceEventButtonValue(d,d->queued_events-1,button-1);
          if (v) return -1; else return 0;
        }
      }//button exists
    }//c==controller
    c++;
      }//type==1
    }//di
    return 0;
  }

  int32 func__fileexists(qbs* file){
    if (new_error) return 0;
    static qbs *strz=NULL;
    if (!strz) strz=qbs_new(0,0);
    qbs_set(strz,qbs_add(file,qbs_new_txt_len("\0",1)));
#ifdef QB64_WINDOWS
    static int32 x;
    x=GetFileAttributes(fixdir(strz));
    if (x==INVALID_FILE_ATTRIBUTES) return 0;
    if (x&FILE_ATTRIBUTE_DIRECTORY) return 0;
    return -1;
#elif defined(QB64_UNIX)
    struct stat sb;
    if (stat(fixdir(strz),&sb) == 0 && S_ISREG(sb.st_mode)) return -1;
    return 0;
#else
    //generic method (not currently used)
    static ifstream fh;
    fh.open(fixdir(strz),ios::binary|ios::in);
    if (fh.is_open()==NULL){fh.clear(ios::goodbit); return 0;}
    fh.clear(ios::goodbit);
    fh.close();
    return -1;
#endif
  }

  int32 func__direxists(qbs* file){
    if (new_error) return 0;
    static qbs *strz=NULL;
    if (!strz) strz=qbs_new(0,0);
    qbs_set(strz,qbs_add(file,qbs_new_txt_len("\0",1)));
#ifdef QB64_WINDOWS
    static int32 x;
    x=GetFileAttributes(fixdir(strz));
    if (x==INVALID_FILE_ATTRIBUTES) return 0;
    if (x&FILE_ATTRIBUTE_DIRECTORY) return -1;
    return 0;
#elif defined(QB64_UNIX)
    struct stat sb;
    if (stat(fixdir(strz),&sb) == 0 && S_ISDIR(sb.st_mode)) return -1;
    return 0;
#else
    return 0;//default response
#endif
  }

  int32 func__console(){
    if (new_error) return -1;
    return console_image;
  }

  void sub__console(int32 onoff){//on=1 off=2
    if (!console) return;//command does nothing if console unavailable
    if (onoff==1){
      //turn on
      if (!console_active){
#ifdef QB64_WINDOWS
    if (console_child){
      ShowWindow( GetConsoleWindow(), SW_SHOWNOACTIVATE );
    }
#endif
    console_active=1;
      }
    }else{
      //turn off
      if (console_active){
#ifdef QB64_WINDOWS
    if (console_child){
      ShowWindow( GetConsoleWindow(), SW_HIDE );
    }
#endif
    console_active=0;
      }
    }

  }


  void sub__screenshow(){
    if (!window_exists){
      create_window=1;
    }else{
#ifdef QB64_GLUT
      glutShowWindow();
#endif
    }
    screen_hide=0;
  }

  void sub__screenhide(){
    if (window_exists){
#ifdef QB64_GLUT
      glutHideWindow();
#endif
    }
    screen_hide=1;
  }

  int32 func__screenhide(){return -screen_hide;}

  void sub__consoletitle(qbs* s){
    if (new_error) return;
    static qbs *sz=NULL; if (!sz) sz=qbs_new(0,0);
    static qbs *cz=NULL; if (!cz){cz=qbs_new(1,0); cz->chr[0]=0;}
    qbs_set(sz,qbs_add(s,cz));
    if (console){ if (console_active){
#ifdef QB64_WINDOWS
    SetConsoleTitle((char*)sz->chr);
#endif
      }}
  }



  void sub__memfree(void *mem){
    //1:malloc: memory will be freed if it still exists
    //2:images: will not be freed, no action will be taken
    //exists?
    if (((mem_block*)(mem))->lock_offset==NULL){error(309); return;}
    if (((mem_lock*)(((mem_block*)(mem))->lock_offset))->id!=((mem_block*)(mem))->lock_id){error(307); return;}//memory has been freed
    if ( ((mem_lock*)(((mem_block*)(mem))->lock_offset))->type ==0){//no security
      free_mem_lock( (mem_lock*)((mem_block*)(mem))->lock_offset );
    }
    if ( ((mem_lock*)(((mem_block*)(mem))->lock_offset))->type ==1){//malloc
      free_mem_lock( (mem_lock*)((mem_block*)(mem))->lock_offset );
    }
    //note: type 2(image) is freed when the image is freed
    //invalidate caller's mem structure (avoids misconception that _MEMFREE failed)
    ((mem_block*)(mem))->lock_id=1073741821;
  }


  extern mem_block func__mem_at_offset(ptrszint offset,ptrszint size){
    static mem_block b;
    new_mem_lock();
    mem_lock_tmp->type=0;//unsecured
    b.lock_offset=(ptrszint)mem_lock_tmp; b.lock_id=mem_lock_id;
    b.offset=offset;
    b.size=size;
    b.type=16384;//_MEMNEW type
    b.elementsize=1;
    b.image=-1;
    if ((size<0)||new_error){
      b.type=0;
      b.size=0;
      b.offset=0;
      if (size<0) error(301);
    }
    return b;
  }

  mem_block func__memnew(ptrszint bytes){
    static mem_block b;
    new_mem_lock();
    b.lock_offset=(ptrszint)mem_lock_tmp;
    b.lock_id=mem_lock_id;
    b.type=16384;//_MEMNEW type
    b.elementsize=1;
    b.image=-1;
    if (new_error){
      b.type=0;
      b.offset=0;
      b.size=0;
      mem_lock_tmp->type=0;
      return b;
    }

    if (bytes<0){
      //still create a block, but an invalid one and generate an error
      error(5);
      b.offset=0;
      b.size=0;
      mem_lock_tmp->type=0;
    }else{
      if (!bytes){
    b.offset=1;//non-zero=success
    b.size=0;
      }else{
    b.offset=(ptrszint)malloc(bytes);
    if (!b.offset){b.size=0; mem_lock_tmp->type=0;} else {b.size=bytes; mem_lock_tmp->type=1; mem_lock_tmp->offset=(void*)b.offset;}
      }
    }
    return b;
  }


  mem_block func__memimage(int32 i,int32 passed){

    static mem_block b;

    if (new_error) goto error;

    static int image_handle;

    static img_struct *im;
    if (passed){
      if (i>=0){
    validatepage(i); im=&img[image_handle=page[i]]; 
    image_handle=-image_handle;
      }else{
    image_handle=i;
    i=-i;
    if (i>=nextimg){error(258); goto error;}
    im=&img[i];
    if (!im->valid){error(258); goto error;}
      }
    }else{
      im=write_page;
    }

    if (im->lock_id){
      b.lock_offset=(ptrszint)im->lock_offset; b.lock_id=im->lock_id;//get existing tag
    }else{
      new_mem_lock();
      mem_lock_tmp->type=2;//image
      b.lock_offset=(ptrszint)mem_lock_tmp; b.lock_id=mem_lock_id;
      im->lock_offset=(void*)mem_lock_tmp; im->lock_id=mem_lock_id;//create tag
    }

    b.offset=(ptrszint)im->offset;
    b.size=im->bytes_per_pixel*im->width*im->height;
    b.type=im->bytes_per_pixel+128+1024+2048;//integer+unsigned+pixeltype
    b.elementsize=im->bytes_per_pixel;
    b.image=image_handle;

    return b;
  error:
    b.offset=0;
    b.size=0;
    b.lock_offset=(ptrszint)mem_lock_base; b.lock_id=1073741821;//set invalid lock
    b.type=0;
    b.elementsize=0;
    b.image=-1;
    return b;
  }

  int32 func__memexists(void* void_blk){
    static mem_block *blk;
    blk=(mem_block*)void_blk;
    if ( ((mem_block*)(blk))->lock_offset==NULL ) return 0;
    if ( ((mem_lock*)(((mem_block*)(blk))->lock_offset))->id == ((mem_block*)(blk))->lock_id ) return -1;
    return 0;
  }

  void *func__memget(mem_block* blk,ptrszint off,ptrszint bytes){
    //checking A
    if ( ((mem_block*)(blk))->lock_offset==NULL ){error(309); goto fail;}
    //checking B
    if ( 
    off< ((mem_block*)(blk))->offset || (off+bytes)> (((mem_block*)(blk))->offset+((mem_block*)(blk))->size) ||
    ((mem_lock*)(((mem_block*)(blk))->lock_offset))->id!=((mem_block*)(blk))->lock_id
     ){
      //error reporting
      if ( ((mem_lock*)(((mem_block*)(blk))->lock_offset))->id!=((mem_block*)(blk))->lock_id ){error(308); goto fail;}
      error(300); goto fail;
    }
    return (void*)off;
    //------------------------------------------------------------
  fail:
    static void *fail_buffer;
    fail_buffer=calloc(bytes,1);
    if (!fail_buffer) error(518);//critical error: out of memory
    return fail_buffer;
  }


  void sub__memfill_nochecks(ptrszint doff,ptrszint dbytes,ptrszint soff,ptrszint sbytes){
    if (sbytes==1){
      memset((void*)doff,*(uint8*)soff,dbytes);
      return;
    }
    static ptrszint si;
    si=0;
    while(dbytes--){
      *(int8*)(doff++)=*(int8*)(soff+si++);
      if (si>=sbytes) si=0;
    }
  }

  void sub__memfill(mem_block* dblk,ptrszint doff,ptrszint dbytes,ptrszint soff,ptrszint sbytes){
    if ( ((mem_block*)(dblk))->lock_offset==NULL ){error(309); return;}
    if ( ((mem_lock*)(((mem_block*)(dblk))->lock_offset))->id!=((mem_block*)(dblk))->lock_id ){error(308); return;}
    if ( (dbytes<0) || (sbytes==0) ){error(301); return;}
    if ( doff< ((mem_block*)(dblk))->offset || (doff+dbytes)> (((mem_block*)(dblk))->offset+((mem_block*)(dblk))->size) ){error(300); return;}
    sub__memfill_nochecks(doff,dbytes,soff,sbytes);
  }

  void sub__memfill_1(mem_block* dblk,ptrszint doff,ptrszint dbytes,int8 val){
    sub__memfill(dblk,doff,dbytes,(ptrszint)&val,1);
  }
  void sub__memfill_nochecks_1(ptrszint doff,ptrszint dbytes,int8 val){
    sub__memfill_nochecks(doff,dbytes,(ptrszint)&val,1);
  }
  void sub__memfill_2(mem_block* dblk,ptrszint doff,ptrszint dbytes,int16 val){
    sub__memfill(dblk,doff,dbytes,(ptrszint)&val,2);
  }
  void sub__memfill_nochecks_2(ptrszint doff,ptrszint dbytes,int16 val){
    sub__memfill_nochecks(doff,dbytes,(ptrszint)&val,2);
  }
  void sub__memfill_4(mem_block* dblk,ptrszint doff,ptrszint dbytes,int32 val){
    sub__memfill(dblk,doff,dbytes,(ptrszint)&val,4);
  }
  void sub__memfill_nochecks_4(ptrszint doff,ptrszint dbytes,int32 val){
    sub__memfill_nochecks(doff,dbytes,(ptrszint)&val,4);
  }
  void sub__memfill_8(mem_block* dblk,ptrszint doff,ptrszint dbytes,int64 val){
    sub__memfill(dblk,doff,dbytes,(ptrszint)&val,8);
  }
  void sub__memfill_nochecks_8(ptrszint doff,ptrszint dbytes,int64 val){
    sub__memfill_nochecks(doff,dbytes,(ptrszint)&val,8);
  }
  void sub__memfill_SINGLE(mem_block* dblk,ptrszint doff,ptrszint dbytes,float val){
    sub__memfill(dblk,doff,dbytes,(ptrszint)&val,4);
  }
  void sub__memfill_nochecks_SINGLE(ptrszint doff,ptrszint dbytes,float val){
    sub__memfill_nochecks(doff,dbytes,(ptrszint)&val,4);
  }
  void sub__memfill_DOUBLE(mem_block* dblk,ptrszint doff,ptrszint dbytes,double val){
    sub__memfill(dblk,doff,dbytes,(ptrszint)&val,8);
  }
  void sub__memfill_nochecks_DOUBLE(ptrszint doff,ptrszint dbytes,double val){
    sub__memfill_nochecks(doff,dbytes,(ptrszint)&val,8);
  }

  static uint8 memfill_FLOAT_padding[]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};//32 null bytes
  void sub__memfill_FLOAT(mem_block* dblk,ptrszint doff,ptrszint dbytes,long double val){
    *(long double*)memfill_FLOAT_padding=val;
    sub__memfill(dblk,doff,dbytes,(ptrszint)memfill_FLOAT_padding,32);
  }
  void sub__memfill_nochecks_FLOAT(ptrszint doff,ptrszint dbytes,long double val){
    *(long double*)memfill_FLOAT_padding=val;
    sub__memfill_nochecks(doff,dbytes,(ptrszint)memfill_FLOAT_padding,32);
  }

  void sub__memfill_OFFSET(mem_block* dblk,ptrszint doff,ptrszint dbytes,ptrszint val){
    sub__memfill(dblk,doff,dbytes,(ptrszint)&val,sizeof(ptrszint));
  }
  void sub__memfill_nochecks_OFFSET(ptrszint doff,ptrszint dbytes,ptrszint val){
    sub__memfill_nochecks(doff,dbytes,(ptrszint)&val,sizeof(ptrszint));
  }





























  void sub__memcopy(void *sblk,ptrszint soff,ptrszint bytes,void *dblk,ptrszint doff){
    //checking A
    if ( ((mem_block*)(sblk))->lock_offset==NULL || ((mem_block*)(dblk))->lock_offset==NULL ){
      //error reporting
      if ( ((mem_block*)(sblk))->lock_offset==NULL && ((mem_block*)(dblk))->lock_offset==NULL ){error(312); return;}
      if ( ((mem_block*)(sblk))->lock_offset==NULL ){error(310); return;}
      error(311); return;
    }
    //checking B
    if ( bytes<0 ||
     soff< ((mem_block*)(sblk))->offset || (soff+bytes)> (((mem_block*)(sblk))->offset+((mem_block*)(sblk))->size) ||
     doff< ((mem_block*)(dblk))->offset || (doff+bytes)> (((mem_block*)(dblk))->offset+((mem_block*)(dblk))->size) ||
     ((mem_lock*)(((mem_block*)(sblk))->lock_offset))->id!=((mem_block*)(sblk))->lock_id ||
     ((mem_lock*)(((mem_block*)(dblk))->lock_offset))->id!=((mem_block*)(dblk))->lock_id
     ){
      //error reporting
      if (((mem_lock*)(((mem_block*)(sblk))->lock_offset))->id!=((mem_block*)(sblk))->lock_id && ((mem_lock*)(((mem_block*)(dblk))->lock_offset))->id!=((mem_block*)(dblk))->lock_id){error(313); return;}
      if (((mem_lock*)(((mem_block*)(sblk))->lock_offset))->id!=((mem_block*)(sblk))->lock_id){error(305); return;}
      if (((mem_lock*)(((mem_block*)(dblk))->lock_offset))->id!=((mem_block*)(dblk))->lock_id){error(306); return;}
      if (bytes<0){error(301); return;}
      if ( soff< ((mem_block*)(sblk))->offset || (soff+bytes)> (((mem_block*)(sblk))->offset+((mem_block*)(sblk))->size) ){
    if ( doff< ((mem_block*)(dblk))->offset || (doff+bytes)> (((mem_block*)(dblk))->offset+((mem_block*)(dblk))->size) ){error(304); return;}
    error(302);
    return;
      }
      error(303); return;
    }
    memmove((char*)doff,(char*)soff,bytes);
  }


  mem_block func__mem(ptrszint offset,ptrszint size,int32 type,ptrszint elementsize,mem_lock *lock){
    static mem_block b;
    b.lock_offset=(ptrszint)lock; b.lock_id=lock->id;
    b.offset=offset;
    b.size=size;
    b.type=type;
    b.elementsize=elementsize;
    b.image=-1;
    return b;
  }

  /* Extra maths functions - we do what we must because we can */
  double func_deg2rad (double value) {
    return (value * 0.01745329251994329576923690768489);
  }

  double func_rad2deg (double value) {
    return (value * 57.29577951308232);
  } 

  double func_deg2grad (double value) {
    return (value * 1.111111111111111);
  }

  double func_grad2deg (double value) {
    return (value * 0.9);
  }

  double func_rad2grad (double value) {
    return (value * 63.66197723675816);
  } 

  double func_grad2rad (double value) {
    return (value * .01570796326794896);
  } 

  double func_pi (double multiplier,int32 passed) {
    if (passed) {return 3.14159265358979323846264338327950288419716939937510582 * multiplier;}
    return (3.14159265358979323846264338327950288419716939937510582);
  }

  double func_arcsec (double num) {
    int sign = (num > 0) - (num < 0);
    if (num<-1||num>1) {error(5);return 0;}
    return atan(num / sqrt(1 - num * num)) + (sign - 1) * (2 * atan(1));
  }

  double func_arccsc (double num) {
    int sign = (num > 0) - (num < 0);
    if (num<-1||num>1) {error(5);return 0;}
    return atan(num / sqrt(1 - num * num)) + (sign - 1) * (2 * atan(1));
  }

  double func_arccot (double num) {return 2 * atan(1) - atan(num);}

  double func_sech (double num) {
    if (num>88.02969) {error(5);return 0;}
    if (exp(num) + exp(-num)==0) {error(5);return 0;}
    return 2/ (exp(num) + exp(-num));
  }

  double func_csch (double num) {
    if (num>88.02969) {error(5);return 0;}
    if (exp(num) - exp(-num)==0) {error(5);return 0;}
    return 2/ (exp(num) - exp(-num));
  }

  double func_coth (double num) {
    if (num>44.014845) {error(5);return 0;}
    if (2 * exp(num) - 1==0) {error(5);return 0;}
    return 2 * exp(num) - 1;
  }

  double func_sec (double num) {
    if (cos(num)==0) {error(5);return 0;}
    return 1/cos(num);
  }

  double func_csc (double num) {
    if (sin(num)==0) {error(5);return 0;}
    return 1/sin(num);
  }

  double func_cot (double num) {
    if (tan(num)==0) {error(5);return 0;}
    return 1/tan(num);
  }


  void GLUT_key_ascii(int32 key,int32 down){
#ifdef QB64_GLUT
    static int32 v;

    static int32 mod;
    mod=glutGetModifiers();//shift=1, control=2, alt=4

#ifndef CORE_FREEGLUT
    /*
    if (mod&GLUT_ACTIVE_SHIFT){
      keydown_vk(VK+QBVK_LSHIFT);
    }else{
      keyup_vk(VK+QBVK_LSHIFT);
    }

    if (mod&GLUT_ACTIVE_CTRL){
      keydown_vk(VK+QBVK_LCTRL);
    }else{
      keyup_vk(VK+QBVK_LCTRL);
    }

    if (mod&GLUT_ACTIVE_ALT){
      keydown_vk(VK+QBVK_LALT);
    }else{
      keyup_vk(VK+QBVK_LALT);
    }
    */
#endif 

//Note: The following is required regardless of whether FREEGLUT is/isn't being used          
//#ifdef CORE_FREEGLUT          
    //Is CTRL key down? If so, unencode character (applying shift as required)
    if (mod&2){
      //if (key==127){ //Removed: Might clash with CTRL+DELETE
      // key=8;
      // goto ctrl_mod;
      //}//127
      //if (key==3){//CTRL+(BREAK|SCROLL-LOCK)
      //if (down) keydown_vk(VK+QBVK_BREAK); else keyup_vk(VK+QBVK_BREAK);
      //return;
      //}
      if (key==10){
    key=13;
    goto ctrl_mod;
      }//10
      if ((key>=1)&&(key<=26)){
    if (mod&1) key=key-1+65; else key=key-1+97;//assume caps lock off
    goto ctrl_mod;
      }//1-26
    }
  ctrl_mod:
//#endif

#ifdef QB64_MACOSX

    //swap DEL and backspace keys

    if (key==8){
      key=127;
    }else{
      if (key==127){
    key=8;
      }
    }

#endif

    if (key==127){//delete
      if (down) keydown_vk(0x5300); else keyup_vk(0x5300);
      return;
    }
    if (down) keydown_ascii(key); else keyup_ascii(key);
#endif
  }

  void GLUT_KEYBOARD_FUNC(unsigned char key,int x, int y){

          
 
          
          

    //glutPostRedisplay();

    //qbs_print(qbs_str(key),0);
    //qbs_print(qbs_str((int32)glutGetModifiers()),1);

    GLUT_key_ascii(key,1);
  }
  void GLUT_KEYBOARDUP_FUNC(unsigned char key,int x, int y){
    GLUT_key_ascii(key,0);
  }

  void GLUT_key_special(int32 key,int32 down){

#ifdef QB64_GLUT
#ifndef CORE_FREEGLUT
    /*
        static int32 mod;
    mod=glutGetModifiers();//shift=1, control=2, alt=4
    if (mod&GLUT_ACTIVE_SHIFT){
      keydown_vk(VK+QBVK_LSHIFT);
    }else{
      keyup_vk(VK+QBVK_LSHIFT);
    }

    if (mod&GLUT_ACTIVE_CTRL){
      keydown_vk(VK+QBVK_LCTRL);
    }else{
      keyup_vk(VK+QBVK_LCTRL);
    }

    if (mod&GLUT_ACTIVE_ALT){
      keydown_vk(VK+QBVK_LALT);
    }else{
      keyup_vk(VK+QBVK_LALT);
    }
        */
#endif
          
    static int32 vk;
    vk=-1;
    if (key==GLUT_KEY_F1){vk=0x3B00;}
    if (key==GLUT_KEY_F2){vk=0x3C00;}
    if (key==GLUT_KEY_F3){vk=0x3D00;}
    if (key==GLUT_KEY_F4){vk=0x3E00;}
    if (key==GLUT_KEY_F5){vk=0x3F00;}
    if (key==GLUT_KEY_F6){vk=0x4000;}
    if (key==GLUT_KEY_F7){vk=0x4100;}
    if (key==GLUT_KEY_F8){vk=0x4200;}
    if (key==GLUT_KEY_F9){vk=0x4300;}
    if (key==GLUT_KEY_F10){vk=0x4400;}
    if (key==GLUT_KEY_F11){vk=0x8500;}
    if (key==GLUT_KEY_F12){vk=0x8600;}
    if (key==GLUT_KEY_LEFT){vk=0x4B00;}
    if (key==GLUT_KEY_UP){vk=0x4800;}
    if (key==GLUT_KEY_RIGHT){vk=0x4D00;}
    if (key==GLUT_KEY_DOWN){vk=0x5000;}
    if (key==GLUT_KEY_PAGE_UP){vk=0x4900;}
    if (key==GLUT_KEY_PAGE_DOWN){vk=0x5100;}
    if (key==GLUT_KEY_HOME){vk=0x4700;}
    if (key==GLUT_KEY_END){vk=0x4F00;}
    if (key==GLUT_KEY_INSERT){vk=0x5200;}

#ifdef CORE_FREEGLUT
	if (key==112){vk=VK+QBVK_LSHIFT;}
	if (key==113){vk=VK+QBVK_RSHIFT;}
	if (key==114){vk=VK+QBVK_LCTRL;}
	if (key==115){vk=VK+QBVK_RCTRL;}
	if (key==116){vk=VK+QBVK_LALT;}
	if (key==117){vk=VK+QBVK_RALT;}
#endif

    if (vk!=-1){
      if (down) keydown_vk(vk); else keyup_vk(vk);
    }

#endif
  }

  void GLUT_SPECIAL_FUNC(int key, int x, int y){

    //qbs_print(qbs_str((int32)glutGetModifiers()),1);

    GLUT_key_special(key,1);
  }
  void GLUT_SPECIALUP_FUNC(int key, int x, int y){
    GLUT_key_special(key,0);
  }


#ifdef QB64_WINDOWS
  void GLUT_TIMER_EVENT(int ignore){
#ifdef QB64_GLUT
    glutPostRedisplay();
    int32 msdelay=1000.0/max_fps;
    Sleep(4); msdelay-=4;//this forces GLUT to relinquish some CPU time to other threads but still allow for _FPS 100+ 
    if (msdelay<1) msdelay=1;
    glutTimerFunc(msdelay,GLUT_TIMER_EVENT,0);
#endif
  }
#else
  void GLUT_IDLEFUNC(){

#ifdef QB64_MACOSX          
#ifdef DEPENDENCY_DEVICEINPUT
          //must be in same thread as GLUT for OSX
          QB64_GAMEPAD_POLL();
          //[[[[NSApplication sharedApplication] mainWindow] standardWindowButton:NSWindowCloseButton] setEnabled:YES];
#endif         
#endif
          
#ifdef QB64_GLUT

    if (x11_lock_request){     
     x11_locked=1;
     x11_lock_request=0;
     while (x11_locked) Sleep(1);
    }

    glutPostRedisplay();
    int32 msdelay=1000.0/max_fps;
    if (msdelay<1) msdelay=1;
    Sleep(msdelay);
#endif
  }
#endif

#include "libqb/gui.cpp"

  void sub__title(qbs *title){
    if (new_error) return;
    static qbs *cz=NULL;
    if (!cz){cz=qbs_new(1,0); cz->chr[0]=0;}
    static qbs *str=NULL; if (!str) str=qbs_new(0,0);
    qbs_set(str,qbs_add(title,cz));

    uint8 *buf,*old_buf;
    buf=(uint8*)malloc(str->len);
    memcpy(buf,str->chr,str->len);
    old_buf=window_title;
    window_title=buf;
    if (old_buf) free(old_buf);

    if (window_exists){
#ifdef QB64_GLUT
      glutSetWindowTitle((char*)window_title);
#endif
    }

  }//title


  //                     0 1  2        0 1       2
  void sub__resize(int32 on_off, int32 stretch_smooth){

    if (on_off==1) resize_snapback=0;
    if (on_off==2) resize_snapback=1;
    //no change if omitted

    if (stretch_smooth){
      resize_auto=stretch_smooth;
    }else{
      resize_auto=0;//revert if omitted
    }

  }

  int32 func__resize(){
    if (resize_snapback) return 0; //resize must be enabled
    if (resize_event){
      resize_event=0;
      return -1;
    } 
    return 0;
  }

  int32 func__resizewidth(){
    return resize_event_x;
  }
  int32 func__resizeheight(){
    return resize_event_y;
  }

  int32 func__scaledwidth(){
    return environment_2d__screen_scaled_width;
  }
  int32 func__scaledheight(){
    return environment_2d__screen_scaled_height;
  }



//Get Current Working Directory
qbs *func__cwd(){
  qbs *final, *tqbs;
  int length;
  char *buf, *ret;

#if defined QB64_WINDOWS
  length = GetCurrentDirectoryA(0, NULL);
  buf = (char *)malloc(length);
  if (!buf) {
    error(7); //"Out of memory"
    return tqbs;
  }
  if (GetCurrentDirectoryA(length, buf) != --length) { //Sanity check
    free(buf); //It's good practice
    tqbs = qbs_new(0, 1);
    error(51); //"Internal error"
    return tqbs;
  }
#elif defined QB64_UNIX
  length = 512;
  while(1) {
    buf = (char *)malloc(length);
    if (!buf) {
      tqbs = qbs_new(0, 1);
      error(7);
      return tqbs;
    }
    ret = getcwd(buf, length);
    if (ret) break;
    if (errno != ERANGE) {
      tqbs = qbs_new(0, 1);
      error(51);
      return tqbs;
    }
    free(buf);
    length += 512;
  }
  length = strlen(ret);
  ret = (char *)realloc(ret, length); //Chops off the null byte
  if (!ret) {
    tqbs = qbs_new(0, 1);
    error(7);
    return tqbs;
  }
  buf = ret;
#endif
  final = qbs_new(length, 1);
  memcpy(final->chr, buf, length);
  free(buf);
  return final;
}

qbs *startDir=NULL;//set on startup
qbs *func__startdir(){
        qbs *temp=qbs_new(0, 1);
        qbs_set(temp, startDir);
        return temp;
}

qbs *rootDir=NULL;//the dir moved to when program begins

char *android_dir_downloads=NULL;
char *android_dir_documents=NULL;
char *android_dir_pictures=NULL;
char *android_dir_music=NULL;
char *android_dir_video=NULL;
char *android_dir_dcim=NULL;

qbs *func__dir(qbs* context_in){
    
    	static qbs *context=NULL;
	if (!context){context=qbs_new(0,0);}

	qbs_set(context,qbs_ucase(context_in));

	if (qbs_equal(qbs_ucase(context),qbs_new_txt("TEXT"))||qbs_equal(qbs_ucase(context),qbs_new_txt("DOCUMENT"))||qbs_equal(qbs_ucase(context),qbs_new_txt("DOCUMENTS"))||qbs_equal(qbs_ucase(context),qbs_new_txt("MY DOCUMENTS"))){
		#ifdef QB64_ANDROID
            mkdir(android_dir_documents,0770);
			return qbs_new_txt(android_dir_documents);
		#endif
		#ifdef QB64_WINDOWS
			CHAR osPath[MAX_PATH];
			if(SUCCEEDED(SHGetFolderPathA(NULL,5,NULL,0,osPath))){ //Documents
				return qbs_add(qbs_new_txt(osPath),qbs_new_txt("\\"));
			}
		#endif
	}
	
	if (qbs_equal(qbs_ucase(context),qbs_new_txt("MUSIC"))||qbs_equal(qbs_ucase(context),qbs_new_txt("AUDIO"))||qbs_equal(qbs_ucase(context),qbs_new_txt("SOUND"))||qbs_equal(qbs_ucase(context),qbs_new_txt("SOUNDS"))||qbs_equal(qbs_ucase(context),qbs_new_txt("MY MUSIC"))){
		#ifdef QB64_ANDROID
            mkdir(android_dir_music,0770);
			return qbs_new_txt(android_dir_music);
		#endif
		#ifdef QB64_WINDOWS
			CHAR osPath[MAX_PATH];
			if(SUCCEEDED(SHGetFolderPathA(NULL,13,NULL,0,osPath))){ //Music
				return qbs_add(qbs_new_txt(osPath),qbs_new_txt("\\"));
			}
		#endif
	}

	if (qbs_equal(qbs_ucase(context),qbs_new_txt("PICTURE"))||qbs_equal(qbs_ucase(context),qbs_new_txt("PICTURES"))||qbs_equal(qbs_ucase(context),qbs_new_txt("IMAGE"))||qbs_equal(qbs_ucase(context),qbs_new_txt("IMAGES"))||qbs_equal(qbs_ucase(context),qbs_new_txt("MY PICTURES"))){
		#ifdef QB64_ANDROID
            		mkdir(android_dir_pictures,0770);
			return qbs_new_txt(android_dir_pictures);
		#endif
		#ifdef QB64_WINDOWS
			CHAR osPath[MAX_PATH];
			if(SUCCEEDED(SHGetFolderPathA(NULL,39,NULL,0,osPath))){//Pictures
				return qbs_add(qbs_new_txt(osPath),qbs_new_txt("\\"));
			}
		#endif
	}

	if (qbs_equal(qbs_ucase(context),qbs_new_txt("DCIM"))||qbs_equal(qbs_ucase(context),qbs_new_txt("CAMERA"))||qbs_equal(qbs_ucase(context),qbs_new_txt("CAMERA ROLL"))||qbs_equal(qbs_ucase(context),qbs_new_txt("PHOTO"))||qbs_equal(qbs_ucase(context),qbs_new_txt("PHOTOS"))){
		#ifdef QB64_ANDROID
            		mkdir(android_dir_dcim,0770);
			return qbs_new_txt(android_dir_dcim);
		#endif
		#ifdef QB64_WINDOWS
			CHAR osPath[MAX_PATH];
			if(SUCCEEDED(SHGetFolderPathA(NULL,39,NULL,0,osPath))){//Pictures
				return qbs_add(qbs_new_txt(osPath),qbs_new_txt("\\"));
			}
		#endif
	}

	if (qbs_equal(qbs_ucase(context),qbs_new_txt("MOVIE"))||qbs_equal(qbs_ucase(context),qbs_new_txt("MOVIES"))||qbs_equal(qbs_ucase(context),qbs_new_txt("VIDEO"))||qbs_equal(qbs_ucase(context),qbs_new_txt("VIDEOS"))||qbs_equal(qbs_ucase(context),qbs_new_txt("MY VIDEOS"))){
		#ifdef QB64_ANDROID
			mkdir(android_dir_video,0770);
			return qbs_new_txt(android_dir_video);
		#endif
		#ifdef QB64_WINDOWS
			CHAR osPath[MAX_PATH];
			if(SUCCEEDED(SHGetFolderPathA(NULL,14,NULL,0,osPath))){ //Videos
				return qbs_add(qbs_new_txt(osPath),qbs_new_txt("\\"));
			}
		#endif
	}

	if (qbs_equal(qbs_ucase(context),qbs_new_txt("DOWNLOAD"))||qbs_equal(qbs_ucase(context),qbs_new_txt("DOWNLOADS"))){
		#ifdef QB64_ANDROID
	        	mkdir(android_dir_downloads,0770);
			return qbs_new_txt(android_dir_downloads);
		#endif
		#ifdef QB64_WINDOWS
			CHAR osPath[MAX_PATH];
			if(SUCCEEDED(SHGetFolderPathA(NULL,0x0028,NULL,0,osPath))){//user folder
				//XP & SHGetFolderPathA do not support the concept of a Downloads folder, however it can be constructed
				mkdir((char*)((qbs_add(qbs_new_txt(osPath),qbs_new_txt_len("\\Downloads\0",11)))->chr));
				return qbs_add(qbs_new_txt(osPath),qbs_new_txt("\\Downloads\\"));
			}
		#endif
	}

	if (qbs_equal(qbs_ucase(context),qbs_new_txt("DESKTOP"))){
		#ifdef QB64_ANDROID
			mkdir(android_dir_downloads,0770);
			return qbs_new_txt(android_dir_downloads);
		#endif
		#ifdef QB64_WINDOWS
			CHAR osPath[MAX_PATH];
			if(SUCCEEDED(SHGetFolderPathA(NULL,0,NULL,0,osPath))){ //Desktop
				return qbs_add(qbs_new_txt(osPath),qbs_new_txt("\\"));
			}
		#endif
	}

	if (qbs_equal(qbs_ucase(context),qbs_new_txt("APPDATA"))||qbs_equal(qbs_ucase(context),qbs_new_txt("APPLICATION DATA"))||qbs_equal(qbs_ucase(context),qbs_new_txt("PROGRAM DATA"))||qbs_equal(qbs_ucase(context),qbs_new_txt("DATA"))){
		#ifdef QB64_ANDROID			
			return qbs_add(rootDir,qbs_new_txt("/"));
		#endif
		#ifdef QB64_WINDOWS
			CHAR osPath[MAX_PATH];
			if(SUCCEEDED(SHGetFolderPathA(NULL,0x001a,NULL,0,osPath))){ //CSIDL_APPDATA (%APPDATA%)
				return qbs_add(qbs_new_txt(osPath),qbs_new_txt("\\"));
			}
		#endif
	}

	if (qbs_equal(qbs_ucase(context),qbs_new_txt("LOCALAPPDATA"))||qbs_equal(qbs_ucase(context),qbs_new_txt("LOCAL APPLICATION DATA"))||qbs_equal(qbs_ucase(context),qbs_new_txt("LOCAL PROGRAM DATA"))||qbs_equal(qbs_ucase(context),qbs_new_txt("LOCAL DATA"))){
		#ifdef QB64_ANDROID
			return qbs_add(rootDir,qbs_new_txt("/"));
		#endif
		#ifdef QB64_WINDOWS
			CHAR osPath[MAX_PATH];
			if(SUCCEEDED(SHGetFolderPathA(NULL,0x001c,NULL,0,osPath))){ //CSIDL_LOCAL_APPDATA (%LOCALAPPDATA%)
				return qbs_add(qbs_new_txt(osPath),qbs_new_txt("\\"));
			}
		#endif
	}

	//general fallback location
	#ifdef QB64_WINDOWS
		CHAR osPath[MAX_PATH];
		if(SUCCEEDED(SHGetFolderPathA(NULL,0,NULL,0,osPath))){ //desktop
			return qbs_add(qbs_new_txt(osPath),qbs_new_txt("\\"));
		}
		return qbs_new_txt(".\\");//current location
	#else
		#ifdef QB64_ANDROID
            		mkdir(android_dir_downloads,0770);
			return qbs_new_txt(android_dir_downloads);
		#endif
		return qbs_new_txt("./");//current location
	#endif
}

  extern void set_dynamic_info();
  int main( int argc, char* argv[] ){

#if defined(QB64_LINUX) && defined(X11)
    XInitThreads();
#endif

    static int32 i,i2,i3,i4;
    static uint8 c,c2,c3,c4;
    static int32 x,x2,x3,x4;
    static int32 y,y2,y3,y4;
    static int32 z,z2,z3,z4;
    static float f,f2,f3,f4;
    static uint8 *cp,*cp2,*cp3,*cp4;





/********** Render State **********/
render_state.dest=NULL;
render_state.source=NULL;
render_state.dest_handle=INVALID_HARDWARE_HANDLE;
render_state.source_handle=INVALID_HARDWARE_HANDLE;
render_state.view_mode=VIEW_MODE__UNKNOWN;
render_state.use_alpha=ALPHA_MODE__UNKNOWN;
render_state.depthbuffer_mode=DEPTHBUFFER_MODE__UNKNOWN;
render_state.cull_mode=CULL_MODE__UNKNOWN;
/********** Render State **********/




    for (i=0;i<=2;i++){
      display_frame[i].state=DISPLAY_FRAME_STATE__EMPTY;
      display_frame[i].order=0;
      display_frame[i].bgra=NULL;
      display_frame[i].w=0; display_frame[i].h=0;
      display_frame[i].bytes=0;
    }




    set_dynamic_info();
    if (ScreenResize){
      resize_snapback=0;
    }
    if (ScreenResizeScale){
      resize_auto=ScreenResizeScale;
    }


    //setup lists
    mouse_message_queue_handles=list_new(sizeof(mouse_message_queue_struct));
    special_handles=list_new(sizeof(special_handle_struct));
    stream_handles=list_new(sizeof(stream_struct));
    connection_handles=list_new(sizeof(connection_struct));

    hardware_img_handles=list_new_threadsafe(sizeof(hardware_img_struct));
    hardware_graphics_command_handles=list_new(sizeof(hardware_graphics_command_struct));

    //setup default mouse message queue
    mouse_message_queue_first=list_add(mouse_message_queue_handles);
    mouse_message_queue_default=mouse_message_queue_first;
    mouse_message_queue_struct *this_mouse_message_queue=(mouse_message_queue_struct*)list_get(mouse_message_queue_handles,mouse_message_queue_default);
    this_mouse_message_queue->lastIndex=65535;
    this_mouse_message_queue->queue=(mouse_message*)calloc(1,sizeof(mouse_message)*(this_mouse_message_queue->lastIndex+1));

    if (!cloud_app){
      snd_init();
    }



    if (screen_hide_startup) screen_hide=1;

#ifdef QB64_WINDOWS
    if (console){
      LPDWORD plist=(LPDWORD)malloc(1000);
      if (GetConsoleProcessList(plist,256)==1){
    console_child=1;//only this program is using the console
      }
    }
#endif





    onkey[1].keycode=59<<8;//F1-F10
    onkey[2].keycode=60<<8;
    onkey[3].keycode=61<<8;
    onkey[4].keycode=62<<8;
    onkey[5].keycode=63<<8;
    onkey[6].keycode=64<<8;
    onkey[7].keycode=65<<8;
    onkey[8].keycode=66<<8;
    onkey[9].keycode=67<<8;
    onkey[10].keycode=68<<8;
    onkey[11].keycode=72<<8;//up,left,right,down
    onkey[11].keycode_alternate=VK+QBVK_KP8;
    onkey[12].keycode=75<<8;
    onkey[12].keycode_alternate=VK+QBVK_KP4;
    onkey[13].keycode=77<<8;
    onkey[13].keycode_alternate=VK+QBVK_KP6;
    onkey[14].keycode=80<<8;
    onkey[14].keycode_alternate=VK+QBVK_KP2;
    onkey[30].keycode=133<<8;//F11,F12
    onkey[31].keycode=134<<8;

    ontimer[0].allocated=1;
    ontimer[0].id=0;
    ontimer[0].state=0;
    ontimer[0].active=0;







    {
      /* For bounds check on numeric ENVIRON$ */
      char **p = envp;
      while (*p++);
      environ_count = p - envp;
    }

    fontwidth[8]=8; fontwidth[14]=8; fontwidth[16]=8;
    fontheight[8]=8; fontheight[14]=14; fontheight[16]=16;
    fontflags[8]=16; fontflags[14]=16; fontflags[16]=16;//monospace flag
    fontwidth[8+1]=8*2; fontwidth[14+1]=8*2; fontwidth[16+1]=8*2;
    fontheight[8+1]=8; fontheight[14+1]=14; fontheight[16+1]=16;
    fontflags[8+1]=16; fontflags[14+1]=16; fontflags[16+1]=16;//monospace flag

    memset(img,0,IMG_BUFFERSIZE*sizeof(img_struct));
    x=newimg();//reserve index 0
    img[x].valid=0;
    x=newimg();//reserve index 1
    img[x].valid=0;








    memset(&cpu,0,sizeof(cpu_struct));

    //uint8 *asmcodep=(uint8*)&asmcode[0];
    //memcpy(&cmem[0],asmcodep,sizeof(asmcode));
    reg8[0]=&cpu.al;
    reg8[1]=&cpu.cl;
    reg8[2]=&cpu.dl;
    reg8[3]=&cpu.bl;
    reg8[4]=&cpu.ah;
    reg8[5]=&cpu.ch;
    reg8[6]=&cpu.dh;
    reg8[7]=&cpu.bh;

    reg16[0]=&cpu.ax;
    reg16[1]=&cpu.cx;
    reg16[2]=&cpu.dx;
    reg16[3]=&cpu.bx;
    reg16[4]=&cpu.sp;
    reg16[5]=&cpu.bp;
    reg16[6]=&cpu.si;
    reg16[7]=&cpu.di;

    reg32[0]=&cpu.eax;
    reg32[1]=&cpu.ecx;
    reg32[2]=&cpu.edx;
    reg32[3]=&cpu.ebx;
    reg32[4]=&cpu.esp;
    reg32[5]=&cpu.ebp;
    reg32[6]=&cpu.esi;
    reg32[7]=&cpu.edi;

    segreg[0]=&cpu.es;
    segreg[1]=&cpu.cs;
    segreg[2]=&cpu.ss;
    segreg[3]=&cpu.ds;
    segreg[4]=&cpu.fs;
    segreg[5]=&cpu.gs;

#ifdef QB64_WINDOWS





    /*

      HANDLE WINAPI GetStdHandle(
      __in  DWORD nStdHandle
      );
      STD_INPUT_HANDLE
      (DWORD)-10

    

      The standard input device. Initially, this is the console input buffer, CONIN$.

      STD_OUTPUT_HANDLE
      (DWORD)-11

    

      The standard output device. Initially, this is the active console screen buffer, CONOUT$.

      STD_ERROR_HANDLE
      (DWORD)-12

    

      The standard error device. Initially, this is the active console screen buffer, CONOUT$.

      // BOOL WINAPI SetConsoleMode(
      //   __in  HANDLE hConsoleHandle,
      //   __in  DWORD dwMode
      // );
      */



#endif

    for (i=0;i<32;i++) sub_file_print_spaces[i]=32;


    port60h_event[0]=128+1;//simulate release of ESC


    mem_static_size=1048576;//1MEG
    mem_static=(uint8*)malloc(mem_static_size);
    mem_static_pointer=mem_static;
    mem_static_limit=mem_static+mem_static_size;

    memset(&cmem[0],0,sizeof(cmem));










    memset(&keyon[0],0,sizeof(keyon));

    dblock=(ptrszint)&cmem+1280;//0:500h

    //define "nothing"
    cmem_sp-=8; nothingvalue=(uint64*)(dblock+cmem_sp);
    *nothingvalue=0;
    nothingstring=qbs_new_cmem(0,0);
    singlespace=qbs_new_cmem(1,0);
    singlespace->chr[0]=32;

//store _CWD$ for recall using _STARTDIR$ in startDir
startDir=qbs_new(0,0);
qbs_set(startDir,func__cwd());

//switch to directory of this EXE file
//http://stackoverflow.com/questions/1023306/finding-current-executables-path-without-proc-self-exe
#if defined(QB64_WINDOWS) && !defined(QB64_MICROSOFT)
    static char *exepath=(char*)malloc(65536);
    GetModuleFileName(NULL,exepath,65536);
    i=strlen(exepath);
    for (i2=i-1;i2>=0;i2--){
      x=exepath[i2];
      if ((x==92)||(x==47)||(x==58)){
    if (x==58) exepath[i2+1]=0; else exepath[i2]=0;
    break;
      }
    }
    chdir(exepath);
#elif defined(QB64_LINUX)
                {
                        char pathbuf[65536];
                        memset(pathbuf, 0, sizeof(pathbuf));
                        readlink("/proc/self/exe", pathbuf, 65535);
                        chdir(dirname(pathbuf));
                }
#elif defined(QB64_MACOSX)
                {
                        char pathbuf[65536];
                        uint32_t pathbufsize = sizeof(pathbuf);
                        _NSGetExecutablePath(pathbuf, &pathbufsize);                        
                        chdir(dirname(pathbuf));
                }
#endif

rootDir=qbs_new(0,0);
qbs_set(rootDir,func__cwd());

    unknown_opcode_mess=qbs_new(0,0);
    qbs_set(unknown_opcode_mess,qbs_new_txt_len("Unknown Opcode (  )\0",20));

    i=argc;
    if (i>1){
      //calculate required size of COMMAND$ string
      i2=0;
      for (i=1;i<argc;i++){
    i2+=strlen(argv[i]);
    if (i!=1) i2++;//for a space
      }
      //create COMMAND$ string
      func_command_str=qbs_new(i2,0);
      //build COMMAND$ string
      i3=0;
      for (i=1;i<argc;i++){
    if (i!=1){func_command_str->chr[i3]=32; i3++;}
    memcpy(&func_command_str->chr[i3],argv[i],strlen(argv[i])); i3+=strlen(argv[i]);
      }
    }else{
      func_command_str=qbs_new(0,0);
    }

    func_command_count = argc;
    func_command_array = argv;


    //struct tm:
    //        int tm_sec;     /* seconds after the minute - [0,59] */
    //        int tm_min;     /* minutes after the hour - [0,59] */
    //        int tm_hour;    /* hours since midnight - [0,23] */
    //        int tm_mday;    /* day of the month - [1,31] */
    //        int tm_mon;     /* months since January - [0,11] */
    //        int tm_year;    /* years since 1900 */
    //        int tm_wday;    /* days since Sunday - [0,6] */
    //        int tm_yday;    /* days since January 1 - [0,365] */
    //        int tm_isdst;   /* daylight savings time flag */
    tm *qb64_tm;
    time_t qb64_tm_val;
    time_t qb64_tm_val_old;
    //call both timing routines as close as possible to each other to maximize accuracy
    //wait for second "hand" to "tick over"/move
    time(&qb64_tm_val_old);
    //note: time() returns the time as seconds elapsed since midnight, January 1, 1970, or -1 in the case of an error. 
    if (qb64_tm_val_old!=-1){
      do{
    time(&qb64_tm_val);
      }while(qb64_tm_val==qb64_tm_val_old);
    }else{
      qb64_tm_val=0;//time unknown! (set to midnight, January 1, 1970)
    }
    clock_firsttimervalue=GetTicks();
    //calculate localtime as milliseconds past midnight
    qb64_tm=localtime(&qb64_tm_val);
    /* re: localtime()
       Return a pointer to the structure result, or NULL if the date passed to the function is:
       Before midnight, January 1, 1970.
       After 03:14:07, January 19, 2038, UTC (using _time32 and time32_t).
       After 23:59:59, December 31, 3000, UTC (using _time64 and __time64_t).
    */
    if (qb64_tm){
      qb64_firsttimervalue=qb64_tm->tm_hour*3600+qb64_tm->tm_min*60+qb64_tm->tm_sec;
      qb64_firsttimervalue*=1000;
    }else{
      qb64_firsttimervalue=0;//time unknown! (set to midnight)
    }
    /* Used as follows for calculating TIMER value:
       x=GetTicks();
       x-=clock_firsttimervalue;
       x+=qb64_firsttimervalue;
    */

    //init truetype .ttf/.fon font library

#ifdef QB64_WINDOWS 
    //for caps lock, use the state of the lock (=1)
    //for held keys check against (=-127)
    if (GetKeyState(VK_SCROLL)&1) keyheld_add(QBK+QBK_SCROLL_LOCK_MODE);
    if (GetKeyState(VK_SCROLL)<0){bindkey=QBVK_SCROLLOCK; keydown_vk(VK+QBVK_SCROLLOCK);}
    if (GetKeyState(VK_LSHIFT)<0){bindkey=QBVK_LSHIFT; keydown_vk(VK+QBVK_LSHIFT);}
    if (GetKeyState(VK_RSHIFT)<0){bindkey=QBVK_RSHIFT; keydown_vk(VK+QBVK_RSHIFT);}
    if (GetKeyState(VK_LCONTROL)<0){bindkey=QBVK_LCTRL; keydown_vk(VK+QBVK_LCTRL);}
    if (GetKeyState(VK_RCONTROL)<0){bindkey=QBVK_RCTRL; keydown_vk(VK+QBVK_RCTRL);}
    if (GetKeyState(VK_LMENU)<0){bindkey=QBVK_LALT; keydown_vk(VK+QBVK_LALT);}
    if (GetKeyState(VK_RMENU)<0){bindkey=QBVK_RALT; keydown_vk(VK+QBVK_RALT);}
    if (GetKeyState(VK_CAPITAL)&1){bindkey=QBVK_CAPSLOCK; keydown_vk(VK+QBVK_CAPSLOCK);}
    if (GetKeyState(VK_NUMLOCK)&1){bindkey=QBVK_NUMLOCK; keydown_vk(VK+QBVK_NUMLOCK);}
    update_shift_state();
    keyhit_next=keyhit_nextfree;//skip hitkey events generated by above code
#endif


    //init fake keyb. cyclic buffer
    cmem[0x41a]=30; cmem[0x41b]=0; //head
    cmem[0x41c]=30; cmem[0x41d]=0; //tail

    ifstream fh;

    //default 256 color palette
    memcpy(&palette_256,&file_qb64_pal[0],file_qb64_pal_len);
    for(i=0;i<256;i++) palette_256[i]|=0xFF000000;

    //default EGA(64) color palette
    memcpy(&palette_64,&file_qb64ega_pal[0],file_qb64ega_pal_len);
    for(i=0;i<64;i++) palette_64[i]|=0xFF000000;

    //manually set screen 10 palette
    pal_mode10[0][0]=0;
    pal_mode10[0][1]=0;
    pal_mode10[0][2]=0;
    pal_mode10[0][3]=0x808080;
    pal_mode10[0][4]=0x808080;
    pal_mode10[0][5]=0x808080;
    pal_mode10[0][6]=0xFFFFFF;
    pal_mode10[0][7]=0xFFFFFF;
    pal_mode10[0][8]=0xFFFFFF;
    pal_mode10[1][0]=0;
    pal_mode10[1][1]=0x808080;
    pal_mode10[1][2]=0xFFFFFF;

    pal_mode10[1][3]=0;
    pal_mode10[1][4]=0x808080;
    pal_mode10[1][5]=0xFFFFFF;
    pal_mode10[1][6]=0;
    pal_mode10[1][7]=0x808080;
    pal_mode10[1][8]=0xFFFFFF;

    //8x8 character set
    memcpy(&charset8x8,&file_charset8_raw[0],file_charset8_raw_len);

    //8x16 character set
    memcpy(&charset8x16,&file_chrset16_raw[0],file_chrset16_raw_len);

    qbg_screen(0,NULL,NULL,NULL,NULL,1);
    width8050switch=1;//reaffirm switch reset by above command

    if (console){
      console_image=func__newimage(80,25,0,0);
      i=-console_image;
      img[i].console=1;
    }


    //int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
    //                   void *(*start_routine) (void *), void *arg);


    //create icon images
    init_icons();

//setup default _DEVICE(s)
i=0;

//keyboard
i++;
devices[i].type=DEVICETYPE_KEYBOARD;
devices[i].name="[KEYBOARD][BUTTON]";
devices[i].lastbutton=512;
devices[i].description="Keyboard";
setupDevice(&devices[i]);

//mouse
i++;
devices[i].type=DEVICETYPE_MOUSE;
devices[i].name="[MOUSE][BUTTON][AXIS][WHEEL]";
devices[i].lastbutton=3;
devices[i].lastaxis=2;
devices[i].lastwheel=3;
devices[i].description="Mouse";
setupDevice(&devices[i]);

device_last=i;


#ifdef DEPENDENCY_DEVICEINPUT
QB64_GAMEPAD_INIT();
#endif

#ifdef QB64_WINDOWS
    {
      uintptr_t thread_handle = _beginthread(QBMAIN_WINDOWS,0,NULL);
      SetThreadPriority((HANDLE)thread_handle, THREAD_PRIORITY_NORMAL);
    }    
#else
    {
      static pthread_t thread_handle;
      pthread_create(&thread_handle,NULL,&QBMAIN_LINUX,NULL);
    }
#endif

#ifdef QB64_WINDOWS    
    {
      uintptr_t thread_handle = _beginthread(TIMERTHREAD_WINDOWS,0,NULL);
      SetThreadPriority((HANDLE)thread_handle, THREAD_PRIORITY_NORMAL);
    }
#else
    {
      static pthread_t thread_handle;
      pthread_create(&thread_handle,NULL,&TIMERTHREAD_LINUX,NULL);
    }
#endif






    







    /*
      GLenum err = glewInit();
      if (GLEW_OK != err)
      {
      exit(0);
      }
      //fprintf(stdout, "Status: Using GLEW %s\n", glewGetString(GLEW_VERSION));


      GLenum err = glewInit();
      if (GLEW_OK != err)
      {
      qbs_print(qbs_str((int32)err),1);

      qbs_print(qbs_new_txt((char*)glewGetString(err)),1);

      //error(70);
      //exit(0);
      }
    */

    lock_display_required=1;

#ifndef QB64_GUI
    //normally MAIN_LOOP() is launched in a separate thread to reserve the primary thread for GLUT
    //that is not required, so run MAIN_LOOP() in our primary thread
    MAIN_LOOP();
    exit(0);
#endif    

#ifdef QB64_WINDOWS
    {
      uintptr_t thread_handle = _beginthread(MAIN_LOOP_WINDOWS,0,NULL);
      SetThreadPriority((HANDLE)thread_handle, THREAD_PRIORITY_NORMAL);
    }
#else
    {
      static pthread_t thread_handle;
      pthread_create(&thread_handle,NULL,&MAIN_LOOP_LINUX,NULL);
    }
#endif

    if (!screen_hide) create_window=1;
    
    while (!create_window){Sleep(100);}


#ifdef QB64_GLUT
    glutInit(&argc, argv);

#ifdef QB64_MACOSX  
          //This is a global keydown handler for OSX, it requires assistive devices in asseccibility to be enabled
          //becuase of security concerns (QB64 will not use this)
          /*
          [NSEvent addGlobalMonitorForEventsMatchingMask:NSKeyDownMask
                                                                                         handler:^(NSEvent *event){
           NSString *chars = [[event characters] lowercaseString];
           unichar character = [chars characterAtIndex:0];
           NSLog(@"keydown globally! Which key? This key: %c", character);
       }];
          */
                     
          /*
          [NSEvent addLocalMonitorForEventsMatchingMask:NSKeyDownMask handler:^NSEvent* (NSEvent* event){
           //NSString *keyPressed = event.charactersIgnoringModifiers;
           //[self.keystrokes appendString:keyPressed];
           NSString *chars = [[event characters] lowercaseString];
           unichar character = [chars characterAtIndex:0];
           NSLog(@"keydown locally! Which key? This key: %c", character);
           return event;
           }];
      */

      //[[[[NSApplication sharedApplication] mainWindow] standardWindowButton:NSWindowCloseButton] setEnabled:YES];
          
          [NSEvent addLocalMonitorForEventsMatchingMask:NSFlagsChangedMask handler:^NSEvent* (NSEvent* event){
           
           //notes on bitfields:
           //if ([event modifierFlags] == 131330) keydown_vk(VK+QBVK_LSHIFT);// 100000000100000010
           //if ([event modifierFlags] == 131332) keydown_vk(VK+QBVK_RSHIFT);// 100000000100000100                      
           //if ([event modifierFlags] == 262401) keydown_vk(VK+QBVK_LCTRL); //1000000000100000001
           //if ([event modifierFlags] == 270592) keydown_vk(VK+QBVK_RCTRL); //1000010000100000000           
           //if ([event modifierFlags] == 524576) keydown_vk(VK+QBVK_LALT); //10000000000100100000
           //if ([event modifierFlags] == 524608) keydown_vk(VK+QBVK_RALT); //10000000000101000000
       //caps lock                                                      //   10000000100000000
           
           int x=[event modifierFlags];
           
           if (x&(1<<0)){
           if (!keyheld(VK+QBVK_LCTRL)) keydown_vk(VK+QBVK_LCTRL);
           }else{
           if (keyheld(VK+QBVK_LCTRL)) keyup_vk(VK+QBVK_LCTRL);
           }
           if (x&(1<<13)){
           if (!keyheld(VK+QBVK_RCTRL)) keydown_vk(VK+QBVK_RCTRL);
           }else{
           if (keyheld(VK+QBVK_RCTRL)) keyup_vk(VK+QBVK_RCTRL);
           }
           
           if (x&(1<<1)){
           if (!keyheld(VK+QBVK_LSHIFT)) keydown_vk(VK+QBVK_LSHIFT);
           }else{
           if (keyheld(VK+QBVK_LSHIFT)) keyup_vk(VK+QBVK_LSHIFT);
           }
           if (x&(1<<2)){
           if (!keyheld(VK+QBVK_RSHIFT)) keydown_vk(VK+QBVK_RSHIFT);
           }else{
           if (keyheld(VK+QBVK_RSHIFT)) keyup_vk(VK+QBVK_RSHIFT);
           }
           
           if (x&(1<<5)){
           if (!keyheld(VK+QBVK_LALT)) keydown_vk(VK+QBVK_LALT);
           }else{
           if (keyheld(VK+QBVK_LALT)) keyup_vk(VK+QBVK_LALT);
           }
           if (x&(1<<6)){
           if (!keyheld(VK+QBVK_RALT)) keydown_vk(VK+QBVK_RALT);
           }else{
           if (keyheld(VK+QBVK_RALT)) keyup_vk(VK+QBVK_RALT);
           }
           
           if (x&(1<<16)){
           if (!keyheld(VK+QBVK_CAPSLOCK)) keydown_vk(VK+QBVK_CAPSLOCK);
           }else{
           if (keyheld(VK+QBVK_CAPSLOCK)) keyup_vk(VK+QBVK_CAPSLOCK);
           }
           
           return event;
           }];
          
       /*
          [NSEvent addLocalMonitorForEventsMatchingMask:NSKeyDownMask|NSFlagsChangedMask handler:^NSEvent *(NSEvent *incomingEvent) {
           if (incomingEvent.type == NSFlagsChanged && (incomingEvent.modifierFlags & NSDeviceIndependentModifierFlagsMask)) {
           NSLog(@"modifier key down");
           } else if (incomingEvent.type == NSKeyDown) {
           NSLog(@"other key down");
           }           
           return incomingEvent;
       }];
       */          
                    
          /*
          if (NSApp){
                  NSMenu      *menu;
                  NSMenuItem  *menuItem;  
                  
                  [NSApp setMainMenu:[[NSMenu alloc] init]];
                  
                  menu = [[NSMenu alloc] initWithTitle:@""];
                  [menu addItemWithTitle:@"About..." action:@selector(orderFrontStandardAboutPanel:) keyEquivalent:@""]; 
                  
                  menuItem = [[NSMenuItem alloc] initWithTitle:@"Apple" action:nil keyEquivalent:@""];
                  [menuItem setSubmenu:menu];
                  [[NSApp mainMenu] addItem:menuItem];
                  [NSApp setAppleMenu:menu];
     }  
         */

#endif 
          
    glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE | GLUT_DEPTH);

    glutInitWindowSize(640,400);//cannot be changed unless display_x(etc) are modified

    //glutInitWindowPosition(300, 200);

    if (!glutGet(GLUT_DISPLAY_MODE_POSSIBLE))//must be called on Linux or GLUT crashes
      { 
    exit(1);
      }

    if (!window_title){
      glutCreateWindow("Untitled");
    }else{
      glutCreateWindow((char*)window_title);
    }
    window_exists=1;

    GLenum err = glewInit();
    if (GLEW_OK != err) {
        alert( (char*)glewGetErrorString(err));
    }
    if (glewIsSupported("GL_EXT_framebuffer_object")) framebufferobjects_supported=1;

    glutDisplayFunc(GLUT_DISPLAY_REQUEST);

#ifdef QB64_WINDOWS
    glutTimerFunc(8,GLUT_TIMER_EVENT,0);
#else
    glutIdleFunc(GLUT_IDLEFUNC);
#endif

    glutKeyboardFunc(GLUT_KEYBOARD_FUNC);
    glutKeyboardUpFunc(GLUT_KEYBOARDUP_FUNC);
    glutSpecialFunc(GLUT_SPECIAL_FUNC);
    glutSpecialUpFunc(GLUT_SPECIALUP_FUNC);
    glutMouseFunc(GLUT_MOUSE_FUNC);
    glutMotionFunc(GLUT_MOTION_FUNC);
    glutPassiveMotionFunc(GLUT_PASSIVEMOTION_FUNC);
    glutReshapeFunc(GLUT_RESHAPE_FUNC);

#ifdef CORE_FREEGLUT
    glutMouseWheelFunc(GLUT_MOUSEWHEEL_FUNC);
#endif

    glutMainLoop();

#endif //QB64_GLUT

  }



  //###################### Main Loop ####################
#ifdef QB64_WINDOWS
  void MAIN_LOOP_WINDOWS(void *unused){
    MAIN_LOOP();
    return;
  }
#else
  void *MAIN_LOOP_LINUX(void *unused){
    MAIN_LOOP();
    return NULL;
  }
#endif
  void MAIN_LOOP(){

    int32 update=0;//0=update input,1=update display

  main_loop:
    
        #ifdef DEPENDENCY_DEVICEINPUT
                #ifndef QB64_MACOSX 
                        QB64_GAMEPAD_POLL();
                #endif
        #endif        

    if (lock_mainloop==1){
      lock_mainloop=2;
      while (lock_mainloop==2) Sleep(1);
    }

    if (exit_value){
      if (!exit_blocked) goto end_program;
    }


    //update timer bytes in cmem
    static uint32 cmem_ticks;
    static double cmem_ticks_double;

    cmem_ticks=GetTicks();
    cmem_ticks-=clock_firsttimervalue;
    cmem_ticks+=qb64_firsttimervalue;
    //make timer value loop after midnight
    //note: there are 86400000 milliseconds in 24hrs(1 day)
    cmem_ticks%=86400000;
    cmem_ticks=((double)cmem_ticks)*0.0182;
    cmem[0x46c]=cmem_ticks&255;
    cmem[0x46d]=(cmem_ticks>>8)&255;
    cmem[0x46e]=(cmem_ticks>>16)&255;
    //note: a discrepancy exists of unknown cause

    if (shell_call_in_progress){
      if (shell_call_in_progress!=-1){
    shell_call_in_progress=-1;
    goto update_display_only;
      }
      Sleep(64);
      goto main_loop;
    }

    Sleep(15);
    vertical_retrace_happened=1; vertical_retrace_in_progress=1;
    Sleep(1);

    if (close_program){
      lock_mainloop=2;//report mainloop as locked so that any process waiting for a successful lock can continue
      goto end_program;
    }

    if (!cloud_app){
      snd_mainloop();
    }



    //check for input event (qloud_next_input_index)
    if (cloud_app){

      //***should be replaced with a timer based check (or removed)***
      //static int qloud_input_frame_count=0;
      //qloud_input_frame_count++;
      //if (qloud_input_frame_count>8) qloud_input_frame_count=1;
      //if (qloud_input_frame_count==1){//~8 checks per second (would be ~64 without this check)

    qloud_input_recheck:

      FILE * pFile;
      long lSize;
      char * buffer;
      size_t result;
      pFile = NULL;
  
      static char filename[] = "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";
      sprintf(filename, "input_%d.txt\0", qloud_next_input_index);
      pFile = fopen ( filename , "rb" );
      if (pFile!=NULL) {
    // obtain file size:
    fseek (pFile , 0 , SEEK_END);
    lSize = ftell (pFile);
    rewind (pFile);
    if (lSize>0){
      // allocate memory to contain the whole file:
      buffer = (char*) calloc (1,sizeof(char)*lSize+1);
      if (buffer != NULL) {
        // copy the file into the buffer:
        result = fread (buffer,1,lSize,pFile);
        if (result == lSize) {

          if (buffer[lSize-1]==42){ //"*" terminator

        int start,stop;
        start=0;

        int bi;

          nextcommand:

        for (bi=start;bi<lSize;bi++){
          if (buffer[bi]==0) goto doneall;
          if (buffer[bi]==58) goto gotcolon;
        }
        goto doneall;
          gotcolon:

        int code;
        int v1,v2;


        code=buffer[start];
        start++;

        #ifdef QB64_GUI
        
        if (code==77){//M (mousemove)
          sscanf (buffer+start,"%d,%d",&v1,&v2);          
          GLUT_MOTION_FUNC(v1,v2);
        }//M

        if (code==76){//L (left mouse button)
          sscanf (buffer+start,"%d,%d",&v1,&v2);
          GLUT_MouseButton_Down(GLUT_LEFT_BUTTON,v1,v2);
        }
        if (code==108){//l (left mouse button up)
          sscanf (buffer+start,"%d,%d",&v1,&v2);
          GLUT_MouseButton_Up(GLUT_LEFT_BUTTON,v1,v2);
        }


        if (code==68){//D (key down)
          sscanf (buffer+start,"%d",&v1);
          keydown_vk(v1);
        }//D
        if (code==85){//U (key up)
          sscanf (buffer+start,"%d",&v1);
          keyup_vk(v1);
        }//U

        #endif

        start=bi+1;
        goto nextcommand;

          doneall:

        qloud_next_input_index++;

        free (buffer);
        fclose (pFile);

        goto qloud_input_recheck;

          }//* terminator

        }//read correct number of bytes

        free (buffer);
      }//could allocate buffer
    }//file has content
    fclose (pFile);
      }//not null

      //}//qloud_input_frame_count


    }//qloud app



    update^=1;//toggle update

    if (!lprint){//not currently performing an LPRINT operation
      lprint_locked=1;
      if (lprint_buffered){
    if (fabs(func_timer(0.001,1)-lprint_last)>=10.0){//10 seconds elapsed since last LPRINT operation
      sub__printimage(lprint_image);
      lprint_buffered=0;
      static int32 old_dest;
      old_dest=func__dest();
      sub__dest(lprint_image);
      sub_cls(NULL,15,2);
      sub__dest(old_dest);
    }
      }
      lprint_locked=0;
    }



    //note: this mainloop loops with breaks of 16ms, display is toggled every 2nd loop
    //update display?
    if (update==1){
    update_display_only:
      if (autodisplay) display();//occurs every 32ms or 31.25 times per second
      frame++;//~32 fps
    }//update==1

    vertical_retrace_in_progress=0;

    if (shell_call_in_progress) goto main_loop;

    if (update==0){

      static int32 scancode;
      static const uint8 QBVK_2_scancode[] = {
    0,0,0,0,0,0,0,0,14,15,0,0,0,28,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,57,0,0,0,0,0,0,40,0,0,0,0,51,12,52,53,11,2,3,4,5,6,7,8,9,10,0,39,0,13,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,26,43,27,0,0,41,30,48,46,32,18,33,34,35,23,36,37,38,50,49,24,25,16,19,31,20,22,47,17,45,21,44,0,0,0,0,83,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,82,79,80,81,75,76,77,71,72,73,83,53,55,74,78,28,0,72,80,77,75,82,71,79,73,81,59,60,61,62,63,64,65,66,67,68,133,134,0,0,0,0,0,0,69,58,70,54,42,29,29,56,56,0,0,91,92,0,0,0,0,55,197,93,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
      };

#ifdef QB64_WINDOWS 
      //manage important external keyboard lock/state changes
      if ((GetKeyState(VK_SCROLL)&1)!=keyheld(QBK+QBK_SCROLL_LOCK_MODE)){
    if (keyheld(QBK+QBK_SCROLL_LOCK_MODE)){
      keyheld_remove(QBK+QBK_SCROLL_LOCK_MODE);
    }else{
      keyheld_add(QBK+QBK_SCROLL_LOCK_MODE);
    }
    update_shift_state();
      }
      if ((GetKeyState(VK_CAPITAL)&1)!=keyheld(VK+QBVK_CAPSLOCK)){
    if (keyheld(VK+QBVK_CAPSLOCK)){
      bindkey=QBVK_CAPSLOCK; keyup_vk(VK+QBVK_CAPSLOCK);
    }else{
      bindkey=QBVK_CAPSLOCK; keydown_vk(VK+QBVK_CAPSLOCK);
    }
    update_shift_state();
      }
      if ((GetKeyState(VK_NUMLOCK)&1)!=keyheld(VK+QBVK_NUMLOCK)){
    if (keyheld(VK+QBVK_NUMLOCK)){
      bindkey=QBVK_NUMLOCK; keyup_vk(VK+QBVK_NUMLOCK);
    }else{
      bindkey=QBVK_NUMLOCK; keydown_vk(VK+QBVK_NUMLOCK);
    }
    update_shift_state();
      }
#endif

      if (shell_call_in_progress) goto main_loop;

    }//update==0

    goto main_loop;

  end_program:
    stop_program=1;
    qbevent=1;
    while (exit_ok!=3) Sleep(16);

    if (lprint_buffered){
      sub__printimage(lprint_image);//print any pending content
    }

    //close all open files
    sub_close(NULL,0);

    //shutdown device interface
    #ifdef DEPENDENCY_DEVICEINPUT
    QB64_GAMEPAD_SHUTDOWN();
    #endif

    if (!cloud_app){
      snd_un_init();
    }

    if (cloud_app){
      FILE *f = fopen("..\\final.txt", "w");
      if (f != NULL){        
        fprintf(f, "Program exited normally");
        fclose(f);
      }
      exit(0);//should log error
    }

    exit(exit_code);
  }









  //used to preserve the previous frame's content for comparison/reuse purposes
  uint8 *pixeldata=(uint8*)malloc(1);
  int32 pixeldatasize=1;
  uint32 paldata[256];

  //note: temporarily swapping a source palette is far more effecient than converting the resulting image pixels
  void swap_paldata_BGRA_with_RGBA(){
    static uint32 col;
    static uint32 *pos;
    pos=(uint32*)&paldata[0];
    static int32 pixels;
    pixels=256;
    while(pixels--){
      col=*pos;
      *pos++= (col&0xFF00FF00) | ((col & 0xFF0000) >> 16) | ((col & 0x0000FF) << 16);
    }
  }

  //display updates the visual page onto the visible window/monitor
  void display(){



    //general variables
    static int32 i,i2,i3,i4;
    static uint8 c,c2,c3,c4;
    static int32 x,x2,x3,x4;
    static int32 y,y2,y3,y4;
    static int32 z,z2,z3,z4;
    static float f,f2,f3,f4;
    static uint8 *cp,*cp2,*cp3,*cp4;
    uint32 *pixel;

    static uint8 BGRA_to_RGBA;//set to 1 to invert the output to RGBA
    BGRA_to_RGBA=0;//default is 0 but 1 is fun
    if (cloud_app){ //more converters handle the RGBA data format than BGRA which is dumped
      BGRA_to_RGBA=1;
    }




    if (lock_display==1){lock_display=2; Sleep(0);}

    if (cloud_app==0){
      if (screen_hide) {display_called=1; return;}
    }

    if (lock_display==0){



      //Identify which display_frame to build
      static int32 frame_i;
      frame_i=-1;
      //use LOWEST order ready frame
      static int64 lowest_order;
      lowest_order=display_frame_order_next;
      for (i=0;i<=2;i++){
    if (display_frame[i].order<lowest_order&&display_frame[i].state==DISPLAY_FRAME_STATE__READY){
      lowest_order=display_frame[i].order;
      frame_i=i;
    }
      }
      //or preferably an unused frame if possible (note: this check happens 2nd for threading reasons)
      for (i=0;i<=2;i++){
    if (display_frame[i].state==DISPLAY_FRAME_STATE__EMPTY){
      frame_i=i;    
    }
      }
      if (frame_i==-1){
    alert("Software frame buffer: Failed to find available frame");
    return;
      }
      display_frame[frame_i].state=DISPLAY_FRAME_STATE__BUILDING;
      display_frame[frame_i].order=display_frame_order_next++;




      if (cloud_app){
    static double cloud_timer_value=0;
    static double cloud_timer_now=0;
    cloud_timer_now=func_timer(0.001,1);
    if (fabs(cloud_timer_value-cloud_timer_now)<0.25){
      goto cloud_skip_frame;
    }
    cloud_timer_value=cloud_timer_now;
      }


      //validate display_page
      if (!display_page) goto display_page_invalid;



      //check what is possible in full screen
      x=display_page->width; y=display_page->height;

      if (display_page->compatible_mode==0){
    x=display_page->width*fontwidth[display_page->font]; y=display_page->height*fontheight[display_page->font];
      }

      //check for y-stretch flag?
      if (x<=512&&y<=384){
    x*=2; y*=2;
      }

      static int32 mode_square,mode_stretch;

      //find best fullscreen mode(s) (eg. square/"1:1", stretched)
      mode_square=-1;
      mode_stretch=-1;

      x=display_page->width; y=display_page->height;
      if (display_page->compatible_mode==0){
    x=display_page->width*fontwidth[display_page->font]; y=display_page->height*fontheight[display_page->font];
      }
      x_monitor=x; y_monitor=y;

      z=0; //?

      conversion_required=0;
      pixel=display_surface_offset;//<-will be made obselete

      if (!display_page->compatible_mode){//text

    static int32 show_flashing_last=0;
    static int32 show_cursor_last=0;
    static int32 check_last;
    static uint8 *cp,*cp2,*cp_last;
    static uint32 *lp;
    static int32 cx,cy;
    static int32 cx_last=-1,cy_last=-1;
    static int32 show_cursor;
    static int32 show_flashing;
    static uint8 chr,col,chr_last,col_last;
    static int32 qbg_y_offset;

    static int32 f,f_pitch,f_width,f_height;//font info
    f=display_page->font; f_width=fontwidth[f]; f_height=fontheight[f];

    static int32 content_changed;


    check_last=screen_last_valid;//If set, modify the previous pixelbuffer's contents
    content_changed=0;

    //Realloc pixel-buffer if necessary
    i=display_page->width*display_page->height*2;
    if (screen_last_size!=i){
      free(screen_last);
      screen_last=(uint8*)malloc(i);
      screen_last_size=i;
      check_last=0;
    }

    if (displayorder_screen==0 && check_last==1){
      //a valid frame of the correct dimensions exists and we are not required to display software content
      goto no_new_frame;
    }

    //Check/Prepare palette-buffer
    if (!check_last){
      //set pal_last (no prev pal was avilable to compare to)
      memcpy(&paldata,display_page->pal,256*4);
    }else{
      //if palette has changed, update paldata and draw all characters
      if (memcmp(&paldata[0],display_page->pal,256*4)){
        //Different palette
        memcpy(&paldata[0],display_page->pal,256*4);
        check_last=0;
      }
    }

    //Check/Prepare content
    if (check_last){
      //i=display_frame_end;
      if (memcmp(screen_last,display_page->offset,screen_last_size)){
        //Different content
        content_changed=1;
      }
    }

    //Note: frame is a global variable incremented ~32 times per second [2013]
    if (frame&8) show_cursor=1; else show_cursor=0;//[2013]halved cursor blink rate from 8 changes p/sec -> 4 changes p/sec
    if (frame&8) show_flashing=1; else show_flashing=0;
    if (cloud_app){
      static double cloud_timer_flash;
      cloud_timer_flash=func_timer(0.001,1)/2.0;
      static int64 cloud_timer_flash_int;
      cloud_timer_flash_int=cloud_timer_flash;
      if (cloud_timer_flash_int&1) show_cursor=1; else show_cursor=0;
      if (cloud_timer_flash_int&1) show_flashing=1; else show_flashing=0;
      //static int qloud_show_cursor=0;
      //qloud_show_cursor++; if (qloud_show_cursor&1) show_cursor=1; else show_cursor=0;
    }




    //calculate cursor position (base 0)
    cx=display_page->cursor_x-1; cy=display_page->cursor_y-1;
    if (display_page->holding_cursor==2){//special case
      if (cy<(display_page->height-1)){cy++; cx=0;}
    }

    if (check_last){
      if (show_flashing!=show_flashing_last) content_changed=1;
      if (show_cursor!=show_cursor_last) content_changed=1;
      if ((cx!=cx_last)||(cy!=cy_last)) content_changed=1;
    }

    if (!check_last) content_changed=1;

    if (!content_changed){
      //No content has changed, so skip the generation & display of this frame
      goto no_new_frame;
    }

    static int64 last_frame_i=0;

    //################################ Setup new frame ################################
    {
      static int32 new_size_bytes;
      new_size_bytes=x_monitor*y_monitor*4;
      if (new_size_bytes>display_frame[frame_i].bytes){
        free(display_frame[frame_i].bgra);
        display_frame[frame_i].bgra=(uint32*)malloc(new_size_bytes);
        display_frame[frame_i].bytes=new_size_bytes;
      }
      display_frame[frame_i].w=x_monitor; display_frame[frame_i].h=y_monitor;
    }

    display_surface_offset=display_frame[frame_i].bgra;

    //If a compare & update changes method will be used copy the previous content to the new buffer

    if (check_last){
      //find the most recently published page to compare with
      //(the most recent READY or DISPLAYING page)
      static int64 highest_order;
      highest_order=0;
      i2=-1;
      for (i3=0;i3<=2;i3++){
        if ((display_frame[i3].state==DISPLAY_FRAME_STATE__DISPLAYING||
         display_frame[i3].state==DISPLAY_FRAME_STATE__READY)
        &&display_frame[i3].order>highest_order){
          highest_order=display_frame[i3].order;
          i2=i3;
        }
      } 
      if (i2!=-1){
        memcpy(display_frame[frame_i].bgra,display_frame[i2].bgra,display_frame[frame_i].w*display_frame[frame_i].h*4);
      }else{
        alert("Text Screen Update: Failed to locate previous frame's data for comparison"); 
        check_last=0;//never occurs, safe-guard only
      }
    }

    qbg_y_offset=0;//the screen base offset
    cp=display_page->offset;//read from
    cp_last=screen_last;//written to for future comparisons


    if (BGRA_to_RGBA) swap_paldata_BGRA_with_RGBA();

    //outer loop
    y2=0;
    for (y=0;y<display_page->height;y++){
      x2=0;
      for (x=0;x<display_page->width;x++){

        chr=*cp; cp++; col=*cp; cp++;

        //can be skipped?
        chr_last=*cp_last; cp_last++; col_last=*cp_last; cp_last++;

        if (check_last){
          if (chr==chr_last){//same character
        if (col==col_last){//same colours
          if (col&128) if (show_flashing!=show_flashing_last) goto cantskip;//same flash
          if (x==cx) if (y==cy) if (show_cursor!=show_cursor_last) goto cantskip;//same cursor
          if (x==cx_last){ if (y==cy_last){
              if ((cx!=cx_last)||(cy!=cy_last)) goto cantskip;//fixup old cursor's location
            }}
          goto skip;
        }}}
      cantskip:
        cp_last-=2; *cp_last=chr; cp_last++; *cp_last=col; cp_last++;

        //set cp2 to the character's data
        z2=0;//double-width if set

        if (f>=32){//custom font

          static uint32 chr_utf32;
          chr_utf32=codepage437_to_unicode16[chr];

          static uint8 *rt_data_last=NULL;
          static int32 render_option;
          static int32 ok;
          static uint8 *rt_data;
          static int32 rt_w,rt_h,rt_pre_x,rt_post_x;
          render_option=1;
          if (rt_data_last) free(rt_data_last);
          ok=FontRenderTextUTF32(font[f],&chr_utf32,1,render_option,
                     &rt_data,&rt_w,&rt_h,&rt_pre_x,&rt_post_x);
          rt_data_last=rt_data;
          cp2=rt_data;
          f_pitch=0;

        }else{//default font
          f_pitch=0;
          if (f==8) cp2=&charset8x8[chr][0][0];
          if (f==14) cp2=&charset8x16[chr][1][0];
          if (f==16) cp2=&charset8x16[chr][0][0];
          if (f==(8+1)) {cp2=&charset8x8[chr][0][0]; z2=1;}
          if (f==(14+1)) {cp2=&charset8x16[chr][1][0]; z2=1;}
          if (f==(16+1)) {cp2=&charset8x16[chr][0][0]; z2=1;}
        }
        c=col&0xF;//foreground col
        if (H3C0_blink_enable) {
            c2=(col>>4)&7;//background col
            c3=col>>7;//flashing?
        } else {
            c2=(col>>4);//background col
        }
        if (c3&&show_flashing && H3C0_blink_enable) c=c2;
        i2=paldata[c];
        i3=paldata[c2];
        lp=display_surface_offset+qbg_y_offset+y2*x_monitor+x2;
        z=x_monitor-fontwidth[display_page->font];

        //inner loop
        for (y3=0;y3<f_height;y3++){
          for (x3=0;x3<f_width;x3++){
        if (*cp2) *lp=i2; else *lp=i3;
        if (z2){
          if (x3&z2) cp2++;
        }else{
          cp2++;
        }
        lp++;
          }
          lp+=z;
          cp2+=f_pitch;
        }//y3,x3

        //draw cursor
        if (display_page->cursor_show&&show_cursor&&(cx==x)&&(cy==y)){
          static int32 v1,v2;
          static uint8 from_bottom;//bottom is the 2nd bottom scanline in width ?x25
          static uint8 half_cursor;//if set, overrides all following values
          static uint8 size;//if 0, no cursor is drawn, if 255, from begin to bottom
          static uint8 begin;//only relevant if from_bottom was not specified
          v1=display_page->cursor_firstvalue;
          v2=display_page->cursor_lastvalue;
          from_bottom=0;
          half_cursor=0;
          size=0;
          begin=0;
          //RULE: IF V2=0, NOTHING (UNLESS V1=0)
          if (v2==0){
        if (v1==0){size=1; goto cursor_created;}
        goto nocursor;//no cursor!
          }
          //RULE: IF V2<V1, FROM V2 TO BOTTOM
          if (v2<v1){begin=v2; size=255; goto cursor_created;}
          //RULE: IF V1=V2, SINGLE SCANLINE AT V1 (OR BOTTOM IF V1>=4)
          if (v1==v2){
        if (v1<=3){begin=v1; size=1; goto cursor_created;}
        from_bottom=1; size=1; goto cursor_created;
          }
          //NOTE: V2 MUST BE LARGER THAN V1!
          //RULE: IF V1>=3, CALC. DIFFERENCE BETWEEN V1 & V2
          //                IF DIFF=1, 2 SCANLINES AT BOTTOM
          //                IF DIFF=2, 3 SCANLINES AT BOTTOM
          //                OTHERWISE HALF CURSOR
          if (v1>=3){
        if ((v2-v1)==1){from_bottom=1; size=2; goto cursor_created;}
        if ((v2-v1)==2){from_bottom=1; size=3; goto cursor_created;}
        half_cursor=1; goto cursor_created;
          }
          //RULE: IF V1<=1, IF V2<=3 FROM V1 TO V3 ELSE FROM V1 TO BOTTOM
          if (v1<=1){
        if (v2<=3){begin=v1;size=v2-v1+1; goto cursor_created;} 
        begin=v1;size=255; goto cursor_created;
          }
          //RULE: IF V1=2, IF V2=3, 2 TO 3
          //               IF V2=4, 3 SCANLINES AT BOTTOM
          //               IF V2>=5, FROM 2 TO BOTTOM
          //(assume V1=2)
          if (v2==3){begin=2;size=2; goto cursor_created;}
          if (v2==4){from_bottom=1; size=3; goto cursor_created;}
          begin=2;size=255;
        cursor_created:
          static int32 cw,ch;
          cw=fontwidth[display_page->font]; ch=fontheight[display_page->font];
          if (half_cursor){
        //half cursor
        y3=ch-1;
        size=ch/2;
        c=col&0xF;//foreground col
        i2=paldata[c];
          draw_half_curs:
        lp=display_surface_offset+qbg_y_offset+(y2+y3)*x_monitor+x2;
        for (x3=0;x3<cw;x3++){
          *lp=i2;
          lp++;
        }
        y3--;
        size--;
        if (size) goto draw_half_curs;
          }else{
        if (from_bottom){
          //cursor from bottom
          y3=ch-1;
          if (y3==15) y3=14;//leave bottom line blank in 8x16 char set
          c=col&0xF;//foreground col
          i2=paldata[c];
        draw_curs_from_bottom:
          lp=display_surface_offset+qbg_y_offset+(y2+y3)*x_monitor+x2;
          for (x3=0;x3<cw;x3++){
            *lp=i2;
            lp++;
          }
          y3--;
          size--;
          if (size) goto draw_curs_from_bottom;
        }else{
          //cursor from begin using size
          if (begin<ch){
            y3=begin;
            c=col&0xF;//foreground col
            i2=paldata[c];
            if (size==255) size=ch-begin;
          draw_curs_from_begin:
            lp=display_surface_offset+qbg_y_offset+(y2+y3)*x_monitor+x2;
            for (x3=0;x3<cw;x3++){
              *lp=i2;
              lp++;
            }
            y3++;
            size--;
            if (size) goto draw_curs_from_begin;
          }
        }
          }
        }//draw cursor?
      nocursor:

        //outer loop
      skip:
        x2=x2+fontwidth[display_page->font];
      }
      y2=y2+fontheight[display_page->font];

    }

    show_flashing_last=show_flashing;
    show_cursor_last=show_cursor;
    cx_last=cx;
    cy_last=cy;
    screen_last_valid=1;

    if (BGRA_to_RGBA) swap_paldata_BGRA_with_RGBA();

    /*
    //backup for reuse in next frame
    i=display_frame[frame_i].w*display_frame[frame_i].h*4;
    if (i!=pixeldatasize){
    free(pixeldata);
    pixeldata=(uint8*)malloc(i);
    pixeldatasize=i;
    }
    memcpy(pixeldata,display_frame[frame_i].bgra,i);
    */

    last_frame_i=frame_i;

    goto screen_refreshed;

      }//text























      if (display_page->bits_per_pixel==32){

    //note: as software->hardware should be avoided at all costs, pixeldata is
    //      still backed up for comparison purposes because in the very likely
    //      event the data has not changed there is no point generating a 
    //      new hardware surface from the software frame when the old hardware surface 
    //      can be reused. It also saves on BGRA->RGBA conversion on some platforms.

    if (!BGRA_to_RGBA){
      //find the most recently published page to compare with
      //(the most recent READY or DISPLAYING page)
      static int64 highest_order;
      highest_order=0;
      i2=-1;
      for (i3=0;i3<=2;i3++){
        if ((display_frame[i3].state==DISPLAY_FRAME_STATE__DISPLAYING||
         display_frame[i3].state==DISPLAY_FRAME_STATE__READY)
        &&display_frame[i3].order>highest_order){
          highest_order=display_frame[i3].order;
          i2=i3;
        }
      } 
      if (force_display_update) goto update_display32b; //force update
      if (i2!=-1){  
        if (!screen_last_valid) goto update_display32b; //force update because of mode change?
        i=display_page->width*display_page->height*4;
        if (i!=(display_frame[i2].w*display_frame[i2].h*4)) goto update_display32b;

        if (displayorder_screen==0){
          //a valid frame of the correct dimensions exists and we are not required to display software content
          goto no_new_frame;
        }
        
        if (memcmp(display_frame[i2].bgra,display_page->offset,i)) goto update_display32b;
        goto no_new_frame;//no need to update display
      }
    update_display32b:;
    }else{
 
     //BGRA_to_RGBA
      i=display_page->width*display_page->height*4;
      if (i!=pixeldatasize){
        free(pixeldata);
        pixeldata=(uint8*)malloc(i);
        pixeldatasize=i;
        goto update_display32;
      }
      if (force_display_update) goto update_display32; //force update

      if (displayorder_screen==0){
        //a valid frame of the correct dimensions exists and we are not required to display software content
        goto no_new_frame;
      }

      if (memcmp(pixeldata,display_page->offset,i)) goto update_display32;
      if (!screen_last_valid) goto update_display32; //force update because of mode change?
      goto no_new_frame;//no need to update display
    update_display32:
      memcpy(pixeldata,display_page->offset,i);
    }


    //################################ Setup new frame ################################
    {
      static int32 new_size_bytes;
      new_size_bytes=x_monitor*y_monitor*4;
      if (new_size_bytes>display_frame[frame_i].bytes){
        free(display_frame[frame_i].bgra);
        display_frame[frame_i].bgra=(uint32*)malloc(new_size_bytes);
        display_frame[frame_i].bytes=new_size_bytes;
      }
      display_frame[frame_i].w=x_monitor; display_frame[frame_i].h=y_monitor;
    }

    if (!BGRA_to_RGBA){
      memcpy(display_frame[frame_i].bgra,display_page->offset,display_frame[frame_i].w*display_frame[frame_i].h*4);
    }else{
      static uint32 col;
      static uint32 *src_pos;
      static uint32 *dst_pos;
      src_pos=(uint32*)pixeldata;
      dst_pos=display_frame[frame_i].bgra;
      static int32 pixels;
      pixels=display_frame[frame_i].w*display_frame[frame_i].h;
      if (pixels>0){
        while(pixels--){
          col=*src_pos++;
          *dst_pos++= (col&0xFF00FF00) | ((col & 0xFF0000) >> 16) | ((col & 0x0000FF) << 16);
        }
      }
    }

    goto screen_refreshed;
      }//32














      //assume <=256 colors using palette

      if (display_page->compatible_mode==10){//update SCREEN 10 palette
    i2=GetTicks()&512;
    if (i2) i2=1;
    for (i=0;i<=3;i++){
      display_page->pal[i]=pal_mode10[i2][display_page->pal[i+4]];//pal_mode10[0-1][0-8]
    }
      }

      i=display_page->width*display_page->height;
      i2=1<<display_page->bits_per_pixel;//unique colors

      //data changed?
      if (i!=pixeldatasize){
            free(pixeldata);
            pixeldata=(uint8*)malloc(i);
            pixeldatasize=i;
            goto update_display;
      }

      if (force_display_update) goto update_display; //force update

      if (displayorder_screen==0){
        //a valid frame of the correct dimensions exists and we are not required to display software content
        goto no_new_frame;
      }

      if (memcmp(pixeldata,display_page->offset,i)) goto update_display;
      //palette changed?
      if (memcmp(paldata,display_page->pal,i2*4)) goto update_display;
      //force update because of mode change?
      if (!screen_last_valid) goto update_display;

      goto no_new_frame;//no need to update display

    update_display:


      //################################ Setup new frame ################################
      {
    static int32 new_size_bytes;
    new_size_bytes=x_monitor*y_monitor*4;
    if (new_size_bytes>display_frame[frame_i].bytes){
      free(display_frame[frame_i].bgra);
      display_frame[frame_i].bgra=(uint32*)malloc(new_size_bytes);
      display_frame[frame_i].bytes=new_size_bytes;
    }
    display_frame[frame_i].w=x_monitor; display_frame[frame_i].h=y_monitor;
      }

      display_surface_offset=display_frame[frame_i].bgra;

      memcpy(pixeldata,display_page->offset,i);
      memcpy(paldata,display_page->pal,i2*4);

      if (BGRA_to_RGBA) swap_paldata_BGRA_with_RGBA();
      static uint8 *cp;
      static uint32 *lp2;
      static uint32 c;
      cp=pixeldata;
      lp2=display_surface_offset;
      x2=display_page->width;
      y2=display_page->height;
      for (y=0;y<y2;y++){
    for (x=0;x<x2;x++){
      *lp2++=paldata[*cp++];
    }//x
      }//y
      if (BGRA_to_RGBA) swap_paldata_BGRA_with_RGBA();

      goto screen_refreshed;








    screen_refreshed:

      force_display_update=0;

      screen_last_valid=1;

      //Set new display frame as ready
      //display_frame_end=frame_i;
      //if (!display_frame_begin) display_frame_begin=frame_i;

      display_frame[frame_i].state=DISPLAY_FRAME_STATE__READY; last_hardware_display_frame_order=display_frame[frame_i].order;


      if (cloud_app){
    if (cloud_chdir_complete){

#ifdef QB64_WINDOWS
      /*
        static FILE *cloud_screenshot_file_handle=NULL;
        if (cloud_screenshot_file_handle==NULL) cloud_screenshot_file_handle=fopen("output_image.raw","w+b");
        fseek ( cloud_screenshot_file_handle , 0 , SEEK_SET );//reset file pointer to beginning of file
        static int32 w,h;
        w=display_frame[frame_i].w;
        h=display_frame[frame_i].h;
        static int32 wh[2];
        wh[0]=w;
        wh[1]=h;
        fwrite (&wh[0] , 8, 1, cloud_screenshot_file_handle);
        fwrite (display_frame[frame_i].bgra , w*h*4, 1, cloud_screenshot_file_handle);
        fflush(cloud_screenshot_file_handle);
      */

      static HANDLE cloud_screenshot_file_handle=NULL;
      if (cloud_screenshot_file_handle==NULL) cloud_screenshot_file_handle=CreateFile("output_image.raw", GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, 
                                              /*FILE_ATTRIBUTE_NORMAL*/FILE_FLAG_WRITE_THROUGH, NULL);


      //fseek ( cloud_screenshot_file_handle , 0 , SEEK_SET );//reset file pointer to beginning of file


      static int32 w,h,index=0;
      w=display_frame[frame_i].w;
      h=display_frame[frame_i].h;
      index++;
      static int32 header[3];
      header[0]=index;
      header[1]=w;
      header[2]=h;

      SetFilePointer(cloud_screenshot_file_handle,12,0,FILE_BEGIN);
      WriteFile(cloud_screenshot_file_handle, display_frame[frame_i].bgra, w*h*4, NULL, NULL);
      FlushFileBuffers(cloud_screenshot_file_handle);
      SetFilePointer(cloud_screenshot_file_handle,0,0,FILE_BEGIN);
      WriteFile(cloud_screenshot_file_handle, &header[0], 12, NULL, NULL);


      //CloseHandle(cloud_screenshot_file_handle);

#endif
    }
      }

    no_new_frame:;
    display_page_invalid:;
    cloud_skip_frame:


      //cancel frame if not built
      if (display_frame[frame_i].state==DISPLAY_FRAME_STATE__BUILDING){
    last_hardware_display_frame_order=display_frame[frame_i].order;
    display_frame[frame_i].state=DISPLAY_FRAME_STATE__EMPTY;
      }



    }//lock_display==0
    if (lock_display==1){lock_display=2; Sleep(0);}
    if (autodisplay==-1) autodisplay=0;
    display_called=1; 
    return;
  }





  /*
    int message_loop(
    SDL_Event event,
    SDL_Surface *screen,
    SDL_Surface *back,
    int *inputedWidth,
    Uint16 *inputedString,
    TTF_Font *font)
    {
    SDL_Color fg = {0x66, 0x66, 0xFF};
    SDL_Color bg = {0x00, 0x00, 0x00};
    SDL_Rect rect;
    SDL_Surface *surface;
    SDL_Event eventExpose;
    
    switch(event.type){
    case SDL_QUIT:
    return 1;
    case SDL_VIDEOEXPOSE:
    SDL_BlitSurface(back, NULL, screen, NULL);
    SDL_UpdateRect(screen, 0, 0, 0, 0);
    break;
    case SDL_KEYDOWN:


            


    if (event.key.keysym.sym == QBVK_F1) {
    InputMethod_Reset();
    }
    if (event.key.keysym.sym == QBVK_F2) {
    *inputedWidth = 0;
    inputedString[0] = 0x0000;
    InputMethod_Reset();
    }
    if (event.key.keysym.sym == QBVK_F3) {
    InputMethod_Validate();
    rect.x = 0;
    rect.y = 200;
    rect.w = 640;
    rect.h = 100;
    SDL_FillRect(back, &rect, 0x00000000);
    surface = TTF_RenderUTF8_Shaded(
    font, "Valid", fg, bg);
    SDL_BlitSurface(surface, NULL, back, &rect);
    SDL_FreeSurface(surface);
    eventExpose.type = SDL_VIDEOEXPOSE;
    SDL_PushEvent(&eventExpose);
    }
    if (event.key.keysym.sym == QBVK_F4) {
    InputMethod_Invalidate();
    rect.x = 0;
    rect.y = 200;
    rect.w = 640;
    rect.h = 100;
    SDL_FillRect(back, &rect, 0x00000000);
    surface = TTF_RenderUTF8_Shaded(
    font, "Invalid", fg, bg);
    SDL_BlitSurface(surface, NULL, back, &rect);
    SDL_FreeSurface(surface);
    eventExpose.type = SDL_VIDEOEXPOSE;
    SDL_PushEvent(&eventExpose);
    }
    break;
    default:
    break;
    }
    return 0;
    }
  */

  void update_shift_state(){
    int32 x;
    /*
      0:417h                   Shift Status
      7 6 5 4 3 2 1 0
      x . . . . . . .      Insert locked
      . x . . . . . .      Caps Lock locked
      . . x . . . . .      Num Lock locked
      . . . x . . . .      Scroll Lock locked
      . . . . x . . .      Alt key is pressed
      . . . . . x . .      Ctrl key is pressed
      . . . . . . x .      Left Shift key is pressed
      . . . . . . . x      Right Shift key is pressed
    */
    x=0;
    if (keyheld(VK+QBVK_RSHIFT)) x|=1;
    if (keyheld(VK+QBVK_LSHIFT)) x|=2;
    if (keyheld(VK+QBVK_LCTRL)||keyheld(VK+QBVK_RCTRL)) x|=4;
    if (keyheld(VK+QBVK_LALT)||keyheld(VK+QBVK_RALT)) x|=8;
    if (keyheld(QBK+QBK_SCROLL_LOCK_MODE)) x|=16;
    if (keyheld(VK+QBVK_NUMLOCK)) x|=32;
    if (keyheld(VK+QBVK_CAPSLOCK)) x|=64;
    //note: insert state is emulated (off by default)
    if (keyheld(QBK+QBK_INSERT_MODE)) x|=128;
    cmem[0x417]=x;
    /*
      0:418h                   Extended Shift Status
      (interpret the word 'pressed' as "being held down")
      7 6 5 4 3 2 1 0
      x . . . . . . .      Ins key is pressed
      . x . . . . . .      Caps Lock key is pressed (detection not possible, return 0)
      . . x . . . . .      Num Lock key is pressed (detection not possible, return 0)
      . . . x . . . .      Scroll Lock key is pressed
      . . . . x . . .      Pause key locked
      . . . . . x . .      SysReq key is pressed
      . . . . . . x .      Left Alt key is pressed
      . . . . . . . x      Left Ctrl key is pressed
    */
    x=0;
    if (keyheld(VK+QBVK_LCTRL)) x|=1;
    if (keyheld(VK+QBVK_LALT)) x|=2;
    if (keyheld(VK+QBVK_SYSREQ)) x|=4;
    if (keyheld(VK+QBVK_PAUSE)) x|=8;
    if (keyheld(VK+QBVK_SCROLLOCK)) x|=16;
    //if (keyheld(VK+QBVK_NUMLOCK)) x|=32;
    //if (keyheld(VK+QBVK_CAPSLOCK)) x|=64;
    if (keyheld(0x5200)) x|=128;
    cmem[0x418]=x;
    /*
      0:496h                   Keyboard Status and Type Flags
      This byte holds keyboard status information.
      Keyboard Status Information
      7 6 5 4 3 2 1 0
      x . . . . . . .       Read ID in progress (always 0)
      . x . . . . . .       Last character was first ID character (always 0)
      . . x . . . . .       Force Num Lock if read ID and KBX (always 0)
      . . . x . . . .       101/102-key keyboard installed (always 1)
      . . . . x . . .       Right Alt key is pressed
      . . . . . x . .       Right Ctrl key is pressed
      . . . . . . x .       Last code was E0 Hidden Code (always 0)
      . . . . . . . x       last code was E1 Hidden Code (always 0)
    */
    x=0;
    if (keyheld(VK+QBVK_RCTRL)) x|=1;
    if (keyheld(VK+QBVK_RALT)) x|=2;
    x|=16;
    cmem[0x496]=x;
  }


  int32 keyup_mask_last=-1;
  uint32 keyup_mask[256];//NULL values indicate removed masks

  void keyup(uint32 x){

    if (!x) x=QBK+QBK_CHR0;

    keyheld_remove(x);

    if (asciicode_reading!=2){//hide numpad presses related to ALT+1+2+3 type entries
      //identify and revert numpad specific key codes to non-numpad codes
      static uint32 x2;
      static int64 numpadkey;
      numpadkey=0;
      x2=x;
      //check multimapped NUMPAD keys
      if ((x>=(VK+QBVK_KP0))&&(x<=(VK+QBVK_KP_ENTER))){
    numpadkey=4294967296ll;
    if ((x>=(VK+QBVK_KP0))&&(x<=(VK+QBVK_KP9))){x2=x-(VK+QBVK_KP0)+48; goto onnumpad;}
    if (x==(VK+QBVK_KP_PERIOD)){x2=46; goto onnumpad;}
    if (x==(VK+QBVK_KP_DIVIDE)){x2=47; goto onnumpad;}
    if (x==(VK+QBVK_KP_MULTIPLY)){x2=42; goto onnumpad;}
    if (x==(VK+QBVK_KP_MINUS)){x2=45; goto onnumpad;}
    if (x==(VK+QBVK_KP_PLUS)){x2=43; goto onnumpad;}
    if (x==(VK+QBVK_KP_ENTER)){x2=13; goto onnumpad;}
      }
      if ((x>=(QBK+0))&&(x<=(QBK+10))){
    numpadkey=4294967296ll;
    x2=x-QBK;
    if (x2==0){x2=82<<8; goto onnumpad;}
    if (x2==1){x2=79<<8; goto onnumpad;}
    if (x2==2){x2=80<<8; goto onnumpad;}
    if (x2==3){x2=81<<8; goto onnumpad;}
    if (x2==4){x2=75<<8; goto onnumpad;}
    if (x2==5){x2=76<<8; goto onnumpad;}
    if (x2==6){x2=77<<8; goto onnumpad;}
    if (x2==7){x2=71<<8; goto onnumpad;}
    if (x2==8){x2=72<<8; goto onnumpad;}
    if (x2==9){x2=73<<8; goto onnumpad;}
    if (x2==10){x2=83<<8; goto onnumpad;}
      }
    onnumpad:;

      static int32 i;
      for (i=0;i<=keyup_mask_last;i++){
    if (x==keyup_mask[i]){
      keyup_mask[i]=0;
      goto key_handled;
    }
      }

      //add x2 to keyhit buffer
      static int32 z;
      z=(keyhit_nextfree+1)&0x1FFF;
      if (z==keyhit_next){//remove oldest message when cyclic buffer is full
    keyhit_next=(keyhit_next+1)&0x1FFF;
      }
      static int32 sx;
      sx=x2; sx=-sx; x2=sx;//negate x2
      keyhit[keyhit_nextfree]=x2|numpadkey;
      keyhit_nextfree=z;
    }//asciicode_reading!=2


    static int32 shift,alt,ctrl,capslock,numlock;
    numlock=0; capslock=0;

    if (x<=255){
      if (scancode_lookup[x*10+2]) scancodeup(scancode_lookup[x*10+1]);
      goto key_handled;
    }//x<=255

    //NUMPAD?
    if ((x>=(VK+QBVK_KP0))&&(x<=(VK+QBVK_KP_ENTER))){
      if ((x>=(VK+QBVK_KP0))&&(x<=(VK+QBVK_KP_PERIOD))) numlock=1;
      x=(x-(VK+QBVK_KP0)+256)*256;
      goto numpadkey;
    }
    if ((x>=(QBK+0))&&(x<=(QBK+0+(QBVK_KP_PERIOD-QBVK_KP0)))){
      x=(x-(QBK+0)+256)*256;
      goto numpadkey;
    }

    if (x<=65535){
      static int32 r;
    numpadkey:
      r=(x>>8)+256;
      if (scancode_lookup[r*10+2]) scancodeup(scancode_lookup[r*10+1]);

      if (x==0x5200){//INSERT lock emulation
    update_shift_state();
      }

      goto key_handled;
    }//x<=65536

    if (x==(VK+QBVK_LSHIFT)){
      scancodeup(42);
      update_shift_state();
    }
    if (x==(VK+QBVK_RSHIFT)){
      scancodeup(54);
      update_shift_state();
    }
    if (x==(VK+QBVK_LALT)){
      scancodeup(56);
      update_shift_state();
    }
    if (x==(VK+QBVK_RALT)){
      scancodeup(56);
      update_shift_state();
    }
    if (x==(VK+QBVK_LCTRL)){
      scancodeup(29);
      update_shift_state();
    }
    if (x==(VK+QBVK_RCTRL)){
      scancodeup(29);
      update_shift_state();
    }
    if (x==(VK+QBVK_NUMLOCK)){
      scancodeup(69);
      update_shift_state();
    }
    if (x==(VK+QBVK_CAPSLOCK)){
      scancodeup(58);
      update_shift_state();
    }
    if (x==(VK+QBVK_SCROLLOCK)){
      scancodeup(70);
      update_shift_state();
    }

  key_handled:;

  }

  void keydown(uint32 x){

    if (!x) x=QBK+QBK_CHR0;

    static int32 glyph;
    glyph=keydown_glyph; keydown_glyph=0;

    //INSERT lock emulation
    static int32 insert_held;
    if (x==0x5200) insert_held=keyheld(0x5200);

    //SCROLL lock tracking
    static int32 scroll_lock_held;
    if (x==(VK+QBVK_SCROLLOCK)) scroll_lock_held=keyheld(VK+QBVK_SCROLLOCK);

    keyheld_add(x);

    //note: On early keyboards without a Pause key (before the introduction of 101-key keyboards) the Pause function was assigned to Ctrl+NumLock, and the Break function to Ctrl+ScrLock; these key-combinations still work with most programs, even on modern PCs with modern keyboards.
    //CTRL+BREAK handling
    if (
    (x==(VK+QBVK_BREAK))
    || ((x==(VK+QBVK_SCROLLOCK))&&(keyheld(VK+QBVK_LCTRL)||keyheld(VK+QBVK_RCTRL))) 
    || ((x==(VK+QBVK_F15))&&(keyheld(VK+QBVK_LCTRL)||keyheld(VK+QBVK_RCTRL))) 
    ){
      if (exit_blocked){exit_value|=2; goto key_handled;}
      close_program=1;
      goto key_handled;
    }

    #ifdef QB64_WINDOWS
        //note: Alt+F4 is supposed to close the window, but glut windows don't seem to be affected;
        //this addresses the issue:
        if ( (x==(0x3E00)) && (keyheld(VK+QBVK_RALT)||keyheld(VK+QBVK_LALT)) ){
          if (exit_blocked){exit_value|=1; goto key_handled;}
          close_program=1;
          goto key_handled;
        }
    #endif    

    //note: On early keyboards without a Pause key (before the introduction of 101-key keyboards) the Pause function was assigned to Ctrl+NumLock, and the Break function to Ctrl+ScrLock; these key-combinations still work with most programs, even on modern PCs with modern keyboards.
    //PAUSE handling
    if ( (x==(VK+QBVK_PAUSE)) || ((x==(VK+QBVK_NUMLOCK))&&(keyheld(VK+QBVK_LCTRL)||keyheld(VK+QBVK_RCTRL))) ){
      suspend_program|=1;
      qbevent=1;
      goto key_handled;
    }else{
      if (suspend_program&1){
    suspend_program^=1;
    goto key_handled;
      }
    }

    //ALT+ENTER
    if (keyheld(VK+QBVK_RALT)||keyheld(VK+QBVK_LALT)){
      if (x==13){
    static int32 fs_mode,fs_smooth;
    fs_mode=full_screen_set;
    if (fs_mode==-1) fs_mode=full_screen;
    fs_smooth=fullscreen_smooth;
    if (fs_mode==2&&fs_smooth==1){
      fs_mode=0;
    }else{
      if (fs_smooth==0&&fs_mode!=0){
        fullscreen_smooth=1;
      }else{
        fs_mode++;
        fullscreen_smooth=0;   
      }
    } 
    if (full_screen!=fs_mode) full_screen_set=fs_mode;
    force_display_update=1;
    goto key_handled;
      }
    }

    if (asciicode_reading!=2){//hide numpad presses related to ALT+1+2+3 type entries

      //identify and revert numpad specific key codes to non-numpad codes
      static uint32 x2;
      static int64 numpadkey;
      numpadkey=0;
      x2=x;
      //check multimapped NUMPAD keys
      if ((x>=(VK+QBVK_KP0))&&(x<=(VK+QBVK_KP_ENTER))){
    numpadkey=4294967296ll;
    if ((x>=(VK+QBVK_KP0))&&(x<=(VK+QBVK_KP9))){x2=x-(VK+QBVK_KP0)+48; goto onnumpad;}
    if (x==(VK+QBVK_KP_PERIOD)){x2=46; goto onnumpad;}
    if (x==(VK+QBVK_KP_DIVIDE)){x2=47; goto onnumpad;}
    if (x==(VK+QBVK_KP_MULTIPLY)){x2=42; goto onnumpad;}
    if (x==(VK+QBVK_KP_MINUS)){x2=45; goto onnumpad;}
    if (x==(VK+QBVK_KP_PLUS)){x2=43; goto onnumpad;}
    if (x==(VK+QBVK_KP_ENTER)){x2=13; goto onnumpad;}
      }
      if ((x>=(QBK+0))&&(x<=(QBK+10))){
    numpadkey=4294967296ll;
    x2=x-QBK;
    if (x2==0){x2=82<<8; goto onnumpad;}
    if (x2==1){x2=79<<8; goto onnumpad;}
    if (x2==2){x2=80<<8; goto onnumpad;}
    if (x2==3){x2=81<<8; goto onnumpad;}
    if (x2==4){x2=75<<8; goto onnumpad;}
    if (x2==5){x2=76<<8; goto onnumpad;}
    if (x2==6){x2=77<<8; goto onnumpad;}
    if (x2==7){x2=71<<8; goto onnumpad;}
    if (x2==8){x2=72<<8; goto onnumpad;}
    if (x2==9){x2=73<<8; goto onnumpad;}
    if (x2==10){x2=83<<8; goto onnumpad;}
      }
    onnumpad:;





      //ON KEY trapping
      {//new scope
    static int32 block_onkey=0;
    static int32 f,x3,scancode,extended,c,flags_mask;
    int32 i,i2;//must not be static!

    //establish scancode (if any)
    scancode=0;
    if (x<=255){scancode=scancode_lookup[x*10+1]; goto onkey_gotscancode;}
    //*check for 2 byte scancodes here
    x3=x;
    if ((x3>=(VK+QBVK_KP0))&&(x3<=(VK+QBVK_KP_ENTER))){
      x3=(x3-(VK+QBVK_KP0)+256)*256;
      goto onkey_numpadkey;
    }
    if ((x3>=(QBK+0))&&(x3<=(QBK+0+(QBVK_KP_PERIOD-QBVK_KP0)))){
      x3=(x3-(QBK+0)+256)*256;
      goto onkey_numpadkey;
    }
    if (x3<=65535){
    onkey_numpadkey:
      i=(x3>>8)+256;
      if (scancode_lookup[i*10+2]) scancode=scancode_lookup[i*10+1];
    }
      onkey_gotscancode:

    //check modifier keys
    if (x==(VK+QBVK_LSHIFT)){
      scancode=42;
      flags_mask=3;
    }
    if (x==(VK+QBVK_RSHIFT)){
      scancode=54;
      flags_mask=3;
    }
    if (x==(VK+QBVK_LALT)){
      scancode=56;
      flags_mask=8;
    }
    if (x==(VK+QBVK_RALT)){
      scancode=56;
      flags_mask=8;
    }
    if (x==(VK+QBVK_LCTRL)){
      scancode=29;
      flags_mask=4;
    }
    if (x==(VK+QBVK_RCTRL)){
      scancode=29;
      flags_mask=4;
    }
    if (x==(VK+QBVK_NUMLOCK)){
      scancode=69;
      flags_mask=32;
    }
    if (x==(VK+QBVK_CAPSLOCK)){
      scancode=58;
      flags_mask=64;
    }
    if (x==(VK+QBVK_SCROLLOCK)){
      scancode=70;
      //note: no mask required
    }

    //establish if key is an extended key
    extended=0;
    //arrow-pad (note: num-pad is ignored because x is a QB64 pure key value and only refers to the arrow-pad)
    if (x==0x4B00)extended=1;
    if (x==0x4800)extended=1;
    if (x==0x4D00)extended=1;
    if (x==0x5000)extended=1;
    //num-pad extended keys
    if (x==VK+QBVK_KP_DIVIDE)extended=1;
    if (x==VK+QBVK_KP_ENTER)extended=1;
    //ins/del/hom/end/pgu/pgd pad
    if (x==0x5200)extended=1;
    if (x==0x4700)extended=1;
    if (x==0x4900)extended=1;
    if (x==0x5300)extended=1;
    if (x==0x4F00)extended=1;
    if (x==0x5100)extended=1;
    //right alt/right control
    if (x==VK+QBVK_RCTRL)extended=1;
    if (x==VK+QBVK_RALT)extended=1;

    if (!block_onkey){

      //priority #1: user defined keys
      if (scancode){
        for (i=0;i<=31;i++){
          if (onkey[i].key_scancode==scancode){
        if (onkey[i].active){
          if (onkey[i].id){
            //check keyboard flags
            f=onkey[i].key_flags;
            //0 No keyboard flag, 1-3 Either Shift key, 4 Ctrl key, 8 Alt key,32 NumLock key,64 Caps Lock key, 128 Extended keys on a 101-key keyboard
            //To specify multiple shift states, add the values together. For example, a value of 12 specifies that the user-defined key is used in combination with the Ctrl and Alt keys.
            if ((flags_mask&3)==0){
              if (f&3){
            if (keyheld(VK+QBVK_LSHIFT)==0&&keyheld(VK+QBVK_RSHIFT)==0) goto wrong_flags;
              }else{
            if (keyheld(VK+QBVK_LSHIFT)||keyheld(VK+QBVK_RSHIFT)) goto wrong_flags;
              }
            }
            if ((flags_mask&4)==0){
              if (f&4){
            if (keyheld(VK+QBVK_LCTRL)==0&&keyheld(VK+QBVK_RCTRL)==0) goto wrong_flags;
              }else{
            if (keyheld(VK+QBVK_LCTRL)||keyheld(VK+QBVK_RCTRL)) goto wrong_flags;
              }
            }
            if ((flags_mask&8)==0){
              if (f&8){
            if (keyheld(VK+QBVK_LALT)==0&&keyheld(VK+QBVK_RALT)==0) goto wrong_flags;
              }else{
            if (keyheld(VK+QBVK_LALT)||keyheld(VK+QBVK_RALT)) goto wrong_flags;
              }
            }
            if ((flags_mask&32)==0){
              if (f&32){
            if (keyheld(VK+QBVK_NUMLOCK)==0) goto wrong_flags;
            //*revise
              }
            }
            if ((flags_mask&64)==0){
              if (f&64){
            if (keyheld(VK+QBVK_CAPSLOCK)==0) goto wrong_flags;
            //*revise
              }
            }
            if ((flags_mask&128)==0){
              if (((f&128)/128)!=extended) goto wrong_flags;
            }
            if (onkey[i].active==1){//(1)ON
              onkey[i].state++;
            }else{//(2)STOP
              onkey[i].state=1;
            }

            qbevent=1;

            //mask trigger key
            for (i=0;i<=keyup_mask_last;i++){
              if (!keyup_mask[i]){
            keyup_mask[i]=x;  
            break;
              }
            }
            if (i==keyup_mask_last+1){
              if (keyup_mask_last<255){
            keyup_mask[i]=x;
            keyup_mask_last++;
              }
            }

            goto key_handled;

          }//id
        }//active
          }//scancode==
        wrong_flags:;
        }//i
      }//scancode

      //priority #2: fixed index F1-F12, arrows
      for (i=0;i<=31;i++){
        if (onkey[i].active){
          if (onkey[i].id){
        if ((x2==onkey[i].keycode)||x==onkey[i].keycode_alternate){
          if (onkey[i].active==1){//(1)ON
            onkey[i].state++;
          }else{//(2)STOP
            onkey[i].state=1;
          }
          qbevent=1;

          //mask trigger key
          for (i=0;i<=keyup_mask_last;i++){
            if (!keyup_mask[i]){
              keyup_mask[i]=x;  
              break;
            }
          }
          if (i==keyup_mask_last+1){
            if (keyup_mask_last<255){
              keyup_mask[i]=x;
              keyup_mask_last++;
            }
          }

          goto key_handled;
        }//keycode
          }//id
        }//active
      }//i

    }//block_onkey

    //priority #3: string insertion
    for (i=0;i<=31;i++){
      if (onkey[i].text){
        if (onkey[i].text->len){
          if ((x2==onkey[i].keycode)||x==onkey[i].keycode_alternate){

        //mask trigger key
        {//scope
          static int32 i;
          for (i=0;i<=keyup_mask_last;i++){
            if (!keyup_mask[i]){
              keyup_mask[i]=x;  
              break;
            }
          }
          if (i==keyup_mask_last+1){
            if (keyup_mask_last<255){
              keyup_mask[i]=x;
              keyup_mask_last++;
            }
          }
        }//descope

        for (i2=0;i2<onkey[i].text->len;i2++){
          block_onkey=1;
          keydown_ascii(onkey[i].text->chr[i2]);
          keyup_ascii(onkey[i].text->chr[i2]);
          block_onkey=0;
        }//i2
        goto key_handled;
          }//keycode
        }}//text
    }//i

      }//descope


      /*
      //keyhit cyclic buffer
      int64 keyhit[8192];
      //    keyhit specific internal flags: (stored in high 32-bits)
      //    &4294967296->numpad was used
      int32 keyhit_nextfree=0;
      int32 keyhit_next=0;
      //note: if full, the oldest message is discarded to make way for the new message
      */
      //add x2 to keyhit buffer
      static int32 z;
      z=(keyhit_nextfree+1)&0x1FFF;
      if (z==keyhit_next){//remove oldest message when cyclic buffer is full
    keyhit_next=(keyhit_next+1)&0x1FFF;
      }
      keyhit[keyhit_nextfree]=x2|numpadkey;
      keyhit_nextfree=z;
    }//asciicode_reading!=2


    static int32 shift,alt,ctrl,capslock,numlock;
    numlock=0; capslock=0;

    if (x==QBK+QBK_CHR0) x=0;

    if (x<=255){
      static int32 b1,b2,z,o;
      b1=x;
      if (b2=scancode_lookup[x*10+1]){//table entry exists
    scancodedown(b2);







    //check for relevent table modifiers
    shift=0; if (keyheld(VK+QBVK_LSHIFT)||keyheld(VK+QBVK_RSHIFT)) shift=1;
    ctrl=0; if (keyheld(VK+QBVK_LCTRL)||keyheld(VK+QBVK_RCTRL)) ctrl=1;
    alt=0; if (keyheld(VK+QBVK_LALT)||keyheld(VK+QBVK_RALT)) alt=1;
    o=0;
    if (shift) o=1;
    if (ctrl) o=2;
    if (alt) o=3;
    if (glyph){
      if ((keyheld(VK+QBVK_LALT)==0)&&(keyheld(VK+QBVK_RCTRL)==0)&&keyheld(VK+QBVK_LCTRL)&&keyheld(VK+QBVK_RALT)) o=0;//assume alt-gr combo-key
    }
    z=scancode_lookup[x*10+2+o];
    if (!z) goto key_handled;//not possible
    if (z&0xFF00){
      b1=0;
      b2=z>>8;
    }else{
      b1=z;
    }
      }//b2
      static int32 i,i2,i3;
      i=cmem[0x41a];
      i2=cmem[0x41c];
      i3=i2+2;
      if (i3==62) i3=30;
      if (i!=i3){//fits in buffer
    cmem[0x400+i2]=b1;
    cmem[0x400+i2+1]=b2;//(scancode)
    cmem[0x41c]=i3;//fix tail location
      }
      goto key_handled;
    }//x<=255

    //NUMPAD?
    if ((x>=(VK+QBVK_KP0))&&(x<=(VK+QBVK_KP_ENTER))){
      if ((x>=(VK+QBVK_KP0))&&(x<=(VK+QBVK_KP_PERIOD))) numlock=1;
      x=(x-(VK+QBVK_KP0)+256)*256;
      goto numpadkey;
    }
    if ((x>=(QBK+0))&&(x<=(QBK+0+(QBVK_KP_PERIOD-QBVK_KP0)))){
      x=(x-(QBK+0)+256)*256;
      goto numpadkey;
    }

    if (x<=65535){
      static int32 b1,b2,z,o,r;
    numpadkey:
      b1=0; b2=x>>8;
      r=(x>>8)+256;
      if (scancode_lookup[r*10+2]){
    scancodedown(scancode_lookup[r*10+1]);
    //check relevent modifiers
    shift=0; if (keyheld(VK+QBVK_LSHIFT)||keyheld(VK+QBVK_RSHIFT)) shift=1;
    ctrl=0; if (keyheld(VK+QBVK_LCTRL)||keyheld(VK+QBVK_RCTRL)) ctrl=1;
    alt=0; if (keyheld(VK+QBVK_LALT)||keyheld(VK+QBVK_RALT)) alt=1;

    if (x==0x5200){//INSERT lock emulation
      if (insert_held==0){//nullify effects of key repeats
        if ((alt==0)&&(shift==0)&&(ctrl==0)){
          //toggle insert mode
          if (keyheld(QBK+QBK_INSERT_MODE)){
        keyheld_remove(QBK+QBK_INSERT_MODE);
          }else{
        keyheld_add(QBK+QBK_INSERT_MODE);
          }
          update_shift_state();
        }
      }
    }

    o=0;
    if (shift) o=1;
    if (numlock) o=4;
    if (numlock&&shift) o=7;
    if (ctrl) o=2;
    if (alt) o=3;
    z=scancode_lookup[r*10+2+o];
    if (!z) goto key_handled;//invalid combination
    if (z&0xFF00){
      b1=0;
      b2=z>>8;
    }else{
      b1=z;
      b2=scancode_lookup[r*10+1];
    }
      }//z
      static int32 i,i2,i3;
      i=cmem[0x41a];
      i2=cmem[0x41c];
      i3=i2+2;
      if (i3==62) i3=30;
      if (i!=i3){//fits in buffer
    cmem[0x400+i2]=b1;
    cmem[0x400+i2+1]=b2;//(scancode)
    cmem[0x41c]=i3;//fix tail location
      }
      goto key_handled;
    }//x<=65536

    if (x==(VK+QBVK_LSHIFT)){
      scancodedown(42);
      update_shift_state();
    }
    if (x==(VK+QBVK_RSHIFT)){
      scancodedown(54);
      update_shift_state();
    }
    if (x==(VK+QBVK_LALT)){
      scancodedown(56);
      update_shift_state();
    }
    if (x==(VK+QBVK_RALT)){
      scancodedown(56);
      update_shift_state();
    }
    if (x==(VK+QBVK_LCTRL)){
      scancodedown(29);
      update_shift_state();
    }
    if (x==(VK+QBVK_RCTRL)){
      scancodedown(29);
      update_shift_state();
    }
    if (x==(VK+QBVK_NUMLOCK)){
      scancodedown(69);
      update_shift_state();
    }
    if (x==(VK+QBVK_CAPSLOCK)){
      scancodedown(58);
      update_shift_state();
    }
    if (x==(VK+QBVK_SCROLLOCK)){
      scancodedown(70);

      if (scroll_lock_held==0){//nullify effects of key repeats
    ctrl=0; if (keyheld(VK+QBVK_LCTRL)||keyheld(VK+QBVK_RCTRL)) ctrl=1;
    if (ctrl==0){
      //toggle insert mode
      if (keyheld(QBK+QBK_SCROLL_LOCK_MODE)){
        keyheld_remove(QBK+QBK_SCROLL_LOCK_MODE);
      }else{
        keyheld_add(QBK+QBK_SCROLL_LOCK_MODE);
      }
    }
      }

      update_shift_state();
    }

  key_handled:
    sleep_break=1;

  }

  void scancodedown(uint8 scancode){
    if (port60h_events){
      if (port60h_event[port60h_events-1]==scancode) return;//avoid duplicate entries in buffer (eg. from key-repeats)
    }
    if (port60h_events==256){memmove(port60h_event,port60h_event+1,255); port60h_events=255;}
    port60h_event[port60h_events]=scancode;
    port60h_events++;
  }

  void scancodeup(uint8 scancode){
    if (port60h_events){
      if (port60h_event[port60h_events-1]==(scancode+128)) return;//avoid duplicate entries in buffer
    }
    if (port60h_events==256){memmove(port60h_event,port60h_event+1,255); port60h_events=255;}
    port60h_event[port60h_events]=scancode+128;
    port60h_events++;
  }

  #define OS_EVENT_PRE_PROCESSING 1
  #define OS_EVENT_POST_PROCESSING 2
  #define OS_EVENT_RETURN_IMMEDIATELY 3

  #ifdef QB64_WINDOWS
  extern "C" LRESULT qb64_os_event_windows(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, int *qb64_os_event_info){
    if (*qb64_os_event_info==OS_EVENT_PRE_PROCESSING){
        //example usage
        /*
        if (uMsg==WM_CLOSE){
          alert("goodbye");
          *qb64_os_event_info=OS_EVENT_RETURN_IMMEDIATELY;
        }
        */


        if (uMsg==WM_KEYDOWN){

        if (device_last){//core devices required?
        
        /*
        16-23        The scan code. The value depends on the OEM.
        24        Indicates whether the key is an extended key, such as the right-hand ALT and CTRL keys that appear on an enhanced 101- or 102-key keyboard. The value is 1 if it is an extended key; otherwise, it is 0.
        */

          static int32 code,special;
          special=0;//set to 2 for keys which we cannot detect a release for
          code=(lParam>>16)&511;          

        keydown_special:
        static device_struct *d;
        d=&devices[1];//keyboard
        if (getDeviceEventButtonValue(d,d->queued_events-1,code)!=1){//don't add message if already on

	  int32 eventIndex=createDeviceEvent(d);
	  setDeviceEventButtonValue(d,eventIndex,code,1);
          if (special==2){special=1; commitDeviceEvent(d); goto keydown_special;}//jump to ~5 lines above to add a 2nd key event
          if (special==1) setDeviceEventButtonValue(d,eventIndex,code,0);
          commitDeviceEvent(d);

        }//not 1          
        }//core devices required

}//WM_KEYDOWN


        if (uMsg==WM_KEYUP){

        if (device_last){//core devices required?
        
        /*
        16-23        The scan code. The value depends on the OEM.
        24        Indicates whether the key is an extended key, such as the right-hand ALT and CTRL keys that appear on an enhanced 101- or 102-key keyboard. The value is 1 if it is an extended key; otherwise, it is 0.
        */

          static int32 code;

          code=(lParam>>16)&511;          

        static device_struct *d;
        d=&devices[1];//keyboard
        if (getDeviceEventButtonValue(d,d->queued_events-1,code)!=0){//don't add message if already off

	  int32 eventIndex=createDeviceEvent(d);
	  setDeviceEventButtonValue(d,eventIndex,code,0);
          commitDeviceEvent(d);

        }//not 1          
        }//core devices required

}//WM_KEYUP















    }
    if (*qb64_os_event_info==OS_EVENT_POST_PROCESSING){



    }
    return 0;
  }
  #endif

#if defined(QB64_LINUX) && defined(QB64_GUI)
  extern "C" void qb64_os_event_linux(XEvent *event, Display *display, int *qb64_os_event_info){
    if (*qb64_os_event_info==OS_EVENT_PRE_PROCESSING){

        if (X11_display==NULL){
         X11_display=display;
         X11_window=event->xexpose.window;
        }

        x11filter(event);//handles clipboard request events from other applications
    }

    if (*qb64_os_event_info==OS_EVENT_POST_PROCESSING){
        switch (event->type) {
            case EnterNotify:
            window_focused = -1;
            break;

            case LeaveNotify:
            window_focused = 0;
            //Iterate over all modifiers
            for (uint32 key = VK + QBVK_RSHIFT; key <= VK + QBVK_MODE; key++) {
                if (keyheld(key)) keyup(key);
            }
            break;
        }
    }
    return;
  }
  #endif

  extern "C" int qb64_custom_event(int event,int v1,int v2,int v3,int v4,int v5,int v6,int v7,int v8,void *p1,void *p2){
    if (event==QB64_EVENT_CLOSE){
      exit_value|=1;
      return NULL;
    }//close
    if (event==QB64_EVENT_KEY){
      if (v1==VK+QBVK_PAUSE){
    if (v2>0) keydown_vk(v1); else keyup_vk(v1);
    return NULL;
      }
      if (v1==VK+QBVK_BREAK){
    if (v2>0) keydown_vk(v1); else keyup_vk(v1);
    return NULL;
      }
      return -1;
    }//key


    if (event==QB64_EVENT_RELATIVE_MOUSE_MOVEMENT){ //qb64_custom_event(QB64_EVENT_RELATIVE_MOUSE_MOVEMENT,xPosRelative,yPosRelative,0,0,0,0,0,0,NULL,NULL);
      static int32 i;
      int32 handle;
      handle=mouse_message_queue_first;
      mouse_message_queue_struct *queue=(mouse_message_queue_struct*)list_get(mouse_message_queue_handles,handle);
      //message #1
      i=queue->last+1; if (i>queue->lastIndex) i=0;
      if (i==queue->current){
        int32 nextIndex=queue->last+1; if (nextIndex>queue->lastIndex) nextIndex=0;
        queue->current=nextIndex;
      }
      queue->queue[i].x=queue->queue[queue->last].x;
      queue->queue[i].y=queue->queue[queue->last].y;
      queue->queue[i].movementx=v1;
      queue->queue[i].movementy=v2;
      queue->queue[i].buttons=queue->queue[queue->last].buttons;
      queue->last=i;
      //message #2 (clears movement values to avoid confusion)
      i=queue->last+1; if (i>queue->lastIndex) i=0;
      if (i==queue->current){
        int32 nextIndex=queue->last+1; if (nextIndex>queue->lastIndex) nextIndex=0;
        queue->current=nextIndex;
      }
      queue->queue[i].x=queue->queue[queue->last].x;
      queue->queue[i].y=queue->queue[queue->last].y;
      queue->queue[i].movementx=0;
      queue->queue[i].movementy=0;
      queue->queue[i].buttons=queue->queue[queue->last].buttons;
      queue->last=i;
      return NULL;
    }//QB64_EVENT_RELATIVE_MOUSE_MOVEMENT


    return -1;//Unknown command (use for debugging purposes only)
  }//qb64_custom_event

  void reinit_glut_callbacks(){

#ifdef QB64_GLUT


    glutDisplayFunc(GLUT_DISPLAY_REQUEST);
#ifdef QB64_WINDOWS
    glutTimerFunc(8,GLUT_TIMER_EVENT,0);
#else
    glutIdleFunc(GLUT_IDLEFUNC);
#endif
    glutKeyboardFunc(GLUT_KEYBOARD_FUNC);
    glutKeyboardUpFunc(GLUT_KEYBOARDUP_FUNC);
    glutSpecialFunc(GLUT_SPECIAL_FUNC);
    glutSpecialUpFunc(GLUT_SPECIALUP_FUNC);
    glutMouseFunc(GLUT_MOUSE_FUNC);
    glutMotionFunc(GLUT_MOTION_FUNC);
    glutPassiveMotionFunc(GLUT_PASSIVEMOTION_FUNC);
    glutReshapeFunc(GLUT_RESHAPE_FUNC);
#ifdef CORE_FREEGLUT
    glutMouseWheelFunc(GLUT_MOUSEWHEEL_FUNC);
#endif

#endif
  }

void showErrorOnScreen(char *errorMessage, int32 errorNumber, int32 lineNumber){//display error message on screen and enter infinite loop
new_error=0;//essential or the following commands won't be called
qbs *tqbs;
qbg_screen(func__newimage( 80 , 25 , 0 ,1),NULL,NULL,NULL,NULL,1);
sub__fullscreen( 3 ,1);//squarepixels+smooth for a beautiful error message
sub__displayorder( 1 ,NULL,NULL,NULL);
qbg_sub_color( 15 , 4 ,NULL,3);
sub_cls(NULL,NULL,0);
if (errorNumber!=0){
tqbs=qbs_new(0,0);
qbs_set(tqbs,qbs_new_txt_len("Unhandled Error #",17));
makefit(tqbs);
qbs_print(tqbs,0);
qbs_free(tqbs);
tqbs=qbs_new(0,0);
qbs_set(tqbs,qbs_ltrim(qbs_str((int32)(errorNumber))));
makefit(tqbs);
qbs_print(tqbs,0);
qbs_free(tqbs);
qbs_print(nothingstring,1);
}
if (lineNumber!=0){
tqbs=qbs_new(0,0);
qbs_set(tqbs,qbs_new_txt_len("Line:",5));
makefit(tqbs);
qbs_print(tqbs,0);
qbs_free(tqbs);
tqbs=qbs_new(0,0);
qbs_set(tqbs,qbs_str((int32)(lineNumber)));
makefit(tqbs);
qbs_print(tqbs,0);
qbs_free(tqbs);
qbs_print(nothingstring,1);
}
tqbs=qbs_new(0,0);
qbs_set(tqbs,qbs_new_txt(errorMessage));
makefit(tqbs);
qbs_print(tqbs,0);
qbs_free(tqbs);
qbs_print(nothingstring,1);
do{
sub__limit( 10 );
sub__display();
}while(1);
//infinite loop (this function never exits)
}
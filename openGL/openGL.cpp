// openGL.cpp : Defines the entry point for the console application.
//{{{  includes
#include "stdafx.h"
#pragma comment (lib,"OpenGL32.lib")
#pragma comment (lib,"glu32.lib")
//}}}

#define WIDTH           800
#define HEIGHT          600

#define BLACK_INDEX     0
#define RED_INDEX       13
#define GREEN_INDEX     14
#define BLUE_INDEX      16

#define GLOBE    1
#define CYLINDER 2
#define CONE     3

CHAR szAppName[] = "Windows OpenGL";
HWND hWnd;
HDC ghDC;
HGLRC ghRC;

GLfloat latitude, longitude, latinc, longinc;
GLdouble radius;

//{{{
BOOL setupPixelFormat(HDC hdc) {

  PIXELFORMATDESCRIPTOR pfd;
  PIXELFORMATDESCRIPTOR* ppfd = &pfd;
  ppfd->nSize = sizeof(PIXELFORMATDESCRIPTOR);
  ppfd->nVersion = 1;
  ppfd->dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
  ppfd->dwLayerMask = PFD_MAIN_PLANE;
  ppfd->iPixelType = PFD_TYPE_COLORINDEX;
  ppfd->cColorBits = 8;
  ppfd->cDepthBits = 16;
  ppfd->cAccumBits = 0;
  ppfd->cStencilBits = 0;

  int pixelformat;
  if ((pixelformat = ChoosePixelFormat(hdc, ppfd)) == 0 ) {
    MessageBox(NULL, "ChoosePixelFormat failed", "Error", MB_OK);
    return FALSE;
    }

  if (SetPixelFormat(hdc, pixelformat, ppfd) == FALSE) {
    MessageBox(NULL, "SetPixelFormat failed", "Error", MB_OK);
    return FALSE;
    }

  return TRUE;
  }
//}}}
//{{{
void polarView (GLdouble radius, GLdouble twist, GLdouble latitude, GLdouble longitude) {

  glTranslated (0.0, 0.0, -radius);
  glRotated (-twist, 0.0, 0.0, 1.0);
  glRotated (-latitude, 1.0, 0.0, 0.0);
  glRotated (longitude, 0.0, 0.0, 1.0);
  }
//}}}

//{{{
GLvoid createObjects() {

  glNewList (GLOBE, GL_COMPILE);
  GLUquadricObj* quadObj = gluNewQuadric();
  gluQuadricDrawStyle (quadObj, GLU_LINE);
  gluSphere (quadObj, 1.5, 16, 16);
  glEndList();

  glNewList (CONE, GL_COMPILE);
  quadObj = gluNewQuadric();
  gluQuadricDrawStyle (quadObj, GLU_FILL);
  gluQuadricNormals (quadObj, GLU_SMOOTH);
  gluCylinder(quadObj, 0.3, 0.0, 0.6, 15, 10);
  glEndList();

  glNewList (CYLINDER, GL_COMPILE);
  glPushMatrix();
  glRotatef ((GLfloat)90.0, (GLfloat)1.0, (GLfloat)0.0, (GLfloat)0.0);
  glTranslatef ((GLfloat)0.0, (GLfloat)0.0, (GLfloat)-1.0);
  quadObj = gluNewQuadric();
  gluQuadricDrawStyle (quadObj, GLU_FILL);
  gluQuadricNormals (quadObj, GLU_SMOOTH);
  gluCylinder (quadObj, 0.3, 0.3, 0.6, 12, 2);
  glPopMatrix();
  glEndList();
  }
//}}}
//{{{
GLvoid initializeGL (GLsizei width, GLsizei height) {

  glClearIndex ((GLfloat)BLACK_INDEX);
  glClearDepth (1.0);

  glEnable (GL_DEPTH_TEST);

  glMatrixMode (GL_PROJECTION);
  GLfloat aspect = (GLfloat) width / height;
  gluPerspective (45.0, aspect, 3.0, 7.0 );
  glMatrixMode (GL_MODELVIEW);

  GLdouble near_plane = 3.0;
  GLdouble far_plane = 7.0;
  GLfloat maxObjectSize = 3.0F;
  radius = near_plane + maxObjectSize/2.0;

  latitude = 0.0F;
  longitude = 0.0F;
  latinc = 6.0F;
  longinc = 2.5F;

  createObjects();
  }
//}}}
//{{{
GLvoid drawScene (GLvoid) {

  glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glPushMatrix();

  latitude += latinc;
  longitude += longinc;
  polarView (radius, 0, latitude, longitude );

  glIndexi (RED_INDEX);
  glCallList (CONE);

  glIndexi (BLUE_INDEX);
  glCallList (GLOBE);

  glIndexi (GREEN_INDEX);
  glPushMatrix();
  glTranslatef (0.8F, -0.65F, 0.0F);
  glRotatef (30.0F, 1.0F, 0.5F, 1.0F);
  glCallList (CYLINDER);
  glPopMatrix();

  glPopMatrix();

  SwapBuffers(ghDC);
  }
//}}}
//{{{
GLvoid resize (GLsizei width, GLsizei height ) {

    glViewport (0, 0, width, height);
    GLfloat aspect = (GLfloat)width / height;

    glMatrixMode (GL_PROJECTION);
    glLoadIdentity();
    gluPerspective (45.0, aspect, 3.0, 7.0);
    glMatrixMode (GL_MODELVIEW);
}
//}}}

//{{{
LONG WINAPI WndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {

  LONG lRet = 1;
  PAINTSTRUCT ps;
  RECT rect;

  switch (uMsg) {
    //{{{
    case WM_CREATE:
      ghDC = GetDC (hWnd);
      if (!setupPixelFormat (ghDC))
        PostQuitMessage (0);

      ghRC = wglCreateContext (ghDC);
      wglMakeCurrent (ghDC, ghRC);
      GetClientRect (hWnd, &rect);
      initializeGL (rect.right, rect.bottom);
      break;
    //}}}
    //{{{
    case WM_PAINT:
      BeginPaint (hWnd, &ps);
      EndPaint (hWnd, &ps);
      break;
    //}}}
    //{{{
    case WM_SIZE:
      GetClientRect(hWnd, &rect);
      resize (rect.right, rect.bottom);
      break;
    //}}}
    //{{{
    case WM_CLOSE:
      if (ghRC)
        wglDeleteContext(ghRC);
      if (ghDC)
        ReleaseDC(hWnd, ghDC);
      ghRC = 0;
      ghDC = 0;
      DestroyWindow (hWnd);
      break;
    //}}}
    //{{{
    case WM_DESTROY:
      if (ghRC)
        wglDeleteContext(ghRC);
      if (ghDC)
        ReleaseDC(hWnd, ghDC);
      PostQuitMessage (0);
      break;
    //}}}
    //{{{
    case WM_KEYDOWN:
      switch (wParam) {
        case VK_LEFT:
          longinc += 0.5F;
          break;
        case VK_RIGHT:
          longinc -= 0.5F;
          break;
        case VK_UP:
          latinc += 0.5F;
          break;
        case VK_DOWN:
          latinc -= 0.5F;
          break;
        }
    //}}}
    default:
      lRet = (LONG)DefWindowProc (hWnd, uMsg, wParam, lParam);
      break;
    }

  return lRet;
  }
//}}}
//{{{
int main (int argc, char* argv[]) {

  // register windowClass
  WNDCLASS wndclass;
  wndclass.style         = 0;
  wndclass.lpfnWndProc   = (WNDPROC)WndProc;
  wndclass.cbClsExtra    = 0;
  wndclass.cbWndExtra    = 0;
  wndclass.hInstance     = GetModuleHandle (0);
  wndclass.hIcon         = LoadIcon (GetModuleHandle(0), szAppName);
  wndclass.hCursor       = LoadCursor (NULL,IDC_ARROW);
  wndclass.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
  wndclass.lpszMenuName  = szAppName;
  wndclass.lpszClassName = szAppName;
  if (!RegisterClass (&wndclass) )
    return FALSE;

  // Create window
  hWnd = CreateWindow (szAppName, "Generic OpenGL Sample",
                       WS_OVERLAPPEDWINDOW | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
                       CW_USEDEFAULT, CW_USEDEFAULT, WIDTH, HEIGHT,
                       NULL, NULL, GetModuleHandle (0), NULL);
  if (!hWnd)
    return FALSE;

  ShowWindow (hWnd, SW_SHOWDEFAULT);
  UpdateWindow (hWnd);

  MSG msg;
  while (true) {
    while (PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE) == TRUE) {
      if (GetMessage(&msg, NULL, 0, 0) ) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        }
      else
        return TRUE;
      }
    drawScene();
    }
  }
//}}}
